# RecastJS WASM

Rebuilds the Cocos-compatible `navigation/recast.js` from this repository's Recast/Detour sources.

The generated module keeps the existing plugin API:

- `NavMesh.build(..., rcConfig, OffMeshLinkConfig)`
- `OffMeshLinkConfig.prototype.GetInstance(...)`
- `Crowd.getPath` / `Crowd.agentStop`

## Build

From `recastjs/`:

```powershell
.\build.ps1
```

The script uses, in order:

1. A local `emcc` / `emcmake` on PATH
2. `recastjs/emsdk` if present
3. Docker image `emscripten/emsdk:3.1.64`

### Local Emscripten

```powershell
git clone https://github.com/emscripten-core/emsdk.git recastjs/emsdk
cd recastjs/emsdk
.\emsdk install 3.1.64
.\emsdk activate 3.1.64
.\emsdk_env.ps1
cd ..
.\build.ps1
```

### Docker

```powershell
docker pull emscripten/emsdk:3.1.64
.\build.ps1
```

The single-file output is copied to `navigation/recast.js`.
