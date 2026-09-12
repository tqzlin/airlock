# Scripts\verify-loop.ps1 -- the PIE proof of the MVP loop. Needs the editor open on StationHub with the MCP port up.
#   powershell -ExecutionPolicy Bypass -File Scripts\verify-loop.ps1
# Spawns at the hub PlayerStart, books a voyage, crosses Dock B, expects to arrive at the Hauler's PlayerStart, walks
# back out through Airlock_ToHub, expects to arrive at the hub PlayerStart. Exit 0 only if both arrivals landed where
# they should and the voyage was logged complete. Captures: Saved\AgentCaptures\.
$ErrorActionPreference = 'Continue'
. (Join-Path $PSScriptRoot 'lib\mcp.ps1')
function Compact($t) { ($t -replace '\s+', ' ') }
function Short($t, $n) { $c = Compact $t; $c.Substring(0, [math]::Min($n, $c.Length)) }
function Say($t) { Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $t" }

try { Mcp-Init } catch { Say "MCP server not reachable on 127.0.0.1:8000 - open the editor first (Scripts\run-editor.bat)"; exit 2 }
if ((Mcp-Call $TS_App 'IsPIERunning' @{}).text -match 'true') { Say 'PIE already running - stopping it first'; $null = Mcp-Call $TS_App 'StopPIE' @{}; Start-Sleep -Seconds 4 }
$lvl = Mcp-Call $TS_Scene 'get_current_level' @{}
if ($lvl.text -notmatch 'StationHub') {
    Say "current level is $(Short $lvl.text 80); loading /Game/Maps/StationHub"
    $r = Mcp-Call $TS_Scene 'load_level' @{ level_path = '/Game/Maps/StationHub' }
    if (-not $r.ok) { Say "load_level failed (dirty level?): $(Short $r.text 200)"; exit 2 }
    Start-Sleep -Seconds 5
}

# dismiss the Message Log that pops on PIE, find the console box
$w = Mcp-Call $TS_Slate 'Windows' @{ action = 'list'; index = -1 }
try { $wins = ($w.text | ConvertFrom-Json).returnValue; if ($wins -is [string]) { $wins = $wins | ConvertFrom-Json }; foreach ($x in $wins) { if ($x.title -match 'Message Log') { $null = Mcp-Call $TS_Slate 'Windows' @{ action = 'close'; index = $x.index } } } } catch {}
$snap = Mcp-Call $TS_Slate 'Snapshot' @{ ref = ''; maxDepth = 40; bIncludeSourceLocations = $false }
$consoleRef = $null
$lines = (($snap.text | ConvertFrom-Json).returnValue) -split "`n"
for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match 'text "Cmd"') { for ($k = $i + 1; $k -lt [math]::Min($i + 6, $lines.Count); $k++) { if ($lines[$k] -match 'textbox.*\[ref=([A-Za-z0-9_]+)\]') { $consoleRef = $matches[1]; break } }; break } }
if (-not $consoleRef) {
    # fallback: any textbox within 5 lines after a text widget labelled Cmd / Python / Console
    for ($i = 0; $i -lt $lines.Count; $i++) { if ($lines[$i] -match 'text "(Cmd|Python|Console|Enter Console Command)') { for ($k = $i + 1; $k -lt [math]::Min($i + 6, $lines.Count); $k++) { if ($lines[$k] -match 'textbox.*\[ref=([A-Za-z0-9_]+)\]') { $consoleRef = $matches[1]; break } }; if ($consoleRef) { break } } }
}
if (-not $consoleRef) {
    Say 'could not find the editor console box in the Slate snapshot; lines mentioning Cmd/Console/textbox:'
    $lines | Where-Object { $_ -match 'Cmd|Console|textbox' } | Select-Object -First 12 | ForEach-Object { Say "    $(Short $_ 160)" }
    Say "  snapshot ok=$($snap.ok) length=$($snap.text.Length)"
    exit 2
}
function Console($cmd) {
    $t = Mcp-Call $TS_Slate 'Type' @{ ref = $consoleRef; text = $cmd; submit = $true }
    if ($t.text -notmatch 'true') { Start-Sleep -Milliseconds 500; $t = Mcp-Call $TS_Slate 'Type' @{ ref = $consoleRef; text = $cmd; submit = $true } }
    Say "> $cmd"
}
# Only log lines stamped after the run started count (the editor log keeps earlier PIE sessions). UE stamps in UTC.
$script:RunStart = [DateTime]::UtcNow.AddSeconds(-5)
function FreshLine($text, $pattern) {
    foreach ($m in [regex]::Matches($text, '\[(\d{4})\.(\d{2})\.(\d{2})-(\d{2})\.(\d{2})\.(\d{2}):(\d{3})\]\[\s*\d+\][^\[]*')) {
        $line = $m.Value
        if ($line -notmatch $pattern) { continue }
        $g = $m.Groups
        $ts = New-Object DateTime ([int]$g[1].Value), ([int]$g[2].Value), ([int]$g[3].Value), ([int]$g[4].Value), ([int]$g[5].Value), ([int]$g[6].Value), ([int]$g[7].Value), ([DateTimeKind]::Utc)
        if ($ts -ge $script:RunStart) { return $line }
    }
    return $null
}
function Where() {
    Console 'AirlockWhere'; Start-Sleep -Seconds 1
    $l = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = 'Where:'; maxEntries = 3 }
    $line = FreshLine $l.text 'Where:'
    $s = if ($line) { ($line -replace '.*Where: ', 'Where: ') -replace '"\]\}.*$', '' } else { 'Where: (no fresh line)' }
    Say "  $s"; return $s
}
function WaitLog($pattern, $seconds) {
    $deadline = (Get-Date).AddSeconds($seconds)
    do { Start-Sleep -Seconds 3; $l = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = $pattern; maxEntries = 5 }; $hit = FreshLine $l.text $pattern } while ((Get-Date) -lt $deadline -and -not $hit)
    return [bool]$hit
}
function Capture($label) { $c = Mcp-Call $TS_App 'CaptureEditorImage' @{}; Say "  capture $label -> $(($c.images | Select-Object -First 1))" }

$fail = @()
Say 'StartPIE at the hub PlayerStart'
$null = Mcp-Call $TS_App 'StartPIE' @{ options = @{ bSimulate = $false; playMode = 'PlayMode_InViewPort'; warmupSeconds = 5; startTransform = @{ location = @{ x = -5640; y = 1560; z = 118 }; rotation = @{ pitch = 0; yaw = 0; roll = 0 }; scale = @{ x = 1; y = 1; z = 1 } } } }
Start-Sleep -Seconds 8
$w0 = Where
if ($w0 -notmatch 'map=UEDPIE_\d+_StationHub' -or $w0 -notmatch 'pawn=\S+ at X=-56[3-4][0-9]\.\d+ Y=15[5-6][0-9]\.') { $fail += 'hub spawn not at PlayerStart_Hub' }
Capture 'hub-spawn'

Console 'AirlockWarp -4700 1560 116'
Console 'AirlockBook 0'
Start-Sleep -Seconds 2
Console 'AirlockWarp -4265 1560 116'
if (-not (WaitLog 'Section Hauler_Mk1' 45)) { $fail += 'no arrival on Hauler_Mk1 within 45 s' }
Start-Sleep -Seconds 8
$w1 = Where
if ($w1 -notmatch 'map=UEDPIE_\d+_Hauler_Mk1' -or $w1 -notmatch 'pawn=\S+ at X=-66[5-7][0-9]\.\d+ Y=15[5-6][0-9]\.') { $fail += 'ship arrival not at PlayerStart_Ship' }
Capture 'ship-arrival'

Console 'AirlockWarp -7015 1560 116'
if (-not (WaitLog 'travelling to /Game/Maps/StationHub' 45)) { $fail += 'return travel not triggered within 45 s' }
Start-Sleep -Seconds 12
$w2 = Where
if ($w2 -notmatch 'map=UEDPIE_\d+_StationHub' -or $w2 -notmatch 'pawn=\S+ at X=-56[3-4][0-9]\.\d+ Y=15[5-6][0-9]\.') { $fail += 'hub return not at PlayerStart_Hub' }
Capture 'hub-return'
$done = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = 'LogAirlock'; pattern = 'complete'; maxEntries = 5 }
if (-not (FreshLine $done.text 'Voyage .* complete')) { $fail += 'no fresh "Voyage ... complete" in the log' }

$null = Mcp-Call $TS_App 'StopPIE' @{}
Start-Sleep -Seconds 2
$errs = Mcp-Call $TS_Logs 'GetLogEntries' @{ category = ''; pattern = 'Error:'; maxEntries = 5 }
if ($errs.text -match 'Error:') { Say "errors in the log during the run: $(Short $errs.text 400)" }

if ($fail.Count -eq 0) { Say 'LOOP OK: hub spawn -> booked -> Dock B -> Hauler_Mk1 PlayerStart -> exit -> hub PlayerStart, voyage complete'; exit 0 }
else { Say ('LOOP FAILED: ' + ($fail -join '; ')); exit 1 }
