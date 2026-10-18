param(
    [string]$Python = 'python',
    [string]$Disc,
    [string]$BuildDirectory,
    [string]$InstallDirectory,
    [ValidateRange(1,64)][int]$Jobs = 8,
    [switch]$ConfigureOnly,
    [switch]$SkipAuroraPatches,
    [switch]$Fresh
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if (!$BuildDirectory) { $BuildDirectory = Join-Path $repo 'build/native' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Microsoft C++ Build Tools and Windows SDK first.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'No installed MSVC x64 toolchain found.' }
& (Join-Path $installation 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
foreach ($tool in @('cl.exe', 'rc.exe', 'mt.exe', 'cmake.exe', 'ninja.exe')) {
    if (!(Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Missing $tool. Wait for Build Tools and Windows SDK installation to finish."
    }
}
$pythonCommand = Get-Command $Python -ErrorAction Stop
$env:PATH = (Split-Path $pythonCommand.Source -Parent) + ';' + $env:PATH
# Trust only this checkout in this process; do not modify global Git settings.
$env:GIT_CONFIG_COUNT = '1'
$env:GIT_CONFIG_KEY_0 = 'safe.directory'
$env:GIT_CONFIG_VALUE_0 = $repo.Replace('\','/')
Push-Location $repo
try {
    # Use this only when building an already-patched checkout whose local Aurora
    # changes are authoritative. The default keeps the normal patch validation.
    if (!$SkipAuroraPatches) {
        foreach ($patchName in @('aurora-keyboard-taps.patch', 'aurora-browser-surface.patch', 'aurora-wasm-portability.patch', 'aurora-no-card-storage.patch', 'aurora-z24-copy.patch', 'aurora-browser-minimum-size.patch')) {
            $auroraPatch = Join-Path $repo ('patches/' + $patchName)
            if (Test-Path -LiteralPath $auroraPatch) {
                & git -C extern/aurora apply --ignore-whitespace --reverse --check $auroraPatch 2>$null
                if ($LASTEXITCODE -ne 0) {
                    & git -C extern/aurora apply --ignore-whitespace --check $auroraPatch
                    if ($LASTEXITCODE -ne 0) { throw 'Aurora patch conflicts with local changes.' }
                    & git -C extern/aurora apply --ignore-whitespace $auroraPatch
                    if ($LASTEXITCODE -ne 0) { throw 'Could not apply Aurora patch.' }
                }
            }
        }
    }
    if ($Disc) {
        & $pythonCommand.Source tools/inspect_disc.py $Disc --output build-disc-preflight.json
        if ($LASTEXITCODE -ne 0) { throw 'Disc preflight failed.' }
    }
    $configureArguments = @('--preset', 'windows-msvc-relwithdebinfo', '-B', $BuildDirectory)
    if ($InstallDirectory) {
        $InstallDirectory = [IO.Path]::GetFullPath($InstallDirectory)
        $configureArguments += "-DCMAKE_INSTALL_PREFIX=$InstallDirectory"
    }
    if ($Fresh) { $configureArguments += '--fresh' }
    & cmake @configureArguments
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    if (!$ConfigureOnly) {
        & cmake --build $BuildDirectory --target partyboard --parallel $Jobs
        if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
        & cmake --install $BuildDirectory
        if ($LASTEXITCODE -ne 0) { throw 'Native installation failed.' }
        $effectiveInstallDirectory = if ($InstallDirectory) { $InstallDirectory } else { Join-Path $repo 'build/install' }
        Write-Host "Native application: $(Join-Path $effectiveInstallDirectory 'partyboard.exe')"
    }
} finally { Pop-Location }


