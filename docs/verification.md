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

## Ten-slot and terrain release — 2026-10-12

The packaged release now uses protocol 4: 128-byte input, 5376-byte response,
128 scene-local world cells and ten virtual hotbar slots. Both pinned source
patches pass reverse-application checks against the compiled vendor trees.

Final real Minecraft regression:

```text
PASS Minecraft protocol 4: ten slots, unlimited stacks, shield, golden apples, armor, elytra/fireworks, axe, obsidian recovery, terrain, TNT and totem
```

The test also checks that using virtual slot 0 does not overwrite the diamond axe
in slot 1. Actual vanilla equipment writes are redirected to the tenth inventory
slot. The final server measured 422,985,728 bytes peak working memory,
453,107,712 bytes peak private memory and 150,770,291 bytes runtime storage.

Final packaged native regression:

```text
PASS native SoH: rendered=1200 mining=3 TNT_place=1 TNT_prime=1 TNT_explode=2 axe_collisions=5 inventory_swap=1 inventory_draws=75 textures=14 elytra=1 fuse=0 charge_fuse=0 enemy_damaged=1 terrain=1 clipped=5544 obsidian=1/1
```

The native test excavated an actual static Zelda floor: its queried height moved
from 0 to -20 units after the Minecraft mining acknowledgement. Room triangle
clipping was invoked, and obsidian count moved 32→31→32 after placement and axe
recovery. Enemy combat, TNT and flight ran through the actual SoH actor hooks.
Test-specific positioning prevents unrelated enemy movement from disrupting the
scripted placement checks; production movement is unaffected. Existing player
inventory is restored after verification.

Both CTest cases passed: separate-process transport and geometric terrain clipping
(area, UV interpolation and preservation of unrelated triangles). Native item
textures now pad rows to the Fast3D eight-byte stride, including narrow horse and
elytra faces. Official wing geometry/UVs, mounted skeleton-horse replacement and
opening-house seated poses are implemented. The automated test verifies rendering
execution, not their visual appearance; the full opening cutscene has not yet
received a visual playback check.

Terrain edits are limited to 128 cells in the current scene and clear on scene
transition. Static room geometry is clipped; moving actors are excluded. Native
dynamic collision capacity can limit simultaneously collidable placed blocks.
TNT remains limited to eight simultaneous charges. These are bounded integration
features, not a persistent voxel conversion of the entire game.

The stripped packaged executable is 33,543,168 bytes. The parent workspace audit
measured 3,339,256,516 logical bytes before these documentation changes, below the
5,000,000,000-byte ceiling. The launcher remains `Play-Steve.cmd`; the game is
closed after the automated checks.

## Manual jump and pillar placement — 2026-10-12

J requests the native player's jumping action, preserving Zelda's floor, ceiling
and landing handling. Airborne placement raycasts directly beneath the feet and
requires clearance above the new cell before submitting it to Minecraft.
`scripts/verify-soh.ps1 -Jump` passed the actual native jump, authoritative
cobblestone placement and landing on its collision surface:

```text
PASS native manual jump: peak=54.00 underfoot_block=1 landed_y=45.00
```

Heights in this diagnostic are relative to the initial warp origin.
`scripts/play-forest.ps1` boots at Kokiri Forest's central area in a temporary
Zelda debug save and restores the previous boot/warp settings when the game closes.

Mouse controls are now reserved before SoH controller dispatch. Both press and
release events are consumed, and direct mouse-state queries return false for
middle/right buttons while the mod is enabled. Middle-click requests jump;
right-click toggles glide or uses selected fireworks during flight. Menus suppress
Steve actions. Keyboard J/V/X alternatives remain available.

The native test dispatches the same mouse handler and checks reservation, actual
jump/placement/landing, glide and rocket acceleration. Its flight starts above
the forest roof to avoid an immediate collision ending the glide.

```text
PASS native mouse controls: jump_peak=54.00 underfoot_block=1 landed=1 reserved=1 glide=1 fireworks_boost=1
```

## Session sandbox and first-person release — 2026-10-13

Scene edits stay in memory by Zelda scene ID. New visit serials restore blocks,
tunnels and unlit TNT. Restarting the game/backend clears edits and shadow-arena
TNT leftovers; inventory saves normally. Primed TNT clears on scene unload.
Response byte 1072 echoes the complete visit token; stale geometry/flight from a
previous visit is suppressed without changing packet/C-ABI sizes (protocol 4).

Left-click mines/attacks, including held input and quick press/release edges.
Right-click uses items and fireworks. J/middle-click jumps on ground or starts
elytra flight in air when equipped; landing stops flight. P switches camera views.
Focus loss and menus release held mouse actions. Mouse buttons bypass Zelda
controller mappings while the mod is enabled.

First-person uses a 70-degree FOV and Minecraft eye heights (32.4 Zelda units
standing, 8 gliding). Official wide-skin arm cuboids/sleeves, held items/blocks and
the shield plate/handle render in the final overlay pass after world geometry.
Official crosshair/hotbar/selection textures and projected white mining/green
placement outlines are drawn by ImGui. Underfoot placement shares its preview
coordinates with the interaction packet. Cutscenes/dialogue use normal cameras.
The Zelda minimap is hidden in first-person to avoid overlapping hands; the
setting gEnhancements.SteveFirstPersonMinimap=1 restores it.

A native framebuffer capture visually confirms the skin, held cobblestone and
shield without minimap overlap. It captures the native layer; ImGui HUD loading
and drawing are checked by counters. Exact pixel/animation equality to the
Minecraft client renderer has not been established.

Final real-server regression checks fixed-key session reset, scene isolation,
restored blocks/tunnels/TNT, ignition and quick-click axe attacks:

```text
PASS Minecraft protocol 4: ten slots, unlimited stacks, shield, golden apples, armor, elytra/fireworks, axe, obsidian recovery, session persistence, terrain, TNT and totem
```

It measured 426,545,152 peak working bytes, 452,554,752 peak private bytes and
150,776,617 runtime bytes. The subsequent native run measured 425,508,864 peak
working bytes, 453,484,544 peak private bytes and 150,778,777 runtime bytes.
Both satisfy the requested 500 MB RAM/200 MB runtime limits.

Final native mouse/camera/preview check:

```text
PASS native mouse controls: jump_peak=54.00 underfoot_block=1 landed=1 reserved=1 glide=1 fireworks_boost=1 left_mine=1 first_person=331 aim=1
```

Final complete native release regression:

```text
PASS native SoH: rendered=1200 mining=3 TNT_place=1 TNT_prime=1 TNT_explode=2 axe_collisions=5 inventory_swap=1 inventory_draws=75 textures=14 elytra=1 fuse=0 charge_fuse=0 enemy_damaged=1 terrain=1 clipped=5544 obsidian=1/1 first_person=350
```

Both CTest cases passed, and both vendor patches passed reverse-application checks.
The stripped executable is 33,596,416 bytes. The parent workspace measured
3,342,028,624 logical bytes before report/commit updates, below 5 GB. Existing
Zelda saves and production Minecraft inventory were preserved. Tests close the
game and stop the backend; normal launch remains Play-Steve.cmd.
