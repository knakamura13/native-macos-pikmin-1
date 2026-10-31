// standin_showcase: flat prop layout + fixed camera for 640x480 stand-in art captures.
#include "scenarios/pc_scenario_list.h"

#if PIKI_SCENARIOS

#include "GlobalGameOptions.h"
#include "GoalItem.h"
#include "ItemMgr.h"
#include "NaviMgr.h"
#include "UfoItem.h"
#include "MapMgr.h"
#include "Camera.h"
#include "scenarios/scenario_progress.h"
#include "scenarios/pc_scenario.h"

namespace {

// Keep in sync with standin_showcase_stage.h spawn coordinates.
constexpr float kShowcaseShipX = 26.0f;
constexpr float kShowcaseShipZ = 36.0f;

void snapShowcaseShip()
{
	if (!itemMgr) return;
	UfoItem* ufo = itemMgr->getUfo();
	if (!ufo) return;
	Vector3f p(kShowcaseShipX, 0.0f, kShowcaseShipZ);
	if (mapMgr) p.y = mapMgr->getMinY(p.x, p.z, true);
	ufo->mSRT.t = p;
}

void updateShowcaseCamera(Navi* navi)
{
	if (!navi) return;
	Vector3f focus = navi->mSRT.t;
	focus.x += 2.0f;
	focus.z += 14.0f;
	focus.y += 12.0f;
	Vector3f eye(focus.x - 24.0f, focus.y + 32.0f, focus.z - 58.0f);
	PcScenarioDebugCamera cam {};
	cam.active  = true;
	cam.eyeX    = eye.x;
	cam.eyeY    = eye.y;
	cam.eyeZ    = eye.z;
	cam.focusX  = focus.x;
	cam.focusY  = focus.y;
	cam.focusZ  = focus.z;
	cam.fov     = 36.0f;
	pc_scenario_set_debug_camera(cam);
}

void spawnShowcasePiki(Navi* navi, int colour, PikiHappa happa, int slot)
{
	if (!scenario_progress::spawnPiki(navi, colour, 1, slot)) return;
	Iterator it(pikiMgr);
	CI_LOOP(it)
	{
		Piki* p = static_cast<Piki*>(*it);
		if (!p || p->mColor != colour) continue;
		Vector3f want = navi->mSRT.t;
		want.x += 18.0f * float(slot) - 18.0f;
		want.z += 22.0f + 6.0f * float(slot);
		want.y = p->mSRT.t.y;
		p->mSRT.t = want;
		p->mHappa = happa;
		break;
	}
}

class StandinShowcase : public PcScenario {
public:
	const char* name() const override { return "standin_showcase"; }
	const char* stageFile() const override { return "stages/sandbox_showcase.ini"; }

	void providePads(unsigned /*tick*/, PcInputPad& /*pad0*/) override
	{
		if (Navi* navi = naviMgr ? naviMgr->getNavi(0) : nullptr) updateShowcaseCamera(navi);
	}

	void onStageReady() override
	{
		Navi* navi = naviMgr->getNavi(0);
		if (GoalItem* onion = itemMgr->getContainer(scenario_progress::kRed)) onion->pcSnapLanded();
		snapShowcaseShip();

		spawnShowcasePiki(navi, scenario_progress::kRed, Leaf, 0);
		spawnShowcasePiki(navi, scenario_progress::kYellow, Bud, 1);
		spawnShowcasePiki(navi, scenario_progress::kBlue, Flower, 2);

		updateShowcaseCamera(navi);
	}

	void onTick(unsigned tick) override
	{
		if (tick + 1 >= kTicks) scenarioPass();
	}

private:
	static constexpr unsigned kTicks = 240;
};

} // namespace

PcScenario* pc_scenario_make_standin_showcase(void)
{
	static StandinShowcase s;
	return &s;
}

#endif
