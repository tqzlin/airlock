# M3-4 PIE proof. Run from the repo root with a clean StationHub open.
# powershell -ExecutionPolicy Bypass -File Scripts/verify-cargo.ps1
# Only the file console drives gameplay. Fresh logs and required captures: Saved/AgentCaptures/M3-4.
# Slate snapshots omit the PIE HUD; visually review the captures for row/caption visibility.
param([switch]$ProbePositions)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$proofDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentCaptures/M3-4'
$commandPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false

function Send-CargoCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}

function Get-CargoProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 1000 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}

function Wait-CargoProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-CargoProofLines $Pattern $Since) | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line within $Seconds seconds"
}

function Save-CargoCapture([string]$Label) {
    Start-Sleep -Seconds 8
    Send-CargoCommand 'AirlockWhere'
    $null = Wait-CargoProofLine 'Where:' 5 $script:LastCommandAt
    $capture = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $source = $capture.images | Select-Object -First 1
    if (-not $capture.ok -or -not $source) { throw "Capture failed: $($capture.text). Restore the editor window and retry." }
    Copy-Item -LiteralPath $source -Destination (Join-Path $proofDir "$Label.png") -Force
    Write-Host "Capture: $proofDir/$Label.png"
}

function Assert-CargoLocation([string]$Map, [double]$ExpectedX) {
    $line = Wait-CargoProofLine 'Where:' 10 $script:LastCommandAt
    $position = [regex]::Match([string]$line, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+)')
    if ($line -notmatch "map=UEDPIE_\d+_$Map " -or -not $position.Success) { throw "Wrong arrival: $line" }
    $x = [double]::Parse($position.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
    $y = [double]::Parse($position.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
    if ([math]::Abs($x - $ExpectedX) -gt 1 -or [math]::Abs($y - 1560) -gt 1) { throw "Arrival missed PlayerStart: $line" }
}

# Final table, measured by dropping a pawn at each centre. Cargo actors ignore pawn collision.
$cargoSpots = @(
    @('Clamp_1', -6200, 1400), @('Clamp_2', -5700, 1400), @('Clamp_3', -5200, 1400),
    @('ColdRack', -5000, 1300), @('C1', -6100, 1700), @('C2', -5600, 1700),
    @('C3', -5300, 1700), @('C4', -5000, 1700)
)
function Move-BesideCargo([string]$Id) {
    $spot = $cargoSpots | Where-Object { $_[0] -eq $Id } | Select-Object -First 1
    if (-not $spot) { throw "Unknown proof spot $Id" }
    # From the west, so labels facing -X and the item ahead are visible. Still within 312 cm.
    Send-CargoCommand "AirlockWarp $($spot[1] - 230) $($spot[2]) 116"
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
    if ($ProbePositions) { $null = Mcp-Text $TS_Scene 'load_level' @{level_path='/Game/Maps/Ships/Hauler_Mk1'} }
    $startX = if ($ProbePositions) { -6660 } else { -5640 }
    $script:ProofStart = [DateTime]::UtcNow
    $start = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = $startX; y = 1560; z = 118 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
    if (-not $start.ok) { throw $start.text }
    $script:StartedPIE = $true
    if ($ProbePositions) {
        $positions = foreach ($spot in $cargoSpots) {
            Send-CargoCommand "AirlockWarp $($spot[1]) $($spot[2]) 300"
            Start-Sleep -Seconds 4
            Send-CargoCommand 'AirlockWhere'
            $line = Wait-CargoProofLine 'Where:' 5 $script:LastCommandAt
            $match = [regex]::Match($line, 'pawn=.*? at X=([-0-9.]+) Y=([-0-9.]+) Z=([-0-9.]+)')
            if (-not $match.Success) { throw "Unparseable probe: $line" }
            $x = [double]::Parse($match.Groups[1].Value, [Globalization.CultureInfo]::InvariantCulture)
            $y = [double]::Parse($match.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture)
            $z = [double]::Parse($match.Groups[3].Value, [Globalization.CultureInfo]::InvariantCulture)
            if ([math]::Abs($x-$spot[1]) -gt 1 -or [math]::Abs($y-$spot[2]) -gt 1 -or [math]::Abs(($z-96)-(-65)) -gt 1) { throw "Cargo spot $($spot[0]) missed the -65 floor: $line" }
            [pscustomobject]@{Id=$spot[0]; X=$x; Y=$y; LayoutZ=-65; PawnZ=$z; FeetZ=$z-96}
        }
        $positions | Export-Csv -LiteralPath (Join-Path $proofDir 'cargo-position-probes.csv') -NoTypeInformation
        $positions | Format-Table | Out-Host
        Write-Host 'CARGO POSITIONS OK: all eight spots settle on the west hall floor'
    }
    else {
        Send-CargoCommand 'AirlockWhere'
        Assert-CargoLocation 'StationHub' -5640
        Send-CargoCommand 'AirlockCargoStatus'
        $null = Wait-CargoProofLine 'Cargo empty' 5 $script:LastCommandAt
        Send-CargoCommand 'AirlockWarp -4700 1560 116'
        Send-CargoCommand 'AirlockBook 0'
        Start-Sleep -Seconds 2
        Send-CargoCommand 'AirlockWarp -4265 1560 116'
        $null = Wait-CargoProofLine 'Incident director started: crew=1 entries=4' 45
        Send-CargoCommand 'AirlockWhere'
        Assert-CargoLocation 'Hauler_Mk1' -6660
        Send-CargoCommand 'AirlockCargoStatus'
        $manifestAt = $script:LastCommandAt
        $null = Wait-CargoProofLine 'Cargo slot ColdRack rack empty' 5 $manifestAt
        if (@(Get-CargoProofLines 'Cargo C[1-4] Loose temp=0' $manifestAt).Count -ne 4) { throw 'Expected four fresh loose crates' }
        if (@(Get-CargoProofLines 'Cargo slot .* empty' $manifestAt).Count -ne 4) { throw 'Expected four empty slots' }
        Move-BesideCargo 'C1'
        Save-CargoCapture 'loose-crate-tasks'
        Send-CargoCommand 'AirlockCarry C1'
        $null = Wait-CargoProofLine 'Carry C1 by' 5 $script:LastCommandAt
        Save-CargoCapture 'carrying-C1'
        Move-BesideCargo 'Clamp_1'
        Send-CargoCommand 'AirlockPlace Clamp_1'
        $null = Wait-CargoProofLine 'Cargo C1 Secured' 5 $script:LastCommandAt
        Move-BesideCargo 'C2'
        Send-CargoCommand 'AirlockCarry C2'
        $null = Wait-CargoProofLine 'Carry C2 by' 5 $script:LastCommandAt
        Move-BesideCargo 'Clamp_2'
        Send-CargoCommand 'AirlockPlace Clamp_2'
        $null = Wait-CargoProofLine 'Cargo C2 Secured' 5 $script:LastCommandAt
        Send-CargoCommand 'AirlockIncident CargoShift Clamp_2'
        $shiftAt = $script:LastCommandAt
        $null = Wait-CargoProofLine 'Cargo C2 Loose' 5 $shiftAt
        $null = Wait-CargoProofLine 'Incident CargoShift Clamp_2 \(0\.00\)' 5 $shiftAt
        $null = Wait-CargoProofLine "Captain .*cargo's shifted.*Clamp_2 let go" 5 $shiftAt
        Send-CargoCommand 'AirlockCrateHeat C2 60'
        $null = Wait-CargoProofLine 'Cargo C2 Loose temp=60' 5 $script:LastCommandAt
        Save-CargoCapture 'hot-crate-task'
        # C2 remains at Clamp_2 after the shift; this pawn is already beside that pad.
        Send-CargoCommand 'AirlockCarry C2'
        $null = Wait-CargoProofLine 'Carry C2 by' 5 $script:LastCommandAt
        Move-BesideCargo 'ColdRack'
        Send-CargoCommand 'AirlockPlace ColdRack'
        $null = Wait-CargoProofLine 'Cargo C2 Racked' 5 $script:LastCommandAt
        Start-Sleep -Seconds 10
        Send-CargoCommand 'AirlockCargoStatus'
        $cooled = Wait-CargoProofLine 'Cargo C2 Racked temp=' 5 $script:LastCommandAt
        $temperature = [int]([regex]::Match($cooled, 'temp=(\d+)').Groups[1].Value)
        if ($temperature -ge 60) { throw "Rack did not cool C2: $cooled" }
        Move-BesideCargo 'C4'
        Send-CargoCommand 'AirlockCrateHeat C4 100'
        $null = Wait-CargoProofLine 'Cargo C4 Vented temp=100' 5 $script:LastCommandAt
        Save-CargoCapture 'vented-C4'
        Send-CargoCommand 'AirlockTally'
        $null = Wait-CargoProofLine 'Tally now: 2/4 crates intact, 1 vented, 1 loose, hull 100\.00%, pay 400' 5 $script:LastCommandAt
        # This is a rewind, never a jump forward; no scheduled incidents belong in this proof.
        Send-CargoCommand 'AirlockVoyageClock 1200'
        Send-CargoCommand 'AirlockShipStatus'
        $statusAt = $script:LastCommandAt
        $null = Wait-CargoProofLine 'Ship Galley Nominal cond=100\.00' 5 $statusAt
        if (@(Get-CargoProofLines 'Ship \w+ Nominal cond=100\.00' $statusAt).Count -ne 7) { throw 'Ship must dock with all seven modules at 100' }
        # Anchor on the director's own line: the console echoes ('AgentConsole: AirlockIncident ...', 'Console: AirlockIncident ... accepted=1') also contain 'Incident CargoShift '.
        if (@(Get-CargoProofLines 'Incident ' | Where-Object { $_ -match ': Incident (CoolantLeak|PowerShort|ModuleDamage|CargoShift) ' }).Count -ne 1) { throw 'A scheduled incident fired during the calm proof' }
        Send-CargoCommand 'AirlockWarp -7015 1560 116'
        $returnAt = $script:LastCommandAt
        $null = Wait-CargoProofLine 'Voyage result: 2/4 crates intact, 1 vented, 1 loose, hull 100\.00%, pay 400' 5 $returnAt
        $null = Wait-CargoProofLine 'Arrival spawn .*X=-5640\.' 45 $returnAt
        Send-CargoCommand 'AirlockWhere'
        Assert-CargoLocation 'StationHub' -5640
        Send-CargoCommand 'AirlockCargoStatus'
        $null = Wait-CargoProofLine 'Cargo empty' 5 $script:LastCommandAt
        Send-CargoCommand 'AirlockBoard'
        $null = Wait-CargoProofLine 'Voyage board opened' 5 $script:LastCommandAt
        Save-CargoCapture 'hub-last-run'
        Write-Host 'CARGO OK: secure two -> shift and cool C2 -> vent C4 -> 2/4 intact, pay 400 -> hub last run'
    }
}
finally {
    if ($script:StartedPIE) {
        try { Get-CargoProofLines | Set-Content -LiteralPath (Join-Path $proofDir $(if ($ProbePositions) { 'position-probe-log.txt' } else { 'cargo-log.txt' })) -Encoding UTF8 }
        finally {
            $stop = Mcp-Call $TS_App 'StopPIE' @{}
            if (-not $stop.ok) { throw "StopPIE failed: $($stop.text)" }
        }
    }
    if ($ProbePositions) { $null = Mcp-Text $TS_Scene 'load_level' @{level_path='/Game/Maps/StationHub'} }
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/Ships/Hauler_Mk1')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map is dirty after the proof" }
    }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Editor did not return to StationHub' }
}
