# Integration verification — 2026-10-11

The Windows Release package uses Ship of Harkinian 9.3.0, Minecraft Java 1.21.11, Fabric Loader 0.19.5, Java 21, and shared-memory protocol version 3. The actor, renderer, inventory UI and bridge client compile as C++26 (`/std:c++latest` in MSVC); the bridge/JNI tools also compile under GCC `-std=c++26` with `-Wall -Wextra -Wpedantic -Werror`.

## Results

- Native separate-process transport regression: passed, including 100,000 concurrent transfers, queue wraparound/full conditions, interpolation, stale-state fallback and audio ordering.
- Real Minecraft server: passed mining/drops, item durability, shield, health, food, bow release/disconnect cancellation, 36-slot swaps, elytra equipment/flight/wear, successive axe hits, multiple TNT placements, ignition durability and TNT chain reactions.
- Final stripped SoH executable: passed the automated in-game test in an isolated temporary save. It recorded 900 rendered actor frames, mining acknowledgement and collision removal, TNT placement/priming and an actual native explosion actor, five native axe collisions with enemy health loss, inventory swaps and 75 inventory render frames with ten loaded official item textures, and actual elytra movement.

Final native test output:

```text
PASS native SoH: rendered=900 mining=1 TNT_place=1 TNT_prime=1 TNT_explode=2 axe_collisions=5 inventory_swap=1 inventory_draws=75 textures=10 elytra=1 fuse=0 charge_fuse=0 enemy_damaged=1
```

Minecraft's final native-game run measured 424,570,880 bytes peak working memory, 453,885,952 bytes peak private memory, and 150,757,561 bytes runtime storage. The supervised process stayed below the requested 500 MB RAM and 200 MB runtime storage limits.

The entire parent workspace, including hidden Git files, caches, sources, builds and both ROM-derived archives, measured approximately 3.103 billion logical bytes. `du -sh ..` reported `3.1G`; both audits are below the 5,000,000,000-byte ceiling. The packaged, stripped `soh.exe` is 33,445,376 bytes.

The original Downloads ROMs were read without modification. Test-created player inventory was archived at `cache/test-player-before-release.json`; the production launch starts with fresh starter equipment. Verification supplies temporary equipment and restores any existing player inventory after shutdown.

Run `Play-Steve.cmd` for normal play. Reproduce the checks with `scripts/verify-minecraft.ps1`, `scripts/verify-soh.ps1`, and `scripts/disk-budget.ps1`. Runtime logs are ignored by Git; this report records the successful release checks.

Post-launch fixes and armor release:

- Steve's material now explicitly selects a single texture cycle; the previous second
  cycle sampled the scene's stale tile 1. Debug overlay defaults off. Glide uses V,
  avoiding SoH's G binding; Left/Right arrows cycle the hotbar.
- The shared stone/TNT collision header now supplies a CAM_SET_NONE camera entry.
  The reproduced TNT crash resolved to BgCheck_GetBgCamSettingImpl dereferencing its
  formerly null camera list. Native TNT placement, priming and explosion now pass.
- Default armor is actual Minecraft diamond equipment with Protection IV and
  Unbreaking III. All four equipment/glint flags pass; a four-point incoming hit
  leaves 18.56 health. Chestplate/elytra swaps and equipment persistence are supported.
- Official diamond and armor-glint textures generate eight masked shimmer phases.
  The native regression requires glint drawing and eleven loaded inventory textures.
  Its final 900-frame run passed mining, TNT, five enemy collisions, inventory and flight.
- C++26 transport tests and real Minecraft interaction tests passed again. Final
  workspace usage is approximately 3.168 billion bytes, below the five-billion ceiling.
