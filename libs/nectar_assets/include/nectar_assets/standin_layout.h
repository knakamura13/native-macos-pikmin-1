#pragma once

// Approximate GameCube 640x480 HUD layout for stand-in .blo files (issue #115).
// Positions follow the retail play09/play10/play_day screens; tolerances in tests
// allow a few pixels of slack for stand-in art tweaks.

namespace nectar::standin_layout {

struct Rect {
	int x, y, w, h;
};

inline constexpr int kScreenW = 640;
inline constexpr int kScreenH = 480;

inline constexpr Rect kPlay10Life { 552, 400, 72, 72 };
inline constexpr Rect kPlay10Navi { 564, 412, 48, 48 };
inline constexpr Rect kPlay10LiI { 564, 412, 48, 48 };
inline constexpr Rect kPlay10NaI { 552, 400, 72, 72 };

inline constexpr Rect kPlay09Stat { 0, 0, 640, 40 };
inline constexpr Rect kPlay09Goal { 280, 0, 80, 40 };
inline constexpr Rect kPlay09Suni { 300, 4, 320, 32 };
inline constexpr Rect kPlay09Sunc { 308, 8, 24, 24 };
inline constexpr Rect kPlay09Sun1 { 340, 6, 28, 28 };
inline constexpr Rect kPlay09Sun2 { 380, 6, 28, 28 };
inline constexpr Rect kPlay09Moon { 420, 6, 28, 28 };

inline constexpr Rect kPlayDayPiki { 8, 392, 48, 48 };
// Bottom-centre digit grid (left of the life gauge); last digit stays inside 640.
inline constexpr Rect kPlayDayCounters { 40, 416, 472, 56 };
inline constexpr Rect kPlayDayDate { 24, 52, 120, 32 };

inline constexpr int kCounterCell = 24;
inline constexpr int kCounterStep = 26;
inline constexpr int kCounterRow0Y  = 416;
inline constexpr int kCounterRow1Y  = 444;
inline constexpr int kCounterStartX = 48;
inline constexpr int kTopBandMaxY = 120;

inline constexpr bool rectInsideScreen(Rect r)
{
	return r.x >= 0 && r.y >= 0 && r.x + r.w <= kScreenW && r.y + r.h <= kScreenH;
}

inline constexpr bool rectsOverlap(Rect a, Rect b)
{
	return a.x < b.x + b.w && a.x + a.w > b.x && a.y < b.y + b.h && a.y + a.h > b.y;
}

} // namespace nectar::standin_layout
