#include "pc_menu_click.h"

#include <cmath>

static bool sPending     = false;
static float sNx         = 0.0f;
static float sNy         = 0.0f;
static int sTick         = 0;
static int sPostedTick   = -100;
static bool sHasPointer  = false;
static float sPointerNx  = 0.0f;
static float sPointerNy  = 0.0f;
static bool sSeenPointer = false;
static bool sMouseDrives = false;
static bool sActivity    = false;
static int sClaimTick    = -100;

void pc_menu_click_output_area(int drawableW, int drawableH, float renderedAspect, int* outX, int* outY, int* outW, int* outH)
{
	const float windowAspect = float(drawableW) / float(drawableH);
	int x                    = 0;
	int y                    = 0;
	int w                    = 0;
	int h                    = 0;
	if (renderedAspect >= windowAspect) {
		w = drawableW;
		h = int(lroundf(float(drawableW) / renderedAspect));
		x = 0;
		y = (drawableH - h) / 2;
	} else {
		h = drawableH;
		w = int(lroundf(float(drawableH) * renderedAspect));
		x = (drawableW - w) / 2;
		y = 0;
	}
	if (outX) {
		*outX = x;
	}
	if (outY) {
		*outY = y;
	}
	if (outW) {
		*outW = w;
	}
	if (outH) {
		*outH = h;
	}
}

bool pc_menu_click_map_to_graph(const PcMenuClickFrame& frame, float nx, float ny, float* x, float* y)
{
	if (frame.drawableW <= 0 || frame.drawableH <= 0 || frame.outW <= 0 || frame.outH <= 0 || frame.graphW <= 0 || frame.graphH <= 0) {
		return false;
	}
	if (!(frame.scaleX > 0.0f) || !(frame.scaleY > 0.0f) || !(frame.targetW > 0.0f) || !(frame.targetH > 0.0f)) {
		return false;
	}
	const float winX          = nx * float(frame.drawableW);
	const float winY          = ny * float(frame.drawableH);
	const float rtX           = (winX - float(frame.outX)) / float(frame.outW) * frame.targetW;
	const float rtYUp         = (1.0f - (winY - float(frame.outY)) / float(frame.outH)) * frame.targetH;
	const float localX        = (rtX - frame.offsetX) / frame.scaleX;
	const float fromBottom    = (rtYUp - frame.offsetY) / frame.scaleY;
	if (x) {
		*x = localX;
	}
	if (y) {
		*y = float(frame.graphH) - fromBottom;
	}
	return true;
}

bool pc_menu_click_is_button_rect(PcMenuClickRect pane, PcMenuClickRect root)
{
	const int paneW = pane.maxX - pane.minX;
	const int paneH = pane.maxY - pane.minY;
	const int rootW = root.maxX - root.minX;
	const int rootH = root.maxY - root.minY;
	if (paneW <= 0 || paneH <= 0 || rootW <= 0 || rootH <= 0) {
		return false;
	}
	const bool coversRoot = paneW >= rootW && paneH >= rootH;
	return !coversRoot;
}

int pc_menu_click_hit(float x, float y, const PcMenuClickRect* rects, const int* active, int count, float margin)
{
	if (!rects || count <= 0) {
		return -1;
	}
	// A text pane is taller than the row pitch, so neighbours overlap. The
	// nearer centre wins. Margin only applies when the point is in neither row.
	for (int pass = 0; pass < 2; pass++) {
		const float slack = pass == 0 ? 0.0f : margin;
		int best          = -1;
		float bestDist    = 0.0f;
		for (int i = 0; i < count; i++) {
			const PcMenuClickRect& b = rects[i];
			if (x < float(b.minX) - slack || x > float(b.maxX) + slack || y < float(b.minY) - slack || y > float(b.maxY) + slack) {
				continue;
			}
			const float dx   = x - 0.5f * float(b.minX + b.maxX);
			const float dy   = y - 0.5f * float(b.minY + b.maxY);
			const float dist = dx * dx + dy * dy;
			if (best < 0 || dist < bestDist) {
				best     = i;
				bestDist = dist;
			}
		}
		if (best >= 0) {
			return (active && !active[best]) ? -1 : best;
		}
	}
	return -1;
}

static bool pc_menu_click_is_menu_screen(int screen)
{
	return screen == PC_MENU_CLICK_TITLE || screen == PC_MENU_CLICK_MAIN;
}

PcMenuClickResult pc_menu_click_hover(int screen, float x, float y, const PcMenuClickRect* rects, const int* active, int count,
                                      float margin)
{
	PcMenuClickResult result;
	result.action = PC_MENU_CLICK_IGNORE;
	result.index  = -1;
	if (!pc_menu_click_is_menu_screen(screen)) {
		return result;
	}
	const int hit = pc_menu_click_hit(x, y, rects, active, count, margin);
	if (hit < 0) {
		return result;
	}
	result.index  = hit;
	result.action = PC_MENU_CLICK_HIGHLIGHT;
	return result;
}

PcMenuClickResult pc_menu_click_resolve(int screen, float x, float y, const PcMenuClickRect* rects, const int* active, int count,
                                        float margin)
{
	PcMenuClickResult result;
	result.action = PC_MENU_CLICK_IGNORE;
	result.index  = -1;
	if (!pc_menu_click_is_menu_screen(screen)) {
		return result;
	}
	const int hit = pc_menu_click_hit(x, y, rects, active, count, margin);
	if (hit < 0) {
		return result;
	}
	result.index  = hit;
	result.action = (screen == PC_MENU_CLICK_TITLE) ? PC_MENU_CLICK_PRESS_START : PC_MENU_CLICK_ACTIVATE;
	return result;
}

void pc_menu_click_reset(void)
{
	sPending     = false;
	sNx          = 0.0f;
	sNy          = 0.0f;
	sTick        = 0;
	sPostedTick  = -100;
	sHasPointer  = false;
	sPointerNx   = 0.0f;
	sPointerNy   = 0.0f;
	sSeenPointer = false;
	sMouseDrives = false;
	sActivity    = false;
	sClaimTick   = -100;
}

void pc_menu_click_set_pointer(float nx, float ny)
{
	if (sSeenPointer && (nx != sPointerNx || ny != sPointerNy)) {
		sMouseDrives = true;
		sActivity    = true;
	}
	sSeenPointer = true;
	sHasPointer  = true;
	sPointerNx   = nx;
	sPointerNy   = ny;
}

void pc_menu_click_clear_pointer(void)
{
	sHasPointer = false;
}

void pc_menu_click_disarm_hover(void)
{
	sMouseDrives = false;
}

bool pc_menu_click_read_pointer_normalized(float* nx, float* ny)
{
	if (!sHasPointer || !sMouseDrives) {
		return false;
	}
	if (nx) {
		*nx = sPointerNx;
	}
	if (ny) {
		*ny = sPointerNy;
	}
	return true;
}

bool pc_menu_click_take_activity(void)
{
	const bool was = sActivity;
	sActivity      = false;
	return was;
}

void pc_menu_click_claim_pointer_menu(void)
{
	sClaimTick = sTick;
}

bool pc_menu_click_pointer_menu_active(void)
{
	return sTick - sClaimTick <= 2;
}

void pc_menu_click_begin_tick(void)
{
	sTick++;
	if (sPending && (sTick - sPostedTick) > 1) {
		sPending = false;
	}
}

void pc_menu_click_post(float nx, float ny)
{
	sActivity   = true;
	sPending    = true;
	sNx         = nx;
	sNy         = ny;
	sPostedTick = sTick;
}

void pc_menu_click_clear(void)
{
	sPending = false;
}

bool pc_menu_click_pending(float* nx, float* ny)
{
	if (!sPending) {
		return false;
	}
	if (nx) {
		*nx = sNx;
	}
	if (ny) {
		*ny = sNy;
	}
	return true;
}

bool pc_menu_click_take(float* nx, float* ny)
{
	if (!sPending) {
		return false;
	}
	if (nx) {
		*nx = sNx;
	}
	if (ny) {
		*ny = sNy;
	}
	sPending = false;
	return true;
}
