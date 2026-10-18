#ifndef PC_MENU_CLICK_H
#define PC_MENU_CLICK_H

// Pointer hits for the Pikmin 1 title screen and the main menu only.
//
// Classic mode still ignores mouse buttons as game input. These two screens
// take a click anyway. While one of them is showing, a mouse button is not
// also A or B, so one click cannot start the menu and activate a row.
//
// Graph coordinates match P2DPane::getGlobalBounds() after the menu is drawn.
// The window point is stored normalized. Mapping into the graph removes the
// present letterbox and the menu scale, so a 4:3 menu inside a wide window
// and a wide menu in a taller window land on the same panes.

enum PcMenuClickScreen {
	PC_MENU_CLICK_NONE  = 0,
	PC_MENU_CLICK_TITLE = 1, // Press Start
	PC_MENU_CLICK_MAIN  = 2, // the title's main menu rows
};

enum PcMenuClickAction {
	PC_MENU_CLICK_IGNORE      = 0,
	PC_MENU_CLICK_PRESS_START = 1,
	PC_MENU_CLICK_ACTIVATE    = 2,
	PC_MENU_CLICK_HIGHLIGHT   = 3, // over a button; do not activate
};

struct PcMenuClickRect {
	int minX;
	int minY;
	int maxX;
	int maxY;
};

struct PcMenuClickResult {
	int action; // PcMenuClickAction
	int index;  // hit button, or -1
};

// Present blit rectangle, then the menu scale that maps graph pixels onto the
// render target. scale/offset are the menu's own (hud-wide or 4:3), already in
// graph pixels, not a fixed 640 that still needs stretching.
struct PcMenuClickFrame {
	int drawableW;
	int drawableH;
	int outX;
	int outY;
	int outW;
	int outH;
	float scaleX;
	float scaleY;
	float offsetX;
	float offsetY;
	float targetW;
	float targetH;
	int graphW;
	int graphH;
};

// Same slack the touch menus use around a row.
constexpr float kPcMenuClickMargin = 8.0f;

// Letterbox of a rendered aspect inside the drawable window. Same split the
// present blit uses.
void pc_menu_click_output_area(int drawableW, int drawableH, float renderedAspect, int* outX, int* outY, int* outW,
                               int* outH);

// Window-normalized point to graph pixels for one frame. False when a size is
// not positive. A point in the letterbox falls outside the graph.
bool pc_menu_click_map_to_graph(const PcMenuClickFrame& frame, float nx, float ny, float* x, float* y);

// A full-screen backdrop covers the root on both axes. A button does not.
bool pc_menu_click_is_button_rect(PcMenuClickRect pane, PcMenuClickRect root);

// Nearest rect that contains the point. Overlapping rows use the nearest
// centre. The margin is only the second pass. -1 on a miss, and when the
// nearest row is inactive. active may be null.
int pc_menu_click_hit(float x, float y, const PcMenuClickRect* rects, const int* active, int count, float margin);

PcMenuClickResult pc_menu_click_hover(int screen, float x, float y, const PcMenuClickRect* rects, const int* active,
                                      int count, float margin);
PcMenuClickResult pc_menu_click_resolve(int screen, float x, float y, const PcMenuClickRect* rects, const int* active,
                                        int count, float margin);

// Latest pointer, normalized to the window. Motion arms the highlight. The
// first sample after a reset is not a move. A key, a controller, or a mouse
// button disarms it until the pointer moves again, so a resting pointer does
// not take the highlight back from the keyboard.
void pc_menu_click_set_pointer(float nx, float ny);
void pc_menu_click_clear_pointer(void);
void pc_menu_click_disarm_hover(void);
bool pc_menu_click_read_pointer_normalized(float* nx, float* ny);

// True once after pointer motion or a click. Taking it clears it. The main
// menu uses this so the idle return to Press Start waits out a mouse user.
bool pc_menu_click_take_activity(void);

// The title screen and its main menu call this every update they are shown.
// The claim still holds on the next input poll (and one poll after that),
// which is the poll that would otherwise turn the same click into A.
void pc_menu_click_claim_pointer_menu(void);
bool pc_menu_click_pointer_menu_active(void);

// One pending click. begin_tick() runs on the logical input sample, not on the
// extra poll at the end of the frame. A click nobody takes is dropped on the
// sample after the next one. pending() does not consume it; take() does.
void pc_menu_click_reset(void);
void pc_menu_click_begin_tick(void);
void pc_menu_click_post(float nx, float ny);
void pc_menu_click_clear(void);
bool pc_menu_click_pending(float* nx, float* ny);
bool pc_menu_click_take(float* nx, float* ny);

#endif
