// throw_release: one captain, Pikmin in grab range. Three cases (a calibration
// that holds A until the grab completes and measures K, a release one tick
// after the press, a release K + 2 ticks after it). Each asserts its
// preconditions, then only behaviour that holds with or without upstream #209:
// the Pikmin is Hanged before the captain enters Throw, the captain enters
// Throw, the Pikmin flies and lands (Normal), and the captain returns to Walk.
// All within tick bounds. The before-keyframe timing check is not in v1.
#include "scenarios/pc_scenario_list.h"

#if PIKI_SCENARIOS

#include <cmath>
#include <cstdio>
#include <vector>

#include "CPlate.h"
#include "Dolphin/pad.h"
#include "GameStat.h"
#include "MapMgr.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiState.h"
#include "keyconfig.h"

namespace {

constexpr int kPikminCount = 3;
constexpr int kBlue        = 0;
constexpr unsigned kAwaySeconds = 4;

class ThrowRelease : public PcScenario {
public:
	const char* name() const override { return "throw_release"; }

	void onStageReady() override
	{
		if (KeyConfig::_instance == nullptr || KeyConfig::_instance->mThrowKey.mBind != KBBTN_A) {
			scenarioFailf(0, "throw key bound to A", "bind 0x%x", KeyConfig::_instance ? KeyConfig::_instance->mThrowKey.mBind : -1);
			return;
		}
		Navi* navi = naviMgr->getNavi();
		if (navi == nullptr) {
			scenarioFailf(0, "a captain", "none");
			return;
		}
		// Skip the ship walk-out: Walk holds before the first scripted input.
		navi->mStateMachine->transit(navi, NAVISTATE_Walk);
		// About 45 units from the captain on a circle, so selection finds them
		// (grab range is 80) and they do not sit on each other.
		for (int i = 0; i < kPikminCount; ++i) {
			Piki* piki = static_cast<Piki*>(pikiMgr->birth());
			if (piki == nullptr) {
				scenarioFailf(0, "pikmin spawned", "birth returned null at %d", i);
				return;
			}
			GameStat::workPikis.inc(kBlue);
			piki->init(navi);
			const float a = 6.2831853f * (float)i / (float)kPikminCount;
			Vector3f at   = navi->mSRT.t;
			at.x += 45.0f * std::cos(a);
			at.z += 45.0f * std::sin(a);
			at.y = mapMgr->getMinY(at.x, at.z, true);
			piki->Creature::init(at);
			piki->initColor(kBlue);
			piki->changeMode(PikiMode::FormationMode, navi);
			mPikis.push_back(piki);
		}
		GameStat::update();
	}

	void providePads(unsigned, PcInputPad& pad) override
	{
		// The captain starts beside the ship, where A enters it. Walk away first.
		if (mPhase == Away) pad.stickY = 100;
		pad.button = mHoldA ? PAD_BUTTON_A : 0;
	}

	void onTick(unsigned t) override
	{
		Navi* navi = naviMgr->getNavi();
		const int cap = navi->getCurrState()->getID();
		const int rate = pc_scenario_ticks_per_second();
		const unsigned bound = 5u * rate; // five seconds per step

		// While the grab is held, note the Pikmin in the hand and whether it was Hanged.
		if (cap == NAVISTATE_ThrowWait) {
			NaviThrowWaitState* wait = static_cast<NaviThrowWaitState*>(navi->getCurrState());
			if (wait->mIsHoldingThrowPiki && wait->mHeldThrowPiki && wait->mHeldThrowPiki->getState() == PIKISTATE_Hanged) mSawHanged = true;
		}
		switch (mPhase) {
		case Away:
			if (t + 1 >= kAwaySeconds * (unsigned)rate) {
				mPhase     = Settle;
				mStepStart = t;
			}
			break;
		case Settle:
			// Captain in Walk and at least one Pikmin in Normal in the plate.
			if (cap == NAVISTATE_Walk && plateNormalCount(navi) > 0) {
				mPress = t + 1;
				mHoldA = true;
				mPhase = Held;
				mThrown = nullptr;
				mSawHanged = false;
				mSawFlying = false;
				mThrownEvent = false;
				mStepStart = t;
			} else if (t - mStepStart > 2 * bound) {
				scenarioFailf(t, "captain Walk with a Normal Pikmin in the plate", "captain state %d, %d in plate", cap, plateNormalCount(navi));
			}
			break;
		case Held: {
			if (t == mPress && cap != NAVISTATE_ThrowWait) {
				scenarioFailf(t, "ThrowWait the tick after the press", "captain state %d", cap);
				return;
			}
			bool release = false;
			if (mCase == 0) {
				if (cap == NAVISTATE_ThrowWait && static_cast<NaviThrowWaitState*>(navi->getCurrState())->mIsHoldingThrowPiki) {
					mK = t - mPress;
					release = true;
					if (mK < 3) {
						scenarioFailf(t, "K >= 3 (grab takes at least 3 ticks)", "K = %u", mK);
						return;
					}
					std::printf("[scenario] throw_release calibration: K = %u ticks\n", mK);
				} else if (t - mPress > bound) {
					scenarioFailf(t, "grab completes", "mIsHoldingThrowPiki never set within %u ticks", bound);
					return;
				}
			} else if (mCase == 1) {
				release = (t == mPress); // A down on tick P only
			} else {
				release = (t == mPress + mK + 1); // released K + 2 ticks after the press
			}
			if (release) {
				mHoldA     = false;
				mPhase     = WaitThrow;
				mStepStart = t;
			}
			break;
		}
		case WaitThrow:
			if (cap == NAVISTATE_Throw) {
				// The Throw state names the Pikmin it throws.
				mThrown = static_cast<NaviThrowState*>(navi->getCurrState())->mTargetPiki;
				if (mThrown == nullptr) {
					scenarioFailf(t, "the Throw state names a Pikmin", "none");
					return;
				}
				// A release before the grab keyframe completes the grab inside the
				// same tick that enters Throw, so Hanged is visible only when the
				// release came after the grab (cases 0 and 2).
				if (mCase != 1 && !mSawHanged) {
					scenarioFailf(t, "Pikmin Hanged before the captain enters Throw", "never Hanged during ThrowWait (state %d)", mThrown->getState());
					return;
				}
				if (mThrown->getState() == PIKISTATE_Flying) {
					scenarioFailf(t, "Pikmin not yet Flying when Throw begins", "Flying");
					return;
				}
				mPhase     = WaitFlying;
				mStepStart = t;
			} else if (t - mStepStart > bound) {
				scenarioFailf(t, "captain enters Throw", "captain state %d after %u ticks", cap, bound);
			}
			break;
		case WaitFlying:
			// The throw flies from the captain's feet (the cursor sits there), so the
			// flight can be shorter than a tick. NaviThrowState::mHasThrownPiki is
			// set on KEY_Action0, the same event that sends the Pikmin to Flying.
			if (mThrown->getState() == PIKISTATE_Flying) mSawFlying = true;
			if (cap == NAVISTATE_Throw && static_cast<NaviThrowState*>(navi->getCurrState())->mHasThrownPiki) mThrownEvent = true;
			if (mThrownEvent || mSawFlying) {
				mPhase     = WaitLanded;
				mStepStart = t;
			} else if (t - mStepStart > bound) {
				scenarioFailf(t, "Pikmin leaves the hand (thrown)", "Pikmin state %d, captain state %d, no throw event", mThrown->getState(), cap);
			}
			break;
		case WaitLanded:
			if (mThrown->getState() == PIKISTATE_Normal) {
				mPhase     = WaitWalk;
				mStepStart = t;
			} else if (t - mStepStart > 2 * bound) {
				scenarioFailf(t, "Pikmin lands (Normal)", "Pikmin state %d", mThrown->getState());
			}
			break;
		case WaitWalk:
			if (cap == NAVISTATE_Walk) {
				std::printf("[scenario] throw_release case %d done at tick %u\n", mCase, t);
				if (++mCase == 3) {
					scenarioPass();
					return;
				}
				mPhase     = Settle;
				mStepStart = t;
			} else if (t - mStepStart > bound) {
				scenarioFailf(t, "captain returns to Walk", "captain state %d", cap);
			}
			break;
		}
	}

private:
	enum Phase { Away, Settle, Held, WaitThrow, WaitFlying, WaitLanded, WaitWalk };

	int plateNormalCount(Navi* navi)
	{
		int n = 0;
		CPlate* plate = navi->mPlateMgr;
		for (int i = plate->getFirst(); !plate->isDone(i); i = plate->getNext(i)) {
			Creature* c = plate->getCreature(i);
			if (c != nullptr && c->mObjType == OBJTYPE_Piki && static_cast<Piki*>(c)->getState() == PIKISTATE_Normal) ++n;
		}
		return n;
	}

	Phase mPhase        = Away;
	int mCase           = 0;
	unsigned mPress     = 0;
	unsigned mK         = 0;
	unsigned mStepStart = 0;
	bool mHoldA         = false;
	bool mSawHanged     = false;
	bool mSawFlying     = false;
	bool mThrownEvent   = false;
	Piki* mThrown       = nullptr;
	std::vector<Piki*> mPikis;
};

} // namespace

PcScenario* pc_scenario_make_throw_release(void)
{
	static ThrowRelease s;
	return &s;
}

#endif
