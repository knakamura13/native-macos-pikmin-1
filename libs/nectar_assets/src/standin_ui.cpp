#include "nectar_assets/standin_ui.h"

#include <cstdio>

namespace nectar {

namespace {

// Tipos como en la tabla de etiquetas de #72: P imagen, B panel simple, T caja de
// texto. Donde el juego acepta cualquier tipo se usa un panel simple, que es lo
// más inerte. Ventanas solo la de con01.blo (p264): P2DWindow::loadResource
// desreferencia sus cuatro texturas de esquina, así que lleva las del juego.

BloPane node(BloType type, const std::string& tag)
{
	BloPane p;
	p.type = type;
	p.tag  = tag;
	p.w = p.h = 32;
	return p;
}

BloPane pane(const std::string& tag) { return node(BloType::Pane, tag); }
BloPane pic(const std::string& tag) { return node(BloType::Picture, tag); } // sin textura

// El nombre de fuente no puede estar vacío (P2DFont::loadFont); P2DFont solo
// lo usa como clave, la textura es siempre bigFont.bti.
BloPane box(const std::string& tag, const std::string& text = std::string())
{
	BloPane p = node(BloType::TextBox, tag);
	p.font    = "standin";
	p.text    = text;
	return p;
}

// Ventana con las cuatro esquinas en `p2b_64.bti`, como la p264 del disco.
BloPane containerWindow(const std::string& tag)
{
	BloPane p    = node(BloType::Window, tag);
	p.texture    = "p2b_64.bti";
	p.otherCorners = { "p2b_64.bti", "p2b_64.bti", "p2b_64.bti" };
	p.winX = p.winY = 0;
	p.winW = p.winH = 32;
	return p;
}

std::string tagf(const char* fmt, int i)
{
	char buf[16];
	std::snprintf(buf, sizeof(buf), fmt, i);
	return buf;
}

// Contenedor de las etiquetas de un diseño: ROOT de 640 por 480 con todo dentro.
struct Kids {
	std::vector<BloPane> v;
	Kids& add(BloPane p)
	{
		v.push_back(std::move(p));
		return *this;
	}
	/// `n` imágenes o cajas con la etiqueta de `fmt` (un %02d).
	Kids& pics(const char* fmt, int from, int n)
	{
		for (int i = from; i < from + n; i++) add(pic(tagf(fmt, i)));
		return *this;
	}
	Kids& boxes(const char* fmt, int from, int n)
	{
		for (int i = from; i < from + n; i++) add(box(tagf(fmt, i)));
		return *this;
	}
};

StandinLayout layout(const char* file, Kids& k)
{
	BloPane root = pane("ROOT");
	root.w       = 640;
	root.h       = 480;
	root.children = std::move(k.v);
	StandinLayout l;
	l.file = file;
	l.panes.push_back(std::move(root));
	return l;
}

// DrawMenu::DrawMenu (drawMenu.cpp): pall, cent, se_c T, `items` filas heNN T con
// iNNl y iNNr P, y una pareja de cursores z00l / z00r P (SpectrumCursorMgr::init
// aborta con cero).
Kids drawMenuSet(int items)
{
	Kids k;
	k.add(pane("pall")).add(pane("cent")).add(box("se_c"));
	k.boxes("he%02d", 0, items);
	for (int i = 0; i < items; i++) {
		k.add(pic(tagf("i%02dl", i)));
		k.add(pic(tagf("i%02dr", i)));
	}
	k.add(pic("z00l")).add(pic("z00r"));
	return k;
}


// Panel con hijos: la jerarquía de los .blo del disco (un ancla con sus cajas de texto).
BloPane with(BloPane parent, std::initializer_list<BloPane> kids)
{
	for (const BloPane& k : kids) parent.children.push_back(k);
	return parent;
}

BloPane sized(BloPane p, int w, int h)
{
	p.w = w;
	p.h = h;
	return p;
}

// Una etiqueta de menos de cuatro letras ('hai', 'fp'): el u32 lleva NUL delante.
std::string tag3(const char* t)
{
	std::string s(t);
	return std::string(4 - s.size(), '\0') + s;
}

// Cursores z00l / z00r (SpectrumCursorMgr::init): una pareja basta.
void cursors(Kids& k) { k.add(pic("z00l")).add(pic("z00r")); }

// ogNitakuMgr::ogNitakuMgr: i00l, i00r, i01l, i01r imágenes y la pareja de cursores.
void nitaku(Kids& k)
{
	k.add(pic("i00l")).add(pic("i00r")).add(pic("i01l")).add(pic("i01r"));
	cursors(k);
}

// El número de tres o cuatro cifras de ogScrResultMgr: `n` etiquetas, de la de millares a la de unidades.
void digitTags(Kids& k, std::initializer_list<const char*> tags)
{
	for (const char* t : tags) k.add(pic(t));
}

StandinLayout tutorialPage(const char* file)
{
	Kids k;
	// Cajas tx00..tx19 opcionales (setPageInfoSub las busca con search(.., false)).
	k.add(box("tx00", "Stand-in text."));
	return layout(file, k);
}

} // namespace

std::vector<StandinLayout> standinBloLayouts()
{
	std::vector<StandinLayout> out;

	{ // ogPause.cpp:37-40: se castea a imagen y se le pone alfa 0.
		Kids k;
		k.add(pic("blck"));
		out.push_back(layout("black.blo", k));
	}
	{ // ogPause.cpp:43-50, 75-77: tres filas (setMenuItemActiveSw(1, ..) y he02/hm02).
		Kids k = drawMenuSet(3);
		k.add(box("yame")).add(box("hm02")).add(pic("back"));
		out.push_back(layout("pause.blo", k));
	}
	{ // ogPause.cpp:56-65, 80-82: dos filas (Sí, No: start(0), selección 1).
		Kids k = drawMenuSet(2);
		k.add(box("yame")).add(box("titl")).add(box("even")).add(pic("back"));
		out.push_back(layout("pause_ok.blo", k));
	}
	{ // drawGameOver.cpp:201-229: go00.. imágenes, al menos una.
		Kids k;
		k.pics("go%02d", 0, 1);
		out.push_back(layout("gameover.blo", k));
	}
	{
		Kids k;
		k.pics("go%02d", 0, 1);
		out.push_back(layout("gameove2.blo", k));
	}
	{ // drawCountDown.cpp:27-57: cd10..cd00 y gath, todas imágenes.
		Kids k;
		k.pics("cd%02d", 0, 11).add(pic("gath"));
		out.push_back(layout("count.blo", k));
	}
	{ // drawContainer.cpp:66-136 y 186-195 (DrawContainer::start, al abrir el menú del
		// contenedor): p264 es una ventana (el disco usa p2b_64.bti en las cuatro esquinas,
		// que está en el juego de texturas) y ws8c, p2c4, ws8u, ws8l imágenes sin textura
		// (setTexture no hace nada con cero texturas).
		Kids k;
		k.add(pane("pall")).add(pic("maru"));
		k.add(containerWindow("p264")).add(pic("ws8c")).add(pic("p2c4")).add(pic("ws8u")).add(pic("ws8l"));
		for (const char* t : { "cu_l", "cu_c", "cu_r", "cusl", "cusc", "cusr", "cl_l", "cl_c", "cl_r", "clsl", "clsc", "clsr", "cc_l", "cc_c",
		                       "cc_r", "ccsl", "ccsc", "ccsr", "y_c_", "y_l_", "y_r_", "ysc_", "ysl_", "ysr_", "cont" })
			k.add(pic(t));
		for (const char* pre : { "rm", "rs", "bm", "bs", "ym", "ys" })
			for (int i = 0; i <= 10; i++) k.add(box(pre + tagf("%02d", i)));
		out.push_back(layout("con01.blo", k));
	}
	{ // drawHurryUp.cpp:34-47. 'root' es el propio P2DScreen, no hace falta panel.
		Kids k;
		for (const char* t : { "hurr", "sund", "hur2", "sun2", "sunl", "sunw" }) k.add(pic(t));
		out.push_back(layout("sund.blo", k));
	}
	{ // drawAccount.cpp:27-75.
		Kids k;
		k.add(pane("blak"));
		k.pics("cl_%d", 0, 5).pics("cc_%d", 0, 5).pics("cr_%d", 0, 5);
		out.push_back(layout("account.blo", k));
	}
	{ // drawGameInfo.cpp:774-807.
		Kids k;
		k.add(pane("stat")).add(pane("goal")).add(pane("suni"));
		for (const char* t : { "sunc", "sun1", "sun2", "moon" }) k.add(pic(t));
		out.push_back(layout("play09.blo", k));
	}
	{ // drawGameInfo.cpp:814-830. `life` es un panel simple: LifeIconCallBack se
		// construye sobre PANETYPE_Pane y con una imagen aborta.
		Kids k;
		k.add(pane("li_i")).add(pane("na_i")).add(pic("navi")).add(pane("life")).add(pic("pin1")).add(pic("pin2"));
		out.push_back(layout("play10.blo", k));
	}
	{ // drawGameInfo.cpp:725-745, 832-840.
		Kids k;
		k.add(pic("piki")).add(pane("w_00")).add(pane("x_00")).add(pic("w_01"));
		for (const char* t : { "c_ml", "c_mc", "c_mr", "c_ll", "c_lc", "c_lr", "c_sl", "c_sc", "c_sr", "c_s1", "dc_c", "dc_l", "dc_r", "dcsc",
		                       "dcsl", "dcsr" })
			k.add(pic(t));
		out.push_back(layout("play_day.blo", k));
	}

	// day30_clock: el texto de la nave (ogScrTutorialMgr, ogMessage.cpp:502-545) y
	// las páginas que abre. Pantalla base: curs y a_bt imágenes, base opcional.
	{
		Kids k;
		k.add(pic("base")).add(pic("curs")).add(pic("a_bt"));
		out.push_back(layout("tu_base.blo", k));
	}
	{ // ogSub.cpp:835-857: maru, ten y han son cajas cuyo texto copia ogMsgCtrlTagMgr
		// en buffers de 8 bytes. Un carácter que no sale en los textos de arriba, para
		// que ninguno de ellos se tome por una pausa.
		Kids k;
		// 'ten' y 'han' son constantes de tres letras: el tag lleva un NUL delante.
		k.add(box("maru", "@")).add(box(std::string("\0ten", 4), "#")).add(box(std::string("\0han", 4), "$"));
		out.push_back(layout("wait_char.blo", k));
	}
	for (const char* f : { "tu_tx00.blo", "tu_tx26.blo", "tu_tx35.blo", "tu_tx37_00.blo", "tu_tx37_01.blo", "tu_tx37_02.blo" })
		out.push_back(tutorialPage(f));
	// The text keys of the stand-in cinemas (standin-cinema.md): TUT_RedOnion in
	// demo04 and TUT_Zenmetu in demo60/61 (ogTutorialData.cpp bloFile_Oniyon,
	// bloFile_Zenmetu).
	for (const char* f : { "tu_tx03.blo", "tu_tx20.blo" })
		out.push_back(tutorialPage(f));

	// captain_death: the day-end chain after the captain falls (coverage/terminals.md
	// T06). Pages of the diary (ogScrMessageMgr), the results screen and its save step
	// (ogSaveMgr and the memory-card and file-select screens it builds in its constructor).
	{ // ogResult.cpp, ogScrResultMgr::ogScrResultMgrSub: account2.blo. The panes are named
		// by the search(.., true) calls; the digits are pictures (NumberPicCallBack), the diary
		// template `getp` a text box. b_00.. (DrawCMbest) and pk00.. (PikaAlphaMgr) are optional.
		Kids k;
		for (const char* t : { "po_1", "po2u", "po2d", "po3u", "po3c", "po3d", "p_rc", "p_bc", "p_yc" }) k.add(pane(t));
		k.add(sized(pane("g_sp"), 375, 61));
		k.add(pic("data")).add(pic("da_1")).add(pic("da_2")).add(pic("da_3"));
		k.add(box("getp", "000"));
		digitTags(k, { "4_rc", "rc_l", "rc_c", "rc_r", "4_bc", "bc_l", "bc_c", "bc_r", "4_yc", "yc_l", "yc_c", "yc_r" });
		digitTags(k, { "tp_4", "tp_l", "tp_c", "tp_r", "4_bo", "bo_l", "bo_c", "bo_r", "4_ba", "ba_l", "ba_c", "ba_r" });
		digitTags(k, { "4_sd", "sd_l", "sd_c", "sd_r", "4bo2", "bo2l", "bo2c", "bo2r", "4ba2", "ba2l", "ba2c", "ba2r" });
		digitTags(k, { "4sd2", "sd2l", "sd2c", "sd2r", "dc_l", "dc_c", "dc_r", "ro_l", "ro_c", "ro_r", "ti_l", "ti_c", "ti_r" });
		out.push_back(layout("account2.blo", k));
	}
	{ // ogSave.cpp, ogSaveMgr::ogSaveMgr: ac_save.blo. he00 and he01 sit under hs00 and hs01 on the disc.
		Kids k;
		k.add(pic("back")).add(pic("1win")).add(box("hs00", "Stand-in text.")).add(box("he00", "Stand-in text."));
		k.add(box("hs01", "Stand-in text.")).add(box("he01", "Stand-in text."));
		k.add(box("sa_s", "Stand-in text.")).add(box("saks", "Stand-in text."));
		k.add(box("se_c", "Stand-in text.")).add(box("sa_c", "Stand-in text.")).add(box("sakc", "Stand-in text."));
		k.add(pic("chui"));
		nitaku(k);
		out.push_back(layout("ac_save.blo", k));
	}
	{ // ogSave.cpp: ac_save2.blo (he00, he01, 2win, back).
		Kids k;
		k.add(pic("back")).add(pic("2win")).add(box("he00", "Stand-in text.")).add(box("he01", "Stand-in text.")).add(box("se_c", "Stand-in text."));
		nitaku(k);
		out.push_back(layout("ac_save2.blo", k));
	}
	{ // ogMemChk.cpp, ogScrMemChkMgr::ogScrMemChkMgr: data_m.blo. Every message is a text box
		// (TypingTextMgr copies the text), the yes/no pair sits in yn_w, the rest are pictures.
		Kids k;
		for (const char* t : { "shom", "shot", "shch", "shoi", "shok", "shos", "shuf", "shsi", "shxx", "sari", "memo", "brom", "kaim", "ijom", "naim", "file", "se_c" })
			k.add(box(t, "Stand-in text."));
		k.add(pic("abtn")).add(pic("main")).add(pic("curs")).add(pane("cpsl"));
		k.add(with(pane("yn_w"), { box(tag3("cws"), "Stand-in text."), box(tag3("hai"), "Stand-in text."), box("fomt", "Stand-in text."),
		                           box("rtry", "Stand-in text."), box(tag3("iie"), "Stand-in text.") }));
		k.add(box("haic", "Stand-in text.")).add(box("iiec", "Stand-in text."));
		nitaku(k); // ogNitakuMgr over this screen
		out.push_back(layout("data_m.blo", k));
	}
	// The diary page of a day that ends with the captain down (ResultFlags::getDocument).
	out.push_back(tutorialPage("re_j_00.blo"));
	{ // ogMakeDefault.cpp, ogScrMakeDefaultMgr::ogScrMakeDefaultMgr: data_pf.blo.
		Kids k;
		k.add(box("tx00", "Stand-in text.")).add(box("pfok", "Stand-in text.")).add(box("pfxx", "Stand-in text."));
		k.add(pic("tx01")).add(pic("tx02")).add(pic("tx03")).add(pic("abtn")).add(pic("curs"));
		out.push_back(layout("data_pf.blo", k));
	}
	{ // ogFileChkSel.cpp: data_b.blo (back).
		Kids k;
		k.add(pic("back")).add(pane("re_p"));
		out.push_back(layout("data_b.blo", k));
	}
	for (int slot = 1; slot <= 3; slot++) { // ogFileSelect.cpp, getPane_FileIcon: data1..data3.blo.
		Kids k;
		for (const char* t : { "ce_p", "efmc", "ef_c" }) k.add(pane(t));
		k.add(pic("newi")).add(pic("nemi")).add(pic("sm_c"));
		if (slot == 1) k.add(pic("no_c")).add(pic("ne_c")); // only mSlotScreensData[0] is searched for them
		k.add(with(pane("fp_m"), { pic("dcmc"), pic("dcml"), pic("dcmr") }));
		k.add(with(pane(tag3("fp")), { pic("dc_c"), pic("dc_l"), pic("dc_r") }));
		out.push_back(layout(("data" + std::to_string(slot) + ".blo").c_str(), k));
	}
	for (int slot = 1; slot <= 3; slot++) {
		Kids k; // data1_n.blo..data3_n.blo: only ROOT is looked up; the disc's panes are the icons' pieces.
		k.add(pic("ce_p"));
		out.push_back(layout(("data" + std::to_string(slot) + "_n.blo").c_str(), k));
	}
	{ // ogFileSelect.cpp, getPane_FileTop1: data_t1.blo.
		Kids k;
		k.add(pic("back")).add(pic("chui")).add(pic("p_sh")).add(pic("p_co")).add(pic("p_ma"));
		k.add(with(pane("yn_w"), { box(tag3("hai"), "Stand-in text."), box(tag3("iie"), "Stand-in text.") }));
		for (const char* t : { "dono", "doko", "d1co", "u1co", "copc", "dcop", "dcxx", "hsho", "usho", "savc", "ddxx", "dosa", "dasa", "uasa", "dsho", "se_c" })
			k.add(box(t, "Stand-in text."));
		k.add(box(tag3("sho"), "Stand-in text."));
		nitaku(k);
		out.push_back(layout("data_t1.blo", k));
	}
	{ // ogFileSelect.cpp, getPane_FileTop2: data_t2.blo.
		Kids k;
		for (const char* t : { "da_1", "da_2", "da_3", "ro_l", "ro_r", "rt_l", "rt_r", "rc_l", "rc_c", "rc_r", "bc_l", "bc_c", "bc_r", "yc_l", "yc_c",
		                       "yc_r", "rcon", "bcon", "ycon", "dall", "navi" })
			k.add(pic(t));
		k.add(box("newd", "Stand-in text."));
		out.push_back(layout("data_t2.blo", k));
	}
	{ // ogFileSelect.cpp, getPane_CpyCurScreen: data_i.blo (i00u..i02d).
		Kids k;
		for (const char* t : { "i00u", "i00d", "i01u", "i01d", "i02u", "i02d" }) k.add(pic(t));
		cursors(k);
		out.push_back(layout("data_i.blo", k));
	}
	{ // drawSaveFailure.cpp: save_x.blo (saxx, back).
		Kids k;
		k.add(pic("saxx")).add(pic("back"));
		out.push_back(layout("save_x.blo", k));
	}

	return out;
}

PcrParticle inertPcrParticle()
{
	PcrParticle p;
	p.billboard         = true;
	p.colour.blendMode  = 0;
	p.colour.duration   = 30;
	p.colour.flags      = 0;
	p.colour.thresholds = { 0.0f, 1.0f };
	p.colour.primary    = { { 255, 255, 255, 255 }, { 255, 255, 255, 0 } };
	p.colour.env        = { { 0, 0, 0, 255 }, { 0, 0, 0, 0 } };
	p.flags             = kPcrDisableEmission;
	p.maxFrame          = 1;
	p.maxPasses         = 1;
	return p;
}

std::vector<std::uint8_t> inertPcrPayload()
{
	std::vector<std::uint8_t> out;
	std::string error;
	writePcr(inertPcrParticle(), out, error); // las constantes de arriba siempre son válidas
	return out;
}

} // namespace nectar
