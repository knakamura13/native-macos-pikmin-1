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
PcScenario* pc_scenario_make_extinction(void);
PcScenario* pc_scenario_make_onion_skip(void);
PcScenario* pc_scenario_make_objects_smoke(void);
PcScenario* pc_scenario_make_container_menu(void);
PcScenario* pc_scenario_make_captain_death(void);
PcScenario* pc_scenario_make_ship_parts(void);
PcScenario* pc_scenario_make_captain_damage(void);
PcScenario* pc_scenario_make_onion_unlock(void);
PcScenario* pc_scenario_make_onion_unlock_whistle(void);
PcScenario* pc_scenario_make_day_end(void);
PcScenario* pc_scenario_make_day_end_day29(void);
PcScenario* pc_scenario_make_extinction_day30(void);
PcScenario* pc_scenario_make_free_play(void);
PcScenario* pc_scenario_make_first_yellow(void);
PcScenario* pc_scenario_make_coop_smoke(void);
PcScenario* pc_scenario_make_coop_look(void);
PcScenario* pc_scenario_make_save_load(void);
PcScenario* pc_scenario_make_save_load_flags(void);
PcScenario* pc_scenario_make_results_nosave_map(void);
PcScenario* pc_scenario_make_results_nosave_cardselect(void);
PcScenario* pc_scenario_make_pause_continue(void);
PcScenario* pc_scenario_make_permadeath_erase(void);
PcScenario* pc_scenario_make_permadeath_unsaved(void);
PcScenario* pc_scenario_make_permadeath_bad_slot(void);
PcScenario* pc_scenario_make_permadeath_off(void);
PcScenario* pc_scenario_make_card_errors(void);
PcScenario* pc_scenario_make_card_errors_saved(void);

#endif
