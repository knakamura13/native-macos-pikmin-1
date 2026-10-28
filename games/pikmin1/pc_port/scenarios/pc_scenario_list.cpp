#include "scenarios/pc_scenario_list.h"

#if PIKI_SCENARIOS

#include <cstdlib>

#include "GameStat.h"

namespace {

// Runner self-test (scenario_exit_atexit_fail): exit(0) before any verdict, as
// System::halt and PPCHalt do. The runner's exit handler must turn it into exit 1.
class ExitZero : public PcScenario {
public:
	const char* name() const override { return "exit_zero"; }
	bool testOnly() const override { return true; }
	void onTick(unsigned tick) override
	{
		if (tick >= 30) std::exit(0);
	}
};

// Runner self-test (scenario_gamestat_corrupt): at tick 5 a counter is raised with
// no Pikmin behind it. The runner's GameStat invariant check must fail the run
// (exit 1) on that same tick; this scenario never reports a verdict itself.
class GameStatCorrupt : public PcScenario {
public:
	const char* name() const override { return "gamestat_corrupt"; }
	bool testOnly() const override { return true; }
	void onTick(unsigned tick) override
	{
		if (tick == 5) GameStat::workPikis.add(Blue, 3);
	}
};

} // namespace

const std::vector<PcScenario*>& pc_scenario_all(void)
{
	static const std::vector<PcScenario*> all = {
		pc_scenario_make_boot_smoke(),
		pc_scenario_make_throw_release(),
		pc_scenario_make_day30_clock(),
		pc_scenario_make_sprout_landing(),
		pc_scenario_make_onion_skip(),
		pc_scenario_make_objects_smoke(),
		pc_scenario_make_container_menu(),
		pc_scenario_make_extinction(),
		pc_scenario_make_captain_death(),
		[] { static GameStatCorrupt s; return static_cast<PcScenario*>(&s); }(),
		[] { static ExitZero s; return static_cast<PcScenario*>(&s); }(),
	};
	return all;
}

#endif
