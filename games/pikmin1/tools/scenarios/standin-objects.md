# Object stand-ins and the object sandbox

What the stand-in tree (`nectar_standin_generator`) writes for objects, plants,
gates, bosses and enemies, and the second stage that places some of them.
Issue #59 and #62 rules apply: nothing is copied from the disc. The numbers that
only the disc has were read once, read-only, and each is named in the file that
uses it.

## What the disc was read for

Per AnimMgr table and bundle (`libs/nectar_assets/src/standin_object_profiles.cpp`):

- the info count, and per info the frame count N, the anim-key frames, the
  info-key records (which anim key each `KEY_*` event sits on) and the `p00`
  flags and `spd` parameters;
- the joint count of the model (the first int of the bundle payloads).

Per model written here (`naka1`, `naka4`, `naka7`, `chappy`): the joint count and
parents, and the collision parts (id, code, joint, radius, centre), as for the
first 45 models (`standin_models.h`).

For the world models below (`standin_world.h`): the joint parents and the
collision parts (id, code, joint, radius, centre, or the platform `.mod` a part
names) of 15 models, and the numbers of the 31 `tekipara/*.bin` files and the two
boss `parms.bin` files, carried as words (a `tekipara` file is a version int,
eight ids and the type's int and float parameters in order; the counts differ per
enemy, so the words are not split).

Not read and not written: curves, geometry, textures, animation names, event keys
(sound and effect cues; their counts per info are 0 to 19), the `pelMgr` carry
weights, the plants' light-flare groups and anything else.

## What is written (`libs/nectar_assets/src/standin_objects.cpp`)

| Files | Notes |
|-------|-------|
| 48 `.anm` bundles and their AnimMgr tables (`anims.bin`, `.key`, `gate.bin`, `whiteN.bin`, `plants/*.bin`, `ufoparts/*.bin`, `ufo0705.bin`) | The 47 of the boot manifest and `gt_hard.anm` (counted on the generated tree, 2026-10-28; this file's own writer covers the 46 object sets, the manifest's Pikmin set is not checked here). Each info is a static identity DCK with the disc's frame count and keys, under invented names (`<id>_<nn>.dck`). |
| `ufoparts/naka1.mod`, `naka4.mod`, `naka7.mod` | One joint, one `cent` sphere. The disc's pellet table loads them by id. |
| `tekis/chappy/chappy.mod` | 17 joints and the disc's collision parts (`cent`, `head`, `body`, `slot`, `lleg`, `rleg`). |
| `parms/pelMgr.bin` | Twelve pellet configs: blue, red and yellow at 1, 5, 10 and 20. The carry numbers are this file's own, not the disc's. |
| `objects/pelAnim.bin` | 23 shape infos: the four pellets, the 16 ship parts the game always loads, and `un01`, `un04`, `un07`. With it the game loads those 20 resident shapes at stage start, as on the disc. |

Choices worth knowing:

- Event keys are dropped. They only produce sound and effect cues
  (`PaniAnimator` maps them to `KEY_PlaySound` and `KEY_PlayEffect`).
- One disc list is not ascending by frame (the Onion's `flowrot`: loop start,
  loop end, then an `Action0` on an earlier frame). `checkConstantKeys` fires such
  a key on the same animate call as the one before it, so the stand-in points it
  at that frame, which the writer accepts and which fires on the same tick.
- The disc's `naka1` table names three animations its bundle lacks (the game opens
  loose `.dck` files that do not exist and falls back to a dummy). The stand-in
  bundle has all of them. The three loose paths stay in the manifest allowlist.
- Sets whose model is not written (plants, gates, bosses, the other enemies) get a
  flat joint list (root, then every joint under joint 0) of the disc's joint
  count. The game stores the parents and never reads them back.

Tests: `nectar_standin_objects_test` (`libs/nectar_assets/tests/standin_objects_test.cpp`)
reads every table back through the game's `Parameters`, `AnimMgr::Parms` and key
types, walks every bundle through a transcription of `StdSystem::loadBundle` and
`AnimDck::read`, parses the models with `nectar::parseMod` and the collision text
through the game's `CmdStream`, reads the pellet tables with Parm fields declared
as in `PelletConfig` and `PelletAnimInfo`, and compares the file lists with the
boot manifest.

## World models and parameter files (`standin_world.cpp`, `standin_world_data.cpp`)

Models for what had animations and no model, so a generator can place any of them
without aborting. Colour-only boxes (`buildStandInModel`) on the disc's joints,
with a collinfo text built from the disc's parts (`worldCollInfo`):

| Group | Files |
|-------|-------|
| Plants | `objects/plants/{chidome,clover,hae,hutaba,ine,tanpopo}.mod` |
| Bridges and the push box | `objects/bridge/brd_test.mod` (37 joints), `brd_long.mod` (91), `objects/hinderrock/cube10.mod` |
| Soft gate | `objects/gate/gt_soft.mod` (4 joints; parts `gate`, `side`, `flag`, `flg1`-`flg3`) and `gt_soft.anm` |
| Bosses | `bosses/{mizu,snake}/*.mod`, `parms.bin` |
| Enemies | `tekis/{beatle,iwagon,kabekuiA,kabekuiB,otama,palm,swallow}/*.mod` |
| Collision platforms | `tekis/beatle/p_{body,l_wing,nozzle,r_wing}.mod`, `tekis/swallow/swalcol{b,h}.mod`: a box whose triangles are also the collision (`writePlatformMod`) |
| Enemy parameters | `tekipara/*.bin`, 31 files |

Notes:

- The gate's `gate.bin`, `gt_hard.mod`, `centcoll.mod`, `sidecoll.mod` and
  `gt_hard.anm` are the sandbox gate's (`standin_sandbox.cpp`); the object
  profiles no longer have the two gate sets. `gt_soft.anm` reuses the sandbox
  table's info names and frame count with the soft gate's four joints.
- `standInObjectModelSpecs()` now includes these, so the animation sets take their
  parents from the models.
- With `tekipara/chappy.bin` the Bulborb of `sandbox_objects` runs on the disc's
  parameters, not the code defaults.

The every-object stage `stages/sandbox_all_objects.ini`
(`tools/scenarios/standin_all_objects_stage.cpp`, a flat floor, a third hidden
`stages.ini` entry) places, from `default.gen`: the soft gate (`sluice soft`), six
plants (clover, chidome, hutaba, ine, tanpopo, hae), two bridges and a push box
(`work` objects `bridge test` shapes `bridge 4` and `bridge 13`, `move stone`
shape `stone 10`), the snake and mizu bosses and eight enemies (iwagon, chappy,
swallow, palm, beatle, kabekuiA, kabekuiB, otama). `scenario_objects_smoke_30` and
`_60` boot it for 300 ticks and check the eight enemies were born; every model,
platform, table and parameter file loads or the process aborts.

New generator kinds in `ObjectGenSpec`: `Plant` (`plnt`), `Work` (`work` v0.3) and
`Boss` (`boss` v2).

## The generators

`nectar::writeObjectGen` / `writeObjectGenFile` (same file) serialize a `.gen`
generator: an `item` (the Onion is `goal`, parameter `p00` its colour), a `pelt`, a
`piki` (a buried sprout with `pikiState` 0) or a `teki` (version 10, a
`TekiPersonality` with the code defaults). Area is a point, type is one thing. The
test reads the bytes with a transcription of `Generator::read` and the part reads
over the game's `RamStream` and `Parameters`, and with `nectar::parseGen`.

An Onion the player does not own is only born by a generator named `next`
(`GenObjectItem::birth` checks `playerState->hasContainer`), which is what the
stage uses.

## The object sandbox (`stages/sandbox_objects.ini`)

`tools/scenarios/standin_objects_stage.cpp`. A second hidden entry in
`stages.ini` (also `STAGE_TESTMAP`). `stages/sandbox.ini`, its map and its
generators are untouched, so every existing scenario keeps its numbers (the
`throw_release` calibration K stays 15 at 30 Hz and 18 at 60 Hz).

- Map: a 1200 by 1200 heightfield with 50-unit cells. Points from x = 100 are 200
  units higher, so across the one cell from x = 50 to x = 100 the ground rises
  200 over 50 units (about 76 degrees, face normal y about 0.24). Flat floor and
  plateau.
- `default.gen`: a blue Onion at (-120, -250), three buried sprouts (blue, red,
  yellow), a `pr01` pellet and a Dwarf Bulborb (chappy) at (-330, 300). The
  captain starts at the origin.

Scenarios opt in with `PIKMIN_SCENARIO_STAGE=stages/sandbox_objects.ini`. The CTest
entries `scenario_sprout_landing_30` and `_60` do.

## sprout_landing (upstream #208)

`pc_port/scenarios/scenario_sprout_landing.cpp`. Asserts the stage's content, then
launches six seeds the way `GoalAI::BootEmit` launches them, from the cliff's
foot toward the face, and follows each through Flying, Bounce and Bury to Wait. It
asserts only what holds with or without #208 (written before it merged): every seed plants within 30 s, none
on a steep face (ground normal y at least 0.5), none below the ground, none more
than 30 units above it. The height and speed at the start of Bury, which #208
changes at 30 Hz, are printed per seed, not asserted.

Observed (macOS, both rates): all six seeds first bury at x = 37, about 13 units
short of the face, and plant on the floor with gap 0; the push off a steep face
never runs. On this 76 degree face the seeds bounce off the wall; the case of
upstream issue #65 (blue Pikmin planted in the air; it is an issue, the fix's PR number is not recorded here) (a seed landing on an edge whose collision normal reads as ground while the
face normal reads as wall) needs a surface built for it. At 30 Hz the branch still
reaches Bury on the floor (gap 0.0). Upstream #208 (keep the GameCube sprout
landing at 30 FPS) was merged upstream on 2026-10-28 and is in this tree. With it
the printed speed at the start of Bury is 1.8 to 3.1 instead of 0.0 (30 Hz, re-run
2026-10-28); the height gap is still 0.0 because the seeds first bury on the
floor, so this stage cannot show the mid-air case.

## What remains

- A surface that makes a seed land on a steep edge (the real #65 case).
- The sprout is launched by the scenario, not by the Onion. Making the Onion shoot
  its own seeds needs its enter animation and a container state (`aiEnter`).
- Enemy, boss, plant and bridge behaviour is not asserted: they are placed and
  loaded and survive 300 ticks, nothing more.
- Models exist only for the objects in the boot manifest; the 23 other enemies, the
  other bosses, the other plants and the slopes have none.
- The plants' light-flare groups (`hae`) are not stand-ins.
- Event keys (sounds, effects) are not stand-ins.
- Disc-only checks: a `_disc` variant of `sprout_landing` does not exist, because
  the stage does not exist on a real install.
