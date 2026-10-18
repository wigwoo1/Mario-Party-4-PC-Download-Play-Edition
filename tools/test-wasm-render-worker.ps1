param([Parameter(Mandatory=$true)][string]$EmsdkRoot,[Parameter(Mandatory=$true)][string]$Python,[Parameter(Mandatory=$true)][string]$TracyInclude,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$sdk=(Resolve-Path $EmsdkRoot).Path
$run=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $run | Out-Null
$oldConfig=$env:EM_CONFIG
try {
    $env:EM_CONFIG=Join-Path $sdk '.emscripten'
    & $Python "$sdk/upstream/emscripten/emcc.py" "$repo/tools/test_wasm_render_worker.cpp" "$repo/extern/aurora/lib/gfx/render_worker.cpp" "-I$TracyInclude" -sDEFAULT_TO_CXX -std=c++20 -sASYNCIFY -sASSERTIONS=2 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 -o "$run/contract.mjs"
    if($LASTEXITCODE -ne 0){throw 'Render worker compilation failed'}
    & node "$repo/tools/test-wasm-browser.cjs" $run worker
    if($LASTEXITCODE -ne 0){throw 'Browser render worker test failed'}
    Write-Host "Worker artifacts: $run"
} finally { $env:EM_CONFIG=$oldConfig }


