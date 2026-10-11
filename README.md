# Zelda–Steve native bridge

This repository contains a working C++26-mode shared-memory relay, a deterministic
headless Minecraft-style logic harness, a native SoH client, and an in-tree player
override patch. **This is a tested bridge prototype, not a finished integration
with Mojang Minecraft or a visually verified SoH mod.**

## Build and test

Tested on Windows with MSYS2 UCRT64 GCC 14.1 and CMake 3.29.3. Put
`C:\msys64\ucrt64\bin` on PATH, then run from this directory:

```powershell
./scripts/build.ps1
./scripts/run.ps1 -Seconds 3600
```

The runner launches the bridge hidden and the backend at 20 Hz. Launch patched
SoH separately and set `gEnhancements.SteveBridge` to `1` in its console-variable
configuration. Both use session `default`. Standalone commands are
`build/bridge.exe default 3600` and `build/headless_mc.exe default 3600`.
Exactly one daemon, one backend, and one frontend may use a session.

For Linux, configure a Release build with a compiler accepting `-std=c++26`:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
ctest --test-dir build --output-on-failure
./build/bridge default 3600 &
./build/headless_mc default 3600
```

Linux/POSIX and MSVC paths are implemented but not validated in this environment.
On POSIX a forcibly killed daemon can leave `/dev/shm/zelda_steve_<session>` behind;
remove only that specific stale session after all clients have exited. On Windows,
close all processes holding a session before restarting its daemon.

## Transport and ownership

`src/bridge/protocol.hpp` defines a 64-byte input packet and a 256-byte response,
both cache-line aligned with compile-time layout checks. Four bounded SPSC rings
connect frontend → relay → backend and backend → relay → frontend. Acquire/release
atomics publish slots without locks or heap allocations. `reserve`/`peek` expose
mapped slots directly. **The relay copies each packet between rings**; the system
does not promise end-to-end zero-copy. On full rings producers retry or drop a
state sample; no consumer-owned slot is overwritten. Input edges survive 60 Hz
to 20 Hz sampling. Multiple presses inside one tick coalesce.

The mapping validates session readiness, protocol version and structure size.
Only same-machine, same-architecture, compatible compiler/standard-library ABIs
are supported. Standard C++ does not itself guarantee process-shared atomics;
the implementation relies on the native lock-free 32-bit atomics of Windows and
POSIX target platforms. A version mismatch requires restarting every process.

The frontend polls without blocking a game frame. Continuous arm pose is
interpolated one tick behind; item IDs and inventories change discretely.
Actions stop after 250 ms without fresh input/state, and the client releases a
stalled session after two seconds. Audio events can be drained once using
`Client::pop_audio()`; the bounded queue reports overflow with `audio_drops()`.
No inventory data is extrapolated. Polling adds up to roughly 1 ms at the relay;
no measured latency or throughput guarantee is claimed.

## Headless harness

`src/headless_mc` has no JVM, game downloads, renderer, network stack or runtime
asset dependencies. It models a nine-slot hotbar, tool durability, food consumption,
bow use, shielding, and ten-stage mining of a regenerating test block within five
blocks of the supplied target. Bridge-local item IDs: 1 pickaxe, 2 sword, 3 food,
4 bow, 5 mined block. It does not simulate vanilla Minecraft physics, crafting,
entities, persistence, complete inventory or registry metadata.

The separate-process test drives inputs at approximately 60 Hz, runs the real
relay and backend processes, and checks mining, durability, inventory and native
actor pose. It also tests concurrent ring wraparound, full queues, short button
edges, audio delivery, stale input and server-state fallback. Windows peak working
set and private memory are checked against 500,000,000 bytes during the test.
This measured test bound is not an OS-enforced production memory quota.

## SoH integration

Source reference: [HarbourMasters/Shipwright](https://github.com/HarbourMasters/Shipwright),
commit `7be5a4feacd1db1474bec97bb3ba7ad08de89d28`. The ignored `vendor/shipwright`
checkout is depth one and sparse. The supplied patch modifies
`soh/src/overlays/actors/ovl_player_actor/z_player.c`: `Player_Update` samples input
and `Player_Draw` selects the custom renderer when fresh backend state is present.
The patch is already applied to the local reference checkout.

For a separate full SoH source checkout:

1. Verify the above commit and run `git apply --check <bridge>/src/soh_mod/shipwright.patch`,
   then `git apply <bridge>/src/soh_mod/shipwright.patch`.
2. After creation of SoH's `soh` CMake target, set `STEVE_BRIDGE_ROOT` to this
   repository's absolute path and include `src/soh_mod/integration.cmake`.
3. Build SoH with its documented dependencies and supported game assets.
   Its documented asset/build requirements are in the upstream README and
   `docs/BUILDING.md`. Full SoH and assets were not fetched or built here.
4. Start the daemon/backend and enable `gEnhancements.SteveBridge`.

The actor adapter draws six original colored boxes plus a placeholder held tool.
B drives mine/attack, R blocks, A consumes/uses the selected item, and C-left/right
cycle hotbar slots. Coordinates use 20 SoH units per block; angles use radians.
This renderer is deliberately untextured and has no imported Steve skin or
production skeleton resource. Animation comes from native rigid limb rotations.
SoH movement/collisions remain governed by the player actor. Health and destruction
stage reach the C API, but are not yet wired to SoH's health HUD, scene destruction
or collision logic. Audio reaches the native client queue but is not yet played
by SoH. The renderer patch has been checked against source, not compiled or
visually playtested inside the game.

## Remaining work for the requested complete system

* A real Minecraft runtime adapter must translate version-specific registry IDs,
  server-player actions and authoritative world/inventory state into these rings.
  The supplied harness is a replacement for testing that interface only.
* Build and playtest the in-tree adapter with complete SoH dependencies and game
  assets; validate matrices, transparency, cutscenes, held-item geometry and limb
  attachment/collision positions when bypassing Link's normal draw routine.
* Connect scene block interactions, health and audio to actual SoH gameplay and
  provide a finished Steve skin/skeleton/asset package.
* Benchmark sustained load and enforce production process memory limits.

## Storage and language limits

All build outputs, game assets, runtime binaries and vendor sources are ignored.
`scripts/disk-budget.ps1` includes hidden Git files across the entire parent
workspace and fails at 5,000,000,000 logical bytes. With `-CleanObjects`, it removes
only `.o`/`.obj` files inside this project's verified `build/CMakeFiles` path when
the workspace reaches 4,000,000,000 bytes. Build scripts check before and after
builds. These checks cannot prevent unrelated processes from filling the directory
between audits. Keep any future dependency fetch below the remaining budget.
Release executables are linker-stripped; Windows GCC executables link runtimes
statically so no DLL distribution is required.

All bridge C++ targets explicitly use `-std=c++26` or `/std:c++latest`, with warnings
as errors. `std::expected` and lock-free atomics are used. GCC 14 accepts C++26 mode
but does not implement the full C++26 standard; reflection and contracts are not
required or claimed. The upstream C actor hook remains C for ABI compatibility.
