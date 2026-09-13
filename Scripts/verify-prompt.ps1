# M5-5 PIE proof: the crosshair prompt. Run from the repo root with a clean StationHub open (port from
# AIRLOCK_MCP_PORT, default 8000):
#   powershell -ExecutionPolicy Bypass -File Scripts/verify-prompt.ps1
# Opens the floor map directly (no voyage: fixtures and controls spawn, no casks), starts PIE in front of Still 2's
# heat handle, and reads the `Prompt: <text>` lines the local pawn logs when the line under the crosshair changes:
# the handle's prompt, none when looking along the row, the hopper's prompt. Captures under Saved/AgentCaptures/M5-5.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'lib/mcp.ps1')
$root = Split-Path $PSScriptRoot -Parent
$proofDir = Join-Path $root 'Saved/AgentCaptures/M5-5'
$commandPath = Join-Path $root 'Saved/AgentConsole/commands.txt'
New-Item -ItemType Directory -Force -Path $proofDir, (Split-Path $commandPath -Parent) | Out-Null
$script:ProofStart = [DateTime]::UtcNow
$script:StartedPIE = $false
$script:LoadedFloor = $false

function Send-PromptCommand([string]$Command) {
    if (Test-Path -LiteralPath $commandPath) { throw 'A previous AgentConsole command is still pending' }
    $script:LastCommandAt = [DateTime]::UtcNow
    [IO.File]::WriteAllText($commandPath, "$Command`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(10)
    while ((Test-Path -LiteralPath $commandPath) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 100 }
    if (Test-Path -LiteralPath $commandPath) { throw "Console command was not consumed: $Command" }
    Write-Host "> $Command"
}
function Get-PromptProofLines([string]$Pattern = '', [DateTime]$Since = $script:ProofStart) {
    $result = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $Pattern; maxEntries = 1000 }
    if (-not $result.ok) { throw $result.text }
    foreach ($match in [regex]::Matches($result.text, '\[(\d{4}\.\d{2}\.\d{2}-\d{2}\.\d{2}\.\d{2}:\d{3})\]\[\s*\d+\][^\[]*')) {
        $stamp = [DateTime]::ParseExact($match.Groups[1].Value, 'yyyy.MM.dd-HH.mm.ss:fff', [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::AssumeUniversal).ToUniversalTime()
        if ($stamp -ge $Since) { $match.Value -replace '(\\r)?\\n.*$|"[,\]}].*$', '' }
    }
}
function Wait-PromptProofLine([string]$Pattern, [int]$Seconds, [DateTime]$Since = $script:ProofStart, [string]$MustContain = '') {
    $deadline = (Get-Date).AddSeconds($Seconds)
    do {
        $line = @(Get-PromptProofLines $Pattern $Since) | Where-Object { -not $MustContain -or $_ -like "*$MustContain*" } | Select-Object -Last 1
        if ($line) { Write-Host $line; return $line }
        Start-Sleep -Milliseconds 200
    } while ((Get-Date) -lt $deadline)
    throw "No fresh '$Pattern' line$(if ($MustContain) { " containing '$MustContain'" }) within $Seconds seconds"
}
function Save-PromptCapture([string]$Label, [int]$SettleSeconds = 6) {
    Start-Sleep -Seconds $SettleSeconds
    Send-PromptCommand 'AirlockWhere'
    $null = Wait-PromptProofLine ': Where:' 5 $script:LastCommandAt
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
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/RefineryFloor')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map must be clean before this proof" }
    }
    $load = Mcp-Call $TS_Scene 'load_level' @{ level_path = '/Game/Maps/RefineryFloor' }
    if (-not $load.ok) { throw "Could not open the floor: $($load.text)" }
    $script:LoadedFloor = $true
    Start-Sleep -Seconds 8
    $script:ProofStart = [DateTime]::UtcNow
    # A metre in front of Still 2's heat handle (Still_2_HeatPlus at -5480, 1700), facing it.
    $start = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -5480; y = 1600; z = 150 }; rotation = @{ pitch = 0; yaw = 90; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
    if (-not $start.ok) { throw "StartPIE failed: $($start.text)" }
    $script:StartedPIE = $true
    $null = Wait-PromptProofLine 'AgentConsole: ready' 30
    $null = Wait-PromptProofLine ': Still controls:' 30
    Send-PromptCommand 'AirlockLook 90 -35'
    $handle = Wait-PromptProofLine ': Prompt:' 15 $script:LastCommandAt 'Heat'
    Save-PromptCapture 'prompt-handle' 4
    # Along the row there is nothing within reach: the line goes away.
    Send-PromptCommand 'AirlockLook 0 0'
    $null = Wait-PromptProofLine ': Prompt: none' 15 $script:LastCommandAt
    # The hopper, empty-handed.
    Send-PromptCommand 'AirlockWarp -5600 1600 116'
    Send-PromptCommand 'AirlockLook 90 -55'
    $hopper = Wait-PromptProofLine ': Prompt:' 15 $script:LastCommandAt 'Hopper'
    Save-PromptCapture 'prompt-hopper' 4
    Write-Host "PROMPT OK: floor direct -> $($handle -replace '^.*Prompt: ', '') -> none along the row -> $($hopper -replace '^.*Prompt: ', '')"
}
finally {
    if ($script:StartedPIE) {
        try { Get-PromptProofLines | Set-Content -LiteralPath (Join-Path $proofDir 'prompt-log.txt') }
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
    foreach ($map in @('/Game/Maps/StationHub', '/Game/Maps/RefineryFloor')) {
        $dirty = Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $map }
        if (-not $dirty.ok -or $dirty.text -match 'true') { throw "$map is dirty after the proof" }
    }
    if ((Mcp-Call $TS_Scene 'get_current_level' @{}).text -notmatch 'StationHub') { throw 'Editor did not return to StationHub' }
}
