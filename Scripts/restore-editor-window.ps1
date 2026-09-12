# Scripts\restore-editor-window.ps1 -- un-minimize the Unreal Editor main window. Slate lists no windows, captures
# fail ("Failed to capture any editor windows") and console typing does nothing while the editor is minimized.
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class EdWin {
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
"@
$done = $false
foreach ($p in Get-Process -Name 'UnrealEditor' -ErrorAction SilentlyContinue) {
    if ($p.MainWindowHandle -ne 0) {
        $was = [EdWin]::IsIconic($p.MainWindowHandle)
        if ($was) { [void][EdWin]::ShowWindow($p.MainWindowHandle, 9) }   # SW_RESTORE
        [void][EdWin]::SetForegroundWindow($p.MainWindowHandle)
        Write-Host "editor pid $($p.Id) '$($p.MainWindowTitle)': was minimized=$was, now minimized=$([EdWin]::IsIconic($p.MainWindowHandle))"
        $done = $true
    }
}
if (-not $done) { Write-Host 'no Unreal Editor window found'; exit 1 }
