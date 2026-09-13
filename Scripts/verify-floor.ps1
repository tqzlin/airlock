# M5-2 PIE proof: the refinery floor. Run from the repo root with a clean StationHub open (port from AIRLOCK_MCP_PORT,
# default 8000):
#   powershell -ExecutionPolicy Bypass -File Scripts/verify-floor.ps1 [-ProbePositions]
# The ship docks (clock expiry) on the floor with the run's intact casks on the pallets, Okafor runs the slot, C1 is
# refined in Still 1 by console (M5-3a: knobs, load, the batch, the tank), the slot's end takes everyone home with the
# combined result and the diesel surplus on the Rig's board. Only the file console drives gameplay; captures and the log
# excerpt land in Saved/AgentCaptures/M5-2; HUD text is capture-only. -ProbePositions instead opens the floor map,
# drops the pawn on every fixture, cask and panel spot and reports where its feet settle (the layout Z is the
# starting value; the report is what the table is tuned from).
param([switch]$ProbePositions)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$root = Split-Path $PSScriptRoot -Parent
$proofDir = Join-Path $root 'Saved/AgentCaptures/M5-2'
$commandPath = Join-Path $root 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false
$script:LoadedFloor = $false

function Send-FloorCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}
function Get-FloorProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 1000 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}
function Wait-FloorProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart, [string]$MustContain = '') {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-FloorProofLines $Pattern $Since) | Where-Object { -not $MustContain -or $_ -like "*$MustContain*" } | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line$(if ($MustContain) { " containing '$MustContain'" }) within $Seconds seconds"
}
function Save-FloorCapture([string]$Label, [int]$SettleSeconds = 8) {
    Start-Sleep -Seconds $SettleSeconds
    Send-FloorCommand 'AirlockWhere'
    $null = Wait-FloorProofLine ': Where:' 5 $script:LastCommandAt
    $capture = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $source = $capture.images | Select-Object -First 1
    if (-not $capture.ok -or -not $source) { throw "Capture failed: $($capture.text). Restore the editor window and retry." }
    Copy-Item -LiteralPath $source -Destination (Join-Path $proofDir "$Label.png") -Force
    Write-Host "Capture: $proofDir/$Label.png"
}
function Assert-FloorArrival([string]$Map, [double]$ExpectedX) {
    $line = Wait-FloorProofLine ': Where:' 10 $script:LastCommandAt
    $position = [regex]::Match([string]$line, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+)')
    if ($line -notmatch "map=UEDPIE_\d+_$Map " -or -not $position.Success) { throw "Wrong arrival: $line" }
    $x = [double]::Parse($position.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $y = [double]::Parse($position.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ([math]::Abs($x - $ExpectedX) -gt 1 -or [math]::Abs($y - 1560) -gt 1) { throw "Arrival missed PlayerStart: $line" }
}
function Read-FloorProbe([string]$Id, [double]$X, [double]$Y, [double]$DropZ) {
    Send-FloorCommand "AirlockWarp $X $Y $DropZ"
    Start-Sleep -Seconds 3
    Send-FloorCommand 'AirlockWhere'
    $line = Wait-FloorProofLine ': Where:' 5 $script:LastCommandAt
    $match = [regex]::Match($line, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+) Z=([-0-9.]+)')
    if (-not $match.Success) { throw "Unparseable probe: $line" }
    $px = [double]::Parse($match.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $py = [double]::Parse($match.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    $pz = [double]::Parse($match.Groups[3].Value, [Globalization.CultureInfo]::InvariantCulture)
    [pscustomobject]@{ Id = $Id; X = $X; Y = $Y; PawnX = $px; PawnY = $py; PawnZ = $pz; FeetZ = $pz - 96; Moved = ([math]::Abs($px - $X) -gt 1 -or [math]::Abs($py - $Y) -gt 1) }
}

# The floor's spots (Ship/RefineryLayouts.cpp, Ship/CargoLayouts.cpp BuildFloor, Ship/ShipLayouts.cpp).
$fixtureSpots = @(@('Still_1', -5800, 1900), @('Still_2', -5400, 1900), @('Still_3', -5000, 1900), @('Tank', -4650, 1900), @('Drain', -5400, 1560), @('Bunk', -6650, 1250), @('Panel', -5400, 1150))
$caskSpots = @()
foreach ($y in 1400, 1250) { foreach ($x in -6240, -6080, -5920, -5760, -5600, -5440) { $caskSpots += ,@("Spot_$($x)_$y", $x, $y) } }

try {
    Mcp-Init
    if ((Mcp-Call $TS_App 'IsPIERunning' @{}).text -match 'true') { throw 'Stop the current PIE session first' }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Open StationHub first' }
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/Ships/Hauler_Mk1', '/Game/Maps/RefineryFloor')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map must be clean before this proof" }
    }
    if ($ProbePositions) {
        $load = Mcp-Call $TS_Scene 'load_level' @{ level_path = '/Game/Maps/RefineryFloor' }
        if (-not $load.ok) { throw "Could not open the floor: $($load.text)" }
        $script:LoadedFloor = $true
        Start-Sleep -Seconds 8
        $script:ProofStart = [DateTime]::UtcNow
        $start = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -6660; y = 1560; z = 150 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
        if (-not $start.ok) { throw "StartPIE failed: $($start.text)" }
        $script:StartedPIE = $true
        $null = Wait-FloorProofLine ': Section RefineryFloor:' 30
        $rows = @()
        foreach ($spot in $fixtureSpots + $caskSpots) { $rows += Read-FloorProbe $spot[0] $spot[1] $spot[2] 200 }
        $rows | Format-Table -AutoSize | Out-String | Write-Host
        $rows | Export-Csv -LiteralPath (Join-Path $proofDir 'floor-position-probe.csv') -NoTypeInformation
        Write-Host "FLOOR POSITIONS REPORTED: $($rows.Count) spots (feet Z per spot in $proofDir/floor-position-probe.csv)"
        return
    }

    $script:ProofStart = [DateTime]::UtcNow
    $start = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -5640; y = 1560; z = 118 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
    if (-not $start.ok) { throw "StartPIE failed: $($start.text)" }
    $script:StartedPIE = $true
    $null = Wait-FloorProofLine 'AgentConsole: ready' 30

    # The ship leg: book the Milk run, board, secure C1 and rack C2, then let the clock dock the ship.
    Send-FloorCommand 'AirlockBook 0'
    Start-Sleep -Seconds 2
    $boardAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockWarp -4700 1560 116'
    Start-Sleep -Seconds 1
    Send-FloorCommand 'AirlockWarp -4265 1560 116'
    $null = Wait-FloorProofLine ': Section Hauler_Mk1: voyage underway' 60 $boardAt
    $null = Wait-FloorProofLine ': Incident director started' 45 $boardAt
    Send-FloorCommand 'AirlockWarp -6330 1700 116'
    Start-Sleep -Seconds 2
    Send-FloorCommand 'AirlockCarry C1'
    $null = Wait-FloorProofLine ': Carry C1 by' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockWarp -6430 1400 116'
    Start-Sleep -Seconds 2
    Send-FloorCommand 'AirlockPlace Clamp_1'
    $null = Wait-FloorProofLine ': Cargo C1 Secured' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockWarp -5830 1700 116'
    Start-Sleep -Seconds 2
    Send-FloorCommand 'AirlockCarry C2'
    $null = Wait-FloorProofLine ': Carry C2 by' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockWarp -5230 1300 116'
    Start-Sleep -Seconds 2
    Send-FloorCommand 'AirlockPlace ColdRack'
    $null = Wait-FloorProofLine ': Cargo C2 Racked' 10 $script:LastCommandAt

    # Docking lands on the floor with the two intact casks; the loose ones never arrive.
    $dockAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockVoyageClock 5'
    $null = Wait-FloorProofLine ': Ship result:' 30 $dockAt '2 casks to the floor'
    $null = Wait-FloorProofLine 'Transition card: DOCKING - HALDER SOLAR REFINERY' 30 $dockAt
    $null = Wait-FloorProofLine ': Section RefineryFloor:' 60 $dockAt
    $null = Wait-FloorProofLine ': Slot clock started: 600s' 30 $dockAt 'casks=2'
    $null = Wait-FloorProofLine ': Floor fixtures: 6' 30 $dockAt
    $null = Wait-FloorProofLine ': Okafor, floor three' 30 $dockAt
    $null = Wait-FloorProofLine ': Arrival spawn for' 30 $dockAt 'X=-6660.'
    Start-Sleep -Seconds 6
    Send-FloorCommand 'AirlockWhere'
    Assert-FloorArrival 'RefineryFloor' -6660
    Send-FloorCommand 'AirlockCargoStatus'
    $null = Wait-FloorProofLine ': Cargo C1 Loose' 10 $script:LastCommandAt
    $null = Wait-FloorProofLine ': Cargo C2 Loose' 10 $script:LastCommandAt
    if (@(Get-FloorProofLines ': Cargo C3 ' $script:LastCommandAt).Count -gt 0) { throw 'A loose cask reached the floor' }
    Send-FloorCommand 'AirlockAir'
    $null = Wait-FloorProofLine ': Air 0 ppm scrub=Normal' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockShipStatus'
    $null = Wait-FloorProofLine ': Ship LifeSupport Nominal' 10 $script:LastCommandAt

    # The stills (M5-3a): three idle stills and an empty tank; Still 1 preheats at heat 40 % while the captures run.
    Send-FloorCommand 'AirlockRefinery'
    $null = Wait-FloorProofLine ': Still_1 Idle T=92' 10 $script:LastCommandAt
    $null = Wait-FloorProofLine ': Tank diesel=0 gas=0 tar=0' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockKnob Still_1 Heat 8'
    $null = Wait-FloorProofLine ': Refinery Still_1 heat 8/20' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockKnob Still_1 Feed 10'
    $null = Wait-FloorProofLine ': Refinery Still_1 feed 10/20' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockStill Still_1 Reset'
    $null = Wait-FloorProofLine ': Refinery Still_1 Reset rejected: Idle' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockStillLoad Still_1 C9'
    $null = Wait-FloorProofLine ': Refinery load rejected: no such cask' 10 $script:LastCommandAt
    $preheatAt = [DateTime]::UtcNow
    Save-FloorCapture 'floor-arrival'
    Send-FloorCommand 'AirlockWarp -6000 1560 116'
    Send-FloorCommand 'AirlockLook -90'
    Save-FloorCapture 'floor-pallets' 4
    Send-FloorCommand 'AirlockWarp -5400 1500 116'
    Send-FloorCommand 'AirlockLook 90'
    Save-FloorCapture 'floor-stills' 4
    Send-FloorCommand 'AirlockLook -90'
    Save-FloorCapture 'floor-panel' 4
    Send-FloorCommand 'AirlockWarp -4900 1560 116'
    Send-FloorCommand 'AirlockLook 0'
    Save-FloorCapture 'floor-east' 4

    # C1 into Still 1's hopper at feed 50 %: the batch runs about 100 s and the tank fills (M5-1's recipe, about 747 L).
    $elapsed = ([DateTime]::UtcNow - $preheatAt).TotalSeconds
    if ($elapsed -lt 30) { Start-Sleep -Seconds ([int](30 - $elapsed)) }
    $loadAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockStillLoad Still_1 C1'
    $null = Wait-FloorProofLine ': Refinery Still_1 loaded C1 (hopper 1/3)' 10 $loadAt
    $null = Wait-FloorProofLine ': Cargo C1 Loaded' 10 $loadAt
    $null = Wait-FloorProofLine ': Still Still_1 Running cask=C1' 10 $loadAt
    Send-FloorCommand 'AirlockWarp -5800 1500 116'
    Send-FloorCommand 'AirlockLook 90'
    Save-FloorCapture 'floor-still-running' 4
    $null = Wait-FloorProofLine ': Tank 300 L diesel' 90 $loadAt
    $null = Wait-FloorProofLine ': Refinery BatchDone Still_1 C1' 120 $loadAt
    $null = Wait-FloorProofLine ": Okafor: That's diesel in the tank" 10 $loadAt
    $null = Wait-FloorProofLine ': Still Still_1 Idle' 10 $loadAt
    Send-FloorCommand 'AirlockRefinery'
    $tank = Wait-FloorProofLine ': Tank diesel=' 10 $script:LastCommandAt
    $litres = [int][regex]::Match([string]$tank, 'Tank diesel=(\d+)').Groups[1].Value
    if ($litres -lt 600) { throw "The batch yielded too little: $tank" }
    Send-FloorCommand 'AirlockTally'
    $null = Wait-FloorProofLine ': Floor tally now: floor: 1/2 casks refined' 10 $script:LastCommandAt
    Save-FloorCapture 'floor-tank' 2

    # The slot's end: the horn, the combined result, the card home, the hub board.
    $hornAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockVoyageClock 5'
    $null = Wait-FloorProofLine ': Horn. Slot' 30 $hornAt
    $null = Wait-FloorProofLine ': Floor result: floor: 1/2 casks refined' 30 $hornAt
    $null = Wait-FloorProofLine ': Voyage result:' 30 $hornAt '| floor: 1/2 casks refined,'
    $null = Wait-FloorProofLine 'Transition card: UNDOCKING - HOME TO THE VELL EXTRACTOR' 30 $hornAt
    $null = Wait-FloorProofLine ': Arrival spawn for' 45 $hornAt 'X=-5640.'
    Start-Sleep -Seconds 6
    Send-FloorCommand 'AirlockWhere'
    Assert-FloorArrival 'StationHub' -5640
    $base = Wait-FloorProofLine ': Base fuel' 10 $hornAt
    $tonnes = [int][regex]::Match([string]$base, '\(\+(\d+) this run\)').Groups[1].Value
    if ($tonnes -lt 5) { throw "The diesel surplus did not reach the Rig: $base" }
    Send-FloorCommand 'AirlockBoard'
    Save-FloorCapture 'hub-board-floor' 3
    Send-FloorCommand 'AirlockSaveReset'
    $null = Wait-FloorProofLine ': Save reset:' 10 $script:LastCommandAt
    Write-Host "FLOOR OK: Milk run -> C1 secured, C2 racked -> clock docks at the refinery -> floor: two casks on the pallets, Okafor, air, scrubber panel, six fixtures -> Still 1 at heat 40 % / feed 50 %, C1 loaded, batch done, $litres L in the tank -> horn -> combined result (1/2 refined, +$tonnes t), undocking card -> hub board, save reset"
}
finally {
    if ($script:StartedPIE) {
        try { Get-FloorProofLines | Set-Content -LiteralPath (Join-Path $proofDir $(if ($ProbePositions) { 'floor-probe-log.txt' } else { 'floor-log.txt' })) }
        finally {
            $stop = Mcp-Call $TS_App 'StopPIE' @{}
            if (-not $stop.ok) { throw "StopPIE failed: $($stop.text)" }
        }
    }
    if ($script:LoadedFloor) {
        Start-Sleep -Seconds 3
        $null = Mcp-Call $TS_Scene 'load_level' @{ level_path = '/Game/Maps/StationHub' }
        Start-Sleep -Seconds 5
    }
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/Ships/Hauler_Mk1', '/Game/Maps/RefineryFloor')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map is dirty after the proof" }
    }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Editor did not return to StationHub' }
}
