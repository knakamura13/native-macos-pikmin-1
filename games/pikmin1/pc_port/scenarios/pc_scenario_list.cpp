#include "scenarios/pc_scenario_list.h"

#if PIKI_SCENARIOS

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
	};
	return all;
}

#endif
