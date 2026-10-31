#!/usr/bin/env bash
# Regenerate the disc-free stand-in asset tree (issue #115). All models, HUD
# textures and .blo layouts are produced procedurally from libs/nectar_assets and
# games/pikmin1/tools/scenarios/standin_generator.cpp.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build/pikmin1-scenarios}"
OUT="${2:-$BUILD/scenario-data}"
cmake -S "$ROOT/games/pikmin1" -B "$BUILD" \
  -DPIKMIN_NATIVE_JAUDIO=OFF \
  -DPIKMIN_BUILD_SCENARIOS=ON
cmake --build "$BUILD" --target nectar_standin_generator
"$BUILD/nectar_standin_generator" "$OUT"
echo "Stand-in assets written to $OUT"
