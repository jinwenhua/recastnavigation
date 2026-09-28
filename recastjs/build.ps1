param(
    [string]$Image = "emscripten/emsdk:3.1.64"
)

$ErrorActionPreference = "Stop"
$recastjsRoot = $PSScriptRoot
$repoRoot = Resolve-Path (Join-Path $recastjsRoot "..")
$outJs = Join-Path $repoRoot "navigation\recast.js"
$buildDir = Join-Path $recastjsRoot "build"

function Copy-Output {
    if (-not (Test-Path (Join-Path $buildDir "recast.js"))) {
        throw "Build finished but recast.js was not produced."
    }
    Copy-Item (Join-Path $buildDir "recast.js") $outJs -Force
    Write-Host "Updated $outJs"
}

if (Get-Command emcmake -ErrorAction SilentlyContinue) {
    Write-Host "Building with local Emscripten..."
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    Push-Location $recastjsRoot
    try {
        emcmake cmake -B build -DCMAKE_BUILD_TYPE=Release
        cmake --build build
    }
    finally {
        Pop-Location
    }
    Copy-Output
    return
}

if (Test-Path (Join-Path $recastjsRoot "emsdk\emsdk_env.ps1")) {
    Write-Host "Activating recastjs/emsdk..."
    . (Join-Path $recastjsRoot "emsdk\emsdk_env.ps1")
    if (Get-Command emcmake -ErrorAction SilentlyContinue) {
        New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
        Push-Location $recastjsRoot
        try {
            emcmake cmake -B build -DCMAKE_BUILD_TYPE=Release
            cmake --build build
        }
        finally {
            Pop-Location
        }
        Copy-Output
        return
    }
}

if (Get-Command docker -ErrorAction SilentlyContinue) {
    Write-Host "Building with Docker image $Image..."
    $src = ($repoRoot.Path -replace '\\', '/')
    docker run --rm `
        -v "${src}:/src" `
        -w /src/recastjs `
        $Image `
        bash -lc "rm -rf build && mkdir -p build && emcmake cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build"
    if ($LASTEXITCODE -ne 0) {
        throw "Docker build failed."
    }
    Copy-Output
    return
}

throw @"
Emscripten is not available.

Install one of:
  1) Emscripten SDK: https://emscripten.org/docs/getting_started/downloads.html
  2) Docker image: docker pull emscripten/emsdk:3.1.64

Then re-run: recastjs/build.ps1
"@
