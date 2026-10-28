#pragma once

// Stand-ins for everything an object, plant, gate, boss or enemy loads besides
// its model: the per-model animation bundles (.anm) and AnimMgr tables (.bin,
// .key) of issue #66's "other AnimMgrs" gap, the three ship parts naka1, naka4
// and naka7 and the pellet tables (parms/pelMgr.bin, objects/pelAnim.bin), and
// the .gen item and creature generators a sandbox stage can place (the Onion,
// a pellet, a sprout, an enemy).
//
// Fidelity (#62): the same rule as standin_anims.h. A stand-in matches the disc
// only where game code depends on it. The numbers below ONLY THE DISC HAS and
// were read once from the VERSION_GPIE01_01 install (read-only, a one-off parse
// of each AnimMgr table and bundle; nothing else was read from it): per info
// the frame count, the anim-key frames, the info-key records (which anim key
// each KEY_* event sits on) and the 'p00'/'spd' parameters, and per model the
// joint count. They are in standin_object_profiles.cpp. No curves, names,
// event-key records (sound and effect cues) or geometry are copied: every info
// is a static identity animation on the model's joints, named by this file.

#include "nectar_assets/mod.h"
#include "nectar_assets/standin_anims.h"
#include "nectar_assets/standin_models.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace nectar {

// ---------------------------------------------------------------- animation profiles

/// One info of an AnimMgr table, as measured on the disc.
struct ObjectAnimInfoProfile {
	int frames = 0; ///< frame count N of the animation payload
	int flags  = 2; ///< 'p00' (AnimInfoFlags; bit 1 = the game sets the speed)
	float speed = 30.0f; ///< 'spd'
	std::vector<int> keys; ///< anim-key frames, already clamped below N (the game clamps them on load)
	std::vector<std::pair<int, int>> infoKeys; ///< (anim-key index, key type): 0 loop start, 1 loop end, 2-5 KEY_Action0-3
};

/// One AnimMgr: its table, its bundle and the joint count of its model.
struct ObjectAnimProfile {
	const char* id;         ///< short name, also the prefix of the invented animation names
	const char* tablePath;  ///< relative to assets/, with dataDir/ as in the boot manifest
	const char* bundlePath;
	const char* baseDir;    ///< 'a01': prefix of every bundle entry name
	int joints;             ///< joint count of the model (and of every payload)
	std::vector<ObjectAnimInfoProfile> infos;
};

/// The 46 AnimMgrs of the boot manifest other than pikis/animMgr.bin, plus the hard
/// gate's bundle (the soft gate's sibling, sharing objects/gate/gate.bin).
const std::vector<ObjectAnimProfile>& objectAnimProfiles();

/// The profiles as animation sets ready to write: invented names, static
/// identity payloads, parents from the matching stand-in model when there is one.
const std::vector<StandInAnimSet>& standInObjectAnims();

/// The generator assertions for an object set: unique names, a joint list that
/// matches the model, anim keys below the frame count (the game clamps them
/// silently otherwise), info keys that point into the anim-key list and are
/// ascending, at most 16 of them.
bool checkObjectAnimSet(const StandInAnimSet& set, std::string& error);

/// Table and bundle of one set (`checkObjectAnimSet` first).
bool writeObjectAnimSet(const StandInAnimSet& set, std::vector<StandInAnimFile>& out, std::string& error);

/// writeObjectAnimSet over standInObjectAnims(). Sets that share a table path
/// (the two gates) write the same bytes twice; the caller's map keeps one.
bool writeStandInObjectAnimations(std::vector<StandInAnimFile>& out, std::string& error);

// ---------------------------------------------------------------- extra models

/// Models beyond the 45 of standin_models.h: ufoparts/naka1, naka4 and naka7
/// (parms/pelMgr.bin names them) and the enemy models of the sandbox.
const std::vector<StandInModelSpec>& standInObjectModelSpecs();

/// buildStandInModel over standInObjectModelSpecs().
bool writeStandInObjectModels(std::vector<StandInModelFile>& out, std::string& error);

// ---------------------------------------------------------------- pellet tables

/// parms/pelMgr.bin (the pellet configs, PelletMgr::readConfigs) and
/// objects/pelAnim.bin (the shape list, PelletMgr::readAnimInfos).
bool writeStandInPelletTables(std::vector<StandInAnimFile>& out, std::string& error);

// ---------------------------------------------------------------- everything

/// Every file this header writes (animations, extra models, pellet tables), each
/// with its path relative to assets/. The generator (#68) puts them in the tree.
bool writeStandInObjectFiles(std::vector<StandInAnimFile>& out, std::string& error);

} // namespace nectar
