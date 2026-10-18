param([Parameter(Mandatory=$true)][string]$EmsdkRoot,[Parameter(Mandatory=$true)][string]$Python,[Parameter(Mandatory=$true)][string]$Dependencies,[Parameter(Mandatory=$true)][string]$BuildDirectory,[string]$NodLibrary,[string]$OverlayPackageDirectory,[string]$CacheDirectory,[string]$EmdawnWebGpuPort,[switch]$OptimizeRenderer)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$sdk=(Resolve-Path $EmsdkRoot).Path
$deps=(Resolve-Path $Dependencies).Path
$emdawnPort='emdawnwebgpu'
if($EmdawnWebGpuPort){$emdawnPort=(Resolve-Path $EmdawnWebGpuPort).Path}
$overlayPackage=$null
if($OverlayPackageDirectory){
  if(-not $NodLibrary){throw '-NodLibrary is required when building the production browser runtime.'}
  $overlayPackage=(Resolve-Path $OverlayPackageDirectory).Path
  if(-not (Test-Path -LiteralPath (Join-Path $overlayPackage 'bootDll.wasm'))){throw '-OverlayPackageDirectory must contain the complete compiled overlay Wasm package, including bootDll.wasm.'}
}
$build=[IO.Path]::GetFullPath($BuildDirectory)
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.CMake.Project -property installationPath
$cmakeRoot=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake'
$cmake=Join-Path $cmakeRoot 'CMake/bin/cmake.exe'
$ninja=Join-Path $cmakeRoot 'Ninja/ninja.exe'
$oldConfig=$env:EM_CONFIG; $oldCache=$env:EM_CACHE; $oldPath=$env:PATH
try {
  $env:EM_CONFIG=Join-Path $sdk '.emscripten'
  if($CacheDirectory){
    $cache=[IO.Path]::GetFullPath($CacheDirectory)
    New-Item -ItemType Directory -Force -Path $cache | Out-Null
    $env:EM_CACHE=$cache
  }
  $env:PATH=(Split-Path ([IO.Path]::GetFullPath($Python)))+';'+$oldPath
  $discArgs=@()
  if($NodLibrary){$discArgs=@("-DPARTYBOARD_NOD_WASM_LIBRARY=$((Resolve-Path $NodLibrary).Path)")}
  if($overlayPackage){
    $discArgs+= "-DPARTYBOARD_OVERLAY_PACKAGE_DIR=$overlayPackage"
    $discArgs+= "-DPARTYBOARD_RUNTIME_OUTPUT_DIRECTORY=$build/browser-runtime"
  }
  $rendererOptimization=if($OptimizeRenderer){'ON'}else{'OFF'}
  # Emscripten's single-thread runtime provides the C++ synchronization API
  # without a separate pthread library. Seed FindThreads explicitly because
  # some newer CMake bundles no longer detect that setup reliably.
  & $cmake -S "$repo/tools/browser-renderer" -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_TOOLCHAIN_FILE=$sdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake" "-DPARTYBOARD_DEPENDENCIES=$deps" "-DPARTYBOARD_EMDAWN_WEBGPU_PORT=$emdawnPort" "-DCMAKE_HAVE_LIBC_PTHREAD=1" -DCMAKE_BUILD_TYPE=Debug "-DPARTYBOARD_BROWSER_OPTIMIZE_RENDERER=$rendererOptimization" @discArgs
  if($LASTEXITCODE -ne 0){throw 'Browser renderer configure failed'}
  & $cmake --build $build --parallel 4
  if($LASTEXITCODE -ne 0){throw 'Browser renderer build failed'}
  & node "$repo/tools/test-wasm-browser.cjs" $build webgpu
  if($LASTEXITCODE -ne 0){throw 'Browser dependency contract failed'}
  & node "$repo/tools/test-wasm-browser.cjs" "$build/dependency-contract" dependencies
  if($LASTEXITCODE -ne 0){throw 'Browser library contract failed'}
  & node "$repo/tools/test-wasm-browser.cjs" "$build/settings-contract" settings
  if($LASTEXITCODE -ne 0){throw 'Browser settings contract failed'}
  & node "$repo/tools/test-wasm-browser.cjs" "$build/trace-contract" trace
  if($LASTEXITCODE -ne 0){throw 'Browser input trace contract failed'}
  & node "$repo/tools/test-wasm-browser.cjs" "$build/input-contract" inputmode
  if($LASTEXITCODE -ne 0){throw 'Browser input mode contract failed'}
  & node "$repo/tools/test-wasm-browser.cjs" "$build/rel-asset-contract" relassets
  if($LASTEXITCODE -ne 0){throw 'Browser REL asset contract failed'}
  & node "$repo/tools/test-wasm-browser.cjs" "$build/overlay-gate-contract" overlaygate
  if($LASTEXITCODE -ne 0){throw 'Browser overlay gate contract failed'}
  if(Select-String -Path (Join-Path $build 'CMakeCache.txt') -Pattern '^PARTYBOARD_NOD_WASM_LIBRARY:FILEPATH=.+') {
    & node "$repo/tools/test-wasm-browser.cjs" "$build/disc-contract" disc
    if($LASTEXITCODE -ne 0){throw 'Browser disc contract failed'}
    & node "$repo/tools/test-wasm-browser.cjs" "$build/asset-mount-contract" assetmount
    if($LASTEXITCODE -ne 0){throw 'Browser verified asset mount contract failed'}
    & node "$repo/tools/test-wasm-browser.cjs" "$build/asset-mount-contract" assetworker
    if($LASTEXITCODE -ne 0){throw 'Browser worker asset mount contract failed'}
  }
  if($overlayPackage){
    & $cmake --build $build --target partyboard_browser_runtime --parallel 4
    if($LASTEXITCODE -ne 0){throw 'Production browser runtime link failed'}
    & node "$repo/tools/browser-runtime-package-browser-test.cjs" "$build/browser-runtime" $overlayPackage
    if($LASTEXITCODE -ne 0){throw 'Production browser runtime package test failed'}
  }
  & node "$repo/tools/test-wasm-browser.cjs" "$build/aurora-contract" aurora
  if($LASTEXITCODE -ne 0){throw 'Aurora browser startup contract failed'}
} finally { $env:EM_CONFIG=$oldConfig; if($CacheDirectory){$env:EM_CACHE=$oldCache}; $env:PATH=$oldPath }
