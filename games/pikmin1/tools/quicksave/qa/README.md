# Quick Save QA fixtures

These preserve the scratch harness used for the phase 2b checks. They are
developer tools, outside the game build. The patch adds synthetic live input,
sprout planting, plucking, sunset, state dumps, and a full-resolution control.
**Never install or distribute the hooked executable.**

## Build the instrumented copy

From the repository root, choose an unused output directory:

```sh
python3 games/pikmin1/tools/quicksave/qa/prepare.py --out /tmp/pikmin-qs-hooks
cmake -S /tmp/pikmin-qs-hooks/src/games/pikmin1 -B /tmp/pikmin-qs-hooks/build \
  -DCMAKE_BUILD_TYPE=Release -DPIKMIN_BUILD_LAUNCHER=OFF -DNECTAR_BUILD_MAPMAKER=OFF
cmake --build /tmp/pikmin-qs-hooks/build -j 8
```

`prepare.py` exports a committed ref (default HEAD) and patches that copy, so
uncommitted source edits are excluded. A patch failure is a source-version
mismatch, not a reason to apply the hooks to the real checkout. The historical patch was captured against `cc3e1c0e`. On product catch-up
source, `prepare.py` omits only the obsolete low-resolution control hunk; the
remaining hooks still apply. See the current audit workflow below.

## Isolated runs

`run.py` reads the local `~/.local/share/pikmin-native` by default. Override
`--fixtures` with a directory containing `assets/`, `save/` and
`pikmin_settings.conf`. It copies card files/settings into a fresh per-case
directory, links assets read-only in practice (the game reads those files),
sets `NECTAR_SAVE_DIR`, and uses dummy audio. It never edits the fixture root.
Actual save files, settings, binaries and build caches are not in this repo.

```sh
python3 games/pikmin1/tools/quicksave/qa/run.py \
  games/pikmin1/tools/quicksave/qa/menu.json \
  --out /tmp/pikmin-qs-cases --exe /tmp/pikmin-qs-hooks/build/bin/nectar
```

Cases accept literal `{out}`, `{repo}`, `{scripts}` and `{exe}` placeholders.
Existing case directories are refused: change the case name or output root to
rerun. Run foreground cases sequentially. On macOS all foreground cases reuse
bundle ID `local.pikmin.qa.ui-clean` and app `QA-ui-clean.app`; allow that app
once through Computer Use. `f1-live.json` and `f1-off-live.json` must use a
**clean, unmodified build**, and need manual F1 input. Close their test window
when done. The runner terminates its own game child on timeout.

## Fixture dependencies

- `menu.json`: no particular card state required; tests enabled actions and
  restart-required toggle. `scripts/menu-off.txt` provides disabled action API
  attempts; `f1-off-live.json` is the visible UI version.
- `precision-long.json`: first records a continued-in-memory day-5 save, then
  loads it, then records a long day-1 save. The precision case requires a normal
  story save in slot 1 at **day 4**. This private prerequisite is intentionally
  not checked in. Its artificial sprouts/sunset/plucks are scratch hooks.
- `precision-in-game.json`: uses `precision-base/save/quicksave/1.qsv` from
  that same executable, recreates the day and requests a load in play.
- `load-comparison.json`: uses `long-base/save/quicksave/0.qsv`; full-res control
  followed by low-res. `PIKMIN_QS_NO_LOWRES` exists only in the hook patch.
- `title-complete.json`: prepare `{out}/title-fixture` as a copy of
  `precision-base/save`, with an optional newer invalid `quicksave/2.qsv` to
  test fallback. It loads the valid day-5 save via the title's menu-row API.

Do not rebuild between creating a quick save and loading it. Executable hashes
must match; no bypass is provided. Each run writes `case.json` (arguments and
scenario environment), `native.log`, `hashes.txt` and `result.json`. A zero exit
code alone is insufficient: inspect the expected save/load/hash-match events
and compare the required tick range:

```sh
python3 games/pikmin1/tools/quicksave/qa/compare_hashes.py \
  /tmp/pikmin-qs-cases/precision-base/hashes.txt \
  /tmp/pikmin-qs-cases/precision-load/hashes.txt \
  --from-tick 5801 --through-tick 6400
```

For the long fixture, compare ticks 11..27060 for each load against long-base.
For an in-game load, the comparator uses the last occurrence of each tick,
so the comparison covers the replayed timeline rather than the abandoned one.
The comparator fails on missing ticks, an empty range or any differing column.

See `docs/quick-save-phase-2b-verification.md` for the original measured results
and limits. The earlier skip-entire-presentation experiment is ruled out for
correct loading: its RNG hash diverged. A GPU-only suppression experiment must
preserve both CPU passes and establish matching hashes before adoption.

## Historical GPU suppression experiment

`catchup-gpu.patch` applies to the historical pre-optimization game code at
`fa2a74bf` (export that ref with `prepare.py --ref fa2a74bf`). Do not apply it to
the product implementation, which already suppresses presentation graphics.
This was an additional, test-only experiment. After `prepare.py`,
apply it inside the exported `src/` with `patch --batch -p1 < /absolute/path/to/catchup-gpu.patch`.
Build once, then record and load with that same executable. Set
`PIKMIN_QS_TEST_NULL_GPU=1` only on experimental loads; leave it unset for the
control. It keeps both CPU passes and suppresses presentation GL during catch-up,
restoring it before the loading UI and swap. `[QS-GPU]` reports suppressed frames
and real GL calls observed inside the gated presentation interval. This is not a
claim that the entire frame performs zero GPU work. Hash correctness and a measured
speedup are prerequisites for considering a product implementation.


The experiment scenarios use the same `--out` root in this order:

1. `catchup-gpu-precision.json`: record the precision fixture, then control and
   experimental loads. Compare ticks 4801..6400 against `precision-base`.
2. `catchup-gpu-record-long.json`: record the long fixture with this executable.
3. `catchup-gpu-comparison.json`: sequential control and experimental long loads.
   Compare ticks 11..27060 against `long-base` for both.
4. `catchup-gpu-load-paths.json`: in-game and title load checks. First copy
   `precision-base/save/` to `{out}/title-fixture/`; optionally add a newer invalid
   `quicksave/2.qsv` there to test fallback. Compare ticks 4801..6400, using the
   last occurrence of a tick, against `precision-base`.
5. `catchup-gpu-visible.json`: a foreground long load with time to inspect the
   loading overlay and resumed game. `catchup-gpu-cancel.json` is the equivalent
   cancellation check: press Escape during replay, inspect the destination, then
   close the test window. These checks require Computer Use.

Invoke `run.py` as above with the selected JSON file. All cases refuse existing
case directories. Reference save files are specific to the exact instrumented
executable; do not rebuild between recording and loading.

`catchup-gpu-fullres.json` repeats the experimental long load without the
resolution reduction, using the existing test-only `PIKMIN_QS_NO_LOWRES` hook.
See `docs/quick-save-catchup-gpu-experiment.md` and
`catchup-gpu-results-macos.json` for the measured macOS results and limits.
For cleanup after closing a test app, wait for the runner to finish before
launching another case; do not query a closed app binding, which may relaunch it.


## Current product catch-up audit

Build the product code normally for shipping. For isolated QA, export an audited
copy with `prepare.py --out /tmp/pikmin-qs-product --audit-catchup`, configure and
build that exported `src/` as above, and never install it. `catchup-audit.patch`
adds a raw renderer GL counter independent of the null-GX flag, an overlay-boundary
leak check, and a test-only area selector. Set `PIKMIN_QS_AUDIT_GPU=1` in test cases.
The measured interval starts before capture/renderall and ends after shape/camera
restoration and deferred-batch discard. It does not include frame setup or UI.
An unexpected raw call exits 88; a failed return to live rendering exits 89;
null-GX leaking into the VI overlay boundary exits 90. `[QS-AUDIT] PASS` reports
replay frame count, zero measured replay GL and resumed live GL calls.

Run `catchup-product-record.json`, then copy `precision-base/save/` to
`{out}/title-fixture/` (optionally add a newer invalid `quicksave/2.qsv`), then run
`catchup-product-loads.json`. Compare precision/in-game/title ticks 4801..6400
against `precision-base`; compare long-load ticks 11..27100 against `long-base`.
The foreground long load leaves time to inspect native-resolution loading and
resumed play before exiting naturally.

`catchup-product-scenes.json` records/loads a VS arena and runs three loads in
one process against a straight reference. Compare VS ticks 11..3200 and repeated
loads ticks 11..2200. `catchup-product-areas.json` uses the private day-4 slot-1
card fixture and the QA-only map selector to test ordinary story areas. Compare
ticks 247..4200 and confirm stage paths and live enemy counts in the dumps.
`catchup-product-cancel.json` requires the long fixture from the same executable;
press Escape during replay, inspect the destination and close the window.
These synthetic regressions do not replace real-play soak QA.

The original product checks used three audited executable builds as the QA-only
area hooks evolved. Each group recorded and loaded with its own exact executable;
no compatibility bypass was used. Their hashes and event excerpts are in
`catchup-product-results-macos.json`. Current hooks can reproduce all scenarios
with a single fresh build. See `docs/quick-save-catchup-product-verification.md`.
