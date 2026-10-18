param(
    [ValidateSet('Native','Keyboard','Browser')][string]$Mode = 'Native',
    [string]$RuntimeDirectory,
    [string]$Disc,
    [ValidateRange(1,65535)][int]$Port = 8784,
    [string]$ListenAddress = '127.0.0.1',
    [string]$TlsCertificate,
    [string]$TlsKey,
    [string]$LobbyWebsite,
    [switch]$LocalOnly,
    [switch]$Inspect
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$outputsRoot = Split-Path $projectRoot -Parent
if (!$RuntimeDirectory) {
    $configuration = Join-Path $outputsRoot 'native-runtime-path.txt'
    if (Test-Path -LiteralPath $configuration) {
        $configuredRuntime = Get-Content -LiteralPath $configuration | Where-Object { $_.Trim() } | Select-Object -First 1
        if (!$configuredRuntime) { throw 'The runtime folder setting is empty. Edit outputs/native-runtime-path.txt.' }
        $RuntimeDirectory = $configuredRuntime.Trim()
        if (![IO.Path]::IsPathRooted($RuntimeDirectory)) { $RuntimeDirectory = Join-Path $outputsRoot $RuntimeDirectory }
    } else { $RuntimeDirectory = Join-Path $projectRoot 'build/install' }
}
$runtimeRoot = [IO.Path]::GetFullPath($RuntimeDirectory)
$required = @('partyboard.exe','dol.dll','initial_pipeline_cache.db','initial_pipeline_cache.json')
if ($Mode -eq 'Browser') { $required += @('native-session-package.json','browser-runtime/contract.mjs','browser-runtime/contract.wasm') }
foreach ($relative in $required) {
    if (!(Test-Path -LiteralPath (Join-Path $runtimeRoot $relative) -PathType Leaf)) {
        throw "Required runtime file is missing: $relative. Set outputs/native-runtime-path.txt to the complete current runtime folder."
    }
}
$executable = Join-Path $runtimeRoot 'partyboard.exe'
$arguments = @()
if ($Mode -eq 'Keyboard') { $arguments += '--keyboard' }
if ($Mode -ne 'Browser' -and !$Disc) {
    $discConfiguration = Join-Path $outputsRoot 'game-disc-path.txt'
    if (Test-Path -LiteralPath $discConfiguration) {
        $configuredDisc = Get-Content -LiteralPath $discConfiguration | Where-Object { $_.Trim() } | Select-Object -First 1
        if ($configuredDisc) { $Disc = $configuredDisc.Trim() }
    }
}
if ($Disc -and (Test-Path -LiteralPath $Disc -PathType Leaf)) { $arguments += @('--disc',[IO.Path]::GetFullPath($Disc)) }
$configuredLobbyWebsite = $null
if ($Mode -ne 'Browser' -and !$LocalOnly) {
    if (!$LobbyWebsite) {
        $lobbyConfiguration = Join-Path $outputsRoot 'lobby-service-url.txt'
        if (Test-Path -LiteralPath $lobbyConfiguration -PathType Leaf) {
            if ((Get-Item -LiteralPath $lobbyConfiguration).Length -gt 4096) { throw 'The lobby website setting is too large. Use one website address.' }
            $LobbyWebsite = (Get-Content -LiteralPath $lobbyConfiguration -Raw).Trim()
            if (!$LobbyWebsite) { throw 'The lobby website setting is empty. Enter the shared lobby website address.' }
        } elseif ($env:PARTYBOARD_LOBBY_URL) { $LobbyWebsite = $env:PARTYBOARD_LOBBY_URL }
    }
    if ($LobbyWebsite) {
        $lobbyUri = $null
        if ($LobbyWebsite.Length -gt 2048 -or ![Uri]::TryCreate($LobbyWebsite.Trim(),[UriKind]::Absolute,[ref]$lobbyUri) -or
            $lobbyUri.Scheme -notin @('http','https') -or !$lobbyUri.Host -or $lobbyUri.UserInfo -or $lobbyUri.Query -or $lobbyUri.Fragment -or
            $lobbyUri.AbsolutePath -notin @('/','/index.html') -or $lobbyUri.Port -lt 1) {
            throw 'Use the shared lobby website address, without passwords, a private join link or extra page paths.'
        }
        $lobbyHost = $lobbyUri.DnsSafeHost
        $lobbyIp = $null
        $ipv6Loopback = [Net.IPAddress]::TryParse($lobbyHost,[ref]$lobbyIp) -and $lobbyIp.Equals([Net.IPAddress]::IPv6Loopback)
        if ($ipv6Loopback) { $lobbyHost = '::1' }
        if ($lobbyUri.Scheme -ne 'https' -and $lobbyHost -notin @('localhost','127.0.0.1','::1')) {
            throw 'A lobby website on another computer requires HTTPS. HTTP is only supported for testing on this computer.'
        }
        $configuredLobbyWebsite = $lobbyUri.GetLeftPart([UriPartial]::Authority) + '/'
        # Windows PowerShell expands ::1; retain the native client's accepted loopback spelling.
        if ($ipv6Loopback) {
            $lobbyPort = if ($lobbyUri.IsDefaultPort) { '' } else { ':' + $lobbyUri.Port }
            $configuredLobbyWebsite = $lobbyUri.Scheme + '://[::1]' + $lobbyPort + '/'
        }
        $env:PARTYBOARD_LOBBY_URL = $configuredLobbyWebsite
    }
}
if ($LocalOnly) { Remove-Item Env:PARTYBOARD_LOBBY_URL -ErrorAction SilentlyContinue }
function Find-PartyboardNode {
    $nodeCommand = Get-Command node -ErrorAction SilentlyContinue
    $nodeCandidates = @((Join-Path $runtimeRoot 'node.exe'),
        (Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node.exe'))
    if ($nodeCommand) { $nodeCandidates += $nodeCommand.Source }
    $nodeCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}
$automaticHosting = $Mode -ne 'Browser' -and !$configuredLobbyWebsite -and !$LocalOnly
if ($Mode -eq 'Browser') {
    $ipAddress = $null
    if ($ListenAddress -ne 'localhost' -and ![Net.IPAddress]::TryParse($ListenAddress,[ref]$ipAddress)) { throw 'ListenAddress must be localhost or an IP address.' }
    if ([bool]$TlsCertificate -ne [bool]$TlsKey) { throw 'HTTPS requires both -TlsCertificate and -TlsKey.' }
    $isLoopback = $ListenAddress -eq 'localhost' -or ($ipAddress -and [Net.IPAddress]::IsLoopback($ipAddress))
    if (!$isLoopback -and !$TlsCertificate) { throw 'Use -TlsCertificate and -TlsKey when listening for guests on another device.' }
    foreach ($tlsFile in @($TlsCertificate,$TlsKey)) {
        if ($tlsFile -and !(Test-Path -LiteralPath $tlsFile -PathType Leaf)) { throw "HTTPS file is missing: $tlsFile" }
    }
    $env:PARTYBOARD_LISTEN_ADDRESS = $ListenAddress
    if ($TlsCertificate) {
        $env:PARTYBOARD_TLS_CERTIFICATE = [IO.Path]::GetFullPath($TlsCertificate)
        $env:PARTYBOARD_TLS_KEY = [IO.Path]::GetFullPath($TlsKey)
    } else { Remove-Item Env:PARTYBOARD_TLS_CERTIFICATE,Env:PARTYBOARD_TLS_KEY -ErrorAction SilentlyContinue }
    $executable = Find-PartyboardNode
    if (!$executable) { throw 'Node.js is required for the local browser page. Install Node.js or provide node.exe beside the native runtime.' }
    $arguments = @((Join-Path $projectRoot 'web/serve.cjs'))
    $env:PARTYBOARD_RUNTIME_PACKAGE = Join-Path $runtimeRoot 'native-session-package.json'
    $env:PARTYBOARD_PORT = [string]$Port
    Remove-Item Env:PARTYBOARD_RUNTIME_DIRECTORY,Env:PARTYBOARD_TEST_RUNTIME_DIRECTORY -ErrorAction SilentlyContinue
}
if ($Inspect) {
    [pscustomobject]@{mode=$Mode;runtimeDirectory=$runtimeRoot;executable=$executable;arguments=$arguments;
        lobbyWebsite=$configuredLobbyWebsite;
        automaticHosting=[bool]$automaticHosting;
        runtimePackage=if($Mode -eq 'Browser'){$env:PARTYBOARD_RUNTIME_PACKAGE}else{$null};
        url=if($Mode -eq 'Browser' -and $ListenAddress -ne '0.0.0.0' -and $ListenAddress -ne '::'){
            $scheme=if($TlsCertificate){'https'}else{'http'}
            $urlHost=if($ipAddress -and $ipAddress.AddressFamily -eq [Net.Sockets.AddressFamily]::InterNetworkV6){"[$ListenAddress]"}else{$ListenAddress}
            "${scheme}://${urlHost}:$Port"
        }else{$null};
        listenAddress=if($Mode -eq 'Browser'){$ListenAddress}else{$null};https=[bool]($Mode -eq 'Browser' -and $TlsCertificate);launched=$false} | ConvertTo-Json -Depth 5
    exit 0
}
if ($automaticHosting) {
    $nodeExecutable = Find-PartyboardNode
    if (!$nodeExecutable) { throw 'The automatic hosting runtime is missing. Restore the complete package, or use -LocalOnly for offline play.' }
    $arguments = @((Join-Path $PSScriptRoot 'host-game.cjs'),$runtimeRoot,(Join-Path $outputsRoot 'hosting-state')) + $arguments
    $executable = $nodeExecutable
}
Push-Location $runtimeRoot
try {
    if ($automaticHosting) {
        . (Join-Path $PSScriptRoot 'hosting-process-job.ps1')
        exit [Partyboard.HostingJob]::Run($executable,[string[]]$arguments,$runtimeRoot)
    }
    & $executable @arguments; exit $LASTEXITCODE
}
finally { Pop-Location }
