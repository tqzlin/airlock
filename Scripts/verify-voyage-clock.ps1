# Repeat M3-1's timed return proof. Requires the editor open on a clean StationHub.
# Console input uses Saved/AgentConsole/commands.txt exclusively; never the Slate console.
# Run from the project root: powershell -ExecutionPolicy Bypass -File Scripts/verify-voyage-clock.ps1
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$proofDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/M3VoyageClock'
$commandPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false

function Send-VoyageCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}

function Get-VoyageProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 500 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}

function Wait-VoyageProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-VoyageProofLines $Pattern $Since) | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line within $Seconds seconds"
}

function Save-VoyageProofCapture([string]$Label) {
    $capture = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $source = $capture.images | Select-Object -First 1
    if (-not $capture.ok -or -not $source) { throw "Capture failed: $($capture.text). Restore the editor window and retry." }
    $destination = Join-Path $proofDir "$Label.png"
    Copy-Item -LiteralPath $source -Destination $destination -Force
    Write-Host "Capture: $destination"
}

function Assert-VoyageProofLocation([string]$Map, [double]$ExpectedX) {
    # Command-file deletion precedes execution, and the MCP log view can lag by a frame.
    # Require a Where response stamped after this command, never an earlier arrival.
    $line = Wait-VoyageProofLine 'Where:' 10 $script:LastCommandAt
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
    $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = '/Game/Maps/StationHub' }
    if (-not $dirty.ok -or $dirty.text -match 'true') { throw 'StationHub must be clean before running this proof' }
    $script:ProofStart = [DateTime]::UtcNow
    $start = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -5640; y = 1560; z = 118 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
    if (-not $start.ok) { throw $start.text }
    $script:StartedPIE = $true
    Send-VoyageCommand 'AirlockWhere'
    Send-VoyageCommand 'AirlockWarp -4700 1560 116'
    Send-VoyageCommand 'AirlockBook 0'
    Start-Sleep -Seconds 2
    Send-VoyageCommand 'AirlockWarp -4265 1560 116'
    $null = Wait-VoyageProofLine 'Section Hauler_Mk1' 45
    Start-Sleep -Seconds 8
    Send-VoyageCommand 'AirlockWhere'
    Assert-VoyageProofLocation 'Hauler_Mk1' -6660
    Send-VoyageCommand 'AirlockVoyageClock 75'
    $null = Wait-VoyageProofLine 'Voyage clock moved:.*remaining=75\.000s' 5
    $null = Wait-VoyageProofLine 'Captain .*One minute to dock' 30
    Send-VoyageCommand 'AirlockWhere'
    Save-VoyageProofCapture 'one-minute'
    # The six-second caption must disappear while the clock continues ticking.
    Start-Sleep -Seconds 7
    Save-VoyageProofCapture 'caption-hidden'
    $null = Wait-VoyageProofLine 'Transition card: DOCKING - HALDER SOLAR REFINERY' 80
    Save-VoyageProofCapture 'return-card-1'
    Save-VoyageProofCapture 'return-card-2'
    Save-VoyageProofCapture 'return-card-3'
    # Since M5-2 the docking lands on the refinery floor; the slot's end is the second hop home.
    $null = Wait-VoyageProofLine 'Section RefineryFloor:' 45 $script:LastCommandAt
    $null = Wait-VoyageProofLine 'Slot clock started' 30 $script:LastCommandAt
    Start-Sleep -Seconds 4
    Send-VoyageCommand 'AirlockVoyageClock 5'
    $null = Wait-VoyageProofLine 'Transition card: UNDOCKING - HOME TO THE VELL EXTRACTOR' 30 $script:LastCommandAt
    $null = Wait-VoyageProofLine 'Arrival spawn .*X=-5640\.' 45 $script:LastCommandAt
    Start-Sleep -Seconds 8
    Send-VoyageCommand 'AirlockWhere'
    Assert-VoyageProofLocation 'StationHub' -5640
    $null = Wait-VoyageProofLine 'Voyage .* complete' 5
    Save-VoyageProofCapture 'auto-return-hub'
    Write-Host 'CLOCK OK: 75 seconds remaining -> one-minute captain line -> automatic return card -> StationHub PlayerStart, voyage complete'
}
finally {
    if ($script:StartedPIE) {
        Get-VoyageProofLines | Set-Content -LiteralPath (Join-Path $proofDir 'voyage-clock-log.txt') -Encoding UTF8
        $stop = Mcp-Call $TS_App 'StopPIE' @{}
        if (-not $stop.ok) { Write-Warning $stop.text }
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = '/Game/Maps/StationHub' }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw 'StationHub is dirty after the proof' }
    }
}
