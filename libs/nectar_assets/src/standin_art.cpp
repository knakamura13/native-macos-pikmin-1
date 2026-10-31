#include "nectar_assets/standin_art.h"

#include "nectar_assets/mesh_mod.h"
#include "nectar_assets/piki_rig.h"
#include "nectar_assets/texture.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

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

void setJointMaterial(Model& m, int joint, int material)
{
	m.joints[std::size_t(joint)].matPolys.clear();
	m.joints[std::size_t(joint)].matPolys.push_back({ material, joint });
}

void clearJointMesh(Model& m, int joint)
{
	jointGroup(m, joint).tris.clear();
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

void pushClockwiseTri(Model& m, ModMtxGroup& group, int ia, int ib, int ic, int normalHint)
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
	if (replaceJoint) clearJointMesh(m, joint);
	const int base = int(m.positions.size());
	for (int c = 0; c < 8; c++) {
		m.positions.push_back({ center.x + ((c & 1) ? hx : -hx), center.y + ((c & 2) ? hy : -hy), center.z + ((c & 4) ? hz : -hz) });
	}
	if (m.normals.empty()) m.normals.assign(kFaceNormal, kFaceNormal + 6);
	if (m.uvs[0].empty()) m.uvs[0].push_back(Vec2 {});

	ModMtxGroup& group = jointGroup(m, joint);
	for (int f = 0; f < 6; f++) {
		const int* q = kFaces[f];
		const int tri[2][3] = { { q[0], q[1], q[2] }, { q[0], q[2], q[3] } };
		for (const auto& t : tri) {
			pushClockwiseTri(m, group, base + t[0], base + t[1], base + t[2], f);
		}
	}
	setJointMaterial(m, joint, material);
}

void addSphereMesh(Model& m, const StandInModelSpec& spec, int joint, int material, float radius, int stacks, int slices, Vec3 center, bool replaceJoint = true)
{
	if (!jointInRange(spec, joint)) return;
	if (replaceJoint) clearJointMesh(m, joint);
	if (m.normals.empty()) m.normals.assign(kFaceNormal, kFaceNormal + 6);
	if (m.uvs[0].empty()) m.uvs[0].push_back(Vec2 {});

	ModMtxGroup& group = jointGroup(m, joint);
	const int base       = int(m.positions.size());
	const int north      = base;
	m.positions.push_back({ center.x, center.y + radius, center.z });
	for (int si = 1; si < stacks; si++) {
		const float v     = float(si) / float(stacks);
		const float phi   = v * 3.14159265f;
		for (int ti = 0; ti < slices; ti++) {
			const float u     = float(ti) / float(slices);
			const float theta = u * 2.0f * 3.14159265f;
			m.positions.push_back({ center.x + radius * std::sin(phi) * std::cos(theta), center.y + radius * std::cos(phi),
			                        center.z + radius * std::sin(phi) * std::sin(theta) });
		}
	}
	const int south = int(m.positions.size());
	m.positions.push_back({ center.x, center.y - radius, center.z });

	auto ring = [&](int si, int ti) { return base + 1 + (si - 1) * slices + (ti % slices); };
	for (int ti = 0; ti < slices; ti++) {
		pushClockwiseTri(m, group, north, ring(1, ti + 1), ring(1, ti), -1);
	}
	for (int si = 1; si < stacks - 1; si++) {
		for (int ti = 0; ti < slices; ti++) {
			const int i0 = ring(si, ti), i1 = ring(si, ti + 1), i2 = ring(si + 1, ti + 1), i3 = ring(si + 1, ti);
			pushClockwiseTri(m, group, i0, i1, i2, -1);
			pushClockwiseTri(m, group, i0, i2, i3, -1);
		}
	}
	for (int ti = 0; ti < slices; ti++) {
		pushClockwiseTri(m, group, south, ring(stacks - 1, ti), ring(stacks - 1, ti + 1), -1);
	}
	setJointMaterial(m, joint, material);
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
	const Rgba8 skin = spec.colour;
	const int skinMat = addMaterial(m, skin);
	const int outlineMat = addMaterial(m, { 25, 25, 30, 255 });
	const int leafMat = addMaterial(m, { 40, 170, 55, 255 });
	const int budMat  = addMaterial(m, { 130, 210, 70, 255 });
	const int noseMat  = addMaterial(m, { 220, 40, 40, 255 });
	const int eyeWhite = addMaterial(m, { 245, 245, 250, 255 });
	const int eyeDark  = addMaterial(m, { 15, 15, 20, 255 });

	addBoxMesh(m, spec, 0, skinMat, 2.6f, 1.2f, 2.4f, { 0.0f, -2.5f, 0.0f });
	addBoxMesh(m, spec, 1, skinMat, 1.7f, 2.1f, 1.4f, { 0.0f, 0.5f, 0.0f });
	addSphereMesh(m, spec, kPikiRigHeadJoint, skinMat, 2.6f, 7, 10, { 0.0f, 1.6f, 1.1f });
	addBoxMesh(m, spec, kPikiRigHeadJoint, outlineMat, 2.75f, 0.35f, 2.75f, { 0.0f, -0.2f, 1.0f }, false);
	addBoxMesh(m, spec, kPikiRigHeadJoint, eyeWhite, 0.55f, 0.45f, 0.25f, { -0.9f, 1.8f, 2.6f }, false);
	addBoxMesh(m, spec, kPikiRigHeadJoint, eyeWhite, 0.55f, 0.45f, 0.25f, { 0.9f, 1.8f, 2.6f }, false);
	addBoxMesh(m, spec, kPikiRigHeadJoint, eyeDark, 0.22f, 0.22f, 0.12f, { -0.9f, 1.75f, 2.78f }, false);
	addBoxMesh(m, spec, kPikiRigHeadJoint, eyeDark, 0.22f, 0.22f, 0.12f, { 0.9f, 1.75f, 2.78f }, false);
	if (red) addBoxMesh(m, spec, 3, noseMat, 0.65f, 0.5f, 1.2f, { 0.0f, 1.0f, 2.8f });
	if (yellow) {
		addBoxMesh(m, spec, 3, skinMat, 1.3f, 0.45f, 0.65f, { 0.0f, 1.25f, 1.55f });
		addBoxMesh(m, spec, 3, outlineMat, 1.35f, 0.2f, 0.7f, { 0.0f, 1.55f, 1.55f }, false);
	}
	if (blue) addBoxMesh(m, spec, 3, outlineMat, 0.35f, 0.35f, 0.35f, { 0.0f, 1.1f, 2.5f }, false);
	addBoxMesh(m, spec, 4, leafMat, 0.28f, 1.8f, 0.14f, { 0.0f, 0.8f, 0.0f });
	addBoxMesh(m, spec, 5, budMat, 0.22f, 1.4f, 0.12f, { 0.0f, 1.7f, 0.0f });
	addBoxMesh(m, spec, kPikiRigLeafTipJoint, leafMat, 1.05f, 0.4f, 1.05f, { 0.0f, 2.5f, 0.0f });
	addBoxMesh(m, spec, kPikiRigRightHandJoint, skinMat, 0.85f, 0.55f, 0.85f, { 2.0f, 0.0f, 0.0f });
	addBoxMesh(m, spec, kPikiRigLeftHandJoint, skinMat, 0.85f, 0.55f, 0.85f, { -2.0f, 0.0f, 0.0f });
	addBoxMesh(m, spec, 9, outlineMat, 1.15f, 0.55f, 1.05f, { 0.0f, -3.5f, 0.0f });
	addBoxMesh(m, spec, 10, skinMat, 0.7f, 1.35f, 0.7f, { 0.85f, -5.1f, 0.0f });
	addBoxMesh(m, spec, 11, skinMat, 0.7f, 1.35f, 0.7f, { -0.85f, -5.1f, 0.0f });
}

void buildCaptain(Model& m, const StandInModelSpec& spec)
{
	const int suit = addMaterial(m, { 240, 240, 250, 255 });
	const int pack = addMaterial(m, { 190, 55, 55, 255 });
	const int helm = addMaterial(m, { 200, 220, 255, 255 });
	const int dark = addMaterial(m, { 35, 35, 45, 255 });
	const int eyeWhite = addMaterial(m, { 250, 250, 255, 255 });
	addBoxMesh(m, spec, 0, dark, 3.0f, 0.9f, 3.0f, { 0.0f, -2.0f, 0.0f });
	addBoxMesh(m, spec, 1, suit, 2.0f, 2.4f, 1.7f, { 0.0f, 1.0f, 0.0f });
	addSphereMesh(m, spec, kPikiRigHeadJoint, helm, 3.0f, 7, 10, { 0.0f, 3.6f, 0.6f });
	addBoxMesh(m, spec, kPikiRigHeadJoint, dark, 3.15f, 0.35f, 3.15f, { 0.0f, 2.2f, 0.6f }, false);
	addBoxMesh(m, spec, kPikiRigHeadJoint, eyeWhite, 0.45f, 0.35f, 0.2f, { -0.85f, 3.9f, 2.3f }, false);
	addBoxMesh(m, spec, kPikiRigHeadJoint, eyeWhite, 0.45f, 0.35f, 0.2f, { 0.85f, 3.9f, 2.3f }, false);
	addBoxMesh(m, spec, 4, pack, 1.35f, 1.8f, 0.8f, { 0.0f, 1.5f, -1.9f });
	addBoxMesh(m, spec, 5, dark, 0.18f, 3.0f, 0.18f, { 0.0f, 5.6f, 0.0f });
	addBoxMesh(m, spec, 6, dark, 0.35f, 0.35f, 0.35f, { 0.0f, 7.0f, 0.0f });
	addBoxMesh(m, spec, kPikiRigRightHandJoint, suit, 0.9f, 0.65f, 0.9f, { 2.2f, 0.5f, 0.0f });
	addBoxMesh(m, spec, kPikiRigLeftHandJoint, suit, 0.9f, 0.65f, 0.9f, { -2.2f, 0.5f, 0.0f });
	addBoxMesh(m, spec, 9, dark, 1.35f, 0.65f, 1.15f, { 0.0f, -3.5f, 0.0f });
	addBoxMesh(m, spec, 10, dark, 0.75f, 1.5f, 0.75f, { 0.75f, -5.1f, 0.0f });
	addBoxMesh(m, spec, 11, dark, 0.75f, 1.5f, 0.75f, { -0.75f, -5.1f, 0.0f });
	(void)spec;
}

void buildOnion(Model& m, const StandInModelSpec& spec)
{
	const int bodyMat = addMaterial(m, spec.colour);
	const int stemMat = addMaterial(m, { 70, 150, 60, 255 });
	const int legMat  = addMaterial(m, { 90, 60, 35, 255 });
	const int rimMat  = addMaterial(m, { 30, 30, 35, 255 });
	const float r = spec.halfSize * 1.15f;
	addSphereMesh(m, spec, 0, bodyMat, r * 0.9f, 8, 12, { 0.0f, r * 0.65f, 0.0f });
	addBoxMesh(m, spec, 0, rimMat, r * 0.95f, 0.45f, r * 0.95f, { 0.0f, r * 0.15f, 0.0f }, false);
	for (int leg : { 3, 6, 9 }) {
		addBoxMesh(m, spec, leg, legMat, 1.4f, 4.0f, 1.4f, { 0.0f, -2.2f, 0.0f });
	}
	addBoxMesh(m, spec, 1, stemMat, 1.8f, 3.0f, 1.8f, { 0.0f, r * 1.25f, 0.0f });
	addSphereMesh(m, spec, 12, bodyMat, r * 0.6f, 6, 9, { 0.0f, r * 1.95f, 0.0f });
}

void buildShip(Model& m, const StandInModelSpec& spec)
{
	const int hull = addMaterial(m, spec.colour);
	const int glass = addMaterial(m, { 100, 180, 230, 255 });
	const int leg = addMaterial(m, { 70, 70, 85, 255 });
	const int rim = addMaterial(m, { 25, 25, 30, 255 });
	addSphereMesh(m, spec, 0, hull, 17.0f, 7, 14, { 0.0f, 11.0f, 0.0f });
	addBoxMesh(m, spec, 0, rim, 18.0f, 0.5f, 18.0f, { 0.0f, 2.0f, 0.0f }, false);
	addBoxMesh(m, spec, 3, glass, 11.0f, 5.5f, 11.0f, { 0.0f, 15.0f, 13.0f });
	for (int j : { 16, 19, 20 }) addBoxMesh(m, spec, j, leg, 1.7f, 7.5f, 1.7f, { 0.0f, -4.2f, 0.0f });
	(void)spec;
}

void buildPellet(Model& m, const StandInModelSpec& spec)
{
	const int mat = addMaterial(m, spec.colour);
	const int rim = addMaterial(m, { 30, 30, 35, 255 });
	const float r = spec.halfSize * 1.2f;
	addSphereMesh(m, spec, 0, mat, r * 0.9f, 6, 12, { 0.0f, r * 0.25f, 0.0f });
	addBoxMesh(m, spec, 1, rim, r, 0.45f, r, { 0.0f, -r * 0.45f, 0.0f });
	if (spec.jointCount() > 2) addBoxMesh(m, spec, 2, rim, 0.75f, 0.3f, 0.75f, { 0.0f, r * 0.55f, 0.0f });
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
		break;
	case Kind::Marker:
		addSphereMesh(m, spec, 0, addMaterial(m, { 255, 255, 100, 120 }), spec.halfSize, 5, 8, {});
		break;
	case Kind::Happa:
		if (spec.path.find("leaf") != std::string::npos) addBoxMesh(m, spec, 0, addMaterial(m, spec.colour), 0.2f, 2.0f, 1.0f, { 0.0f, 1.0f, 0.0f });
		else if (spec.path.find("bud") != std::string::npos) addSphereMesh(m, spec, 0, addMaterial(m, spec.colour), 1.5f, 4, 6, { 0.0f, 0.8f, 0.0f });
		else addBoxMesh(m, spec, 0, addMaterial(m, spec.colour), 1.2f, 0.3f, 1.2f, { 0.0f, 0.5f, 0.0f });
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
