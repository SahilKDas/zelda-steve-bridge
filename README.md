# Zelda–Steve native Minecraft bridge

A native C++26 shared-memory bridge connects Ship of Harkinian 9.3.0 to a real Minecraft Java 1.21.11 Fabric server. Minecraft owns inventory, hotbar selection, item components, durability, health, mining and item use. SoH owns traversal, camera, collision and Zelda gameplay. The player override draws an original procedural Steve-style rigid model, held tools, shield, mining cube and hotbar HUD.

## Windows setup

Prerequisites: Windows x64, Java **21** JDK, Git, MSYS2 UCRT64 GCC supporting `-std=c++26`, CMake, and Visual Studio C++ tools with CMake/Ninja and `/std:c++latest`. The installed JDK is external to the workspace; it is not downloaded or bundled. MSVC preview mode is used because complete C++26 support is not yet available. Reflection and contracts are not required by this implementation.

From the repository root in PowerShell:

```powershell
$env:JAVA_HOME = 'C:\Program Files\Microsoft\jdk-21.0.10.7-hotspot'
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
./scripts/build.ps1
./scripts/fetch-minecraft.ps1 -AcceptEula
./scripts/fetch-soh.ps1
./scripts/build-soh.ps1
./scripts/setup-soh.ps1 -Rom 'C:\path\to\supported-OoT.z64'
./scripts/run.ps1
```

Use `-AcceptEula` only after accepting the [Minecraft EULA](https://www.minecraft.net/eula). It writes `eula=true`; acceptance was explicitly provided for this workspace. ROMs and extracted archives are ignored by Git. The extractor reads the original ROM without modifying it. Both the US revision 2 and US GameCube Master Quest dumps found in Downloads have supported hashes. Pass both paths to `-Rom` to extract both sets of assets. There is no ROM download step.

`run.ps1` launches the mapping owner and a memory-limited Minecraft process, waits for readiness, then opens SoH. Closing SoH sends the server `stop` so it saves normally. Logs live in `runtime/minecraft`; game configuration and saves live in `runtime/soh`. Use `-BackendOnly` to start the services for a separately launched frontend. Run only one frontend and one backend per session; close the previous session before restarting. All components must run in the same Windows user/security context.

## Controls and game behavior

| SoH input | Minecraft action |
|---|---|
| B | Mine with the pickaxe; otherwise attack/swing |
| R | Use the offhand shield |
| Hold A | Use held food or draw the bow |
| Release A | Release the bow |
| C-left / C-right | Previous / next hotbar slot |
| Z | Supply the current lock-on target |

The initial hotbar contains an iron pickaxe, iron sword, eight cooked beef, a bow and 64 arrows. A shield occupies the offhand. Minecraft's player data is saved with vanilla item-component codecs every five seconds and at shutdown in `bridge-world/steve-player.json`. Zelda damage is translated to Minecraft health; returned health drives Zelda's hearts. Respawn requests restore health and hunger.

The backend uses a bounded flat arena with a bedrock floor and a stone mining target. The native cube has dynamic collision and disappears after the authoritative mining acknowledgement. Entering a different Zelda scene anchors the arena at the new player position and restores its test block. This is an item-logic bridge and a bounded interaction arena; it does not convert every Zelda surface or enemy into a Minecraft block or entity, provide full Minecraft terrain, or add a crafting interface. Vanilla Zelda collisions and animation-derived limb positions are retained before replacing render commands. Sword attack logic uses Minecraft entities within the backend arena when present.

The override defaults on in this patched build when the backend is connected. Set `gEnhancements.SteveBridge` to `0` to disable it. Without fresh backend state, Link's normal renderer is used. SoH's normal controller mapping remains available; hotbar counts, selection, remaining durability and destruction stage are shown in the native overlay.

## Native transport

`src/bridge/protocol.hpp` specifies a 128-byte input and a 256-byte response, both aligned to 64 bytes. The two SPSC rings expose mapped slots directly. Acquire/release atomics transfer ownership; the daemon owns mapping lifecycle and updates a heartbeat without relaying or copying packets between rings. JNI exposes the same slots as direct byte buffers. Snapshot copies are made only for frontend interpolation and Java reads fields directly from the mapped input.

The input carries transform, buttons and preserved press edges, target/lock-on, slot, scene, damage and respawn flags. The response carries vanilla numeric item registry IDs, components represented by damage/count/remaining durability, nine hotbar slots, hand/swing, health, destruction stage, eight audio events and a monotonic block-break acknowledgement. Transport version and size are checked. Exactly one producer and consumer own each ring. Full rings apply backpressure; unconsumed slots are never overwritten. Multiple button edges within a single Minecraft tick coalesce.

Minecraft ticks at 20 Hz. Continuous arm poses interpolate one tick behind; inventories and events remain discrete. Stale input stops actions after 250 ms, stale responses fall back to Link, and a stalled session is released after two seconds. This relies on native lock-free atomics on the same machine and x64 ABI, rather than a portable network wire format. Windows cross-compiler interoperability is exercised by the GCC/JNI backend and MSVC frontend. POSIX mapping support is implemented but the full Linux integration has not been tested here.

## Verification and resource budget

```powershell
ctest --test-dir build --output-on-failure
./scripts/verify-minecraft.ps1
./scripts/verify-soh.ps1
./scripts/disk-budget.ps1
```

The deterministic separate-process harness (`headless_mc.exe`) exists only for transport regression tests. The production launcher uses the actual Fabric server. Native tests cover 100,000 concurrent ring transfers, wraparound/full queues, button edges, audio ordering, stale-state fallback and actor pose. The real-server test uses the same C API as SoH and verifies vanilla stone mining, drops, durability, shield pose, transferred damage, timed food use and bow release.

`verify-soh.ps1` launches the compiled game with an isolated temporary debug save in `runtime/soh-test`. Its opt-in actor test checks rendered frames, dynamic collision registration/removal, mining durability, shielding, health, food and bow ammunition. The result is written to `runtime/soh-test/steve-in-game-test.log`. Use `-KeepOpen` to inspect the model after the test, then close the game to finish verification.

A fresh Minecraft test measured 421,744,640 bytes peak working memory, 451,620,864 bytes peak private memory, and 150,070,577 bytes runtime storage. The supervisor enforces a 480,000,000-byte committed-memory Job Object limit, checks peak working memory against 500,000,000 bytes, and stops if runtime storage reaches 200,000,000 bytes. It launches without a window and kills its child on supervisor termination. The fixed spawn bypasses expensive vanilla spawn-region pre-generation.

The entire parent workspace, including hidden Git history and caches, must remain below **5,000,000,000 bytes**. `disk-budget.ps1` audits that scope and warns at 4 GB. Release builds strip GCC executable symbols; SoH Release disables PDB generation and uses static dependencies. Pinned source revisions are fetched at depth one. Do not bundle an additional JDK or large world. Build/cache/runtime/native binaries and game assets are ignored. Keep the audit below the ceiling before downloading dependencies or rebuilding.

## Source map

- `src/bridge`: aligned protocol, mapping lifecycle and mapping-owner daemon.
- `src/headless_mc/fabric`: pinned Fabric server mod, fixed-spawn mixins and direct-buffer Java adapter.
- `src/headless_mc/jni.cpp`: native shared-memory JNI implementation.
- `src/headless_mc/supervisor.cpp`: Windows process/memory/disk supervisor.
- `src/soh_mod`: nonblocking C++ client, C actor API, procedural renderer and pinned SoH patch.
- `scripts/soh-dependencies.json`: exact dependency commits for repeatable shallow fetches.
- `tests`: deterministic transport tests and real-Minecraft C API verification.

The SoH patch also repairs the pinned upstream extractor's ROM-bytes API mismatch and removes Release debug-symbol generation. The model is generated from native cubes; no external Steve mesh, texture pack, or animation archive is needed.
