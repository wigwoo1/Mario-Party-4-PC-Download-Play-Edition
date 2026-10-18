param(
    [Parameter(Mandatory=$true)][string]$EmsdkRoot,
    [Parameter(Mandatory=$true)][string]$Python,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$Node = 'node',
    [switch]$Browser
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$sdk = (Resolve-Path -LiteralPath $EmsdkRoot).Path
$run = Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $run | Out-Null
$oldConfig = $env:EM_CONFIG
try {
    $env:EM_CONFIG = Join-Path $sdk '.emscripten'
    & $Python (Join-Path $sdk 'upstream/emscripten/emcc.py') "$repo/tools/test_coroutine_contract.c" "$repo/src/port/wasm_coroutine.c" "-I$repo/extern/libco" -O1 -sASYNCIFY -sASSERTIONS=2 -sSTACK_OVERFLOW_CHECK=2 -sENVIRONMENT=node -o (Join-Path $run 'contract.js')
    if ($LASTEXITCODE -ne 0) { throw 'WebAssembly coroutine compilation failed.' }
    & $Node (Join-Path $run 'contract.js')
    if ($LASTEXITCODE -ne 0) { throw 'WebAssembly coroutine contract failed.' }
    if ($Browser) {
        & $Python (Join-Path $sdk 'upstream/emscripten/emcc.py') "$repo/tools/test_coroutine_contract.c" "$repo/src/port/wasm_coroutine.c" "-I$repo/extern/libco" -O1 -sASYNCIFY -sASSERTIONS=2 -sSTACK_OVERFLOW_CHECK=2 -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 -o (Join-Path $run 'contract.mjs')
        if ($LASTEXITCODE -ne 0) { throw 'Browser coroutine compilation failed.' }
        & $Node "$repo/tools/test-wasm-browser.cjs" $run
        if ($LASTEXITCODE -ne 0) { throw 'Browser coroutine contract failed.' }
    }
    Write-Host "WebAssembly artifacts: $run"
} finally { $env:EM_CONFIG = $oldConfig }
