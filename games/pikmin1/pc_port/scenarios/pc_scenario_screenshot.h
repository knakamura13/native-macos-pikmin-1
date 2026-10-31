#pragma once

#if PIKI_SCENARIOS

// --scenario-screenshot <tick>:<path> (repeatable). Parsed from argv before the window exists.
bool pc_scenario_screenshot_parse_arg(const char* tickAndPath);
void pc_scenario_screenshot_set_base_window_size(int width, int height);
bool pc_scenario_screenshot_any(void);

// Called from System::run before idle() on scenario ticks after the intro gate.
void pc_scenario_screenshot_on_tick_begin(unsigned tick);
// Called from pc_gfx_present after the finished frame is ready (before FB invalidate).
void pc_scenario_screenshot_present_hook(unsigned sourceFramebuffer, int width, int height);
// Called from pc_scenario_on_tick_end after onTick for the same tick.
void pc_scenario_screenshot_on_tick_end(unsigned tick);

#endif
