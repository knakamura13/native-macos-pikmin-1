#pragma once

// The second stand-in stage: stages/sandbox_objects.ini (#59, objects gap). The
// first one, stages/sandbox.ini, is a flat floor with a few walls and no
// generators; every existing scenario runs there and its numbers (the
// throw_release calibration K) depend on it staying that way. This stage is for
// scenarios that opt in with PIKMIN_SCENARIO_STAGE=stages/sandbox_objects.ini.
//
// It has a heightfield with a cliff on one side and a default.gen that places,
// from the nectar_assets stand-ins only: a blue Onion beside the cliff, three
// buried sprouts, a one-pellet red pellet and a sleeping Dwarf Bulborb. Nothing
// here is read from the disc.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace standin {

/// `stages.ini` `file` value of the stage.
constexpr const char* kObjectsStageIni = "stages/sandbox_objects.ini";

/// Where things are, in map units (the cliff face runs along x = kCliffFootX to
/// kCliffTopX, rising kCliffHeight; the captain starts at the origin).
constexpr float kCliffFootX   = 50.0f;
constexpr float kCliffTopX    = 100.0f;
constexpr float kCliffHeight  = 200.0f;
constexpr float kOnionX       = -120.0f;
constexpr float kOnionZ       = -250.0f;

/// Adds the stage's map, .ini and generators to `tree` (paths relative to assets/, with dataDir/).
bool addObjectsStage(std::map<std::string, std::vector<std::uint8_t>>& tree, std::string& error);

} // namespace standin
