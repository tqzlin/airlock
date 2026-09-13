# M3-8 PIE proof. Requires this checkout's clean StationHub on MCP port 8000.
# powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/verify-legibility.ps1
# Only the file console drives gameplay. HUD text is capture-only; logs never assert widget text.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$proofDir = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentCaptures/M3-8'
$commandPath = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false

function Send-LegibilityCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}
function Get-LegibilityProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 1000 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}
function Wait-LegibilityProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart) {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-LegibilityProofLines $Pattern $Since) | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line within $Seconds seconds"
}
function Save-LegibilityCapture([string]$Label, [int]$SettleSeconds = 8) {
    Start-Sleep -Seconds $SettleSeconds
    Send-LegibilityCommand 'AirlockWhere'
    $null = Wait-LegibilityProofLine ': Where:' 5 $script:LastCommandAt
    $capture = Mcp-Call $TS_App 'CaptureEditorImage' @{}
    $source = $capture.images | Select-Object -First 1
    if (-not $capture.ok -or -not $source) { throw "Capture failed: $($capture.text). Restore the editor window and retry." }
    Copy-Item -LiteralPath $source -Destination (Join-Path $proofDir "$Label.png") -Force
    Write-Host "Capture: $proofDir/$Label.png"
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
    Send-LegibilityCommand 'AirlockBook 2'
    Start-Sleep -Seconds 2
    Send-LegibilityCommand 'AirlockWarp -4265 1560 116'
    $null = Wait-LegibilityProofLine ': Incident director started: crew=1 entries=4' 45 $script:LastCommandAt
    Send-LegibilityCommand 'AirlockVoyageClock 1200'
    $null = Wait-LegibilityProofLine ': Cargo manifest: 12 casks, 10 slots' 5
    # Beside C1: the walkway spot at X -5450 looks straight into the pack's barrel and tarp props (reviewer, 2026-09-12).
    Send-LegibilityCommand 'AirlockWarp -6100 1500 116'
    Send-LegibilityCommand 'AirlockLook 90'
    Save-LegibilityCapture 'hold-north'
    Send-LegibilityCommand 'AirlockLook -90'
    Save-LegibilityCapture 'hold-south'

    Send-LegibilityCommand 'AirlockWarp -6330 1700 116'
    Send-LegibilityCommand 'AirlockLook 0'
    Send-LegibilityCommand 'AirlockCarry C1'
    $null = Wait-LegibilityProofLine ': Carry C1 by' 5 $script:LastCommandAt
    Save-LegibilityCapture 'carrying-no-label'
    Send-LegibilityCommand 'AirlockDrop'
    $null = Wait-LegibilityProofLine ': PutDown C1 by' 5 $script:LastCommandAt
    Send-LegibilityCommand 'AirlockVoyageClock 1200'
    Send-LegibilityCommand 'AirlockCrateHeat C2 70'
    Send-LegibilityCommand 'AirlockCrateHeat C4 85'
    $heatAt = $script:LastCommandAt
    $null = Wait-LegibilityProofLine ': Cargo C4 Loose temp=85' 5 $heatAt
    Send-LegibilityCommand 'AirlockCrateHeat C6 60'
    # Already settled here: capture within 5 s of the C4 crossing, before any unrelated delay.
    Save-LegibilityCapture 'board-summaries' 0
    $captureSeconds = ([DateTime]::UtcNow - $heatAt).TotalSeconds
    if ($captureSeconds -gt 5) { throw "Summary capture was late: $captureSeconds s after C4 heating" }
    Write-Host ('Summary captured {0:F2} s after C4 heating' -f $captureSeconds)

    # One step further back than the walkway spot so the life-support label and its light sit in frame.
    Send-LegibilityCommand 'AirlockWarp -1400 760 118'
    Send-LegibilityCommand 'AirlockLook 0'
    Save-LegibilityCapture 'engineering-panel'
    Send-LegibilityCommand 'AirlockVoyageClock 1200'
    Send-LegibilityCommand 'AirlockFuelLeak'
    $null = Wait-LegibilityProofLine ': SetPiece FuelLeak' 5 $script:LastCommandAt
    Send-LegibilityCommand 'AirlockWarp -1380 280 118'
    Send-LegibilityCommand 'AirlockLook 0'
    Save-LegibilityCapture 'feed-leak-steam'

    Send-LegibilityCommand 'AirlockWarp -7015 1560 116'
    $returnAt = $script:LastCommandAt
    $null = Wait-LegibilityProofLine ': Voyage result:' 5 $returnAt
    $null = Wait-LegibilityProofLine ': Arrival spawn .*X=-5640\.' 45 $returnAt
    Send-LegibilityCommand 'AirlockSaveReset'
    $null = Wait-LegibilityProofLine ': Save reset: base fuel 200 t, runs 0' 5 $script:LastCommandAt
    Write-Host 'LEGIBILITY OK: Priority hold -> north/south labels -> carried label hidden -> drop -> cargo summaries -> engineering panel -> feed steam -> refinery result -> hub, save reset'
}
finally {
    if ($script:StartedPIE) {
        try { Get-LegibilityProofLines | Set-Content -LiteralPath (Join-Path $proofDir 'legibility-log.txt') -Encoding UTF8 }
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
