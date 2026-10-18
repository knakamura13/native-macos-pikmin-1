# Quick-save catch-up GPU suppression experiment

The test-only null-GX experiment preserved replay state and made the long load substantially faster in the measured macOS runs. It is a viable direction for a product implementation, with further testing still required. The game defaults and PR #16 are unchanged by this experiment.

## Method

The source export was commit `6118f1dc79506c3d685359158d8b4c5809494f97`, containing game code identical to `3ae1dc0be4a00384496bc7385d246b3799dbac49`. Apply the preserved `test-hooks.patch`, then the new [catchup-gpu.patch](../games/pikmin1/tools/quicksave/qa/catchup-gpu.patch), to an exported source tree. Build once and use that exact executable to record and load. Both CPU render passes still run; during catch-up only, `PIKMIN_QS_TEST_NULL_GPU=1` gates the presentation pass with existing null-GX, discards its deferred geometry, and restores real graphics before overlays and swap.

Release CMake build passed on arm64 macOS 27.0.1 (26A434), with launcher/mapmaker off. Executable SHA-256: `79a703fd22a54d0fdd52f85d13514d2ee9da39afb9cd97be3ce6eb1d70a939e1`. Source patches, scenario names, timings, event excerpts and comparisons are preserved in [machine-readable results](../games/pikmin1/tools/quicksave/qa/catchup-gpu-results-macos.json). Follow the [QA README](../games/pikmin1/tools/quicksave/qa/README.md) to reproduce; private card files and binaries are not published.

The long fixture records 26,990 replay ticks with 30 synthetic Pikmin and scripted input. Its saved state hash is `2638fdb9766296b1`. Foreground loads ran sequentially with the same executable, save and requested 2560x1080 window; the actual window was 2560x1018. Dummy audio and per-tick hash logging were enabled. Timing is from the game's load instrumentation, excluding the later live-input observation period.

| Long-load variant | Actual catch-up resolution | Replay | Total load | Replay throughput |
| --- | --- | ---: | ---: | ---: |
| Existing rendering, reduced resolution | 1280x509 | 103.57 s | 103.93 s | 261 ticks/s |
| Null-GX presentation, reduced resolution | 1280x509 | 27.10 s | 27.49 s | 996 ticks/s |
| Null-GX presentation, native resolution | 2560x1018 | 23.00 s | 23.36 s | 1174 ticks/s |

The matched reduced-resolution pair reduced total load time by 73.5% (3.78 times faster); replay itself was 3.82 times faster. The native-resolution experiment also preserved state and showed that resolution reduction is unnecessary for correctness in this fixture. These are individual local samples, not a controlled repeated benchmark proving that native resolution is faster. A separate visual run at reduced resolution loaded in 24.98 seconds total, illustrating run-to-run variation.

## Correctness and UI checks

- Precision fixture: the control and experimental launch loads matched all columns for ticks 4801..6400, covering the 999-tick replay and 600 ticks after handoff. Saved state `88624d0fa6d69280` matched.
- In-game and title loads with suppression also matched all columns for ticks 4801..6400. The title fixture included a newer invalid `2.qsv` and selected the compatible `1.qsv`.
- Each of the three long-load variants matched all columns for ticks 11..27060 (27,050 ticks), including live input after handoff. The separate visual run matched ticks 11..27100 (27,090 ticks).
- The existing renderer tripwire reported `null_gl=0` across the suppressed presentation intervals: 999 frames for precision, 26,990 frames for each complete long replay. It reported `restored=1` at the next presentation after catch-up.
- Computer Use showed the loading UI updating while suppression was active, both at reduced and native resolution. Rendered gameplay returned after loading; the reduced-resolution load restored actual resolution 2560x1018.
- Actual Escape input cancelled at replay tick 6481/26990, returned to the card-select path's captain prompt with the cancellation toast, and restored actual resolution 2560x1018.
- Host tests: 37/38 passed. The existing `pc_tev_shader_test` failure remains: generated `uChan0AlphaEn` and `uChan0AlphaDiff` are unknown to the ubershader. This experiment does not alter shader code.

One initial visual launch exited 137 after closing/observing the prior app caused a relaunch overlapping the next harness run. That launch-only result is excluded. The leftover test process was terminated and a fresh `null-gpu-visible-v2` run completed successfully. All test game processes were closed afterward.

## Interpretation and next implementation

This suppresses world graphics submission while retaining CPU presentation side effects that the previous whole-pass-skipping experiment lost. The saved hashes, per-tick checks and live handoff support the approach on these fixtures. Time saved includes renderer CPU work and driver submission as well as GPU work; it is not a direct GPU-time measurement.

It is not literally zero GPU work for the entire load: the progress screen, compositing, swaps and any work outside the gated presentation interval remain. The null-GL tripwire covers its instrumented renderer paths; it is not a universal GPU profiler. The loading UI still renders once per replay tick in this prototype.

Next: implement scoped catch-up suppression in product code, retain a native-resolution loading UI, and review null-GX state/counter/frame-lifecycle behavior before removing the old resolution reduction. Exercise additional areas, enemies/effects, repeated loads and cancellation before promotion. Windows/Linux, physical audio and real long-play soak testing remain unverified. Keep the full presentation CPU pass; the prior attempt to skip it diverged.

Feature lifecycle: [issue #17](https://github.com/knakamura13/native-macos-pikmin-1/issues/17). Existing fork implementation: [draft PR #16](https://github.com/knakamura13/native-macos-pikmin-1/pull/16).
