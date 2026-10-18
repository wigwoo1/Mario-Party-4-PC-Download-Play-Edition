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
    & $Python "$sdk/upstream/emscripten/emcc.py" "$repo/tools/test_wasm_webgpu.cpp" -sDEFAULT_TO_CXX --use-port=emdawnwebgpu -std=c++20 -sASYNCIFY -sASSERTIONS=2 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 -o "$run/contract.mjs"
    if($LASTEXITCODE -ne 0){throw 'Browser WebGPU compilation failed'}
    & node "$repo/tools/test-wasm-browser.cjs" $run webgpu
    if($LASTEXITCODE -ne 0){throw 'Browser WebGPU test failed'}
    Write-Host "WebGPU artifacts: $run"
} finally { $env:EM_CONFIG=$oldConfig; $env:PATH=$oldPath }

