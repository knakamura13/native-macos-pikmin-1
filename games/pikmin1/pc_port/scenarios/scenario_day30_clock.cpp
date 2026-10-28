// day30_clock: Infinite Day holds the world clock on Days 2 to 29 and not on
// Day 30 (MAX_DAYS). Regression test for upstream #100.
//
// Day 2: a positive control (override off, the clock advances), then override
// on and the clock stays put. Day 30: override on, the clock advances to
// sunset. The run starts on Day 2 and moves to Day 30 mid-run, because the
// clock hold reads only mCurrentDay each tick; generator files and the RNG
// reseed, which key off the day at stage load, are not what is under test.
#include "scenarios/pc_scenario_list.h"

#if PIKI_SCENARIOS

#include "MoviePlayer.h"
#include "PlayerState.h"
#include <cstdio>

#include "Dolphin/pad.h"
#include "gameflow.h"
#include "mods/pc_mission.h"
#include "pc_coop.h"
#include "pc_permadeath.h"
#include "pc_speedrun.h"
#include "settings/pc_settings.h"

namespace {

constexpr unsigned kControlTicks = 300;
constexpr unsigned kHoldTicks    = 300;
// Sunset is 19:00. Disc stages stretch the day (about 6,750 ticks per game hour
// at 30 Hz on Forest of Hope), so the run starts 12 game minutes before it.
constexpr float kDay30StartHour  = 18.8f; // sunset (19:00) is 30 game minutes, about 33.75 s, away
constexpr int kLastDay           = 30;    // MAX_DAYS

class Day30Clock : public PcScenario {
public:
	const char* name() const override { return "day30_clock"; }
	int startDay() const override { return 2; }

	// A ship text window can open on its own (the last day has one) and holds
	// the clock until it is dismissed, as it does for a player. Press A to close it.
	void providePads(unsigned tick, PcInputPad& pad) override
	{
		if (gameflow.mIsUIOverlayActive && tick % 15 == 0) pad.button = PAD_BUTTON_A;
	}

	void onTick(unsigned tick) override
	{
		WorldClock& clock = gameflow.mWorldClock;
		switch (mPhase) {
		case Settle:
			// Wait for any ship text or overlay to clear, then check the
			// preconditions the clock update depends on.
			if (gameflow.mIsUIOverlayActive || (gameflow.mMoviePlayer && gameflow.mMoviePlayer->mIsActive)) {
				if (tick > kSettleTicks * ticksPerSecond()) scenarioFailf(tick, "no overlay or movie", "overlay or movie still active");
				return;
			}
			if (pc_speedrun_active() || pc_vs_active() || pc_mission_active() || pc_hardmode_active()) {
				scenarioFailf(tick, "speedrun, vs, mission and hardmode off", "a mode is active");
				return;
			}
			if (playerState->isTutorial()) {
				scenarioFailf(tick, "non-tutorial stage", "tutorial is on");
				return;
			}
			if (clock.mCurrentDay != 2) {
				scenarioFailf(tick, "day 2", "day %d", clock.mCurrentDay);
				return;
			}
			pc_settings_scenario_set_infinite_day(0);
			begin(tick, clock);
			mPhase = Control;
			break;
		case Control:
			if (tick - mStartTick + 1 >= kControlTicks) {
				if (!(clock.mTimeOfDay > mTimeOfDay)) {
					scenarioFailf(tick, "clock advances with Infinite Day off", "mTimeOfDay %.4f unchanged", clock.mTimeOfDay);
					return;
				}
				pc_settings_scenario_set_infinite_day(1);
				begin(tick, clock);
				mPhase = HoldDay2;
			}
			break;
		case HoldDay2:
			if (clock.mTimeOfDay != mTimeOfDay || clock.mRealSecsIntoHour != mSecsIntoHour || clock.mCurrentGameHour != mGameHour) {
				scenarioFailf(tick, "clock held on day 2", "mTimeOfDay %.4f -> %.4f", mTimeOfDay, clock.mTimeOfDay);
				return;
			}
			if (tick - mStartTick + 1 >= kHoldTicks) {
				clock.mCurrentDay = kLastDay;
				clock.setTime(kDay30StartHour);
				begin(tick, clock);
				mPhase = Day30;
			}
			break;
		case Day30: {
			const unsigned ran = tick - mStartTick;
			if (ran % 300 == 0) {
				std::printf("[scenario] day30 +%u: time %.4f overlay %d dayEndActive %d movie %d\n", ran, clock.mTimeOfDay,
				            (int)gameflow.mIsUIOverlayActive, (int)gameflow.mIsDayEndActive,
				            gameflow.mMoviePlayer ? (int)gameflow.mMoviePlayer->mIsActive : -1);
			}
			if (gameflow.mIsDayEndActive) {
				if (clock.mTimeOfDay < gameflow.mParameters->mEndHour()) {
					scenarioFailf(tick, "sunset at or after the end hour", "day end active at %.4f", clock.mTimeOfDay);
					return;
				}
				scenarioPass();
				return;
			}
			// Ticks with a text window open do not count: the window holds the clock.
			if (!gameflow.mIsUIOverlayActive && ++mOpenTicks == kHoldTicks && !(clock.mTimeOfDay > mTimeOfDay)) {
				scenarioFailf(tick, "clock advances on day 30", "clock held on day 30 at %.4f", clock.mTimeOfDay);
				return;
			}
			if (ran > 8000u * ticksPerSecond() / 30u) {
				scenarioFailf(tick, "sunset within the tick budget", "no day end by %.4f", clock.mTimeOfDay);
			}
			break;
		}
		}
	}

private:
	enum Phase { Settle, Control, HoldDay2, Day30 };
	static constexpr unsigned kSettleTicks = 10;

	static int ticksPerSecond() { return pc_scenario_ticks_per_second(); }

	void begin(unsigned tick, const WorldClock& clock)
	{
		mStartTick    = tick;
		mOpenTicks    = 0;
		mTimeOfDay    = clock.mTimeOfDay;
		mSecsIntoHour = clock.mRealSecsIntoHour;
		mGameHour     = clock.mCurrentGameHour;
	}

	Phase mPhase        = Settle;
	unsigned mStartTick = 0;
	unsigned mOpenTicks = 0;
	float mTimeOfDay    = 0.0f;
	float mSecsIntoHour = 0.0f;
	int mGameHour       = 0;
};

} // namespace

PcScenario* pc_scenario_make_day30_clock(void)
{
	static Day30Clock s;
	return &s;
}

#endif
