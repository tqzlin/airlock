# M3-5 PIE proof. Run from the repo root with clean StationHub open on MCP port 8000.
# powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/verify-atmosphere.ps1
# Gameplay uses the file console. HUD evidence is capture-only, under Saved/AgentCaptures/M3-5.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$proofDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentCaptures/M3-5'
$commandPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false

function Send-AirCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}
function Get-AirProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 1000 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}
function Wait-AirProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-AirProofLines $Pattern $Since) | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line within $Seconds seconds"
}
function Save-AirCapture([string]$Label, [int]$SettleSeconds = 0) {
    if ($SettleSeconds -gt 0) { Start-Sleep -Seconds $SettleSeconds }
    Send-AirCommand 'AirlockWhere'
    $null = Wait-AirProofLine ': Where:' 5 $script:LastCommandAt
    $capture = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $source = $capture.images | Select-Object -First 1
    if (-not $capture.ok -or -not $source) { throw "Capture failed: $($capture.text). Run restore-editor-window.ps1 and retry." }
    Copy-Item -LiteralPath $source -Destination (Join-Path $proofDir "$Label.png") -Force
    Write-Host "Capture: $proofDir/$Label.png"
}
function Assert-AirArrival([string]$Map, [double]$ExpectedX) {
    $line = Wait-AirProofLine ': Where:' 10 $script:LastCommandAt
    $position = [regex]::Match([string]$line, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+)')
    if ($line -notmatch "map=UEDPIE_\d+_$Map " -or -not $position.Success) { throw "Wrong arrival: $line" }
    $x = [double]::Parse($position.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $y = [double]::Parse($position.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ([math]::Abs($x - $ExpectedX) -gt 1 -or [math]::Abs($y - 1560) -gt 1) { throw "Arrival missed PlayerStart: $line" }
}
function Move-BesideCargo([string]$Id) {
    switch ($Id) {
        'C2' { Send-AirCommand 'AirlockWarp -5830 1700 116' }
        'ColdRack' { Send-AirCommand 'AirlockWarp -5230 1300 116' }
        default { throw "Unknown atmosphere proof spot $Id" }
    }
    Start-Sleep -Seconds 1
}
function Read-AirPpm {
    Send-AirCommand 'AirlockAir'
    $line = Wait-AirProofLine ': Air \d+ ppm scrub=' 5 $script:LastCommandAt
    return [int]([regex]::Match($line, ': Air (\d+) ppm').Groups[1].Value)
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
    Send-AirCommand 'AirlockWhere'
    Assert-AirArrival 'StationHub' -5640
    Send-AirCommand 'AirlockWarp -4700 1560 116'
    Send-AirCommand 'AirlockBook 0'
    Start-Sleep -Seconds 2
    Send-AirCommand 'AirlockWarp -4265 1560 116'
    $null = Wait-AirProofLine ': Incident director started: crew=1 entries=4' 45
    Send-AirCommand 'AirlockVoyageClock 1200'
    Send-AirCommand 'AirlockWhere'
    Assert-AirArrival 'Hauler_Mk1' -6660
    Send-AirCommand 'AirlockAir'
    $null = Wait-AirProofLine ': Air 0 ppm scrub=Normal brownout=0 disabled=0 sources=0' 5 $script:LastCommandAt

    Move-BesideCargo 'C2'
    Send-AirCommand 'AirlockLook 0 -15'
    Send-AirCommand 'AirlockCrateHeat C2 45'
    $warmAt = $script:LastCommandAt
    $null = Wait-AirProofLine ': Cargo C2 Loose temp=45' 5 $warmAt
    $null = Wait-AirProofLine ': Air 20 ppm scrub=Normal' 40 $warmAt
    $null = Wait-AirProofLine ': Captain .*Nitriles in the hold air' 5 $warmAt
    if ((Read-AirPpm) -lt 20) { throw 'First puff did not raise the air to warning' }
    Save-AirCapture 'first-puff'

    Send-AirCommand 'AirlockAirSet 80'
    $badAt = $script:LastCommandAt
    $null = Wait-AirProofLine ': Air 80 ppm scrub=Normal' 5 $badAt
    $null = Wait-AirProofLine ": Captain .*Air's bad" 5 $badAt
    Send-AirCommand 'AirlockWarp -1300 760 118'
    Start-Sleep -Seconds 2
    Send-AirCommand 'AirlockWhere'
    $walkway = Wait-AirProofLine ': Where:' 5 $script:LastCommandAt
    $probe = [regex]::Match($walkway, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+) Z=([-0-9.]+)')
    if (-not $probe.Success) { throw "Unparseable life-support walkway probe: $walkway" }
    $px = [double]::Parse($probe.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $py = [double]::Parse($probe.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    $pz = [double]::Parse($probe.Groups[3].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ([math]::Abs($px + 1300) -gt 1 -or [math]::Abs($py - 760) -gt 1 -or [math]::Abs($pz - 118) -gt 2) { throw "Walkway probe missed floor: $walkway" }
    Send-AirCommand 'AirlockLook 0'
    Send-AirCommand 'AirlockScrub Hazard'
    $hazardAt = $script:LastCommandAt
    $null = Wait-AirProofLine ': Scrub Hazard by AirlockCharacter_0' 5 $hazardAt
    $null = Wait-AirProofLine ': Air \d+ ppm scrub=Hazard' 5 $hazardAt
    Save-AirCapture 'scrubbing' 5
    if (([DateTime]::UtcNow - $hazardAt).TotalSeconds -gt 8) { throw 'Scrubbing capture missed its eight-second window' }

    Move-BesideCargo 'C2'
    Send-AirCommand 'AirlockCarry C2'
    $null = Wait-AirProofLine ': Carry C2 by' 5 $script:LastCommandAt
    Move-BesideCargo 'ColdRack'
    Send-AirCommand 'AirlockLook 0 -15'
    Send-AirCommand 'AirlockPlace ColdRack'
    $null = Wait-AirProofLine ': Cargo C2 Racked' 5 $script:LastCommandAt
    $remaining = [math]::Max(1, [math]::Ceiling(60 - ([DateTime]::UtcNow - $hazardAt).TotalSeconds))
    $null = Wait-AirProofLine ': Air 19 ppm scrub=Hazard' $remaining $hazardAt
    $null = Wait-AirProofLine ": Captain .*Air's clearing" 5 $hazardAt
    if (([DateTime]::UtcNow - $hazardAt).TotalSeconds -gt 60 -or (Read-AirPpm) -ge 20) { throw 'Hazard scrubbing did not clear the warning within 60 seconds' }

    Send-AirCommand 'AirlockAirSet 120'
    $null = Wait-AirProofLine ': Air 120 ppm scrub=Hazard' 5 $script:LastCommandAt
    Send-AirCommand 'AirlockExpose 90'
    $exposeAt = $script:LastCommandAt
    $null = Wait-AirProofLine ': Exposure AirlockCharacter_0 100%' 10 $exposeAt
    $blackout = Wait-AirProofLine ': Blackout AirlockCharacter_0' 2 $exposeAt
    $blackoutAt = [DateTime]::UtcNow
    Send-AirCommand 'AirlockLook 0 -15'
    Save-AirCapture 'blackout'
    $remaining = [math]::Max(1, [math]::Ceiling(40 - ([DateTime]::UtcNow - $blackoutAt).TotalSeconds))
    $null = Wait-AirProofLine ': Recovered AirlockCharacter_0' $remaining $exposeAt
    Send-AirCommand 'AirlockVoyageClock 1200'
    $clearDeadline = (Get-Date).AddSeconds(30)
    do {
        $ppm = Read-AirPpm
        if ($ppm -lt 10) { break }
        Start-Sleep -Seconds 1
    } while ((Get-Date) -lt $clearDeadline)
    if ($ppm -ge 10) { throw "Air did not clear for docking: $ppm ppm" }
    Send-AirCommand 'AirlockScrub Hazard'
    $null = Wait-AirProofLine ': Scrub Hazard by AirlockCharacter_0' 5 $script:LastCommandAt
    Send-AirCommand 'AirlockTally'
    $tally = '1/4 crates intact, 0 vented, 3 loose, hull 100\.00%, pay 400, air \d ppm, 1 blackouts'
    $null = Wait-AirProofLine ": Tally now: $tally" 5 $script:LastCommandAt
    Send-AirCommand 'AirlockShipStatus'
    $statusAt = $script:LastCommandAt
    $null = Wait-AirProofLine ': Ship Galley Nominal cond=100\.00' 5 $statusAt
    if (@(Get-AirProofLines ': Ship \w+ Nominal cond=100\.00' $statusAt).Count -ne 7) { throw 'All seven modules must remain at 100' }
    if (@(Get-AirProofLines ': Incident (CoolantLeak|PowerShort|ModuleDamage|CargoShift) ').Count -ne 0) { throw 'A scheduled incident fired during the calm proof' }
    Send-AirCommand 'AirlockWarp -7015 1560 116'
    $returnAt = $script:LastCommandAt
    $null = Wait-AirProofLine ": Voyage result: $tally" 5 $returnAt
    $null = Wait-AirProofLine ': Arrival spawn .*X=-5640\.' 45 $returnAt
    Send-AirCommand 'AirlockWhere'
    Assert-AirArrival 'StationHub' -5640
    Send-AirCommand 'AirlockAir'
    $null = Wait-AirProofLine ': No atmosphere on this section' 5 $script:LastCommandAt
    Send-AirCommand 'AirlockLook 0'
    Send-AirCommand 'AirlockBoard'
    $null = Wait-AirProofLine ': Voyage board opened' 5 $script:LastCommandAt
    Save-AirCapture 'hub-last-run-air' 8
    Write-Host 'ATMOSPHERE OK: warm cask puffs -> hazard scrub -> rack -> blackout and recovery -> clean air, pay 400 -> hub last run'
}
finally {
    if ($script:StartedPIE) {
        try { Get-AirProofLines | Set-Content -LiteralPath (Join-Path $proofDir 'atmosphere-log.txt') -Encoding UTF8 }
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
