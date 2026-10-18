param(
    [Parameter(Mandatory=$true)][string]$Source,
    [Parameter(Mandatory=$true)][string]$Destination
)
$ErrorActionPreference = 'Stop'
$sourcePath = (Resolve-Path -LiteralPath $Source).Path.TrimEnd('\', '/')
$destinationPath = [IO.Path]::GetFullPath($Destination).TrimEnd('\', '/')
if (!(Test-Path -LiteralPath $sourcePath -PathType Container)) { throw 'Source must be a directory.' }
if (Test-Path -LiteralPath $destinationPath) { throw 'Destination must not already exist.' }
if (Test-Path -LiteralPath ($destinationPath + '.manifest.json')) { throw 'Destination manifest must not already exist.' }
if ($destinationPath.StartsWith($sourcePath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Destination must be outside the source profile.'
}
$entries = @(Get-Item -LiteralPath $sourcePath) + @(Get-ChildItem -LiteralPath $sourcePath -Recurse -Force)
if ($entries | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) {
    throw 'Profile contains a link or junction; refusing to follow it.'
}
New-Item -ItemType Directory -Path $destinationPath | Out-Null
$manifest = @()
foreach ($entry in $entries | Where-Object { !$_.PSIsContainer } | Sort-Object FullName) {
    $relative = [IO.Path]::GetRelativePath($sourcePath, $entry.FullName)
    $target = Join-Path $destinationPath $relative
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    $before = (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $entry.FullName -Destination $target
    $after = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    $sourceAfter = (Get-FileHash -LiteralPath $entry.FullName -Algorithm SHA256).Hash
    if ($before -ne $after -or $before -ne $sourceAfter) { throw "Profile changed while copying: $relative" }
    $manifest += [pscustomobject]@{ path = $relative; bytes = $entry.Length; sha256 = $after }
}
# Keep the manifest outside the profile so cloning it does not change profile contents.
$manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath ($destinationPath + '.manifest.json') -Encoding utf8
Write-Host "Verified profile copy: $destinationPath ($($manifest.Count) files)"
