# M4-1 PIE proof: two players in one process (a listen server in the editor viewport plus one client window), each
# driven through its own AgentConsole file (commands.txt / commands.client1.txt), the dev-cheats gate, and captures
# taken by each instance of its own viewport (AirlockCapture). Run from the repo root with a clean StationHub open
# (port from AIRLOCK_MCP_PORT, default 8000):
#   powershell -ExecutionPolicy Bypass -File Scripts/verify-two-clients.ps1
# The editor's play settings are switched to listen server / 2 clients / one process for the run and restored after.
# Captures and the log excerpt land in Saved/AgentCaptures/M4-1. HUD text is capture-only; the script asserts log lines.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$root = Split-Path $PSScriptRoot -Parent
$proofDir = Join-Path $root 'Saved/AgentCaptures/M4-1'
$consoleDir = Join-Path $root 'Saved/AgentConsole'
$commandFiles = @{ server = (Join-Path $consoleDir 'commands.txt'); client = (Join-Path $consoleDir 'commands.client1.txt') }
New-Item -ItemType Directory -Force -Path $proofDir, $consoleDir | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false
$playSettings = @{ refPath = '/Script/UnrealEd.Default__LevelEditorPlaySettings' }
$playKeys = @('PlayNetMode', 'PlayNumberOfClients', 'RunUnderOneProcess', 'bLaunchSeparateServer', 'NewWindowWidth', 'NewWindowHeight')
$playDefaults = @{ PlayNetMode = 'PIE_Standalone'; PlayNumberOfClients = 1; RunUnderOneProcess = $true; bLaunchSeparateServer = $false; NewWindowWidth = 1280; NewWindowHeight = 720 }
$script:SavedPlay = $null
# PIE start persists the play settings to this ini; a restore through set_properties only changes memory, so the
# section is snapshotted here and written back in finally (otherwise a relaunched editor comes up in two-client mode).
$playIni = Join-Path $root 'Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini'
$playIniPattern = '(?ms)^\[/Script/UnrealEd\.LevelEditorPlaySettings\]\r?\n.*?(?=^\[|\z)'
$script:SavedPlayIni = $null

function Send-TwoClientCommand([string]$Who, [string]$Command) {
    $path = $commandFiles[$Who]
    if (Test-Path -LiteralPath $path) { throw "A previous $Who command is still pending" }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($path, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $path) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $path) { throw "Console command was not consumed by the $Who instance: $Command" }
    Write-Host "$Who> $Command"
}
function Get-TwoClientProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 1000 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}
function Wait-TwoClientProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart, [string]$MustContain = '') {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-TwoClientProofLines $Pattern $Since) | Where-Object { -not $MustContain -or $_ -like "*$MustContain*" } | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line$(if ($MustContain) { " containing '$MustContain'" }) within $Seconds seconds"
}
function Assert-TwoClientWhere([string]$Who, [string]$Map) {
    Send-TwoClientCommand $Who 'AirlockWhere'
    $line = Wait-TwoClientProofLine ': Where:' 10 $script:LastCommandAt "map=$Map "
    if ($line -notmatch 'pawn=AirlockCharacter') { throw "$Who has no pawn on $Map`: $line" }
    return $line
}
function Save-TwoClientCapture([string]$Who, [string]$Label, [string]$Map, [int]$SettleSeconds = 4) {
    Start-Sleep -Seconds $SettleSeconds
    $null = Assert-TwoClientWhere $Who $Map
    Send-TwoClientCommand $Who "AirlockCapture $Label"
    $line = Wait-TwoClientProofLine ": Capture $Label " 10 $script:LastCommandAt
    if ($line -notmatch 'saved (\d+)x(\d+)') { throw "Capture $Label failed: $line" }
    $source = Join-Path $root "Saved/AgentCaptures/$Label.png"
    Copy-Item -LiteralPath $source -Destination (Join-Path $proofDir "$Label.png") -Force
    Write-Host "Capture: $proofDir/$Label.png ($($matches[1])x$($matches[2]))"
}
function Get-PlaySettings {
    $r = Mcp-Call $TS_Object 'get_properties' @{ instance = $playSettings; properties = $playKeys }
    if (-not $r.ok) { throw "Play settings could not be read: $($r.text)" }
    $values = @{}
    $json = $r.json
    if ($json -and $json.PSObject.Properties['returnValue']) { $json = $json.returnValue }
    if ($json -is [string]) { $json = $json | ConvertFrom-Json }
    foreach ($key in $playKeys) {
        $value = $null
        if ($json -and $json.PSObject.Properties[$key]) { $value = $json.$key }
        elseif ($r.text -match "`"$key`"\s*:\s*(`"[^`"]*`"|[-0-9.]+|true|false)") { $value = $matches[1] | ConvertFrom-Json }
        if ($null -eq $value) { throw "Play setting $key not in: $($r.text)" }
        $values[$key] = $value
    }
    return $values
}
function Get-PlayIniSection {
    if (-not (Test-Path -LiteralPath $playIni)) { return $null }
    $m = [regex]::Match([IO.File]::ReadAllText($playIni), $playIniPattern)
    if ($m.Success) { return $m.Value } else { return $null }
}
function Restore-PlayIniSection([string]$Section) {
    if ($null -eq $Section -or -not (Test-Path -LiteralPath $playIni)) { return }
    $text = [IO.File]::ReadAllText($playIni)
    $m = [regex]::Match($text, $playIniPattern)
    if (-not $m.Success -or $m.Value -eq $Section) { return }
    [IO.File]::WriteAllText($playIni, $text.Substring(0, $m.Index) + $Section + $text.Substring($m.Index + $m.Length))
    Write-Host 'Play settings ini section restored'
}
function Set-PlaySettings($Values) {
    $ordered = [ordered]@{}
    foreach ($key in $playKeys) { $ordered[$key] = $Values[$key] }
    $r = Mcp-Call $TS_Object 'set_properties' @{ instance = $playSettings; values = ($ordered | ConvertTo-Json -Compress) }
    if (-not $r.ok) { throw "Play settings could not be set: $($r.text)" }
    Write-Host "Play settings: $($ordered | ConvertTo-Json -Compress)"
}

try {
    Mcp-Init
    if ((Mcp-Call $TS_App 'IsPIERunning' @{}).text -match 'true') { throw 'Stop the current PIE session first' }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Open StationHub first' }
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/Ships/Hauler_Mk1')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map must be clean before this proof" }
    }
    foreach ($file in $commandFiles.Values) { if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force } }

    $script:SavedPlay = Get-PlaySettings
    $script:SavedPlayIni = Get-PlayIniSection
    Write-Host "Play settings before: $(($script:SavedPlay.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join ' ')"
    # PlayNumberOfClients counts the listen server: 2 gives exactly one client window.
    Set-PlaySettings @{ PlayNetMode = 'PIE_ListenServer'; PlayNumberOfClients = 2; RunUnderOneProcess = $true; bLaunchSeparateServer = $false; NewWindowWidth = 960; NewWindowHeight = 540 }

    # The server plays in the viewport from the hub PlayerStart; client 1 gets its own window and spawns at the same start.
    $script:ProofStart = [DateTime]::UtcNow
    $start = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -5640; y = 1560; z = 118 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
    if (-not $start.ok) { throw "StartPIE failed: $($start.text)" }
    $script:StartedPIE = $true
    $null = Wait-TwoClientProofLine 'AgentConsole: ready' 30
    $null = Wait-TwoClientProofLine 'client 1: ready' 60 $script:ProofStart 'UEDPIE_1_StationHub'
    $null = Assert-TwoClientWhere 'server' 'UEDPIE_0_StationHub'
    $null = Assert-TwoClientWhere 'client' 'UEDPIE_1_StationHub'

    # The server books the Priority run; the client is the one who crosses Dock B, and everyone travels.
    Send-TwoClientCommand 'server' 'AirlockBook 2'
    Start-Sleep -Seconds 2
    $crossAt = [DateTime]::UtcNow
    Send-TwoClientCommand 'client' 'AirlockWarp -4700 1560 116'
    Start-Sleep -Seconds 1
    Send-TwoClientCommand 'client' 'AirlockWarp -4265 1560 116'
    $null = Wait-TwoClientProofLine ': Section Hauler_Mk1: voyage underway' 60 $crossAt
    $null = Wait-TwoClientProofLine 'client 1: ready' 60 $crossAt 'UEDPIE_1_Hauler_Mk1'
    Start-Sleep -Seconds 3
    $starts = @(Get-TwoClientProofLines ': Arrival spawn for' $crossAt)
    if ($starts.Count -lt 2) { throw "Expected an arrival spawn per player on the ship, saw $($starts.Count)" }
    $starts | ForEach-Object { Write-Host $_ }
    $null = Wait-TwoClientProofLine ': Arrival spawn for' 10 $crossAt 'stepped +120 cm aside'
    $null = Assert-TwoClientWhere 'server' 'UEDPIE_0_Hauler_Mk1'
    $null = Assert-TwoClientWhere 'client' 'UEDPIE_1_Hauler_Mk1'

    # The client carries and secures a cask; the server's model and both boards agree.
    Send-TwoClientCommand 'client' 'AirlockWarp -6330 1700 116'
    Start-Sleep -Seconds 2
    Send-TwoClientCommand 'client' 'AirlockCarry C1'
    $carry = Wait-TwoClientProofLine ': Carry C1 by' 10 $script:LastCommandAt
    # The shared log does not say which player acted: the server's own pawn still standing at its arrival spot does.
    $serverStill = Assert-TwoClientWhere 'server' 'UEDPIE_0_Hauler_Mk1'
    if ($serverStill -notmatch 'at X=-6660\.\d+ ') { throw "The server's pawn moved during the client's carry: $serverStill" }
    Send-TwoClientCommand 'server' 'AirlockCargoStatus'
    $null = Wait-TwoClientProofLine ': Cargo C1 Carried' 10 $script:LastCommandAt
    Send-TwoClientCommand 'client' 'AirlockWarp -6430 1400 116'
    Start-Sleep -Seconds 2
    Send-TwoClientCommand 'client' 'AirlockPlace Clamp_1'
    $null = Wait-TwoClientProofLine ': Cargo C1 Secured' 10 $script:LastCommandAt
    Send-TwoClientCommand 'client' 'AirlockLook 90'
    Save-TwoClientCapture 'client' 'm4-1-client-hold' 'UEDPIE_1_Hauler_Mk1'
    Send-TwoClientCommand 'server' 'AirlockWarp -6430 1700 116'
    Send-TwoClientCommand 'server' 'AirlockLook -90'
    Save-TwoClientCapture 'server' 'm4-1-server-hold' 'UEDPIE_0_Hauler_Mk1'

    # The client takes the bridge seat and sees the control board; then it sets the scrubbers from the console path.
    Send-TwoClientCommand 'client' 'AirlockWarp 1760 240 130'
    Start-Sleep -Seconds 2
    Send-TwoClientCommand 'client' 'AirlockSit Bridge'
    $null = Wait-TwoClientProofLine ': Seat Bridge taken by' 10 $script:LastCommandAt
    Save-TwoClientCapture 'client' 'm4-1-client-seat' 'UEDPIE_1_Hauler_Mk1' 5
    Send-TwoClientCommand 'client' 'AirlockStand'
    $null = Wait-TwoClientProofLine ': Seat Bridge released by' 10 $script:LastCommandAt
    Send-TwoClientCommand 'client' 'AirlockScrub Hazard'
    $null = Wait-TwoClientProofLine 'scrub=Hazard' 10 $script:LastCommandAt

    # The gate: with airlock.DevCheats at 0 the client's warp is refused on the server and its pawn stays put.
    Send-TwoClientCommand 'server' 'airlock.DevCheats 0'
    Start-Sleep -Seconds 1
    Send-TwoClientCommand 'client' 'AirlockWarp 0 0 0'
    $null = Wait-TwoClientProofLine ': Cheat AirlockWarp refused for' 10 $script:LastCommandAt
    $where = Assert-TwoClientWhere 'client' 'UEDPIE_1_Hauler_Mk1'
    if ($where -notmatch 'pawn=.*? at X=1790\.\d+ Y=240\.') { throw "Refused warp still moved the client (expected the seat's exit point): $where" }
    Send-TwoClientCommand 'server' 'airlock.DevCheats 1'
    Start-Sleep -Seconds 1

    # Dock: the clock runs out for everyone; both instances return to the hub.
    $dockAt = [DateTime]::UtcNow
    Send-TwoClientCommand 'server' 'AirlockVoyageClock 5'
    $result = Wait-TwoClientProofLine ': Voyage result:' 60 $dockAt
    $null = Wait-TwoClientProofLine 'client 1: ready' 60 $dockAt 'UEDPIE_1_StationHub'
    $null = Wait-TwoClientProofLine ': Arrival spawn for' 20 $dockAt 'stepped +120 cm aside'
    Start-Sleep -Seconds 3
    $null = Assert-TwoClientWhere 'server' 'UEDPIE_0_StationHub'
    $null = Assert-TwoClientWhere 'client' 'UEDPIE_1_StationHub'
    $editor = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $editorImage = $editor.images | Select-Object -First 1
    if ($editor.ok -and $editorImage) { Copy-Item -LiteralPath $editorImage -Destination (Join-Path $proofDir 'm4-1-editor-two-windows.png') -Force; Write-Host "Capture: $proofDir/m4-1-editor-two-windows.png" }
    else { Write-Host "Editor capture skipped: $($editor.text)" }

    Write-Host "TWO CLIENTS OK: server + client 1 on the hub -> the client crosses Dock B, both aboard -> client carries and secures C1 -> client at the seat -> client scrubs -> cheat refused at DevCheats 0 -> dock, both home, the second arrival steps aside"
}
finally {
    if ($script:StartedPIE) {
        # The cheat switch is process-wide and survives PIE: never leave it at 0 for the next proof in this editor.
        try { Send-TwoClientCommand 'server' 'airlock.DevCheats 1' } catch { Write-Host "Could not restore airlock.DevCheats: $_" }
        try { Get-TwoClientProofLines | Set-Content -LiteralPath (Join-Path $proofDir 'two-clients-log.txt') }
        finally {
            $stop = Mcp-Call $TS_App 'StopPIE' @{}
            if (-not $stop.ok) { throw "StopPIE failed: $($stop.text)" }
        }
    }
    if ($script:SavedPlay) {
        Set-PlaySettings $script:SavedPlay
        $after = Get-PlaySettings
        $mismatch = @($playKeys | Where-Object { "$($after[$_])" -ne "$($script:SavedPlay[$_])" })
        if ($mismatch.Count -gt 0) { Write-Host "Play settings did not restore ($($mismatch -join ', ')); writing the single-player defaults"; Set-PlaySettings $playDefaults }
    }
    Restore-PlayIniSection $script:SavedPlayIni
    foreach ($file in $commandFiles.Values) { if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force } }
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/Ships/Hauler_Mk1')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map is dirty after the proof" }
    }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Editor did not return to StationHub' }
}
