# Scripts\close-editor.ps1 -- close the editor that has THIS checkout's project open, and only that one, politely:
# WM_CLOSE, wait (default 90 s), then kill (a modal dialog). Editors of other checkouts (a worktree running side by
# side, HANDOFF section 7) are left alone. Exit 0 when no editor of this checkout is running.
#   powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\close-editor.ps1 [-WaitSeconds 90]
param([int]$WaitSeconds = 90)
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.TrimEnd('\')
$mine = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" | Where-Object { $_.CommandLine -and $_.CommandLine.IndexOf("$root\", [StringComparison]::OrdinalIgnoreCase) -ge 0 })
if ($mine.Count -eq 0) { "close-editor: no editor is running on $root"; exit 0 }
foreach ($p in $mine) {
    $proc = Get-Process -Id $p.ProcessId -ErrorAction SilentlyContinue
    if (-not $proc) { continue }
    "close-editor: pid $($proc.Id) ($root) - sending WM_CLOSE"
    $null = $proc.CloseMainWindow()
    $deadline = (Get-Date).AddSeconds($WaitSeconds)
    while ((Get-Date) -lt $deadline -and -not $proc.HasExited) { Start-Sleep -Seconds 2 }
    if (-not $proc.HasExited) { "close-editor: still running after $WaitSeconds s (a dialog?) - killing"; Stop-Process -Id $proc.Id -Force; Start-Sleep -Seconds 3 }
    else { "close-editor: closed at $(Get-Date -Format 'HH:mm:ss')" }
}
exit 0
