param([Parameter(Mandatory=$true)][string]$EmsdkRoot,[Parameter(Mandatory=$true)][string]$Python,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$sdk=(Resolve-Path $EmsdkRoot).Path
$run=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $run | Out-Null
$oldConfig=$env:EM_CONFIG; $oldPath=$env:PATH
try {
    $env:EM_CONFIG=Join-Path $sdk '.emscripten'
    $env:PATH=(Split-Path ([IO.Path]::GetFullPath($Python)))+';'+$oldPath
    & $Python "$sdk/upstream/emscripten/emcc.py" "$repo/tools/test_wasm_sdl.c" -sUSE_SDL=3 -sASSERTIONS=2 -sEXPORTED_RUNTIME_METHODS=requestFullscreen -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 -o "$run/contract.mjs"
    if($LASTEXITCODE -ne 0){throw 'Browser SDL compilation failed'}
    & node "$repo/tools/test-wasm-browser.cjs" $run sdl
    if($LASTEXITCODE -ne 0){throw 'Browser SDL test failed'}
    Write-Host "SDL artifacts: $run"
} finally { $env:EM_CONFIG=$oldConfig; $env:PATH=$oldPath }

