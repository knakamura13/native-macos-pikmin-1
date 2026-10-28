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
JAudio off (the legacy audio facade needs no sound data):

```sh
cmake -S games/pikmin1 -B build/pikmin1-scenarios -G Ninja \
    -DPIKMIN_NATIVE_JAUDIO=OFF -DPIKMIN_BUILD_SCENARIOS=ON
cmake --build build/pikmin1-scenarios --target pikmin_pc nectar_standin_generator
```

`pikmin_pc` produces `build/pikmin1-scenarios/bin/nectar`.
`nectar_standin_generator` is a separate executable, so release packaging never
ships it. `PIKMIN_BUILD_SCENARIOS` also calls `enable_testing()` and defines
`PIKI_SCENARIOS=1`; every scenario source is wrapped in `#if PIKI_SCENARIOS`.

## Run

Through CTest (builds the stand-in tree first):

```sh
ctest --test-dir build/pikmin1-scenarios --output-on-failure -R ^scenario_ -LE game-assets
```

Directly, from the directory that holds `assets/`:

```sh
PIKMIN_SCENARIO_DATA_DIR=build/pikmin1-scenarios/scenario-data \
PIKMIN_SCENARIO_STAGE=stages/sandbox.ini \
    build/pikmin1-scenarios/bin/nectar --scenario boot_smoke --scenario-tick-rate 30
```

| Argument | Meaning |
|----------|---------|
| `--scenario <name>` | Scenario to run: `boot_smoke`, `throw_release`, `day30_clock` or `sprout_landing` (needs the object stage, below). |
| `--scenario-tick-rate 30\|60` | Ticks per second. Default `30`. Any other value exits 2. |
| `--scenario-visible` | Show the window. Default is hidden. |

| Environment variable | Meaning |
|----------------------|---------|
| `PIKMIN_SCENARIO_DATA_DIR` | Folder that holds `assets/`. The process changes into it. Unset, the run exits 77. |
| `PIKMIN_SCENARIO_STAGE` | Stage `.ini` to boot, as written in `stages.ini`. Default `stages/stage1.ini` (Forest of Hope on disc). The stand-in tree uses `stages/sandbox.ini`. |
| `PIKMIN_SCENARIO_TRACE` | Number of ticks after tick 0 for which the runner logs captain state, movie and overlay flags. |
| `PIKMIN_LOG_FILE_OPENS` | File path. Appends `<game path>\t<OK\|FAILED>` per `DVDOpen`. Works in any build, used to make the boot manifest. |

A scenario run forces its own conditions: dummy SDL audio, the netplay
deterministic mode, settings that are neither read nor written (640x360 window,
no vsync), a fresh `NECTAR_SAVE_DIR` in the temp folder removed at exit, and a
cleared list of environment variables that change the sim or the input
(`kClearedEnv` in `pc_scenario.cpp`).

### Exit codes

| Code | Meaning |
|------|---------|
| `0` | Pass. |
| `1` | A check failed (one `[scenario] FAIL ...: expected ..., observed ...` line on stderr), the boot never reached the stage, or the process ended before a verdict. |
| `2` | Unknown scenario or bad arguments, before any window exists. |
| `4` | Window or GL context creation failed. |
| `77` | Skip: no data directory, no `assets/dataDir/parms/gamePrms.bin`, or the scenario does not support the rate. |

Three rules keep an abnormal exit from reading as a pass:

- `--scenario` rejects boot-path-changing flags with exit 2 before any window
  exists: `--coop-direct`, `--coop`, `--vs-map`, `--mission-map`, `--campaign`,
  `--netplay-host`, `--netplay-join`, `--netplay-host-ice`, `--netplay-join-ice`,
  `--netplay-exit-after-ticks` and `--input-replay` (plus `PIKMIN_INPUT_REPLAY`).
  `--input-record` stays allowed and records the scripted pads. The list is
  `kRejectedFlags` in `pc_scenario.cpp`.
- A wall-clock watchdog fails the run with exit 1 (default 540 s, override with
  `PIKMIN_SCENARIO_WATCHDOG_SECONDS`). It sits under the CTest timeout of 600 s.
- Any `exit()` before a verdict is a failure. The exit handler is armed once
  argument and data checks pass. `System::halt` and `PPCHalt` call `exit(0)`,
  which would otherwise pass. A GL context failure records exit 4 first, so the
  handler reports 4.

An ERROR or abort ends the process by signal, which CTest also reports as a
failure.

## CTest entries

All names are `scenario_<name>_<rate>` for each scenario at 30 and 60.

| Entry | Data | Label | Notes |
|-------|------|-------|-------|
| `scenario_standin_generate` | writes `<build>/scenario-data` | | `FIXTURES_SETUP scenario_standin_tree`, 120 s timeout. Passes `--manifest boot-manifest.txt`. |
| `scenario_<name>_<rate>` | stand-in tree | none | `FIXTURES_REQUIRED scenario_standin_tree`, runs with `PIKMIN_SCENARIO_STAGE=stages/sandbox.ini`, 600 s timeout. No skip code: a missing or broken tree fails. |
| `scenario_<name>_<rate>_disc` | a real install | `game-assets` | Runs in `<build>/scenario-scratch`, 600 s timeout, `SKIP_RETURN_CODE 77`. Set `PIKMIN_SCENARIO_DATA_DIR` to run it; otherwise CTest reports a skip. |

`scenario_sprout_landing_<rate>` (30 and 60) is stand-in only: it runs with
`PIKMIN_SCENARIO_STAGE=stages/sandbox_objects.ini`, the opt-in second stage
with a cliff, an Onion, sprouts, a pellet and an enemy. See
`standin-objects.md` for what the object stand-ins are, what was read from the
disc and what remains.

The stand-in entries run through `run_standin_scenario.cmake`. It starts
`nectar`, keeps the scenario's exit code, and additionally fails the test on any
`FAILED to open assets/...` line whose path is not in
`standin-open-allowlist.txt` (one path relative to `assets/` per line, a trailing
`/` covers a folder, `#` starts a comment). Every allowlisted file is one the
game tolerates missing and no scenario depends on, with the reason beside it.

### CI

`.github/workflows/ci.yml` builds `build/pikmin1-scenarios` with the options
above, builds the two targets, then runs
`ctest -R ^scenario_ -LE game-assets --timeout 600`, so disc entries never run.
The environment is `SDL_AUDIODRIVER=dummy`, `LIBGL_ALWAYS_SOFTWARE=1`,
`GALLIUM_DRIVER=llvmpipe`.

- **Linux**: wrapped in `xvfb-run -a`, with Mesa packages installed.
- **macOS**: runs directly.
- **Windows (MinGW-w64)**: copies `opengl32.dll` and `libgallium_wgl.dll` from
  `/mingw64/bin` into `bin/` first. Without them Windows returns the GDI Generic
  GL 1.1 renderer.
- **`SCENARIO_SKIP_OS`** (an `env` entry in `ci.yml`, empty today): a list of
  `runner.os` values whose GL probe failed. Listed OSes skip the scenario build
  and test steps; their normal build and unit tests still run. Add an OS only
  with a linked tracking issue.

## The scenarios

| Scenario | Start | What it asserts |
|----------|-------|-----------------|
| `boot_smoke` | Day 1 | Boots into the stage and passes after 900 ticks. Fails if the stage-ready hook does not fire within 20,000 boot ticks. |
| `throw_release` | Day 1 | One captain, three blue Pikmin in grab range, three cases: a calibration that holds A until the grab completes and measures K (ticks from the press to the grab), a release one tick after the press, a release K + 2 ticks after it. Each case checks that the Pikmin is Hanged before the captain enters Throw (except the early release), the captain enters Throw, the Pikmin leaves the hand, flies and lands (Normal), and the captain returns to Walk, all within 5 s bounds. Also asserts the throw key is bound to A. |
| `day30_clock` | Day 2 | Infinite Day holds the world clock on Days 2 to 29 and not on Day 30 (regression test for upstream #100). Day 2 with the option off: the clock advances. Option on: it stays put. Then Day 30 at 18:48 with the option on: the clock advances to sunset and day end begins at or after the end hour. |

`throw_release` measures K at run time and never hard-codes it.

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
`CMakeLists.txt`, and add the name to both `foreach(scenario ...)` lists at the
end of that file (disc and stand-in). Then run it once on the stand-in tree.
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
`blo.h`, `pcr.h` and `standin_ui.h` (screens, particles), and `parms.h`,
`archive.h`, `stages_ini.h` and `gen.h`.

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

- Audio is off. Scenario builds use `PIKMIN_NATIVE_JAUDIO=OFF` and
  `SDL_AUDIODRIVER=dummy`, so nothing audio-related is tested.
- No cutscene or movie stand-ins. The stand-in stage plays no landing movie and
  the post-extinction movie finds no file and ends at once. The sandbox has no
  enemies, bosses, plants, gates or bridges.
- Stand-in and disc runs can differ in timing. `throw_release` measures K, the
  press-to-grab delay, per run because the stand-in value need not equal the
  disc's.
- A scenario needs a GL context, even hidden. Without one the run exits 4. On
  Windows, Mesa's `opengl32.dll` and `libgallium_wgl.dll` must sit beside the
  executable.
