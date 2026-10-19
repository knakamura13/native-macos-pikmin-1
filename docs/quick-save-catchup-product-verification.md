# Product catch-up verification — 2026-10-18

Quick-load replay now keeps both CPU passes, suppresses world GL submission during presentation, discards deferred geometry while suppressed, and restores real graphics before overlays. The old resolution reduction and its saved state are removed, so loading UI retains the configured resolution. Quick Save remains opt-in and default-off.

The product source is commit `7be75896`; later changes refine only QA artifacts/documentation. Clean unmodified macOS Release build passed (launcher/mapmaker off), SHA-256 `b5b93cf22f54040555021adb389697242f03ca95ca0f0d02320b9262447acfc6`. Host tests: 37/38 passed; the existing shader-uniform mismatch (`uChan0AlphaEn`/`uChan0AlphaDiff`) remains. Windows/Linux and physical audio were not verified.

## Regression and runtime evidence

The QA-only raw GL audit counts renderer calls independently of null-GX. Before the product change it failed at tick4801 with 24,010 calls (exit88). Afterward, measured replay presentation/restoration intervals issued zero raw calls, live graphics resumed, and the overlay-boundary leak guard did not fire. This does not measure all frame GPU work: initialization, load UI, post-processing and swap remain active.

| Scenario | Required comparison | Result |
| --- | --- | --- |
| Precision launch, in-game and title loads | 4801..6400, all columns | Identical for each; saved state matched |
| Long replay with 30 synthetic Pikmin | 11..27100, all columns | 27,090 identical ticks; 24.54 s replay / 24.92 s total load |
| VS arena | 11..3200, all columns | 3,190 identical ticks |
| Three loads in one process | 11..2200, last occurrence of each tick | 2,190 identical ticks; three saved-hash matches and three graphics-audit passes |
| Forest of Hope, continued day 4 | 247..4200, all columns | 3,954 identical ticks; dumps show 38 live enemies |
| Forest Navel, continued day 4 | 247..4200, all columns | 3,954 identical ticks; dumps show 58 live enemies |

Computer Use verified native 2560x1018 loading progress and rendered gameplay after the long load. Escape cancellation returned to the card-select path's captain prompt with a cancellation toast and working graphics. All game processes used for QA were closed. No installed app or real card was modified.

The long-load timing is one local arm64 macOS sample with dummy audio and hash logging. The preceding same-executable experiment measured 103.93 seconds with the old reduced-resolution rendering versus 27.49 seconds with suppression (73.5% reduction); the product result is consistent with that direction but is not a controlled cross-build speed ratio.

## QA corrections and limitations

The first extra-area selector accidentally chose challenge maps sharing story IDs; those forced runs diverged when loading the story map. They are excluded. Corrected ordinary-day fixtures select story maps and pass. A forced day-1 Forest of Hope save was blocked by its intro; an early repeated-load case also had no save because it ran during the intro. Those are not passes. The final repeat case uses VS and verifies three actual loads.

A synthetic enemy/bomb fixture crashed in the straight recording before saving, so it provides no replay evidence and is not shipped as a passing scenario. Natural live enemies in the two ordinary-area fixtures provide the additional enemy-state coverage. A later bounded Impact Arena throw fight is recorded in [quick-save-combat-effects.md](quick-save-combat-effects.md): one bulborb took damage, and ticks 11..2000 matched. Bomb throws, HD texture packs, broader real-play soak tests and Windows/Linux remain follow-ups.

Audited binaries changed as QA-only map/dump hooks evolved. Every save/load pair used the same exact executable. [Machine-readable results](../games/pikmin1/tools/quicksave/qa/catchup-product-results-macos.json) preserve each group hash and relevant events; [QA instructions](../games/pikmin1/tools/quicksave/qa/README.md) preserve scenarios and export/build steps. QA patches are outside the game build and must never be installed or distributed.

Independent source review found no substantive product defect. It confirmed frame initialization precedes suppression and ordinary deterministic/netplay rendering is unchanged. Its documentation notes were addressed. This remains fork review under [feature issue #17](https://github.com/knakamura13/native-macos-pikmin-1/issues/17); heavy human testing and platform verification are still required before upstream readiness.
