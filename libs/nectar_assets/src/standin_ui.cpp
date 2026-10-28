#include "nectar_assets/standin_ui.h"

#include <cstdio>

namespace nectar {

namespace {

// Tipos como en la tabla de etiquetas de #72: P imagen, B panel simple, T caja de
// texto. Donde el juego acepta cualquier tipo se usa un panel simple, que es lo
// más inerte. Sin ventanas: P2DWindow::loadResource desreferencia sus texturas.

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
	{ // drawContainer.cpp:66-136. Sin p264 ni ws8c, p2c4, ws8u, ws8l: solo los busca
		// DrawContainer::start al abrir el menú del contenedor, fuera del arranque.
		Kids k;
		k.add(pane("pall")).add(pic("maru"));
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
