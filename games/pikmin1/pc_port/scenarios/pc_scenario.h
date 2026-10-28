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
	// Runs once when the stage is ready, before tick 0.
	virtual void onStageReady() { }
	// Fills pad 0 for this tick; pads 1-3 arrive neutral. Called every tick, before the sim.
	virtual void providePads(unsigned /*tick*/, PcInputPad& /*pad0*/) { }
	// Runs after each tick's sim. Report a verdict with scenarioPass() / scenarioFail().
	virtual void onTick(unsigned tick) = 0;
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

// For scenarios.
int pc_scenario_ticks_per_second(void); // 30 or 60
void scenarioPass(void);
void scenarioFail(unsigned tick, const char* expected, const char* observed);
// printf-style: records failure with expected/observed text.
void scenarioFailf(unsigned tick, const char* expected, const char* fmt, ...);

#endif
