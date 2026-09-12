# M3-2 PIE proof. Requires the editor on clean StationHub; uses only the file console.
# powershell -ExecutionPolicy Bypass -File Scripts/verify-ship-graph.ps1
# Evidence: Saved/AgentCaptures/M3-2. Fresh timestamps isolate this run and each command.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$proofDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentCaptures/M3-2'
$commandPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false

function Send-ShipCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}

function Get-ShipProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 500 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}

function Wait-ShipProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-ShipProofLines $Pattern $Since) | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line within $Seconds seconds"
}

function Save-ShipProofCapture([string]$Label) {
    $capture = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $source = $capture.images | Select-Object -First 1
    if (-not $capture.ok -or -not $source) { throw "Capture failed: $($capture.text). Restore the editor window and retry." }
    $destination = Join-Path $proofDir "$Label.png"
    Copy-Item -LiteralPath $source -Destination $destination -Force
    Write-Host "Capture: $destination"
}

function Assert-ShipProofLocation([string]$Map, [double]$ExpectedX) {
    # Command-file deletion precedes execution, and the MCP log view can lag by a frame.
    # Require a Where response stamped after this command, never an earlier arrival.
    $line = Wait-ShipProofLine 'Where:' 10 $script:LastCommandAt
    $position = [regex]::Match([string]$line, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+)')
    if ($line -notmatch "map=UEDPIE_\d+_$Map " -or -not $position.Success) { throw "Wrong arrival: $line" }
    $x = [double]::Parse($position.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $y = [double]::Parse($position.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ([math]::Abs($x - $ExpectedX) -gt 1 -or [math]::Abs($y - 1560) -gt 1) { throw "Arrival missed PlayerStart: $line" }
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
    Send-ShipCommand 'AirlockWhere'
    Assert-ShipProofLocation 'StationHub' -5640
    Send-ShipCommand 'AirlockShipStatus'
    $null = Wait-ShipProofLine 'Ship graph empty' 5 $script:LastCommandAt
    Send-ShipCommand 'AirlockWarp -4700 1560 116'
    Send-ShipCommand 'AirlockBook 0'
    Start-Sleep -Seconds 2
    Send-ShipCommand 'AirlockWarp -4265 1560 116'
    $null = Wait-ShipProofLine 'Ship graph built: 7 modules, 4 lines \(section Hauler_Mk1\)' 45
    Start-Sleep -Seconds 8
    Send-ShipCommand 'AirlockWhere'
    Assert-ShipProofLocation 'Hauler_Mk1' -6660
    Send-ShipCommand 'AirlockShipStatus'
    foreach ($id in @('PowerCore', 'CoolantLoop', 'LifeSupport', 'CargoClamps', 'Engines', 'Nav', 'Galley')) {
        $null = Wait-ShipProofLine "Ship $id Nominal cond=100\.00" 5 $script:LastCommandAt
    }
    Save-ShipProofCapture 'modules-nominal'
    Send-ShipCommand 'AirlockFault Coolant_1 Leak'
    $null = Wait-ShipProofLine 'Line Coolant_1 fault=Leak' 5 $script:LastCommandAt
    Send-ShipCommand 'AirlockDamage CoolantLoop 50'
    $null = Wait-ShipProofLine 'Ship CoolantLoop Degraded' 5 $script:LastCommandAt
    Start-Sleep -Seconds 12
    Send-ShipCommand 'AirlockShipStatus'
    $core = Wait-ShipProofLine 'Ship PowerCore .*cond=' 5 $script:LastCommandAt
    $condition = [regex]::Match($core, 'cond=([0-9.]+)')
    if (-not $condition.Success -or [double]::Parse($condition.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture) -ge 100) {
        throw "Coolant degradation did not drift PowerCore: $core"
    }
    Send-ShipCommand 'AirlockWhere'
    Save-ShipProofCapture 'coolant-leak-and-degraded-labels'
    Send-ShipCommand 'AirlockHold Coolant_1'
    $null = Wait-ShipProofLine 'Hold Coolant_1 rejected: out of range' 5 $script:LastCommandAt
    Send-ShipCommand 'AirlockWarp -1300 430 118'   # engineering walkway in front of Coolant_1 (level pass)
    Start-Sleep -Seconds 8
    Send-ShipCommand 'AirlockWhere'
    $null = Wait-ShipProofLine 'Where:.*Hauler_Mk1.*pawn=.*at X=-1300\.' 5 $script:LastCommandAt
    Send-ShipCommand 'AirlockHold Coolant_1'
    $holdStartedAt = $script:LastCommandAt
    $null = Wait-ShipProofLine 'Hold Coolant_1 started .*duration=3\.0s' 5 $holdStartedAt
    Start-Sleep -Milliseconds 800
    Save-ShipProofCapture 'hold-progress'
    $null = Wait-ShipProofLine 'Hold Coolant_1 complete by' 6 $holdStartedAt
    $null = Wait-ShipProofLine 'Line Coolant_1 fault=None' 1 $holdStartedAt
    Send-ShipCommand 'AirlockRepairModule CoolantLoop 100'
    $null = Wait-ShipProofLine 'Ship CoolantLoop Nominal cond=100\.00' 5 $script:LastCommandAt
    Send-ShipCommand 'AirlockShipStatus'
    $null = Wait-ShipProofLine 'Line Coolant_1 fault=None' 5 $script:LastCommandAt
    Send-ShipCommand 'AirlockWarp -6660 1560 116'
    Start-Sleep -Seconds 8
    Send-ShipCommand 'AirlockWhere'
    Save-ShipProofCapture 'repaired-labels-no-steam'
    Send-ShipCommand 'AirlockWarp -7015 1560 116'
    $returnAt = $script:LastCommandAt
    $null = Wait-ShipProofLine 'Arrival spawn .*X=-5640\.' 45 $returnAt
    Start-Sleep -Seconds 8
    Send-ShipCommand 'AirlockWhere'
    Assert-ShipProofLocation 'StationHub' -5640
    $null = Wait-ShipProofLine 'Voyage .* complete' 5 $returnAt
    Send-ShipCommand 'AirlockShipStatus'
    $null = Wait-ShipProofLine 'Ship graph empty' 5 $script:LastCommandAt
    Save-ShipProofCapture 'returned-to-hub'
    Write-Host 'GRAPH OK: seven nominal modules -> coolant leak -> degraded coolant drifts core -> hold repairs line -> coolant restored -> hub PlayerStart, empty graph'
}
finally {
    if ($script:StartedPIE) {
        try { Get-ShipProofLines | Set-Content -LiteralPath (Join-Path $proofDir 'ship-graph-log.txt') -Encoding UTF8 }
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