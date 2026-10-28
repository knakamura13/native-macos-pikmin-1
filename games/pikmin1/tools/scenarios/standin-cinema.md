# Stand-in cinemas

Cutscene support for the disc-free scenario tree. `nectar_standin_generator`
now writes the movies the scenario boot opens. The game's real movie code
(`MoviePlayer`, `CinematicPlayer`, `SceneData`, `DemoEventMgr`) runs on them,
including the key events and the skip replay.

## Formats

Both formats are CmdStream text: tokens split on whitespace and commas. A token
that starts with `#` or `//` skips the rest of the line, but only where a token
starts. Inside a `{ }` section the loaders test for `}` before they skip
comments, so a comment there is read as tokens. The writers emit comments only
before the first command. CmdStream reads into one shared 0x8000-byte buffer.
Every stand-in file stays under 0x4000 bytes, so it is read in the first fill.

### `.cin`: the movie script

Read by `CinematicPlayer::loadCin` (`plugPikiColin/cinePlayer.cpp`). Path from
`movieTitles` in `moviePlayer.cpp`, after `translateIndex`. A stage with no `id`
(the sandbox, `STAGE_TESTMAP`) translates as Impact Site.

| Command | Meaning |
|---------|---------|
| `type N` | Read, never used. |
| `flags N` | `CinePlayerFlags` (`CinematicPlayer.h`). For example, Localized places the movie at the target's position, HideNavi hides the captain, NaviNoAI stops the captain's AI, and PauseAll pauses the game. |
| `addScene { scene <path.dsk> }` | One per scene, in index order. |
| `addActor { shape <mod> bundle <anm> anims <bin> }` | Loads a model through `gameflow.loadShape` and an AnimMgr. |
| `addCut { cut <scene> <start> <end> flags F actor <mod> acflags A anim S C keys N { type cmd value frame ... } }` | Plays scene frames `start` to `end`. The playback mode is `(F >> 1) & 7`: 3 plays once, 5 loops until skipped. |

Key `type 0` (Notify) calls `gameInterface->message(cmd, value)` with a
`MOVIECMD_*` (`Interface.h`). `cmd 0` is `TextDemo`: it opens the tutorial
text whose number is `value` (`ogTutorial.h`). Key `type 1` (Action) calls
`demoEventMgr->act(cmd, value)`. Here `cmd` is the sender and `value` is the
event (`demoEvent.cpp`, for example sender 3 = red Onion, event 5 = Boot).
`frame` is a scene frame. A key fires when playback passes that frame. A skip
replays the remaining keys, except `TextDemo` (`CinematicPlayer::skipScene`).

What crashes the loader, and what the writer therefore refuses:

- A cut when there is no scene. `addSceneCut` reads `findScene(0)->mNumFrames`.
- A cut actor that is not in the actor list.
- `acflags` or `anim` with no actor before it.

### `.dsk`: one scene's camera and lights

Read by `SceneData::parse` and `getAnimInfo` (`sysCommon/shapeBase.cpp`).

| Section | Content |
|---------|---------|
| `<SCENE_KEY_ANM_INFO> { numframes N  numcameras C  numDifLights L }` | Other lines are skipped one line at a time. Must come before the tables. |
| `<KEY_CAMERA_ANM> { size N  float v v v v v v v v  float ... }` | `DataChunk::getData`: a `float` word before every run of 8 values. Size 0 would swallow the closing brace, so an empty chunk is left out. |
| `<KEY_CAMERA_TABLE> { index i  name n  cam_pos_x E O F ... }` | One per camera. The fields are `cam_pos_xyz`, `cam_lat_xyz` (look-at), `cam_twist`, `cam_fovy`, `cam_near`, `cam_far`. |
| `<KEY_DIFFUSE_LIGHT_ANM>`, `<KEY_DIFFUSE_LIGHT_TABLE>` | The same for lights: `light_pos_xyz`, `light_r/g/b_param` (0 to 255), `light_visible` (on only at exactly 1.0). |

A track is `entries offset flags`. If entries is 0, the game uses its default:
0 for position, 1.0 for fovy (so the writer requires fovy). If entries is 1,
the value is `data[offset]`. If entries is 2 or more, each key is three floats
(frame, value, tangent), read as a Hermite curve (`extract`). Every movie with a
scene needs a camera, because `MovieInfo::setCamera` reads `mCameraData[0]`.

## Writers

`libs/nectar_assets/include/nectar_assets/cinema.h`:

- `writeCin(const Cinema&, std::string&, std::string& error)`
- `writeDsk(const DskScene&, std::string&, std::string& error)`
- `writeStandInCinemas(std::vector<StandInCinemaFile>&, std::string& error)`

These are plain input structs with text output and no file I/O. The generator
calls `writeStandInCinemas` from `standInCinemas()` in `buildTree()`.

Test: `nectar_cinema_writer_test <boot-manifest.txt>`. It reads the output back
with the real `CmdStream`, through transcribed copies of `loadCin`,
`SceneData::parse`, `getAnimInfo` and `DataChunk::getData`. Those cannot link
into a host test. Drift note: update the copies if those functions change. The
test checks round trips, the refusals above, determinism, the key events listed
below, and that every `cinemas/` path in the boot manifest is a stand-in file.

## Stand-in set

No actors, so no model or animation is opened. Every cut plays once, so a movie
ends without a skip. Each scene is a short dolly towards the origin (or towards
the target, for a Localized movie), 30 degrees, with one white light. The
cameras, lights, lengths and frames are invented. The `.dsk` paths of the disc
boot are the ones listed in `boot-manifest.txt`.

| File | Movie | Flags | Keys |
|------|-------|-------|------|
| `demo60.cin` + 2 scenes (60 + 30 frames) | Post-extinction bonus seeds (`DEMOID_Unk60Cat`, played by the sandbox from `finalSetup` on Day 1) | HideNavi, NaviNoAI | BonusPiki from the blue, red and yellow Onions at 10; `TextDemo` `TUT_Zenmetu` (13) at 50 |
| `demo61.cin` + 2 scenes | The same for Forest of Hope (opened on the disc boot) | same | same |
| `demo41.cin` + `demo40/ca_d_m01.dsk` (90) | Forest of Hope landing (`DEMOID_LandingForest`) | HideNavi, NaviNoAI | yellow Onion Land at 10, EmitCone at 70 |
| `demo45.cin` + 2 scenes (40 + 40) | Pikmin into the Onion, Forest of Hope | HideNavi, NaviNoAI | none |
| `demo04.cin` + 1 scene (120) | Red Onion discovery (`DEMOID_FindRedOnyon`; the sandbox translates `DEMOID_FindOnyon` to it) | Localized, HideNavi, NaviNoAI | red Onion Boot at 10, EmitCone at 80; `TextDemo` `TUT_RedOnion` (1) at 100 |

On stand-ins only `demo60` plays: the sandbox plays no landing. The runner's
intro gate presses START on the first movie tick. The skip replays the
BonusPiki keys, which do nothing because the player has no Onion. The text key
is not replayed. `demo41`, `demo45` and `demo61` exist so that the generator's
manifest check passes with no `cinemas/` allowlist entry.

The text keys need their screens: `TUT_Zenmetu` opens `tu_tx20.blo` and
`TUT_RedOnion` opens `tu_tx03.blo` (`ogTutorialData.cpp`). A missing screen
panics (`Cannot open file`). `standin_ui.cpp` therefore writes both as one-box
tutorial pages, like `tu_tx00`.

## Results (macOS, 2026-10-28)

- `nectar_cinema_writer_test` passes, along with every other `nectar_*`
  writer test. `nectar_blo_writer_test` now expects 22 layouts.
- Stand-in scenarios: 7 of 7 pass. `throw_release` K is unchanged (15 at
  30 Hz, 18 at 60 Hz). The intro now ends after 15 ticks; it was 92 at 30 Hz
  and 182 at 60 Hz. The START pulse now skips a real movie, and
  `MoviePlayer::skipScene` moves the captain from Starting to Walk, as it
  does on the disc (45 ticks there: landing, demo61, text). Before this
  change, the missing demo60 ended before the first pulse, so the captain
  walked out of the ship on his own.
- Disc scenarios: 6 of 6 pass. The disc reads its own cinemas; nothing here
  changes game code.
- Watched-to-end check (a one-off local experiment with the START pulse off,
  not committed): `demo60` played through both scenes. The text key opened
  `tu_tx20`, the gate's A closed it, the intro ended after 189 ticks, and
  `boot_smoke` passed. Before `tu_tx20` was added, the same run panicked on
  the missing screen.

## Allowlist changes

- `standin-open-allowlist.txt`: `dataDir/cinemas/demo60.cin` removed.
- `kManifestAllowlist` in `standin_generator.cpp`: the `dataDir/cinemas/`
  prefix removed. Every cinema path that the disc boot opens is now in the tree.

## Onion discovery skip scenario (upstream #205): not added

Upstream PR #205 is open and is not on this branch
(`pc_cine_skip_order.h` is absent, and `EmitCone`'s skip branch still calls
`pcSnapLanded()` without `finishOnionBootAfterSkip()` first). So on this branch
the window between Boot and the seed still has the bug, and a regression
scenario for it would fail until the fix is merged.

The sandbox also cannot host it yet with stand-in assets:

1. **Onion animations.** `objects/goal/goal.anm` and `anims.bin` are missing
   (an allowlisted #66 gap). `GOAL_BootInit` goes to `GOAL_BootEmit` only on the
   boot motion's Action0 key (`itemAI.cpp:730`), so without the animations a
   watched discovery never emits its seed. The "skip after the seed" window and
   the watched-to-end control case cannot be reached.
2. **Seed.** `BootEmit` emits a sprout. The sprout's animations
   (`objects/pikihead/`) are another allowlisted #66 gap.
3. **Route.** `GoalItem::startAI` calls `routeMgr->getWayPoint('test',
   mWaypointIdx)->setFlag(...)` without a null check. The Onion needs a waypoint
   in the sandbox route. This is untested on the sandbox map.
4. **Trigger.** `DemoInvoker` (`demoInvoker.cpp:39`) starts the discovery only
   on the story stages. On `STAGE_TESTMAP` the scenario must call
   `playerState->mDemoFlags.setFlag(DEMOFLAG_DiscoverRedOnyon, onion)` itself.
   Movie 4 is `DEMOID_FindRedOnyon`; the stand-in `demo04.cin` is written for it.

### Design, for when items 1 to 3 exist

`scenario_onion_skip_seed`, Day 1, sandbox:

- `onStageReady`: `itemMgr->birth(OBJTYPE_Goal)` as red, placed about 300 units
  in front of the captain, not booted (`hasBootContainer(Red)` false). Then
  `startAI`, and `setFlag(DEMOFLAG_DiscoverRedOnyon, onion)`.
- Cases, each a fresh run or a reset Onion. Each case presses START at a chosen
  movie frame (`MoviePlayer::mCurrentFrame`), using the stand-in `demo04` key
  frames:
  - before Boot (frame < 10);
  - after Boot, before the seed (frame 10 to 80, Onion in `GOAL_BootInit`);
    this is the #205 bug window;
  - after EmitCone (frame > 80);
  - with the animations: after the seed, and watched to the end as the control.
- Assert in every case: exactly one seed emitted (count `PikiHeadItem`s, or
  `GameStat` seed counters, before and after), the Onion in `GOAL_Wait`,
  `hasBootContainer(Red)`, the cone open, and the Onion's waypoint flag on.
- The bug-window case is expected to fail until #205 merges. It goes in as a
  separate test entry with `WILL_FAIL` (or with the fix) so that the suite
  stays green.
