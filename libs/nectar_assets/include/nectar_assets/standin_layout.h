#pragma once

// Approximate GameCube 640x480 HUD layout for stand-in .blo files (issue #115).
// Positions follow the retail play09/play10/play_day screens; tolerances in tests
// allow a few pixels of slack for stand-in art tweaks.

namespace nectar::standin_layout {

struct Rect {
	int x, y, w, h;
};

inline constexpr Rect kPlay10Life { 528, 384, 96, 96 };
inline constexpr Rect kPlay10Navi { 552, 408, 64, 64 };
inline constexpr Rect kPlay10LiI { 552, 408, 64, 64 };
inline constexpr Rect kPlay10NaI { 528, 384, 96, 96 };

inline constexpr Rect kPlay09Stat { 0, 0, 640, 40 };
inline constexpr Rect kPlay09Goal { 280, 0, 80, 40 };
inline constexpr Rect kPlay09Suni { 300, 4, 320, 32 };
inline constexpr Rect kPlay09Sunc { 308, 8, 24, 24 };
inline constexpr Rect kPlay09Sun1 { 340, 6, 28, 28 };
inline constexpr Rect kPlay09Sun2 { 380, 6, 28, 28 };
inline constexpr Rect kPlay09Moon { 420, 6, 28, 28 };

inline constexpr Rect kPlayDayPiki { 8, 392, 48, 48 };
inline constexpr Rect kPlayDayCounters { 468, 404, 160, 64 };
inline constexpr Rect kPlayDayDate { 24, 52, 120, 32 };

inline constexpr int kBottomRightMinX = 440;
inline constexpr int kBottomRightMinY = 360;
inline constexpr int kTopBandMaxY     = 120;

} // namespace nectar::standin_layout
