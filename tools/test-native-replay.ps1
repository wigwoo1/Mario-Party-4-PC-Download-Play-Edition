param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'No installed MSVC x64 toolchain found.' }
& (Join-Path $installation 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
# A unique run directory keeps exclusive-creation tests repeatable without deleting files.
$runDirectory = Join-Path $OutputDirectory ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
Push-Location $runDirectory
try {
    & cl /nologo /TC "/I$repo/extern/libco" "$repo/tools/test_coroutine_contract.c" "$repo/extern/libco/libco.c" /Fe:coroutine.exe
    if ($LASTEXITCODE -ne 0) { throw 'Coroutine contract compilation failed.' }
    & ./coroutine.exe
    if ($LASTEXITCODE -ne 0) { throw 'Coroutine contract failed.' }
    $includes = @("/I$repo/include", "/I$repo/extern/aurora/include")
    & cl /nologo /TC /DTARGET_PC /DNON_MATCHING @includes "/I$repo/extern" "$repo/tools/test_process_scheduler.c" "$repo/src/game/process.c" "$repo/src/game/memory.c" "$repo/extern/libco/libco.c" /Fe:scheduler.exe
    if ($LASTEXITCODE -ne 0) { throw 'Scheduler contract compilation failed.' }
    & ./scheduler.exe
    if ($LASTEXITCODE -ne 0) { throw 'Scheduler contract failed.' }
    & cl /nologo /std:c++20 /EHsc "$repo/tools/test_trace_codec.cpp" /Fe:codec.exe
    if ($LASTEXITCODE -ne 0) { throw 'Trace codec compilation failed.' }
    & ./codec.exe
    if ($LASTEXITCODE -ne 0) { throw 'Trace codec tests failed.' }
    & cl /nologo /std:c++20 /EHsc /DTARGET_PC @includes "$repo/tools/test_input_trace.cpp" "$repo/src/port/input_trace.cpp" /Fe:trace.exe
    if ($LASTEXITCODE -ne 0) { throw 'Trace integration compilation failed.' }
    & ./trace.exe (Join-Path $runDirectory 'fixtures')
    if ($LASTEXITCODE -ne 0) { throw 'Trace integration tests failed.' }
    foreach ($version in @(0, 2)) {
        & cl /nologo /DTARGET_PC /DTARGET_DOL "/DVERSION=$version" @includes "/I$repo/extern/aurora/include/dolphin" "$repo/tools/test_random_seed.c" "$repo/src/game/frand.c" "$repo/src/port/board_random.c" "$repo/src/port/simulation_clock.c" "/Fe:random-$version.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Random seed compilation failed.' }
        & "./random-$version.exe"
        if ($LASTEXITCODE -ne 0) { throw 'Random seed/clock tests failed.' }
    }
    Write-Host "All replay diagnostics tests passed. Test artifacts: $runDirectory"
} finally { Pop-Location }
