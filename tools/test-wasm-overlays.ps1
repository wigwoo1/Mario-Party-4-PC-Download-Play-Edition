param([Parameter(Mandatory=$true)][string]$EmsdkRoot,[Parameter(Mandatory=$true)][string]$Python,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
$sdk=(Resolve-Path $EmsdkRoot).Path
$run=Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory $run | Out-Null
$oldConfig=$env:EM_CONFIG
$oldPath=$env:PATH
try {
    # Emscripten's file-packager launcher invokes python.exe by name.
    $env:PATH=(Split-Path ([IO.Path]::GetFullPath($Python))) + ';' + $oldPath
    $env:EM_CONFIG=Join-Path $sdk '.emscripten'
    $emcc=Join-Path $sdk 'upstream/emscripten/emcc.py'
    foreach($side in @(@('a',11),@('b',22))) {
        & $Python $emcc "$repo/tools/wasm_overlay_probe/side.c" "$repo/src/REL/m420dll/rand.c" -DTARGET_PC "-DMODULE_SEED=$($side[1])" "-I$repo/extern/aurora/include" -sSIDE_MODULE=1 '-Wl,-Bsymbolic' -o "$run/side-$($side[0]).wasm"
        if($LASTEXITCODE -ne 0){throw 'Side module compilation failed'}
    }
    $flags=@('-sMAIN_MODULE=1','-sASSERTIONS=2','--embed-file',"$run/side-a.wasm@/side-a.wasm",'--embed-file',"$run/side-b.wasm@/side-b.wasm")
    & $Python $emcc "$repo/tools/wasm_overlay_probe/main.c" @flags -sENVIRONMENT=node -o "$run/probe.js"
    if($LASTEXITCODE -ne 0){throw 'Overlay probe compilation failed'}
    & node "$run/probe.js"
    if($LASTEXITCODE -ne 0){throw 'Overlay probe failed'}
    & $Python $emcc "$repo/tools/wasm_overlay_probe/main.c" @flags -sENVIRONMENT=web -sMODULARIZE -sEXPORT_ES6 -o "$run/contract.mjs"
    if($LASTEXITCODE -ne 0){throw 'Browser overlay compilation failed'}
    & node "$repo/tools/test-wasm-browser.cjs" $run overlays
    if($LASTEXITCODE -ne 0){throw 'Browser overlay probe failed'}
    Write-Host "Overlay artifacts: $run"
} finally { $env:EM_CONFIG=$oldConfig; $env:PATH=$oldPath }


