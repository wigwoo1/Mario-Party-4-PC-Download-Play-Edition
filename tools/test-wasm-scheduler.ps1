param(
    [Parameter(Mandatory=$true)][string]$EmsdkRoot,
    [Parameter(Mandatory=$true)][string]$Python,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$sdk=(Resolve-Path $EmsdkRoot).Path
$run=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $run | Out-Null
$oldConfig=$env:EM_CONFIG
try {
    $env:EM_CONFIG=Join-Path $sdk '.emscripten'
    $sources=@("$repo/tools/test_process_scheduler.c", "$repo/src/game/process.c", "$repo/src/game/memory.c", "$repo/src/port/wasm_coroutine.c")
    $flags=@('-DTARGET_PC','-DNON_MATCHING',"-I$repo/include","-I$repo/extern","-I$repo/extern/libco","-I$repo/extern/aurora/include",'-O1','-sASYNCIFY','-sASSERTIONS=2','-sSTACK_OVERFLOW_CHECK=2')
    & $Python "$sdk/upstream/emscripten/emcc.py" @sources @flags -sENVIRONMENT=node -o "$run/scheduler.js"
    if($LASTEXITCODE -ne 0){throw 'Scheduler compilation failed'}
    & node "$run/scheduler.js"
    if($LASTEXITCODE -ne 0){throw 'Scheduler contract failed'}
    & $Python "$sdk/upstream/emscripten/emcc.py" @sources @flags -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 -o "$run/contract.mjs"
    if($LASTEXITCODE -ne 0){throw 'Browser scheduler compilation failed'}
    & node "$repo/tools/test-wasm-browser.cjs" $run scheduler
    if($LASTEXITCODE -ne 0){throw 'Browser scheduler contract failed'}
    Write-Host "Scheduler artifacts: $run"
} finally { $env:EM_CONFIG=$oldConfig }

