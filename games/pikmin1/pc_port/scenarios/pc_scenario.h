#pragma once
// Scripted gameplay scenarios (--scenario <name>), compiled only when
// PIKMIN_BUILD_SCENARIOS is ON (PIKI_SCENARIOS=1). A scenario boots straight
// into a stage, feeds pad input through an in-process provider, reads game
// objects directly and exits with a verdict.
//
// Exit codes: 0 pass, 1 assertion failed (one stderr line), 2 unknown scenario
// or bad arguments (before any window exists), 4 window or GL context creation
// failed, 77 skip because no game data (PIKMIN_SCENARIO_DATA_DIR). After a
// --scenario-handoff, closing the window exits 0.

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
	// Local co-op: two captains in split screen. Pad 1 is connected and drives
	// the second captain (providePad1). The runner arms co-op before the stage
	// is built and draws the split screen's two views (see pc_scenario.cpp).
	virtual bool coop() const { return false; }
	// Fills pad 1 (the second captain's) in a co-op scenario, after providePads.
	virtual void providePad1(unsigned /*tick*/, PcInputPad& /*pad1*/) { }
	// True for a scenario that calls scenarioHandOff(); only those accept --scenario-handoff.
	virtual bool handsOff() const { return false; }
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
// Ticks the intro gate waited after the stage was ready (cutscene and text), before tick 0.
unsigned pc_scenario_intro_ticks(void);
void scenarioPass(void);
void scenarioFail(unsigned tick, const char* expected, const char* observed);
// printf-style: records failure with expected/observed text.
void scenarioFailf(unsigned tick, const char* expected, const char* fmt, ...);
// The scenario's hand-off point (issue #76). In a --scenario-handoff run the
// scripted pad is detached, deterministic and unthrottled mode end, the
// watchdog and every check stop, and the real keyboard, mouse and controllers
// drive the game until the window is closed (exit 0). The scratch save
// directory and the settings layer stay. In any other run it is a pass: the
// scripted part reached the state it promises.
void scenarioHandOff(void);
// True once a --scenario-handoff run has handed the game to the player.
bool pc_scenario_handed_off(void);
// The disc's parms/key.bin bindings (throw A, cursor B, pluck A, attack A,
// disband X, menu Y). The stand-in tree has no key.bin, and KeyConfig's code
// defaults put pluck on B. Read from the disc 2026-10-28 (scenario_onion_unlock.cpp).
void scenarioApplyDiscKeyBindings(void);

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
