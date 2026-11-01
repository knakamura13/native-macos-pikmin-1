#include "nectar_assets/standin_art.h"

#include "nectar_assets/mesh_mod.h"
#include "nectar_assets/piki_rig.h"
#include "nectar_assets/texture.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace nectar {

namespace {

constexpr Vec3 kFaceNormal[6] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
constexpr int kFaces[6][4] = { { 1, 3, 7, 5 }, { 0, 4, 6, 2 }, { 2, 6, 7, 3 }, { 0, 1, 5, 4 }, { 4, 5, 7, 6 }, { 0, 2, 3, 1 } };

int addMaterial(Model& m, Rgba8 colour)
{
	ModMaterial mat;
	mat.colour = colour;
	m.materials.push_back(mat);
	return int(m.materials.size()) - 1;
}

ModMtxGroup& jointGroup(Model& m, int joint)
{
	return m.meshes[std::size_t(joint)].groups[0];
}

bool jointInRange(const StandInModelSpec& spec, int joint)
{
	return joint >= 0 && joint < spec.jointCount();
}

void clearJointMesh(Model& m, int joint)
{
	jointGroup(m, joint).tris.clear();
}

// A .mod mesh has one material (ModJoint::matPolys pairs a material with a mesh),
// so a second colour on the same joint needs its own mesh. replaceJoint clears the
// joint back to its first mesh; otherwise the part goes in the joint's mesh for
// that material, made on first use.
ModMtxGroup& groupFor(Model& m, int joint, int material, bool replaceJoint)
{
	ModJoint& j = m.joints[std::size_t(joint)];
	if (replaceJoint || j.matPolys.empty()) {
		clearJointMesh(m, joint);
		j.matPolys.clear();
		j.matPolys.push_back({ material, joint });
		return jointGroup(m, joint);
	}
	for (const auto& mp : j.matPolys) {
		if (mp.first == material) return m.meshes[std::size_t(mp.second)].groups[0];
	}
	ModMesh mesh;
	mesh.parentJoint = joint;
	mesh.flags       = m.meshes[std::size_t(joint)].flags;
	mesh.groups.resize(1);
	mesh.groups[0].deps = { joint };
	m.meshes.push_back(std::move(mesh));
	j.matPolys.push_back({ material, int(m.meshes.size()) - 1 });
	return m.meshes.back().groups[0];
}

int addUnitNormal(Model& m, Vec3 n)
{
	const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
	if (len > 1e-6f) {
		n.x /= len;
		n.y /= len;
		n.z /= len;
	}
	m.normals.push_back(n);
	return int(m.normals.size()) - 1;
}

// normalHint < 0 orients the triangle away from `centre` (the centre of the
// sphere it belongs to), not from the joint origin: for a sphere off the origin
// the origin test flips some faces inward, which lit them as a dark checker.
void pushClockwiseTri(Model& m, ModMtxGroup& group, int ia, int ib, int ic, int normalHint, Vec3 centre = {})
{
	auto crossN = [&](int a, int b, int c) {
		const Vec3 &pa = m.positions[std::size_t(a)], &pb = m.positions[std::size_t(b)], &pc = m.positions[std::size_t(c)];
		const Vec3 u { pc.x - pa.x, pc.y - pa.y, pc.z - pa.z }, v { pb.x - pa.x, pb.y - pa.y, pb.z - pa.z };
		return Vec3 { u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x };
	};

	int nrm = normalHint;
	if (nrm < 0) {
		Vec3 sum {};
		for (int idx : { ia, ib, ic }) {
			Vec3 p = m.positions[std::size_t(idx)];
			p.x -= centre.x;
			p.y -= centre.y;
			p.z -= centre.z;
			const float len = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
			if (len > 1e-6f) {
				sum.x += p.x / len;
				sum.y += p.y / len;
				sum.z += p.z / len;
			}
		}
		const float len = std::sqrt(sum.x * sum.x + sum.y * sum.y + sum.z * sum.z);
		if (len > 1e-6f) {
			sum.x /= len;
			sum.y /= len;
			sum.z /= len;
		}
		if (crossN(ia, ib, ic).x * sum.x + crossN(ia, ib, ic).y * sum.y + crossN(ia, ib, ic).z * sum.z < 0.0f) std::swap(ib, ic);
		nrm = addUnitNormal(m, crossN(ia, ib, ic));
	} else {
		const Vec3& want = m.normals[std::size_t(nrm)];
		if (crossN(ia, ib, ic).x * want.x + crossN(ia, ib, ic).y * want.y + crossN(ia, ib, ic).z * want.z < 0.0f) std::swap(ib, ic);
	}

	ModTriangle tr;
	const int order[3] = { ia, ib, ic };
	for (int k = 0; k < 3; k++) {
		tr.v[k].mtxSlot = 0;
		tr.v[k].pos     = order[k];
		tr.v[k].nrm     = nrm;
		tr.v[k].uv[0]   = 0;
	}
	group.tris.push_back(tr);
}

void addBoxMesh(Model& m, const StandInModelSpec& spec, int joint, int material, float hx, float hy, float hz, Vec3 center, bool replaceJoint = true)
{
	if (!jointInRange(spec, joint)) return;
	ModMtxGroup& group = groupFor(m, joint, material, replaceJoint);
	const int base     = int(m.positions.size());
	for (int c = 0; c < 8; c++) {
		m.positions.push_back({ center.x + ((c & 1) ? hx : -hx), center.y + ((c & 2) ? hy : -hy), center.z + ((c & 4) ? hz : -hz) });
	}
	if (m.normals.empty()) m.normals.assign(kFaceNormal, kFaceNormal + 6);
	if (m.uvs[0].empty()) m.uvs[0].push_back(Vec2 {});

	for (int f = 0; f < 6; f++) {
		const int* q = kFaces[f];
		const int tri[2][3] = { { q[0], q[1], q[2] }, { q[0], q[2], q[3] } };
		for (const auto& t : tri) {
			pushClockwiseTri(m, group, base + t[0], base + t[1], base + t[2], f);
		}
	}
}

// Ellipsoid split into horizontal bands, top to bottom, one material per band:
// light top, darker underside. Unlit stand-in draws are flat colour, so the bands
// are what make a bulb or a hull read as round.
void addBandedEllipsoid(Model& m, const StandInModelSpec& spec, int joint, const std::vector<int>& bands, Vec3 radii, int stacks, int slices,
                        Vec3 center, bool replaceJoint = true)
{
	if (!jointInRange(spec, joint) || bands.empty()) return;
	if (m.normals.empty()) m.normals.assign(kFaceNormal, kFaceNormal + 6);
	if (m.uvs[0].empty()) m.uvs[0].push_back(Vec2 {});
	// Make every band's mesh first: groupFor can grow m.meshes and move groups.
	groupFor(m, joint, bands[0], replaceJoint);
	for (int mat : bands) groupFor(m, joint, mat, false);
	auto bandGroup = [&](int row) -> ModMtxGroup& {
		const int mat = bands[std::size_t(std::min(int(bands.size()) - 1, row * int(bands.size()) / stacks))];
		return groupFor(m, joint, mat, false);
	};

	const int base  = int(m.positions.size());
	const int north = base;
	m.positions.push_back({ center.x, center.y + radii.y, center.z });
	for (int si = 1; si < stacks; si++) {
		const float phi = float(si) / float(stacks) * 3.14159265f;
		for (int ti = 0; ti < slices; ti++) {
			const float theta = float(ti) / float(slices) * 2.0f * 3.14159265f;
			m.positions.push_back({ center.x + radii.x * std::sin(phi) * std::cos(theta), center.y + radii.y * std::cos(phi),
			                        center.z + radii.z * std::sin(phi) * std::sin(theta) });
		}
	}
	const int south = int(m.positions.size());
	m.positions.push_back({ center.x, center.y - radii.y, center.z });

	auto ring = [&](int si, int ti) { return base + 1 + (si - 1) * slices + (ti % slices); };
	for (int ti = 0; ti < slices; ti++) {
		pushClockwiseTri(m, bandGroup(0), north, ring(1, ti + 1), ring(1, ti), -1, center);
	}
	for (int si = 1; si < stacks - 1; si++) {
		for (int ti = 0; ti < slices; ti++) {
			const int i0 = ring(si, ti), i1 = ring(si, ti + 1), i2 = ring(si + 1, ti + 1), i3 = ring(si + 1, ti);
			pushClockwiseTri(m, bandGroup(si), i0, i1, i2, -1, center);
			pushClockwiseTri(m, bandGroup(si), i0, i2, i3, -1, center);
		}
	}
	for (int ti = 0; ti < slices; ti++) {
		pushClockwiseTri(m, bandGroup(stacks - 1), south, ring(stacks - 1, ti), ring(stacks - 1, ti + 1), -1, center);
	}
}

void addSphereMesh(Model& m, const StandInModelSpec& spec, int joint, int material, float radius, int stacks, int slices, Vec3 center, bool replaceJoint = true)
{
	addBandedEllipsoid(m, spec, joint, { material }, { radius, radius, radius }, stacks, slices, center, replaceJoint);
}

Model skeletonModel(const StandInModelSpec& spec)
{
	Model m;
	m.normals.assign(kFaceNormal, kFaceNormal + 6);
	m.uvs[0].push_back(Vec2 {});
	const int joints = spec.jointCount();
	const int defaultMat = addMaterial(m, spec.colour);
	for (int j = 0; j < joints; j++) {
		const float h = j == 0 ? spec.halfSize : spec.halfSize * 0.5f;
		ModMesh mesh;
		mesh.parentJoint = j;
		mesh.flags       = 1u | (1u << 3);
		mesh.groups.resize(1);
		mesh.groups[0].deps = { j };
		m.meshes.push_back(std::move(mesh));
		ModJoint joint;
		joint.parent    = spec.parents[std::size_t(j)];
		joint.boundsMin = { -h, -h, -h };
		joint.boundsMax = { h, h, h };
		joint.scale     = { 1, 1, 1 };
		m.joints.push_back(std::move(joint));
		m.vtxMatrices.push_back({ true, j });
		(void)defaultMat;
	}
	m.trailingIni = spec.collInfo;
	return m;
}

void fillEmptyJointMeshes(Model& m, const StandInModelSpec& spec)
{
	const int placeholderMat = addMaterial(m, { 8, 8, 8, 255 });
	for (int j = 0; j < spec.jointCount(); j++) {
		if (jointGroup(m, j).tris.empty()) addBoxMesh(m, spec, j, placeholderMat, 0.01f, 0.01f, 0.01f, {}, true);
	}
}

void addPlatformCollision(Model& m, const StandInModelSpec& spec)
{
	if (!spec.platform || m.meshes.empty()) return;
	for (const ModMtxGroup& g : m.meshes[0].groups) {
		for (const ModTriangle& t : g.tris) {
			ModCollTri c;
			c.code = 0x20000000u;
			for (int k = 0; k < 3; k++) c.v[k] = t.v[k].pos;
			m.collision.push_back(c);
		}
	}
}

enum class Kind {
	Generic,
	Captain,
	Piki,
	Cursor,
	Marker,
	Happa,
	Pellet,
	Onion,
	Ship,
	Bomb,
	PikiHead,
	Chappy,
};

Kind kindForPath(const std::string& path)
{
	if (path.find("nv3Model") != std::string::npos) return Kind::Captain;
	if (path.find("Model.mod") != std::string::npos && path.find("pikis/") != std::string::npos) return Kind::Piki;
	if (path.find("cursor.mod") != std::string::npos) return Kind::Cursor;
	if (path.find("marker.mod") != std::string::npos) return Kind::Marker;
	if (path.find("happas/") != std::string::npos) return Kind::Happa;
	if (path.find("pellets/") != std::string::npos) return Kind::Pellet;
	if (path.find("objects/goal/goal.mod") != std::string::npos) return Kind::Onion;
	if (path.find("ufo0705") != std::string::npos) return Kind::Ship;
	if (path.find("bomb.mod") != std::string::npos) return Kind::Bomb;
	if (path.find("pikihead") != std::string::npos) return Kind::PikiHead;
	if (path.find("chappy") != std::string::npos) return Kind::Chappy;
	return Kind::Generic;
}

void buildPikiBody(Model& m, const StandInModelSpec& spec, bool red, bool yellow, bool blue)
{
	// Material 0 is tinted at runtime via MaterialList[0].setColour(); keep the whole
	// body on slot 0 and secondary slots for eyes / stem toppers only.
	Rgba8 skin = spec.colour;
	if (red) skin = { 255, 45, 50, 255 };
	else if (yellow) skin = { 255, 215, 35, 255 };
	else if (blue) skin = { 55, 95, 255, 255 };
	const int skinMat   = addMaterial(m, skin);
	const int eyeDark   = addMaterial(m, { 15, 15, 20, 255 });
	const int leafMat   = addMaterial(m, { 45, 190, 55, 255 });
	const int budMat    = addMaterial(m, { 140, 220, 75, 255 });
	const int flowerMat = addMaterial(m, { 255, 180, 210, 255 });
	const int noseMat   = addMaterial(m, { 220, 40, 40, 255 });

	// The stand-in animations leave every joint at the model origin, so each part's
	// offset is its rest position; the feet sit about 1 unit under the origin.
	// Legs, a round body, a big round head on top with two dark eyes on its front
	// (+z), and the stem rising from the crown. The happa .mod is drawn at the
	// leaf-tip joint and is offset up to the stem top in its own stand-in.
	addBoxMesh(m, spec, 10, skinMat, 0.3f, 0.55f, 0.3f, { 0.55f, -0.55f, 0.0f });
	addBoxMesh(m, spec, 11, skinMat, 0.3f, 0.55f, 0.3f, { -0.55f, -0.55f, 0.0f });
	addBandedEllipsoid(m, spec, 0, { skinMat }, { 1.2f, 1.6f, 1.1f }, 6, 10, { 0.0f, 1.4f, 0.0f });
	addBoxMesh(m, spec, kPikiRigRightHandJoint, skinMat, 0.28f, 0.85f, 0.28f, { 1.35f, 1.5f, 0.0f });
	addBoxMesh(m, spec, kPikiRigLeftHandJoint, skinMat, 0.28f, 0.85f, 0.28f, { -1.35f, 1.5f, 0.0f });
	addSphereMesh(m, spec, kPikiRigHeadJoint, skinMat, 1.6f, 7, 10, { 0.0f, 4.4f, 0.2f });
	addBoxMesh(m, spec, kPikiRigHeadJoint, eyeDark, 0.28f, 0.36f, 0.1f, { -0.55f, 4.7f, 1.72f }, false);
	addBoxMesh(m, spec, kPikiRigHeadJoint, eyeDark, 0.28f, 0.36f, 0.1f, { 0.55f, 4.7f, 1.72f }, false);
	if (red) addBoxMesh(m, spec, 3, noseMat, 0.3f, 0.25f, 0.45f, { 0.0f, 4.15f, 1.85f });
	addBoxMesh(m, spec, 4, skinMat, 0.15f, 1.1f, 0.15f, { 0.0f, 7.0f, 0.0f });
	(void)leafMat;
	(void)budMat;
	(void)flowerMat;
}

void buildCaptain(Model& m, const StandInModelSpec& spec)
{
	const int suit = addMaterial(m, { 240, 240, 250, 255 });
	const int pack = addMaterial(m, { 190, 55, 55, 255 });
	const int helm = addMaterial(m, { 200, 220, 255, 255 });
	const int dark = addMaterial(m, { 35, 35, 45, 255 });
	const int face = addMaterial(m, { 235, 190, 150, 255 });
	// Every joint stays at the origin (see buildPikiBody); feet about 1 under it.
	// Dark boots, a round white suit with the red pack on his back, then the glass
	// helmet with his face, big nose and dark eyes at the front, and the antenna.
	addBoxMesh(m, spec, 10, dark, 0.45f, 0.8f, 0.45f, { 0.7f, -0.2f, 0.0f });
	addBoxMesh(m, spec, 11, dark, 0.45f, 0.8f, 0.45f, { -0.7f, -0.2f, 0.0f });
	addBandedEllipsoid(m, spec, 1, { suit }, { 1.7f, 1.8f, 1.5f }, 6, 10, { 0.0f, 2.2f, 0.0f });
	addBoxMesh(m, spec, 4, pack, 1.2f, 1.4f, 0.6f, { 0.0f, 2.6f, -1.7f });
	addBoxMesh(m, spec, kPikiRigRightHandJoint, suit, 0.45f, 1.0f, 0.45f, { 2.0f, 2.4f, 0.0f });
	addBoxMesh(m, spec, kPikiRigLeftHandJoint, suit, 0.45f, 1.0f, 0.45f, { -2.0f, 2.4f, 0.0f });
	addBoxMesh(m, spec, 0, dark, 1.9f, 0.25f, 1.9f, { 0.0f, 3.9f, 0.0f });
	addSphereMesh(m, spec, kPikiRigHeadJoint, helm, 2.2f, 7, 10, { 0.0f, 6.0f, 0.3f });
	addBoxMesh(m, spec, kPikiRigHeadJoint, face, 1.2f, 1.0f, 0.15f, { 0.0f, 6.0f, 2.45f }, false);
	addSphereMesh(m, spec, kPikiRigHeadJoint, face, 0.6f, 5, 8, { 0.0f, 5.6f, 2.75f }, false);
	addBoxMesh(m, spec, kPikiRigHeadJoint, dark, 0.25f, 0.3f, 0.1f, { -0.5f, 6.4f, 2.65f }, false);
	addBoxMesh(m, spec, kPikiRigHeadJoint, dark, 0.25f, 0.3f, 0.1f, { 0.5f, 6.4f, 2.65f }, false);
	addBoxMesh(m, spec, 5, dark, 0.12f, 1.0f, 0.12f, { 0.0f, 9.2f, 0.0f });
	addSphereMesh(m, spec, 6, pack, 0.4f, 4, 6, { 0.0f, 10.4f, 0.0f });
	(void)spec;
}

void buildOnion(Model& m, const StandInModelSpec& spec)
{
	// Red Onion: a round red bulb up on three thin legs, a pale rim under it, and a
	// stalk with a pale flower on top. The bulb is banded light to dark so it reads
	// as round when drawn unlit.
	const float k = spec.halfSize / 15.0f;
	std::vector<int> bulb;
	for (Rgba8 c : { Rgba8 { 255, 125, 120, 255 }, Rgba8 { 240, 60, 60, 255 }, Rgba8 { 215, 38, 44, 255 }, Rgba8 { 170, 26, 32, 255 },
	                 Rgba8 { 120, 18, 24, 255 } }) {
		bulb.push_back(addMaterial(m, c));
	}
	const int rimMat    = addMaterial(m, { 235, 225, 205, 255 });
	const int legMat    = addMaterial(m, { 95, 70, 70, 255 });
	const int footMat   = addMaterial(m, { 60, 45, 45, 255 });
	const int stalkMat  = addMaterial(m, { 120, 190, 80, 255 });
	const int petalMat  = addMaterial(m, { 255, 215, 225, 255 });
	const int centreMat = addMaterial(m, { 255, 225, 70, 255 });
	const float bulbY   = 30.0f * k;
	addBandedEllipsoid(m, spec, 0, bulb, { 15.0f * k, 12.5f * k, 15.0f * k }, 10, 16, { 0.0f, bulbY, 0.0f });
	addBandedEllipsoid(m, spec, 0, { rimMat }, { 9.0f * k, 2.2f * k, 9.0f * k }, 5, 12, { 0.0f, bulbY - 11.5f * k, 0.0f }, false);
	// Legs: three joints (3, 6, 9 with children) at 120 degrees; each is a thin
	// upper strut under the bulb, a lower strut further out and a foot pad.
	const int legJoints[3][3] = { { 3, 4, 5 }, { 6, 7, 8 }, { 9, 10, 11 } };
	for (int i = 0; i < 3; i++) {
		const float a  = (90.0f + 120.0f * float(i)) * 3.14159265f / 180.0f;
		const float dx = std::cos(a), dz = std::sin(a);
		addBoxMesh(m, spec, legJoints[i][0], legMat, 0.9f * k, 6.0f * k, 0.9f * k, { dx * 10.0f * k, 15.0f * k, dz * 10.0f * k });
		addBoxMesh(m, spec, legJoints[i][1], legMat, 0.9f * k, 5.0f * k, 0.9f * k, { dx * 13.0f * k, 5.5f * k, dz * 13.0f * k });
		addBoxMesh(m, spec, legJoints[i][2], footMat, 2.0f * k, 0.6f * k, 2.0f * k, { dx * 13.5f * k, 0.6f * k, dz * 13.5f * k });
	}
	// Stalk and flower on joint 12.
	addBoxMesh(m, spec, 12, stalkMat, 0.7f * k, 4.0f * k, 0.7f * k, { 0.0f, bulbY + 15.5f * k, 0.0f });
	addBoxMesh(m, spec, 12, petalMat, 5.0f * k, 0.8f * k, 1.8f * k, { 0.0f, bulbY + 20.0f * k, 0.0f }, false);
	addBoxMesh(m, spec, 12, petalMat, 1.8f * k, 0.8f * k, 5.0f * k, { 0.0f, bulbY + 20.0f * k, 0.0f }, false);
	addBoxMesh(m, spec, 12, centreMat, 1.4f * k, 1.0f * k, 1.4f * k, { 0.0f, bulbY + 20.6f * k, 0.0f }, false);
}

void buildShip(Model& m, const StandInModelSpec& spec)
{
	// Dolphin: a long rounded silver hull on three short legs, a red-orange nose at
	// +z, a blue cockpit dome on top and a tail fin. Banded light to dark like the
	// onion so the hull reads as round when drawn unlit.
	const float k = spec.halfSize / 20.0f;
	std::vector<int> hull, nose;
	for (Rgba8 c : { Rgba8 { 245, 247, 252, 255 }, Rgba8 { 222, 228, 238, 255 }, Rgba8 { 196, 204, 218, 255 }, Rgba8 { 160, 168, 186, 255 },
	                 Rgba8 { 112, 120, 138, 255 } }) {
		hull.push_back(addMaterial(m, c));
	}
	for (Rgba8 c : { Rgba8 { 255, 150, 90, 255 }, Rgba8 { 245, 95, 45, 255 }, Rgba8 { 215, 60, 35, 255 }, Rgba8 { 150, 40, 25, 255 } }) {
		nose.push_back(addMaterial(m, c));
	}
	const int glassTop = addMaterial(m, { 140, 200, 255, 255 });
	const int glass    = addMaterial(m, { 50, 120, 215, 255 });
	const int dark     = addMaterial(m, { 55, 60, 75, 255 });
	const int finMat   = addMaterial(m, { 205, 60, 45, 255 });
	const float hullY  = 12.0f * k;
	addBandedEllipsoid(m, spec, 0, hull, { 13.0f * k, 8.5f * k, 24.0f * k }, 10, 18, { 0.0f, hullY, 0.0f });
	addBandedEllipsoid(m, spec, 0, nose, { 7.0f * k, 6.0f * k, 7.5f * k }, 8, 12, { 0.0f, hullY - 0.5f * k, 21.0f * k }, false);
	addBandedEllipsoid(m, spec, 0, { glassTop, glass, glass }, { 6.0f * k, 4.5f * k, 8.0f * k }, 6, 12, { 0.0f, hullY + 7.0f * k, 8.0f * k }, false);
	// Tail fin and side engine pods.
	addBoxMesh(m, spec, 0, finMat, 0.8f * k, 5.0f * k, 4.0f * k, { 0.0f, hullY + 9.0f * k, -18.0f * k }, false);
	addBandedEllipsoid(m, spec, 0, { hull[1], hull[2], hull[3] }, { 3.5f * k, 3.5f * k, 7.0f * k }, 6, 10, { 13.5f * k, hullY - 1.0f * k, -10.0f * k }, false);
	addBandedEllipsoid(m, spec, 0, { hull[1], hull[2], hull[3] }, { 3.5f * k, 3.5f * k, 7.0f * k }, 6, 10, { -13.5f * k, hullY - 1.0f * k, -10.0f * k }, false);
	// Three landing legs down to the ground.
	for (Vec3 c : { Vec3 { 8.0f * k, 2.5f * k, -10.0f * k }, Vec3 { -8.0f * k, 2.5f * k, -10.0f * k }, Vec3 { 0.0f, 2.5f * k, 14.0f * k } }) {
		addBoxMesh(m, spec, 0, dark, 0.8f * k, 2.5f * k, 0.8f * k, c, false);
		addBoxMesh(m, spec, 0, dark, 2.0f * k, 0.4f * k, 2.0f * k, { c.x, 0.4f * k, c.z }, false);
	}
}

void buildPellet(Model& m, const StandInModelSpec& spec)
{
	// Number pellet: a thick upright disc, banded yellow, with a dark "1" on the face.
	std::vector<int> shell;
	for (Rgba8 c : { Rgba8 { 255, 245, 140, 255 }, Rgba8 { 255, 220, 40, 255 }, Rgba8 { 230, 180, 20, 255 }, Rgba8 { 170, 125, 10, 255 } }) {
		shell.push_back(addMaterial(m, c));
	}
	const int mark = addMaterial(m, { 120, 70, 10, 255 });
	const float r  = spec.halfSize * 0.9f;
	addBandedEllipsoid(m, spec, 0, shell, { r, r, r * 0.55f }, 8, 14, { 0.0f, r, 0.0f });
	addBoxMesh(m, spec, 0, mark, r * 0.12f, r * 0.5f, 0.15f, { 0.0f, r, r * 0.55f }, false);
	addBoxMesh(m, spec, 0, mark, r * 0.12f, r * 0.5f, 0.15f, { 0.0f, r, -r * 0.55f }, false);
}

void buildChappy(Model& m, const StandInModelSpec& spec)
{
	const int body = addMaterial(m, spec.colour);
	const int belly = addMaterial(m, { 240, 210, 170, 255 });
	const int spot = addMaterial(m, { 120, 40, 30, 255 });
	addSphereMesh(m, spec, 0, body, 10.0f, 6, 10, { 0.0f, 0.0f, 0.0f });
	addSphereMesh(m, spec, 1, belly, 7.5f, 5, 8, { 0.0f, -1.0f, 2.0f });
	addSphereMesh(m, spec, 4, body, 6.5f, 5, 7, { 0.0f, 2.0f, 6.0f });
	addBoxMesh(m, spec, 5, spot, 1.8f, 0.5f, 0.9f, { 0.0f, 2.5f, 8.5f });
	for (int j : { 8, 11 }) addBoxMesh(m, spec, j, body, 1.8f, 2.8f, 1.8f, { 0.0f, -2.0f, 0.0f });
	(void)spec;
}

} // namespace

void artFill(Image& img, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
	for (std::size_t i = 0; i < img.rgba.size(); i += 4) {
		img.rgba[i] = r;
		img.rgba[i + 1] = g;
		img.rgba[i + 2] = b;
		img.rgba[i + 3] = a;
	}
}

void artFillRect(Image& img, int x0, int y0, int x1, int y1, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
	x0 = std::max(0, x0);
	y0 = std::max(0, y0);
	x1 = std::min(img.width, x1);
	y1 = std::min(img.height, y1);
	for (int y = y0; y < y1; y++) {
		for (int x = x0; x < x1; x++) {
			std::uint8_t* p = img.at(x, y);
			p[0] = r;
			p[1] = g;
			p[2] = b;
			p[3] = a;
		}
	}
}

void artFillCircle(Image& img, int cx, int cy, int radius, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
	const int r2 = radius * radius;
	for (int y = cy - radius; y <= cy + radius; y++) {
		for (int x = cx - radius; x <= cx + radius; x++) {
			if (x < 0 || y < 0 || x >= img.width || y >= img.height) continue;
			const int dx = x - cx, dy = y - cy;
			if (dx * dx + dy * dy > r2) continue;
			std::uint8_t* p = img.at(x, y);
			p[0] = r;
			p[1] = g;
			p[2] = b;
			p[3] = a;
		}
	}
}

void artFillRing(Image& img, int cx, int cy, int outerR, int innerR, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
	const int o2 = outerR * outerR, i2 = innerR * innerR;
	for (int y = cy - outerR; y <= cy + outerR; y++) {
		for (int x = cx - outerR; x <= cx + outerR; x++) {
			if (x < 0 || y < 0 || x >= img.width || y >= img.height) continue;
			const int dx = x - cx, dy = y - cy;
			const int d2 = dx * dx + dy * dy;
			if (d2 > o2 || d2 < i2) continue;
			std::uint8_t* p = img.at(x, y);
			p[0] = r;
			p[1] = g;
			p[2] = b;
			p[3] = a;
		}
	}
}

void artDrawDigit(Image& img, int digit, int x, int y, int scale, std::uint8_t r, std::uint8_t g, std::uint8_t b)
{
	static const unsigned char kSeg[10][7] = {
	    { 1, 1, 1, 0, 1, 1, 1 }, { 0, 0, 1, 0, 0, 1, 0 }, { 1, 0, 1, 1, 1, 0, 1 }, { 1, 0, 1, 1, 0, 1, 1 },
	    { 0, 1, 1, 1, 0, 1, 0 }, { 1, 1, 0, 1, 0, 1, 1 }, { 1, 1, 0, 1, 1, 1, 1 }, { 1, 0, 1, 0, 0, 1, 0 },
	    { 1, 1, 1, 1, 1, 1, 1 }, { 1, 1, 1, 1, 0, 1, 1 },
	};
	if (digit < 0 || digit > 9) return;
	const auto seg = [&](int sx0, int sy0, int sx1, int sy1) { artFillRect(img, x + sx0 * scale, y + sy0 * scale, x + sx1 * scale, y + sy1 * scale, r, g, b); };
	if (kSeg[digit][0]) seg(1, 0, 3, 1);
	if (kSeg[digit][1]) seg(0, 1, 1, 3);
	if (kSeg[digit][2]) seg(3, 1, 4, 3);
	if (kSeg[digit][3]) seg(1, 3, 3, 4);
	if (kSeg[digit][4]) seg(0, 4, 1, 6);
	if (kSeg[digit][5]) seg(3, 4, 4, 6);
	if (kSeg[digit][6]) seg(1, 6, 3, 7);
}

std::vector<std::uint8_t> artBtiRgb5a3(int width, int height, const Image& img)
{
	BtiInput in;
	in.format = BTI_RGB5A3;
	in.width  = width;
	in.height = height;
	in.data   = encodeRgb5a3(img);
	std::vector<std::uint8_t> out;
	std::string error;
	if (!writeBti(in, out, error)) std::abort();
	return out;
}

Image artFlatGreyImage(int width, int height)
{
	Image img;
	img.resize(width, height);
	artFill(img, 0x80, 0x80, 0x80);
	return img;
}

std::vector<std::uint8_t> artHudTexture(const std::string& baseName)
{
	Image img;
	img.resize(64, 64);
	artFill(img, 0, 0, 0, 0);
	if (baseName == "navi" || baseName == "navi_portrait") {
		artFillCircle(img, 32, 36, 22, 240, 240, 250);
		artFillCircle(img, 32, 30, 18, 200, 220, 255, 220);
		artFillRect(img, 26, 8, 38, 16, 80, 80, 90);
	} else if (baseName.find("piki") != std::string::npos || baseName.find("bp_") == 0 || baseName.find("rp_") == 0
	           || baseName.find("yp_") == 0) {
		std::uint8_t r = 220, g = 50, b = 50;
		if (baseName.find("bp_") == 0 || baseName.find("yp_") == 0) {
			r = baseName[0] == 'y' ? 230 : 40;
			g = baseName[0] == 'y' ? 210 : 60;
			b = baseName[0] == 'y' ? 40 : 230;
		}
		artFillCircle(img, 32, 34, 18, r, g, b);
		artFillCircle(img, 32, 16, 8, 60, 180, 60);
	} else if (baseName.find("_sha64") != std::string::npos) {
		const int digit = baseName[0] >= '0' && baseName[0] <= '9' ? baseName[0] - '0' : -1;
		if (digit >= 0) artFillCircle(img, 34, 38, 12, 0, 0, 0, 70);
	} else if (baseName.find("_64") != std::string::npos || (baseName.size() == 5 && baseName[1] == '_')) {
		const int digit = baseName[0] >= '0' && baseName[0] <= '9' ? baseName[0] - '0' : -1;
		artFillRect(img, 4, 4, 60, 60, 20, 20, 30, 200);
		if (digit >= 0) artDrawDigit(img, digit, 18, 12, 4, 240, 240, 120);
	} else if (baseName.find("sun") != std::string::npos || baseName == "w08_160" || baseName == "ws08_160"
	           || baseName == "ws08_yel") {
		artFillCircle(img, 32, 32, 20, 255, 220, 80);
		artFillCircle(img, 32, 32, 14, 255, 240, 140);
	} else if (baseName == "moon") {
		artFillCircle(img, 28, 32, 18, 200, 210, 240);
		artFillCircle(img, 36, 28, 16, 20, 30, 60);
	} else if (baseName.find("dot") != std::string::npos) {
		artFillCircle(img, 32, 32, 10, 240, 240, 240);
	} else if (baseName.find("waku") != std::string::npos || baseName.find("p2b") != std::string::npos) {
		artFillRect(img, 4, 4, 60, 60, 40, 80, 140, 220);
		artFillRect(img, 10, 10, 54, 54, 0, 0, 0, 0);
	} else {
		artFill(img, 120, 120, 140);
	}
	return artBtiRgb5a3(64, 64, img);
}

Model buildArtStandInModel(const StandInModelSpec& spec)
{
	Model m = skeletonModel(spec);
	const Kind kind = kindForPath(spec.path);
	switch (kind) {
	case Kind::Captain: buildCaptain(m, spec); break;
	case Kind::Piki:
		buildPikiBody(m, spec, spec.path.find("red") != std::string::npos, spec.path.find("yel") != std::string::npos, spec.path.find("blu") != std::string::npos);
		break;
	case Kind::Cursor:
		addBoxMesh(m, spec, 0, addMaterial(m, { 255, 255, 255, 255 }), 1.4f, 0.18f, 0.18f, { 0.0f, 0.0f, 0.0f });
		addBoxMesh(m, spec, 1, addMaterial(m, { 255, 255, 80, 255 }), 0.25f, 0.25f, 2.2f, { 0.0f, 0.0f, 1.8f });
		break;
	case Kind::Marker:
		addSphereMesh(m, spec, 0, addMaterial(m, { 255, 255, 100, 120 }), spec.halfSize, 5, 8, {});
		break;
	case Kind::Happa:
		// Drawn at the Pikmin's leaf-tip joint, which stays at its origin: lift the
		// leaf, bud or flower to the top of the stem (y 8.1 in buildPikiBody).
		if (spec.path.find("leaf") != std::string::npos) addBoxMesh(m, spec, 0, addMaterial(m, spec.colour), 0.15f, 1.0f, 0.6f, { 0.0f, 9.0f, 0.0f });
		else if (spec.path.find("bud") != std::string::npos) addSphereMesh(m, spec, 0, addMaterial(m, spec.colour), 0.75f, 4, 6, { 0.0f, 8.7f, 0.0f });
		else addBoxMesh(m, spec, 0, addMaterial(m, spec.colour), 1.0f, 0.25f, 1.0f, { 0.0f, 8.3f, 0.0f });
		break;
	case Kind::Pellet: buildPellet(m, spec); break;
	case Kind::Onion: buildOnion(m, spec); break;
	case Kind::Ship: buildShip(m, spec); break;
	case Kind::Bomb: addSphereMesh(m, spec, 0, addMaterial(m, spec.colour), spec.halfSize, 5, 7, {}); break;
	case Kind::PikiHead:
		addSphereMesh(m, spec, 0, addMaterial(m, spec.colour), 2.5f, 5, 6, { 0.0f, 1.0f, 0.0f });
		addBoxMesh(m, spec, 3, addMaterial(m, { 50, 180, 50, 255 }), 0.2f, 1.2f, 0.2f, { 0.0f, 2.5f, 0.0f });
		break;
	case Kind::Chappy: buildChappy(m, spec); break;
	default: {
		const int mat = addMaterial(m, spec.colour);
		for (int j = 0; j < spec.jointCount(); j++) {
			const float h = j == 0 ? spec.halfSize : spec.halfSize * 0.5f;
			addBoxMesh(m, spec, j, mat, h, h, h, {});
		}
		break;
	}
	}
	fillEmptyJointMeshes(m, spec);
	addPlatformCollision(m, spec);
	return m;
}

} // namespace nectar
