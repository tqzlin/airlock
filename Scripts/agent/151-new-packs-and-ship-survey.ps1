# 151-new-packs-and-ship-survey.ps1 -- Tom added the Season 1 Starter Bundle (Command Center, Hallways, Props I,
# Materials & Decals) and three industrial packs (SD_Art Industrial Infrastructure, Construction_VOL2,
# Warehouse_Storage). (1) ignore them in git BEFORE anything else touches the index; (2) wait for the import to
# go quiet; (3) editor up, Nanite pass on the StarterBundle meshes (post-import step); (4) survey Hauler_Mk1
# (every actor: label, mesh, position) so the ship's rooms can be assigned to systems; (5) docs, commit, loop proof.
$ErrorActionPreference = 'Continue'
$Name = 'Airlock'
$Root = "C:\Users\tclin\Desktop\ai_game_studio\$Name"
$UE   = 'C:\Program Files\Epic Games\UE_5.8'
$Res  = 'C:\Users\tclin\Desktop\ai_game_studio\agent\results'
$Staged = 'C:\Users\tclin\Desktop\ai_game_studio\agent\staged\handoff10'
$utf8 = New-Object System.Text.UTF8Encoding $false
function Step($t) { "`n==================== $t  [$(Get-Date -Format 'HH:mm:ss')] ====================" }
function Short($t, $n) { $c = ($t -replace '\s+', ' '); $c.Substring(0, [math]::Min($n, $c.Length)) }

Step 'GITIGNORE FIRST (the new packs are multi-GB)'
$gi = Join-Path $Root '.gitignore'
$txt = Get-Content $gi -Raw
$added = @()
foreach ($f in '/Content/StarterBundle/', '/Content/SD_Art/', '/Content/Construction_VOL2/', '/Content/Warehouse_Storage/') {
    if ($txt -notmatch [regex]::Escape($f)) { $txt = $txt.TrimEnd() + "`r`n$f"; $added += $f }
}
if ($added.Count) { [IO.File]::WriteAllText($gi, $txt + "`r`n", $utf8); "  added: " + ($added -join ', ') } else { "  nothing to add" }
$untracked = @(git -C $Root status --short 2>&1 | Where-Object { $_ -match '^\?\?' })
"  untracked entries now: $($untracked.Count)"
$untracked | Select-Object -First 15 | ForEach-Object { "    $_" }
git -C $Root add .gitignore 2>&1 | Out-Null
git -C $Root commit -q -m "gitignore: StarterBundle, SD_Art, Construction_VOL2, Warehouse_Storage packs" 2>&1 | ForEach-Object { "  $_" }
"  status: " + ((git -C $Root status --short 2>&1) -join ' | ')

Step 'IMPORT QUIET?'
$deadline = (Get-Date).AddMinutes(12)
do {
    $newest = Get-ChildItem (Join-Path $Root 'Content') -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.FullName -match 'StarterBundle|SD_Art|Construction_VOL2|Warehouse_Storage' } | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    $age = if ($newest) { ((Get-Date) - $newest.LastWriteTime).TotalSeconds } else { 9999 }
    "  newest pack file: $($newest.Name)  age $([int]$age) s"
    if ($age -lt 120) { Start-Sleep -Seconds 30 }
} while ($age -lt 120 -and (Get-Date) -lt $deadline)
foreach ($d in 'StarterBundle', 'SD_Art', 'Construction_VOL2', 'Warehouse_Storage') {
    $p = Join-Path $Root "Content\$d"
    if (Test-Path $p) { $files = Get-ChildItem $p -Recurse -File; "  {0,-20} {1,6} files  {2,7:N0} MB" -f $d, $files.Count, (($files | Measure-Object Length -Sum).Sum / 1MB) }
}

Step 'EDITOR'
. 'C:\Users\tclin\Desktop\ai_game_studio\agent\lib\mcp.ps1'
$TS_Prog = 'editor_toolset.toolsets.programmatic.ProgrammaticToolset'
$ed = Get-Process -Name 'UnrealEditor' -ErrorAction SilentlyContinue
if (-not $ed) {
    Start-Process "$UE\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"$Root\$Name.uproject`"", "-ModelContextProtocolStartServer"
    "  editor was not running - launched at $(Get-Date -Format 'HH:mm:ss')"
    $deadline = (Get-Date).AddMinutes(8); $port = $false
    while ((Get-Date) -lt $deadline) { $tcp = New-Object Net.Sockets.TcpClient; try { $port = $tcp.BeginConnect('127.0.0.1', 8000, $null, $null).AsyncWaitHandle.WaitOne(1000) -and $tcp.Connected } catch { $port = $false } finally { $tcp.Close() }; if ($port) { break }; Start-Sleep -Seconds 5 }
    if (-not $port) { "  MCP port never came up"; exit 4 }
    Start-Sleep -Seconds 45
} else { "  editor pid $($ed.Id) running since $($ed.StartTime.ToString('HH:mm:ss'))" }
Mcp-Init
if ((Mcp-Call $TS_App 'IsPIERunning' @{}).text -match 'true') { "  PIE running - stopping"; $null = Mcp-Call $TS_App 'StopPIE' @{}; Start-Sleep -Seconds 4 }
$lvl = (Mcp-Call $TS_Scene 'get_current_level' @{}).text
"  level: $lvl"
$hubDirty = (Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = '/Game/Maps/StationHub' }).text
"  StationHub dirty: $hubDirty"
if ($hubDirty -match 'true') { "  saving StationHub (our map; editor session left it dirty)"; Mcp-Show 'save hub' (Mcp-Call $TS_Asset 'save_assets' @{ asset_paths = @('/Game/Maps/StationHub') }) }
if ($lvl -notmatch 'StationHub') {
    $cur = ($lvl -replace '.*"returnValue":"', '') -replace '".*', ''
    "  current level is $cur - checking dirty"
    $d = (Mcp-Call $TS_Asset 'is_dirty' @{ asset_path = $cur }).text
    if ($d -match 'true' -and $cur -match '^/Game/Maps/') { Mcp-Show 'save current' (Mcp-Call $TS_Asset 'save_assets' @{ asset_paths = @($cur) }) }
}

Step 'INVENTORY: static meshes per new folder'
$pyInv = @'
import json
FOLDERS = ["/Game/StarterBundle/ModularSci_Comm", "/Game/StarterBundle/ModularScifiProps", "/Game/StarterBundle/ModularScifiHallways", "/Game/StarterBundle/ModularSciFiMats", "/Game/StarterBundle/ModularSci_Int", "/Game/StarterBundle/UE4_Assets", "/Game/SD_Art", "/Game/Construction_VOL2", "/Game/Warehouse_Storage"]
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
def run():
    out = {}
    for f in FOLDERS:
        try:
            found = rv(call("editor_toolset.toolsets.asset.AssetTools.find_assets", {"folder_path": f, "name": "", "asset_type": {"refPath": "/Script/Engine.StaticMesh"}, "recursive": True})) or []
            if isinstance(found, str):
                try:
                    found = json.loads(found)
                except Exception:
                    found = [found]
            names = sorted(set(as_path(x).split("/")[-1].split(".")[0] for x in found))
            out[f] = {"n_meshes": len(names), "names": names}
        except Exception as e:
            out[f] = {"error": str(e)[:160]}
    return out
'@
$r = Mcp-Call $TS_Prog 'execute_tool_script' @{ script = $pyInv }
"  ok=$($r.ok)"
if ($r.ok) {
    [IO.File]::WriteAllText((Join-Path $Res '151-packs-meshes.json'), $r.text, $utf8)
    try { $inv = ($r.text | ConvertFrom-Json).returnValue; if ($inv -is [string]) { $inv = $inv | ConvertFrom-Json }
          foreach ($p in $inv.PSObject.Properties) { $v = $p.Value; if ($v.n_meshes -ne $null) { "  {0,-48} {1,4} meshes  e.g. {2}" -f $p.Name, $v.n_meshes, (($v.names | Select-Object -First 6) -join ', ') } else { "  {0,-48} ERR {1}" -f $p.Name, $v.error } } }
    catch { "  parse: $_" }
} else { "  " + (Short $r.text 600) }

Step 'NANITE: StarterBundle (post-import step; industrial packs on demand)'
$pyNan = @'
import json, re
FOLDER = "__FOLDER__"
LIMIT = __LIMIT__
SKIP = re.compile(r"glass|window|transl|holo|decal|fog|volum|_UI$|_UI_|screen|skysphere|zsphere|backdrop|lightbeam|light_beam|godray", re.I)
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
    paths = [p for p in paths if "/ModularSci_Int/" not in p]
    res = {"folder": FOLDER, "found": len(paths), "already": 0, "skipped": [], "enabled": [], "errors": [], "remaining": 0}
    todo = []
    for p in paths:
        name = p.split("/")[-1].split(".")[0]
        if SKIP.search(name):
            res["skipped"].append(name); continue
        try:
            r = rv(call("editor_toolset.toolsets.static_mesh.StaticMeshTools.is_nanite_enabled", {"mesh": mesh_ref(p)}))
        except Exception as e:
            res["errors"].append(name + ": " + str(e)[:60]); continue
        if r is True or r == "true" or r == "True":
            res["already"] += 1; continue
        todo.append(p)
    res["remaining"] = max(0, len(todo) - LIMIT)
    saved = []
    for p in todo[:LIMIT]:
        name = p.split("/")[-1].split(".")[0]
        try:
            slots = rv(call("editor_toolset.toolsets.static_mesh.StaticMeshTools.get_material_slots", {"mesh": mesh_ref(p)}))
            if SKIP.search(json.dumps(slots) if not isinstance(slots, str) else slots):
                res["skipped"].append(name + "(mat)"); continue
            call("editor_toolset.toolsets.static_mesh.StaticMeshTools.set_nanite_enabled", {"mesh": mesh_ref(p), "enabled": True})
            res["enabled"].append(name)
            saved.append(mesh_ref(p)["refPath"].split(".")[0])
        except Exception as e:
            res["errors"].append(name + ": " + str(e)[:80])
    if saved:
        try:
            call("editor_toolset.toolsets.asset.AssetTools.save_assets", {"asset_paths": saved})
        except Exception as e:
            res["errors"].append("save: " + str(e)[:120])
    res["enabled_count"] = len(res["enabled"]); res["enabled"] = res["enabled"][:10]
    res["skipped_count"] = len(res["skipped"]); res["skipped"] = res["skipped"][:30]
    res["errors"] = res["errors"][:10]
    return res
'@
$total = 0
foreach ($folder in '/Game/StarterBundle') {
    $pass = 0
    do {
        $pass++
        $script = $pyNan.Replace('__FOLDER__', $folder).Replace('__LIMIT__', '50')
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $r = Mcp-Call $TS_Prog 'execute_tool_script' @{ script = $script }
        "  pass $pass ok=$($r.ok) $([int]$sw.Elapsed.TotalSeconds)s " + (Short $r.text 700)
        $remaining = 0; $enabled = 0
        try { $inner = ($r.text | ConvertFrom-Json).returnValue; if ($inner -is [string]) { $inner = $inner | ConvertFrom-Json }; $remaining = [int]$inner.remaining; $enabled = [int]$inner.enabled_count } catch { "  parse failed: $_"; $remaining = 0 }
        $total += $enabled
    } while ($remaining -gt 0 -and $pass -lt 20 -and $r.ok)
}
"  StarterBundle meshes switched to Nanite this run: $total"

Step 'SURVEY: Hauler_Mk1 (label, mesh, position of every actor)'
Mcp-Show 'load ship' (Mcp-Call $TS_Scene 'load_level' @{ level_path = '/Game/Maps/Ships/Hauler_Mk1' })
Start-Sleep -Seconds 8
"  level: " + (Mcp-Call $TS_Scene 'get_current_level' @{}).text
$pySurvey = @'
import json, re
START = __START__
COUNT = __COUNT__
def call(tool, args):
    return execute_tool(tool, json.dumps(args))
def rv(r):
    return r["returnValue"] if isinstance(r, dict) and "returnValue" in r else r
def ref(a):
    return a if isinstance(a, dict) and "refPath" in a else {"refPath": str(a)}
def run():
    allactors = rv(call("editor_toolset.toolsets.scene.SceneTools.find_actors", {"name": "", "tag": "", "collision_channels": []})) or []
    if isinstance(allactors, str):
        allactors = json.loads(allactors)
    out = {"total": len(allactors), "start": START, "items": []}
    for a in allactors[START:START + COUNT]:
        a = ref(a); rp = a["refPath"]; name = rp.split(".")[-1].split(":")[-1]
        item = {"n": name}
        try:
            item["l"] = str(rv(call("editor_toolset.toolsets.actor.ActorTools.get_label", {"actor": a})))
        except Exception:
            item["l"] = "?"
        try:
            x = rv(call("editor_toolset.toolsets.actor.ActorTools.get_actor_transform", {"actor": a}))
            if isinstance(x, str):
                x = json.loads(x)
            item["p"] = [round(x["location"]["x"]), round(x["location"]["y"]), round(x["location"]["z"])]
            item["yaw"] = round(x["rotation"]["yaw"])
        except Exception as e:
            item["e"] = str(e)[:60]
        if not (item["l"].startswith("SM_") or name.startswith("SM_")):
            try:
                comps = rv(call("editor_toolset.toolsets.actor.ActorTools.get_components", {"actor": a, "component_type": {"refPath": "/Script/Engine.StaticMeshComponent"}})) or []
                if comps:
                    m = rv(call("editor_toolset.toolsets.object.ObjectTools.get_properties", {"instance": ref(comps[0]), "properties": ["StaticMesh"]}))
                    ms = json.dumps(m)
                    mm = re.search(r"(SM_[A-Za-z0-9_]+)\.", ms)
                    item["m"] = mm.group(1) if mm else ms[:60]
            except Exception as e:
                item["me"] = str(e)[:60]
        out["items"].append(item)
    return out
'@
$items = New-Object System.Collections.ArrayList
$start = 0; $totalActors = -1; $chunk = 350
do {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $r = Mcp-Call $TS_Prog 'execute_tool_script' @{ script = $pySurvey.Replace('__START__', "$start").Replace('__COUNT__', "$chunk") }
    if (-not $r.ok) { "  chunk at $start FAILED: " + (Short $r.text 300); break }
    $inner = ($r.text | ConvertFrom-Json).returnValue; if ($inner -is [string]) { $inner = $inner | ConvertFrom-Json }
    $totalActors = [int]$inner.total
    foreach ($it in $inner.items) { [void]$items.Add($it) }
    "  chunk $start..$($start + $chunk): got $($inner.items.Count) (total $totalActors) in $([int]$sw.Elapsed.TotalSeconds)s"
    $start += $chunk
} while ($start -lt $totalActors)
$json = ConvertTo-Json -InputObject @{ total = $totalActors; items = $items } -Depth 6 -Compress
[IO.File]::WriteAllText((Join-Path $Res '151-hauler-survey.json'), $json, $utf8)
"  survey written: $($items.Count) actors, $([int]($json.Length / 1024)) KB"
$byMesh = @{}
foreach ($it in $items) { $k = if ($it.m) { $it.m } else { (($it.l -replace '\d+$', '') -replace '_$', '') }; if (-not $byMesh.ContainsKey($k)) { $byMesh[$k] = 0 }; $byMesh[$k]++ }
"  top kinds:"
$byMesh.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 25 | ForEach-Object { "    {0,5}  {1}" -f $_.Value, $_.Key }
Mcp-Show 'load hub' (Mcp-Call $TS_Scene 'load_level' @{ level_path = '/Game/Maps/StationHub' })
Start-Sleep -Seconds 6
"  level: " + (Mcp-Call $TS_Scene 'get_current_level' @{}).text

Step 'DOCS + COMMIT'
foreach ($k in 'THIRD_PARTY_ASSETS.md', 'Docs\tasks\M3-2-ship-graph.md') {
    $src = Join-Path $Staged $k; $dst = Join-Path $Root $k
    if (Test-Path $src) { New-Item -ItemType Directory -Force -Path (Split-Path $dst -Parent) | Out-Null; Copy-Item $src $dst -Force; "  {0,-34} {1,7} B" -f $k, (Get-Item $dst).Length } else { "  MISSING $src" }
}
Copy-Item 'C:\Users\tclin\Desktop\ai_game_studio\agent\queue\151-new-packs-and-ship-survey.ps1' (Join-Path $Root 'Scripts\agent\151-new-packs-and-ship-survey.ps1') -Force -ErrorAction SilentlyContinue
if (-not (Test-Path (Join-Path $Root 'Scripts\agent\151-new-packs-and-ship-survey.ps1'))) { Copy-Item 'C:\Users\tclin\Desktop\ai_game_studio\agent\done\151-new-packs-and-ship-survey.ps1' (Join-Path $Root 'Scripts\agent\151-new-packs-and-ship-survey.ps1') -Force -ErrorAction SilentlyContinue }
git -C $Root add THIRD_PARTY_ASSETS.md Docs/tasks/M3-2-ship-graph.md Scripts/agent 2>&1 | Out-Null
git -C $Root commit -q -m "Assets: Season 1 bundle (Command Center, Props I, Hallways, Materials) and three industrial packs inventoried, ignored, Nanite on the bundle; task brief M3-2 ship systems graph" 2>&1 | ForEach-Object { "  $_" }
if ((git -C $Root status --short 2>&1) -match 'umap') { git -C $Root add Content/Maps 2>&1 | Out-Null; git -C $Root commit -q -m "Maps: editor re-save" 2>&1 | ForEach-Object { "  $_" } }
git -C $Root log --oneline -3 2>&1 | ForEach-Object { "  $_" }
"  status: " + ((git -C $Root status --short 2>&1) -join ' | ')

Step 'HEALTH: Scripts\verify-loop.ps1'
$sw = [Diagnostics.Stopwatch]::StartNew()
$vl = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $Root 'Scripts\verify-loop.ps1') 2>&1
$vlRc = $LASTEXITCODE
$vl | Select-Object -Last 12 | ForEach-Object { "  $_" }
"  ---- verify-loop exit=$vlRc  elapsed=$([int]$sw.Elapsed.TotalSeconds)s ----"
Mcp-Show 'errors' (Mcp-Call $TS_Logs 'GetLogEntries' @{ category = ''; pattern = 'Error:'; maxEntries = 6 })
"done $(Get-Date -Format 'HH:mm:ss')"
