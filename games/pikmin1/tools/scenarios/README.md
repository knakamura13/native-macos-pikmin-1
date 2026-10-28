# scenarios

Scenario test harness. A scenario boots the real game code straight into a
stage with no window on screen, feeds pad input from a script, reads game
objects directly and exits with a pass or fail. It lives in two places:
`pc_port/scenarios/` (the runner and the scenarios, compiled into `nectar`) and
this folder (the stand-in asset generator, the CTest runner script, the manifest
and the GL probe).

## Why it exists

The disc data cannot be shipped, so CI has no game assets. Scenarios therefore
run on **stand-in assets**: files that `nectar_standin_generator` writes from
code, with the same formats the game parses and none of the disc's content. The
game code under test is the real one. The same scenarios also run against a real
install (the `_disc` tests) when you have one.

## Build

Scenarios are off by default. Configure with the scenario option and native
JAudio off (the legacy audio facade needs no sound data). Run these from the
repository root. `-G Ninja` needs Ninja installed; leave it out to use the
default generator (the commands in this note were run with Unix Makefiles;
`-G Ninja` was not run, because Ninja is not installed on that Mac):

```sh
cmake -S games/pikmin1 -B build/pikmin1-scenarios -G Ninja \
    -DPIKMIN_NATIVE_JAUDIO=OFF -DPIKMIN_BUILD_SCENARIOS=ON
cmake --build build/pikmin1-scenarios --target pikmin_pc nectar_standin_generator
```

`PIKMIN_BUILD_TESTS` defaults to ON, so this configure also registers the unit
tests: `ctest -N` lists 105 entries, 27 of them scenario entries. Most test
executables are not built by the two targets above; CTest reports them as missing
and `-R ^scenario_` leaves them out.

The same scenarios also run with the original audio engine: configure a second
build with `-DPIKMIN_NATIVE_JAUDIO=ON`. The generator writes a stand-in
`SndData/Seqs/pikiseq.arc` that lets the engine boot with no sound data; see
[standin-audio.md](standin-audio.md).

`pikmin_pc` produces `build/pikmin1-scenarios/bin/nectar`.
`nectar_standin_generator` is a separate executable, so release packaging never
ships it. `PIKMIN_BUILD_SCENARIOS` also calls `enable_testing()` and defines
`PIKI_SCENARIOS=1`; every scenario source is wrapped in `#if PIKI_SCENARIOS`.

## Run

Through CTest. CTest does not build: build the two targets above first. The
`scenario_standin_generate` entry then writes the stand-in tree and the scenario
entries run on it:

```sh
ctest --test-dir build/pikmin1-scenarios --output-on-failure -R ^scenario_ -LE game-assets
```

Directly, from the repository root. `PIKMIN_SCENARIO_DATA_DIR` is resolved
against the directory the command starts in (the process then changes into it), so
a relative path like this one works only from the root. Write the stand-in tree
first, for example with the generator (see
[Stand-in assets](#stand-in-assets)):

```sh
build/pikmin1-scenarios/nectar_standin_generator build/pikmin1-scenarios/scenario-data
PIKMIN_SCENARIO_DATA_DIR=build/pikmin1-scenarios/scenario-data \
PIKMIN_SCENARIO_STAGE=stages/sandbox.ini \
    build/pikmin1-scenarios/bin/nectar --scenario boot_smoke --scenario-tick-rate 30
```

| Argument | Meaning |
|----------|---------|
| `--scenario <name>` | Scenario to run: `boot_smoke`, `throw_release`, `day30_clock`, `captain_death`, `sprout_landing`, `onion_skip`, `container_menu` or `objects_smoke`. The last three need an object stage (`PIKMIN_SCENARIO_STAGE`, see the table below). |
| `--scenario-tick-rate 30\|60` | Ticks per second. Default `30`. Any other value (or a missing one) exits 2. |
| `--scenario-visible` | Show the window. Default is hidden. A visible window is also positioned and may move the real mouse pointer (see [Mouse pointer](#mouse-pointer)). |

| Environment variable | Meaning |
|----------------------|---------|
| `PIKMIN_SCENARIO_DATA_DIR` | Folder that holds `assets/`. The process changes into it. Unset or empty, the run exits 77. Set to a folder that cannot be entered, it exits 2. |
| `PIKMIN_SCENARIO_STAGE` | Stage `.ini` to boot, as written in `stages.ini`. Default `stages/stage1.ini` (Forest of Hope on disc). The stand-in tree uses `stages/sandbox.ini`, `stages/sandbox_objects.ini` or `stages/sandbox_all_objects.ini`. On the stand-in tree without it the run names `stage1.ini`, which has no files there, and hangs until the watchdog fails it (exit 1). An unknown stage name fails with exit 1 and `no stage with file ...`. |
| `PIKMIN_SCENARIO_TRACE` | Number of ticks, from tick 0, for which the runner logs captain state, movie and overlay flags, day and time (`[scenario-trace]` lines). |
| `PIKMIN_SCENARIO_WATCHDOG_SECONDS` | Wall-clock limit in seconds, default 540. A value that is not a positive integer is ignored. |
| `PIKMIN_SCENARIO_STATE_STATS` | Any value. With the verdict, prints per state id of the captain and of the Pikmin the longest uninterrupted stay in ticks and the entry count (`[scenario-states]` lines). A state stuck on an animation key the stand-ins lack shows as a long stay. |
| `PIKMIN_LOG_FILE_OPENS` | File path. Appends `<game path>\t<OK\|FAILED>` per `DVDOpen`. Works in any build, used to make the boot manifest. The file is appended to, so use a fresh path per run. |
| `PIKMIN_SCENARIO_WINDOW` | Window size `WxH`, default `64x36`. The scene renders at the same size (render scale 1, no 160x120 floor). Anything but two positive integers up to 8192 exits 2. See [Window size](#window-size). |

A scenario run forces its own conditions:

- Dummy SDL audio (`SDL_AUDIODRIVER=dummy`).
- Netplay deterministic mode, forced in code (`pc_netplay_det_scenario_enable`):
  deterministic fixed-step mode and unthrottled, so each loop runs one tick with
  no vsync wait; the session seed is 0 and an ambient `PIKMIN_NETPLAY_*`
  variable cannot change it; the sim RNG is reseeded from that seed, the day and
  the stage at each new day; the floating-point environment is pinned every tick.
  Two runs on one machine are identical, nothing more is claimed.
- Settings that are neither read nor written: windowed 64x36 (`PIKMIN_SCENARIO_WINDOW`), vsync off, the
  frame rate set from `--scenario-tick-rate`, Infinite Day set by the scenario.
- A fresh `NECTAR_SAVE_DIR` under the system temp folder, named
  `nectar-scenario-<name>-<rate>-<pid>`. An ambient `NECTAR_SAVE_DIR` is not
  used. The folder is removed on every normal exit, on a watchdog failure and
  when a stage cannot be found, but a process killed by a signal (an abort, a
  crash, CTest's timeout) leaves it behind.
- A cleared list of environment variables that change the sim or the input
  (`kClearedEnv` in `pc_scenario.cpp`: `PIKMIN_INPUT_RECORD`, `PIKMIN_UNLOCK_ALL`,
  `PIKMIN_COOP`, `PIKMIN_COOP_CAPTAINS`, `PIKMIN_REPLAY_TEST`, `PIKMIN_NO_H4M`,
  `PIKMIN_NETPLAY_EXIT_AFTER_TICKS` and three `PIKMIN_NETPLAY_TEST_*` variables).

### Window size

Every tick draws a frame, and the world sim runs inside the draw step, so a
runner without a GPU (CI's macOS, Linux and Windows runners use a software
renderer) pays per pixel on every tick. No scenario reads the picture. The
default window is 64x36 and the scene renders at that size: the player's
render scale (2/3) and its 160x120 floor do not apply (`pc_gfx_set_min_render_size`).

Measured on an M3 Max with Apple's software renderer (the renderer macOS CI
gets), forced by a local, uncommitted `SDL_GL_ACCELERATED_VISUAL` 0 in
`pc_window.cpp`, `boot_smoke` at 30 Hz on the stand-in tree, 900 ticks plus boot:

| Window | Scene rendered at | Wall time |
|--------|-------------------|-----------|
| 640x360 (old default, render scale 2/3) | 427x240 | 34.5 s |
| 160x90 with the 160x120 floor | 160x120 | 9.8 s |
| 64x36 with the 160x120 floor | 160x120 | 7.9 s |
| 160x90 | 160x90 | 9.2 s |
| 128x72 | 128x72 | 7.6 s |
| 64x36 (default) | 64x36 | 4.1 to 4.9 s |

The same runs on the GPU take under a second at any of these sizes, so local
GPU timing cannot show this.

The sim does not read the window size: every scenario's `--state-hash-log`
at 160x90, 128x72 and 64x36 is byte-identical to the 640x360 run of the same
scenario and rate, on the stand-in and the disc tree, and `throw_release`
measures the same K. Set `PIKMIN_SCENARIO_WINDOW=640x360` (or larger) to
compare, or with `--scenario-visible` to watch a run at a readable size.

### Exit codes

| Code | Meaning |
|------|---------|
| `0` | Pass. |
| `1` | A check failed (one `[scenario] FAIL ...: expected ..., observed ...` line on stderr), the boot never reached the stage, the stage name is not in `stages.ini`, the watchdog fired, or the process ended before a verdict. |
| `2` | Unknown scenario, bad arguments or a rejected flag, or a data directory that is set but cannot be entered. Before any window exists. |
| `4` | Window or GL context creation failed (from the code; not reproduced, because this Mac has a GL context). |
| `77` | Skip: `PIKMIN_SCENARIO_DATA_DIR` unset or empty, no `assets/dataDir/parms/gamePrms.bin`, or the scenario does not support the rate (no scenario refuses a rate today). |

Exit 77 only reaches CTest as a skip for the `_disc` entries (`SKIP_RETURN_CODE
77`); a stand-in entry that exits 77 fails.

Three rules keep an abnormal exit from reading as a pass:

- `--scenario` rejects boot-path-changing flags with exit 2 before any window
  exists: `--coop-direct`, `--coop`, `--vs-map`, `--mission-map`, `--campaign`,
  `--netplay-host`, `--netplay-join`, `--netplay-host-ice`, `--netplay-join-ice`,
  `--netplay-exit-after-ticks` and `--input-replay` (plus `PIKMIN_INPUT_REPLAY`).
  `--input-record` stays allowed and records the scripted pads. The list is
  `kRejectedFlags` in `pc_scenario.cpp`.
- A wall-clock watchdog fails the run with exit 1 and prints
  `[scenario] FAIL <name>: wall-clock watchdog after N s` (default 540 s,
  override with `PIKMIN_SCENARIO_WATCHDOG_SECONDS`). It sits under the CTest
  timeout of 600 s. On this Mac a stand-in scenario takes 1 to 2 s, so the
  default is a bound for slow runners (the source comment says the slowest takes
  about 85 s on software GL; that was not measured here).
- Any `exit()` before a verdict is a failure. The exit handler is armed once
  argument and data checks pass. `System::halt` and `PPCHalt` call `exit(0)`,
  which would otherwise pass. A GL context failure records exit 4 first, so the
  handler reports 4.

An ERROR or abort ends the process by signal, which CTest also reports as a
failure.

## CTest entries

Registered in `games/pikmin1/CMakeLists.txt` under `if(PIKMIN_BUILD_SCENARIOS)`.
`ctest -N` lists 13 stand-in entries (`-R ^scenario_ -LE game-assets`) and 6 disc
entries (`-L game-assets`).

All names are `scenario_<name>_<rate>`, at the rates in [Tick rates](#tick-rates).

### Tick rates

A scenario is registered at 60 as well as 30 only when the 60 Hz run executes
code or timing the 30 Hz run does not. The disc entries keep `boot_smoke`,
`throw_release` and `day30_clock` at both rates (issue #61).

| Scenario | Rates | Why |
|----------|-------|-----|
| `boot_smoke` | 30, 60 | The rate is boot configuration (`fpsMode`, the tick length); each rate must boot and run. |
| `throw_release` | 30, 60 | K and the release timings are tick arithmetic and differ per rate (15 and 18). |
| `day30_clock` | 30, 60 | The clock moves by one frame time per tick, so the hold and the sunset arithmetic differ per rate. |
| `captain_death` | 30, 60 | Bounds are in seconds and tick counts differ (`Dead` at tick 128 and 247, the results screen about 11 s later at both); the clock and the movie advance per tick. |
| `sprout_landing` | 30, 60 | `BuryInit` snaps the seed to the ground only above 30 Hz (`pc_seed_landing_snaps_to_ground`); 60 Hz is the only run of that path. |
| `onion_skip` | 30, 60 | The movie advances half a frame per tick at 60 Hz (330 ticks against 660), so the skip frames and the boot motion's key events land on different ticks. |
| `container_menu` | 30, 60 | Same object stage as `onion_skip`; both rates are registered, not yet measured for a rate-specific path. |
| `objects_smoke` | 30 | Loads the every-object stage and counts the enemies born; neither depends on the tick rate. |

| Entry | Data | Label | Notes |
|-------|------|-------|-------|
| `scenario_standin_generate` | writes `<build>/scenario-data` | | `FIXTURES_SETUP scenario_standin_tree`, 120 s timeout. Passes `--manifest tools/scenarios/boot-manifest.txt`. |
| `scenario_<name>_<rate>` | stand-in tree | none | For `boot_smoke`, `throw_release`, `day30_clock`, `captain_death`, `sprout_landing`, `onion_skip` and `container_menu` at 30 and 60, and `objects_smoke` at 30 (see Tick rates). `FIXTURES_REQUIRED scenario_standin_tree`, 600 s timeout, `PIKMIN_SCENARIO_STAGE` as in the stage table below. No skip code: a missing or broken tree fails. |
| `scenario_<name>_<rate>_disc` | a real install | `game-assets` | For `boot_smoke`, `throw_release` and `day30_clock` only, at 30 and 60 (6 entries). Runs in `<build>/scenario-scratch`, 600 s timeout, `SKIP_RETURN_CODE 77`. Set `PIKMIN_SCENARIO_DATA_DIR` to run it; otherwise CTest reports a skip. |

Which stage each stand-in entry boots:

| Scenarios | `PIKMIN_SCENARIO_STAGE` |
|-----------|-------------------------|
| `boot_smoke`, `throw_release`, `day30_clock`, `captain_death` | `stages/sandbox.ini` |
| `sprout_landing`, `onion_skip`, `container_menu` | `stages/sandbox_objects.ini`, the opt-in second stage with a cliff, an Onion, sprouts, a pellet and an enemy. See `standin-objects.md` for what the object stand-ins are, what was read from the disc and what remains, and `standin-cinema.md` for `onion_skip`. |
| `objects_smoke` | `stages/sandbox_all_objects.ini`, one of each plant, bridge, gate, boss and eight enemies. |

`sprout_landing`, `onion_skip`, `container_menu` and `objects_smoke` have no `_disc` entry: their
stages do not exist on a real install.

The stand-in entries run through `run_standin_scenario.cmake`. It starts
`nectar`, keeps the scenario's exit code, and additionally fails the test on any
`FAILED to open assets/...` line whose path is not in
`standin-open-allowlist.txt` (one path relative to `assets/` per line, a trailing
`/` covers a folder, `#` starts a comment). Every allowlisted file is one the
game tolerates missing and no scenario depends on, with the reason beside it.
Under CTest it also passes `--state-hash-log` and writes
`<build>/scenario-logs/scenario_<name>_<rate>.hash.log` and `.log` (CI uploads
that folder when a test fails).

### CI

`.github/workflows/ci.yml` configures and builds `build/pikmin1` (Release,
Ninja, `PIKMIN_BUILD_LAUNCHER=ON`, `PIKMIN_BUILD_TESTS=ON`,
`PIKMIN_NATIVE_JAUDIO=OFF`, `PIKMIN_BUILD_SCENARIOS=ON`; the whole project, not
only the two targets), then runs
`ctest -R ^scenario_ -LE game-assets --timeout 600 -j 3`, so disc entries never
run. The unit tests run in a later step with `-E ^scenario_`. The environment is
`SDL_AUDIODRIVER=dummy`, `LIBGL_ALWAYS_SOFTWARE=1`, `GALLIUM_DRIVER=llvmpipe`.

- **Linux**: wrapped in `xvfb-run -a`, with Mesa packages installed.
- **macOS**: runs directly.
- **Windows (MinGW-w64)**: copies `opengl32.dll` and `libgallium_wgl.dll` from
  `/mingw64/bin` into `build/pikmin1/bin/` first. Without them Windows returns the
  GDI Generic GL 1.1 renderer.
- **`SCENARIO_SKIP_OS`** (an `env` entry in `ci.yml`, empty today): a list of
  `runner.os` values whose GL probe failed. Listed OSes skip the scenario test
  step only; their build and unit tests still run. Add an OS only with a linked
  tracking issue.
- The native JAudio scenario run in `standin-audio.md` is a proposal and is not
  in CI; the `jaudio-build` job only compiles that configuration.

### Cross-platform floats

A scenario build (`PIKMIN_BUILD_SCENARIOS=ON`) also turns on
`PIKMIN_SCENARIO_FLOAT_DET` (default ON), so a state-hash log, K, or a stand-in
file from Linux or Windows means the same as one from macOS:

- `-ffp-contract=off` on the game, the legacy library, the generator,
  `nectar_assets` and `nectar_mapcore`. arm64 clang otherwise fuses
  multiply-adds; x86-64 GCC without `-march` has no FMA to fuse.
- The netplay build's software libm (`pc_port/netplay/pc_netplay_libm.c`)
  replaces the platform's `sinf`, `atan2f`, `powf` and the rest in `nectar`
  and the generator. On Apple, `pc_port/scenarios/pc_scenario_sincos_apple.c`
  also replaces `__sincosf_stret` and `__sincos_stret`, which clang calls when it
  fuses a sine and a cosine of the same value.

Release builds never set `PIKMIN_BUILD_SCENARIOS`, so their flags and libm are
unchanged. A `PIKMIN_NETPLAY_BUILD` already has the first two and is left as is.
`-DPIKMIN_SCENARIO_FLOAT_DET=OFF` gives the platform's own floats again.

Measured 2026-10-28: macOS arm64 (Apple clang, Release, Unix Makefiles) against
Linux x86-64 (Ubuntu 24.04 in Docker `linux/amd64`, GCC 13.3, glibc 2.39,
Release, Ninja, the CI Linux packages, `xvfb-run`, llvmpipe). `--state-hash-log`
of `throw_release`, `boot_smoke` and `day30_clock` at 30 and 60 Hz on the
stand-in tree:

| Build | Identical across the two | First difference |
|-------|--------------------------|------------------|
| Default (option OFF) | none of 6 | tick 30 (30 Hz) or 23 (60 Hz), captain hash, in every scenario: arm64 FMA |
| `-ffp-contract=off` only | 4 of 6 (`boot_smoke`, `day30_clock`) | `throw_release` at tick 53 (30 Hz) or 52 (60 Hz), Pikmin hash: libm (`atan2f`) |
| Option ON (both) | 6 of 6 | none |

On Linux the default and the `-ffp-contract=off` logs are the same, as
expected without FMA. With the option ON the generator writes byte-identical
trees on both (by default `sandbox_objects.mod` and `sandbox_all_objects.mod`
differed), and the hash logs of all 18 stand-in scenario entries, the object
stages included, are byte-identical on the two; all 19 stand-in entries pass on
both. K is 15 at 30 Hz and 18 at 60 Hz in every build, on the stand-in tree
and on a real install (macOS only).

Not measured: Windows (MinGW-w64). Its libm uses x87 transcendentals, which is
what the software libm exists to replace, so the same result is expected but was
not observed. Remaining assumptions: compile-time folding of a math call on
constant arguments is up to each compiler, and the sqrt, `frexpf` and `ldexpf`
that stay with the platform are exact by IEEE 754.

## The scenarios

| Scenario | Start | What it asserts |
|----------|-------|-----------------|
| `boot_smoke` | Day 1 | Boots into the stage and passes after 900 ticks. Fails if the stage-ready hook does not fire within 20,000 boot ticks. |
| `throw_release` | Day 1 | The squad is born only after the stage has been ready for 45 ticks (a hold of neutral input; 0 on the disc, whose intro already takes 45), then the captain walks away for 4 s with three blue Pikmin following. Three cases follow, each starting when the captain is in Walk with a Normal Pikmin in the plate within 80 units: a calibration that holds A until the grab completes and measures K (ticks from the press to the grab), a case with A down for one tick only (the early release), and a case that releases 2 ticks after that Pikmin's own grab completes. Each case checks that the Pikmin is Hanged before the captain enters Throw (except the early release), the captain enters Throw, the Pikmin leaves the hand (moves at least 5 units), flies and lands (Normal), and the captain returns to Walk. The grab, Throw and flight steps are bounded at 5 s, landing and the Settle wait at 10 s. Also asserts the throw key is bound to A and, before the calibration, that the ThrowWait animation and the AnimMgr tables are sane. |
| `day30_clock` | Day 2 | Infinite Day holds the world clock on Days 2 to 29 and not on Day 30 (regression test for upstream #100). Day 2 with the option off: the clock advances. Option on: it stays put. Each phase is 30 open ticks. Then Day 30 with the option on, started 120 clock ticks before the end hour (the start is computed from `mRealSecsPerGameHour` and the frame time, so it is the same tick count on every tree and rate): the clock advances to sunset and day end begins at or after the end hour. A build without the Day 30 exemption fails with `clock held on day 30` 30 ticks into Day 30. Passes at tick 277 at 30 Hz and 298 at 60 Hz (was 1,146 and 1,584). |
| `captain_death` | Day 1 | The captain takes a Flick hit for 1000 damage at tick 10 (T06 in `coverage/terminals.md`). The real chain runs with no further input except A presses once the results window is up: `Dead` (29) within 8 s with `GameStat::orimaDead`, `GAMEEND_NaviDown` within 4 s, `MessageModeState`, the day advancing exactly once (`DayOverModeState` phase two opens the results window), then the soft reset toward `ONEPLAYER_MapSelect` at least 3 s after the results opened (the diary page, `account2.blo`, the save step) and within 60 s. `Dead` entered twice, a second day advance or a reset toward `GameExit` fail it. Needs the stand-in `demo36` (see `standin-cinema.md`): without it the end-of-day movie has zero length, phases three and four run the next tick and the day leaves for `GameExit` with the results unread. At 30 Hz: Dead t=128, NaviDown t=167, day advance t=340, reset t=676; at 60 Hz: 247, 325, 662, 1306. |
| `container_menu` | Day 1, object stage | The captain walks onto the Onion's `cont` spot and presses A; the container menu (`DrawContainer`, stand-in `con01.blo` with its `p264` window) opens, passes Wait, Start and Operation, stays in Operation for 4 s with one stick push, and closes, once with B and once with A. The captain returns to Walk and the Onion count is unchanged. |
| `onion_skip` | Day 1, object stage | The blue Onion discovery movie (upstream #205), started with `DEMOFLAG_DiscoverBlueOnyon`. Four cases: watched to the end, START before Boot, between Boot and the first seed, and between the seed and EmitCone. Each asserts that the movie plays and ends, the Onion boots and reaches Wait, and exactly one seed comes out. The between case is the regression test for #205 (0 seeds without the fix). Upstream PR #205 is still open; the fix is in this tree through the kyle-develop merge. See `standin-cinema.md`. |
| `sprout_landing` | Day 1, object stage | Six seeds launched at the cliff the way `GoalAI::BootEmit` launches them. Each must plant (Wait) within a bound, not on a steep face, not below the ground and not floating more than 30 units above it. Height and speed at the start of Bury are printed, not asserted (upstream #208, merged upstream 2026-10-28, changes them at 30 Hz). See `standin-objects.md`. |
| `objects_smoke` | Day 1, every-object stage | Boots `stages/sandbox_all_objects.ini`, checks the eight enemies were born and passes after 300 ticks. A missing or malformed model, platform, table or parameter file aborts the run instead. |

`throw_release` measures K at run time and never hard-codes it. On this tree both
the stand-in sandbox and a real install give K = 15 at 30 Hz and 18 at 60 Hz
(measured 2026-10-28), but the stand-in gate is fitted to land there
(`standin-k-investigation.md`), so a change to the squad AI, collision or the sim
RNG can move the two apart.

## Writing a scenario

A scenario is a class in `pc_port/scenarios/scenario_<name>.cpp` that derives
from `PcScenario` (`pc_scenario.h`):

| Member | Purpose |
|--------|---------|
| `name()` | The `--scenario` argument. |
| `startDay()` | Day the stage starts on. Default 1. Set before the stage is built, because `GameCoreSection`'s constructor consumes it. |
| `supportsRate(int)` | Return false to skip a rate with exit 77. |
| `onStageReady()` | Runs once, after the intro gate, before tick 0. Spawn objects here. |
| `providePads(tick, pad0)` | Fills pad 0 for this tick, before the sim. Pads 1 to 3 are neutral. |
| `onTick(tick)` | Runs after each tick's sim. Call `scenarioPass()`, or `scenarioFail()` / `scenarioFailf()` with expected and observed text. |

The scenario reads game objects directly (`naviMgr`, `pikiMgr`, `gameflow`).
Never call `exit()`. Report only through the verdict functions.

How a run flows:

1. `pc_scenario_parse_argv` (called from `pc_main.cpp`) checks arguments and the
   data directory, and sets up the environment above.
2. The title and file-select sections are skipped (`ninLogo.cpp`), and
   `GameSetup` goes straight to the one-player stage (`gameSetup.cpp`).
3. `pc_scenario_pre_stage` (`game.cpp`) does what the no-save new-game path would
   do, selects the stage and sets the start day and hour.
4. `pc_scenario_on_stage_ready` (`newPikiGame.cpp`, after `finalSetup`) marks the
   stage as ready.
5. `pc_scenario_on_tick_end` (`System::run`) counts ticks. The **intro gate**
   comes first: a new game opens with the landing cutscene and ship text, so the
   input provider pulses START while a movie plays and A while the UI overlay is
   up, and tick 0 is handed to the scenario only once both are gone and the
   captain is in Walk. If that takes more than 90 s of ticks, the run fails.
6. Input reaches the game through `pc_input_log_set_provider`.

To add a scenario: write the class, add a `pc_scenario_make_<name>()` factory
declaration in `pc_scenario_list.h`, return it from `pc_scenario_all()` in
`pc_scenario_list.cpp`, add the source to the `PIKMIN_BUILD_SCENARIOS` block in
`CMakeLists.txt`, and add the name to a `foreach(scenario ...)` list in the
scenario test block at the end of that file: the stand-in list for the stage it
runs on (`sandbox.ini`, `sandbox_objects.ini` or `sandbox_all_objects.ini`) and,
only if it works on a real install, the disc list. Then run it once on the
stand-in tree.
Expect a missing-file abort or null dereference if it needs something the tree
lacks; see [Adding a stand-in file](#adding-a-stand-in-file).

## Stand-in assets

`nectar_standin_generator <out> [--manifest <boot-manifest.txt>]` writes
`<out>/assets/dataDir/...`, the layout the game opens (every file open resolves
to `assets/` plus the path the game passes). It builds the tree in memory twice
and fails if the two hashes differ (run-to-run identity on one machine only), and
treats every writer error as fatal. With `--manifest`, every path of the disc
boot manifest must be in the tree or on the generator's built-in allowlist
(`kManifestAllowlist` in `standin_generator.cpp`, each entry with a reason), or it
exits 1. It omits `assets/.pikmin-assets`, which only the launcher reads.

The tree holds: a sandbox stage (a flat floor with one cliff, `stages/sandbox.ini`
and its generator files); `stages/stages.ini`; the parameter files; the 18
`.dir`/`.arc` archive pairs; and the models, animations, textures, screens and
particles from the writers below. The sandbox has no `id` line, so it is
`STAGE_TESTMAP`: not a tutorial, so the clock runs.

The writers live in `libs/nectar_assets`: `mod_writer.h` and `standin_models.h`
(`.mod`), `anim.h` and `standin_anims.h` (`.dca`, `.dck`, `animMgr.bin`),
`texture.h`, `bundle.h` and `standin_textures.h` (`.bti`, `.txe`, bundles),
`blo.h`, `pcr.h` and `standin_ui.h` (screens, particles), `cinema.h` (`.cin`
and `.dsk` cutscenes, see `standin-cinema.md`), and `parms.h`, `archive.h`,
`stages_ini.h` and `gen.h`.

Each writer is a pure function (a plain input struct, bytes out, an error
string, no file I/O) with a write-then-read unit test registered in CTest, for
example `nectar_mod_writer_test`.

### Fidelity rule

A stand-in matches the real data **only where a scenario depends on it**: joint
indices, animation lengths, key-event frames, collision sizes and parameter
values. Take values from the decompiled source or code defaults first. A number
only the disc has is recorded in a comment saying where it came from (the
`throw_release` movement and grab parameters are the main case, see `parmFiles`
in `standin_generator.cpp`). No disc geometry, textures, audio or text is used.
Models are flat colours on an invented rig, animations are identity poses.

### The allowlist and the boot manifest

`boot-manifest.txt` lists every file the game opens on the scenario boot path on
a real install: path, `OK|FAILED`, and the scenarios that opened it. It is a
cross-check, not an input to the game. The generator checks the tree against it,
and some writer tests take it as an argument.

Regenerate it after a change that alters the boot path. On disc data, run each
scenario at least once with `PIKMIN_LOG_FILE_OPENS` set to a fresh file, then:

```sh
python3 tools/scenarios/make_boot_manifest.py \
    --version VERSION_GPIE01_01 --flags "RelWithDebInfo, PIKMIN_BUILD_SCENARIOS=ON, PIKMIN_NATIVE_JAUDIO=OFF" \
    boot_smoke=a.log throw_release=b.log day30_clock=c.log > tools/scenarios/boot-manifest.txt
```

(Run from `games/pikmin1`. Several logs per scenario go comma-separated.) The
script stores paths only, no sizes or contents.

### Adding a stand-in file

1. Run the scenario on the stand-in tree. A missing required file aborts, so
   gaps appear one at a time.
2. If the game needs the file, add it to the generator with the matching writer
   (add a writer and a test in `libs/nectar_assets` if the format has none).
3. If the game tolerates its absence and no scenario depends on it, add its path
   to `standin-open-allowlist.txt` with the reason, and to `kManifestAllowlist`
   if it is in the boot manifest.
4. If a new scenario depends on a value, apply the fidelity rule and cite where
   the number came from.

## GL probe

`gl_probe.c` checks that a runner can host the scenario: it creates a hidden SDL
window with the context profile the game requests (4.1 core on macOS, 3.3
compatibility elsewhere), prints `GL_VERSION`, `GL_RENDERER` and the GLSL
version, compiles and links a `#version 330 core` program, reads back a cleared
framebuffer and swaps 60 times. It prints `GL_PROBE_OK` and exits 0 only if every
step works; on Windows it also rejects the GDI Generic renderer. It exits
without GL teardown because llvmpipe's context destruction hangs on Windows
runners. The CI `gl-probe` job builds and runs it on each OS.

Build it with `cc gl_probe.c $(sdl2-config --cflags --libs) -o gl_probe`.

## Limitations

- Audio is off in the default scenario build (`PIKMIN_NATIVE_JAUDIO=OFF`,
  `SDL_AUDIODRIVER=dummy`). A `PIKMIN_NATIVE_JAUDIO=ON` build boots the
  original engine on stand-in sequences that play no notes, so it tests that
  the engine starts and keeps running, not what it sounds like
  ([standin-audio.md](standin-audio.md)).
- Cutscenes are stand-ins with no actors (`standin-cinema.md`). The sandbox
  plays no landing; it plays the stand-in post-extinction movie (demo60), which
  the intro gate skips on its first tick. `onion_skip` plays and skips the
  stand-in blue Onion discovery (demo06) on the object stage. The sandbox has no
  enemies, bosses, plants, gates or bridges.
- Stand-in and disc runs can differ in timing. `throw_release` measures K, the
  press-to-grab delay, per run because the stand-in value need not equal the
  disc's. Today the two agree (15 at 30 Hz, 18 at 60 Hz), by a fitted sandbox
  gate, not by construction.
- A scenario needs a GL context, even hidden. Without one the run exits 4. On
  Windows, Mesa's `opengl32.dll` and `libgallium_wgl.dll` must sit beside the
  executable.

## Mouse pointer

A hidden window (`--scenario`, the default) must not move the real mouse
pointer. `pc_window.cpp` skips `SDL_WarpMouseInWindow` when the window carries
`SDL_WINDOW_HIDDEN`, in `warpMouseToCentre()` and in the startup centre warp
(commit `3d1e66c0`). The cause had been inferred from the code. It was observed
on macOS (Apple M3 Max, 2560x1080 main display) on 2026-10-28 with a read-only
helper (`python3 -I` with pyobjc: `CGEventGetLocation` and
`NSEvent.mouseLocation`, in CoreGraphics coordinates, origin top left). The
helper never posts events. Each run is `boot_smoke` at 30 Hz on the stand-in tree.

| Case | Pointer before | Pointer after |
|------|----------------|---------------|
| Current binary, hidden | (-417.8, 964.0) | (-417.8, 964.0), unchanged |
| Both guards removed, hidden (one run) | (-417.8, 964.0) | (1280.0, 555.0), moved |
| Current binary, `--scenario-visible`, killed at the warp call | (1280.0, 555.0) | (1280.0, 555.0), no information (see below) |

Both guards removed moved the pointer to the centre of the 640x360 window (x
1280 is the middle of the 2560-wide display; y 555 is the window's content
centre, below the title bar; this reading of y is an inference). One run, so the
pointer moving is one observation, but it moved from a far position to the window
centre and the same binary with the guards did not move it. That confirms the
startup warp as the cause on this Mac.

For the visible case the pointer already sat at the warp target, so a position
check could not tell a warp from no warp. The run was repeated under `lldb` with
a breakpoint on `SDL_WarpMouseInWindow`, killed at the first hit so the warp never
ran: the visible run reaches the call (it warps, as before) and the hidden run
never reaches it (the whole scenario passed with the breakpoint armed). Pointer
positions were unchanged by those two runs.

Not covered: Linux and Windows (SDL's warp semantics differ; the guard is
platform-neutral C++ but was not compiled there), and the other
`SDL_SetRelativeMouseMode` call sites, which this change does not touch.

## Verification of this note

The commands in this note were run on 2026-10-28 on macOS arm64 (RelWithDebInfo,
Unix Makefiles, `PIKMIN_NATIVE_JAUDIO=OFF`):

- Configure and build of `pikmin_pc` and `nectar_standin_generator`: worked.
- `ctest -R ^scenario_ -LE game-assets`: 13 of 13 passed (generator plus 12
  scenario runs). `ctest -N -L game-assets` lists the 6 disc entries.
- The generator with `--manifest`: 314 files, 294 manifest paths in the tree,
  17 allowlisted, 0 missing. A manifest path that cannot be read exits 1; no
  arguments exits 2.
- The direct `nectar --scenario boot_smoke` command on the stand-in tree and
  on a disc tree (a scratch folder whose `assets` is a symlink to an install):
  both exit 0. The three disc scenarios pass at 30 Hz and `throw_release` at 60.
- `make_boot_manifest.py` with fresh `PIKMIN_LOG_FILE_OPENS` logs from the three
  disc runs reproduces `boot-manifest.txt` byte for byte.
- Exit codes 0, 1, 2 and 77, each rejected flag, the data-directory cases, the
  watchdog and `PIKMIN_SCENARIO_TRACE` were run. Exit 4 was not (no GL failure
  could be produced).
- `nectar_cinema_writer_test`, `nectar_blo_writer_test`, `nectar_pcr_writer_test`,
  `nectar_standin_objects_test` and `nectar_audio_writer_test` pass. Three rows of
  `standin-texture-experiments.md` re-run on the current tree (delete
  `consFont.bti`: exit 139; delete `effects/pcr/effects.bun`: exit 134 with
  `Cannot open file.[effects/pcr/ore_rec.pcr]`; delete `intro/nintendo.bti`: exit
  0) match.
- Not run: the native JAudio configuration, the Linux and Windows builds, CI, and
  `-G Ninja`.
- `gl_probe.c`, built with the command above, printed `GL_PROBE_OK` (4.1 Metal,
  Apple M3 Max).
