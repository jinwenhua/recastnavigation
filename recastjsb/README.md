# RecastJSB (Cocos native)

Cocos **JSB** bindings for the same RecastJS API used by `navigation/recastJsPlugin.ts`. WASM lives in [`recastjs/`](../recastjs/README.md) only; this folder does not touch the Emscripten build.

## Integrate

1. In the Cocos native CMake (usually `native/engine/common/CMakeLists.txt`):

```cmake
include(<path-to-recastnavigation>/recastjsb/cmake/recastjsb_native.cmake)
recastjsb_add_native(${ENGINE_NAME})
```

Use your real engine/app target name if it is not `ENGINE_NAME`.

2. In `jsb_module_register.cpp`, register the module (see `jsb/jsb_module_register.inl`):

```cpp
#include "jsb_recast.h"

register_all_recastjsb(ns);
```

This exposes global `Recast` with `Recast._isJSB === true`, matching the WASM module’s classes (`NavMesh`, `Crowd`, `OffMeshLinkConfig`, etc.).

## JSB-only code

- `include/recastjsb_offmesh.h`, `src/recastjsb_offmesh.cpp` — owned off-mesh arrays for `OffMeshLinkConfig.GetInstance` (WASM uses `onload.js` + heap instead).
- `jsb/jsb_recast.cpp` — Cocos `se::` bindings.

## Plugin

`navigation/recastJsPlugin.ts` branches on `Recast._isJSB` for navmesh serialize/deserialize; Web keeps using `recast.js` and Emscripten `_malloc` / `HEAPU8`.
