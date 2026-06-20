# AGENTS.md

## Project mission

Rewrite the recovered code as a clean C++20 library that loads and parses
Quake 1 and Quake 2 data and can be plugged into an existing renderer.

The primary output is renderer-neutral scene and BSP data. The library must not
become a standalone engine, viewer, or graphics abstraction layer.

## Scope

In scope:

- Quake 1 BSP parsing.
- Quake 2 BSP parsing.
- Entity text, geometry, texture metadata, lightmaps, BSP trees, leaves,
  clusters, and potentially-visible-set data.
- WAD/PAK support when needed to resolve map resources.
- Safe parsing from caller-owned memory or byte buffers.
- Optional callbacks or opaque handles for host renderer resources.
- Tests against sample and malformed files.

Out of scope for the initial rewrite:

- Quake 3 BSP support.
- Owning a window, swap chain, render loop, or graphics context.
- Direct OpenGL, Direct3D, Vulkan, Metal, or engine-specific calls in parsers.
- Requiring the host application to use the library's renderer wrapper.

## Architecture rules

Keep these concerns separate:

- `bsp_quake1.*`: Quake 1 on-disk structures and decoding.
- `bsp_quake2.*`: Quake 2 on-disk structures and decoding.
- Common scene types: normalized runtime data consumed by applications.
- Resource integration: optional callbacks for textures and buffers.
- Examples: host-side usage only; never required by the library target.

On-disk structures are untrusted input. Do not expose pointers into a byte
buffer as long-lived public data. Validate every lump offset, size, element
count, index, and arithmetic operation before use.

Parsing must work without renderer callbacks. GPU upload is a separate,
explicit operation controlled by the caller.

`LightmapPacker` is a general CPU utility. Keep it independent from BSP disk
structures and renderer callbacks. Its output owns its pixel memory and uses
edge extrusion around padded rectangles to remain safe under bilinear
filtering.

`BSPRenderer` owns no backend objects directly. Buffers and textures are opaque
handles created and destroyed through configured resource callbacks. Rendering
calls `drawPrimitive` once per generated `BSPDrawBatch`; the host binds the
supplied albedo/lightmap handles and draws the supplied index range.

`BSPExplorer` owns Quake BSP/PVS traversal but not frustum mathematics. Pass
leaf bounds to the host-provided `leafInFrustum` callback. Keep camera-frustum
types and conventions out of the idtech public API.

## Public API direction

Prefer an API shaped around:

- loading from `std::span<const std::byte>` or an equivalent byte view;
- a parse result that reports useful errors rather than only `true`/`false`;
- immutable access to parsed scene data;
- explicit ownership and lifetime rules;
- opaque renderer resource handles;
- callbacks supplied by the embedding application;
- no global state.

Avoid leaking packed file structures, graphics API types, or archived V3X
types into public headers.

## Implementation priorities

1. Make the common types and Quake 1 parser compile.
2. Add strict BSP header and lump validation.
3. Verify Quake 1 geometry, texture coordinates, lightmaps, and visibility.
4. Implement Quake 2 using the same common scene representation.
5. Add PAK/WAD resource lookup where parser functionality requires it.
6. Stabilize the renderer integration after CPU-side parsing is tested.

Correct parsing and clear ownership are more important than preserving the
shape of the recovered implementation.

## Build and verification

Configure Visual Studio 2022:

```powershell
cmake --preset vs2022
```

Build:

```powershell
cmake --build --preset vs2022-debug
```

Run the default Quake 1 smoke test:

```powershell
.\build\vs2022\Debug\idtech_loader_test.exe
```

Run the focused WAD2/WAD3 smoke test:

```powershell
.\build\vs2022\Debug\idtech_wad_test.exe
```

For parser changes, add or run tests that cover:

- a valid representative BSP;
- truncated files;
- invalid lump offsets and lengths;
- invalid cross-lump indices;
- integer overflow in counts and byte sizes;
- missing optional texture or lighting data.

Do not treat successful compilation as sufficient verification for binary
parsing changes.

## Repository notes

- `src/` contains the active rewrite.
- `archives/` is reference material only.
- `assets/` contains local fixtures and must not be assumed redistributable.
- `example.cpp` is illustrative host code and is not part of the library.
- The current recovered source is incomplete; fix inconsistencies deliberately
  instead of adding placeholders merely to silence compiler errors.
