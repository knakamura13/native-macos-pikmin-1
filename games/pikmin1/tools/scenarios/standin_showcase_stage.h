#pragma once

// Flat art showcase stage: stages/sandbox_showcase.ini. No cliff; props sit near the
// captain spawn for the standin_showcase scenario camera and 640x480 captures.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace standin {

constexpr const char* kShowcaseStageIni = "stages/sandbox_showcase.ini";

constexpr float kShowcaseRedOnionX = -28.0f;
constexpr float kShowcaseRedOnionZ = 38.0f;
constexpr float kShowcasePelletX   = 26.0f;
constexpr float kShowcasePelletZ   = 34.0f;
constexpr float kShowcaseUfoX      = 4.0f;
constexpr float kShowcaseUfoZ      = -58.0f;

bool addShowcaseStage(std::map<std::string, std::vector<std::uint8_t>>& tree, std::string& error);

} // namespace standin
