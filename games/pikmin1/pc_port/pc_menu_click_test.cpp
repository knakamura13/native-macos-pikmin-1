/**
 * @file pc_menu_click_test.cpp
 * @brief Title and main-menu pointer hits, without a window.
 *
 * The mapping cases are the present letterbox and the menu scale: a wide
 * window, a 4:3 menu inside that window, and a tall window with bars. Moving
 * across rows changes the highlight and does not activate. A click activates
 * once. Empty space, an inactive row, and any other screen do not.
 */

#include "pc_menu_click.h"

#include <cmath>
#include <cstdio>

namespace {

int sFailures = 0;

void check(bool condition, const char* what)
{
	if (!condition) {
		std::printf("FAIL: %s\n", what);
		++sFailures;
	}
}

bool near(float got, float want)
{
	return std::fabs(got - want) < 0.5f;
}

void menuScale(int targetW, int targetH, int graphW, int graphH, float* scaleX, float* scaleY, float* offsetX, float* offsetY)
{
	const float scale = std::fmin(float(targetW) / float(graphW), float(targetH) / float(graphH));
	*scaleX           = scale;
	*scaleY           = scale;
	*offsetX          = (float(targetW) - float(graphW) * scale) * 0.5f;
	*offsetY          = (float(targetH) - float(graphH) * scale) * 0.5f;
}

PcMenuClickFrame frameFor(int drawableW, int drawableH, float renderedAspect, int graphW, int graphH)
{
	PcMenuClickFrame frame {};
	frame.drawableW = drawableW;
	frame.drawableH = drawableH;
	frame.graphW    = graphW;
	frame.graphH    = graphH;
	pc_menu_click_output_area(drawableW, drawableH, renderedAspect, &frame.outX, &frame.outY, &frame.outW, &frame.outH);
	// At render scale 1 the target is the blit rectangle, and the menu scale
	// maps the graph onto that target.
	frame.targetW = float(frame.outW);
	frame.targetH = float(frame.outH);
	menuScale(frame.outW, frame.outH, graphW, graphH, &frame.scaleX, &frame.scaleY, &frame.offsetX, &frame.offsetY);
	return frame;
}

void testOutputArea()
{
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	const float wide = 16.0f / 9.0f;
	pc_menu_click_output_area(1920, 1080, wide, &x, &y, &w, &h);
	check(x == 0 && y == 0 && w == 1920 && h == 1080, "16:9 window matches a 16:9 picture");

	pc_menu_click_output_area(1920, 1200, wide, &x, &y, &w, &h);
	check(x == 0 && w == 1920 && h == 1080 && y == 60, "a taller window letterboxes a 16:9 picture");

	pc_menu_click_output_area(1920, 1080, 4.0f / 3.0f, &x, &y, &w, &h);
	check(y == 0 && h == 1080 && w == 1440 && x == 240, "a wide window pillarboxes a 4:3 picture");

	pc_menu_click_output_area(1280, 720, wide, &x, &y, &w, &h);
	check(x == 0 && y == 0 && w == 1280 && h == 720, "a smaller 16:9 window still fills");
}

void testMapCenters()
{
	const int graphW = int(lroundf(480.0f * (16.0f / 9.0f)));
	check(graphW == 853, "wide menu graph width");
	const PcMenuClickFrame hd  = frameFor(1920, 1080, 16.0f / 9.0f, graphW, 480);
	const PcMenuClickFrame hd2 = frameFor(1280, 720, 16.0f / 9.0f, graphW, 480);
	float x                     = 0.0f;
	float y                     = 0.0f;
	float x2                    = 0.0f;
	float y2                    = 0.0f;
	check(pc_menu_click_map_to_graph(hd, 0.5f, 0.5f, &x, &y), "1080p center converts");
	check(pc_menu_click_map_to_graph(hd2, 0.5f, 0.5f, &x2, &y2), "720p center converts");
	check(near(x, 426.5f) && near(y, 240.0f), "1080p center is the wide graph center");
	check(near(x2, x) && near(y2, y), "720p and 1080p centers land on the same graph point");

	const PcMenuClickFrame pillar = frameFor(1920, 1080, 16.0f / 9.0f, 640, 480);
	check(pc_menu_click_map_to_graph(pillar, 0.5f, 0.5f, &x, &y), "4:3 menu center converts");
	check(near(x, 320.0f) && near(y, 240.0f), "window center is the 4:3 menu center");
	check(pc_menu_click_map_to_graph(pillar, 10.0f / 1920.0f, 0.5f, &x, &y), "left bar converts");
	check(x < 0.0f, "a click in the side bar is outside the 4:3 menu");

	const PcMenuClickFrame tall = frameFor(1080, 1920, 16.0f / 9.0f, graphW, 480);
	check(tall.outY > 0 && tall.outH < 1920, "a portrait window bars a 16:9 picture");
	const float bandCenter = (float(tall.outY) + float(tall.outH) * 0.5f) / 1920.0f;
	check(pc_menu_click_map_to_graph(tall, 0.5f, bandCenter, &x, &y), "portrait band center converts");
	check(near(x, 426.5f) && near(y, 240.0f), "the visible band center is the graph center");
	check(pc_menu_click_map_to_graph(tall, 0.5f, 0.01f, &x, &y), "portrait bar converts");
	check(y < 0.0f || y > 480.0f, "a click in the portrait bar is outside the graph");

	PcMenuClickFrame bad = hd;
	bad.drawableW        = 0;
	check(!pc_menu_click_map_to_graph(bad, 0.5f, 0.5f, &x, &y), "a zero window does not convert");
}

void testHits()
{
	const PcMenuClickRect rows[3] = {
		{ 200, 100, 440, 140 },
		{ 200, 130, 440, 170 },
		{ 200, 180, 440, 220 },
	};
	const int active[3] = { 1, 1, 1 };
	const PcMenuClickResult hover0 = pc_menu_click_hover(PC_MENU_CLICK_MAIN, 320.0f, 110.0f, rows, active, 3, kPcMenuClickMargin);
	const PcMenuClickResult hover2 = pc_menu_click_hover(PC_MENU_CLICK_MAIN, 320.0f, 200.0f, rows, active, 3, kPcMenuClickMargin);
	check(hover0.action == PC_MENU_CLICK_HIGHLIGHT && hover0.index == 0, "the pointer highlights the row it is on");
	check(hover2.action == PC_MENU_CLICK_HIGHLIGHT && hover2.index == 2, "moving the pointer highlights the new row");
	check(hover0.action != PC_MENU_CLICK_ACTIVATE && hover2.action != PC_MENU_CLICK_ACTIVATE, "a move does not activate");

	// Overlap of row 0 (centre 120) and row 1 (centre 150). 132 is nearer 120.
	check(pc_menu_click_hit(320.0f, 132.0f, rows, active, 3, 0.0f) == 0, "the nearer row wins an overlap");
	check(pc_menu_click_hit(320.0f, 148.0f, rows, active, 3, 0.0f) == 1, "the other side of the overlap picks the next row");

	const PcMenuClickResult miss = pc_menu_click_resolve(PC_MENU_CLICK_MAIN, 10.0f, 10.0f, rows, active, 3, kPcMenuClickMargin);
	check(miss.action == PC_MENU_CLICK_IGNORE && miss.index == -1, "empty space does not activate");

	const PcMenuClickResult pressed
	    = pc_menu_click_resolve(PC_MENU_CLICK_MAIN, 320.0f, 200.0f, rows, active, 3, kPcMenuClickMargin);
	check(pressed.action == PC_MENU_CLICK_ACTIVATE && pressed.index == 2, "a click activates the row under the pointer");

	const PcMenuClickResult start
	    = pc_menu_click_resolve(PC_MENU_CLICK_TITLE, 320.0f, 200.0f, rows, active, 3, kPcMenuClickMargin);
	check(start.action == PC_MENU_CLICK_PRESS_START && start.index == 2, "a title click presses start");

	const int locked[3] = { 1, 0, 1 };
	check(pc_menu_click_hit(320.0f, 150.0f, rows, locked, 3, 0.0f) == -1, "an inactive row does not hand the click to a neighbour");

	const PcMenuClickResult other
	    = pc_menu_click_resolve(PC_MENU_CLICK_NONE, 320.0f, 200.0f, rows, active, 3, kPcMenuClickMargin);
	check(other.action == PC_MENU_CLICK_IGNORE, "another screen ignores the pointer");

	const PcMenuClickRect root = { 0, 0, 640, 480 };
	const PcMenuClickRect backdrop = { 0, 0, 640, 480 };
	const PcMenuClickRect button = { 200, 200, 440, 260 };
	check(!pc_menu_click_is_button_rect(backdrop, root), "a full-screen pane is not a button");
	check(pc_menu_click_is_button_rect(button, root), "a small pane is a button");
	const PcMenuClickRect empty = { 0, 0, 0, 0 };
	check(!pc_menu_click_is_button_rect(empty, root), "an empty pane is not a button");
}

void testClickOnce()
{
	pc_menu_click_reset();
	pc_menu_click_begin_tick();
	pc_menu_click_set_pointer(0.2f, 0.2f);
	float nx = 0.0f;
	float ny = 0.0f;
	check(!pc_menu_click_read_pointer_normalized(&nx, &ny), "the first sample does not arm the highlight");
	check(!pc_menu_click_pending(&nx, &ny), "moving the pointer does not post a click");
	pc_menu_click_set_pointer(0.4f, 0.6f);
	check(pc_menu_click_read_pointer_normalized(&nx, &ny) && near(nx, 0.4f) && near(ny, 0.6f), "a move arms the highlight");
	check(pc_menu_click_take_activity(), "pointer motion counts as activity");
	check(!pc_menu_click_take_activity(), "activity is taken once");

	pc_menu_click_disarm_hover();
	check(!pc_menu_click_read_pointer_normalized(&nx, &ny), "a key disarms the highlight");
	pc_menu_click_set_pointer(0.4f, 0.6f);
	check(!pc_menu_click_read_pointer_normalized(&nx, &ny), "a resting pointer stays disarmed");
	pc_menu_click_set_pointer(0.5f, 0.6f);
	check(pc_menu_click_read_pointer_normalized(&nx, &ny), "the next move arms the highlight again");

	pc_menu_click_post(0.25f, 0.75f);
	check(pc_menu_click_pending(&nx, &ny) && near(nx, 0.25f) && near(ny, 0.75f), "a click is pending");
	check(pc_menu_click_take(&nx, &ny) && near(nx, 0.25f), "a click is taken once");
	check(!pc_menu_click_take(&nx, &ny), "the same click does not fire again");
	check(pc_menu_click_take_activity(), "a click counts as activity");

	pc_menu_click_post(0.1f, 0.1f);
	pc_menu_click_begin_tick();
	check(pc_menu_click_pending(nullptr, nullptr), "a click survives the next sample");
	pc_menu_click_begin_tick();
	check(!pc_menu_click_pending(nullptr, nullptr), "a click nobody takes is dropped after that");

	pc_menu_click_claim_pointer_menu();
	check(pc_menu_click_pointer_menu_active(), "the menu claim holds on this sample");
	pc_menu_click_begin_tick();
	pc_menu_click_begin_tick();
	check(pc_menu_click_pointer_menu_active(), "the claim still holds for the next polls");
	pc_menu_click_begin_tick();
	check(!pc_menu_click_pointer_menu_active(), "the claim ends after the menu stops renewing it");

	pc_menu_click_clear();
	check(!pc_menu_click_pending(nullptr, nullptr), "clear drops a click before the next screen");
}

} // namespace

int main()
{
	testOutputArea();
	testMapCenters();
	testHits();
	testClickOnce();
	if (sFailures != 0) {
		std::printf("pc_menu_click_test: %d failed\n", sFailures);
		return 1;
	}
	std::printf("pc_menu_click_test: all checks passed\n");
	return 0;
}
