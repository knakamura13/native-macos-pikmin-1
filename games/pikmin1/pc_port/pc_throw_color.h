#ifndef PC_THROW_COLOR_H
#define PC_THROW_COLOR_H

/**
 * @file pc_throw_color.h
 * @brief Throw-colour choice before and after the player picks one.
 *
 * The preference starts at -1, which means the original grab: the nearest
 * Pikmin of any colour within 80. Mouse-wheel mode is on by default, and
 * captain 1 is the keyboard owner even when a gamepad is driving that
 * captain. Treating "no colour yet" as index 0 of the squad then selected
 * blue, or the next present colour, on the first walk frame. The grab
 * searched that colour out to 200 and, past the hand, walked it in for up
 * to 3 seconds.
 *
 * A notch, a touch tap, or D-pad left/right still cycles. The keyboard-owner
 * slot is unchanged: this does not decide which captain a device drives.
 */

/// True when this captain follows the colour wheel rather than staying idle.
///
/// @p mouseWheelAction 0 is "Pikmin Colour". Deterministic mode ignores the
/// saved setting and uses that default, because the wheel is not in the
/// shared pad state. Being armed does not select a colour.
inline bool pc_throw_color_wheel_armed(bool keyboardOwner, bool deterministic, int mouseWheelAction)
{
	return keyboardOwner && (deterministic || mouseWheelAction == 0);
}

/// True when an unselected colour must stay unselected.
///
/// The wheel, touch, and D-pad have all contributed nothing, and this captain
/// is not armed. Callers still have to consume a banked wheel notch before
/// deciding that @p steps is zero.
inline bool pc_throw_color_keep_unset(bool wheelArmed, int touchSteps, int padSteps, int preferred)
{
	return !wheelArmed && touchSteps == 0 && padSteps == 0 && preferred < 0;
}

/**
 * @brief Next preferred colour, or -1 to keep the original grab.
 *
 * @p present is the squad colours in ascending colour order (blue, red,
 * yellow). @p steps is the notches this tick; negative steps move backward.
 * An unselected colour with no notches stays unselected. A colour that has
 * left the squad snaps to one that remains.
 */
inline int pc_next_preferred_throw_color(int preferred, int steps, const int* present, int presentCount)
{
	if (presentCount <= 0 || present == nullptr) {
		return -1;
	}
	if (preferred < 0 && steps == 0) {
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

#endif // PC_THROW_COLOR_H
