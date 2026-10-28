// Stand-in asset generator for the disc-free scenario tests (issue #68, under
// #62 and #59). It calls every M2 writer in libs/nectar_assets and writes the
// complete stand-in tree to <out>/assets/dataDir/..., so `nectar --scenario`
// boots the sandbox stage with no disc data:
//
//   nectar_standin_generator <out> [--manifest <boot-manifest.txt>]
//
// <out> is the folder a scenario run uses as PIKMIN_SCENARIO_DATA_DIR (CTest
// passes <build>/scenario-data). The tree omits assets/.pikmin-assets: only the
// launcher reads it (#62).
//
// Pass criteria owned here (#68): the tree is built twice in-process and the two
// hashes must match (run-to-run identity on one machine; no cross-OS identity is
// claimed), every writer error is fatal, and with --manifest every path of the
// disc boot manifest (#64) is either in the tree or on the allowlist below with
// its reason.
//
// Fidelity (#62): nothing here is read from disc data. Numbers that only the
// disc has are recorded where they are used, in the sibling stand-in lists and
// below.

#include "nectar_assets/archive.h"
#include "nectar_assets/bundle.h"
#include "nectar_assets/gen.h"
#include "nectar_assets/mod_writer.h"
#include "nectar_assets/parms.h"
#include "nectar_assets/stages_ini.h"
#include "nectar_assets/standin_anims.h"
#include "nectar_assets/standin_models.h"
#include "nectar_assets/standin_textures.h"
#include "nectar_assets/standin_ui.h"
#include "nectar_mapcore/compile.h"
#include "nectar_mapcore/map.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

using Bytes = std::vector<std::uint8_t>;
/// Path relative to assets/ (so it starts with "dataDir/", as in the boot manifest) -> bytes.
using Tree = std::map<std::string, Bytes>;

// ── The sandbox stage ────────────────────────────────────────────────────────
// stages.ini `file` value, the stage .ini, its generator folder (the game cuts
// the path at its first '.', gameCoreSection.cpp initStage) and the map model.
constexpr const char* kSandboxIni    = "stages/sandbox.ini";
constexpr const char* kSandboxGenDir = "stages/sandbox/";
constexpr const char* kSandboxMod    = "courses/sandbox/sandbox.mod";
// In-memory only: what compileMap reads through its GameFileReader.
constexpr const char* kSourceMod  = "courses/sandbox/source.mod";
constexpr const char* kSourceIni  = "stages/sandbox_base.ini";

// compileMap with grid=false always builds meshes on materials 0 (walls), 14
// (floor), 15, 16 and 24 (water layers) and 23 (pond bed), and the game reads
// mMaterialList[poly->mIndex] without a bounds check (shapeBase.cpp), so the
// source .mod has at least 25 materials.
constexpr int kSourceMaterials = 25;

bool fail(std::string& error, const std::string& what)
{
	error = what;
	return false;
}

Bytes textBytes(const std::string& s) { return Bytes(s.begin(), s.end()); }

/// The map-source .mod: tables only, flat colour materials, no textures.
bool sandboxSourceMod(Bytes& out, std::string& error)
{
	nectar::Model m;
	for (int i = 0; i < kSourceMaterials; i++) {
		nectar::ModMaterial mat;
		mat.flags   = nectar::MOD_MAT_OPAQUE;
		mat.texAttr = -1;
		mat.colour  = { 110, 150, 90, 255 }; // grass-ish green
		if (i == 0) mat.colour = { 120, 110, 100, 255 }; // walls: rock grey
		m.materials.push_back(mat);
	}
	if (!nectar::writeModTables(m, out, error)) return fail(error, "sandbox source .mod: " + error);
	return true;
}

/// The stage .ini base that compileMap's buildIni rewrites: it only needs a
/// map_file line. createMapObjects (newPikiGame.cpp) acts on map_file,
/// day_multiply and an optional dayMgr block; with no dayMgr block DayMgr keeps
/// TimeSetting's default white parallel lights. day_multiply 1.0 is the value of
/// the disc's stages/stage1.ini (Forest of Hope, the disc-mode scenario stage),
/// so the world clock runs at the same rate as the disc-mode runs.
std::string sandboxBaseIni()
{
	return "# Sandbox stage for the disc-free scenario tests (#68), written by nectar_standin_generator.\n"
	       "map_file\t\tcourses/sandbox/sandbox.mod\n"
	       "day_multiply\t1.0\n";
}

/// A flat floor with one steep cliff: compileMap with grid=false and one MapWall
/// (DocWall defaults: thickness 40, height 150). The code dictates no cliff
/// numbers and no v1 scenario uses the cliff (#68). It stands off to the side,
/// clear of the 4 s walk throw_release scripts from the origin (stick forward is
/// -z with the start camera; the captain covers about 640 units, the map edge is
/// at 1200). A wall across that walk was tried: the captain stops against it
/// and one Pikmin settles within 5 units of him, inside the disc's 15-unit grab
/// range, so the grab skips the walk to the hand and K drops to 1.
bool sandboxMap(Tree& tree, std::string& error)
{
	Bytes sourceMod;
	if (!sandboxSourceMod(sourceMod, error)) return false;
	const std::string baseIni = sandboxBaseIni();

	nectar::MapDoc doc;
	doc.name      = "sandbox";
	doc.symmetric = false;
	doc.minX      = -1200;
	doc.maxX      = 1200;
	doc.minZ      = -1200;
	doc.maxZ      = 1200;
	doc.sourceMod = kSourceMod;
	doc.baseStage = kSourceIni;
	doc.terrain.border.source = kSourceMod;
	nectar::DocWall cliff;
	cliff.ax = 700;
	cliff.az = -300;
	cliff.bx = 700;
	cliff.bz = 300;
	doc.walls.push_back(cliff);

	nectar::MapData data;
	if (!nectar::expandMap(doc, data, error, /*validate=*/false)) return fail(error, "sandbox map: " + error);

	// Serves the in-memory source files and refuses every other path, so an
	// accidental disc read fails loudly.
	const nectar::GameFileReader read = [&](const std::string& path, Bytes& out) {
		if (path == kSourceMod) {
			out = sourceMod;
			return true;
		}
		if (path == kSourceIni) {
			out = textBytes(baseIni);
			return true;
		}
		std::fprintf(stderr, "[standin] compileMap asked for %s, which the generator does not serve\n", path.c_str());
		return false;
	};
	nectar::CompiledMap compiled;
	if (!nectar::compileMap(data, read, kSandboxMod, compiled, error)) return fail(error, "sandbox map: " + error);

	tree[std::string("dataDir/") + kSandboxMod] = compiled.mod;
	tree[std::string("dataDir/") + kSandboxIni] = compiled.ini;
	// The captain at the origin with no generators. Optional for the stage load,
	// but explicit data; day, init and plants files are omitted because scenarios
	// spawn their own Pikmin (#61).
	tree[std::string("dataDir/") + kSandboxGenDir + "default.gen"] = nectar::writeGen({});
	return true;
}

// ── stages/stages.ini ────────────────────────────────────────────────────────
// Five story entries with ids 0-4 (GameSetupSection walks the first STAGE_COUNT
// list nodes without a null check, #64), then the sandbox as the sixth. The
// sandbox has no `id` line, so it is STAGE_TESTMAP (5): not the tutorial (the
// clock runs), no zone banner, music falls back to stage 0. `hidden` keeps it out
// of the in-game map select. Only `file` must match; the story stage .ini files
// are never opened because no scenario enters them on stand-ins.
bool stagesIni(Tree& tree, std::string& error)
{
	std::vector<nectar::StagesEntry> entries;
	const char* const story[] = { "stages/practice.ini", "stages/stage1.ini", "stages/stage2.ini", "stages/stage3.ini", "stages/last.ini" };
	for (int id = 0; id < 5; id++) {
		nectar::StagesEntry e;
		e.visible = true;
		e.name    = "story" + std::to_string(id);
		e.id      = id;
		e.file    = story[id];
		entries.push_back(e);
	}
	nectar::StagesEntry sandbox;
	sandbox.visible = false; // `other`: readMapList treats anything but `visible` as hidden
	sandbox.name    = "sandbox";
	sandbox.file    = kSandboxIni;
	entries.push_back(sandbox);

	std::string text;
	if (!nectar::writeStagesIni(entries, text, error)) return fail(error, "stages.ini: " + error);
	tree["dataDir/stages/stages.ini"] = textBytes(text);
	return true;
}

// ── Parameter files ─────────────────────────────────────────────────────────
// Every value not listed keeps its code default. gamePrms.bin must exist (it is
// the scenario runner's data gate) and stays empty. naviMgr.bin and pikiMgr.bin
// hold two lists each (CreatureProp, then NaviProp or PikiProp).
//
// throw_release touches how the captain and his Pikmin move, collide, aim and
// throw, and how far a grab reaches, so those IDs carry the disc's values. With
// the code defaults the captain grabs straight from 80 units (K = 1, the
// ThrowWait key frame) where the disc walks the Pikmin to the hand first (K = 13
// at 30 Hz). Only the disc has these numbers: they were read once from the
// VERSION_GPIE01_01 install's parms/naviMgr.bin, pikiMgr.bin, aiconst.bin and
// cunit.bin (read-only, decoded with the Parameters::read layout, 2026-10-27),
// and each differs from the code default in NaviMgr.h, PikiMgr.h,
// CreatureProp.h, AIConstant.h or CPlate.h (noted per line).
// The other differing IDs (whistle, punch, health, growth, bombs) are left at
// the code defaults because no v1 scenario touches them.
bool parmFiles(Tree& tree, std::string& error)
{
	using nectar::parmFloat;
	struct File {
		const char* path;
		std::vector<std::vector<nectar::ParmEntry>> lists;
	};
	const File files[] = {
		{ "dataDir/parms/gamePrms.bin", { {} } },
		// AIConstant (aiConstants.cpp): gravity shapes the throw's flight.
		{ "dataDir/parms/aiconst.bin", { { parmFloat("p00", 560.0f) } } }, // mGravity (code 550)
		// CPlate (cPlate.cpp): where the squad stands behind the captain.
		{ "dataDir/parms/cunit.bin",
		  { {
		      parmFloat("p00", 17.5f), // mStartOffset (code 30)
		      parmFloat("p01", 90.0f), // mLengthLimit (code 150)
		      parmFloat("p02", 7.0f),  // mMaxPosSize (code 12)
		  } } },
		{ "dataDir/parms/naviMgr.bin",
		  {
		      // CreatureProp (code 0.5, 0.5, 0.25, 0.1, 0.3).
		      { parmFloat("s00", 1.0f), parmFloat("s01", 0.5f), parmFloat("s02", 0.5f), parmFloat("s03", 0.1f), parmFloat("s04", 0.3f) },
		      {
		          parmFloat("p00", 25.0f),   // mActionRadius (code 120)
		          parmFloat("p04", 160.0f),  // mMoveSpeed (code 170)
		          parmFloat("p56", 160.0f),  // mRunSpeed (code 210)
		          parmFloat("p38", 1.2f),    // mDisplayScale (code 1)
		          parmFloat("p10", 140.0f),  // mThrowMaxDistance (code 600)
		          parmFloat("p11", 140.0f),  // mThrowMinDistance (code 150)
		          parmFloat("p24", 75.0f),   // mThrowMaxHeight (code 100)
		          parmFloat("p25", 75.0f),   // mThrowMinHeight (code 80)
		          parmFloat("p54", 115.0f),  // mYellowThrowHeight (code 200)
		          parmFloat("p37", 15.0f),   // mPluckGrabRange (code 80): beyond it the Pikmin walks to the hand first
		          parmFloat("p39", 15.0f),   // mPikiWaitRange (code 30)
		          parmFloat("p40", 40.0f),   // mPikiFormationChangeRange (code 60)
		          parmFloat("p21", 8.0f),    // mBodyCollisionRadius (code 15)
		          parmFloat("p41", 8.5f),    // mGroundCollisionRadius (code 16)
		          parmFloat("p22", 1.0f),    // mInverseMass (code 4)
		          parmFloat("p23", 0.03f),   // aiming rotation speed (code 0.04)
		          parmFloat("p15", 8.0f),    // mWalkStartSpeed (code 35)
		          parmFloat("p16", 20.0f),   // mRunStartSpeed (code 70)
		          parmFloat("p17", 95.0f),   // mEscapeStartSpeed (code 160)
		          parmFloat("p18", 60.0f),   // mWalkAnimMinFrames (code 30)
		          parmFloat("p19", 90.0f),   // mWalkAnimMaxFrames (code 60)
		          parmFloat("p27", 40.0f),   // mRunAnimMinFrames (code 30)
		          parmFloat("p44", 0.65f),   // mCursorMoveStickThreshold (code 0.75)
		          parmFloat("p45", 0.0f),    // mCursorMinRadius (code 50)
		          parmFloat("p46", 100.0f),  // mCursorMaxRadius (code 300)
		          parmFloat("p47", 300.0f),  // mCursorMoveSpeed (code 200)
		          parmFloat("p48", 0.85f),   // mCursorMoveDelayTime (code 2)
		      },
		  } },
		{ "dataDir/parms/pikiMgr.bin",
		  {
		      // CreatureProp (code 0.5, 0.5, 0.25, 0.1, 0.3).
		      { parmFloat("s00", 0.1f), parmFloat("s01", 0.5f), parmFloat("s02", 0.8f), parmFloat("s03", 0.14f), parmFloat("s04", 0.3f) },
		      {
		          parmFloat("p00", 10.0f),  // mMinMoveSpeed (code 84)
		          parmFloat("p01", 120.0f), // mMaxLeafMoveSpeed (code 162.5)
		          parmFloat("p54", 170.0f), // mMaxFlowerMoveSpeed (code 220)
		          parmFloat("p65", 140.0f), // mMaxBudMoveSpeed (code 160)
		          parmFloat("p18", 8.0f),   // mWalkSpeedThreshold (code 35)
		          parmFloat("p19", 20.0f),  // mRunSpeedThreshold (code 70)
		          parmFloat("p20", 95.0f),  // mEscapeSpeedThreshold (code 160)
		          parmFloat("p27", 3.0f),   // mMinPikiSize (code 6)
		          parmFloat("p34", 5.0f),   // mMaxPikiSize (code 10)
		          parmFloat("p39", 8.5f),   // mCollisionRadius (code 8)
		      },
		  } },
	};
	for (const File& f : files) {
		nectar::ParmFile pf;
		pf.lists = f.lists;
		Bytes bytes;
		if (!nectar::writeParmFile(pf, bytes, error)) return fail(error, std::string(f.path) + ": " + error);
		tree[f.path] = bytes;
	}
	return true;
}

// ── Archive pairs ───────────────────────────────────────────────────────────
// The 18 pairs the USA build parses (arambundleList in gameSetup.cpp plus
// blo_eng from preloadLanguage). On PC files are read loose and the entries are
// never consumed, so an empty index and a 0-byte blob are enough; a missing .arc
// aborts with "Missing archive".
bool archives(Tree& tree, std::string& error)
{
	const char* const names[] = { "blo_eng", "tekis",   "bosses", "tekipara", "tekikey", "plants",   "ufopartsbin", "bridges", "gates",
		                          "bomb",    "rope",    "water",  "fl_water", "pelletsbin", "pikihead", "effshapes",   "weeds",   "goal" };
	for (const char* n : names) {
		nectar::ArchivePair pair;
		if (!nectar::writeArchivePair({}, pair, error)) return fail(error, std::string(n) + ": " + error);
		tree[std::string("dataDir/archives/") + n + ".dir"] = pair.dir;
		tree[std::string("dataDir/archives/") + n + ".arc"] = pair.arc;
	}
	return true;
}

// ── Models, animations, textures, screens and particles (#65 #66 #67 #72) ──
bool siblingStandIns(Tree& tree, std::string& error)
{
	std::vector<nectar::StandInModelFile> models;
	if (!nectar::writeStandInModels(models, error)) return fail(error, "stand-in models: " + error);
	for (auto& f : models) tree[f.path] = std::move(f.bytes);

	std::vector<nectar::StandInAnimFile> anims;
	if (!nectar::writeStandInAnimations(anims, error)) return fail(error, "stand-in animations: " + error);
	for (auto& f : anims) tree[f.path] = std::move(f.bytes);

	// Optional textures too: the game tolerates their absence, but the disc boot
	// opens them, so writing them keeps the open log close to the disc's.
	std::vector<nectar::StandInFile> textures;
	if (!nectar::standInTextureFiles(textures, error)) return fail(error, "stand-in textures: " + error);
	for (auto& f : textures) tree["dataDir/" + f.path] = std::move(f.bytes);

	for (const nectar::StandinLayout& l : nectar::standinBloLayouts()) {
		Bytes bytes;
		if (!nectar::writeBlo(l.panes, bytes, error)) return fail(error, l.file + ": " + error);
		tree["dataDir/screen/eng_blo/" + l.file] = std::move(bytes);
	}

	// effects/pcr/effects.bun: one inert type-0 entry per particle name, under its
	// full path, which is what particleLoader::load looks up.
	std::vector<nectar::BundleEntry> pcr;
	const Bytes inert = nectar::inertPcrPayload();
	for (const std::string& name : nectar::standinPcrNames()) pcr.push_back({ nectar::BUNDLE_BINARY, name, inert });
	Bytes bun;
	if (!nectar::writeBundle(pcr, bun, error)) return fail(error, "effects/pcr/effects.bun: " + error);
	tree["dataDir/effects/pcr/effects.bun"] = std::move(bun);
	return true;
}

bool buildTree(Tree& tree, std::string& error)
{
	tree.clear();
	return parmFiles(tree, error) && archives(tree, error) && stagesIni(tree, error) && sandboxMap(tree, error) && siblingStandIns(tree, error);
}

/// FNV-1a 64 over every path and its bytes, in path order.
std::uint64_t hashTree(const Tree& tree)
{
	std::uint64_t h = 1469598103934665603ull;
	auto mix        = [&](const std::uint8_t* p, std::size_t n) {
        for (std::size_t i = 0; i < n; i++) {
            h ^= p[i];
            h *= 1099511628211ull;
        }
	};
	for (const auto& [path, bytes] : tree) {
		mix(reinterpret_cast<const std::uint8_t*>(path.data()), path.size() + 1);
		const std::uint64_t n = bytes.size();
		mix(reinterpret_cast<const std::uint8_t*>(&n), sizeof(n));
		mix(bytes.data(), bytes.size());
	}
	return h;
}

// ── Manifest check ───────────────────────────────────────────────────────────
// Paths the disc boot manifest (#64, Forest of Hope on disc) lists that the
// stand-in tree leaves out on purpose. A prefix covers every path under it.
struct Allowed {
	const char* prefix;
	const char* why;
};
const Allowed kManifestAllowlist[] = {
	// What the Forest of Hope loads and the sandbox (no generators) does not.
	{ "dataDir/courses/stage1/", "the disc-mode stage's map; stand-ins boot the sandbox map" },
	{ "dataDir/stages/stage1", "the disc-mode stage's .ini and generators; stand-ins boot stages/sandbox.ini" },
	{ "dataDir/tekis/", "Forest of Hope enemies, created by its generators" },
	{ "dataDir/tekikeys/", "Forest of Hope enemy key files, read with its enemies" },
	{ "dataDir/bosses/", "Forest of Hope bosses, created by its generators" },
	{ "dataDir/objects/plants/", "Forest of Hope plants, created by its generators" },
	{ "dataDir/objects/bridge/", "Forest of Hope bridges, created by its generators" },
	{ "dataDir/objects/gate/", "Forest of Hope gates, created by its generators" },
	{ "dataDir/objects/hinderrock/", "Forest of Hope push box, created by its generators" },
	{ "dataDir/cinemas/", "Forest of Hope landing and Onion movies (STAGE_TESTMAP plays no landing); no cinema stand-ins" },
	// Silent when missing: CoreNode::load and the null-checked opens keep the
	// code defaults (see standin-open-allowlist.txt for each).
	{ "dataDir/tekipara/", "enemy parameters, CoreNode::load skips a missing file" },
	{ "dataDir/parms/demo.bin", "demo flag parameters, skipped when missing" },
	{ "dataDir/parms/sound.bin", "sound constants, skipped when missing" },
	{ "dataDir/parms/key.bin", "key config, skipped when missing; the throw key stays A" },
	{ "dataDir/parms/pelMgr.bin", "pellet configs, skipped when missing; no scenario creates a pellet by config" },
	{ "dataDir/camera/camepara.bin", "camera parameters, skipped when missing" },
	{ "dataDir/ghost/record.gst", "ghost replay, null-checked" },
	// Known gaps of the sibling stand-in lists (#65, #66): animation tables and
	// bundles of objects other than the captain and Pikmin, and the three
	// pelMgr-driven ship parts. A missing bundle or table is tolerated.
	{ "dataDir/objects/pelAnim.bin", "pellet animation table (#66 gap)" },
	{ "dataDir/objects/ufo/ufo0705.anm", "ship animations (#66 gap)" },
	{ "dataDir/objects/ufo/ufo0705.bin", "ship animation table (#66 gap)" },
	{ "dataDir/objects/ufoparts/", "ship part animations, and naka1/4/7 loaded through pelMgr.bin (#65, #66 gaps)" },
	{ "dataDir/objects/pellets/", "pellet animations (#66 gap)" },
	{ "dataDir/objects/pikihead/anims.bin", "sprout animations (#66 gap)" },
	{ "dataDir/objects/pikihead/pikihead.anm", "sprout animations (#66 gap)" },
	{ "dataDir/objects/bomb/anims.bin", "bomb animations (#66 gap)" },
	{ "dataDir/objects/bomb/bomb.anm", "bomb animations (#66 gap)" },
	{ "dataDir/objects/water/anims.bin", "water animations (#66 gap)" },
	{ "dataDir/objects/water/water.anm", "water animations (#66 gap)" },
	{ "dataDir/objects/fl_water/anims.bin", "flowing water animations (#66 gap)" },
	{ "dataDir/objects/fl_water/fl_water.anm", "flowing water animations (#66 gap)" },
	{ "dataDir/objects/goal/anims.bin", "Onion animations (#66 gap)" },
	{ "dataDir/objects/goal/goal.anm", "Onion animations (#66 gap)" },
};

int checkManifest(const Tree& tree, const std::string& manifestPath)
{
	std::ifstream in(manifestPath);
	if (!in) {
		std::fprintf(stderr, "[standin] cannot read %s\n", manifestPath.c_str());
		return 1;
	}
	int missing = 0, allowed = 0, present = 0;
	std::set<std::string> listed;
	std::string line;
	while (std::getline(in, line)) {
		if (line.empty() || line[0] == '#') continue;
		const std::string path = line.substr(0, line.find('\t'));
		listed.insert(path);
		if (tree.count(path)) {
			present++;
			continue;
		}
		const Allowed* hit = nullptr;
		for (const Allowed& a : kManifestAllowlist) {
			if (path.rfind(a.prefix, 0) == 0) hit = &a;
		}
		if (hit) {
			allowed++;
			continue;
		}
		std::fprintf(stderr, "[standin] manifest path not in the tree and not allowlisted: %s\n", path.c_str());
		missing++;
	}
	int extra = 0;
	for (const auto& kv : tree) extra += listed.count(kv.first) ? 0 : 1;
	std::printf("[standin] manifest: %d in the tree, %d allowlisted, %d missing; %d tree files the disc boot does not open (sandbox stage, stand-in parameter files)\n", present, allowed, missing, extra);
	return missing == 0 ? 0 : 1;
}

bool writeTree(const Tree& tree, const fs::path& out, std::string& error)
{
	std::error_code ec;
	// Only the generated dataDir is replaced, so a stale file from an older
	// generator never survives into a run.
	fs::remove_all(out / "assets" / "dataDir", ec);
	for (const auto& [path, bytes] : tree) {
		const fs::path file = out / "assets" / path;
		fs::create_directories(file.parent_path(), ec);
		std::ofstream f(file, std::ios::binary | std::ios::trunc);
		if (!f) return fail(error, "cannot write " + file.string());
		f.write(reinterpret_cast<const char*>(bytes.data()), std::streamsize(bytes.size()));
		if (!f) return fail(error, "cannot write " + file.string());
	}
	return true;
}

} // namespace

int main(int argc, char** argv)
{
	std::string out, manifest;
	for (int i = 1; i < argc; i++) {
		const std::string a = argv[i];
		if (a == "--manifest" && i + 1 < argc) {
			manifest = argv[++i];
		} else if (out.empty() && !a.empty() && a[0] != '-') {
			out = a;
		} else {
			out.clear();
			break;
		}
	}
	if (out.empty()) {
		std::fprintf(stderr, "usage: nectar_standin_generator <out> [--manifest <boot-manifest.txt>]\n");
		return 2;
	}

	std::string error;
	Tree first, second;
	if (!buildTree(first, error) || !buildTree(second, error)) {
		std::fprintf(stderr, "[standin] %s\n", error.c_str());
		return 1;
	}
	const std::uint64_t h1 = hashTree(first), h2 = hashTree(second);
	if (h1 != h2 || first != second) {
		std::fprintf(stderr, "[standin] two builds of the tree differ (%016llx vs %016llx)\n", (unsigned long long)h1, (unsigned long long)h2);
		return 1;
	}
	std::size_t bytes = 0;
	for (const auto& kv : first) bytes += kv.second.size();
	std::printf("[standin] %zu files, %zu bytes, hash %016llx (two builds identical)\n", first.size(), bytes, (unsigned long long)h1);

	if (!manifest.empty() && checkManifest(first, manifest) != 0) return 1;

	if (!writeTree(first, out, error)) {
		std::fprintf(stderr, "[standin] %s\n", error.c_str());
		return 1;
	}
	std::printf("[standin] wrote %s/assets/dataDir\n", out.c_str());
	return 0;
}
