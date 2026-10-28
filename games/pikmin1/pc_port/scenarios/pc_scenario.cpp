#include "scenarios/pc_scenario.h"

#if PIKI_SCENARIOS

#include "scenarios/pc_scenario_list.h"

#include <SDL.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

#include "FlowController.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "gameflow.h"
#include "netplay/pc_netplay_det.h"
#include "Dolphin/pad.h"
#include "pc_permadeath.h"
#include "pc_window.h"
#include "randomizer/pc_randomizer.h"
#include "settings/pc_settings.h"

#if defined(_WIN32)
#include <direct.h>
#include <stdlib.h>
#define PC_SCENARIO_CHDIR _chdir
#else
#include <unistd.h>
#define PC_SCENARIO_CHDIR chdir
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

void usage(const char* msg)
{
	std::fprintf(stderr, "[scenario] %s\n", msg);
	std::fprintf(stderr, "usage: nectar --scenario <name> [--scenario-tick-rate 30|60] [--scenario-visible]\n");
	std::fprintf(stderr, "scenarios:");
	for (PcScenario* s : pc_scenario_all()) std::fprintf(stderr, " %s", s->name());
	std::fprintf(stderr, "\n");
}

void providerThunk(PcInputPad pads[4])
{
	if (sScenario == nullptr) return;
	// Pad 0 is the captain's. Before the stage is ready the pad stays neutral.
	if (!sStageReady || sVerdict) return;
	if (!sIntroDone) {
		pads[0].button = (sIntroTicks % 15 == 0) ? (PAD_BUTTON_START | PAD_BUTTON_A) : 0;
		return;
	}
	sScenario->providePads(sTick, pads[0]);
}

void finish(int code)
{
	if (sVerdict) return;
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
	bool replay        = false;
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
		} else if (std::strcmp(argv[i], "--input-replay") == 0) {
			replay = true;
		}
	}
	if (name == nullptr) return -1;
	if (replay) {
		usage("--scenario cannot be combined with --input-replay");
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
		std::printf("[scenario] cannot enter %s; skipping\n", dataDir);
		return PC_SCENARIO_EXIT_SKIP;
	}
	if (!std::filesystem::exists("assets/dataDir/parms/gamePrms.bin")) {
		std::printf("[scenario] no assets/dataDir/parms/gamePrms.bin in %s; skipping\n", dataDir);
		return PC_SCENARIO_EXIT_SKIP;
	}
	if (const char* stage = std::getenv("PIKMIN_SCENARIO_STAGE")) {
		if (*stage != '\0') sStageFile = stage;
	}

	sActive         = true;
	sTicksPerSecond = rate;

	// Everything a scenario run must not inherit from the machine.
	SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);
	if (std::getenv("NECTAR_SAVE_DIR") == nullptr) {
		std::error_code ec;
		std::filesystem::path scratch = std::filesystem::temp_directory_path(ec) / ("nectar-scenario-" + std::string(name) + "-" + std::to_string(SDL_GetTicks()));
		std::filesystem::create_directories(scratch, ec);
		// The game reads this with the CRT getenv. SDL_setenv on Windows only sets
		// the process environment block, which the MinGW CRT copy does not see.
#if defined(_WIN32)
		_putenv_s("NECTAR_SAVE_DIR", scratch.string().c_str());
#else
		setenv("NECTAR_SAVE_DIR", scratch.string().c_str(), 1);
#endif
	}
	pc_netplay_det_scenario_enable();
	pc_settings_scenario_enable();
	pc_settings_scenario_set_fps_mode(rate == 60 ? 1 : 0);
	pc_window_set_hidden(!visible);
	pc_input_log_set_provider(providerThunk);
	std::printf("[scenario] %s at %d ticks per second, stage %s, data %s\n", name, rate, sStageFile.c_str(), dataDir);
	return -1;
}

bool pc_scenario_active(void) { return sActive; }

int pc_scenario_exit_code(void) { return sExitCode; }

int pc_scenario_ticks_per_second(void) { return sTicksPerSecond; }

void pc_scenario_pre_stage(void)
{
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
		return;
	}
	// The day is consumed by GameCoreSection's constructor (generator file and
	// RNG reseed), so it is set here and not in the stage-ready hook.
	gameflow.mWorldClock.mCurrentDay = sScenario->startDay();
	gameflow.mWorldClock.setTime(gameflow.mParameters->mStartHour());
	std::printf("[scenario] stage '%s' id %d, day %d\n", flowCont.mCurrentStage->mFileName, (int)flowCont.mCurrentStage->mStageID,
	            sScenario->startDay());
}

void pc_scenario_on_stage_ready(void)
{
	if (!sActive || sStageReady) return;
	sStageReady = true;
	sTick       = 0;
	std::printf("[scenario] stage ready after %u boot ticks\n", sBootTicks);
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
	++sTick;
}

void scenarioPass(void)
{
	std::printf("[scenario] PASS %s\n", sScenario ? sScenario->name() : "?");
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

#endif
