#include "scenarios/pc_scenario_screenshot.h"

#if PIKI_SCENARIOS

#include "scenarios/pc_scenario.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "gl/pc_gfx.h"
#include "pc_window.h"
#include "settings/pc_settings.h"

namespace {

struct Capture {
	unsigned tick = 0;
	std::string path;
};

std::vector<Capture> sCaptures;
int sBaseWindowW = 64;
int sBaseWindowH = 36;
int sCaptureWindowW = 128;
int sCaptureWindowH = 72;
bool sSizedUpThisTick = false;

void parseCaptureWindow()
{
	if (const char* env = std::getenv("PIKMIN_SCENARIO_SCREENSHOT_WINDOW")) {
		int w = 0, h = 0;
		char tail = '\0';
		if (std::sscanf(env, "%dx%d%c", &w, &h, &tail) == 2 && w > 0 && h > 0 && w <= 8192 && h <= 8192) {
			sCaptureWindowW = w;
			sCaptureWindowH = h;
		}
	}
}

void applyWindowSize(int w, int h)
{
	pc_settings_scenario_set_window_size(w, h);
	pc_window_set_window_size(w, h);
	pc_gfx_set_render_resolution(w, h);
}

std::vector<Capture> capturesForTick(unsigned tick)
{
	std::vector<Capture> out;
	for (const Capture& c : sCaptures) {
		if (c.tick == tick) out.push_back(c);
	}
	return out;
}

} // namespace

bool pc_scenario_screenshot_parse_arg(const char* tickAndPath)
{
	if (tickAndPath == nullptr || *tickAndPath == '\0') return false;
	const char* colon = std::strchr(tickAndPath, ':');
	if (colon == nullptr || colon == tickAndPath || colon[1] == '\0') return false;
	std::string tickStr(tickAndPath, colon - tickAndPath);
	char* end = nullptr;
	unsigned long tick = std::strtoul(tickStr.c_str(), &end, 10);
	if (end == tickStr.c_str() || *end != '\0') return false;
	sCaptures.push_back({ static_cast<unsigned>(tick), std::string(colon + 1) });
	return true;
}

void pc_scenario_screenshot_set_base_window_size(int width, int height)
{
	if (width > 0 && height > 0) {
		sBaseWindowW = width;
		sBaseWindowH = height;
	}
	parseCaptureWindow();
}

bool pc_scenario_screenshot_any(void) { return !sCaptures.empty(); }

void pc_scenario_screenshot_on_tick_begin(unsigned tick)
{
	sSizedUpThisTick = false;
	if (sCaptures.empty()) return;
	if (capturesForTick(tick).empty()) return;
	applyWindowSize(sCaptureWindowW, sCaptureWindowH);
	sSizedUpThisTick = true;
}

bool writeCapture(const Capture& c, unsigned tick, unsigned sourceFramebuffer, int width, int height)
{
	std::error_code ec;
	const std::filesystem::path out(c.path);
	if (out.has_parent_path()) std::filesystem::create_directories(out.parent_path(), ec);
	int w = 0, h = 0;
	if (!pc_gfx_write_scene_png(c.path.c_str(), sourceFramebuffer, width, height, &w, &h)) {
		scenarioFailf(tick, "scenario screenshot PNG written", "write failed for %s", c.path.c_str());
		return false;
	}
	return true;
}

void pc_scenario_screenshot_on_present(unsigned tick, unsigned sourceFramebuffer, int width, int height)
{
	for (const Capture& c : capturesForTick(tick)) {
		if (!writeCapture(c, tick, sourceFramebuffer, width, height)) return;
	}
}

void pc_scenario_screenshot_present_hook(unsigned sourceFramebuffer, int width, int height)
{
	if (!pc_scenario_screenshot_frame_active() || sCaptures.empty() || width <= 0 || height <= 0) return;
	pc_scenario_screenshot_on_present(pc_scenario_sim_tick(), sourceFramebuffer, width, height);
}

void pc_scenario_screenshot_on_tick_end(unsigned tick)
{
	if (sSizedUpThisTick) applyWindowSize(sBaseWindowW, sBaseWindowH);
	sSizedUpThisTick = false;
	(void)tick;
}

#endif
