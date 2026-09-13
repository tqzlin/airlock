# M3-6 PIE proof. Run from the repo root with a clean StationHub open.
# powershell -ExecutionPolicy Bypass -File Scripts/verify-fuel.ps1
# Only the file console drives gameplay. Fresh logs and required captures: Saved/AgentCaptures/M3-6.
# Slate snapshots omit the PIE HUD; visually review the captures for row/caption visibility.
# Default follows the brief literally. -ShortFollowup stages 80 units before the leak
# to exercise the short-reserve branch; at 90 units the +10 s follow-up still has reserve.
param([switch]$ShortFollowup)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$proofDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentCaptures/M3-6'
$commandPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false

function Send-FuelCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}

function Get-FuelProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 1000 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}

function Wait-FuelProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-FuelProofLines $Pattern $Since) | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line within $Seconds seconds"
}

function Save-FuelCapture([string]$Label) {
    Start-Sleep -Seconds 8
    Send-FuelCommand 'AirlockWhere'
    $null = Wait-FuelProofLine 'Where:' 5 $script:LastCommandAt
    $capture = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $source = $capture.images | Select-Object -First 1
    if (-not $capture.ok -or -not $source) { throw "Capture failed: $($capture.text). Restore the editor window and retry." }
    Copy-Item -LiteralPath $source -Destination (Join-Path $proofDir "$Label.png") -Force
    Write-Host "Capture: $proofDir/$Label.png"
}

function Assert-FuelLocation([string]$Map, [double]$ExpectedX) {
    $line = Wait-FuelProofLine 'Where:' 10 $script:LastCommandAt
    $position = [regex]::Match([string]$line, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+)')
    if ($line -notmatch "map=UEDPIE_\d+_$Map " -or -not $position.Success) { throw "Wrong arrival: $line" }
    $x = [double]::Parse($position.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $y = [double]::Parse($position.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ([math]::Abs($x - $ExpectedX) -gt 1 -or [math]::Abs($y - 1560) -gt 1) { throw "Arrival missed PlayerStart: $line" }
}


function Move-FuelCrew([double]$X, [double]$Y, [double]$Z = 118) {
    Send-FuelCommand "AirlockWarp $X $Y $Z"
    Start-Sleep -Seconds 1
}
try {
    Mcp-Init
    if ((Mcp-Call $TS_App 'IsPIERunning' @{}).text -match 'true') { throw 'Stop the current PIE session first' }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Open StationHub first' }
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/Ships/Hauler_Mk1')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{asset_path=$map}
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map must be clean before this proof" }
    }
    $script:ProofStart = [DateTime]::UtcNow
    $start = Mcp-Call $TS_App 'StartPIE' @{options=@{bSimulate=$false;playMode='PlayMode_InViewPort';warmupSeconds=5;startTransform=@{location=@{x=-5640;y=1560;z=118};rotation=@{pitch=0;yaw=0;roll=0};scale=@{x=1;y=1;z=1}}}}
    if (-not $start.ok) { throw $start.text }
    $script:StartedPIE=$true
    Send-FuelCommand 'AirlockWhere'
    Assert-FuelLocation 'StationHub' -5640
    Send-FuelCommand 'AirlockWarp -4700 1560 116'
    Send-FuelCommand 'AirlockBook 0'
    Start-Sleep -Seconds 2
    Send-FuelCommand 'AirlockWarp -4265 1560 116'
    $null=Wait-FuelProofLine ': Incident director started: crew=1 entries=4' 45
    Send-FuelCommand 'AirlockVoyageClock 1200'
    Send-FuelCommand 'AirlockWhere'
    Assert-FuelLocation 'Hauler_Mk1' -6660
    Send-FuelCommand 'AirlockFuel'
    $null=Wait-FuelProofLine ': Fuel (89|90)% leak=0 bypass=closed' 5 $script:LastCommandAt
    if ($ShortFollowup) { Send-FuelCommand 'AirlockFuelSet 80' }
    Send-FuelCommand 'AirlockFuelLeak'
    $leakAt=$script:LastCommandAt
    $null=Wait-FuelProofLine ': SetPiece FuelLeak at ' 5 $leakAt
    $null=Wait-FuelProofLine ': Captain .* (Feed valve.s failed open|Fuel line.s split|The Rig short-filled us)' 5 $leakAt
    $null=Wait-FuelProofLine ': Captain .*open the crude bypass and feed me casks' 12 $leakAt
    Move-FuelCrew -1300 280
    Send-FuelCommand 'AirlockLook 0'
    Save-FuelCapture 'feed-leak'
    Send-FuelCommand 'AirlockHold FeedValve'
    $holdAt=$script:LastCommandAt
    $null=Wait-FuelProofLine ': Hold FeedValve complete' 6 $holdAt
    $null=Wait-FuelProofLine ': Fuel .* leak=0' 5 $holdAt
    Send-FuelCommand 'AirlockFuelSet 25'
    $null=Wait-FuelProofLine ': Fuel 25%' 5 $script:LastCommandAt
    Move-FuelCrew -1300 700
    Send-FuelCommand 'AirlockLook 0'
    Save-FuelCapture 'low-fuel'
    Send-FuelCommand 'AirlockBypass'
    $openAt=$script:LastCommandAt
    $null=Wait-FuelProofLine ': Bypass opened by' 5 $openAt
    $null=Wait-FuelProofLine ': Fuel .*bypass=open' 5 $openAt
    Save-FuelCapture 'bypass-open'
    Send-FuelCommand 'AirlockVoyageClock 1200'
    Move-FuelCrew -5530 1700 116
    Send-FuelCommand 'AirlockCarry C3'
    $null=Wait-FuelProofLine ': Carry C3 by' 5 $script:LastCommandAt
    Move-FuelCrew -1300 600
    Send-FuelCommand 'AirlockLook 0 -25'
    Send-FuelCommand 'AirlockFeed C3'
    $feedAt=$script:LastCommandAt
    $null=Wait-FuelProofLine ': Feed C3 by' 5 $feedAt
    $null=Wait-FuelProofLine ': Fuel feed C3 started' 5 $feedAt
    Save-FuelCapture 'intake-burning'
    $null=Wait-FuelProofLine ': Fuel feed C3 done: \+15' 23 $feedAt
    Send-FuelCommand 'AirlockShipStatus'
    $null=Wait-FuelProofLine ': Ship Engines Nominal cond=90\.00' 5 $script:LastCommandAt
    Move-FuelCrew -6330 1700 116
    Send-FuelCommand 'AirlockCarry C1'
    $null=Wait-FuelProofLine ': Carry C1 by' 5 $script:LastCommandAt
    Move-FuelCrew -1300 600
    Send-FuelCommand 'AirlockFeed C1'
    $feedAt=$script:LastCommandAt
    $null=Wait-FuelProofLine ': Feed C1 by' 5 $feedAt
    $null=Wait-FuelProofLine ': Fuel feed C1 done: \+15' 23 $feedAt
    $null=Wait-FuelProofLine ': Injectors fouled' 5 $feedAt
    Send-FuelCommand 'AirlockShipStatus'
    $null=Wait-FuelProofLine ': Ship Engines Nominal cond=80\.00' 5 $script:LastCommandAt
    Move-FuelCrew -1300 880
    Send-FuelCommand 'AirlockLook 0'
    Save-FuelCapture 'injectors-fouled'
    Send-FuelCommand 'AirlockHold Engines'
    $holdAt=$script:LastCommandAt
    $null=Wait-FuelProofLine ': Hold Engines complete' 7 $holdAt
    $null=Wait-FuelProofLine ': Injectors cleared' 5 $holdAt
    Send-FuelCommand 'AirlockScrub Hazard'
    $null=Wait-FuelProofLine ': Scrub Hazard by' 5 $script:LastCommandAt
    $deadline=(Get-Date).AddSeconds(30)
    do {
        Send-FuelCommand 'AirlockAir'
        $air=@(Get-FuelProofLines ': Air [0-9] ppm scrub=Hazard' $script:LastCommandAt)
        if ($air.Count -gt 0) { Write-Host $air[-1]; break }
        Start-Sleep -Seconds 1
    } while ((Get-Date) -lt $deadline)
    if ($air.Count -eq 0) { throw 'Air did not clear under 10 ppm' }
    Send-FuelCommand 'AirlockTally'
    $null=Wait-FuelProofLine ': Tally now: 0/4 crates intact, 0 vented, 2 loose, hull 97\.14%, pay 264, air \d ppm, 0 blackouts, 2 burned' 5 $script:LastCommandAt
    Send-FuelCommand 'AirlockRepairModule Engines 100'
    Send-FuelCommand 'AirlockVoyageClock 1200'
    Send-FuelCommand 'AirlockShipStatus'
    $statusAt=$script:LastCommandAt
    $null=Wait-FuelProofLine ': Ship Galley Nominal cond=100\.00' 5 $statusAt
    if (@(Get-FuelProofLines ': Ship \w+ Nominal cond=100\.00' $statusAt).Count -ne 7) { throw 'Seven nominal modules required' }
    if (@(Get-FuelProofLines ': Incident (CoolantLeak|PowerShort|ModuleDamage|CargoShift) ').Count -ne 0) { throw 'Scheduled incident fired during calm proof' }
    Send-FuelCommand 'AirlockWarp -7015 1560 116'
    $returnAt=$script:LastCommandAt
    $null=Wait-FuelProofLine ': Voyage result: 0/4 crates intact, 0 vented, 2 loose, hull 100\.00%, pay 270, air \d ppm, 0 blackouts, 2 burned' 5 $returnAt
    $null=Wait-FuelProofLine ': Arrival spawn .*X=-5640\.' 45 $returnAt
    Send-FuelCommand 'AirlockWhere'
    Assert-FuelLocation 'StationHub' -5640
    Send-FuelCommand 'AirlockFuel'
    $null=Wait-FuelProofLine ': No fuel system on this section' 5 $script:LastCommandAt
    Send-FuelCommand 'AirlockLook 0'
    Send-FuelCommand 'AirlockBoard'
    $null=Wait-FuelProofLine ': Voyage board opened' 5 $script:LastCommandAt
    Save-FuelCapture 'hub-last-run-fuel'
    Write-Host 'FUEL OK: feed leak -> shut -> bypass -> burn C3/C1 -> clear injectors -> clean air, pay 270 -> hub last run'
}
finally {
    if ($script:StartedPIE) {
        try { Get-FuelProofLines | Set-Content -LiteralPath (Join-Path $proofDir 'fuel-log.txt') -Encoding UTF8 }
        finally { $stop=Mcp-Call $TS_App 'StopPIE' @{}; if (-not $stop.ok) { throw "StopPIE failed: $($stop.text)" } }
    }
    foreach ($map in @('/Game/Maps/StationHub','/Game/Maps/Ships/Hauler_Mk1')) {
        $dirty=Mcp-Call $TS_Asset 'is_dirty' @{asset_path=$map}
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map is dirty after the proof" }
    }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Editor did not return to StationHub' }
}
