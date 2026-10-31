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

find_generator() {
  local base="$BUILD/nectar_standin_generator"
  if [[ -x "$base" ]]; then echo "$base"; return; fi
  for sub in Debug Release RelWithDebInfo MinSizeRel; do
    if [[ -x "$BUILD/$sub/nectar_standin_generator" ]]; then
      echo "$BUILD/$sub/nectar_standin_generator"
      return
    fi
  done
  echo "nectar_standin_generator not found under $BUILD" >&2
  exit 1
}

GEN="$(find_generator)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

"$GEN" "$TMP/first"
"$GEN" "$TMP/second"
if ! diff -qr "$TMP/first/assets" "$TMP/second/assets" >/dev/null; then
  echo "rebuild-standin-assets: two generator runs differ" >&2
  diff -qr "$TMP/first/assets" "$TMP/second/assets" >&2 || true
  exit 1
fi

"$GEN" "$OUT"
echo "Stand-in assets written to $OUT (verified identical double-run)"
