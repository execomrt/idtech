# idtech

`idtech` is a C++20 library for loading and parsing classic id Tech BSP
content, initially targeting Quake 1 and Quake 2.

The library is designed to plug into an existing renderer. It does not own a
window, graphics API, render loop, or engine-specific resource system. Instead,
it parses game data into renderer-neutral scene structures and uses callbacks
or handles where integration with GPU resources is required.

## Goals

- Parse Quake 1 and Quake 2 BSP files safely from memory.
- Expose geometry, materials, textures, lightmaps, BSP nodes, leaves, and
  visibility data through a stable C++ API.
- Keep file-format parsing independent from rendering.
- Support OpenGL, Direct3D, Vulkan, Metal, software renderers, and custom
  engines without introducing a dependency on any of them.
- Allow applications to control resource creation, destruction, and drawing.
- Preserve enough BSP information for visibility traversal and collision work,
  rather than producing only a flattened mesh.

Quake 3 is not part of the initial rewrite. Files under `archives/` are legacy
reference implementations and may contain useful format knowledge, but they
are not the architecture to reproduce directly.

## Intended architecture

The library is split into three layers:

1. **Format readers** validate binary input and decode Quake-specific lumps.
2. **Common scene data** represents vertices, indices, surfaces, materials,
   lightmaps, BSP trees, leaves, and visibility in renderer-neutral types.
3. **Renderer integration** supplies optional callbacks for creating textures
   and buffers while leaving command submission and drawing to the host engine.

Parsing should remain usable without a GPU or renderer. Renderer callbacks must
therefore be optional and must not be required to inspect a BSP file.

## CPU lightmap atlas

`LightmapPacker` is a standalone C++20 utility in
`src/lightmap_packer.hpp`. It accepts arbitrary 8-bit CPU images with a common
channel count and returns:

- a power-of-two CPU texture;
- one placement rectangle per input image;
- configurable padding with edge-texel extrusion for bilinear filtering.

The current BSP parser extracts the first static light style from each lit
face, packs those lightmaps, and writes atlas coordinates into vertex
`texCoord1`. Quake BSP29 produces an R8 atlas, while BSP30 produces an RGB8
atlas. The renderer callback receives this CPU atlas during
`BSPRenderer::createResources()`.

Animated or additional Quake light styles remain in the original BSP light
data but are not packed yet.

## Renderer callback contract

`BSPRenderer` follows a config/callback model. The host supplies resource
callbacks and a draw callback:

```cpp
idtech::BSPRenderer::Config config;

config.resourceCallbacks.createTexture = createTexture;
config.resourceCallbacks.destroyTexture = destroyTexture;
config.resourceCallbacks.createBuffer = createBuffer;
config.resourceCallbacks.destroyBuffer = destroyBuffer;

config.callbacks.drawPrimitive =
    [](const idtech::BSPDrawBatch& batch) {
        // Bind the vertex and index buffers.
        // Bind the albedo and lightmap textures.
        // Draw batch.indexCount indices from batch.firstIndex.
    };
```

The lifecycle is explicit:

```cpp
renderer.setConfig(config);
renderer.loadScene(bspBytes);
renderer.createResources();
renderer.render();
renderer.destroyResources();
```

For unculled rendering, indices are regrouped into contiguous material
batches. Each batch carries opaque albedo and lightmap texture handles, buffer
handles, its material index, and its first-index/index-count range.

Albedo handles remain invalid until BSP/WAD indexed textures are converted to
CPU textures. The callback contract is ready for that next step.

## BSP exploration and caller-owned frustum testing

`BSPExplorer` handles format-specific visibility work:

- locating the camera leaf through the BSP planes and nodes;
- decompressing the Quake potentially-visible-set data;
- resolving visible leaves through marksurfaces;
- producing unique visible face indices.

The library does not implement a camera frustum. The host supplies the leaf
AABB test:

```cpp
idtech::BSPRenderer::Config config;

config.settings.usePotentiallyVisibleSet = true;
config.settings.allVisibleOnMissingPVS = true;

config.callbacks.leafInFrustum =
    [&](const idtech::Bounds& bounds) {
        return cameraFrustum.intersects(bounds.minimum, bounds.maximum);
    };
```

Render only the explored ranges:

```cpp
renderer.renderVisible(cameraPosition);
```

The vertex and material-sorted index buffers remain static. Visible faces are
merged into contiguous index ranges and submitted through the same
`drawPrimitive` callback. Calling `renderer.render()` still draws the complete
unculled world.

## Building

Generate a Visual Studio 2022 solution:

```powershell
cmake --preset vs2022
```

Build it:

```powershell
cmake --build --preset vs2022-debug
cmake --build --preset vs2022-release
```

The generated solution is written to `build/vs2022/idtech.sln`.

Run the loader test against the bundled Quake 1 fixture:

```powershell
.\build\vs2022\Debug\idtech_loader_test.exe
```

Run the standalone lightmap packing test:

```powershell
.\build\vs2022\Debug\idtech_lightmap_packer_test.exe
```

The test defaults to `assets/start.bsp` and also opens `assets/skins.wad` to
report how many BSP material names resolve through the external texture
archive. Different BSP and WAD files can be passed as arguments:

```powershell
.\build\vs2022\Debug\idtech_loader_test.exe path\to\map.bsp path\to\textures.wad
```

The WAD parser also has a focused smoke test:

```powershell
.\build\vs2022\Debug\idtech_wad_test.exe
```

`start.bsp` is a Quake BSP29 map with embedded texture data. To exercise the
external WAD lookup used by the archived demo, run the bundled BSP30/WAD pair:

```powershell
.\build\vs2022\Debug\idtech_loader_test.exe .\assets\de_train.bsp .\assets\skins.wad
```

Consumers will link the library through the CMake target:

```cmake
target_link_libraries(my_renderer PRIVATE idtech::idtech)
```

## Project status

This repository is being reconstructed from recovered and archived source.
The current library and loader test compile, and the Quake 1 parser can load
`assets/start.bsp`. The next milestones are:

- complete texture, lightmap, entity, and visibility handling for Quake 1;
- implement the Quake 2 parser;
- define a small, stable renderer-integration API;
- add malformed-input checks and tests using known BSP files;
- add a minimal sample integration without making it part of the library.

## Design constraints

- C++20 is the minimum language standard.
- The public library must not depend on V3X or another rendering engine.
- Binary data must be bounds-checked before structures or lump contents are
  accessed.
- On-disk structures and public runtime structures should remain separate.
- Format-specific details belong in their respective Quake 1 or Quake 2
  modules.
- No graphics API calls belong in the parsers.

Quake and the original game data formats are associated with id Software. This
project is an independent loader/parser library and is not affiliated with or
endorsed by id Software.
