# Quick-save catch-up GPU suppression experiment

Approved scope: resume the proposed isolated performance investigation. This is a QA experiment, not a change to the game's default behavior or PR #16.

Catch-up currently executes both deterministic CPU render passes and submits the second pass to GL at reduced resolution. Skipping that entire pass previously caused RNG divergence. Hypothesis: run both CPU passes, but use the existing null-GX gate for the second pass during catch-up. This may remove world graphics submission without losing CPU side effects. The load overlay and swap remain active after null-GX is restored.

Alternatives: keep reduced resolution as the control; rasterizer discard could avoid fragments but retains uploads and driver calls; skipping the whole presentation pass is ruled out by the prior divergent run.

Implement only as an additional QA patch under `games/pikmin1/tools/quicksave/qa/`, applied after the existing hooks to an exported source tree. `PIKMIN_QS_TEST_NULL_GPU=1` enables it only during catch-up. Always discard queued geometry before leaving null-GX. Preserve camera/matrix restoration and both CPU passes. Record suppressed frame count and real GL calls observed while gated. Do not call this literally zero GPU work: the load screen still renders, and work outside the gated pass may remain.

Success requires matching the reference state hashes throughout replay and the live handoff, no measured GL calls inside the gated interval, visible loading UI, restored gameplay, and a sequential same-fixture speed comparison. A failed state comparison rejects the optimization regardless of speed.
