param(
  [Parameter(Mandatory=$true)][string]$SourceDirectory,
  [Parameter(Mandatory=$true)][string]$RustRoot,
  [Parameter(Mandatory=$true)][string]$EmsdkRoot,
  [Parameter(Mandatory=$true)][string]$Python
)
$ErrorActionPreference='Stop'
$source=(Resolve-Path $SourceDirectory).Path
$rust=(Resolve-Path $RustRoot).Path
$sdk=(Resolve-Path $EmsdkRoot).Path
$patch=Join-Path $PSScriptRoot 'nod-dol-bounds.patch'
$revision=& git -c "safe.directory=$source" -C $source rev-parse HEAD
if($LASTEXITCODE -ne 0 -or $revision -ne 'dc18d2ff129f05228b8510ea092d8b24c290a49a') {
  throw 'Disc source must be the pinned nod v2.0.0-alpha.10 commit'
}
& git -c "safe.directory=$source" -C $source apply --reverse --check $patch 2>$null
if($LASTEXITCODE -ne 0) {
  & git -c "safe.directory=$source" -C $source apply --check $patch
  if($LASTEXITCODE -ne 0){throw 'DOL bounds patch conflicts with source changes'}
  & git -c "safe.directory=$source" -C $source apply $patch
  if($LASTEXITCODE -ne 0){throw 'Failed to apply DOL bounds patch'}
}
$oldPath=$env:PATH; $oldConfig=$env:EM_CONFIG
$oldRust=$env:RUSTUP_HOME; $oldCargo=$env:CARGO_HOME
try {
  $env:RUSTUP_HOME=Join-Path $rust 'rustup'
  $env:CARGO_HOME=Join-Path $rust 'cargo'
  $env:EM_CONFIG=Join-Path $sdk '.emscripten'
  $env:PATH=(Join-Path $env:CARGO_HOME 'bin')+';'+(Join-Path $sdk 'upstream/emscripten')+';'+(Split-Path ([IO.Path]::GetFullPath($Python)))+';'+$oldPath
  & (Join-Path $env:CARGO_HOME 'bin/cargo.exe') +1.90.0 rustc --manifest-path (Join-Path $source 'Cargo.toml') -p nod-ffi --target wasm32-unknown-emscripten --no-default-features --release --locked --crate-type staticlib
  if($LASTEXITCODE -ne 0){throw 'Browser disc library build failed'}
  $archive=Join-Path $source 'target/wasm32-unknown-emscripten/release/libnod.a'
  if(!(Test-Path -LiteralPath $archive)){throw 'Browser disc archive was not produced'}
  Write-Output "Browser disc archive: $archive"
} finally {
  $env:PATH=$oldPath; $env:EM_CONFIG=$oldConfig
  $env:RUSTUP_HOME=$oldRust; $env:CARGO_HOME=$oldCargo
}
