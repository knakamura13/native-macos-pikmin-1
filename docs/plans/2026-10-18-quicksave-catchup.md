# Quick-save Catch-up Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Determine whether suppressing gameplay GPU submission during catch-up is correct and measurably faster.

**Architecture:** Apply a separate QA patch to an exported tree after the preserved hooks. Gate only the presentation pass with existing null-GX while catch-up and an explicit environment flag are active; flush discarded geometry and restore real GL before overlays and swap.

**Tech Stack:** C++, existing GL/null-GX renderer, CMake, Python QA runner and hash comparator.

## Task 1: Preserve an opt-in experiment

Create `games/pikmin1/tools/quicksave/qa/catchup-gpu.patch`, modifying exported `games/pikmin1/src/plugPikiColin/plugPiki.cpp` only. Read `PIKMIN_QS_TEST_NULL_GPU`; when exactly `1` and `pc_netplay_det_catchup()`, enable null-GX before presentation setup, execute existing presentation code, discard the batch while null, and restore normal GL. Accumulate suppressed frames and delta in the real-GL tripwire, logging at catch-up exit. Document application in the QA README and preserve patch whitespace in `.gitattributes`.

Verify with `git diff --check`, `prepare.py` into a new scratch directory, `patch --dry-run --batch -p1`, then apply. Configure a fresh Release build with launcher/mapmaker off and ccache. Do not install.

## Task 2: Correctness gate

Use the existing precision and long-input scenarios, using one executable for recording and both control/experimental loads. Generate separate flag-off and flag-on cases. Run precision recording, both precision loads, and compare every required replay and 600 live ticks using `compare_hashes.py`. Require a saved hash match, null-GL tripwire zero and at least one suppressed frame. Stop performance claims if any required state differs.

## Task 3: Measure and report

Record the 27,000-tick long fixture with that same executable. Load sequential control and GPU-suppressed variants at the same requested resolution/settings. Compare every tick 11..27060 with the reference; inspect load-screen and post-load UI through Computer Use. Extract replay and total-load timings from native logs. Run relevant host tests and document exact build, fixture, results and limits. Commit the patch, scenarios and report on the isolated spike branch; keep PR #16 unchanged. Link substantive findings to issue #17 using the authorized GitHub tracking workflow.
