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
