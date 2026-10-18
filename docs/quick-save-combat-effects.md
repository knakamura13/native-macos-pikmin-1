# Quick-load combat check — 2026-10-18

This checks the product catch-up at commit `b8479302bf62d1aa7468c67463075d06f5eeb82c` while a fight is underway. World graphics stay suppressed during replay. Both CPU presentation passes still run. No product source changed for this check.

The executable is a QA-audited macOS Release export of that commit, launcher and map maker off. SHA-256 `0ff939624eb94848ed4315b87da7106abf2ba437eccb97814cfc4dafd3fd9877`. It was not installed. The run used dummy audio and an isolated copy of the settings and card files. Kyle's private saves were not written.

## Scenario

`games/pikmin1/tools/quicksave/qa/combat-effects.json` plays an ordinary Impact Arena match. The squad, bomb piles, and sleeping bulborbs come from the map. The script does not use `PIKMIN_QS_SPAWN`, `PIKMIN_QS_TEKI`, or `PIKMIN_QS_BOMBS`. It walks south through the base, east through the south gate gap, whistles, and throws. It does not press the bomb button. Lock-on and charge are off. The save is requested at tick 1301. The load replays that recording, then plays the post-save inputs live.

The straight case exited 0 and wrote `quicksave/0.qsv` before the load was judged. Saved state: day 1, tick 1290 of the day (tick 1300), hash `f7877ff635250292`.

## Result

`compare_hashes.py` reported `IDENTICAL: 1990 ticks (11..2000), all columns` between the straight recording and the load. That range includes the replay and the live ticks after handoff.

The load graphics audit printed `[QS-AUDIT] PASS replay_frames=1290 replay_gl=0 live_gl=23659 null_off=1`. Measured replay presentation issued no raw GL. Live rendering resumed. This does not claim the whole frame did zero GPU work: the loading UI, frame setup, and swap still run.

Two bulborbs were alive (type 4, 1100 health each, health sum 2200). The captain reached the nearer one before the save. Only that enemy took damage. The health-sum drop equals the minimum-health drop at every sample below, so the other enemy stayed at 1100. Bomb items stayed 0. No Pikmin were holding or placing a bomb.

| Tick | Nearest distance | Minimum health | Health sum | Attacking | Airborne throws |
| --- | ---: | ---: | ---: | ---: | ---: |
| 11 | 1028.83 | 1100.000 | 2200.000 | 0 | 0 |
| 800 | 75.68 | 1100.000 | 2200.000 | 0 | 0 |
| 1200 | 75.82 | 1049.782 | 2149.782 | 4 | 1 |
| 1300 | 76.64 | 1036.860 | 2136.860 | 6 | 0 |
| 1600 | 76.83 | 983.108 | 2083.108 | 4 | 0 |
| 1900 | 76.81 | 989.360 | 2089.360 | 3 | 0 |

Tick 1300 is the saved tick. Ticks 1600 and 1900 are live ticks after the load. The damaged enemy's health was lower after the load than at the save, then rose by about 6 points by tick 1900. The load dump matched these combat lines.

The game reported the replay at 1.36 s (946 ticks/s) and 1.65 s from the start of the load. That is one local sample from this run, not a speed comparison.

## Excluded runs

Two earlier scripts on this same binary walked into arena walls and never reached an enemy. They are not combat evidence.

| Run | Where the captain stopped | Nearest enemy | Enemy health | Hash comparison |
| --- | --- | ---: | --- | --- |
| First | Base gate, about x=-1178, z=-68 | about 858 | 2200, unchanged | Identical, ticks 11..2400 |
| Second | South map edge, x=-1178.50, z=-866 | about 934 | 2200, unchanged | Identical, ticks 11..2600 |

Both of those loads also passed the graphics audit. The passing script uses the south speed measured on the second run, 2.64 units per tick. The older synthetic enemy/bomb recording that crashed before saving stays excluded as well.

## Limitations

One of the two bulborbs took damage. The script never threw a bomb, and the dumps showed no loose bombs or bomb carriers. Audio was the dummy driver, which is not a physical audio test. Nobody played this fight by hand. Effect-shape counts were not recorded, because that list is protected. Windows, Linux, HD texture packs, and extended human testing were not part of this run. Issue #17 stays in fork review.

The QA patch and this scenario are outside the game build. Do not install or distribute the audited binary. Machine-readable figures are in [combat-effects-results-macos.json](../games/pikmin1/tools/quicksave/qa/combat-effects-results-macos.json).
