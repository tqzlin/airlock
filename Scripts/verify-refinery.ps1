# M3-7 PIE proof. Run from the repo root with a clean StationHub open (port from AIRLOCK_MCP_PORT, default 8000).
# powershell -ExecutionPolicy Bypass -File Scripts/verify-refinery.ps1 [-ProbePositions]
# Only the file console drives gameplay. Fresh logs and captures: Saved/AgentCaptures/M3-7. HUD text is capture-only.
# -ProbePositions instead drops a pawn on every crate, clamp and rack spot of the twelve-cask manifest and on the
# two hub sign spots, and checks each settles where the layout says.
param([switch]$ProbePositions)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$proofDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentCaptures/M3-7'
$commandPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false

function Send-RefineryCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}
function Get-RefineryProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 1000 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}
function Wait-RefineryProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-RefineryProofLines $Pattern $Since) | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line within $Seconds seconds"
}
function Save-RefineryCapture([string]$Label, [int]$SettleSeconds = 8) {
    Start-Sleep -Seconds $SettleSeconds
    Send-RefineryCommand 'AirlockWhere'
    $null = Wait-RefineryProofLine ': Where:' 5 $script:LastCommandAt
    $capture = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $source = $capture.images | Select-Object -First 1
    if (-not $capture.ok -or -not $source) { throw "Capture failed: $($capture.text). Restore the editor window and retry." }
    Copy-Item -LiteralPath $source -Destination (Join-Path $proofDir "$Label.png") -Force
    Write-Host "Capture: $proofDir/$Label.png"
}
function Assert-RefineryArrival([string]$Map, [double]$ExpectedX) {
    $line = Wait-RefineryProofLine ': Where:' 10 $script:LastCommandAt
    $position = [regex]::Match([string]$line, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+)')
    if ($line -notmatch "map=UEDPIE_\d+_$Map " -or -not $position.Success) { throw "Wrong arrival: $line" }
    $x = [double]::Parse($position.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $y = [double]::Parse($position.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ([math]::Abs($x - $ExpectedX) -gt 1 -or [math]::Abs($y - 1560) -gt 1) { throw "Arrival missed PlayerStart: $line" }
}
function Read-RefineryProbe([string]$Id, [double]$X, [double]$Y, [double]$DropZ, [double]$FloorZ) {
    Send-RefineryCommand "AirlockWarp $X $Y $DropZ"
    Start-Sleep -Seconds 4
    Send-RefineryCommand 'AirlockWhere'
    $line = Wait-RefineryProofLine ': Where:' 5 $script:LastCommandAt
    $match = [regex]::Match($line, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+) Z=([-0-9.]+)')
    if (-not $match.Success) { throw "Unparseable probe: $line" }
    $px = [double]::Parse($match.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $py = [double]::Parse($match.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    $pz = [double]::Parse($match.Groups[3].Value, [Globalization.CultureInfo]::InvariantCulture)
    $feet = $pz - 96
    if ([math]::Abs($px - $X) -gt 1 -or [math]::Abs($py - $Y) -gt 1 -or [math]::Abs($feet - $FloorZ) -gt 2) { throw "Spot $Id missed the floor: $line" }
    [pscustomobject]@{ Id = $Id; X = $px; Y = $py; LayoutZ = $FloorZ; PawnZ = $pz; FeetZ = $feet }
}

# The twelve-cask manifest (Ship/CargoLayouts.cpp) and the hub's sign spots (Station/StationSign.cpp).
$crateSpots = @(
    @('C1', -6100, 1700), @('C2', -5600, 1700), @('C3', -5300, 1700), @('C4', -5000, 1700),
    @('C5', -6100, 1900), @('C6', -5500, 1900), @('C7', -5350, 1900), @('C8', -5200, 1900),
    @('C9', -6100, 2000), @('C10', -5500, 2000), @('C11', -5350, 2000), @('C12', -5200, 2000))
$clampSpots = @(
    @('Clamp_1', -6200, 1400), @('Clamp_2', -5700, 1400), @('Clamp_3', -5200, 1400), @('Clamp_4', -5950, 1400), @('Clamp_5', -5450, 1400),
    @('Clamp_6', -5950, 1250), @('Clamp_7', -5650, 1250), @('Clamp_8', -5500, 1250), @('Clamp_9', -5050, 1200),
    @('ColdRack', -5000, 1300))
function Move-BesideRefinerySpot([string]$Id) {
    $spot = ($crateSpots + $clampSpots) | Where-Object { $_[0] -eq $Id } | Select-Object -First 1
    if (-not $spot) { throw "Unknown proof spot $Id" }
    Send-RefineryCommand "AirlockWarp $($spot[1] - 230) $($spot[2]) 116"
    Start-Sleep -Seconds 1
}

try {
    Mcp-Init
    if ((Mcp-Call $TS_App 'IsPIERunning' @{}).text -match 'true') { throw 'Stop the current PIE session first' }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Open StationHub first' }
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/Ships/Hauler_Mk1')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map must be clean before this proof" }
    }
    if ($ProbePositions) {
        # The hub signs first (this map is open), then the hold on the ship map.
        $script:ProofStart = [DateTime]::UtcNow
        $start = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -5640; y = 1560; z = 118 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
        if (-not $start.ok) { throw $start.text }
        $script:StartedPIE = $true
        $positions = @()
        # A sign hangs at Z 330; the probe checks the floor under it is the hall's walking surface.
        foreach ($spot in @(@('Sign_DockB', -4520, 1560), @('Sign_DockA', -6760, 1560))) {
            Send-RefineryCommand "AirlockWarp $($spot[1]) $($spot[2]) 300"
            Start-Sleep -Seconds 4
            Send-RefineryCommand 'AirlockWhere'
            $line = Wait-RefineryProofLine ': Where:' 5 $script:LastCommandAt
            Write-Host "  sign floor: $line"
        }
        Send-RefineryCommand 'AirlockWarp -4700 1560 116'
        Send-RefineryCommand 'AirlockBook 2'
        Start-Sleep -Seconds 2
        Send-RefineryCommand 'AirlockWarp -4265 1560 116'
        $null = Wait-RefineryProofLine ': Incident director started: crew=1 entries=4' 45
        Send-RefineryCommand 'AirlockVoyageClock 1200'
        foreach ($spot in ($crateSpots + $clampSpots)) { $positions += Read-RefineryProbe $spot[0] $spot[1] $spot[2] 300 -65 }
        $positions | Export-Csv -LiteralPath (Join-Path $proofDir 'refinery-position-probes.csv') -NoTypeInformation
        $positions | Format-Table | Out-Host
        Write-Host 'REFINERY POSITIONS OK: every spot of the twelve-cask manifest settles on the hall floor'
    }
    else {
        $script:ProofStart = [DateTime]::UtcNow
        $start = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -5640; y = 1560; z = 118 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
        if (-not $start.ok) { throw $start.text }
        $script:StartedPIE = $true
        Send-RefineryCommand 'AirlockWhere'
        Assert-RefineryArrival 'StationHub' -5640
        Send-RefineryCommand 'AirlockSaveReset'
        $null = Wait-RefineryProofLine ': Save reset: base fuel 200 t, runs 0' 5 $script:LastCommandAt
        Send-RefineryCommand 'AirlockBaseFuel'
        $null = Wait-RefineryProofLine ': Base fuel 200 t, runs 0' 5 $script:LastCommandAt
        # The Rig's name on the wall over Dock B, seen from the hall.
        Send-RefineryCommand 'AirlockWarp -4900 1560 116'
        Send-RefineryCommand 'AirlockLook 0 18'
        Save-RefineryCapture 'sign-dock-b'
        Send-RefineryCommand 'AirlockBoard'
        $null = Wait-RefineryProofLine ': Voyage board opened' 5 $script:LastCommandAt
        Save-RefineryCapture 'hub-board-fresh' 6
        Send-RefineryCommand 'AirlockBook 2'
        Start-Sleep -Seconds 2
        Send-RefineryCommand 'AirlockWarp -4265 1560 116'
        $null = Wait-RefineryProofLine ': Incident director started: crew=1 entries=4' 45
        Send-RefineryCommand 'AirlockWhere'
        Assert-RefineryArrival 'Hauler_Mk1' -6660
        Send-RefineryCommand 'AirlockVoyageClock 1200'
        $null = Wait-RefineryProofLine ': Cargo manifest: 12 casks, 10 slots' 5
        Send-RefineryCommand 'AirlockCargoStatus'
        $manifestAt = $script:LastCommandAt
        $null = Wait-RefineryProofLine ': Cargo slot ColdRack rack empty' 5 $manifestAt
        if (@(Get-RefineryProofLines ': Cargo C\d+ Loose temp=0' $manifestAt).Count -ne 12) { throw 'Expected twelve loose casks' }
        if (@(Get-RefineryProofLines ': Cargo slot Clamp_\d+ clamp empty' $manifestAt).Count -ne 9) { throw 'Expected nine empty clamps' }
        Send-RefineryCommand 'AirlockLook 0'
        Save-RefineryCapture 'priority-hold'
        Move-BesideRefinerySpot 'C1'
        Send-RefineryCommand 'AirlockCarry C1'
        $null = Wait-RefineryProofLine ': Carry C1 by' 5 $script:LastCommandAt
        Move-BesideRefinerySpot 'Clamp_1'
        Send-RefineryCommand 'AirlockPlace Clamp_1'
        $null = Wait-RefineryProofLine ': Cargo C1 Secured' 5 $script:LastCommandAt
        Move-BesideRefinerySpot 'C2'
        Send-RefineryCommand 'AirlockCarry C2'
        $null = Wait-RefineryProofLine ': Carry C2 by' 5 $script:LastCommandAt
        Move-BesideRefinerySpot 'ColdRack'
        Send-RefineryCommand 'AirlockPlace ColdRack'
        $null = Wait-RefineryProofLine ': Cargo C2 Racked' 5 $script:LastCommandAt
        Send-RefineryCommand 'AirlockVoyageClock 1200'
        Send-RefineryCommand 'AirlockWarp -7015 1560 116'
        $returnAt = $script:LastCommandAt
        $null = Wait-RefineryProofLine ': Transition: DOCKING - HALDER SOLAR REFINERY ->' 5 $returnAt
        $result = 'Voyage result: 2/12 crates intact, 0 vented, 10 loose, hull 100\.00%, pay 550, air \d+ ppm, 0 blackouts(, 0 burned)?, fuel \+20 t'
        $null = Wait-RefineryProofLine ": $result" 5 $returnAt
        $null = Wait-RefineryProofLine ': Base fuel 220 t \(\+20 this run\), runs 1' 5 $returnAt
        $null = Wait-RefineryProofLine ': Arrival spawn .*X=-5640\.' 45 $returnAt
        Send-RefineryCommand 'AirlockWhere'
        Assert-RefineryArrival 'StationHub' -5640
        Send-RefineryCommand 'AirlockBaseFuel'
        $null = Wait-RefineryProofLine ': Base fuel 220 t, runs 1' 5 $script:LastCommandAt
        Send-RefineryCommand 'AirlockBoard'
        $null = Wait-RefineryProofLine ': Voyage board opened' 5 $script:LastCommandAt
        Save-RefineryCapture 'hub-board-after-run' 6
        # A second, empty run counts but adds nothing: the stock never falls.
        Send-RefineryCommand 'AirlockBook 0'
        Start-Sleep -Seconds 2
        Send-RefineryCommand 'AirlockWarp -4265 1560 116'
        $null = Wait-RefineryProofLine ': Incident director started: crew=1 entries=4' 45 $script:LastCommandAt
        Send-RefineryCommand 'AirlockWarp -7015 1560 116'
        $secondAt = $script:LastCommandAt
        $null = Wait-RefineryProofLine ': Voyage result: 0/4 crates intact, 0 vented, 4 loose' 5 $secondAt
        $null = Wait-RefineryProofLine ': Base fuel 220 t \(\+0 this run\), runs 2' 5 $secondAt
        $null = Wait-RefineryProofLine ': Arrival spawn .*X=-5640\.' 45 $secondAt
        Send-RefineryCommand 'AirlockSaveReset'
        $null = Wait-RefineryProofLine ': Save reset: base fuel 200 t, runs 0' 5 $script:LastCommandAt
        Write-Host 'REFINERY OK: reset -> priority contract, 12 casks -> 2 intact -> refinery dock, +20 t -> hub board -> empty run adds 0 -> reset'
    }
}
finally {
    if ($script:StartedPIE) {
        try { Get-RefineryProofLines | Set-Content -LiteralPath (Join-Path $proofDir $(if ($ProbePositions) { 'position-probe-log.txt' } else { 'refinery-log.txt' })) -Encoding UTF8 }
        finally {
            $stop = Mcp-Call $TS_App 'StopPIE' @{}
            if (-not $stop.ok) { throw "StopPIE failed: $($stop.text)" }
        }
    }
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/Ships/Hauler_Mk1')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map is dirty after the proof" }
    }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Editor did not return to StationHub' }
}
