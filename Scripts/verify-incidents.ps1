# M3-3 PIE proof. Run from the repo root with a clean StationHub open.
# powershell -ExecutionPolicy Bypass -File Scripts/verify-incidents.ps1
# Only the file console drives gameplay. Fresh logs and required captures: Saved/AgentCaptures/M3-3.
# Slate snapshots omit the PIE HUD; visually review the captures for row/caption visibility.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$proofDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentCaptures/M3-3'
$commandPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false

function Send-IncidentCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}

function Get-IncidentProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 1000 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}

function Wait-IncidentProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-IncidentProofLines $Pattern $Since) | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line within $Seconds seconds"
}

function Save-IncidentCapture([string]$Label) {
    $capture = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $source = $capture.images | Select-Object -First 1
    if (-not $capture.ok -or -not $source) { throw "Capture failed: $($capture.text). Restore the editor window and retry." }
    Copy-Item -LiteralPath $source -Destination (Join-Path $proofDir "$Label.png") -Force
    Write-Host "Capture: $proofDir/$Label.png"
}

function Assert-IncidentLocation([string]$Map, [double]$ExpectedX) {
    $line = Wait-IncidentProofLine 'Where:' 10 $script:LastCommandAt
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
    Send-IncidentCommand 'AirlockWhere'
    Assert-IncidentLocation 'StationHub' -5640
    Send-IncidentCommand 'AirlockIncidentSchedule'
    $null = Wait-IncidentProofLine 'Incident schedule empty' 5 $script:LastCommandAt
    Send-IncidentCommand 'AirlockWarp -4700 1560 116'
    Send-IncidentCommand 'AirlockBook 0'
    Start-Sleep -Seconds 2
    Send-IncidentCommand 'AirlockWarp -4265 1560 116'
    $null = Wait-IncidentProofLine 'Incident director started: crew=1 entries=4' 45
    Start-Sleep -Seconds 8
    Send-IncidentCommand 'AirlockWhere'
    Assert-IncidentLocation 'Hauler_Mk1' -6660
    Send-IncidentCommand 'AirlockIncidentSchedule'
    $scheduleAt = $script:LastCommandAt
    $header = Wait-IncidentProofLine 'Incident schedule: 4 entries next=0 duration=' 5 $scheduleAt
    $duration = [double]([regex]::Match($header, 'duration=([0-9.]+)s').Groups[1].Value)
    $null = Wait-IncidentProofLine 'Incident schedule entry=3' 5 $scheduleAt
    $scheduleLines = @(Get-IncidentProofLines 'Incident schedule entry=' $scheduleAt)
    if ($scheduleLines.Count -ne 4) { throw "Expected exactly four schedule entries, found $($scheduleLines.Count)" }
    $schedule = @($scheduleLines | ForEach-Object {
        $entry = [regex]::Match($_, 'schedule entry=(\d+) at ([0-9.]+)s (\w+) (\w+)')
        if (-not $entry.Success) { throw "Invalid schedule line: $_" }
        [pscustomobject]@{ Index=[int]$entry.Groups[1].Value; At=[double]::Parse($entry.Groups[2].Value, [Globalization.CultureInfo]::InvariantCulture); Type=$entry.Groups[3].Value; Target=$entry.Groups[4].Value }
    })
    if ($schedule[0].Type -ne 'CoolantLeak') { throw 'The teaching incident is not a coolant leak' }
    $target = $schedule[0].Target
    $targetX = switch ($target) { 'Coolant_1' { -6100 } 'Coolant_2' { -5700 } default { throw "Unknown teaching line: $target" } }
    Send-IncidentCommand "AirlockWarp $targetX 1400 116"
    Start-Sleep -Seconds 8
    Send-IncidentCommand 'AirlockWhere'
    $null = Wait-IncidentProofLine "Where:.*Hauler_Mk1.*pawn=.*at X=$targetX\." 5 $script:LastCommandAt
    Save-IncidentCapture 'tasks-nominal'
    Send-IncidentCommand "AirlockVoyageClock $($duration - 160)"
    $firstAt = $script:LastCommandAt
    $null = Wait-IncidentProofLine "Incident CoolantLeak $target .*at 150\.000s" 15 $firstAt
    $null = Wait-IncidentProofLine "Captain .*find the leak at $target" 5 $firstAt
    $null = Wait-IncidentProofLine "Line $target fault=Leak" 5 $firstAt
    Save-IncidentCapture 'teaching-leak-and-captain'
    Send-IncidentCommand "AirlockHold $target"
    $holdAt = $script:LastCommandAt
    $null = Wait-IncidentProofLine "Hold $target complete by" 6 $holdAt
    $null = Wait-IncidentProofLine "Line $target fault=None" 2 $holdAt
    Start-Sleep -Milliseconds 500
    Send-IncidentCommand 'AirlockWhere'
    Save-IncidentCapture 'teaching-leak-repaired'
    Send-IncidentCommand 'AirlockIncident ModuleDamage PowerCore 45'
    $null = Wait-IncidentProofLine 'Ship PowerCore Degraded cond=55\.00' 5 $script:LastCommandAt
    Start-Sleep -Milliseconds 500
    Save-IncidentCapture 'core-damaged-task'
    Send-IncidentCommand 'AirlockVoyageClock 70'
    $jumpAt = $script:LastCommandAt
    $last = $schedule[3]
    $null = Wait-IncidentProofLine "Incident $($last.Type) $($last.Target) .*at $([regex]::Escape(('{0:F3}' -f $last.At)))s" 15 $jumpAt
    $fired = @(Get-IncidentProofLines 'Incident (CoolantLeak|PowerShort|ModuleDamage) ' $jumpAt)
    if ($fired.Count -ne 3) { throw "Expected three catch-up events, found $($fired.Count)" }
    $pair = $fired | Select-Object -Last 2
    $pairStamps = @($pair | ForEach-Object { [DateTime]::ParseExact($_.Substring(1,23), 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture) })
    if (($pairStamps[1] - $pairStamps[0]).TotalSeconds -gt 20) { throw 'Climax pair did not fire within 20 seconds' }
    if (($schedule[3].At - $schedule[2].At) -lt 9.999 -or ($schedule[3].At - $schedule[2].At) -gt 20.001) { throw 'Climax schedule spacing invalid' }
    foreach ($entry in $schedule[2..3]) { if ($entry.At -lt $duration*.78 -or $entry.At -gt $duration*.85) { throw 'Climax outside 78..85 percent' } }
    Send-IncidentCommand 'AirlockWhere'
    $null = Save-IncidentCapture 'climax-tasks'
    # Rewind and repeat the jump: consumed entries must not fire again, including skipped events.
    Send-IncidentCommand "AirlockVoyageClock $duration"
    Send-IncidentCommand 'AirlockVoyageClock 70'
    $repeatAt = $script:LastCommandAt
    Start-Sleep -Seconds 1
    if (@(Get-IncidentProofLines 'Incident (CoolantLeak|PowerShort|ModuleDamage) ' $repeatAt).Count -ne 0) { throw 'A clock rewind replayed incidents' }
    Send-IncidentCommand 'AirlockIncidentSchedule'
    $null = Wait-IncidentProofLine 'Incident schedule: 4 entries next=4' 5 $script:LastCommandAt
    # Optional console arguments and missing/already-faulted targets use the same director path.
    Send-IncidentCommand 'AirlockFault Power_1 None'
    Send-IncidentCommand 'AirlockIncident PowerShort'
    $null = Wait-IncidentProofLine 'Incident PowerShort Power_1 .*elapsed=' 5 $script:LastCommandAt
    Send-IncidentCommand 'AirlockIncident PowerShort Power_1'
    $null = Wait-IncidentProofLine 'Incident PowerShort Power_1 .*skipped: already faulted' 5 $script:LastCommandAt
    Send-IncidentCommand 'AirlockIncident ModuleDamage Missing 45'
    $null = Wait-IncidentProofLine 'Incident ModuleDamage Missing .*skipped: missing' 5 $script:LastCommandAt
    Send-IncidentCommand 'AirlockRepairModule PowerCore 100'
    Send-IncidentCommand 'AirlockIncident ModuleDamage'
    $null = Wait-IncidentProofLine 'Incident ModuleDamage PowerCore \(40\.00\)' 5 $script:LastCommandAt
    Send-IncidentCommand 'AirlockWarp -7015 1560 116'
    $returnAt = $script:LastCommandAt
    $null = Wait-IncidentProofLine 'Arrival spawn .*X=-5640\.' 45 $returnAt
    Start-Sleep -Seconds 8
    Send-IncidentCommand 'AirlockWhere'
    Assert-IncidentLocation 'StationHub' -5640
    $null = Wait-IncidentProofLine 'Voyage .* complete' 5 $returnAt
    Send-IncidentCommand 'AirlockIncidentSchedule'
    $null = Wait-IncidentProofLine 'Incident schedule empty' 5 $script:LastCommandAt
    Save-IncidentCapture 'returned-to-hub'
    Write-Host 'INCIDENTS OK: solo schedule -> teaching leak and captain -> hold repair clears row -> core task -> climax -> no replay -> hub'
}
finally {
    if ($script:StartedPIE) {
        try { Get-IncidentProofLines | Set-Content -LiteralPath (Join-Path $proofDir 'incidents-log.txt') -Encoding UTF8 }
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
