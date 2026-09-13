# M5-2 PIE proof: the refinery floor. Run from the repo root with a clean StationHub open (port from AIRLOCK_MCP_PORT,
# default 8000):
#   powershell -ExecutionPolicy Bypass -File Scripts/verify-floor.ps1 [-ProbePositions]
# The ship docks (clock expiry) on the floor with the run's intact casks on the pallets and Okafor runs the slot. Since
# M5-5 the crew works the floor by hand wherever a hand can: C1 is carried into Still 1's hopper, C2 into Still 2's, the
# handles are tapped and held, the flood comes from ten taps on FEED +, the pop from HEAT + and a held VENT - (the cask
# is lost, Okafor warns first, the puddle merges), the stumble drops C3 in the crude (its landing point on the deck is
# checked), two squeegee passes clear the merged puddle (the first logs what is left), RESET is a hold, C3 goes into the
# reset still by hand, and the horn takes everyone home with the combined result (2/3 refined, 2 spills, 1 pop) and the
# diesel surplus on the Rig's board. The console still sets exact knob values after a hand pass and drives Still 1's
# preheat. `AirlockFps` records the frame rate with two stills running. Only the file console drives gameplay; captures
# and the log excerpt land in Saved/AgentCaptures/M5-5; HUD text is capture-only. -ProbePositions instead opens the
# floor map, drops the pawn on every fixture, cask and panel spot and reports where its feet settle (the layout Z is the
# starting value; the report is what the table is tuned from).
param([switch]$ProbePositions)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$root = Split-Path $PSScriptRoot -Parent
$proofDir = Join-Path $root 'Saved/AgentCaptures/M5-5'
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
# A handle held for a while: the repeating hold steps the knob every quarter second until the release.
function Hold-FloorHandle([string]$Control, [double]$Seconds) {
    Send-FloorCommand "AirlockHold $Control"
    Start-Sleep -Milliseconds ([int]($Seconds * 1000))
    Send-FloorCommand 'AirlockRelease'
}

# The floor's spots (Ship/RefineryLayouts.cpp, Ship/CargoLayouts.cpp BuildFloor, Ship/ShipLayouts.cpp).
$fixtureSpots = @(@('Still_1', -5800, 1900), @('Still_2', -5400, 1900), @('Still_3', -5000, 1900), @('Tank', -4650, 1900), @('Drain', -5400, 1560), @('Bunk', -6650, 1250), @('Panel', -5400, 1150))
$caskSpots = @()
foreach ($y in 1400, 1250) { foreach ($x in -6240, -6080, -5920, -5760, -5600, -5440) { $caskSpots += ,@("Spot_$($x)_$y", $x, $y) } }
# Unscaled control offsets from RefineryLayouts::StillControls. Probe the deck under each actor, including the raised levers.
$controlOffsets = @(@('Hopper',-200,-200), @('HeatMinus',-120,-200), @('HeatPlus',-80,-200),
    @('FeedMinus',-20,-200), @('FeedPlus',20,-200), @('VentMinus',80,-200), @('VentPlus',120,-200),
    @('Reset',-75,-130), @('Purge',-25,-130), @('Scrape',25,-130), @('Repair',75,-130), @('Calibrate',150,-60))
$controlSpots = @()
$spillSpots = @()
for ($i=1; $i -le 3; $i++) {
    $stillX = -5800 + ($i-1)*400
    # RefineryLayouts::SpillPoint rotates (0,-230,0) without the tank's 0.8 scale; all three yaw 0.
    $spillSpots += ,@("Still_${i}_Spill", $stillX, 1670)
    foreach ($offset in $controlOffsets) {
        $controlY = if ($i -eq 3 -and $offset[0] -ne 'Hopper') { 1600 } else { 1900+$offset[2] }
        $controlSpots += ,@("Still_${i}_$($offset[0])", ($stillX+$offset[1]), $controlY)
    }
}

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
        foreach ($spot in $fixtureSpots + $caskSpots + $controlSpots + $spillSpots) { $rows += Read-FloorProbe $spot[0] $spot[1] $spot[2] 200 }
        $rows | Format-Table -AutoSize | Out-String | Write-Host
        $rows | Export-Csv -LiteralPath (Join-Path $proofDir 'floor-position-probe.csv') -NoTypeInformation
        Write-Host "FLOOR POSITIONS REPORTED: $($rows.Count) spots (feet Z per spot in $proofDir/floor-position-probe.csv)"
        for ($i=1; $i -le 3; $i++) {
            $stillX = -5800 + ($i-1)*400
            Send-FloorCommand "AirlockWarp $stillX 1450 116"
            Send-FloorCommand 'AirlockLook 90'
            Save-FloorCapture "floor-still-$i-face" 4
        }
        return
    }

    $script:ProofStart = [DateTime]::UtcNow
    $start = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -5640; y = 1560; z = 118 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
    if (-not $start.ok) { throw "StartPIE failed: $($start.text)" }
    $script:StartedPIE = $true
    $null = Wait-FloorProofLine 'AgentConsole: ready' 30

    # The ship leg: book the Milk run, board, secure C1/C3 and rack C2, then let the clock dock the ship.
    Send-FloorCommand 'AirlockBook 0'
    Start-Sleep -Seconds 2
    $boardAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockWarp -4700 1560 116'
    Start-Sleep -Seconds 1
    Send-FloorCommand 'AirlockWarp -4265 1560 116'
    $null = Wait-FloorProofLine ': Section Hauler_Mk1: voyage underway' 60 $boardAt
    $null = Wait-FloorProofLine ': Incident director started' 45 $boardAt
    # Consume the skipped ship incidents while clamps are empty. Otherwise today's final jump releases
    # Clamp_3 after C3 is secured, defeating the brief's three-cask delivery before the floor proof starts.
    Send-FloorCommand 'AirlockVoyageClock 75'
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
    Send-FloorCommand 'AirlockWarp -5330 1700 116'
    Start-Sleep -Seconds 2
    Send-FloorCommand 'AirlockCarry C3'
    $null = Wait-FloorProofLine ': Carry C3 by' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockWarp -5430 1400 116'
    Start-Sleep -Seconds 2
    Send-FloorCommand 'AirlockPlace Clamp_3'
    $null = Wait-FloorProofLine ': Cargo C3 Secured' 10 $script:LastCommandAt

    # Docking lands on the floor with the three intact casks; the loose one never arrives.
    $dockAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockVoyageClock 5'
    $null = Wait-FloorProofLine ': Ship result:' 30 $dockAt '3 casks to the floor'
    $null = Wait-FloorProofLine 'Transition card: DOCKING - HALDER SOLAR REFINERY' 30 $dockAt
    $null = Wait-FloorProofLine ': Section RefineryFloor:' 60 $dockAt
    $null = Wait-FloorProofLine ': Slot clock started: 600s' 30 $dockAt 'casks=3'
    $null = Wait-FloorProofLine ': Floor fixtures: 6' 30 $dockAt
    $null = Wait-FloorProofLine ': Still controls: 36' 30 $dockAt
    $null = Wait-FloorProofLine ': Okafor, floor three' 30 $dockAt
    $null = Wait-FloorProofLine ': Arrival spawn for' 30 $dockAt 'X=-6660.'
    Start-Sleep -Seconds 6
    Send-FloorCommand 'AirlockWhere'
    Assert-FloorArrival 'RefineryFloor' -6660
    Send-FloorCommand 'AirlockCargoStatus'
    $null = Wait-FloorProofLine ': Cargo C1 Loose' 10 $script:LastCommandAt
    $null = Wait-FloorProofLine ': Cargo C2 Loose' 10 $script:LastCommandAt
    $null = Wait-FloorProofLine ': Cargo C3 Loose' 10 $script:LastCommandAt
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

    # C1 by hand into Still 1's hopper at feed 50 %: the batch runs about 100 s and the tank fills (M5-1's recipe, about 747 L).
    $elapsed = ([DateTime]::UtcNow - $preheatAt).TotalSeconds
    if ($elapsed -lt 30) { Start-Sleep -Seconds ([int](30 - $elapsed)) }
    Send-FloorCommand 'AirlockWarp -6240 1560 116'
    Start-Sleep -Seconds 2
    Send-FloorCommand 'AirlockCarry C1'
    $null = Wait-FloorProofLine ': Carry C1 by' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockWarp -5800 1600 116'
    Start-Sleep -Seconds 2
    $loadAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockPress Still_1_Hopper'
    $null = Wait-FloorProofLine ': Load C1 into Still_1 by' 10 $loadAt
    $null = Wait-FloorProofLine ': Refinery Still_1 loaded C1' 10 $loadAt '(hopper 1/3)'
    $null = Wait-FloorProofLine ': Cargo C1 Loaded' 10 $loadAt
    $null = Wait-FloorProofLine ': Still Still_1 Running cask=C1' 10 $loadAt
    # The light cask by hand into Still 2 alongside C1, before it can warm and vent on the staging deck.
    Send-FloorCommand 'AirlockWarp -6080 1560 116'
    Start-Sleep -Seconds 2
    Send-FloorCommand 'AirlockCarry C2'
    $null = Wait-FloorProofLine ': Carry C2 by' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockWarp -5400 1600 116'
    Start-Sleep -Seconds 2
    $handAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockPress Still_2_Hopper'
    $null = Wait-FloorProofLine ': Load C2 into Still_2 by' 10 $handAt
    $null = Wait-FloorProofLine ': Refinery Still_2 loaded C2' 10 $handAt
    $null = Wait-FloorProofLine ': Cargo C2 Loaded' 10 $handAt
    for ($i=0; $i -lt 6; $i++) { Send-FloorCommand 'AirlockPress Still_2_HeatPlus' }
    $null = Wait-FloorProofLine ': Refinery Still_2 heat 6/20' 10 $handAt
    $repeatAt = [DateTime]::UtcNow
    Hold-FloorHandle 'Still_2_HeatPlus' 1
    $steps = @(Get-FloorProofLines ': Refinery Still_2 heat [0-9]+/20' $repeatAt)
    if ($steps.Count -lt 3) { throw "Repeating handle produced only $($steps.Count) steps" }
    Write-Host "Held heat handle: $($steps.Count) steps"
    Send-FloorCommand 'AirlockKnob Still_2 Heat 6'
    for ($i=0; $i -lt 10; $i++) { Send-FloorCommand 'AirlockPress Still_2_FeedPlus' }
    $null = Wait-FloorProofLine ': Refinery Still_2 feed 10/20' 10 $handAt
    $null = Wait-FloorProofLine ': Still Still_2 Running cask=C2' 10 $handAt

    # The flood by hand: ten more taps take the feed to 100 %; five seconds over 80 % and the crude comes over the lip.
    $spillAt = [DateTime]::UtcNow
    for ($i=0; $i -lt 10; $i++) { Send-FloorCommand 'AirlockPress Still_2_FeedPlus' }
    $null = Wait-FloorProofLine ': Refinery Still_2 feed 20/20' 10 $spillAt
    $null = Wait-FloorProofLine ': Refinery Spill Still_2 C2 litres=200' 12 $spillAt
    $null = Wait-FloorProofLine ': Spill S1 by Still_2: 200 L' 6 $spillAt
    $null = Wait-FloorProofLine ': Okafor: Flood on Still 2' 6 $spillAt
    # The pop by hand, while the charge is still high: the feed eases to 25 % (the gain needs some feed and the charge
    # must last), four taps on HEAT + take the bath toward 220 K and a held VENT - shuts the trap. The needle goes red,
    # Okafor warns, the relief goes, the cask is lost and the pop's 200 L merge into the flood's puddle. (Started a
    # minute into the batch the pressure only reaches the warning band: the gain falls with the charge.)
    Send-FloorCommand 'AirlockKnob Still_2 Feed 5'
    $null = Wait-FloorProofLine ': Refinery Still_2 feed 5/20' 10 $spillAt
    $popAt = [DateTime]::UtcNow
    for ($i=0; $i -lt 4; $i++) { Send-FloorCommand 'AirlockPress Still_2_HeatPlus' }
    $null = Wait-FloorProofLine ': Refinery Still_2 heat 10/20' 10 $popAt
    Hold-FloorHandle 'Still_2_VentMinus' 3.2
    $null = Wait-FloorProofLine ': Refinery Still_2 vent 0/20' 10 $popAt
    $null = Wait-FloorProofLine ': Hold Still_2_VentMinus released by' 10 $popAt
    # The pressure builds while the spill capture runs (M5-4b's white burst at the pop lasts half a second).
    Send-FloorCommand 'AirlockWarp -5400 1450 116'
    Send-FloorCommand 'AirlockLook 90 -25'
    Save-FloorCapture 'floor-spill' 4
    Send-FloorCommand 'AirlockWarp -5400 1600 116'
    Send-FloorCommand 'AirlockLook 90'
    $null = Wait-FloorProofLine ': Still Still_2 pressure' 90 $popAt 'warning band'
    $null = Wait-FloorProofLine ': Okafor: Pressure on Still 2' 10 $popAt
    $null = Wait-FloorProofLine ': Refinery Pop Still_2 C2' 90 $popAt
    $null = Wait-FloorProofLine ': Still Still_2 Popped' 10 $popAt
    $merged = Wait-FloorProofLine ': Spill S1 merged:' 10 $popAt
    $mergedLitres = [int][regex]::Match([string]$merged, 'merged: (\d+) L').Groups[1].Value
    if ($mergedLitres -lt 300) { throw "The pop did not merge into a puddle a squeegee pass leaves behind: $merged" }
    Save-FloorCapture 'floor-still-popped' 2
    # The pop fumes the room (the board's `Hold air .. ppm - scrub` goes red): scrubbers to Hazard at the panel by hand,
    # as a player would - the first run of this proof left them on Normal and the crew blacked out forty seconds later.
    $scrubAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockWarp -5400 1300 116'
    Send-FloorCommand 'AirlockLook -90'
    Send-FloorCommand 'AirlockPress LifeSupport'
    $null = Wait-FloorProofLine ': Scrub Hazard by' 10 $scrubAt
    $null = Wait-FloorProofLine ': Air ' 10 $scrubAt 'scrub=Hazard'
    Send-FloorCommand 'AirlockLook 90'

    # C3 into the crude: the stumble drops it, and since M5-5 it lands on the deck (Z -65), not on a marker box.
    Send-FloorCommand 'AirlockWarp -5920 1560 116'
    Start-Sleep -Seconds 2
    Send-FloorCommand 'AirlockCarry C3'
    $null = Wait-FloorProofLine ': Carry C3 by' 10 $script:LastCommandAt
    $stumbleAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockWarp -5400 1600 116'
    $null = Wait-FloorProofLine ': Stumble ' 8 $stumbleAt 'drops C3 in S1'
    $dropped = Wait-FloorProofLine ': PutDown C3 by' 6 $stumbleAt
    $dropZ = [double]::Parse([regex]::Match([string]$dropped, 'Z=([-0-9.]+)').Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ([math]::Abs($dropZ + 65) -gt 6) { throw "C3 did not land on the deck: $dropped" }
    Write-Host "C3 landed at Z=$dropZ (deck -65)"
    Send-FloorCommand 'AirlockWarp -5400 1450 116'
    $squeegeeAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockHold S1'
    Save-FloorCapture 'floor-squeegee' 2
    $null = Wait-FloorProofLine ': Hold S1 complete by' 10 $squeegeeAt
    $partial = Wait-FloorProofLine ': Spill S1 squeegeed by' 10 $squeegeeAt '300 L off'
    Send-FloorCommand 'AirlockHold S1'
    $null = Wait-FloorProofLine ': Spill S1 gone' 20 $squeegeeAt
    Send-FloorCommand 'AirlockFloor'
    $null = Wait-FloorProofLine ': No spills' 10 $script:LastCommandAt
    Save-FloorCapture 'floor-cleared' 2

    # RESET by hand, the vent back up by hand, then exact knobs, and C3 by hand into the reset still.
    Send-FloorCommand 'AirlockWarp -5400 1600 116'
    Send-FloorCommand 'AirlockLook 90'
    $resetAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockHold Still_2_Reset'
    $null = Wait-FloorProofLine ': Hold Still_2_Reset complete' 10 $resetAt
    $null = Wait-FloorProofLine ': Refinery Still_2 reset' 10 $resetAt
    $null = Wait-FloorProofLine ': Still Still_2 Idle' 10 $resetAt
    Hold-FloorHandle 'Still_2_VentPlus' 2.2
    $null = Wait-FloorProofLine ': Refinery Still_2 vent [5-9]/20' 10 $resetAt
    # C3 is a heavy cask: heat 40 % (M5-1's heavy recipe), not the light cask's 30 % that C2 ran at - at 30 % with the
    # feed's cooling the bath settles under the heavy cut and the batch goes to tar (121 L diesel in the first rerun).
    Send-FloorCommand 'AirlockKnob Still_2 Vent 11'
    Send-FloorCommand 'AirlockKnob Still_2 Heat 8'
    Send-FloorCommand 'AirlockKnob Still_2 Feed 10'
    $null = Wait-FloorProofLine ': Refinery Still_2 heat 8/20' 10 $resetAt
    $null = Wait-FloorProofLine ': Refinery Still_2 feed 10/20' 10 $resetAt
    Send-FloorCommand 'AirlockCarry C3'
    $null = Wait-FloorProofLine ': Carry C3 by' 10 $script:LastCommandAt
    $c3At = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockPress Still_2_Hopper'
    $null = Wait-FloorProofLine ': Load C3 into Still_2 by' 10 $c3At
    $null = Wait-FloorProofLine ': Still Still_2 Running cask=C3' 10 $c3At
    Send-FloorCommand 'AirlockHold Still_2_Calibrate'
    $null = Wait-FloorProofLine ': Hold Still_2_Calibrate complete' 10 $script:LastCommandAt
    $null = Wait-FloorProofLine ': Refinery Still_2 calibrated' 10 $script:LastCommandAt
    Send-FloorCommand 'AirlockHold Still_2_Reset'
    $null = Wait-FloorProofLine ': Hold Still_2_Reset rejected: Running' 10 $script:LastCommandAt
    Save-FloorCapture 'floor-still-gauges' 4
    Send-FloorCommand 'AirlockWarp -5400 1450 116'
    Send-FloorCommand 'AirlockLook 90 -20'
    Save-FloorCapture 'floor-still-handles' 4
    Send-FloorCommand 'AirlockWarp -5800 1500 116'
    Send-FloorCommand 'AirlockLook 90'
    Save-FloorCapture 'floor-still-running' 4
    Send-FloorCommand 'AirlockFps'
    $fpsLine = Wait-FloorProofLine ': Fps:' 5 $script:LastCommandAt
    $fps = [double]::Parse([regex]::Match([string]$fpsLine, 'Fps: ([0-9.]+) fps').Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ($fps -le 0) { throw "No frame rate reading: $fpsLine" }

    # Both batches: C1 in Still 1 and C3 in the reset Still 2; C2 went out with the pop.
    $null = Wait-FloorProofLine ': Tank [3-9][0-9][0-9] L diesel' 120 $loadAt
    $null = Wait-FloorProofLine ': Refinery BatchDone Still_1 C1' 120 $loadAt
    $null = Wait-FloorProofLine ": Okafor: That's diesel in the tank" 10 $loadAt
    $null = Wait-FloorProofLine ': Refinery BatchDone Still_2 C3' 130 $c3At
    Send-FloorCommand 'AirlockRefinery'
    $tank = Wait-FloorProofLine ': Tank diesel=' 10 $script:LastCommandAt
    $litres = [int][regex]::Match([string]$tank, 'Tank diesel=(\d+)').Groups[1].Value
    if ($litres -lt 600) { throw "The batches yielded too little: $tank" }
    Send-FloorCommand 'AirlockTally'
    $null = Wait-FloorProofLine ': Floor tally now: floor: 2/3 casks refined' 10 $script:LastCommandAt
    Save-FloorCapture 'floor-tank' 2

    # The slot's end: the horn, the combined result, the card home, the hub board.
    $hornAt = [DateTime]::UtcNow
    Send-FloorCommand 'AirlockVoyageClock 5'
    $null = Wait-FloorProofLine ': Horn. Slot' 30 $hornAt
    $null = Wait-FloorProofLine ': Floor result: floor: 2/3 casks refined' 30 $hornAt '2 spills, 1 pop'
    $null = Wait-FloorProofLine ': Voyage result:' 30 $hornAt '| floor: 2/3 casks refined,'
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
    Write-Host "FLOOR OK: Milk run -> C1/C3 secured, C2 racked -> floor: three casks, six fixtures, 36 controls -> C1 by hand into Still 1 -> C2 by hand into Still 2, flooded at feed 100 % (200 L into S1), popped at vent 0 after Okafor's warning (C2 lost, $mergedLitres L merged) -> C3 stumble lands on the deck (Z=$dropZ) -> two squeegee passes ($partial) -> RESET by hand, vent up by hand -> C3 by hand into Still 2 -> both batches done, $litres L diesel, 2 spills, 1 pop, $fps fps with two stills running -> horn -> combined result (2/3 refined, +$tonnes t), undocking card -> hub board, save reset"
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
