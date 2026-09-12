# Level-pass PIE proof (Claude, 2026-09-12). Run from the repo root with a clean StationHub open (set
# AIRLOCK_MCP_PORT first when the editor runs on another port):
#   powershell -ExecutionPolicy Bypass -File Scripts/verify-level-pass.ps1
# Proves that every ship module, line and the control station spawn where the layout puts them, that each line can be
# repaired from the walkway in front of it (engineering bay, main corridor), that the control-room seat works (sit,
# board, stand) and that the hub round trip still lands on both PlayerStarts. Fresh log lines only; captures under
# Saved/AgentCaptures/LevelPass. Slate snapshots omit the PIE HUD, so review the captures visually.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$proofDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentCaptures/LevelPass'
$commandPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false

# Where the crew stands to look at / repair each thing (x, y, z, yaw): the reader's side of every label.
$Look = @{
    Engineering = @(-1320, 1050, 120, -90)   # the L-corridor ("CENTRAL ENGINEERING") from its north end, looking south
    Coolant_1   = @(-1250, 430, 120, 180)    # facing the pipe along the west wall in front of the core band
    Power_1     = @(-1400, 450, 120, 0)      # facing the east-wall wire below life support
    Coolant_2   = @(-3320, 1600, 30, -90)    # main corridor, facing the south-wall pipe
    Power_2     = @(-2700, 1480, 30, 90)     # main corridor, facing the north-wall wire
    Hold        = @(-6300, 1500, 30, 90)     # cargo hold, facing the clamp controller
    Galley      = @(-1150, -20, 118, 180)    # junction, facing the galley cabinet in the west stub
    Bridge      = @(1600, 240, 120, 0)       # bridge door side, facing the terminal and the seat
}

function Send-LevelCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}

function Get-LevelProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 1000 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}

function Wait-LevelProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-LevelProofLines $Pattern $Since) | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line within $Seconds seconds"
}

function Save-LevelCapture([string]$Label) {
    $capture = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $source = $capture.images | Select-Object -First 1
    if (-not $capture.ok -or -not $source) { throw "Capture failed: $($capture.text). Restore the editor window and retry." }
    Copy-Item -LiteralPath $source -Destination (Join-Path $proofDir "$Label.png") -Force
    Write-Host "Capture: $proofDir/$Label.png"
}

function Assert-LevelLocation([string]$Map, [double]$ExpectedX) {
    $line = Wait-LevelProofLine 'Where:' 10 $script:LastCommandAt
    $position = [regex]::Match([string]$line, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+)')
    if ($line -notmatch "map=UEDPIE_\d+_$Map " -or -not $position.Success) { throw "Wrong arrival: $line" }
    $x = [double]::Parse($position.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $y = [double]::Parse($position.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ([math]::Abs($x - $ExpectedX) -gt 1 -or [math]::Abs($y - 1560) -gt 1) { throw "Arrival missed PlayerStart: $line" }
}

function Go-LevelSpot([string]$Name, [int]$Settle = 6) {
    $s = $Look[$Name]
    Send-LevelCommand "AirlockWarp $($s[0]) $($s[1]) $($s[2] + 100)"
    Start-Sleep -Seconds 2
    Send-LevelCommand "AirlockLook $($s[3])"
    Start-Sleep -Seconds $Settle
}

function Repair-LevelLine([string]$Line, [string]$Fault, [int]$HoldSeconds) {
    Send-LevelCommand "AirlockFault $Line $Fault"
    $null = Wait-LevelProofLine "Line $Line fault=$Fault" 5 $script:LastCommandAt
    Start-Sleep -Seconds 2
    Save-LevelCapture "$Line-$($Fault.ToLower())"
    Send-LevelCommand "AirlockHold $Line"
    $holdAt = $script:LastCommandAt
    $null = Wait-LevelProofLine "Hold $Line started by" 5 $holdAt
    $null = Wait-LevelProofLine "Hold $Line complete by" ($HoldSeconds + 4) $holdAt
    $null = Wait-LevelProofLine "Line $Line fault=None" 3 $holdAt
}

try {
    Mcp-Init
    if ((Mcp-Call $TS_App 'IsPIERunning' @{}).text -match 'true') { throw 'Stop the current PIE session first' }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Open StationHub first' }
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/Ships/Hauler_Mk1')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map must be clean before this proof" }
    }
    $script:ProofStart = [DateTime]::UtcNow
    $start = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -5640; y = 1560; z = 118 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
    if (-not $start.ok) { throw $start.text }
    $script:StartedPIE = $true
    Send-LevelCommand 'AirlockWhere'
    Assert-LevelLocation 'StationHub' -5640
    Send-LevelCommand 'AirlockSit Bridge'
    $null = Wait-LevelProofLine 'Seat Bridge rejected: no such station on this section' 5 $script:LastCommandAt
    Send-LevelCommand 'AirlockWarp -4700 1560 116'
    Send-LevelCommand 'AirlockBook 0'
    Start-Sleep -Seconds 2
    Send-LevelCommand 'AirlockWarp -4265 1560 116'
    $boardAt = $script:LastCommandAt
    $null = Wait-LevelProofLine 'Ship graph built: 7 modules, 4 lines \(section Hauler_Mk1\)' 45 $boardAt
    $null = Wait-LevelProofLine 'Control station Bridge at' 5 $boardAt
    Start-Sleep -Seconds 8
    Send-LevelCommand 'AirlockWhere'
    Assert-LevelLocation 'Hauler_Mk1' -6660
    Send-LevelCommand 'AirlockShipStatus'
    $statusAt = $script:LastCommandAt
    $null = Wait-LevelProofLine 'Line Power_2 fault=None' 5 $statusAt
    if (@(Get-LevelProofLines 'Ship \w+ Nominal cond=100' $statusAt).Count -ne 7) { throw 'Expected seven nominal modules' }

    # Cargo hold: the clamp controller stays in the west hall.
    Go-LevelSpot 'Hold'
    Save-LevelCapture 'hold-cargo-clamps'

    # Engineering (the L-corridor): the core band, tank, life support and engine node, and two repairable lines.
    Go-LevelSpot 'Engineering' 8
    Save-LevelCapture 'engineering-overview'
    Go-LevelSpot 'Coolant_1'
    Repair-LevelLine 'Coolant_1' 'Leak' 3
    Go-LevelSpot 'Power_1'
    Repair-LevelLine 'Power_1' 'Short' 4

    # Main corridor: a pipe on the south wall and a wire on the north wall, each a run from the bay.
    Go-LevelSpot 'Coolant_2'
    Repair-LevelLine 'Coolant_2' 'Leak' 3
    Go-LevelSpot 'Power_2'
    Repair-LevelLine 'Power_2' 'Short' 4

    # Galley in the junction stub, then the bridge: seat, board, stand.
    Go-LevelSpot 'Galley'
    Save-LevelCapture 'galley-stub'
    Go-LevelSpot 'Bridge' 8
    Save-LevelCapture 'bridge-station-free'
    Send-LevelCommand 'AirlockSit Bridge'
    $null = Wait-LevelProofLine 'Seat Bridge rejected: out of range' 5 $script:LastCommandAt
    Send-LevelCommand 'AirlockWarp 1760 240 130'
    Start-Sleep -Seconds 2
    Send-LevelCommand 'AirlockSit Bridge'
    $sitAt = $script:LastCommandAt
    $null = Wait-LevelProofLine 'Seat Bridge taken by AirlockCharacter' 5 $sitAt
    Start-Sleep -Seconds 4
    Send-LevelCommand 'AirlockWhere'
    $seated = Wait-LevelProofLine 'Where:' 5 $script:LastCommandAt
    if ($seated -notmatch 'at X=1930\.\d+ Y=240\.\d+') { throw "Seated pawn is not on the seat point: $seated" }
    Save-LevelCapture 'bridge-control-board'
    Send-LevelCommand 'AirlockStand'
    $standAt = $script:LastCommandAt
    $null = Wait-LevelProofLine 'Seat Bridge released by AirlockCharacter' 5 $standAt
    Start-Sleep -Seconds 2
    Send-LevelCommand 'AirlockWhere'
    $stood = Wait-LevelProofLine 'Where:' 5 $script:LastCommandAt
    if ($stood -notmatch 'at X=1790\.\d+ Y=240\.\d+') { throw "Pawn did not stand up at the exit point: $stood" }
    Save-LevelCapture 'bridge-station-released'

    # Home through the exit airlock.
    Send-LevelCommand 'AirlockWarp -7015 1560 116'
    $returnAt = $script:LastCommandAt
    $null = Wait-LevelProofLine 'Arrival spawn .*X=-5640\.' 45 $returnAt
    Start-Sleep -Seconds 8
    Send-LevelCommand 'AirlockWhere'
    Assert-LevelLocation 'StationHub' -5640
    $null = Wait-LevelProofLine 'Voyage .* complete' 5 $returnAt
    Write-Host 'LEVEL OK: hold -> engineering (2 lines repaired) -> corridor (2 lines repaired) -> galley -> bridge seat, board, stand -> hub PlayerStart'
}
finally {
    if ($script:StartedPIE) {
        try { Get-LevelProofLines | Set-Content -LiteralPath (Join-Path $proofDir 'level-pass-log.txt') -Encoding UTF8 }
        finally {
            $stop = Mcp-Call $TS_App 'StopPIE' @{}
            if (-not $stop.ok) { throw "StopPIE failed: $($stop.text)" }
        }
        foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/Ships/Hauler_Mk1')) {
            $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
            if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map is dirty after the proof" }
        }
        if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Editor did not return to StationHub' }
    }
}
