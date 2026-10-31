// standin_showcase: object sandbox with HUD visible; used for 640x480 art captures.
#include "scenarios/pc_scenario_list.h"

#if PIKI_SCENARIOS

#include "scenarios/scenario_progress.h"
#include "NaviMgr.h"

namespace {

class StandinShowcase : public PcScenario {
public:
	const char* name() const override { return "standin_showcase"; }

	void onStageReady() override
	{
		Navi* navi = naviMgr->getNavi(0);
		scenario_progress::spawnPiki(navi, 0, 0, 0);
		scenario_progress::spawnPiki(navi, 1, 0, 1);
		scenario_progress::spawnPiki(navi, 2, 0, 2);
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
