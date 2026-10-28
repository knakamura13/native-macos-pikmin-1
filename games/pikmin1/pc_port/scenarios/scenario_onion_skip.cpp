// onion_skip: the Onion discovery cutscene, watched and skipped (upstream #205).
// Runs on the opt-in object stage, stages/sandbox_objects.ini, whose default.gen
// places a blue Onion the player does not own yet (standin-objects.md). The
// discovery movie is the stand-in demo06.cin (standin-cinema.md): Boot at movie
// frame 10, the first seed from the Onion's boot motion KEY_Action0 at motion
// frame 240 (movie frame about 250), EmitCone at frame 300, 330 frames long.
//
// DemoInvoker starts the discovery only on story stages, so the scenario sets
// DEMOFLAG_DiscoverBlueOnyon itself, as DemoInvoker would (movie 6,
// DEMOID_FindBlueOnyon, target the Onion). Between cases the Onion is put back to
// "not booted" the way GoalItem::startAI does for an Onion without its boot bit
// (goalItem.cpp: speed 0, GOAL_BootInit, cone shrink, colour animation), the
// boot bit is cleared and the demo flag reset, so every case starts from the same
// discovery. Seeds are counted with GameStat::bornPikis, which only
// GoalAI::BootEmit increments.
//
// Cases, in order:
//   watch      no skip: the movie plays to its end
//   pre_boot   START at movie frame 5, before Boot
//   mid_boot   START at movie frame 100, after Boot, before the seed (the #205 bug window)
//   post_seed  START at movie frame 275, after the seed, before EmitCone
//
// Asserted (holds with or without upstream #205): the movie starts and ends;
// the Onion boots (boot bit set) and ends in GOAL_Wait; watch, pre_boot and
// post_seed emit exactly one seed; no case emits more than one.
// Printed, not asserted: the mid_boot seed count. Without #205 the replayed
// EmitCone snaps the Onion from BootInit to Wait before the recovery runs, so no
// seed is born (0); with #205 EmitCone first moves it to BootEmit (1). The line
// says which one the run saw.
#include "scenarios/pc_scenario_list.h"

#if PIKI_SCENARIOS

#include <cstdio>

#include "Demo.h"
#include "Dolphin/pad.h"
#include "GameStat.h"
#include "GoalItem.h"
#include "ItemMgr.h"
#include "MoviePlayer.h"
#include "PikiHeadItem.h"
#include "PlayerState.h"
#include "SimpleAI.h"
#include "gameflow.h"

namespace {

constexpr int kBlue = 0;

struct Case {
	const char* name;
	int skipFrame; ///< movie frame at which START is pressed; < 0: watch to the end
	int expectSeeds; ///< exact count asserted; < 0: printed only (at most 1 asserted)
};
const Case kCases[] = {
	{ "watch", -1, 1 },
	{ "pre_boot", 5, 1 },
	{ "mid_boot", 100, -1 },
	{ "post_seed", 275, 1 },
};
constexpr int kCaseCount = int(sizeof(kCases) / sizeof(kCases[0]));

enum Phase { Start, WaitMovie, Playing, Settle, Done };

const char* stateName(int id)
{
	switch (id) {
	case GoalAI::GOAL_Wait: return "Wait";
	case GoalAI::GOAL_BootInit: return "BootInit";
	case GoalAI::GOAL_BootEmit: return "BootEmit";
	default: return "other";
	}
}

class OnionSkip : public PcScenario {
public:
	const char* name() const override { return "onion_skip"; }

	void onStageReady() override
	{
		mOnion = itemMgr->getContainer(kBlue);
		if (!mOnion) {
			scenarioFailf(0, "the object stage's blue Onion", "itemMgr->getContainer(blue) is null");
			return;
		}
		std::printf("[scenario] onion_skip Onion at (%.0f, %.0f) state %s, boot bit %d, container %d\n", mOnion->mSRT.t.x, mOnion->mSRT.t.z,
		            stateName(goalState()), (int)playerState->hasBootContainer(kBlue), (int)playerState->hasContainer(kBlue));
	}

	void providePads(unsigned, PcInputPad& pad0) override
	{
		if (mPressStart) {
			pad0.button |= PAD_BUTTON_START;
			mPressStart = false;
		}
	}

	void onTick(unsigned t) override
	{
		if (!mOnion) return;
		const int rate      = pc_scenario_ticks_per_second();
		const bool movie    = gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive;
		const Case& c       = kCases[mCase];
		const unsigned span = t - mPhaseTick;

		switch (mPhase) {
		case Start:
			if (movie) return; // a movie from an earlier case still closing
			resetOnion();
			mBorn0 = GameStat::bornPikis[kBlue];
			playerState->mDemoFlags.setFlag(DEMOFLAG_DiscoverBlueOnyon, mOnion);
			enter(WaitMovie, t);
			return;
		case WaitMovie:
			if (movie) {
				mMovieTick = t;
				mPressed   = false;
				enter(Playing, t);
			} else if (span > unsigned(2 * rate)) {
				scenarioFailf(t, "the discovery movie starts within 2 s of the demo flag", "case %s: no movie", c.name);
			}
			return;
		case Playing:
		{
			const int frame = gameflow.mMoviePlayer->mCurrentFrame;
			if (c.skipFrame >= 0 && !mPressed && frame >= c.skipFrame) {
				mPressed    = true;
				mPressStart = true;
				mPressFrame = frame;
				mPressState = goalState();
				mPressBorn  = GameStat::bornPikis[kBlue] - mBorn0;
			}
			if (!movie) {
				mMovieTicks = t - mMovieTick;
				enter(Settle, t);
			} else if (span > unsigned(20 * rate)) {
				scenarioFailf(t, "the discovery movie ends within 20 s", "case %s: still playing at frame %d", c.name, frame);
			}
			return;
		}
		case Settle:
			// Watched to the end, the boot motion (360 frames from Boot at movie frame 10) outlasts the movie by about
			// 40 frames; wait for Wait, then 2 s more for a late seed.
			if (goalState() == GoalAI::GOAL_Wait) {
				if (mWaitTick == 0) mWaitTick = t;
				if (t - mWaitTick >= unsigned(2 * rate)) {
					finishCase(t);
				}
			} else {
				mWaitTick = 0;
				if (span > unsigned(10 * rate)) {
					scenarioFailf(t, "the Onion reaches GOAL_Wait within 10 s of the movie's end", "case %s: state %s (%d)", c.name, stateName(goalState()), goalState());
				}
			}
			return;
		case Done: return;
		}
	}

private:
	int goalState() { return C_SAI(mOnion)->getCurrID(mOnion); }

	void enter(Phase p, unsigned t)
	{
		mPhase     = p;
		mPhaseTick = t;
	}

	/// An undiscovered Onion again: the !hasBootContainer branch of GoalItem::startAI
	/// (goalItem.cpp), without creating its legs' ropes a second time.
	void resetOnion()
	{
		playerState->mContainerFlag &= u8(~(1 << (kBlue + 3)));
		playerState->mDemoFlags.resetFlag(DEMOFLAG_DiscoverBlueOnyon);
		mOnion->setMotionSpeed(0.0f);
		C_SAI(mOnion)->start(mOnion, GoalAI::GOAL_BootInit);
		mOnion->startConeShrink();
		mOnion->enableColorAnim();
		mOnion->mColourFadeRate = 0.0f;
		mWaitTick               = 0;
	}

	void finishCase(unsigned t)
	{
		const Case& c   = kCases[mCase];
		const int seeds = GameStat::bornPikis[kBlue] - mBorn0;
		const bool boot = playerState->hasBootContainer(kBlue);
		if (c.skipFrame >= 0) {
			std::printf("[scenario] onion_skip %s: START at movie frame %d (Onion %s, %d seed(s) so far); movie ran %u ticks; seeds %d; Onion %s; boot bit %d\n", c.name,
			            mPressFrame, stateName(mPressState), mPressBorn, mMovieTicks, seeds, stateName(goalState()), (int)boot);
		} else {
			std::printf("[scenario] onion_skip %s: movie ran %u ticks; seeds %d; Onion %s; boot bit %d\n", c.name, mMovieTicks, seeds, stateName(goalState()), (int)boot);
		}
		if (c.skipFrame >= 0 && !mPressed) {
			scenarioFailf(t, "START pressed during the movie", "case %s: the movie ended before frame %d", c.name, c.skipFrame);
			return;
		}
		if (!boot) {
			scenarioFailf(t, "the Onion is booted (hasBootContainer)", "case %s: boot bit clear", c.name);
			return;
		}
		if (c.expectSeeds >= 0 && seeds != c.expectSeeds) {
			scenarioFailf(t, "exactly one first seed", "case %s: %d seeds", c.name, seeds);
			return;
		}
		if (seeds > 1) {
			scenarioFailf(t, "at most one first seed", "case %s: %d seeds", c.name, seeds);
			return;
		}
		if (c.expectSeeds < 0) {
			std::printf("[scenario] onion_skip %s: %s\n", c.name,
			            seeds == 0 ? "no seed: the upstream #205 bug (EmitCone snapped the Onion out of BootInit before the recovery)"
			                       : "one seed: the upstream #205 behaviour (EmitCone fired the first seed before the snap)");
		}
		if (++mCase == kCaseCount) {
			mPhase = Done;
			scenarioPass();
			return;
		}
		enter(Start, t);
	}

	GoalItem* mOnion = nullptr;
	int mCase        = 0;
	Phase mPhase     = Start;
	unsigned mPhaseTick = 0, mMovieTick = 0, mMovieTicks = 0, mWaitTick = 0;
	int mBorn0 = 0, mPressFrame = -1, mPressState = -1, mPressBorn = 0;
	bool mPressed = false, mPressStart = false;
};

} // namespace

PcScenario* pc_scenario_make_onion_skip(void)
{
	static OnionSkip s;
	return &s;
}

#endif
