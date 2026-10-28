#include "nectar_assets/standin_textures.h"

#include "nectar_assets/bundle.h"
#include "nectar_assets/mesh_mod.h"
#include "nectar_assets/texture.h"

#include <cstdlib>

namespace nectar {

namespace {

// Gris medio opaco. Nada de la simulación depende del color ni del tamaño.
constexpr std::uint8_t GREY = 0x80;

// writeBti/writeTxe solo fallan con datos mal formados, y aquí son fijos: si
// fallan es un fallo de programación, no de entrada.
std::vector<std::uint8_t> mustBti(const BtiInput& in)
{
	std::vector<std::uint8_t> out;
	std::string error;
	if (!writeBti(in, out, error)) std::abort();
	return out;
}

} // namespace

std::vector<std::uint8_t> standInBti()
{
	Image img;
	img.resize(8, 8);
	for (std::size_t i = 0; i < img.rgba.size(); i += 4) {
		img.rgba[i]     = GREY;
		img.rgba[i + 1] = GREY;
		img.rgba[i + 2] = GREY;
		img.rgba[i + 3] = 0xFF;
	}
	BtiInput in;
	in.format = BTI_RGB5A3;
	in.width  = 8;
	in.height = 8;
	in.data   = encodeRgb5a3(img);
	return mustBti(in);
}

std::vector<std::uint8_t> standInConsFontBti()
{
	BtiInput in;
	in.format = BTI_IA4;
	in.width  = 128;
	in.height = 64;
	in.data.assign(btiDataBytes(BTI_IA4, 128, 64), 0xFF); // alfa 0xF, intensidad 0xF
	return mustBti(in);
}

std::vector<std::uint8_t> standInShadowTxe()
{
	Image img;
	img.resize(8, 8);
	for (std::size_t i = 0; i < img.rgba.size(); i += 4) {
		img.rgba[i] = img.rgba[i + 1] = img.rgba[i + 2] = GREY;
		img.rgba[i + 3]                                 = 0xFF;
	}
	TxeInput in;
	in.format = MOD_TEX_RGB5A3;
	in.width  = 8;
	in.height = 8;
	in.data   = encodeRgb5a3(img);
	std::vector<std::uint8_t> out;
	std::string error;
	if (!writeTxe(in, out, error)) std::abort();
	return out;
}

const std::vector<std::string>& effectTextureNames()
{
	static const std::vector<std::string> names = [] {
		// effectMgr.cpp, ptclLoadInfo (25-357) y simplePtclLoadInfo (366): 54 nombres distintos.
		static const char* const base[] = {
			"awa1",    "awaawa",   "biri1_s", "biriri",  "bomcross", "deadpiki", "dg_doro2", "dg_hahe2", "fuchibal",
			"g_cursor", "glow2_i", "growup00", "growup01", "hamon",   "hanabira", "hane4",    "hasikemu", "hibana2",
			"hithit",  "hitstar",  "hokori4",  "ice_smok", "iwa_1_32", "kaen_2",   "kaen_ok",  "mizukemu", "mizutama",
			"ov_cloud", "p_cloud05", "p_dust1", "p_hsbs",  "p_hsbs3",  "p_hsbs_n", "pkf",      "ps_ball",  "ps_glow",
			"qpika",   "rct_hahe", "ring_i4",  "ringhalo", "ringwave", "rock_a",   "soulf3",   "soulf4",   "star4_i",
			"star4_sp", "star5_03", "star8_i", "starx1b",  "starx1f",  "weed",     "woodchip", "yodapota", "yodare",
		};
		std::vector<std::string> v;
		for (const char* b : base) v.push_back(std::string("effects/tex/") + b + ".bti");
		return v;
	}();
	return names;
}

bool standInEffectTexBundle(std::vector<std::uint8_t>& out, std::string& error)
{
	std::vector<BundleEntry> entries;
	const std::vector<std::uint8_t> bti = standInBti();
	for (const std::string& name : effectTextureNames()) entries.push_back({ BUNDLE_BTI, name, bti });
	return writeBundle(entries, out, error);
}

bool standInTextureFiles(std::vector<StandInFile>& out, std::string& error)
{
	out.clear();
	const std::vector<std::uint8_t> bti = standInBti();

	out.push_back({ "consFont.bti", standInConsFontBti(), false });
	out.push_back({ "effects/shadow.txe", standInShadowTxe(), false });
	out.push_back({ "intro/nintendo.bti", bti, false });
	// Null comprobado en el juego, pero el manifiesto de disco los lista como abiertos.
	out.push_back({ "intro/item_0_9.bti", bti, true });
	out.push_back({ "intro/map1.bti", bti, true });

	// Texturas sueltas de screen/eng_tex que abre el arranque (manifiesto de #64).
	// Las que citan los .blo de #72 y viven en screen.bun se añaden cuando #72 las enumere.
	static const char* const hud[] = { "a_btn", "b_btn", "dot_24", "dot_32", "w08_160", "ws08_160", "ws08_yel", "x_btn", "y_btn", "z_btn" };
	for (const char* h : hud) out.push_back({ std::string("screen/eng_tex/") + h + ".bti", bti, false });

	StandInFile fx { "effects/tex/effects.bun", {}, false };
	if (!standInEffectTexBundle(fx.bytes, error)) return false;
	out.push_back(std::move(fx));

	// Sin entradas: los nombres salen de los .blo del HUD y el juego carga las
	// texturas sueltas al mismo nombre.
	StandInFile lang { "screen/eng_tex/screen.bun", {}, false };
	if (!writeBundle({}, lang.bytes, error)) return false;
	out.push_back(std::move(lang));
	return true;
}

} // namespace nectar
