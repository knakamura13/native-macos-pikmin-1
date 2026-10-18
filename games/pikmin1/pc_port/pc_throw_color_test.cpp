/**
 * @file pc_throw_color_test.cpp
 * @brief Throw colour stays unset until the player picks one.
 *
 * Colours match PikiColor: blue 0, red 1, yellow 2. The old assignment is
 * inlined here as the behaviour this change replaces, so an explicit notch
 * can be checked against it without running the game.
 */

#include "pc_throw_color.h"

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

/// The pre-fix assignment. Index 0 when nothing matched, including -1.
int legacyNext(int preferred, int steps, const int* present, int presentCount)
{
	if (presentCount <= 0) {
		return -1;
	}
	int index = 0;
	for (int i = 0; i < presentCount; i++) {
		if (present[i] == preferred) {
			index = i;
			break;
		}
	}
	if (steps != 0) {
		index = ((index + steps) % presentCount + presentCount) % presentCount;
	}
	return present[index];
}

} // namespace

int main()
{
	const int mixed[]   = {0, 1, 2};
	const int noBlue[]  = {1, 2};
	const int yellow[]  = {2};

	// No selection. The old path picked the first present colour.
	check(legacyNext(-1, 0, mixed, 3) == 0, "old mixed squad picked blue");
	check(pc_next_preferred_throw_color(-1, 0, mixed, 3) == -1, "mixed squad stays unset");
	check(legacyNext(-1, 0, noBlue, 2) == 1, "old red/yellow squad picked red");
	check(pc_next_preferred_throw_color(-1, 0, noBlue, 2) == -1, "red/yellow squad stays unset");
	check(legacyNext(-1, 0, yellow, 1) == 2, "old single colour picked yellow");
	check(pc_next_preferred_throw_color(-1, 0, yellow, 1) == -1, "single colour stays unset");

	// Walk frames call this with no notches. The choice must stay unset.
	int preferred = -1;
	preferred     = pc_next_preferred_throw_color(preferred, 0, mixed, 3);
	check(preferred == -1, "a later walk frame still has no colour");

	// The first explicit notch lands on the same colour as before, because
	// the old code had already stored blue and then stepped once.
	check(pc_next_preferred_throw_color(-1, 1, mixed, 3) == legacyNext(0, 1, mixed, 3),
	      "first forward notch matches the old first notch");
	check(pc_next_preferred_throw_color(-1, 1, mixed, 3) == 1, "first forward notch is red");
	check(pc_next_preferred_throw_color(-1, -1, mixed, 3) == legacyNext(0, -1, mixed, 3),
	      "first backward notch matches the old first notch");
	check(pc_next_preferred_throw_color(-1, -1, mixed, 3) == 2, "first backward notch is yellow");
	check(pc_next_preferred_throw_color(-1, 2, mixed, 3) == legacyNext(0, 2, mixed, 3),
	      "two banked notches match the old step");
	check(pc_next_preferred_throw_color(-1, 1, yellow, 1) == 2, "a notch on one colour selects it");

	// After a colour is chosen, cycling and the empty-colour snap stay put.
	check(pc_next_preferred_throw_color(1, 1, mixed, 3) == legacyNext(1, 1, mixed, 3),
	      "a later notch matches the old cycle");
	check(pc_next_preferred_throw_color(1, 1, mixed, 3) == 2, "red then forward is yellow");
	check(pc_next_preferred_throw_color(0, -1, mixed, 3) == 2, "blue then backward is yellow");
	check(pc_next_preferred_throw_color(0, 0, noBlue, 2) == legacyNext(0, 0, noBlue, 2),
	      "a missing colour still snaps");
	check(pc_next_preferred_throw_color(0, 0, noBlue, 2) == 1, "missing blue snaps to red");
	check(pc_next_preferred_throw_color(1, 0, mixed, 0) == -1, "an empty squad clears the colour");

	// Captain 1 is the keyboard owner by default, including with a gamepad
	// in that slot. Armed means the selector runs. It still does not pick.
	check(pc_throw_color_wheel_armed(true, false, 0), "owner with Pikmin Colour is armed");
	check(!pc_throw_color_wheel_armed(false, false, 0), "the other captain is not armed");
	check(!pc_throw_color_wheel_armed(true, false, 1), "Camera Zoom leaves the owner idle");
	check(pc_throw_color_wheel_armed(true, true, 1), "deterministic mode arms the owner anyway");
	check(!pc_throw_color_wheel_armed(false, true, 0), "deterministic mode does not arm the other captain");
	check(pc_throw_color_keep_unset(false, 0, 0, -1), "an idle captain with no colour stays idle");
	check(!pc_throw_color_keep_unset(true, 0, 0, -1), "an armed captain is not kept out of the selector");
	check(!pc_throw_color_keep_unset(false, 0, 1, -1), "a D-pad press is not idle");
	check(!pc_throw_color_keep_unset(false, 0, 0, 0), "a chosen colour is not cleared by going idle");

	// Armed, no notch: the gamepad case. The selector runs and leaves -1,
	// so the throw-wait search stays at 80 rather than 200.
	check(pc_throw_color_wheel_armed(true, false, 0), "gamepad on captain 1 is still the armed owner");
	check(pc_next_preferred_throw_color(-1, 0, mixed, 3) == -1, "armed and untouched stays unset");

	if (sFailures == 0) {
		std::printf("pc_throw_color_test: all checks passed\n");
		return 0;
	}
	std::printf("pc_throw_color_test: %d failure(s)\n", sFailures);
	return 1;
}
