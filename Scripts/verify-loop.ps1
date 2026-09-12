# Scripts\verify-loop.ps1 -- the PIE proof of the MVP loop. Needs the editor open on StationHub with the MCP port up
# (the editor may be behind other windows or minimized: commands go through Saved\AgentConsole, not the Slate console).
#   powershell -ExecutionPolicy Bypass -File Scripts\verify-loop.ps1
# Spawns at the hub PlayerStart, books a voyage, crosses Dock B, expects to arrive at the Hauler's PlayerStart, walks
# back out through Airlock_ToHub, expects to arrive at the hub PlayerStart. Exit 0 only if both arrivals landed where
# they should and the voyage was logged complete. Captures: Saved\AgentCaptures\.
$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot 'lib\mcp.ps1')
function Compact($t) { ($t -replace '\s+', ' ') }
function Short($t, $n) { $c = Compact $t; $c.Substring(0, [math]::Min($n, $c.Length)) }
function Write-Say($t) { Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $t" }

try { Mcp-Init } catch { Write-Say "MCP server not reachable on 127.0.0.1:$($script:McpPort) - open the editor first (Scripts\run-editor.bat)"; exit 2 }
if ((Mcp-Call $TS_App 'IsPIERunning' @{}).text -match 'true') { Write-Say 'PIE already running - stopping it first'; $null = Mcp-Call $TS_App 'StopPIE' @{}; Start-Sleep -Seconds 4 }
$lvl = Mcp-Call $TS_Scene 'get_current_level' @{}
if ($lvl.text -notmatch 'StationHub') {
    Write-Say "current level is $(Short $lvl.text 80); loading /Game/Maps/StationHub"
    $r = Mcp-Call $TS_Scene 'load_level' @{ level_path = '/Game/Maps/StationHub' }
    if (-not $r.ok) { Write-Say "load_level failed (dirty level?): $(Short $r.text 200)"; exit 2 }
    Start-Sleep -Seconds 5
}

# dismiss the Message Log that pops on PIE (best effort; Slate may list nothing while the editor is minimized)
try { $w = Mcp-Call $TS_Slate 'Windows' @{ action = 'list'; index = -1 }; $wins = ($w.text | ConvertFrom-Json).returnValue; if ($wins -is [string]) { $wins = $wins | ConvertFrom-Json }; foreach ($x in $wins) { if ($x.title -match 'Message Log') { $null = Mcp-Call $TS_Slate 'Windows' @{ action = 'close'; index = $x.index } } } } catch {}
# console commands go through the AgentConsole file (UAgentConsoleSubsystem polls it every 0.5 s in PIE) - no Slate,
# no keyboard focus, works with the editor behind other windows
$script:CmdFile = Join-Path (Split-Path $PSScriptRoot -Parent) 'Saved\AgentConsole\commands.txt'
New-Item -ItemType Directory -Force -Path (Split-Path $script:CmdFile -Parent) | Out-Null
if (Test-Path $script:CmdFile) { Remove-Item $script:CmdFile -Force }
function Send-Console($cmd) {
    [IO.File]::WriteAllText($script:CmdFile, "$cmd`r`n", [Text.Encoding]::ASCII)
    $deadline = (Get-Date).AddSeconds(8)
    while ((Get-Date) -lt $deadline -and (Test-Path $script:CmdFile)) { Start-Sleep -Milliseconds 250 }
    if (Test-Path $script:CmdFile) { Write-Say "> $cmd  (NOT consumed within 8 s - is PIE running with the Airlock GameInstance?)" } else { Write-Say "> $cmd" }
}
# Only log lines stamped after the run started count (the editor log keeps earlier PIE sessions). UE stamps in UTC.
$script:RunStart = [DateTime]::UtcNow.AddSeconds(-5)
function Get-FreshLine($text, $pattern) {
    # newest matching line stamped after the run started (entries arrive oldest first)
    $best = $null
    foreach ($m in [regex]::Matches($text, '\[(\d{4})\.(\d{2})\.(\d{2})-(\d{2})\.(\d{2})\.(\d{2}):(\d{3})\]\[\s*\d+\][^\[]*')) {
        $line = $m.Value
        if ($line -notmatch $pattern) { continue }
        $g = $m.Groups
        $ts = New-Object DateTime ([int]$g[1].Value), ([int]$g[2].Value), ([int]$g[3].Value), ([int]$g[4].Value), ([int]$g[5].Value), ([int]$g[6].Value), ([int]$g[7].Value), ([DateTimeKind]::Utc)
        if ($ts -ge $script:RunStart) { $best = $line }
    }
    return $best
}
function Get-PawnWhere {
    Send-Console 'AirlockWhere'; Start-Sleep -Seconds 1
    $l = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = 'Where:'; maxEntries = 3 }
    $line = Get-FreshLine $l.text 'Where:'
    $s = if ($line) { ($line -replace '.*Where: ', 'Where: ') -replace '"\]\}.*$', '' } else { 'Where: (no fresh line)' }
    Write-Say "  $s"; return $s
}
function Wait-LogLine($pattern, $seconds) {
    $deadline = (Get-Date).AddSeconds($seconds)
    do { Start-Sleep -Seconds 3; $l = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $pattern; maxEntries = 5 }; $hit = Get-FreshLine $l.text $pattern } while ((Get-Date) -lt $deadline -and -not $hit)
    return [bool]$hit
}
function Save-Capture($label) { $c = Mcp-Call $TS_App 'CaptureEditorImage' @{}; Write-Say "  capture $label -> $(($c.images | Select-Object -First 1))" }

$fail = @()
Write-Say 'StartPIE at the hub PlayerStart'
$null = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -5640; y = 1560; z = 118 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
Start-Sleep -Seconds 8
$w0 = Get-PawnWhere
if ($w0 -notmatch 'map=UEDPIE_\d+_StationHub' -or $w0 -notmatch 'pawn=\S+ at X=-56[3-4][0-9]\.\d+ Y=15[5-6][0-9]\.') { $fail += 'hub spawn not at PlayerStart_Hub' }
Save-Capture 'hub-spawn'

Send-Console 'AirlockWarp -4700 1560 116'
Send-Console 'AirlockBook 0'
Start-Sleep -Seconds 2
Send-Console 'AirlockWarp -4265 1560 116'
if (-not (Wait-LogLine 'Section Hauler_Mk1' 45)) { $fail += 'no arrival on Hauler_Mk1 within 45 s' }
Start-Sleep -Seconds 8
$w1 = Get-PawnWhere
if ($w1 -notmatch 'map=UEDPIE_\d+_Hauler_Mk1' -or $w1 -notmatch 'pawn=\S+ at X=-66[5-7][0-9]\.\d+ Y=15[5-6][0-9]\.') { $fail += 'ship arrival not at PlayerStart_Ship' }
Save-Capture 'ship-arrival'

Send-Console 'AirlockWarp -7015 1560 116'
if (-not (Wait-LogLine 'travelling to /Game/Maps/StationHub' 45)) { $fail += 'return travel not triggered within 45 s' }
Start-Sleep -Seconds 12
$w2 = Get-PawnWhere
if ($w2 -notmatch 'map=UEDPIE_\d+_StationHub' -or $w2 -notmatch 'pawn=\S+ at X=-56[3-4][0-9]\.\d+ Y=15[5-6][0-9]\.') { $fail += 'hub return not at PlayerStart_Hub' }
Save-Capture 'hub-return'
$done = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = 'complete'; maxEntries = 5 }
if (-not (Get-FreshLine $done.text 'Voyage .* complete')) { $fail += 'no fresh "Voyage ... complete" in the log' }

$null = Mcp-Call $TS_App 'StopPIE' @{}
Start-Sleep -Seconds 2
$errs = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = ''; pattern = 'Error:'; maxEntries = 5 }
if ($errs.text -match 'Error:') { Write-Say "errors in the log during the run: $(Short $errs.text 400)" }

if ($fail.Count -eq 0) { Write-Say 'LOOP OK: hub spawn -> booked -> Dock B -> Hauler_Mk1 PlayerStart -> exit -> hub PlayerStart, voyage complete'; exit 0 }
else { Write-Say ('LOOP FAILED: ' + ($fail -join '; ') + '  (if someone was playing in this PIE session, their input moves the pawn - rerun with hands off)'); exit 1 }
