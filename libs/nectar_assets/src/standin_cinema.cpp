// Stand-in cinemas for the disc-free scenario tests (cinema.h,
// games/pikmin1/tools/scenarios/standin-cinema.md).
//
// Fidelity (#62): nothing here comes from a disc cinema. The cameras, lights,
// lengths and frames are invented; there are no actors (no model, bundle or
// animation table is opened). What matches the game is only what its code
// defines: the movie that each path is (moviePlayer.cpp movieTitles and the
// translation tables), the CinePlayerFlags, and the key events the movie's
// game logic needs, by their code enums (demoEvent.cpp, Interface.h,
// ogTutorial.h). The file paths of the .dsk scenes the disc boot opens are the
// ones in boot-manifest.txt, so the manifest check needs no cinema allowlist.
#include "nectar_assets/cinema.h"

namespace nectar {
namespace {

// zen::ogScrTutorialMgr::EnumTutorial (ogTutorial.h).
constexpr int kTutRedOnion = 1;  ///< TUT_RedOnion
constexpr int kTutZenmetu  = 13; ///< TUT_Zenmetu, the post-extinction text

/// A slow dolly from behind and above the origin towards it, looking at the
/// origin, 30 degrees, one white light overhead. `frames` long.
DskScene standInScene(int frames)
{
	const float end = float(frames);
	DskScene s;
	s.numFrames = frames;
	DskCamera c;
	c.name = "standin_cam";
	c.posX = { { 0, -120, 0 }, { end, 120, 0 } };
	c.posY = { { 0, 420, 0 }, { end, 360, 0 } };
	c.posZ = { { 0, 640, 0 }, { end, 560, 0 } };
	c.latX = { { 0, 0, 0 } };
	c.latY = { { 0, 20, 0 } };
	c.latZ = { { 0, 0, 0 } };
	c.fovy = { { 0, 30, 0 } };
	s.cameras.push_back(c);
	DskLight l;
	l.name = "standin_light";
	l.posX = { { 0, 0, 0 } };
	l.posY = { { 0, 800, 0 } };
	l.posZ = { { 0, 200, 0 } };
	l.r    = { { 0, 255, 0 } };
	l.g    = { { 0, 255, 0 } };
	l.b    = { { 0, 255, 0 } };
	s.lights.push_back(l);
	return s;
}

CinKey action(int sender, int event, int frame) { return { kCinEventAction, sender, event, frame }; }
CinKey notify(int cmd, int value, int frame) { return { kCinEventNotify, cmd, value, frame }; }

struct Movie {
	const char* cin;
	Cinema cinema;
	std::vector<int> sceneFrames; ///< one per scene path, the .dsk length
};

std::vector<Movie> movies()
{
	std::vector<Movie> list;
	const std::uint32_t hideNavi = kCinHideNavi | kCinNaviNoAI;

	// Bonus seeds after an extinction (DEMOID_Unk60Cat). demo60 is what the
	// sandbox (STAGE_TESTMAP, translated as Impact Site) plays from finalSetup
	// on Day 1; demo61 is the Forest of Hope's. BonusPiki from each Onion (the
	// game emits only for an Onion the player has and with no Pikmin of its
	// colour), then the extinction text.
	for (const char* id : { "60", "61" }) {
		Movie m;
		m.cin            = id[1] == '0' ? "dataDir/cinemas/demo60.cin" : "dataDir/cinemas/demo61.cin";
		m.cinema.flags   = hideNavi;
		m.cinema.scenes  = { std::string("cinemas/demo") + id + "/dm_" + id + "_1.dsk", std::string("cinemas/demo") + id + "/dm_" + id + "_2.dsk" };
		m.sceneFrames    = { 60, 30 };
		CinCut first;
		first.scene    = 0;
		first.endFrame = 60;
		first.keys     = { action(kDemoSenderBluOnion, kDemoOnionBonusPiki, 10), action(kDemoSenderRedOnion, kDemoOnionBonusPiki, 10),
                           action(kDemoSenderYelOnion, kDemoOnionBonusPiki, 10), notify(kMovieCmdTextDemo, kTutZenmetu, 50) };
		CinCut second;
		second.scene    = 1;
		second.endFrame = 30;
		m.cinema.cuts   = { first, second };
		list.push_back(m);
	}

	// Forest of Hope landing (DEMOID_LandingForest), played on disc only: the
	// sandbox is not a story stage and plays no landing. The Onion keys are the
	// two that the landing's game logic acts on (both guarded by hasContainer).
	{
		Movie m;
		m.cin           = "dataDir/cinemas/demo41.cin";
		m.cinema.flags  = hideNavi;
		m.cinema.scenes = { "cinemas/demo40/ca_d_m01.dsk" };
		m.sceneFrames   = { 90 };
		CinCut cut;
		cut.endFrame  = 90;
		cut.keys      = { action(kDemoSenderYelOnion, kDemoOnionLand, 10), action(kDemoSenderYelOnion, kDemoOnionEmitCone, 70) };
		m.cinema.cuts = { cut };
		list.push_back(m);
	}

	// Pikmin into the Onion, Forest of Hope (DEMOID_PikminInOnyonForest): no keys.
	{
		Movie m;
		m.cin           = "dataDir/cinemas/demo45.cin";
		m.cinema.flags  = hideNavi;
		m.cinema.scenes = { "cinemas/demo45/dm_45_1.dsk", "cinemas/demo45/dm_45_2.dsk" };
		m.sceneFrames   = { 40, 40 };
		CinCut a, b;
		a.endFrame    = 40;
		b.scene       = 1;
		b.endFrame    = 40;
		m.cinema.cuts = { a, b };
		list.push_back(m);
	}

	// Onion discovery: red (DEMOID_FindRedOnyon, demo04; the sandbox translates
	// DEMOID_FindOnyon to it) and blue (DEMOID_FindBlueOnyon, demo06, which
	// DEMOFLAG_DiscoverBlueOnyon starts untranslated; the object stage's Onion is
	// blue). Localized at the Onion. Boot at frame kDiscoveryBoot starts the
	// Onion's boot motion (info colour + 3), whose KEY_Action0 emits the first
	// seed: frame 270 (red) of that motion in the stand-in Onion table
	// (standin_object_profiles.cpp, the disc's key frame), or frame 60 (blue),
	// which is our own short blue boot motion (100 frames; the disc's has 360 and
	// the key at 240) so that the onion_skip scenario runs few ticks. So the seed
	// comes out at movie frame 280 (red) or 70 (blue), and EmitCone follows it,
	// as on the disc: Boot, seed, EmitCone. That leaves the three skip windows
	// upstream #205 is about (before Boot, between Boot and the seed, between the
	// seed and EmitCone). demo04 then opens the red Onion text; demo06 has no
	// text key (its screen, tu_tx22, is not a stand-in).
	{
		constexpr int kDiscoveryBoot = 10;
		for (int blue = 0; blue < 2; blue++) {
			const int cone   = blue ? 90 : 300; // EmitCone: after the seed, before the movie's end
			const int frames = blue ? 100 : 330;
			const int sender = blue ? kDemoSenderBluOnion : kDemoSenderRedOnion;
			Movie m;
			m.cin           = blue ? "dataDir/cinemas/demo06.cin" : "dataDir/cinemas/demo04.cin";
			m.cinema.flags  = kCinLocalized | hideNavi;
			m.cinema.scenes = { blue ? "cinemas/demo06/demo06_1.dsk" : "cinemas/demo04/demo04_1.dsk" };
			m.sceneFrames   = { frames };
			CinCut cut;
			cut.endFrame = frames;
			cut.keys     = { action(sender, kDemoOnionBoot, kDiscoveryBoot), action(sender, kDemoOnionEmitCone, cone) };
			if (!blue) cut.keys.push_back(notify(kMovieCmdTextDemo, kTutRedOnion, cone + 10));
			m.cinema.cuts = { cut };
			list.push_back(m);
		}
	}

	// Pikmin extinction (DEMOID_Extinction, demo46), then the day end after it
	// (DEMOID_ExtDayEnd, translated per stage: the sandbox plays demo47, Impact
	// Site's). The keys are the disc movies' (read-only, 2026-10-28) on invented
	// frames; the disc movies have one actor (nv3Model) and an animation, the
	// stand-ins none. demo46 (Localized at the captain): FadeOut, then
	// GameEndCondition (PikminZero) and CleanupDayEnd on one frame. The first
	// GameEndCondition message starts demo46; this second one, with the end flag
	// set, starts the day-end movie and sets the clock to the end hour
	// (newPikiGame.cpp, GameMovieInterface::parse). demo47: FadeIn at 0, FadeOut
	// and ForceResults near the end; ForceResults clears the end flag, which
	// releases MessageModeState into the day end. The disc's demo47 also has two
	// ship Actions (sender 1, events 5 and 6, the take-off); the stand-in leaves
	// them out because it has no ship actor to move.
	{
		Movie ext;
		ext.cin           = "dataDir/cinemas/demo46.cin";
		ext.cinema.flags  = kCinLocalized | hideNavi;
		ext.cinema.scenes = { "cinemas/demo46/dm_46_1.dsk" };
		ext.sceneFrames   = { 60 };
		CinCut cut;
		cut.endFrame    = 60;
		cut.keys        = { notify(kMovieCmdGameEndCondition, kEndCausePikminZero, 57), notify(kMovieCmdFadeOut, 0, 40), notify(kMovieCmdCleanupDayEnd, 0, 57) };
		ext.cinema.cuts = { cut };
		list.push_back(ext);

		Movie end;
		end.cin           = "dataDir/cinemas/demo47.cin";
		end.cinema.flags  = hideNavi;
		end.cinema.scenes = { "cinemas/demo47/de_ptd_00.dsk" };
		end.sceneFrames   = { 60 };
		CinCut endCut;
		endCut.endFrame = 60;
		endCut.keys     = { notify(kMovieCmdFadeIn, 0, 0), notify(kMovieCmdFadeOut, 0, 50), notify(kMovieCmdForceResults, 0, 57) };
		end.cinema.cuts = { endCut };
		list.push_back(end);
	}

	// End-of-day results backdrop (DEMOID_EndOfDayResults, demo36): started by
	// DayOverModeState::initialisePhaseTwo together with the results screen, which stays
	// up as long as the movie is active (DayOverModeState::update waits on
	// mMoviePlayer->mIsActive before phases three and four). The disc's movie plays a
	// 420-frame cut once and then a 10-frame cut in a loop (CINMODE_LoopScene) that runs
	// until exitDayEnd drops it. Same shape here, shorter. A missing demo36 is a
	// zero-length movie: phases three and four then run on the next ticks and the day
	// ends through the title exit (ONEPLAYER_GameExit) with the results screen unread.
	{
		Movie m;
		m.cin           = "dataDir/cinemas/demo36.cin";
		m.cinema.flags  = kCinNonGameMovie;
		m.cinema.scenes = { "cinemas/demo36/ship_t00.dsk", "cinemas/demo36/ship_l00.dsk" };
		m.sceneFrames   = { 30, 10 };
		CinCut once, loop;
		once.endFrame = 30;
		loop.scene    = 1;
		loop.endFrame = 10;
		loop.flags    = kCutLoop;
		m.cinema.cuts = { once, loop };
		list.push_back(m);
	}

	// Bad ending, Olimar fails to escape (DEMOID_BadEndingFailEscape, demo73, untranslated):
	// started by DayOverModeState::initialisePhaseTwo on Day 30 with fewer than 25 parts,
	// right after makeTotalScoreWindow builds the final results window. The window does not
	// start itself: the movie's Notify StartTotalResults key does (GameMovieInterface::parse).
	// The disc's demo73 (read-only, 2026-10-28) is NonGameMovie, a 449-frame cut played once
	// with that key at frame 446, then a 4-frame cut in a loop; DayOverModeState::update skips
	// the scene when the window reports it is done, which ends the loop. Same shape here,
	// shorter, with no actors (the disc's has the results backdrop and the ship). A missing
	// demo73 is a zero-length movie: the window is built but never started, and phases three
	// and four run on the next ticks.
	{
		Movie m;
		m.cin           = "dataDir/cinemas/demo73.cin";
		m.cinema.flags  = kCinNonGameMovie;
		m.cinema.scenes = { "cinemas/demo73/demo73_t.dsk", "cinemas/demo73/demo73_l.dsk" };
		m.sceneFrames   = { 30, 10 };
		CinCut once, loop;
		once.endFrame = 30;
		once.keys     = { notify(kMovieCmdStartTotalResults, 0, 27) };
		loop.scene    = 1;
		loop.endFrame = 10;
		loop.flags    = kCutLoop;
		m.cinema.cuts = { once, loop };
		list.push_back(m);
	}

	// The last ship upgrade (DEMOID_ShipUpgradeLast, demo26, untranslated): queued by
	// PlayerState::preloadHenkaMovie when a delivery brings the 30th part. Its Notify key
	// MOVIECMD_SpecialDayEnd is the happy ending's trigger (GameMovieInterface::parse:
	// forceDayEnd, mIsDayEndTriggered, ENDING_Happy). The disc's demo26 (read-only,
	// 2026-10-28): one 530-frame cut played once with a ship Action (sender 1, event 2) at
	// 180, then SpecialDayEnd at 521 and FadeOut at 519, listed in that order; the movie also
	// shows the field Pikmin (Show/Update flags) near the ship. The stand-in keeps the two
	// Notify keys, their file order and their two-frame gap, on an invented 60-frame scene; the
	// ship Action is left out (no ship on the stand-in stages; DemoEventMgr::act ignores it
	// without one). Used by ending_happy and ending_happy_skip: a START skip replays the keys in
	// frame order (FadeOut, then SpecialDayEnd; pc_cine_skip_order_test has the disc frames).
	{
		Movie m;
		m.cin           = "dataDir/cinemas/demo26.cin";
		m.cinema.flags  = hideNavi;
		m.cinema.scenes = { "cinemas/demo26/demo26_1.dsk" };
		m.sceneFrames   = { 60 };
		CinCut cut;
		cut.endFrame  = 60;
		cut.keys      = { notify(kMovieCmdSpecialDayEnd, 0, 52), notify(kMovieCmdFadeOut, 0, 50) };
		m.cinema.cuts = { cut };
		list.push_back(m);
	}

	// Flying through space (DEMOID_EndingSpace, demo77, untranslated): the neutral ending's
	// phase three and the happy ending's phase four (DayOverModeState), each right after
	// makeTotalScoreWindow. Like demo73, the movie starts the final results window with its
	// StartTotalResults key and then loops until the window is done: in the neutral ending
	// DayOverModeState::update leaves for GameExit when the window reports done in phase three,
	// and runs phase four (GameExit, window unread) as soon as no movie is active. The disc's
	// (read-only, 2026-10-28): NonGameMovie, one scene, a 1199-frame cut played once with FadeIn
	// at 0 and StartTotalResults at 1197, then the same scene's frames 1197-1199 in a loop, with
	// the space backdrop, Olimar and the ship as actors. Same shape here, shorter, no actors.
	{
		Movie m;
		m.cin           = "dataDir/cinemas/demo77.cin";
		m.cinema.flags  = kCinNonGameMovie;
		m.cinema.scenes = { "cinemas/demo77/demo77.dsk" };
		m.sceneFrames   = { 30 };
		CinCut once, loop;
		once.endFrame   = 30;
		once.keys       = { notify(kMovieCmdStartTotalResults, 0, 27), notify(kMovieCmdFadeIn, 0, 0) };
		loop.startFrame = 27;
		loop.endFrame   = 30;
		loop.flags      = kCutLoop;
		m.cinema.cuts   = { once, loop };
		list.push_back(m);
	}
	return list;
}

} // namespace

bool writeStandInCinemas(std::vector<StandInCinemaFile>& out, std::string& error)
{
	out.clear();
	for (const Movie& m : movies()) {
		StandInCinemaFile cin;
		cin.path = m.cin;
		if (!writeCin(m.cinema, cin.text, error)) return error = cin.path + ": " + error, false;
		out.push_back(cin);
		for (std::size_t i = 0; i < m.cinema.scenes.size(); i++) {
			StandInCinemaFile dsk;
			dsk.path = "dataDir/" + m.cinema.scenes[i];
			if (!writeDsk(standInScene(m.sceneFrames[i]), dsk.text, error)) return error = dsk.path + ": " + error, false;
			out.push_back(dsk);
		}
	}
	return true;
}

} // namespace nectar
