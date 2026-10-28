#include "scenarios/pc_scenario.h"

#if PIKI_SCENARIOS

#include "scenarios/pc_scenario_list.h"

#include <SDL.h>

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <thread>
#include <chrono>

#include "FlowController.h"
#include "MoviePlayer.h"
#include "Animator.h"
#include "Navi.h"
#include "PaniAnimator.h"
#include "PaniPikiAnimator.h"
#include "GameStat.h"
#include "Piki.h"
#include "NaviMgr.h"
#include "PikiMgr.h"
#include "gameflow.h"
#include "jaudio/piki_scene.h"
#include "netplay/pc_input_log.h"
#include "netplay/pc_netplay_det.h"
#include "netplay/pc_state_hash.h"
#include "Dolphin/pad.h"
#include "pc_permadeath.h"
#include "gl/pc_gfx.h"
#include "pc_window.h"
#include "randomizer/pc_randomizer.h"
#include "settings/pc_settings.h"

#if defined(_WIN32)
#include <direct.h>
#include <process.h>
#include <stdlib.h>
#define PC_SCENARIO_CHDIR _chdir
#define PC_SCENARIO_GETPID _getpid
#else
#include <unistd.h>
#define PC_SCENARIO_CHDIR chdir
#define PC_SCENARIO_GETPID getpid
#endif

namespace {

constexpr const char* kDefaultStageFile = "stages/stage1.ini"; // Forest of Hope, a non-tutorial course
// Loop ticks the boot may take before the stage-ready hook must have fired.
constexpr unsigned kBootTickBudget = 20000;

PcScenario* sScenario      = nullptr;
bool sActive               = false;
int sTicksPerSecond        = 30;
int sExitCode              = PC_SCENARIO_EXIT_FAIL; // no verdict is a failure
bool sVerdict              = false;
bool sStageReady           = false;
// The stage opens with the landing cutscene and ship text on a new game. The
// intro gate pulses START and A until they are gone and the captain walks, then
// hands tick 0 to the scenario.
bool sIntroDone            = false;
unsigned sIntroTicks       = 0;
constexpr unsigned kIntroSeconds = 90;
unsigned sBootTicks        = 0;
unsigned sTick             = 0; // ticks since the stage became ready
std::string sStageFile     = kDefaultStageFile;
std::filesystem::path sScratchDir; // NECTAR_SAVE_DIR for this run, removed at exit

// Environment variables the game reads that change the sim, the input or the
// exit path. A scenario run starts without them (PIKMIN_INPUT_REPLAY is
// rejected instead, in pc_scenario_parse_argv).
const char* const kClearedEnv[] = {
	"PIKMIN_INPUT_RECORD",
	"PIKMIN_UNLOCK_ALL",
	"PIKMIN_COOP",
	"PIKMIN_COOP_CAPTAINS",
	"PIKMIN_REPLAY_TEST",
	"PIKMIN_NO_H4M",
	"PIKMIN_NETPLAY_EXIT_AFTER_TICKS",
	"PIKMIN_NETPLAY_TEST_PREROLL_RAND",
	"PIKMIN_NETPLAY_TEST_NAVI_TO_BOSS",
	"PIKMIN_NETPLAY_TEST_CAMERA_WOBBLE",
};

// The game reads these with the CRT getenv. SDL_setenv on Windows only sets the
// process environment block, which the MinGW CRT copy does not see.
void setEnv(const char* name, const char* value)
{
#if defined(_WIN32)
	_putenv_s(name, value);
#else
	setenv(name, value, 1);
#endif
}

void clearEnv(const char* name)
{
#if defined(_WIN32)
	_putenv_s(name, ""); // an empty value removes the variable
#else
	unsetenv(name);
#endif
}

// Boot flags that change the boot path, the sim or the exit path. --scenario
// is refused with any of them (exit 2) so a scenario run cannot take another
// route into the game. --input-record is deliberately absent: the provider runs
// inside pc_input_log_tick, so a recording of a scenario run is supported.
const char* const kRejectedFlags[] = {
	"--coop-direct",
	"--coop",
	"--vs-map",
	"--mission-map",
	"--campaign",
	"--netplay-host",
	"--netplay-join",
	"--netplay-host-ice",
	"--netplay-join-ice",
	"--netplay-exit-after-ticks",
	"--input-replay",
};

// Wall-clock watchdog. The default (540 s) is shorter than the CTest TIMEOUT of
// 600 s so the in-binary FAIL line wins over a bare CTest timeout. The slowest
// stand-in scenario takes about 85 s on software GL. PIKMIN_SCENARIO_WATCHDOG_SECONDS
// overrides it for slow CI runners. The thread only ever leaves through _Exit.
constexpr long kDefaultWatchdogSeconds = 540;

void removeScratch()
{
	if (sScratchDir.empty()) return;
	// PIKMIN_SCENARIO_KEEP_SAVE_DIR=1 keeps it for inspection (check_scenario_save_dir.cmake).
	if (const char* keep = std::getenv("PIKMIN_SCENARIO_KEEP_SAVE_DIR"); keep != nullptr && std::strcmp(keep, "1") == 0) return;
	std::error_code ec;
	std::filesystem::remove_all(sScratchDir, ec);
	sScratchDir.clear();
}

[[noreturn]] void terminate(int code)
{
	// _Exit skips stdio's own flush: the hash log flushes every 300 ticks, so
	// its tail (and a --input-record file's) would be lost.
	pc_state_hash_flush();
	pc_input_log_flush();
	std::fflush(stdout);
	std::fflush(stderr);
	removeScratch();
	std::_Exit(code);
}

// Runs on every exit() and on main's return. System::halt (Windows) and PPCHalt
// call exit(0), which must not read as a pass: from the moment it is registered
// (a scenario is active and its argument and data checks passed), any exit
// without a verdict is a failure. A missing window or GL context records its
// verdict (4) through pc_scenario_on_no_gl before main returns.
void onExit() { terminate(sVerdict ? sExitCode : PC_SCENARIO_EXIT_FAIL); }

#ifndef _WIN32
#if __has_include(<execinfo.h>)
#include <execinfo.h>
#define PC_SCENARIO_HAVE_BACKTRACE 1
#endif

// A fatal signal (an abort from OSPanic, a bad pointer) is a scenario failure,
// not a crash: the player's handler would re-raise it with the default action,
// and macOS then writes a crash report and can open a dialog for every failing
// run. This prints a short backtrace and leaves through terminate().
void onFatalSignal(int sig)
{
	std::fprintf(stderr, "[scenario] FAIL: fatal signal %d\n", sig);
#if PC_SCENARIO_HAVE_BACKTRACE
	void* frames[48];
	const int n = backtrace(frames, 48);
	backtrace_symbols_fd(frames, n, 2);
#endif
	terminate(PC_SCENARIO_EXIT_FAIL);
}

void installFatalSignalHandlers()
{
	for (int sig : {SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE}) std::signal(sig, onFatalSignal);
}
#else
void installFatalSignalHandlers() {}
#endif

void startWatchdog(const char* name)
{
	long seconds = kDefaultWatchdogSeconds;
	if (const char* env = std::getenv("PIKMIN_SCENARIO_WATCHDOG_SECONDS")) {
		char* end = nullptr;
		long v    = std::strtol(env, &end, 10);
		if (end != env && *end == '\0' && v > 0) seconds = v;
	}
	std::string scenarioName = name;
	std::thread([scenarioName, seconds]() {
		std::this_thread::sleep_for(std::chrono::seconds(seconds));
		std::fprintf(stderr, "[scenario] FAIL %s: wall-clock watchdog after %ld s\n", scenarioName.c_str(), seconds);
		terminate(PC_SCENARIO_EXIT_FAIL); // flushes, then _Exit; no atexit handlers
	}).detach();
}

void usage(const char* msg)
{
	std::fprintf(stderr, "[scenario] %s\n", msg);
	std::fprintf(stderr, "usage: nectar --scenario <name> [--scenario-tick-rate 30|60] [--scenario-visible]\n");
	std::fprintf(stderr, "scenarios:");
	for (PcScenario* s : pc_scenario_all()) {
		if (!s->testOnly()) std::fprintf(stderr, " %s", s->name());
	}
	std::fprintf(stderr, "\n");
}

void providerThunk(PcInputPad pads[4])
{
	if (sScenario == nullptr) return;
	// Pad 0 is the captain's. Before the stage is ready the pad stays neutral.
	if (!sStageReady || sVerdict) return;
	if (!sIntroDone) {
		// START skips a movie and would open the pause menu otherwise; A closes ship text.
		u32 buttons = 0;
		if (sIntroTicks % 15 == 0) {
			if (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive) buttons |= PAD_BUTTON_START;
			if (gameflow.mIsUIOverlayActive) buttons |= PAD_BUTTON_A;
		}
		pads[0].button = buttons;
		return;
	}
	sScenario->providePads(sTick, pads[0]);
}

// PIKMIN_SCENARIO_STATE_STATS (any value): per state id of the captain and of
// the Pikmin, the longest uninterrupted stay in ticks and how often it was
// entered, printed with the verdict. A state that waits on an animation key the
// stand-in lacks shows up as a long stay (verification-log.md, item 5).
struct StateDwell {
	unsigned longest = 0;
	unsigned entries = 0;
};
struct StateTrack {
	int state      = -1;
	unsigned since = 0;
};
std::map<int, StateDwell> sCaptainDwell, sPikiDwell;
std::map<const void*, StateTrack> sTracks;
bool sStateStats = false;
unsigned sStatsTick = 0;

void noteState(const void* who, int state, std::map<int, StateDwell>& table)
{
	StateTrack& t = sTracks[who];
	if (t.state != state) {
		t.state = state;
		t.since = sStatsTick;
		table[state].entries++;
	}
	StateDwell& d = table[state];
	if (sStatsTick - t.since + 1 > d.longest) d.longest = sStatsTick - t.since + 1;
}

void trackStates()
{
	if (Navi* navi = naviMgr ? naviMgr->getNavi() : nullptr) {
		if (navi->getCurrState()) noteState(navi, navi->getCurrState()->getID(), sCaptainDwell);
	}
	if (pikiMgr) {
		Iterator it(pikiMgr);
		CI_LOOP(it)
		{
			Piki* piki = static_cast<Piki*>(*it);
			noteState(piki, piki->getState(), sPikiDwell);
		}
	}
	++sStatsTick;
}

// Runner-level invariant (coverage/edge-cases.md I-2, EC-05/06/08): GameStat's
// counters agree with each other and with the live Pikmin, after every tick of
// every scenario. allPikis counts field Pikmin in every mode, sprouts (mePikis)
// and Onion contents (containerPikis); it does not count the dead. The first
// violation fails the scenario with the tick and the values.
void checkGameStatInvariants(unsigned tick)
{
	static const char* const colName[PikiColorCount] = { "blue", "red", "yellow" };
	struct Named {
		const char* name;
		GameStat::ColCounter* c;
	};
	const Named all[] = { { "deadPikis", &GameStat::deadPikis }, { "fallPikis", &GameStat::fallPikis },
		                  { "formationPikis", &GameStat::formationPikis }, { "freePikis", &GameStat::freePikis },
		                  { "workPikis", &GameStat::workPikis }, { "mePikis", &GameStat::mePikis },
		                  { "containerPikis", &GameStat::containerPikis }, { "bornPikis", &GameStat::bornPikis },
		                  { "victimPikis", &GameStat::victimPikis }, { "mapPikis", &GameStat::mapPikis },
		                  { "allPikis", &GameStat::allPikis } };
	for (const Named& n : all) {
		for (int c = PikiMinColor; c < PikiColorCount; c++) {
			if ((*n.c)[c] < 0) {
				scenarioFailf(tick, "GameStat counters are never negative", "GameStat::%s[%s] = %d", n.name, colName[c], (*n.c)[c]);
				return;
			}
		}
	}
	if (GameStat::killTekis < 0 || GameStat::getPellets < 0) {
		scenarioFailf(tick, "GameStat counters are never negative", "killTekis %d getPellets %d", (int)GameStat::killTekis,
		              (int)GameStat::getPellets);
		return;
	}
	int live[PikiColorCount] = { 0, 0, 0 };
	if (pikiMgr) {
		Iterator it(pikiMgr);
		CI_LOOP(it)
		{
			Piki* piki = static_cast<Piki*>(*it);
			if (piki->mColor < PikiColorCount) live[piki->mColor]++;
		}
	}
	for (int c = PikiMinColor; c < PikiColorCount; c++) {
		const int field = GameStat::formationPikis[c] + GameStat::freePikis[c] + GameStat::workPikis[c];
		if (field != live[c]) {
			scenarioFailf(tick, "formation + free + work equals the live Pikmin of the colour",
			              "%s: formation %d + free %d + work %d = %d, live Pikmin %d", colName[c], GameStat::formationPikis[c],
			              GameStat::freePikis[c], GameStat::workPikis[c], field, live[c]);
			return;
		}
		const int map = field + GameStat::mePikis[c];
		if (GameStat::mapPikis[c] != map) {
			scenarioFailf(tick, "mapPikis = formation + free + me + work", "%s: map %d, parts sum to %d", colName[c],
			              GameStat::mapPikis[c], map);
			return;
		}
		const int total = GameStat::mapPikis[c] + GameStat::containerPikis[c];
		if (GameStat::allPikis[c] != total) {
			scenarioFailf(tick, "allPikis = mapPikis + containerPikis", "%s: all %d, map %d + container %d", colName[c],
			              GameStat::allPikis[c], GameStat::mapPikis[c], GameStat::containerPikis[c]);
			return;
		}
	}
	const int allTotal = GameStat::allPikis;
	if (GameStat::maxPikis < allTotal) {
		scenarioFailf(tick, "maxPikis >= allPikis", "maxPikis %d, allPikis %d", GameStat::maxPikis, allTotal);
		return;
	}
	if (GameStat::minPikis < 0 || GameStat::minPikis > GameStat::maxPikis) {
		scenarioFailf(tick, "0 <= minPikis <= maxPikis", "minPikis %d, maxPikis %d", GameStat::minPikis, GameStat::maxPikis);
		return;
	}
}

void printStateStats()
{
	if (!sStateStats) return;
	for (const auto& [id, d] : sCaptainDwell)
		std::printf("[scenario-states] captain state %d: longest %u ticks, entered %u times\n", id, d.longest, d.entries);
	for (const auto& [id, d] : sPikiDwell)
		std::printf("[scenario-states] piki state %d: longest %u ticks, entered %u times\n", id, d.longest, d.entries);
}

void finish(int code)
{
	if (sVerdict) return;
	printStateStats();
	sVerdict  = true;
	sExitCode = code;
	std::fflush(stdout);
	std::fflush(stderr);
	pc_window_request_close();
}

} // namespace

int pc_scenario_parse_argv(int argc, char** argv)
{
	const char* name   = nullptr;
	bool visible       = false;
	int rate           = 30;
	const char* rejected = nullptr;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--scenario") == 0) {
			if (i + 1 >= argc) {
				usage("--scenario needs a name");
				return PC_SCENARIO_EXIT_USAGE;
			}
			name = argv[++i];
		} else if (std::strcmp(argv[i], "--scenario-tick-rate") == 0) {
			if (i + 1 >= argc) {
				usage("--scenario-tick-rate needs 30 or 60");
				return PC_SCENARIO_EXIT_USAGE;
			}
			rate = std::atoi(argv[++i]);
			if (rate != 30 && rate != 60) {
				usage("--scenario-tick-rate must be 30 or 60");
				return PC_SCENARIO_EXIT_USAGE;
			}
		} else if (std::strcmp(argv[i], "--scenario-visible") == 0) {
			visible = true;
		} else {
			for (const char* flag : kRejectedFlags) {
				if (rejected == nullptr && std::strcmp(argv[i], flag) == 0) rejected = flag;
			}
		}
	}
	if (name == nullptr) return -1;
	if (rejected != nullptr) {
		usage((std::string("--scenario cannot be combined with ") + rejected).c_str());
		return PC_SCENARIO_EXIT_USAGE;
	}
	if (const char* envReplay = std::getenv("PIKMIN_INPUT_REPLAY"); envReplay != nullptr && *envReplay != '\0') {
		usage("--scenario cannot be combined with PIKMIN_INPUT_REPLAY");
		return PC_SCENARIO_EXIT_USAGE;
	}
	for (PcScenario* s : pc_scenario_all()) {
		if (std::strcmp(s->name(), name) == 0) sScenario = s;
	}
	if (sScenario == nullptr) {
		usage("unknown scenario");
		return PC_SCENARIO_EXIT_USAGE;
	}
	if (!sScenario->supportsRate(rate)) {
		std::printf("[scenario] %s does not support %d ticks per second; skipping\n", name, rate);
		return PC_SCENARIO_EXIT_SKIP;
	}

	// Data gate, as --audio-self-test does: the caller's environment names the
	// directory that holds assets/, and the game opens files relative to it.
	const char* dataDir = std::getenv("PIKMIN_SCENARIO_DATA_DIR");
	if (dataDir == nullptr || *dataDir == '\0') {
		std::printf("[scenario] PIKMIN_SCENARIO_DATA_DIR is not set; skipping\n");
		return PC_SCENARIO_EXIT_SKIP;
	}
	if (PC_SCENARIO_CHDIR(dataDir) != 0) {
		std::string msg = std::string("PIKMIN_SCENARIO_DATA_DIR is set but cannot be entered: ") + dataDir;
		usage(msg.c_str());
		return PC_SCENARIO_EXIT_USAGE;
	}
	if (!std::filesystem::exists("assets/dataDir/parms/gamePrms.bin")) {
		std::printf("[scenario] no assets/dataDir/parms/gamePrms.bin in %s; skipping\n", dataDir);
		return PC_SCENARIO_EXIT_SKIP;
	}
	if (const char* stage = std::getenv("PIKMIN_SCENARIO_STAGE")) {
		if (*stage != '\0') sStageFile = stage;
	}
	// The window size, WxH (default in pc_settings.cpp). The sim does not read it.
	int windowWidth = 0, windowHeight = 0;
	if (const char* window = std::getenv("PIKMIN_SCENARIO_WINDOW"); window != nullptr && *window != '\0') {
		char tail = '\0';
		if (std::sscanf(window, "%dx%d%c", &windowWidth, &windowHeight, &tail) != 2 || windowWidth < 1 || windowHeight < 1
		    || windowWidth > 8192 || windowHeight > 8192) {
			usage((std::string("PIKMIN_SCENARIO_WINDOW must be WxH, for example 64x36, not ") + window).c_str());
			return PC_SCENARIO_EXIT_USAGE;
		}
	}

	sActive         = true;
	sTicksPerSecond = rate;
	sStateStats     = std::getenv("PIKMIN_SCENARIO_STATE_STATS") != nullptr;

	// Everything a scenario run must not inherit from the machine.
	SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
	for (const char* var : kClearedEnv) clearEnv(var);
	{
		// Always a fresh save dir (an ambient NECTAR_SAVE_DIR is not kept). SDL_GetTicks
		// is about 0 before SDL_Init, so the process id keeps concurrent runs apart.
		std::error_code ec;
		sScratchDir = std::filesystem::temp_directory_path(ec) / ("nectar-scenario-" + std::string(name) + "-" + std::to_string(rate) + "-"
		                                                          + std::to_string((long)PC_SCENARIO_GETPID()));
		std::filesystem::remove_all(sScratchDir, ec);
		std::filesystem::create_directories(sScratchDir, ec);
		setEnv("NECTAR_SAVE_DIR", sScratchDir.string().c_str());
		std::printf("[scenario] save dir %s\n", sScratchDir.string().c_str());
	}
	std::atexit(onExit);
	installFatalSignalHandlers();
	startWatchdog(name);
	pc_netplay_det_scenario_enable();
	pc_settings_scenario_enable();
	pc_settings_scenario_set_fps_mode(rate == 60 ? 1 : 0);
	if (windowWidth > 0) pc_settings_scenario_set_window_size(windowWidth, windowHeight);
	pc_gfx_set_min_render_size(1, 1); // the window size, below the player's 160x120 floor
	pc_window_set_hidden(!visible);
	pc_input_log_set_provider(providerThunk);
	std::printf("[scenario] %s at %d ticks per second, stage %s, data %s\n", name, rate, sStageFile.c_str(), dataDir);
	return -1;
}

bool pc_scenario_active(void) { return sActive; }

int pc_scenario_exit_code(void) { return sExitCode; }

int pc_scenario_ticks_per_second(void) { return sTicksPerSecond; }

unsigned pc_scenario_intro_ticks(void) { return sIntroTicks; }

void pc_scenario_on_no_gl(void)
{
	// No window exists, so no close request; main returns right after this and
	// the exit handler reports the code.
	sVerdict  = true;
	sExitCode = PC_SCENARIO_EXIT_NO_GL;
}

void pc_scenario_pre_stage(void)
{
	static bool sRan = false;
	if (sRan) return;
	sRan = true;

	// What CardSelect would have done on its no-save new-game path, without
	// writing a save (#61, skip-path equivalence).
	pc_permadeath_begin_new_run();
	pc_hardmode_begin_new_run();
	pc_randomizer_begin_new_run();
	gameflow.mGamePrefs.mHasSaveGame = false;
	gameflow.mIsChallengeMode        = FALSE;

	flowCont.setStage(sStageFile.c_str());
	if (flowCont.mCurrentStage == nullptr || std::strcmp(flowCont.mCurrentStage->mFileName, sStageFile.c_str()) != 0) {
		scenarioFailf(0, "stage in stages.ini", "no stage with file '%s'", sStageFile.c_str());
		// Booting on with no stage would dereference it; the verdict is already set.
		terminate(PC_SCENARIO_EXIT_FAIL);
	}
	// The day is consumed by GameCoreSection's constructor (generator file and
	// RNG reseed), so it is set here and not in the stage-ready hook.
	gameflow.mWorldClock.mCurrentDay = sScenario->startDay();
	gameflow.mWorldClock.setTime(gameflow.mParameters->mStartHour());
#if PIKI_USE_JAUDIO
	// The title section the scenario boot skips (ninLogo.cpp) ends with
	// Jac_SceneSetup(SCENE_Title) (titles.cpp), which is the only call that sets
	// the engine's first_load flag (piki_scene.c __Loaded, 0x20000). Without it
	// the stage's Jac_SceneSetup(SCENE_Course) spins forever waiting for that
	// flag. The legacy facade (PIKMIN_NATIVE_JAUDIO=OFF) has no such wait, so
	// the call is made in the native build only (standin-audio.md).
	Jac_SceneSetup(SCENE_Title, 0);
#endif
	std::printf("[scenario] stage '%s' id %d, day %d\n", flowCont.mCurrentStage->mFileName, (int)flowCont.mCurrentStage->mStageID,
	            sScenario->startDay());
}

void pc_scenario_on_stage_ready(void)
{
	if (!sActive || sStageReady) return;
	sStageReady = true;
	sTick       = 0;
	int drawableWidth = 0, drawableHeight = 0, renderWidth = 0, renderHeight = 0;
	pc_gfx_get_drawable_size(&drawableWidth, &drawableHeight);
	pc_gfx_get_render_size(&renderWidth, &renderHeight);
	std::printf("[scenario] stage ready after %u boot ticks (drawable %dx%d, render %dx%d)\n", sBootTicks, drawableWidth,
	            drawableHeight, renderWidth, renderHeight);
}

void pc_scenario_on_tick_end(void)
{
	if (!sActive || sVerdict) return;
	if (!sStageReady) {
		if (++sBootTicks > kBootTickBudget) scenarioFailf(sBootTicks, "stage ready within boot budget", "%u boot ticks without the stage-ready hook", sBootTicks);
		return;
	}
	if (!sIntroDone) {
		Navi* navi        = naviMgr ? naviMgr->getNavi() : nullptr;
		const bool movie  = gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive;
		const bool walk   = navi && navi->getCurrState() && navi->getCurrState()->getID() == 0;
		if (!movie && !gameflow.mIsUIOverlayActive && walk) {
			sIntroDone = true;
			sTick      = 0;
			std::printf("[scenario] intro done after %u ticks\n", sIntroTicks);
			if (!scenarioCheckAnimMgrPreconditions(0)) return;
			sScenario->onStageReady();
			return;
		}
		++sIntroTicks;
		if (sIntroTicks > kIntroSeconds * (unsigned)sTicksPerSecond) {
			scenarioFailf(sIntroTicks, "intro cutscene and text finish", "movie %d overlay %d captain %d", (int)movie,
			              (int)gameflow.mIsUIOverlayActive, navi && navi->getCurrState() ? navi->getCurrState()->getID() : -1);
		}
		return;
	}
	// PIKMIN_SCENARIO_TRACE=<ticks>: log the captain state, movie and overlay flags
	// for the first ticks after the stage is ready (what tick 0 looks like).
	static const int traceTicks = std::getenv("PIKMIN_SCENARIO_TRACE") ? std::atoi(std::getenv("PIKMIN_SCENARIO_TRACE")) : 0;
	if ((int)sTick < traceTicks) {
		Navi* navi = naviMgr ? naviMgr->getNavi() : nullptr;
		std::printf("[scenario-trace] tick %u captain %d movie %d overlay %d day %d time %.3f\n", sTick,
		            navi && navi->getCurrState() ? navi->getCurrState()->getID() : -1,
		            gameflow.mMoviePlayer ? (int)gameflow.mMoviePlayer->mIsActive : -1, (int)gameflow.mIsUIOverlayActive,
		            gameflow.mWorldClock.mCurrentDay, gameflow.mWorldClock.mTimeOfDay);
	}
	sScenario->onTick(sTick);
	if (!sVerdict) checkGameStatInvariants(sTick);
	if (sStateStats) trackStates();
	++sTick;
}

void scenarioPass(void)
{
	// sTick is the tick being checked, counted from 0 after the intro gate.
	std::printf("[scenario] PASS %s at tick %u (intro %u ticks)\n", sScenario ? sScenario->name() : "?", sTick, sIntroTicks);
	finish(PC_SCENARIO_EXIT_PASS);
}

void scenarioFail(unsigned tick, const char* expected, const char* observed)
{
	if (sVerdict) return;
	std::fprintf(stderr, "[scenario] FAIL %s tick %u: expected %s, observed %s\n", sScenario ? sScenario->name() : "?", tick, expected,
	             observed);
	finish(PC_SCENARIO_EXIT_FAIL);
}

void scenarioFailf(unsigned tick, const char* expected, const char* fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	std::vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	scenarioFail(tick, expected, buf);
}

bool scenarioCheckAnimMgrPreconditions(unsigned tick)
{
	AnimMgr* mgr = PikiShapeObject::getAnimMgr();
	if (mgr == nullptr) {
		scenarioFailf(tick, "the Pikmin AnimMgr exists", "PikiShapeObject::getAnimMgr() returned null");
		return false;
	}
	int index = 0;
	FOREACH_NODE(AnimInfo, mgr->mAnimList.mChild, info)
	{
		const char* infoName = info->mName ? info->mName : "(null)";
		if (info->mData == nullptr) {
			scenarioFailf(tick, "every AnimMgr info has an animation", "info %d '%s' has no animation data", index, infoName);
			return false;
		}
		if (info->mData->mTotalFrameCount <= 0) {
			scenarioFailf(tick, "every AnimMgr animation has mTotalFrameCount > 0", "info %d '%s' has mTotalFrameCount %d", index, infoName,
			              info->mData->mTotalFrameCount);
			return false;
		}
		const char* dataName = info->mData->mName ? info->mData->mName : "";
		if (std::strcmp(dataName, "Null Anim") == 0) {
			scenarioFailf(tick, "no AnimMgr animation is the Null Anim dummy", "info %d '%s' fell back to the Null Anim dummy", index, infoName);
			return false;
		}
		++index;
	}
	if (index == 0) {
		scenarioFailf(tick, "the Pikmin AnimMgr has infos", "zero infos");
		return false;
	}
	std::printf("[scenario] AnimMgr preconditions ok: %d infos, all with frames and not Null Anim\n", index);
	return true;
}

bool scenarioCheckThrowWaitPreconditions(unsigned tick, Navi* navi)
{
	constexpr int kThrowWaitInfo = 63; // setMotion(PIKIANIM_ThrowWait, new PaniMotion(63))
	PaniAnimator& upper          = navi->mNaviAnimMgr.getUpperAnimator();
	if (upper.getCurrentMotionIndex() != PIKIANIM_ThrowWait) {
		scenarioFailf(tick, "upper animator motion is ThrowWait", "motion index %d (ThrowWait is %d)", upper.getCurrentMotionIndex(),
		              (int)PIKIANIM_ThrowWait);
		return false;
	}
	AnimInfo* info = upper.mAnimInfo;
	AnimMgr* mgr   = PikiShapeObject::getAnimMgr();
	if (info == nullptr || mgr == nullptr || info != mgr->findAnim(kThrowWaitInfo)) {
		scenarioFailf(tick, "the ThrowWait animation is AnimMgr info 63", "upper animator info %p, info 63 %p", (void*)info,
		              mgr ? (void*)mgr->findAnim(kThrowWaitInfo) : nullptr);
		return false;
	}
	const int frames = upper.getFrameCount();
	const int aKeys  = info->countAKeys();
	const int iKeys  = info->countIKeys();
	for (int i = 0; i < iKeys; ++i) {
		AnimKey* key = info->getInfoKey(i);
		if (key->mKeyType != 2) continue;
		if (key->mFrameIndex < 0 || key->mFrameIndex >= aKeys) {
			scenarioFailf(tick, "the type-2 info key indexes an anim key", "info key %d refers to anim key %d of %d", i, key->mFrameIndex, aKeys);
			return false;
		}
		const int frame = info->getKeyValue(key->mFrameIndex);
		if (frame < 0 || frame >= frames) {
			scenarioFailf(tick, "the type-2 info key's anim-key frame is below the frame count", "frame %d, frame count %d", frame, frames);
			return false;
		}
		std::printf("[scenario] ThrowWait preconditions ok: info 63 type-2 key at frame %d of %d\n", frame, frames);
		return true;
	}
	scenarioFailf(tick, "info 63 has a type-2 info key", "%d info keys, none of type 2", iKeys);
	return false;
}

#endif
