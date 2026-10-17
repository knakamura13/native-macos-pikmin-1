#pragma once
// Quick Save (experimental): save at any point of a day and later load that
// exact point, by replaying the day.
//
// With the Quick Save setting on (or PIKMIN_QUICKSAVE=1) the game runs in
// deterministic mode from launch and keeps the running day's input in
// memory, one PKNI v4 record per tick from the tick after the day starts.
//
// Save (the Quick Save action, F6 by default, while a day is being played):
// <save root>/quicksave/<slot>.qsv (pc_quicksave_file.h) gets the day's
// start (the card block, the clock and the sim state that carries over from
// day to day outside the card, such as the throw preferences), every input
// since, the state hash at this tick and the view and window state. The
// game itself is untouched.
//
// Load (the Quick Load action, F7 by default, inside a day; or --quick-load
// <file> at launch): the file is checked against the running game (CRC,
// build, region, sim settings, test switches, mode) before anything
// changes. The running day is left, the recorded day is entered through the
// normal day start (so the RNG reseeds), its input is replayed unthrottled
// with the audio muted, and at the saved tick the state hash is compared. On
// a match the view and window state come back and the next tick is live
// input; recording carries on, so the day can be saved again. On a mismatch
// the game stops with a message: the day is not the recorded one.
//
// A load re-enters a story day from its card block, as the file select loads
// a save. A day the game reached by continuing the previous one in memory,
// without loading the file, can start differently from that block (the card
// keeps buried Pikmin at whole-unit positions and drops their facing); the
// hash check at the saved tick then refuses the load rather than continue a
// different day.
//
// Story mode and VS only; not with Permadeath, Speedrun, the Randomizer, the
// Challenge Mode or a netplay session. With the feature off nothing here does
// anything: no deterministic mode, no recording, the actions do nothing.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pc_quicksave_exit {
constexpr int kRefused    = 6; // --quick-load: the file cannot be loaded into this game
constexpr int kLoadFailed = 7; // a load did not reproduce the recorded day
} // namespace pc_quicksave_exit

// Launch (pc_main, after the settings are loaded): reads the setting,
// PIKMIN_QUICKSAVE and --quick-load <file>. A change of the setting takes
// effect at the next launch.
void pc_quicksave_init(int argc, char** argv);
bool pc_quicksave_enabled(void);

// The bound actions (pc_window). A request acts at the end of the tick.
void pc_quicksave_request_save(void);
void pc_quicksave_request_load(void); // the running day's slot file
void pc_quicksave_request_load_file(const char* path);

// Game flow hooks. Each one does nothing with the feature off.
void pc_quicksave_day_prepare(void);  // NewPikiGameSection: before the day is built
void pc_quicksave_day_begin(void);    // GameCoreSection: right after the RNG reseed
void pc_quicksave_note_running(void); // RunningModeState::update, every tick
// RunningModeState: a load wants the running day left. 0 no; 1 to the card
// select (a story day); 2 straight into the day again (VS, as a rematch).
int pc_quicksave_take_leave(void);
bool pc_quicksave_boot_pending(void); // logo: a --quick-load is waiting (no logo pause)
bool pc_quicksave_take_title(void);   // title: a story --quick-load starts its run
bool pc_quicksave_take_setup(void);   // CardSelect: skip the menus for a story load
int pc_quicksave_setup_slot(void);    // ...and the card file to commit (1-3, 0 none)
// CardSelect exit: puts the recorded day's start back (the card block, the
// clock, the stage). True when it did; the next section is the day.
bool pc_quicksave_take_enter(void);
void pc_quicksave_tick_end(void); // System::run, after the state hash

// Defined by the game (gameCoreSection.cpp): the cameras of the running day,
// written and read back as an opaque block.
bool pc_gamecore_view_state_save(std::vector<uint8_t>& out);
bool pc_gamecore_view_state_restore(const uint8_t* data, size_t len);
