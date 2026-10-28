#pragma once
#if PIKI_SCENARIOS

#include "scenarios/pc_scenario.h"

#include <vector>

// Registry: every scenario compiled into the binary.
const std::vector<PcScenario*>& pc_scenario_all(void);

PcScenario* pc_scenario_make_boot_smoke(void);
PcScenario* pc_scenario_make_throw_release(void);
PcScenario* pc_scenario_make_day30_clock(void);
PcScenario* pc_scenario_make_sprout_landing(void);
PcScenario* pc_scenario_make_objects_smoke(void);

#endif
