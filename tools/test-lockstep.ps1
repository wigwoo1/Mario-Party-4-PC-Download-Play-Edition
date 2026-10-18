param([Parameter(Mandatory=$true)][string]$OutputDirectory, [string]$Python, [string]$Node, [string]$JsonInclude)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$jsonIncludeCandidates = @(@(
    $JsonInclude,
    (Join-Path $repo 'build/windows-msvc-relwithdebinfo/_deps/json-src/include'),
    (Join-Path $repo 'build/_deps/json-src/include')
) | Where-Object { $_ -and (Test-Path -LiteralPath (Join-Path $_ 'nlohmann/json.hpp')) })
if ($jsonIncludeCandidates.Count -eq 0) { throw 'nlohmann/json.hpp was not found. Pass -JsonInclude with its include directory.' }
$jsonIncludeDirectory = $jsonIncludeCandidates | Select-Object -First 1
$jsonIncludeFlag = "/I$jsonIncludeDirectory"
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'No installed MSVC x64 toolchain found.' }
& (Join-Path $installation 'Common7/Tools/Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$runDirectory = Join-Path $OutputDirectory ([guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
Push-Location $runDirectory
try {
    & cl /nologo /std:c++20 /EHsc /W4 $jsonIncludeFlag "$repo/tools/test_reconnect_policy.cpp" /Fe:reconnect-policy.exe
    if ($LASTEXITCODE -ne 0) { throw 'Reconnect policy compilation failed.' }
    & ./reconnect-policy.exe
    if ($LASTEXITCODE -ne 0) { throw 'Reconnect policy tests failed.' }
    & cl /nologo /std:c++20 /EHsc /W4 $jsonIncludeFlag "$repo/tools/test_lockstep.cpp" /Fe:lockstep.exe
    if ($LASTEXITCODE -ne 0) { throw 'Lockstep compilation failed.' }
    & ./lockstep.exe
    if ($LASTEXITCODE -ne 0) { throw 'Lockstep tests failed.' }
    & cl /nologo /std:c++20 /EHsc /W4 /DTARGET_PC $jsonIncludeFlag "/I$repo/extern/aurora/include" "$repo/tools/test_pad_exchange.cpp" /Fe:pad-exchange.exe
    if ($LASTEXITCODE -ne 0) { throw 'Pad exchange compilation failed.' }
    & ./pad-exchange.exe
    if ($LASTEXITCODE -ne 0) { throw 'Pad exchange tests failed.' }
    & cl /nologo /std:c++20 /EHsc /W4 /DTARGET_PC $jsonIncludeFlag "/I$repo/extern/aurora/include" "$repo/tools/test_controller_routes.cpp" /Fe:controller-routes.exe
    if ($LASTEXITCODE -ne 0) { throw 'Controller routing compilation failed.' }
    & ./controller-routes.exe
    if ($LASTEXITCODE -ne 0) { throw 'Controller routing tests failed.' }
    & cl /nologo /std:c++20 /EHsc /W4 /DTARGET_PC $jsonIncludeFlag "/I$repo/extern/aurora/include" "$repo/tools/test_input_wait.cpp" /Fe:input-wait.exe
    if ($LASTEXITCODE -ne 0) { throw 'Input wait compilation failed.' }
    & ./input-wait.exe
    if ($LASTEXITCODE -ne 0) { throw 'Input wait tests failed.' }
    if ($Python) {
        & cl /nologo /std:c++20 /EHsc /W4 /DTARGET_PC $jsonIncludeFlag "/I$repo/extern/aurora/include" "$repo/tools/lockstep_socket_probe.cpp" /Fe:socket-probe.exe
        if ($LASTEXITCODE -ne 0) { throw 'Socket probe compilation failed.' }
        & $Python "$repo/tools/test_socket_probe.py" (Join-Path $runDirectory 'socket-probe.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Socket probe tests failed.' }
    }
    if ($Node) {
        & cl /nologo /std:c++20 /EHsc /W4 /DTARGET_PC $jsonIncludeFlag "/I$repo/extern/aurora/include" "$repo/tools/capability_vectors.cpp" /Fe:capability-vectors.exe
        if ($LASTEXITCODE -ne 0) { throw 'Capability vector compilation failed.' }
        & $Node "$repo/web/test-native-capabilities.mjs" (Join-Path $runDirectory 'capability-vectors.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Native/browser capability interoperability failed.' }
        & cl /nologo /std:c++20 /EHsc /W4 $jsonIncludeFlag "$repo/tools/lockstep_vectors.cpp" /Fe:lockstep-vectors.exe
        if ($LASTEXITCODE -ne 0) { throw 'Lockstep vector compilation failed.' }
        & $Node "$repo/web/test-native-lockstep.mjs" (Join-Path $runDirectory 'lockstep-vectors.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Native/browser lockstep conformance failed.' }
        & cl /nologo /std:c++20 /EHsc /W4 $jsonIncludeFlag "$repo/tools/reconnect_vectors.cpp" /Fe:reconnect-vectors.exe
        if ($LASTEXITCODE -ne 0) { throw 'Reconnect vector compilation failed.' }
        & $Node "$repo/web/test-native-reconnect.mjs" (Join-Path $runDirectory 'reconnect-vectors.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Native/browser reconnect conformance failed.' }
    }
    Write-Host "Test artifacts: $runDirectory"
} finally { Pop-Location }
