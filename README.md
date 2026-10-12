# Zelda–Steve native Minecraft bridge

A native C++26 shared-memory bridge connects Ship of Harkinian 9.3.0 to a real Minecraft Java 1.21.11 Fabric server. Minecraft owns inventory, hotbar selection, durability, health, mining, TNT, combat cooldowns and elytra physics. SoH supplies its world, enemy collision/reactions and camera. Steve uses the official 1.21.11 wide skin, standard six-part dimensions and joint pivots, outer skin layers, textured held items and elytra wings.

## Windows setup

This workspace is already built and packaged. Double-click `Play-Steve.cmd` to launch the local Minecraft service and SoH together. It uses `JAVA_HOME` or discovers the installed Microsoft Java 21 JDK. Closing SoH saves and stops the backend.

Prerequisites: Windows x64, Java **21** JDK, Python with Pillow, Git, MSYS2 UCRT64 GCC supporting `-std=c++26`, CMake, and Visual Studio C++ tools with CMake/Ninja and `/std:c++latest`. The installed JDK is external to the workspace; it is not downloaded or bundled. MSVC preview mode is used because complete C++26 support is not yet available. Reflection and contracts are not required by this implementation.

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
| I | Open/close Steve's 36-slot inventory; click two slots to swap |
| G, while airborne | Toggle gliding with an equipped elytra |

Select TNT in the hotbar and tap A to place it ahead of Steve. Select flint and steel and tap A to ignite the nearest placed charge in reach. Up to eight charges are synchronized with native meshes and collision; Minecraft drives their fuse and detonation, and SoH applies the explosion through its bomb actor. Charges belong to the current scene and are cleared when it unloads.

Select an elytra in the inventory and use **Equip selected / swap chest**. Walk off a ledge and press G to glide; landing ends flight. Minecraft supplies glide velocity, while the native player hook applies movement and checks Zelda's floor and walls. Netherite-axe attacks use a Minecraft combat proxy for damage, cooldown and durability, then enter Zelda's normal attack-collider pipeline for enemy damage and reactions. Nearby enemies can be attacked directly or through Z targeting.

The initial hotbar contains an iron pickaxe, iron sword, eight cooked beef, a bow, a netherite axe, 16 TNT, flint and steel, an elytra and 64 arrows. A shield occupies the offhand. Older bridge saves receive the new equipment once without overwriting occupied slots. Player inventory and chest equipment use vanilla item-component codecs and save every five seconds and at shutdown in `bridge-world/steve-player.json`. Zelda damage is translated to Minecraft health; returned health drives Zelda's hearts. Respawn requests restore health and hunger.

The backend uses a bounded flat arena with a bedrock floor and a stone mining target. The native cube has dynamic collision and disappears after the authoritative mining acknowledgement. Entering a different Zelda scene anchors the arena at the settled player position and restores its test block. Enemy combat uses relative positions, and TNT maintains native world positions while Minecraft simulates the corresponding arena objects. This bridge does not turn every Zelda surface into Minecraft terrain or add a crafting interface. Zelda collisions and animation-derived limb positions are retained before replacing render commands.

The override defaults on in this patched build when the backend is connected. Set `gEnhancements.SteveBridge` to `0` to disable it. Without fresh backend state, Link's normal renderer is used. SoH's normal controller mapping remains available; hotbar counts, selection, remaining durability and destruction stage are shown in the native overlay.

## Native transport

`src/bridge/protocol.hpp` specifies a 128-byte input and a 1280-byte response, both aligned to 64 bytes. Protocol version 3 adds 36 inventory slots, equipment, acknowledged commands, combat results, flight velocity and eight TNT states. The two SPSC rings expose mapped slots directly. Acquire/release atomics transfer ownership; the daemon owns mapping lifecycle and updates a heartbeat without relaying or copying packets between rings. JNI exposes the same slots as direct byte buffers. Snapshot copies are made only for frontend interpolation and Java reads fields directly from the mapped input.

The input carries transform, buttons and preserved press edges, target/lock-on, slot, scene, damage and respawn flags. The response carries vanilla numeric item registry IDs, components represented by damage/count/remaining durability, nine hotbar slots, hand/swing, health, destruction stage, eight audio events and a monotonic block-break acknowledgement. Transport version and size are checked. Exactly one producer and consumer own each ring. Full rings apply backpressure; unconsumed slots are never overwritten. Multiple button edges within a single Minecraft tick coalesce.

Minecraft ticks at 20 Hz. Continuous arm poses interpolate one tick behind; inventories and events remain discrete. Stale input stops actions after 250 ms, stale responses fall back to Link, and a stalled session is released after two seconds. This relies on native lock-free atomics on the same machine and x64 ABI, rather than a portable network wire format. Windows cross-compiler interoperability is exercised by the GCC/JNI backend and MSVC frontend. POSIX mapping support is implemented but the full Linux integration has not been tested here.

## Verification and resource budget

The completed Windows release results are recorded in [docs/verification.md](docs/verification.md).

```powershell
ctest --test-dir build --output-on-failure
./scripts/verify-minecraft.ps1
./scripts/verify-soh.ps1
./scripts/disk-budget.ps1
```

The deterministic separate-process harness (`headless_mc.exe`) exists only for transport regression tests. The production launcher uses the actual Fabric server. Native tests cover 100,000 concurrent ring transfers, wraparound/full queues, button edges, audio ordering, stale-state fallback and actor pose. The real-server test uses the same C API as SoH and verifies stone mining, drops, durability, shielding, health, food, bow release/cancellation, inventory swaps, elytra wear, axe combo hits, TNT placement/ignition and chain reactions. Verification temporarily supplies fresh player equipment and restores the existing inventory after server shutdown.

`verify-soh.ps1` launches the compiled game with an isolated temporary debug save in `runtime/soh-test`. Its opt-in actor test checks rendered frames, mining collision removal, item use, inventory rendering and swaps, TNT placement/priming/explosion, successive native axe collisions and enemy health loss, and actual gliding movement. The result is written to `runtime/soh-test/steve-in-game-test.log`. Use `-KeepOpen` to inspect the model after the test, then close the game to finish verification.

A fresh Minecraft equipment test measured 422,412,288 bytes peak working memory, 452,481,024 bytes peak private memory, and 150,756,538 bytes runtime storage. The supervisor enforces a 480,000,000-byte committed-memory Job Object limit, checks peak working memory against 500,000,000 bytes, and stops if runtime storage reaches 200,000,000 bytes. It launches without a window and kills its child on supervisor termination. The fixed spawn bypasses expensive vanilla spawn-region pre-generation.

The entire parent workspace, including hidden Git history and caches, must remain below **5,000,000,000 bytes**. `disk-budget.ps1` audits that scope and warns at 4 GB. Release builds strip GCC executable symbols; SoH Release disables PDB generation and uses static dependencies. Pinned source revisions are fetched at depth one. Do not bundle an additional JDK or large world. Build/cache/runtime/native binaries and game assets are ignored. Keep the audit below the ceiling before downloading dependencies or rebuilding.

## Source map

- `src/bridge`: aligned protocol, mapping lifecycle and mapping-owner daemon.
- `src/headless_mc/fabric`: pinned Fabric server mod, fixed-spawn mixins and direct-buffer Java adapter.
- `src/headless_mc/jni.cpp`: native shared-memory JNI implementation.
- `src/headless_mc/supervisor.cpp`: Windows process/memory/disk supervisor.
- `src/soh_mod`: nonblocking C++ client, C actor API, procedural renderer and pinned SoH patch.
- `scripts/soh-dependencies.json`: exact dependency commits for repeatable shallow fetches.
- `tests`: deterministic transport tests and real-Minecraft C API verification.

The SoH patch also repairs the pinned upstream extractor's ROM-bytes API mismatch, fixes an audio-library CMake typo, skips unnecessary extractor manifests when compatible archives exist, and removes Release debug-symbol generation.

## Minecraft assets

Textures come from the user-supplied [Minecraft 1.21.11 assets repository](https://github.com/InventivetalentDev/minecraft-assets/tree/1.21.11), pinned to `0c2358f50e81e34def33b75af34fc65409893dc0`. `scripts/fetch-minecraft-assets.py` downloads only the required PNGs, records SHA-256 hashes, and generates big-endian RGBA16 face textures for Fast3D. The native rig uses the standard wide-arm skin UV layout and dimensions; it has no drawn replacement skin. GUI icons use the original PNG pixels. Cached assets and generated texture data are ignored by Git. Building SoH prepares them automatically when missing.
