// Write-then-read tests for the object stand-ins (standin_objects.h): the
// per-model animation bundles and AnimMgr tables of the other AnimMgrs, the
// extra models (naka1, naka4, naka7, chappy), the pellet tables and the .gen
// item and creature generators.
//
// Oracles, per #62 (the loaders in shapeBase.cpp, animMgr.cpp, pelletMgr.cpp and
// generator.cpp do not link into a host test: they need Shape, CoreNode, a live
// StdSystem):
//  1. The game's real types and streams: DataChunk::read, AnimParam, AnimKey,
//     AnimInfo::Parms and AnimMgr::Parms (Animator.h), RamStream, the real
//     Parameters::read (src/plugPikiColin/parameters.cpp) and CmdStream
//     (src/sysCommon/cmdStream.cpp). The pellet config and shape records are
//     read with Parm fields declared the way PelletConfig and PelletAnimInfo
//     declare them (pelletMgr.cpp:108-155, animPellet.cpp:27-80).
//  2. Transcribed loops, same calls in the same order: AnimDck::read with
//     checkMask (shapeBase.cpp:1465-1520, 1181-1226), AnimMgr::read and
//     AnimInfo::doread (animMgr.cpp:229-297, 452-466), the entry loop of
//     StdSystem::loadBundle (stdSystem.cpp:631-714), Generator::read and the
//     GenBase::read of each part (generator.cpp:727-822), GenObjectItem,
//     GenObjectPellet, GenObjectTeki, GenAreaPoint and GenTypeOne. The weak
//     spot is the same as in anim_writer_test.cpp: the transcription is mine.
//     The strong check is the scenario run on the generated tree.
//  3. nectar::readBundle, nectar::parseMod and nectar::parseGen (libs/nectar_assets).
//
// Usage: nectar_standin_objects_test [boot-manifest.txt]. With the manifest, every
// .anm bundle and AnimMgr table of the boot manifest is checked to be written.
#include "nectar_assets/anim.h"
#include "nectar_assets/bundle.h"
#include "nectar_assets/gen.h"
#include "nectar_assets/mod.h"
#include "nectar_assets/mod_writer.h"
#include "nectar_assets/standin_anims.h"
#include "nectar_assets/standin_models.h"
#include "nectar_assets/standin_objects.h"

#include "Animator.h"
#include "CmdStream.h"
#include "Common/String.h"
#include "Parameters.h"
#include "Stream.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

extern std::string g_haltMessage;

namespace {
int failures = 0;

void check(bool ok, const std::string& what)
{
	if (!ok) {
		std::printf("FAIL: %s\n", what.c_str());
		failures++;
	}
}

using Bytes = std::vector<std::uint8_t>;

RamStream streamOf(const Bytes& b) { return RamStream(const_cast<std::uint8_t*>(b.data()), int(b.size())); }

// ------------------------------------------------------------------ AnimDck reader (shapeBase.cpp:1465-1520, 1181-1226)

struct LoadedAnim {
	int jointCount = 0, frames = 0;
	DataChunk scale, rotate, translate;
	std::vector<AnimDataInfo> info;
	std::vector<int> parent;
	int activeJoints = 0;
	bool ok          = true;
};

LoadedAnim readDck(RamStream& in)
{
	LoadedAnim a;
	a.jointCount = in.readInt();
	a.frames     = in.readInt();
	if (a.jointCount < 0 || a.jointCount > 4096) {
		a.ok = false;
		return a;
	}
	a.scale.read(in);
	a.rotate.read(in);
	a.translate.read(in);
	a.info.resize(std::size_t(a.jointCount));
	for (int i = 0; i < a.jointCount; i++) {
		in.readInt(); // group
		a.parent.push_back(in.readInt());
		AnimParam* sets[3] = { a.info[std::size_t(i)].mScale, a.info[std::size_t(i)].mRotation, a.info[std::size_t(i)].mTranslation };
		for (AnimParam* set : sets) {
			for (int j = 0; j < 3; j++) {
				set[j].mEntryNum   = in.readInt();
				set[j].mDataOffset = in.readInt();
				set[j].mFlags      = in.readInt();
			}
		}
	}
	for (int i = 0; i < a.jointCount; i++) { // checkMask
		AnimDataInfo& d    = a.info[std::size_t(i)];
		d.mFlags           = 0;
		int bit            = 0;
		AnimParam* sets[3] = { d.mScale, d.mRotation, d.mTranslation };
		for (AnimParam* set : sets) {
			for (int j = 0; j < 3; j++, bit++) {
				if (set[j].mEntryNum == 1) d.mFlags |= (1 << bit);
			}
			bit++;
		}
		if ((d.mFlags & AnimDataFlags::AllComponentsStatic) != AnimDataFlags::AllComponentsStatic) a.activeJoints++;
	}
	return a;
}

// ------------------------------------------------------------------ AnimMgr reader (animMgr.cpp:229-297, 452-466)

struct LoadedInfo {
	std::string name;
	int flags    = 0;
	float speed  = 0;
	std::vector<int> animKeys;
	std::vector<AnimKey> infoKeys, eventKeys;
};

struct LoadedMgr {
	int version = 0;
	std::string baseDir;
	std::vector<LoadedInfo> infos;
	int pending = -1;
};

LoadedMgr readAnimMgr(const Bytes& bytes)
{
	LoadedMgr m;
	RamStream in  = streamOf(bytes);
	m.version     = in.readInt();
	int infoCount = in.readInt();
	AnimMgr::Parms mgrParms;
	mgrParms.read(in);
	m.baseDir = mgrParms.mBasePath().mString;
	for (int i = 0; i < infoCount; i++) {
		LoadedInfo info;
		String str(0);
		in.readString(str);
		info.name = str.mString;
		AnimInfo::Parms parms;
		parms.read(in);
		info.flags     = parms.mFlags();
		info.speed     = parms.mSpeed();
		int animKeyNum = in.readInt();
		for (int k = 0; k < animKeyNum; k++) info.animKeys.push_back(in.readInt());
		if (m.version >= 1) {
			int infoKeyNum = in.readInt();
			for (int k = 0; k < infoKeyNum; k++) {
				AnimKey key;
				key.mFrameIndex = in.readInt();
				key.mEventType  = in.readShort();
				key.mEventCmdID = in.readByte();
				key.mKeyType    = in.readByte();
				info.infoKeys.push_back(key);
			}
		}
		int eventKeyNum = in.readInt();
		for (int k = 0; k < eventKeyNum; k++) {
			AnimKey key;
			key.mFrameIndex = in.readInt();
			key.mEventType  = in.readShort();
			key.mEventCmdID = in.readByte();
			key.mKeyType    = in.readByte();
			info.eventKeys.push_back(key);
		}
		m.infos.push_back(std::move(info));
	}
	m.pending = in.getPending();
	return m;
}

struct LoadedEntry {
	int type, size, payloadAt, consumedEnd;
	std::string name;
	LoadedAnim anim;
};

// The entry loop of StdSystem::loadBundle: types 2 and 3 hand the stream to a
// nested reader that consumes exactly its own bytes and ignores `size`.
std::vector<LoadedEntry> loadBundleLoop(const Bytes& bytes, int& pendingAfter)
{
	std::vector<LoadedEntry> out;
	RamStream fs  = streamOf(bytes);
	int fileCount = fs.readInt();
	for (int i = 0; i < fileCount; i++) {
		LoadedEntry e;
		e.type = fs.readInt();
		e.size = fs.readInt();
		String path(0);
		fs.readString(path);
		e.name      = path.mString;
		e.payloadAt = fs.getPosition();
		if (e.type == 3) {
			e.anim = readDck(fs);
		} else {
			e.anim.ok = false;
			fs.setPosition(fs.getPosition() + e.size);
		}
		e.consumedEnd = fs.getPosition();
		out.push_back(std::move(e));
	}
	pendingAfter = fs.getPending();
	return out;
}

// ------------------------------------------------------------------ collinfo (shapeBase.cpp:1901, shpObjColl.cpp:124)

struct CollRead {
	int joint = -1, depth = 0, type = -1;
	std::string id, code;
	float radius = 0.0f;
	nectar::Vec3 centre;
};

void loadini(CmdStream* cmd, std::vector<CollRead>& out, std::size_t self, int depth)
{
	while (!cmd->endOfCmds() && !cmd->endOfSection()) {
		cmd->getToken(true);
		if (cmd->isToken("id")) {
			cmd->getToken(true);
			out[self].id = std::string(cmd->mCurrentToken, strnlen(cmd->mCurrentToken, 4));
		} else if (cmd->isToken("code")) {
			cmd->getToken(true);
			out[self].code = std::string(cmd->mCurrentToken, strnlen(cmd->mCurrentToken, 4));
		} else if (cmd->isToken("type")) {
			sscanf(cmd->getToken(true), "%d", &out[self].type);
			if (out[self].type == 1) {
				cmd->getToken(true);
				sscanf(cmd->getToken(true), "%f", &out[self].radius);
				cmd->getToken(true);
				sscanf(cmd->getToken(true), "%f", &out[self].centre.x);
				sscanf(cmd->getToken(true), "%f", &out[self].centre.y);
				sscanf(cmd->getToken(true), "%f", &out[self].centre.z);
			}
		} else if (cmd->isToken("collinfo")) {
			int joint = -1;
			sscanf(cmd->getToken(true), "%d", &joint);
			out.emplace_back();
			const std::size_t child = out.size() - 1;
			out[child].joint        = joint;
			out[child].depth        = depth + 1;
			cmd->getToken(true);
			loadini(cmd, out, child, depth + 1);
		}
	}
	if (!cmd->endOfCmds()) cmd->getToken(true);
}

std::vector<CollRead> readColl(const std::string& text)
{
	std::vector<CollRead> out;
	Bytes tail(text.begin(), text.end());
	RamStream file(tail.data(), int(tail.size()));
	CmdStream* cmd = new CmdStream(&file);
	while (!cmd->endOfCmds() && !cmd->endOfSection()) {
		cmd->getToken(true);
		if (cmd->isToken("collinfo")) {
			int joint = -1;
			sscanf(cmd->getToken(true), "%d", &joint);
			out.emplace_back();
			const std::size_t self = out.size() - 1;
			out[self].joint        = joint;
			cmd->getToken(true);
			loadini(cmd, out, self, 0);
		}
	}
	return out;
}

// ------------------------------------------------------------------ tests: animation sets

nectar::Model parsedModel(const nectar::StandInModelSpec& spec)
{
	Bytes bytes;
	std::string err;
	check(nectar::writeMod(nectar::buildStandInModel(spec), bytes, err), spec.path + ": writeMod: " + err);
	nectar::Model m;
	check(nectar::parseMod(bytes.data(), bytes.size(), m, err), spec.path + ": parseMod: " + err);
	return m;
}

void testProfiles()
{
	const auto& profiles = nectar::objectAnimProfiles();
	const auto& sets     = nectar::standInObjectAnims();
	check(profiles.size() == 47 && sets.size() == 47, "47 object AnimMgrs (the boot manifest's 46 .anm bundles but the Pikmin one, plus the hard gate's)");

	std::set<std::string> bundles;
	for (std::size_t s = 0; s < sets.size(); s++) {
		const nectar::StandInAnimSet& set = sets[s];
		const nectar::ObjectAnimProfile& p = profiles[s];
		std::string err;
		check(nectar::checkObjectAnimSet(set, err), set.tablePath + ": " + err);
		check(bundles.insert(set.bundlePath).second, set.bundlePath + ": bundle path is unique");
		check(int(set.parents.size()) == p.joints, set.bundlePath + ": joint count is the profile's");

		std::vector<nectar::StandInAnimFile> files;
		check(nectar::writeObjectAnimSet(set, files, err), set.bundlePath + ": write: " + err);
		if (files.size() != 2) continue;

		// Table, through the game's Parameters and key types.
		LoadedMgr m = readAnimMgr(files[0].bytes);
		check(files[0].path == set.tablePath && files[1].path == set.bundlePath, set.bundlePath + ": paths");
		check(m.pending == 0, set.tablePath + ": AnimMgr::read consumes every byte");
		check(m.version == 1 && m.baseDir == set.baseDir, set.tablePath + ": version 1 and the base directory");
		check(m.infos.size() == p.infos.size(), set.tablePath + ": info count");
		for (std::size_t i = 0; i < m.infos.size() && i < p.infos.size(); i++) {
			const LoadedInfo& l = m.infos[i];
			const std::string who = set.tablePath + " info " + std::to_string(i);
			check(l.name == set.anims[i].info.name, who + ": name");
			check(l.flags == p.infos[i].flags && l.speed == p.infos[i].speed, who + ": p00 and spd");
			check(std::vector<int>(set.anims[i].info.animKeys.begin(), set.anims[i].info.animKeys.end()) == l.animKeys && l.animKeys == p.infos[i].keys,
			      who + ": anim keys");
			check(l.infoKeys.size() == p.infos[i].infoKeys.size(), who + ": info key count");
			for (std::size_t k = 0; k < l.infoKeys.size() && k < p.infos[i].infoKeys.size(); k++) {
				check(l.infoKeys[k].mKeyType == p.infos[i].infoKeys[k].second, who + ": info key type");
				// The same animate call fires the key: the frame it points at is the profile's, or the one before for
				// the disc's one descending list.
				check(l.infoKeys[k].mFrameIndex >= 0 && l.infoKeys[k].mFrameIndex < int(l.animKeys.size()), who + ": info key points into the anim keys");
			}
			check(l.eventKeys.empty(), who + ": no event keys (sound and effect cues are not stand-in data)");
			for (int f : l.animKeys) check(f >= 0 && f < p.infos[i].frames, who + ": anim key inside the frame count (checkAnimData clamps otherwise)");
			check(l.animKeys.size() >= 2 && l.animKeys.size() < 100, who + ": a start and an end anim key");
		}

		// Bundle, through the loader's entry loop and nectar::readBundle.
		int pending = -1;
		std::vector<LoadedEntry> loaded = loadBundleLoop(files[1].bytes, pending);
		check(loaded.size() == p.infos.size() && pending == 0, set.bundlePath + ": entry loop reads every entry and ends at the end of the file");
		std::vector<nectar::BundleEntry> viaLib;
		check(nectar::readBundle(files[1].bytes.data(), files[1].bytes.size(), viaLib, err), set.bundlePath + ": readBundle: " + err);
		check(viaLib.size() == loaded.size(), set.bundlePath + ": readBundle agrees on the entry count");
		for (std::size_t i = 0; i < loaded.size() && i < p.infos.size(); i++) {
			const LoadedEntry& e = loaded[i];
			const std::string who = set.bundlePath + " entry " + std::to_string(i);
			check(e.name == m.baseDir + "/" + m.infos[i].name, who + ": named <base>/<info name> (exact, case-sensitive match)");
			check(e.type == 3 && e.anim.ok, who + ": a DCK entry the loader accepts");
			check(e.consumedEnd - e.payloadAt == e.size, who + ": the reader consumed exactly `size` bytes");
			check(e.anim.jointCount == p.joints, who + ": joint count is the model's");
			check(e.anim.frames == p.infos[i].frames, who + ": frame count is the profile's");
			check(e.anim.activeJoints == 0, who + ": every joint static");
			check(e.anim.parent == set.parents, who + ": parents");
		}

		// The model, when one is written: same joint count and parents.
		std::string mod = set.bundlePath;
		mod.replace(mod.size() - 4, 4, ".mod");
		const nectar::StandInModelSpec* spec = nectar::findStandInModel(mod);
		if (!spec) {
			for (const nectar::StandInModelSpec& o : nectar::standInObjectModelSpecs()) {
				if (o.path == mod) spec = &o;
			}
		}
		if (spec) {
			nectar::Model model = parsedModel(*spec);
			check(int(model.joints.size()) == p.joints, set.bundlePath + ": the stand-in model has the animation's joint count");
			std::vector<int> parents;
			for (const auto& j : model.joints) parents.push_back(j.parent);
			check(parents == set.parents, set.bundlePath + ": the stand-in model has the animation's parents");
		}
	}

	// Specific disc-measured shapes the objects' code leans on.
	for (const auto& p : profiles) {
		if (std::string(p.id) == "goal") {
			check(p.joints == 13 && p.infos.size() == 13, "goal (the Onion): 13 joints, 13 infos");
		}
		if (std::string(p.id) == "pikihead") check(p.joints == 4 && p.infos.size() == 7, "pikihead (the sprout): 4 joints, 7 infos");
		if (std::string(p.id) == "chappy") check(p.joints == 17 && p.infos.size() == 15, "chappy: 17 joints, 15 infos");
		if (std::string(p.id) == "ufo0705") check(p.joints >= nectar::kStandInShipMinJoints && p.infos.size() == 17, "ufo0705: the ship's 57 joints, 17 infos");
	}
}

void testCheckRejects()
{
	const nectar::StandInAnimSet& good = nectar::standInObjectAnims()[2]; // goal
	std::string err;
	check(nectar::checkObjectAnimSet(good, err), "baseline set passes: " + err);

	nectar::StandInAnimSet s = good;
	s.anims[1].info.name = s.anims[0].info.name;
	check(!nectar::checkObjectAnimSet(s, err), "duplicate names are rejected");
	s = good;
	s.anims[0].info.animKeys.back() = s.anims[0].frameCount;
	check(!nectar::checkObjectAnimSet(s, err), "an anim key at the frame count is rejected (the game clamps it silently)");
	s = good;
	s.anims[0].frameCount = 0;
	check(!nectar::checkObjectAnimSet(s, err), "a frame count below 1 is rejected");
	s = good;
	s.anims[0].info.infoKeys.assign(17, nectar::AnimKeyRecord {});
	check(!nectar::checkObjectAnimSet(s, err), "more than 16 info keys are rejected");
	s = good;
	s.anims[0].info.infoKeys = { { 2, 0, 0, 2 }, { 1, 0, 0, 2 } };
	check(!nectar::checkObjectAnimSet(s, err), "descending info keys are rejected");
	s = good;
	s.parents.pop_back();
	check(!nectar::checkObjectAnimSet(s, err), "a joint list that differs from the stand-in model is rejected");
	s = good;
	s.anims.clear();
	check(!nectar::checkObjectAnimSet(s, err), "an empty set is rejected");
}

// ------------------------------------------------------------------ tests: models

void testModels()
{
	const auto& specs = nectar::standInObjectModelSpecs();
	check(specs.size() == 4, "four extra models: naka1, naka4, naka7, chappy");
	std::vector<nectar::StandInModelFile> files;
	std::string err;
	check(nectar::writeStandInObjectModels(files, err), "write models: " + err);
	check(files.size() == specs.size(), "one file per spec");
	for (const nectar::StandInModelSpec& spec : specs) {
		check(nectar::findStandInModel(spec.path) == nullptr, spec.path + ": not also one of the 45 models");
		nectar::Model m = parsedModel(spec);
		check(int(m.joints.size()) == spec.jointCount() && m.joints[0].parent == -1, spec.path + ": joint count and root");
		for (int j = 0; j < spec.jointCount(); j++) check(m.joints[std::size_t(j)].parent == spec.parents[std::size_t(j)], spec.path + ": parent " + std::to_string(j));
		check(!m.meshes.empty(), spec.path + ": has geometry");

		g_haltMessage.clear();
		std::vector<CollRead> parts = readColl(m.trailingIni);
		check(g_haltMessage.empty(), spec.path + ": CmdStream raised an error: " + g_haltMessage);
		for (const CollRead& p : parts) {
			check(p.joint >= 0 && p.joint < spec.jointCount(), spec.path + ": collinfo " + p.id + " joint index is in range");
			check(p.type == 1 && p.radius > 0.0f && p.id.size() == 4, spec.path + ": collinfo " + p.id + " is a sphere with a 4-letter id");
		}
		if (spec.path.find("naka") != std::string::npos) {
			check(parts.size() == 1 && parts[0].id == "cent" && parts[0].radius == 35.0f && parts[0].centre.y == 3.0f, spec.path + ": one 'cent' sphere");
		} else {
			// chappy: the parts the enemy code looks up by id, and the Pikmin sticking code.
			std::map<std::string, const CollRead*> byId;
			for (const CollRead& p : parts) byId[p.id] = &p;
			for (const char* id : { "cent", "head", "body", "slot", "lleg", "rleg" }) check(byId.count(id), spec.path + ": has '" + id + "'");
			check(byId.count("head") && byId["head"]->code == "stik" && byId["head"]->joint == 4, spec.path + ": head sticks, joint 4");
			check(byId.count("cent") && byId["cent"]->depth == 1, spec.path + ": 'cent' is a child of the body part");
			check(parts.size() == 8, spec.path + ": eight parts, two of them without an id");
		}
	}
}

// ------------------------------------------------------------------ tests: pellet tables

// PelletConfig's Parm fields, declared as in pelletMgr.cpp:108-125.
struct TestPelletConfig : public Parameters {
	TestPelletConfig()
	    : Parameters("pellet")
	    , mPelletName(this, String("new", 0), String("", 0), String("", 0), "x99", "theName")
	    , mPelletType(this, 0, 0, 0, "p00", "type")
	    , mPelletColor(this, -1, 0, 0, "p09", "color")
	    , mCarryMin(this, 1, 0, 0, "p01", "min pikis")
	    , mCarryMax(this, 1, 0, 0, "p02", "max pikis")
	{
	}
	Parm<String> mPelletName;
	Parm<int> mPelletType, mPelletColor, mCarryMin, mCarryMax;
	std::uint32_t model = 0, shape = 0, unused = 0;
	int joint = 0;
};

// PelletAnimInfo's, animPellet.cpp:27-35.
struct TestAnimInfo : public Parameters {
	TestAnimInfo()
	    : Parameters("pelletAnimInfo")
	    , mFolder(this, String("noname", 0), String("", 0), String("", 0), "x00", "folder")
	    , mFile(this, String("noname", 0), String("", 0), String("", 0), "x01", "theName")
	{
	}
	Parm<String> mFolder, mFile;
	std::uint32_t id = 0;
	int creation = 0, teki = 0, overrideJoint = 0;
};

std::uint32_t readId(RamStream& in) { return __builtin_bswap32(static_cast<std::uint32_t>(in.readInt())); } // ID32::read, then PelletConfig::read's swap

std::string idString(std::uint32_t id)
{
	std::string s;
	for (int i = 3; i >= 0; i--) s += char(id >> (8 * i));
	return s;
}

void testPelletTables()
{
	std::vector<nectar::StandInAnimFile> files;
	std::string err;
	check(nectar::writeStandInPelletTables(files, err), "write pellet tables: " + err);
	check(files.size() == 2, "two pellet files");
	if (files.size() != 2) return;

	check(files[0].path == "dataDir/parms/pelMgr.bin", "pelMgr.bin path");
	RamStream cfg = streamOf(files[0].bytes);
	const int configs = cfg.readInt(); // PelletMgr::readConfigs
	check(configs == 12, "twelve coloured pellet configs");
	std::set<std::string> ids, configShapes;
	for (int i = 0; i < configs; i++) {
		TestPelletConfig c;
		c.read(cfg); // PelletConfig::read
		c.model  = readId(cfg);
		c.shape  = readId(cfg);
		c.unused = readId(cfg);
		c.joint  = cfg.readInt();
		const std::string model = idString(c.model);
		check(ids.insert(model).second, "config id " + model + " is unique");
		configShapes.insert(idString(c.shape));
		check(model.size() == 4 && (model[1] == 'b' || model[1] == 'r' || model[1] == 'y') && model[0] == 'p', "config id " + model + " is p<colour><number>");
		check(idString(c.shape).rfind("pe", 0) == 0 && idString(c.unused) == "none" && c.joint == -1, "config " + model + ": shape pe**, no split, no repair joint");
		const int want = model[1] == 'b' ? 0 : model[1] == 'r' ? 1 : 2;
		check(c.mPelletType() == want && c.mPelletColor() == want, "config " + model + ": type and colour");
		check(c.mCarryMin() >= 1 && c.mCarryMax() >= c.mCarryMin(), "config " + model + ": carry range");
	}
	check(cfg.getPending() == 0, "PelletMgr::readConfigs consumes every byte");
	for (const char* id : { "pb01", "pr01", "py01", "pb20", "pr20", "py20" }) check(ids.count(id), std::string("config ") + id + " exists");

	check(files[1].path == "dataDir/objects/pelAnim.bin", "pelAnim.bin path");
	RamStream sh = streamOf(files[1].bytes);
	const int infos = sh.readInt(); // PelletMgr::readAnimInfos
	check(infos == 23, "23 shape infos: 4 pellets, 16 ship parts, 3 naka parts");
	std::set<std::string> shapeIds;
	int resident = 0;
	for (int i = 0; i < infos; i++) {
		TestAnimInfo a;
		a.id            = readId(sh); // PelletAnimInfo::read
		a.creation      = sh.readInt();
		a.teki          = sh.readInt();
		a.overrideJoint = sh.readInt();
		a.read(sh);
		const std::string id = idString(a.id);
		check(shapeIds.insert(id).second, "shape id " + id + " is unique");
		check(a.creation == 0 || a.creation == 1, "shape " + id + ": creation type");
		check(a.teki == -1, "shape " + id + ": no teki type");
		resident += a.creation == 0;
		const std::string path = std::string("dataDir/objects/") + a.mFolder().mString + "/" + a.mFile().mString + ".mod";
		const nectar::StandInModelSpec* spec = nectar::findStandInModel(path);
		if (!spec) {
			for (const nectar::StandInModelSpec& o : nectar::standInObjectModelSpecs()) {
				if (o.path == path) spec = &o;
			}
		}
		check(spec != nullptr, "shape " + id + ": " + path + " is a stand-in model");
		if (spec) check(a.overrideJoint < spec->jointCount(), "shape " + id + ": override joint is inside the model");
		const std::string table = std::string("dataDir/objects/") + a.mFolder().mString + "/" + a.mFile().mString + ".bin";
		bool hasTable = false;
		for (const nectar::StandInAnimSet& s : nectar::standInObjectAnims()) hasTable = hasTable || s.tablePath == table;
		check(hasTable, "shape " + id + ": " + table + " is a written AnimMgr table");
	}
	check(sh.getPending() == 0, "PelletMgr::readAnimInfos consumes every byte");
	check(resident == 20, "20 resident shapes (the ones the disc boot loads), three loaded on use");
	for (const char* id : { "pe01", "pe05", "pe10", "pe20", "ust1", "uf01", "un01", "un04", "un07" }) check(shapeIds.count(id), std::string("shape ") + id + " exists");
	for (const std::string& shape : configShapes) check(shapeIds.count(shape) == 1, "config shape " + shape + " has a shape info");
}

// ------------------------------------------------------------------ tests: boot manifest

void testManifest(const std::string& path)
{
	if (path.empty()) return;
	std::ifstream in(path);
	check(bool(in), "cannot read " + path);
	std::set<std::string> written;
	std::vector<nectar::StandInAnimFile> files;
	std::string err;
	check(nectar::writeStandInObjectFiles(files, err), "write all object files: " + err);
	for (const auto& f : files) written.insert(f.path);

	int anm = 0, tables = 0;
	std::string line;
	while (std::getline(in, line)) {
		if (line.empty() || line[0] == '#') continue;
		const std::string p = line.substr(0, line.find('\t'));
		auto ends = [&](const char* s) { return p.size() >= std::strlen(s) && p.compare(p.size() - std::strlen(s), std::string::npos, s) == 0; };
		if (p == "dataDir/pikis/bluModel.anm" || p == "dataDir/pikis/animMgr.bin") continue;
		if (ends(".anm")) {
			anm++;
			check(written.count(p) == 1, "manifest bundle " + p + " is written");
		}
		const bool isTable = ends("/anims.bin") || ends(".key") || p == "dataDir/objects/gate/gate.bin" || p.find("dataDir/objects/pellets/") == 0 && ends(".bin")
		                  || p.find("dataDir/objects/ufoparts/") == 0 && ends(".bin") || p == "dataDir/objects/ufo/ufo0705.bin" || p == "dataDir/objects/pelAnim.bin"
		                  || p == "dataDir/parms/pelMgr.bin" || p.find("dataDir/objects/plants/") == 0 && ends(".bin");
		if (isTable) {
			tables++;
			check(written.count(p) == 1, "manifest table " + p + " is written");
		}
		if (p.find("dataDir/objects/ufoparts/naka") == 0 && ends(".mod")) check(written.count(p) == 1, "manifest model " + p + " is written");
		if (p == "dataDir/tekis/chappy/chappy.mod") check(written.count(p) == 1, "manifest model " + p + " is written");
	}
	check(anm == 46, "the manifest lists 46 .anm bundles besides the Pikmin one, found " + std::to_string(anm));
	check(tables == 48, "found the manifest's 48 AnimMgr and pellet tables, " + std::to_string(tables));
}

} // namespace

int main(int argc, char** argv)
{
	testProfiles();
	testCheckRejects();
	testModels();
	testPelletTables();
	testManifest(argc > 1 ? argv[1] : "");
	if (failures) {
		std::printf("%d failure(s)\n", failures);
		return 1;
	}
	std::printf("standin objects tests passed\n");
	return 0;
}
