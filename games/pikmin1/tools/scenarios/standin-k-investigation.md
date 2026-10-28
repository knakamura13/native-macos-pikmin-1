# throw_release K: stand-ins vs disc

`throw_release` reports K, the ticks from pressing A to the grab completing
(`mIsHoldingThrowPiki`). The values below were measured 2026-10-27 on a
RelWithDebInfo build of `e10b00be`, macOS. Each run is deterministic: repeated
runs give the same K.

| Tree | 30 Hz | 60 Hz |
|---|---|---|
| Disc (VERSION_GPIE01_01, Forest of Hope) | 13 | 14 |
| Stand-ins (sandbox) | 12 | 23 |

The stand-ins are unchanged, because the difference does not come from a
stand-in parameter, animation or timing value.

## What is the same on both trees

Temporary per-tick tracing of case 0 showed:

- **Selection.** The grabbed Pikmin is beyond `mPluckGrabRange` (15), so it
  goes to `PIKISTATE_GoHang` and walks to the hand.
- **Walk.** The target speed is 240 beyond 30 units (2 x grab range), then 120.
  This is the same on both trees.
- **Hand-over.** It happens when the Pikmin is within 15 units of the captain.
- **Grab.** ThrowWait (motion 36) starts at counter 0. `KEY_Action0` fires at
  counter 1.0, which is 1 tick later at 30 Hz and 2 ticks later at 60 Hz. This
  is the same on both trees.

## What differs

K is mostly the length of the walk. The walk starts where the Pikmin stands on
the tick the scenario presses A, and that position depends on the map and the
sim RNG.

- **Disc.** The 4 s walk away drives the captain into a wall near the ship. The
  squad leaves the plate, and later comes back in `PIKISTATE_Emotion`. The
  Settle phase presses A when the first Pikmin is Normal again:
  - At 30 Hz, the Pikmin is 58 units away and the press is on tick 239.
  - At 60 Hz, the Pikmin is 28 units away and the press is on tick 544.

  The walk is also slower than on open ground (55 to 130 u/s instead of up to
  220). The likely cause is map collision, because no other Pikmin is near for
  most of the walk. This cause is not checked.
- **Stand-ins.** On the open sandbox the squad trails the captain. A is pressed
  as soon as the nearest Pikmin is within 80, at about 79 units, at both rates.
  The time is about 0.4 s at both rates, so K roughly doubles at 60 Hz.

The disc's 13 and 14 are close only by chance. In seconds the two grabs take
0.43 s and 0.23 s, because the starting distances are different.

## Falsifying checks

- **Stand-in at the disc's press distance.** With the press forced at the disc's
  distance (temporary env override), stand-in K is 9 at 58 units (30 Hz) and 11
  at 28 units (60 Hz). The disc's slower walk explains the remaining 3 to 4
  ticks.
- **Particle RNG.** `zen::Rand` (`include/zen/Math.h`) is `gsys->getRand`, which
  is `pc_sim_randf`. `particleGenerator.cpp` and `zen/particle.h` draw from the
  sim stream for each emitted particle and on every frame. With only those draws
  moved to `pc_cosmetic_randf`, the disc K becomes 15 / 18 (2 runs each). The
  stand-in K stays at 12 / 23, because the inert stand-in effects never emit.
  Effect content therefore changes the squad's AI path, and so K. This matches
  the #72 overlay result (disc tree with the stand-in `effects.bun`: K = 8 at
  30 Hz).

## Consequences

- Matching K within one tick would mean building sandbox terrain that copies
  the Forest of Hope wall and its RNG-dependent behaviour of the squad. It would
  also mean drawing the disc's particle RNG sequence, which needs every disc
  emitter's rates and jitter flags. That is disc data.
- Stand-in K is stable, and the scenario asserts only behaviour. Both trees
  pass.
- Moving particle draws to the cosmetic stream would remove the effect-content
  coupling. It is a game-code change, and it would change deterministic-mode
  state hashes and replays. Nobody has decided whether to make it.
- Case 2 releases at `K + 2`. The case-0 Pikmin calibrates K, so if the case-2
  Pikmin starts farther away, the release comes before the grab. With a forced
  short K (6 at 30 Hz, 11 at 60 Hz) on the stand-ins, case 2 failed with "never
  Hanged during ThrowWait". The same failure can happen on the disc when K gets
  shorter, which is consistent with the #72 overlay failure at 60 Hz.
