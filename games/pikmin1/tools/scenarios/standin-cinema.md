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

No actors, so no model or animation is opened. Every cut plays once (demo36 and demo73 end in a loop), so a movie
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
| `demo04.cin` + 1 scene (330) | Red Onion discovery (`DEMOID_FindRedOnyon`; the sandbox translates `DEMOID_FindOnyon` to it) | Localized, HideNavi, NaviNoAI | red Onion Boot at 10, EmitCone at 300; `TextDemo` `TUT_RedOnion` (1) at 310 |
| `demo06.cin` + 1 scene (100) | Blue Onion discovery (`DEMOID_FindBlueOnyon`, started untranslated by `DEMOFLAG_DiscoverBlueOnyon`); used by `onion_skip` | Localized, HideNavi, NaviNoAI | blue Onion Boot at 10, EmitCone at 90; no text key (its screen, `tu_tx22`, is not a stand-in) |
| `demo05.cin` + 1 scene (300) | Yellow Onion discovery (movie 5, started by `DEMOFLAG_DiscoverYellowOnyon`); used by `onion_day3` | Localized, HideNavi, NaviNoAI | yellow Onion Boot at 10, EmitCone at 270; no text key (the disc's demo05 has none either, per upstream #205's audit). The yellow boot motion keeps the disc's numbers (360 frames, `KEY_Action0` at 240), so the seed comes out at movie frame 250, leaving the three skip windows |
| `demo46.cin` + 1 scene (60) | Pikmin extinction (`DEMOID_Extinction`), Localized at the captain | Localized, HideNavi, NaviNoAI | FadeOut at 40; `GameEndCondition` (PikminZero) and `CleanupDayEnd` at 57 |
| `demo47.cin` + 1 scene (60) | Day end after an extinction (`DEMOID_ExtDayEnd`, translated per stage; the sandbox falls back to Impact Site, demo47) | HideNavi, NaviNoAI | FadeIn at 0, FadeOut at 50, `ForceResults` at 57 |
| `demo36.cin` + 2 scenes (30 + 10) | End-of-day results backdrop (`DEMOID_EndOfDayResults`, started by `DayOverModeState::initialisePhaseTwo` with the results window); used by `captain_death` | NonGameMovie | none. The first cut plays once, the second loops (`CINMODE_LoopScene`) until `exitDayEnd` drops the movie, as the disc's does (a 420-frame cut and a 10-frame loop). The results window stays up only while a movie is active: with no demo36 the phases advance on the next ticks and the day leaves for `GameExit` |
| `demo73.cin` + 2 scenes (30 + 10) | Bad ending, Olimar fails to escape (`DEMOID_BadEndingFailEscape`, untranslated, started by `DayOverModeState::initialisePhaseTwo` on Day 30 with fewer than 25 parts); used by `extinction_day30` | NonGameMovie | `StartTotalResults` (12) at 27 in the first cut, which plays once; the second loops until the final results window reports done and `DayOverModeState::update` skips the scene. The disc's (read-only, 2026-10-28): NonGameMovie, a 449-frame cut with the key at 446, then a 4-frame loop, with the results backdrop and ship actors. Without it the window is built but never started and the phases run on |

The discovery key frames follow the Onion's boot motion. Boot starts the motion
(info colour + 3), and its `KEY_Action0` emits the first seed. Red keeps the
disc's motion (472 frames, key at 270), kept in the stand-in Onion table
(`standin_object_profiles.cpp`, `standin-objects.md`), so its seed comes out at
movie frame 280, before EmitCone at 300.

Blue is shortened on purpose. The disc's blue boot motion has 360 frames with
the key at 240, which made `onion_skip` run about 965 ticks at 30 Hz. The blue
stand-in motion (goal info 3) is our own: 100 frames, loop key 0, `KEY_Action0`
at 60, last key 99. The yellow info (5), which onion_day3 uses as it is, and the
Onion's other infos keep the disc's numbers, so `sprout_landing` and
`objects_smoke` are not affected. The seed comes out at movie frame 70, before
EmitCone at 90, and the movie is 100 frames: Boot, seed, EmitCone, the same
order as on the disc. This leaves three skip windows: before Boot, between Boot
and the seed, and between the seed and EmitCone.

On `stages/sandbox.ini` only `demo60` plays: the sandbox plays no landing. The runner's
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
  writer test. `nectar_blo_writer_test` now expects 22 layouts (12 on the boot
  path, 8 for `day30_clock`, 2 for the text keys).
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

- With `onion_skip` added on the merged tree (`aceb6995` plus this work), all
  55 non-disc tests pass and the disc scenarios pass 6 of 6. `throw_release`
  K is 16 / 18, the same as the merged tree before `onion_skip` was added. The
  merged tree's gate re-fit, not the cinemas, set those values
  (`standin-k-investigation.md`).

## Allowlist changes

- `standin-open-allowlist.txt`: `dataDir/cinemas/demo60.cin` removed.
- `kManifestAllowlist` in `standin_generator.cpp`: the `dataDir/cinemas/`
  prefix removed. Every cinema path that the disc boot opens is now in the tree.

## onion_skip: the Onion discovery skip (upstream #205)

`pc_port/scenarios/scenario_onion_skip.cpp`, CTest `scenario_onion_skip_30` and
`_60`. Stand-in only. Runs on the object stage (`stages/sandbox_objects.ini`,
`standin-objects.md`), whose `default.gen` places a blue Onion the player does
not own yet, in `GOAL_BootInit`.

What it needs, and where each part comes from:

- **Onion and seed animations:** the object stand-ins (`goal.anm` and
  `anims.bin`, the `pikihead` set).
- **Route:** `GoalItem::startAI` runs on the object stage without trouble, so
  no extra waypoint data was needed.
- **Trigger:** `DemoInvoker` starts the discovery only on story stages, so the
  scenario calls `setFlag(DEMOFLAG_DiscoverBlueOnyon, onion)` itself. That
  plays movie 6, the stand-in `demo06.cin`, targeted and localized at the
  Onion, as `DemoInvoker` would.

There are four cases in one run. Between cases the scenario puts the Onion back
to undiscovered: it clears the boot bit, resets the demo flag, and repeats the
`!hasBootContainer` branch of `GoalItem::startAI` (speed 0, `GOAL_BootInit`,
cone shrink, colour animation) without creating the leg ropes again. Seeds are
counted with `GameStat::bornPikis[blue]`, which only `GoalAI::BootEmit`
increments.

| Case | START at movie frame | Asserted |
|------|----------------------|----------|
| `watch` | none | 1 seed |
| `pre_boot` | 5 (before Boot) | 1 seed |
| `mid_boot` | 40 (after Boot, before the seed) | 1 seed (the #205 regression test) |
| `post_seed` | 80 (after the seed, before EmitCone) | 1 seed |

Every case also asserts the following:

- The movie starts within 2 s of the flag.
- The movie ends within 20 s.
- The boot bit is set.
- The Onion reaches `GOAL_Wait` within 10 s of the movie's end.
- No second seed comes out in the 1 s after that.

`mid_boot` is the #205 window. Without the fix, the replayed EmitCone snaps the
Onion from `BootInit` to `Wait` before `finishOnionBootAfterSkip` runs, so no
seed is born. With the fix, EmitCone first moves the Onion to `BootEmit`, so one
seed is born. The fix is in this tree through the kyle-develop merge (upstream
PR #205 is still open, checked 2026-10-28), so the case asserts exactly one
seed; a count of zero fails with a message that names the #205 bug.

### Observed (macOS, 2026-10-28)

Before the #205 fix was in the tree, both rates (`mid_boot` was then printed,
not asserted):

```
onion_skip watch: movie ran 330 ticks; seeds 1; Onion Wait; boot bit 1          (660 at 60 Hz)
onion_skip pre_boot: START at movie frame 5 (Onion BootInit, 0 seed(s) so far); seeds 1; Onion Wait
onion_skip mid_boot: START at movie frame 100 (Onion BootInit, 0 seed(s) so far); seeds 0; Onion Wait
onion_skip mid_boot: no seed: the upstream #205 bug (...)
onion_skip post_seed: START at movie frame 275 (Onion BootEmit, 1 seed(s) so far); seeds 1; Onion Wait
PASS onion_skip
```

With the game-code part of PR #205 applied locally (`cinePlayer.cpp`,
`demoEvent.cpp`, `goalItem.cpp`, `pc_onion_skip.h`, `pc_cine_skip_order.h`;
a one-off check, reverted and not committed), both rates pass. The other
three lines are unchanged, and `mid_boot` prints:

```
onion_skip mid_boot: START at movie frame 100 (Onion BootInit, 0 seed(s) so far); seeds 1; Onion Wait
onion_skip mid_boot: one seed: the upstream #205 behaviour (...)
```

So the scenario told the two builds apart cleanly.

With the #205 fix merged into kyle-develop (rebuild v1), both rates pass with
exactly one seed in every case, and `mid_boot` now asserts it (re-run on
2026-10-28, the `movie ran N ticks` text is now printed for every case):

```
onion_skip watch: movie ran 330 ticks; seeds 1; Onion Wait; boot bit 1          (660 at 60 Hz)
onion_skip pre_boot: START at movie frame 5 (Onion BootInit, 0 seed(s) so far); seeds 1; Onion Wait
onion_skip mid_boot: START at movie frame 100 (Onion BootInit, 0 seed(s) so far); seeds 1; Onion Wait
onion_skip post_seed: START at movie frame 275 (Onion BootEmit, 1 seed(s) so far); seeds 1; Onion Wait
PASS onion_skip
```

Not covered:

- What #205 also restores on a skip: the cone size, the colour fade and the
  Onion's route waypoint flag.
- The frame-order sort of the skip replay. #205 tests it in
  `pc_cine_skip_order_test`.
- The red and yellow discovery movies.

### Length after the blue motion was shortened (macOS, 2026-10-28)

Total ticks of one `onion_skip` run, from its `ticks in all` line:

| Rate | Before (330-frame movie, 360-frame motion, 2 s settle) | After (100-frame movie, 100-frame motion, 1 s settle) |
|------|------|------|
| 30 Hz | 965 | 368 |
| 60 Hz | 1912 | 719 |

The movie lines are now `watch` 100 ticks (200 at 60 Hz), `pre_boot` 6,
`mid_boot` 41 and `post_seed` 81 (twice that at 60 Hz). Every case still
reports exactly one seed.

## Extinction chain: demo46 and demo47 (G1, 2026-10-28)

`pc_port/scenarios/scenario_extinction.cpp`, CTest `scenario_extinction_30` and
`_60`. Stand-in only (the sandbox, Day 2). The files the chain opens, and why
the movies need their Notify keys:

- `GameCoreSection::update` puts the captain in `NAVISTATE_PikiZero` when
  `GameStat::allPikis == 0` and `maxPikis > 0`. `NaviPikiZeroState::exec`
  counts 30 ticks and sends `MOVIECMD_GameEndCondition` (PikminZero).
- The first message sets `mGameEndFlag` and starts `DEMOID_Extinction`
  (`cinemas/demo46.cin`). The disc's demo46 sends the second message itself, from
  a key at frame 167 (with FadeOut at 150 and CleanupDayEnd at 167). The second
  message starts `DEMOID_ExtDayEnd` and sets the clock to the end hour.
- `DEMOID_ExtDayEnd` is translated per stage (`movie47table`,
  `moviePlayer.cpp`): demo47 Impact Site (and the sandbox), demo48 Forest of
  Hope, demo49 Cave and Last, demo50 Yakushima. The disc's demo47 and demo48
  differ only in flags (68 / 69), scene and model files. Each has a
  `ForceResults` key at frame 397 that clears the end flag, which lets
  `MessageModeState` queue `DayOverModeState`.
- A missing `.cin` is tolerated (the movie finishes in one tick and drops its
  keys), which is why the chain looped: the second message came only from the
  first-message replay, 30 ticks apart, and `ForceResults` never ran.

The stand-ins keep the disc's keys and key order on invented, shorter frames
(60-frame scenes). The disc movies' actor (`nv3Model`, the Pikmin used for the
death animation) and the ship Actions in demo47 (sender 1, events 5 and 6) are
not written. The `.cin` files for the other three stages (demo48 to demo50) are
not written: no stand-in stage translates to them.

Files the chain opens besides the two movies and their two scenes: none that
were missing. `gameover.blo` and `tu_tx20.blo` already exist. The scenario passes
at `DayOverModeState`'s construction, before phase one opens the results
screens.

## Day-end screens: day_end and extinction_day30 (2026-10-28)

`scenario_day_end.cpp` (`day_end`, `day_end_day29`) and
`scenario_extinction_day30.cpp`. Stand-in only. What each chain opens that the
tree did not have, and what was written for it (`standin_ui.cpp` and
`standin_cinema.cpp`, both through the existing writers, `blo.h` and
`cinema.h`):

- The diary page of an ordinary sunset. `ResultFlags::getDocument` picks the
  flag of lowest priority that is on: with nothing else on, `RESFLAG_Misc1`
  (`re_n1_00.blo`, priority 600) on Day 2, and on Day 29 `RESFLAG_FinalDay`
  (`re_g_00.blo`, priority 13), which `cleanupDayEnd` sets when the next day is
  the last. One page each. Written like `re_j_00`: one `tx00` text box. The
  disc's pages hold `tx00` to `tx02`, all optional to the code.
- The bad ending's final results window (`DayOverModeState::makeTotalScoreWindow`,
  built in phase two on Day 30). Its constructors open four screens:
  `ufo_p.blo` (`DrawUfoParts`; `dataSet` looks up `ui01`-`ui30` pictures and
  `un01`-`un30` text boxes once the window is operated), `f_result.blo`
  (`DrawTotalScore`; every `search(.., true)` of its constructor: `win1`-`win4`,
  the digit pictures `pa_l` ... `tp_1`, `pd_s` and `newd` pictures, the record rows
  `pd01`-`dp05` and four small text boxes, a `best` anchor with one `b_00`),
  `cha_sav2.blo` and `ac_chui.blo` (`DrawSaveMes`, inside the options save step).
  Every tag and type the code asks for is also on the disc's file
  (`NECTAR_DISC_DATA` run of `nectar_blo_writer_test`: 45 layouts, 0 problems).
  The record-row texts are checked to fit the 128-byte buffers they are copied to.
- `demo73.cin` (table above). The disc's demo73 starts the window with its
  `StartTotalResults` key; the stand-in keeps that key and the loop. With it the
  run goes through the scores, the parts list and the options save (A every 6
  ticks), about 14 s at 30 Hz, then phase three and four. Without it (a run
  before demo73 was written) the chain still reached `GameExit`, at tick 99
  instead of 517 at 30 Hz, with the window never shown.

Left missing on purpose, each a zero-length movie whose keys no chain step needs
(allowlisted in `standin-open-allowlist.txt`): demo28 (the march-in, the
sandbox stages' `DEMOID_DayEnd`; the disc's has one Notify key, FadeIn at 0),
demo32 and demo56 (take-off), demo84 (red Onion flight) and demo69 (the bad
ending's first movie; the disc's has Action keys only). demo74 (Olimin) is not
opened: `MoviePlayer::startMovie` returns before it when the stage has no red
Onion.
