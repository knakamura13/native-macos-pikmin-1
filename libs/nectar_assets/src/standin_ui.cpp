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

// Any tag up to four characters, NUL-padded to four bytes (P2DPane tags).
std::string tag4(const char* t)
{
	std::string s(t);
	if (s.size() > 4) s.resize(4);
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
		k.add(pic("base")).add(pic("capu")).add(pic("curs")).add(pic("a_bt"));
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
	// captain_damage: DEMOFLAG_OlimarLowHealth asks for TUT_APunchUFO (navi.cpp
	// startDamageEffect), whose page is bloFile_APunchUFO.
	out.push_back(tutorialPage("tu_tx31.blo"));

	// bomb_firsts, nectar_growth, carry_blocked: the texts the game opens by itself, with no cinema
	// (ogTutorialData.cpp): TUT_BombInfo (bloFile_BombInfo, two pages) after the first bomb
	// explodes (DemoFlags::update), TUT_Mitu (bloFile_Mitu) at the first flower from nectar
	// (PikiGrowupState::procAnimMsg) and TUT_Rute (bloFile_Rute) when a carry has no way
	// (ActTransport::moveGuruGuru).
	for (const char* f : { "tu_tx27_00.blo", "tu_tx27_01.blo", "tu_tx29.blo", "tu_tx30.blo" })
		out.push_back(tutorialPage(f));
	// ufo_part_discovery: movie 12 is no cinema, it asks for TUT_GetParts directly
	// (MoviePlayer::startMovie), which with SHIPTEXT_PartDiscovery opens the part's
	// "get only" page (bloFile_PartsGetOnly_*). One page per part; the Main Engine's is tu_tx14.
	for (const char* f : { "tu_tx14.blo",  "ufo_a_0.blo", "ufo_b_0.blo", "ufo_c_0.blo", "ufo_d_0.blo", "ufo_e_0.blo",  "ufo_f_0.blo",  "ufo_g_0.blo",
	                       "ufo_h_0.blo",  "ufo_i_0.blo", "ufo_j_0.blo", "ufo_k_0.blo", "ufo_l_0.blo", "ufo_m_0.blo",  "ufo_n_0.blo",  "ufo_o_0.blo",
	                       "ufo_p_0.blo",  "ufo_q_0.blo", "ufo_r_0.blo", "ufo_s_0.blo", "ufo_t_0.blo", "ufo_u_0.blo",  "ufo_v_0.blo",  "ufo_w_0.blo",
	                       "ufo_x_0.blo",  "ufo_y_0.blo", "ufo_z_0.blo", "ufo_a1_0.blo", "ufo_b1_0.blo", "ufo_c1_0.blo" })
		out.push_back(tutorialPage(f));

	// ship_part_carry: delivering the Whimsical Radar (uf01) opens its "get" text
	// (bloFile_PartsGet_uf01: auf_f_0 and tu_tx24) from the demo78 stand-in's TUT_GetParts key.
	for (const char* f : { "auf_f_0.blo", "tu_tx24.blo" })
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
	// First Pikmin death diary page (RESFLAG path from ogResult.cpp re_k_00).
	out.push_back(tutorialPage("re_k_00.blo"));
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

	// day_end: the diary pages of an ordinary sunset (ResultFlags::getDocument, resultFlag.cpp
	// flagTable). With no other flag on, the lowest priority wins: RESFLAG_Misc1 (re_n1_00) on
	// Day 2, RESFLAG_FinalDay (re_g_00, set by cleanupDayEnd on Day 29) on Day 29. One page each.
	// onion_day3: the first yellow pluck sets RESFLAG_MeetYellowPikminNoBomb (priority 206, re_q_00).
	for (const char* f : { "re_n1_00.blo", "re_g_00.blo", "re_q_00.blo" })
		out.push_back(tutorialPage(f));

	// extinction_day30: the final results window that DayOverModeState::makeTotalScoreWindow
	// builds on Day 30 (bad ending). Constructed in phase two whether or not it is ever started
	// (the disc's demo73 starts it with a StartTotalResults key; the stand-in tree has no demo73).
	{ // drawUfoParts.cpp: ufo_p.blo. The constructor needs only the screen (DrawCMbest falls back to
		// the screen's own 'root'); dataSet, run once the window is operated, looks up ui01..ui30
		// pictures and un01..un30 text boxes. b_00 is one DrawCMbest picture (the count stops at the
		// first missing b_NN).
		Kids k;
		k.pics("ui%02d", 1, 30).boxes("un%02d", 1, 30);
		k.add(pic("b_00"));
		out.push_back(layout("ufo_p.blo", k));
	}
	{ // drawFinalResult.cpp, DrawTotalScore::DrawTotalScore: f_result.blo. Every search(.., true) of
		// the constructor: win1..win4 anchors, the digits (NumberPicCallBack on pictures), pd_s and
		// newd pictures (setRankInColor reads their colours), the record rows pd/lp/dp01..05 (text
		// boxes copied into 128-byte buffers) and pa_k, pa_d, lp_h, dp_h text boxes. 'best' anchors
		// DrawCMbest with one b_NN picture.
		Kids k;
		for (const char* t : { "win1", "win2", "win3", "win4" }) k.add(pane(t));
		digitTags(k, { "pa_l", "pa_r", "da_l", "da_r", "lp_4", "lp_3", "lp_2", "lp_1", "dp_4", "dp_3", "dp_2", "dp_1" });
		digitTags(k, { "tp_8", "tp_7", "tp_6", "tp_5", "tp_4", "tp_3", "tp_2", "tp_1" });
		k.add(pic("pd_s")).add(pic("newd"));
		for (const char* t : { "pa_k", "pa_d", "lp_h", "dp_h" }) k.add(box(t, "00"));
		for (const char* fmt : { "pd%02d", "lp%02d", "dp%02d" })
			for (int i = 1; i <= 5; i++) k.add(box(tagf(fmt, i), "Stand-in text."));
		k.add(with(pane("best"), { pic("b_00") }));
		out.push_back(layout("f_result.blo", k));
	}
	// DrawFinalResult's save step (DrawOptionSave): DrawSaveMes::DrawSaveMes builds these two; its
	// ogScrMemChkMgr reuses data_m.blo above.
	{ // drawSaveMes.cpp: cha_sav2.blo (savk, save, sakc, sa_c, saks, sa_s text boxes, abtn picture).
		Kids k;
		for (const char* t : { "savk", "save", "sakc", "sa_c", "saks", "sa_s" }) k.add(box(t, "Stand-in text."));
		k.add(pic("abtn"));
		out.push_back(layout("cha_sav2.blo", k));
	}
	{ // drawSaveMes.cpp: ac_chui.blo (back and chui pictures).
		Kids k;
		k.add(pic("back")).add(pic("chui"));
		out.push_back(layout("ac_chui.blo", k));
	}

	{ // drawGameInfo.cpp, DrawGameInfo::DrawGameInfo(MODE_Challenge): the challenge-mode HUD,
		// play_cha.blo in place of play_day.blo. The same lookups as play_day.blo except the date
		// (dc_*, story mode only).
		Kids k;
		k.add(pic("piki")).add(pane("w_00")).add(pane("x_00")).add(pic("w_01"));
		for (const char* t : { "c_ml", "c_mc", "c_mr", "c_ll", "c_lc", "c_lr", "c_sl", "c_sc", "c_sr", "c_s1" }) k.add(pic(t));
		out.push_back(layout("play_cha.blo", k));
	}

	// challenge_day_end, challenge_captain_death, challenge_extinction: the challenge results
	// window (zen::DrawCMresult, built by DayOverModeState::initialisePhaseTwo in challenge mode).
	// Its constructor opens three screens; its save step (DrawOptionSave) reuses cha_sav2,
	// ac_chui and data_m above. Tags and types are every search(.., true) of the constructor and
	// of what it builds, the same as on the disc's files (blo_writer_test with NECTAR_DISC_DATA).
	{ // drawCMresult.cpp, drawCMtitle.cpp, ogGraph.cpp: cha_re.blo. 'titl' anchors the title
		// (DrawCMtitleObj) with its 'mark' (a BalloonPane); the score digits bigl..bigr and
		// sbil..sbir are pictures (NumberPicCallBack); p_m0..p_m4 are the five course names, one
		// shown (start); 'bi_w' places the new-record effect; 'g_sp' is the graph area (ogGraphMgr
		// reads its size).
		Kids k;
		k.add(with(pane("titl"), { pic("mark") }));
		digitTags(k, { "sbil", "sbic", "sbir", "bigl", "bigc", "bigr" });
		k.add(pic("bi_w"));
		k.add(sized(pane("g_sp"), 384, 64));
		for (int i = 0; i < 5; i++) k.add(pane(tagf("p_m%d", i)));
		out.push_back(layout("cha_re.blo", k));
	}
	{ // drawCMscore.cpp, DrawCMscoreObj::init for ranks 1..5 (MEMORY_BEST_SCORE): an 'Nban'
		// anchor, its digits Ni_l, Ni_c, Ni_r (pictures) and the new-rank mark mN_w (a picture,
		// createRankInEffect), all under 'rank' (DrawCMresult moves it).
		Kids k;
		BloPane rank = pane("rank");
		for (int n = 1; n <= 5; n++) {
			rank.children.push_back(with(pane(tagf("%dban", n)), { pic(tagf("m%d_w", n)), pic(tagf("%di_l", n)), pic(tagf("%di_c", n)), pic(tagf("%di_r", n)) }));
		}
		k.add(rank);
		out.push_back(layout("cha_rank.blo", k));
	}
	{ // drawCMbest.cpp: cha_best.blo. 'best' with b_00 (DrawCMbest counts b_NN until one is missing).
		Kids k;
		k.add(with(pane("best"), { pic("b_00") }));
		out.push_back(layout("cha_best.blo", k));
	}
	// The tutorial scenarios (tools/scenarios/standin_practice_stage.h): DEMOFLAG_Pluck15thPikmin
	// opens TUT_NukiAndFree, two pages (ogTutorialData.cpp bloFile_NukiAndFree); the ship-upgrade
	// movie after the Main Engine (stand-in demo17) opens TUT_GetEngine (bloFile_GetEngine); the
	// tutorial's day end writes the Day 1 diary entry RESFLAG_EndFirstDay, two pages (ogResult.cpp
	// bloFile_Res_Table: re_a_00, re_a_01).
	for (const char* f : { "tu_tx38_00.blo", "tu_tx38_01.blo", "tu_tx15.blo", "re_a_00.blo", "re_a_01.blo" })
		out.push_back(tutorialPage(f));

	// World map, title and file-select screens (#99). Tags match search(.., true) in
	// drawWorldMap.cpp, drawWMPause.cpp, ogTitle.cpp, ogStart.cpp, drawHiScore.cpp,
	// ogDiary.cpp (m_menu_r).
	auto wmConfirm = []() {
		Kids k = drawMenuSet(2);
		k.add(pic("z**l")).add(pic("z**r"));
		return k;
	};
	{
		Kids k;
		k.pics("wp%02d", 0, 4);
		out.push_back(layout("w_wipe.blo", k));
	}
	{
		Kids k;
		k.add(pic(tag4("ri"))).add(pic("ci_b")).add(pic("ci_r")).add(pic("ci_y"));
		out.push_back(layout("w_icon.blo", k));
	}
	{
		Kids k;
		for (int i = 1; i <= 5; i++) k.add(pic(tagf("map%d", i)));
		out.push_back(layout("w_title.blo", k));
	}
	{
		Kids k;
		for (int i = 1; i <= 5; i++) k.add(pic(tagf("mi_%d", i)));
		out.push_back(layout("w_moni.blo", k));
	}
	{
		Kids k;
		for (const char* t : { "dc_c", "dc_l", "dc_r", "ro_l", "ro_c", "ro_r", "rt_l", "rt_c", "rt_r", "bp_l", "bp_c", "bp_r", "rp_l", "rp_c", "rp_r",
		                       "yp_l", "yp_c", "yp_r", "p_bc", "p_rc", "p_yc" })
			k.add(pic(t));
		out.push_back(layout("w_data1.blo", k));
	}
	{
		Kids k;
		for (int i = 0; i < 13; i++) {
			k.add(pic(tagf("pa%02d", i)));
			k.add(pic(tagf("pn%02d", i)));
		}
		out.push_back(layout("w_data2.blo", k));
	}
	{
		Kids k;
		k.add(pic(tag4("x_1"))).add(pic("x_1s")).add(pic("po01"));
		for (int i = 2; i <= 5; i++) {
			char bx[8], bxs[8], bpo[8];
			std::snprintf(bx, sizeof bx, "x_%d", i);
			std::snprintf(bxs, sizeof bxs, "x_%ds", i);
			std::snprintf(bpo, sizeof bpo, "po%02d", i);
			k.add(pic(tag4(bx)));
			k.add(pic(bxs));
			k.add(pic(bpo));
		}
		for (int i = 1; i <= 5; i++) {
			k.add(pic(tagf("sli%d", i)));
			k.add(pane(tagf("ony%d", i)));
			k.add(pane(tagf("cha%d", i)));
		}
		out.push_back(layout("w_point.blo", k));
	}
	{
		Kids k;
		for (int i = 1; i <= 5; i++) k.add(pic(tagf("sli%d", i)));
		out.push_back(layout("w_line.blo", k));
	}
	{
		Kids k;
		out.push_back(layout("w_back.blo", k));
	}
	{
		Kids k = wmConfirm();
		out.push_back(layout("w_ok.blo", k));
	}
	{
		Kids k = drawMenuSet(2);
		k.add(pic("back"));
		out.push_back(layout("pause_w.blo", k));
	}
	{
		Kids k;
		k.add(pic("abtn"));
		for (int i = 0; i < 30; i++) {
			k.add(pic(tagf("i%02du", i)));
			k.add(pic(tagf("i%02dd", i)));
			k.add(pane(tagf("pi%02d", i)));
		}
		for (int i = 14; i <= 43; i++) k.add(pic(tagf("pk%02d", i)));
		cursors(k);
		out.push_back(layout("m_menu_r.blo", k));
	}
	{
		Kids k;
		out.push_back(layout("m_menu_b.blo", k));
	}
	{
		Kids k;
		k.add(pane("ttal")).add(pane("char"));
		k.add(with(pane("best"), { pic("b_00") }));
		for (int i = 0; i < 5; i++) {
			k.add(box(tagf("p_%02d", i), "00"));
			k.add(box(tagf("d_%02d", i), "00"));
			k.add(box(tagf("lp%02d", i), "0000"));
			k.add(box(tagf("dp%02d", i), "0000"));
			k.add(pane(tagf("pm_%d", i)));
		}
		for (int j = 0; j < 5; j++)
			for (int i = 0; i < 5; i++) {
				char buf[8];
				std::snprintf(buf, sizeof buf, "%d_%02d", j, i);
				k.add(box(buf, "000"));
			}
		out.push_back(layout("cha_hi.blo", k));
	}
	{
		Kids k;
		k.add(pane("root"));
		out.push_back(layout("press_s.blo", k));
	}
	auto titleMenu = [](int items) {
		Kids k = drawMenuSet(items);
		k.add(pic("back"));
		return k;
	};
	{
		Kids k = titleMenu(2);
		out.push_back(layout("m_select.blo", k));
	}
	{
		Kids k = titleMenu(3);
		out.push_back(layout("m_selec2.blo", k));
	}
	{
		Kids k = titleMenu(4);
		out.push_back(layout("option.blo", k));
	}
	{
		Kids k = titleMenu(2);
		k.add(pic("on21")).add(pic("on22")).add(box("on_c", "on")).add(box("offc", "off"));
		for (int i = 1; i <= 10; i++) k.add(pic(tagf("on%02d", i)));
		for (int i = 11; i <= 20; i++) k.add(pic(tagf("on%02d", i)));
		out.push_back(layout("s_select.blo", k));
	}
	{
		Kids k = titleMenu(2);
		out.push_back(layout("v_select.blo", k));
	}
	{
		Kids k = titleMenu(2);
		out.push_back(layout("ms_selec.blo", k));
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
