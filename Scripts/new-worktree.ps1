# Scripts\new-worktree.ps1 -- give an agent its own checkout for parallel work (HANDOFF section 7).
#   powershell -NoProfile -ExecutionPolicy Bypass -File Scripts\new-worktree.ps1 -Name Airlock-claude -Branch claude/level-pass [-From main] [-Build]
# Creates ..\<Name> as a git worktree on a new branch, junctions every git-ignored pack folder from this checkout
# (packs are never copied or committed), and optionally builds its editor binaries. Each worktree has its own
# Binaries, Intermediate and Saved (file console, captures, logs) and runs its own editor on its own MCP port
# (set AIRLOCK_MCP_PORT before Scripts\run-editor.bat and the proof scripts). The engine's DDC is shared and safe.
param([Parameter(Mandatory = $true)][string]$Name, [Parameter(Mandatory = $true)][string]$Branch, [string]$From = 'main', [switch]$Build)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path.TrimEnd('\')
$target = Join-Path (Split-Path $root -Parent) $Name
if (-not (Test-Path $target)) {
    git -C $root worktree add $target -b $Branch $From
    if ($LASTEXITCODE -ne 0) { throw "git worktree add failed ($LASTEXITCODE)" }
} else { "new-worktree: folder exists, reusing: $target" }
$packs = Get-Content (Join-Path $root '.gitignore') | ForEach-Object { if ($_ -match '^/Content/([^/]+)/$') { $matches[1] } }
foreach ($p in $packs) {
    $src = Join-Path $root "Content\$p"; $dst = Join-Path $target "Content\$p"
    if (-not (Test-Path $src)) { "  pack not present here, skipped: Content\$p"; continue }
    if (Test-Path $dst) { "  exists: Content\$p"; continue }
    New-Item -ItemType Junction -Path $dst -Target $src | Out-Null
    "  junction: Content\$p -> $src"
}
if ($Build) {
    & cmd /c "`"$target\Scripts\build-editor.bat`""
    if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }
}
"new-worktree: ready at $target on $Branch"
git -C $root worktree list
