# throw_release K: stand-ins vs disc

`throw_release` reports K, the ticks from pressing A to the grab completing
(`mIsHoldingThrowPiki`). The values below were measured 2026-10-27 on a
RelWithDebInfo build of `e10b00be`, macOS. Each run is deterministic: repeated
runs give the same K.

| Tree | 30 Hz | 60 Hz |
|---|---|---|
| Disc (VERSION_GPIE01_01, Forest of Hope) | 13 | 14 |
| Stand-ins (sandbox) | 12 | 23 |

At that point the stand-ins were unchanged, because the difference does not
come from a stand-in parameter, animation or timing value. The sandbox walls
were fitted later; see "Fitted sandbox walls" below.

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

## Fitted sandbox walls (after the particle RNG change)

With the particle draws on the cosmetic stream (`standin-particle-rng.md`), the
disc K is 15 / 18. The stand-in generator now builds the sandbox with four
short posts across the walk instead of the wall at x = 700
(`kSandboxWalls` in `standin_generator.cpp`), and the stand-in K equals the
disc K.

RelWithDebInfo, macOS, `nectar --scenario throw_release`, 3 runs per cell, all
runs equal and passing. The disc tree is the install with the particle RNG
change; the stand-in data is `nectar_standin_generator` output.

| Tree | 30 Hz before | 60 Hz before | 30 Hz after | 60 Hz after |
|---|---|---|---|---|
| Disc (Forest of Hope) | 15, 15, 15 | 18, 18, 18 | 15, 15, 15 | 18, 18, 18 |
| Stand-ins (sandbox) | 12, 12, 12 | 23, 23, 23 | 15, 15, 15 | 18, 18, 18 |

"Before" is `ae235144`. The game binary is the same before and after (only
stand-in data changed), so the disc row is one set of runs.

The walls (centre lines, thickness 40, height 150; the captain starts at the
origin and walks to -z):

| Post | a (x, z) | b (x, z) |
|---|---|---|
| 1 | (-20, -445) | (-2, -426) |
| 2 | (60, -517) | (42, -482) |
| 3 | (21, -466) | (-4, -455) |
| 4 | (73, -486) | (45, -471) |

### How the posts set K

Temporary per-tick tracing (removed before commit) showed:

- The captain reaches the posts in the last second of the 4 s walk and is
  deflected between them. Collision is resolved per tick, so he ends the walk
  at z = -510 at 30 Hz and at z = -483 at 60 Hz.
- The squad trails, as on open ground. At 60 Hz the captain is further back, so
  the nearest Pikmin is closer when A is pressed: 76 units at 30 Hz, 52 units
  at 60 Hz. It walks to the hand around the posts.

The disc reaches its K from a different situation. The trace of the disc run
shows the captain walking about 1.3 s, sliding along a wall, and stopping
about 220 units from the start. The squad catches up, then switches to
`BreakwallMode` (a Forest of Hope gate), and returns in `PIKISTATE_Emotion`.
A is pressed when the first Pikmin is Normal in the plate again, from rest:
62 units at 30 Hz, 39 units at 60 Hz. The sandbox has no gate, so it matches
the resulting K, not the squad behaviour.

### How the posts were found

The posts are fitted by search, not derived from the disc. Each layout was
generated, then run at 30 and 60 Hz (about 2,900 layouts):

- One wall across the walk at 200 to 650 units and -60 to 60 degrees: K pairs
  (30 Hz / 60 Hz) up to 13 / 23. A straight wall stops the captain early, a
  Pikmin settles within the 15-unit grab range, and K = 1 (fails K >= 3).
- One to three random long walls: no pair with K30 >= 13 and K60 <= 21. On open
  ground K60 is close to twice K30.
- A stopper wall plus three to five posts: pairs such as 15 / 22 and 16 / 15.
  Small random moves around the closest layouts gave six layouts in range,
  including 15 / 18. Its stopper was then removed: the captain does not reach
  it, and K stays 15 / 18.

### Limits

- The match is fragile. Moving every post end by about 2 units (40 random
  moves) gave pairs from 8 / 17 to 15 / 29; only 3 of 40 stayed in range and passed.
- A change to the squad or captain AI, to collision, or to the sim RNG draws
  can move the stand-in K and the disc K independently. Re-measure both after
  such a change, and re-fit the posts if the stand-in K must match again.
- Matching K does not mean the grab starts from the same distance or state as
  on the disc (76 / 52 units moving, against 62 / 39 units from rest).

## Consequences

- (Written before the particle RNG change and the fitted walls.) Matching K
  within one tick would mean building sandbox terrain that copies the Forest
  of Hope wall and its RNG-dependent behaviour of the squad. It would also mean
  drawing the disc's particle RNG sequence, which needs every disc emitter's
  rates and jitter flags. That is disc data. The particle draws have since
  moved, and the fitted walls match K without copying the squad behaviour.
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
