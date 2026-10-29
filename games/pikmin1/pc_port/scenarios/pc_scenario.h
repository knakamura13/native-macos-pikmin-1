#pragma once
// Scripted gameplay scenarios (--scenario <name>), compiled only when
// PIKMIN_BUILD_SCENARIOS is ON (PIKI_SCENARIOS=1). A scenario boots straight
// into a stage, feeds pad input through an in-process provider, reads game
// objects directly and exits with a verdict.
//
// Exit codes: 0 pass, 1 assertion failed (one stderr line), 2 unknown scenario
// or bad arguments (before any window exists), 4 window or GL context creation
// failed, 77 skip because no game data (PIKMIN_SCENARIO_DATA_DIR).

#if PIKI_SCENARIOS

#include "netplay/pc_input_log.h"

enum {
	PC_SCENARIO_EXIT_PASS  = 0,
	PC_SCENARIO_EXIT_FAIL  = 1,
	PC_SCENARIO_EXIT_USAGE = 2,
	PC_SCENARIO_EXIT_NO_GL = 4,
	PC_SCENARIO_EXIT_SKIP  = 77,
};

// One scenario. Ticks are counted from the first tick after the stage is ready
// (tick 0), at the rate chosen with --scenario-tick-rate.
class PcScenario {
public:
	virtual ~PcScenario() { }
	virtual const char* name() const = 0;
	// Day the stage starts on (the day is consumed by GameCoreSection's constructor).
	virtual int startDay() const { return 1; }
	virtual bool supportsRate(int /*ticksPerSecond*/) const { return true; }
	// True for runner self-tests (exit_zero): selectable by name, left out of the usage list.
	virtual bool testOnly() const { return false; }
	// Runs once when the stage is ready, before tick 0.
	virtual void onStageReady() { }
	// Fills pad 0 for this tick; pads 1-3 arrive neutral. Called every tick, before the sim.
	virtual void providePads(unsigned /*tick*/, PcInputPad& /*pad0*/) { }
	// Runs after each tick's sim. Report a verdict with scenarioPass() / scenarioFail().
	virtual void onTick(unsigned tick) = 0;

	// Scenarios that cross a day end into the next stage (onion_day3). With this
	// true, a soft reset seen after onTick without a verdict does not end the run:
	// the runner stops ticking the scenario, waits for the next stage, runs the
	// intro gate again and calls onStageReady() once more (pc_scenario_stage_number
	// says which). Ticks keep counting across stages. Off by default, so every
	// other scenario keeps its single-stage behaviour.
	virtual bool crossesDays() const { return false; }
	// Called when OnePlayerSection builds MapSelect for a crossesDays() scenario.
	// Return the stages.ini `file` of the course to enter: the runner then does what
	// MapSelectSetupSection::enterCourse and its exit do and goes to NewPikiGame
	// without building the world map (the stand-in tree has no world map screens).
	// Return nullptr to build the real MapSelectSection (disc) and drive it with
	// providePadsBetweenStages.
	virtual const char* onMapSelect() { return nullptr; }
	// Pad 0 between a soft reset and the next stage ready (the real world map).
	virtual void providePadsBetweenStages(unsigned /*ticksSinceReset*/, PcInputPad& /*pad0*/) { }
	// Whether the intro gate pulses START over the opening movie of stage `stageNumber`
	// (1 = the first). False lets the movie run to its end; A still closes text.
	virtual bool introSkipsMovies(int /*stageNumber*/) const { return true; }
	// Whether the runner's GameStat invariant check runs after this tick. A scenario
	// turns it off only where the original game's own counting breaks it (say why).
	virtual bool checksGameStat() const { return true; }
};

// Returns -1 to continue booting (no --scenario), else the process exit code.
int pc_scenario_parse_argv(int argc, char** argv);
bool pc_scenario_active(void);
// Exit code for pc_main to return once the game loop ends.
int pc_scenario_exit_code(void);
// Called by pc_main when the window or GL context failed: records exit 4 as the
// verdict so the exit handler (which main's return also runs) reports it.
void pc_scenario_on_no_gl(void);

// Hooks in the boot path.
void pc_scenario_pre_stage(void);      // OnePlayerSection, before NewPikiGameSection is built
void pc_scenario_on_stage_ready(void); // after GameCoreSection::finalSetup
void pc_scenario_on_tick_end(void);    // System::run, after every tick including boot
// OnePlayerSection, case ONEPLAYER_MapSelect: true when a crossesDays() scenario
// chose the next course itself (gameflow.mNextOnePlayerSectionID is then NewPikiGame).
bool pc_scenario_on_map_select(void);

// For scenarios.
int pc_scenario_stage_number(void); // 1 for the first stage, 2 after the first soft reset, ...

// For scenarios.
int pc_scenario_ticks_per_second(void); // 30 or 60
// Ticks the intro gate waited after the stage was ready (cutscene and text), before tick 0.
unsigned pc_scenario_intro_ticks(void);
void scenarioPass(void);
void scenarioFail(unsigned tick, const char* expected, const char* observed);
// printf-style: records failure with expected/observed text.
void scenarioFailf(unsigned tick, const char* expected, const char* fmt, ...);

// Scenario preconditions (issue #66), run against the game's own objects.
// Both fail through scenarioFailf and return false on a violated precondition.
// For every info in the shared Pikmin AnimMgr: the animation exists, has
// mTotalFrameCount > 0 and is not the "Null Anim" dummy. The runner calls this
// once when the stage is ready, before the scenario's onStageReady.
bool scenarioCheckAnimMgrPreconditions(unsigned tick);
// While the captain is in ThrowWait: the upper animator's motion is
// PIKIANIM_ThrowWait and info 63 has a type-2 info key whose anim-key frame is
// below the animator's frame count.
class Navi;
bool scenarioCheckThrowWaitPreconditions(unsigned tick, Navi* navi);

#endif
