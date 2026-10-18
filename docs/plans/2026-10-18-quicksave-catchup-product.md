# Quick-save Catch-up Product Implementation Plan

**Goal:** Make quick-load catch-up suppress world graphics submission by default while retaining both CPU passes and a native-resolution loading screen.

**Approved design:** The measured null-GX experiment is the basis. Only an active catch-up changes presentation submission; normal deterministic play and netplay keep their existing rendering. Flush/discard geometry while null-GX is active, restore graphics before overlays, keep shape/camera restoration, and remove low-resolution save/restore state. Quick Save remains opt-in and default-off.

**Isolation:** `knakamura/quick-save-catchup`, based on the preserved experiment; stack its PR on `feat/quick-save-ux`. Do not merge or change PR #16's feature branch.

## 1. Regression evidence before product changes

Add a separate QA-only audit patch that counts raw renderer GL calls regardless of the null-GX flag, checks presentation submission during catch-up, and checks that null-GX is off at the overlay boundary. Build the old product code with this audit and the existing hooks. Record/load the short precision fixture and require the old implementation to fail because it submits GL during replay. Preserve this red result; no test switch bypasses executable compatibility.

## 2. Product implementation

Modify `games/pikmin1/src/plugPikiColin/plugPiki.cpp` to turn on null-GX for the catch-up presentation pass, then discard the batch and restore real GL before leaving the pass. Preserve all existing CPU setup, renderall and shape/camera restoration.

Remove catch-up resolution state/helpers/calls and the obsolete gfx include from `games/pikmin1/pc_port/quicksave/pc_quicksave.cpp`; update load logging and relevant comments. Keep QA hooks compatible by omitting only their legacy low-resolution control hunk when the exported source no longer contains that helper. Preserve historical experiment artifacts.

## 3. Verification and delivery

Build clean unmodified product code and an exported audited copy. Run the host suite, precision, in-game/title, long replay, different available areas or modes, and repeated-load checks. Compare every required state-hash column through replay/live handoff; require the audit to show zero raw GL calls inside catch-up presentation and null-GX off before UI. Use Computer Use for native-resolution loading, restored play, and cancellation. Report gaps honestly, including platform limitations and the known shader test.

Request independent review of the focused product diff and audit evidence; resolve substantive findings. Commit and push, open a stacked fork PR with a link to issue #17, attach it to this chat, label `goal:performance` and `upstream:candidate`, update #17, and verify remote head/body. Do not merge.
