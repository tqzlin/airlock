# 135-enable-kit-nanite.ps1 -- post-import step for the Modular SciFi packs: enable Nanite on every opaque kit
# static mesh so Virtual Shadow Maps stop overflowing the non-Nanite marking queue (warning Tom saw in the editor).
# Skips meshes whose name or material names suggest translucency (glass/window/holo/decal) since Nanite cannot
# render those. Idempotent: is_nanite_enabled is checked first, so re-running only touches what is left.
# Documented in Airlock/THIRD_PARTY_ASSETS.md ("after import").
$ErrorActionPreference = 'Continue'
. 'C:\Users\tclin\Desktop\ai_game_studio\agent\lib\mcp.ps1'
$TS_Prog = 'editor_toolset.toolsets.programmatic.ProgrammaticToolset'
$Root = 'C:\Users\tclin\Desktop\ai_game_studio\Airlock'
function Step($t) { "`n==================== $t  [$(Get-Date -Format 'HH:mm:ss')] ====================" }
function Compact($t) { ($t -replace '\s+', ' ') }
function Short($t, $n) { $c = Compact $t; $c.Substring(0, [math]::Min($n, $c.Length)) }
Mcp-Init

$py = @'
import json, re
FOLDER = "__FOLDER__"
LIMIT = __LIMIT__
SKIP = re.compile(r"glass|window|transl|holo|decal|fog|beam|volum", re.I)
def call(tool, args):
    return execute_tool(tool, json.dumps(args))
def rv(r):
    return r["returnValue"] if isinstance(r, dict) and "returnValue" in r else r
def as_path(x):
    if isinstance(x, dict):
        for k in ("refPath", "path", "asset_path", "object_path", "package_name"):
            if k in x:
                return str(x[k])
        return json.dumps(x)[:120]
    return str(x)
def mesh_ref(p):
    # accept "/Game/A/SM_X", "/Game/A/SM_X.SM_X" or "StaticMesh /Game/A/SM_X.SM_X"
    p = p.split(" ")[-1].strip("'")
    if "." not in p.split("/")[-1]:
        p = p + "." + p.split("/")[-1]
    return {"refPath": p}
def run():
    found = rv(call("editor_toolset.toolsets.asset.AssetTools.find_assets", {"folder_path": FOLDER, "name": "", "asset_type": {"refPath": "/Script/Engine.StaticMesh"}, "recursive": True})) or []
    if isinstance(found, str):
        try:
            found = json.loads(found)
        except Exception:
            found = [found]
    paths = [as_path(x) for x in found]
    res = {"folder": FOLDER, "found": len(paths), "sample": paths[:3], "already": 0, "skipped": [], "enabled": [], "remaining": 0}
    todo = []
    for p in paths:
        name = p.split("/")[-1].split(".")[0]
        if SKIP.search(name):
            res["skipped"].append(name); continue
        r = rv(call("editor_toolset.toolsets.static_mesh.StaticMeshTools.is_nanite_enabled", {"mesh": mesh_ref(p)}))
        if r is True or r == "true" or r == "True":
            res["already"] += 1; continue
        todo.append(p)
    res["remaining"] = max(0, len(todo) - LIMIT)
    saved = []
    for p in todo[:LIMIT]:
        name = p.split("/")[-1].split(".")[0]
        slots = rv(call("editor_toolset.toolsets.static_mesh.StaticMeshTools.get_material_slots", {"mesh": mesh_ref(p)}))
        if SKIP.search(json.dumps(slots) if not isinstance(slots, str) else slots):
            res["skipped"].append(name + "(mat)"); continue
        call("editor_toolset.toolsets.static_mesh.StaticMeshTools.set_nanite_enabled", {"mesh": mesh_ref(p), "enabled": True})
        res["enabled"].append(name)
        saved.append(mesh_ref(p)["refPath"].split(".")[0])
    if saved:
        call("editor_toolset.toolsets.asset.AssetTools.save_assets", {"asset_paths": saved})
    res["enabled_count"] = len(res["enabled"]); res["enabled"] = res["enabled"][:12]
    res["skipped_count"] = len(res["skipped"]); res["skipped"] = res["skipped"][:40]
    return res
'@

# the staticmesh toolset id: confirm from the registered list once (module path may differ from the class name)
Step 'TOOLSET CHECK'
$smTool = 'editor_toolset.toolsets.static_mesh.StaticMeshTools'
try {
    $env = Mcp-Call $TS_Prog 'get_execution_environment' @{}
    "  env: " + (Short $env.text 300)
} catch { "  env probe failed: $_" }

$total = 0
foreach ($folder in '/Game/ModSci_Engineer', '/Game/ModSci_EngiProps', '/Game/ModSciInteriors') {
    Step "NANITE: $folder"
    $pass = 0
    do {
        $pass++
        $script = $py.Replace('__FOLDER__', $folder).Replace('__LIMIT__', '60')
        $r = Mcp-Call $TS_Prog 'execute_tool_script' @{ script = $script }
        "  pass $pass ok=$($r.ok) " + (Short $r.text 900)
        $remaining = 0; $enabled = 0
        try {
            $inner = ($r.text | ConvertFrom-Json).returnValue; if ($inner -is [string]) { $inner = $inner | ConvertFrom-Json }
            $remaining = [int]$inner.remaining; $enabled = [int]$inner.enabled_count
        } catch { "  parse failed: $_"; $remaining = 0 }
        $total += $enabled
    } while ($remaining -gt 0 -and $pass -lt 20 -and $r.ok)
}
"  total meshes switched to Nanite this run: $total"

Step 'DOCS'
$doc = Join-Path $Root 'THIRD_PARTY_ASSETS.md'
$txt = Get-Content $doc -Raw
if ($txt -notmatch 'Post-import step') {
    $txt = $txt.TrimEnd() + "`r`n`r`n## Post-import step: Nanite`r`n`r`nAfter importing the Modular SciFi packs, enable Nanite on every opaque kit static mesh (skip glass/window/holo/decal meshes). Without it the demo-hall-sized levels overflow the Virtual Shadow Map non-Nanite marking queue. The agent script ``agent/queue/135-enable-kit-nanite.ps1`` (copy in ``Scripts/agent/``) does this through Unreal MCP: ``find_assets`` (StaticMesh, recursive) -> ``is_nanite_enabled`` -> ``set_nanite_enabled`` -> ``save_assets``, 60 meshes per call, idempotent.`r`n"
    [IO.File]::WriteAllText($doc, $txt, (New-Object System.Text.UTF8Encoding $false))
    "  THIRD_PARTY_ASSETS.md: post-import section added"
} else { "  THIRD_PARTY_ASSETS.md: section already present" }
New-Item -ItemType Directory -Force -Path (Join-Path $Root 'Scripts\agent') | Out-Null
Copy-Item 'C:\Users\tclin\Desktop\ai_game_studio\agent\done\135-enable-kit-nanite.ps1' (Join-Path $Root 'Scripts\agent\135-enable-kit-nanite.ps1') -ErrorAction SilentlyContinue
if (-not (Test-Path (Join-Path $Root 'Scripts\agent\135-enable-kit-nanite.ps1'))) { Copy-Item 'C:\Users\tclin\Desktop\ai_game_studio\agent\queue\135-enable-kit-nanite.ps1' (Join-Path $Root 'Scripts\agent\135-enable-kit-nanite.ps1') -ErrorAction SilentlyContinue }
git -C $Root add THIRD_PARTY_ASSETS.md Scripts/agent 2>&1 | Out-Null
git -C $Root commit -q -m "Docs: Nanite post-import step for the kit packs (script kept under Scripts/agent)" 2>&1 | ForEach-Object { "  $_" }
git -C $Root log --oneline -2 2>&1 | ForEach-Object { "  $_" }
Mcp-Show 'errors' (Mcp-Call $TS_Logs 'GetLogEntries' @{ category = ''; pattern = 'Error:'; maxEntries = 6 })
"done $(Get-Date -Format 'HH:mm:ss')"
