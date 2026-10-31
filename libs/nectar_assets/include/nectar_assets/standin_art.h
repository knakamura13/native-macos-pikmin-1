#pragma once

// Procedural stand-in art for disc-free builds (issue #115). All geometry and
// pixel data are generated in code; nothing is taken from the game disc.

#include "nectar_assets/mod.h"
#include "nectar_assets/standin_models.h"
#include "nectar_assets/image.h"
#include "nectar_assets/texture.h"

#include <cstdint>
#include <string>

namespace nectar {

/// Paint helpers for stand-in BTI sources.
void artFill(Image& img, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
void artFillRect(Image& img, int x0, int y0, int x1, int y1, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
void artFillCircle(Image& img, int cx, int cy, int radius, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
void artFillRing(Image& img, int cx, int cy, int outerR, int innerR, std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);
void artDrawDigit(Image& img, int digit, int x, int y, int scale, std::uint8_t r, std::uint8_t g, std::uint8_t b);

std::vector<std::uint8_t> artBtiRgb5a3(int width, int height, const Image& img);

/// Named HUD / screen textures referenced from stand-in .blo files.
std::vector<std::uint8_t> artHudTexture(const std::string& baseName);
/// Generic grey placeholder (legacy callers).
Image artFlatGreyImage(int width, int height);

/// Replace uniform boxes with recognizable coloured shapes while keeping joint
/// count, parents and collision text from `spec`.
Model buildArtStandInModel(const StandInModelSpec& spec);

} // namespace nectar
