# Quick Save phase 2b verification

Verified locally on 2026-10-17 (Pacific) against `feat/quick-save-ux`,
`cc3e1c0e65acbfc89d99ab43fce07f03c730f648`. The nine commits after
`feat/quick-save-file` were read. No game-source edits were required by these
checks. This is macOS runtime evidence; Windows/Linux were not rebuilt here.

## Build and host tests

A new build directory was used after discovering that the previous scratch
build contained stale objects: its executable lacked the F1 rows even though
their source was present. Source timestamps were older than those objects.
Do not reuse that executable as evidence for this commit.

```sh
cmake -S /Users/kylenakamura/documents-local/development-local/side-projects/pikmin/.claude/worktrees/agent-a2d873c4412ff04e0/games/pikmin1 \
  -B /private/tmp/pikmin-qs-pickup/build-clean \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DPIKMIN_BUILD_LAUNCHER=OFF -DNECTAR_BUILD_MAPMAKER=OFF
CCACHE_DIR=/private/tmp/pikmin-qs-cache cmake --build /private/tmp/pikmin-qs-pickup/build-clean -j 8
ctest --test-dir /private/tmp/pikmin-qs-pickup/build-clean --output-on-failure
```

Build passed. Host tests: **37/38 passed**, including both quick-save test
executables. The sole failure is the shader mismatch already recorded in the
incoming handoff: generated `uChan0AlphaEn` and `uChan0AlphaDiff` uniforms are
unknown to the ubershader. Launcher and mapmaker were disabled for this fresh
game/test build; app packaging and installation were not performed.

Clean game executable SHA-256:
`1669b7b891892a8f2e8ff23095c1b2b886dbb5ec6eabb24dabf75593bf5bfd5a`.

## Runtime results

All runs used copied card files, isolated settings and `NECTAR_SAVE_DIR` under
`/private/tmp/pikmin-qs-pickup`. The installed game and real saves/settings were
not replaced. Audio output was a dummy device; audibility was not tested.

| Check | Result | Evidence |
| --- | --- | --- |
| F1 rows, clean unmodified game build | PASS | All three rows visible under Data / QUICK SAVE. The toggle offers On and Off, displays restart required while pending state differs, and clears that marker when restored. |
| F1 save and load, clean unmodified game build | PASS | `f1-live`: menu save at day tick 962; success notice; menu load closes F1, displays the load screen, matches hash `6c77b0944139f9de`, and resumes gameplay. Load took 3.08 s. |
| F1 disabled actions, clean unmodified game build | PASS | `f1-off-live`: both actions disabled with the off/restart explanation; activating them creates no quick-save directory. |
| Action API overwrite and toggle | PASS | `menu-on-v2`: save at ticks 300 and 800, then load; hash matches. Toggling does not change the simulation settings hash. `menu-off`: disabled save/load and restart-required enable state. |
| Load screen | PASS | Inspected during full-resolution and reduced-resolution catch-up: day, progress, elapsed time, estimate and Esc/B hint visible. Also inspected in the unmodified build's F1 load. |
| Day continued in memory | PASS | `precision-base`: day 4 plants 12 fractional-position sprouts with nonzero yaw, goes to sunset, continues day 5 without card reload, plucks three and saves. |
| Precision block at launch | PASS | `precision-load`: saved hash `88624d0fa6d69280` matches; all 600 checked ticks after handoff match the uninterrupted run. |
| Precision block, in-game load | PASS | `precision-in-game`: same hash matches; all 600 checked ticks after handoff match the uninterrupted run. |
| Title-screen load | PASS | Real F7 starts the newest story save from the title. Separately, `title-complete` loads a day-5 save through the title's menu action, skips a newer corrupt file, matches the saved hash and all 600 checked ticks after handoff. |
| Escape cancellation | PASS | Real Escape cancels title-started catch-up at replay tick 1,531, leaves for card selection and displays the cancellation toast. A captain prompt appears before the file screen in this flow. |

The deterministic fixtures use an exported copy of the current source with the
prior session's scratch hooks for synthetic input, fractional sprouts, sunset,
pluck actions and per-tick state hashes. They are not installed product builds.
The F1 visual, off-state and menu save/load checks above additionally ran the
clean, unmodified executable.

## Low-resolution catch-up

One controlled sequential pair loaded the same 26,990-tick story save, with
30 spawned Pikmin and the same input script. The full-resolution control used
the scratch-only `PIKMIN_QS_NO_LOWRES` switch. Both renders still execute the
presentation pass. The window requested 2560x1080; its measured internal target
was 2560x1018 in this desktop session.

| Mode | Replay | Total load | Replay rate |
| --- | ---: | ---: | ---: |
| Full resolution, 2560x1018 | 120.92 s | 121.87 s | 223 ticks/s |
| Reduced resolution, 1280x509 | 104.64 s | 105.61 s | 258 ticks/s |

Reduced resolution saved **16.26 s (13.3%)** of total load time in this pair.
This is one local measurement, not a general performance guarantee. The aspect
ratio was preserved, and the original resolution request and measured target
were restored after handoff. Both loads matched saved hash `8e1e460e979763b8`;
all **27,050 checked ticks** (11 through 27,060, including live input after
handoff) matched the reference run for each load.

## Test infrastructure corrections

- The old shell runner dropped `--quick-load` from some intended load cases.
  Its resulting timings and hash comparisons were discarded. The replacement
  Python runner records exact arguments and the scenario environment.
- Scripted menu actions initially repeated when a load rewound the tick counter.
  The scratch hook now consumes each menu action once. This was a harness loop,
  not repeated product input.
- Foreground scenarios now reuse `QA-ui-clean.app` and bundle ID
  `local.pikmin.qa.ui-clean`, instead of creating a new permission identity for
  each scenario. The runner records its game PID and terminates that child on
  timeout. The previously orphaned test window was closed.
- Supported Computer Use config enables persistent approval and allows that
  bundle ID. Access succeeded across the subsequent foreground scenarios.
  This does not remove product confirmation requirements for sensitive actions.
  Config backup: `~/.codex/config.toml.bak-pikmin-cua-20261017-234549`.

## Local evidence and reruns

Scratch root: `/private/tmp/pikmin-qs-pickup`.

- `clean-configure.log`, `clean-build.log`, `clean-ctest.log`
- Per scenario: `case.json`, `native.log`, `result.json`, `hashes.txt` where applicable
- `load-screen.png`, `f1-disabled.png`
- Replacement runner: `run.py`; scenario inputs: `menu.json`, `menu-v2.json`,
  `precision-long.json`, `precision-in-game.json`, `load-comparison.json`,
  `title-complete.json`, `f1-live.json`, `f1-off-live.json`

Each run directory is preserved. Choose a new scenario name before rerunning
the Python runner; do not overwrite earlier evidence. Never reuse a quick save
with another executable: the format checks the exact binary hash.

The original fixture scripts, exported source and comparison tools remain at:
`/private/tmp/claude-501/-Users-kylenakamura-documents-local-development-local-side-projects-pikmin--claude-worktrees-storybook-repo-ui-stories-d691b6/ea8dbc65-b68a-4380-b78c-0e82f09a1d49/scratchpad/qs2b`.

No push, merge, installed-app replacement or ticket mutation was made. Merging
the quick-save stack into `kyle-develop` still requires Kyle's decision. The
incoming handoff's reset-all-settings UX question was left unchanged.

## Preserved QA harness

The hook patch, portable runner, synthetic input scripts and scenario templates
are now checked in under `games/pikmin1/tools/quicksave/qa/`. Follow that
directory's README to recreate the exported test source without the old
scratch paths. The preserved patch recreated all seven instrumented source
files byte for byte. Private card fixtures and compiled binaries remain local.
