# Disc scenarios on Linux x86-64 (Docker)

Recorded 2026-10-28 on an M3 Max Mac, Docker Desktop 29.9.2, `linux/amd64`
(emulated), tree `ccf3c90a`. Scenarios `boot_smoke`, `throw_release` and
`day30_clock` at 30 and 60 Hz against a real disc install, then compared with
macOS arm64 runs of the same tree on the same install.

## Recipe

Ubuntu 20.04's SDL 2.0.10 is too old (the game needs `SDL_GameControllerHasSensor`),
so the image is `ubuntu:24.04` with its `libsdl2-dev`:

```dockerfile
FROM --platform=linux/amd64 ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake ninja-build ccache pkg-config g++ libsdl2-dev libgl1-mesa-dev libx11-dev \
    libgl1-mesa-dri libglx-mesa0 libegl-mesa0 xvfb xauth ca-certificates python3 \
 && rm -rf /var/lib/apt/lists/*
```

Build (source mounted read-only at `/src`, build output in a Docker volume):

```sh
docker build -t pikmin-linux-disc .
docker run --rm --platform linux/amd64 -v "$PWD":/src:ro -v pikmin-linux-build:/build \
  pikmin-linux-disc bash -c '
    cmake -S /src/games/pikmin1 -B /build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DPIKMIN_NATIVE_JAUDIO=OFF -DPIKMIN_BUILD_SCENARIOS=ON &&
    cmake --build /build --target pikmin_pc -j 12'
```

Run (the install is mounted read-only; the scratch folder the game changes into
holds a symlink to its `assets`, so nothing is written to the install):

```sh
docker run --rm --platform linux/amd64 -v "$HOME/<install>":/disc:ro -v "$PWD/out":/out \
  -v pikmin-linux-build:/build:ro pikmin-linux-disc bash -c '
    export LIBGL_ALWAYS_SOFTWARE=1
    mkdir -p /scratch && ln -sfn /disc/assets /scratch/assets
    for n in boot_smoke throw_release day30_clock; do for r in 30 60; do
      PIKMIN_SCENARIO_DATA_DIR=/scratch xvfb-run -a /build/bin/nectar \
        --scenario $n --scenario-tick-rate $r --state-hash-log /out/$n-$r.hash \
        > /out/$n-$r.out 2>&1; echo "$n $r exit $?"
    done; done'
```

The build and the six runs each finished within a few minutes under emulation.

## Results

| Scenario | Rate | Linux exit | macOS exit | `throw_release` K (Linux / macOS) |
|----------|------|-----------|-----------|-----------------------------------|
| boot_smoke | 30 | 0 | 0 | |
| boot_smoke | 60 | 0 | 0 | |
| throw_release | 30 | 0 | 0 | 15 / 15 |
| throw_release | 60 | 0 | 0 | 18 / 18 |
| day30_clock | 30 | 0 | 0 | |
| day30_clock | 60 | 0 | 0 | |

All six pass on Linux with K = 15 (30 Hz) and 18 (60 Hz), the same as macOS and
the stand-in.

## State-hash logs: Linux and macOS differ

Each platform is repeatable: a second `boot_smoke` 30 Hz run on each reproduces
its own log byte for byte. Across platforms none of the six logs match, with the
same number of ticks. Per column (see `pc_state_hash.h`), first differing tick:

| Scenario / rate | item | navi | teki | piki | rng |
|-----------------|------|------|------|------|-----|
| boot_smoke 30 | 6 | 16 | 51 | same | same |
| boot_smoke 60 | 6 | 9 | 51 | same | same |
| throw_release 30 | 6 | 16 | 51 | 52 | 374 |
| throw_release 60 | 6 | 9 | 51 | 52 | same |
| day30_clock 30 | 6 | 16 | 51 | same | same |
| day30_clock 60 | 6 | 9 | 51 | same | same |

The `item` hash diverges first, at tick 6, in every run; `world` and `mode` never
differ. Cause not investigated. The hash covers float bits of item positions and
velocities, so x86-64 against arm64 floating-point differences (fused
multiply-add contraction, libm) are a candidate, not a finding. The scenarios
assert behaviour (state, timing, K), not hash equality, so they pass anyway. A
cross-platform hash comparison, as netplay would need, does not hold today.

## Not verified

- Windows (wine in Docker) was not attempted: the disc run needs an MSVC- or
  MinGW-built `nectar.exe` and a GL stack under wine, not cheap.
- The `ctest -R _disc` path on Linux (the runs above call `nectar` directly).
- Software GL was llvmpipe under xvfb in an emulated container; real GPU
  drivers were not tried.
- The cause of the macOS/Linux hash divergence.
