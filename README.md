# Ore ESP (`1.0.0hf2`)

Through-wall 3D ore vein highlighter for **LeviLaunchroid** (`preload-native`, `arm64-v8a`, Minecraft Bedrock `1.26.*`).

## What Changed in `1.0.0hf2`

1. **Fixed Mod Menu UI Freeze / Touch Deadlock When Turning Off**:
   - `libpreloader.so` invokes `onToggle` and `onConfigChanged` on Android's UI thread while holding its non-recursive `ModMenu` mutex.
   - `1.0.0hf1` called `pl::modmenu::setModuleEnabled` and `GameHook::clearOverlay()` (`pl::modmenu::submitDrawCommands`) synchronously inside `handleToggle` / `handleConfigChanged`, re-entering `libpreloader.so`'s mutex on the same UI thread and freezing Mod Menu touch input whenever the module was turned off.
   - `1.0.0hf2` makes `handleToggle`, `handleConfigChanged`, and `GameHook::applyConfig` strictly non-reentrant and lock-free with respect to `pl::modmenu`, deferring `HudOverlay` command clearing safely to the render/client update thread on the next frame.

## What Changed in `1.0.0hf1`

1. **Fixed Android 11+ ARM64 Scudo TBI Tagged-Pointer Rejection (`0xB400...`)**:
   - Android 11+ Scudo allocator tags all heap allocations in the top byte (`0xB400007...`) under ARM64 Top-Byte-Ignore (`TBI`).
   - `1.0.0`'s `guard::isPointer` checked `addr <= 0x0000FFFFFFFFFFFFull` before masking the top byte, rejecting 100% of live heap pointers (`LocalPlayer*`, `Dimension*`, `BlockSource*`, `LevelRenderer*`, `ScreenContext*`) and causing both scanning and rendering to bail out immediately.
   - `1.0.0hf1` strips the TBI tag byte (`addr & 0x0000FFFFFFFFFFFFull`) while rejecting kernel (`0xFF...`) and null-page (`< 0x10000`) addresses.
2. **In-Engine 3D GPU Through-Wall Renderer (`ui_fill_color` + `Tessellator`)**:
   - In LeviLaunchroid, `HudOverlay` is hidden (`View.GONE`) whenever **Hide HUD (`F1`)** or **Hide Launcher Overlays** is active (or Mod Menu overlay isn't enabled).
   - `1.0.0hf1` renders 3D camera-facing wireframe outlines and tracer ribbons **directly inside Bedrock's `LevelRenderer::renderLevel` pass** via `Tessellator` (`QuadList`) + `MeshHelpers::renderMeshImmediately` + `RenderMaterialGroup::common->getMaterial("ui_fill_color")` (depth test disabled), so through-wall ore outlines render with zero lag even when HUD/overlays are hidden.
   - Floating vein/distance labels (`Iron x6 [11m]`) continue to render via `HudOverlay` when HUD is visible, and an optional **2D HUD Outlines** toggle is available in Mod Menu.

## Features

- **In-Engine 3D Through-Wall Vein Wireframes**: Groups adjacent ores of the same type into 6-connected veins and cancels shared interior edges for crisp 3D silhouettes.
- **Color-Coded Ore Palette & Rarity Priority**:
  - **Ancient Debris** (`#FF8C42`), **Diamond** (`#24F0FF`), **Emerald** (`#20FF70`), **Gold** (`#FFD83B`), **Lapis** (`#4A8CFF`), **Redstone** (`#FF3B4E`), **Iron** (`#F2CBB0`), **Nether Quartz** (`#F5F2EA`), **Copper** (`#EA8248`, optional), **Coal** (`#9BA6B4`, optional).
- **Live Mod Menu Controls** (all apply immediately without restarting):
  - Master `Enabled` switch (cleanly stops scanning & clears ESP immediately when switched off)
  - `Scan Radius (blocks)` (`8..48`, default `24`)
  - `Rescan Interval (ticks)` (`4..40`, default `10`)
  - `Max Highlighted Veins` (`16..200`, default `80`)
  - `Outline Width` (`1.0..4.0`, default `1.8`)
  - `Merge Connected Veins (Single Box)`
  - `Show Vein & Distance Labels`
  - `Show Tracer Lines`
  - `Use 2D HUD Outlines (Needs Mod Menu Overlay)`
  - Individual toggles for all 10 ore families
- **No Unrequested HUD Buttons**: Zero floating buttons or keybinds are added by default.

## Verified `1.26.60.04` Anchors

- `LocalPlayer::normalTick` -> `0xaf5e460`
- `BlockSource::getBlock(BlockPos const&)` -> `0x10371248`
- `LevelRenderer::renderLevel` -> `0xb7a35d8`
- `ClientInstance::update` -> `0x9b7b278`
- `Tessellator::begin` / `color` / `vertex` -> `0xac2a408` / `0xac2a9bc` / `0xac2ac58`
- `mce::MeshHelpers::renderMeshImmediately` -> `0xb769d24`
- `mce::RenderMaterialGroup::common` -> `0x14098800` (`"ui_fill_color"` depth-disabled pass)

## Building from Source

Requirements: Linux x86_64, `cmake`, `ninja`, `g++` (C++20), `python3` (`pyelftools`), and Android NDK r28c (fetched automatically by `bootstrap.sh`).

```bash
./scripts/bootstrap.sh
./scripts/build.sh --clean
```

The verified `.levipack` package is written to `packages/`.

## License

Released under the [MIT License](./LICENSE).
