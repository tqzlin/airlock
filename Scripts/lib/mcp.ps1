# Scripts\lib\mcp.ps1 -- dot-source this (PowerShell 5.1). Minimal Streamable-HTTP MCP client for the Unreal MCP server
# that the editor starts on 127.0.0.1:8000. Repo copy of the agent runner's library; captures land in Saved\AgentCaptures.
#   Mcp-Init                              -> initialize + initialized notification (sets session id)
#   Mcp-Call  <toolset> <tool> <hashtable> -> returns [pscustomobject]{ ok, text, json, images, raw }
#   Mcp-Text  <toolset> <tool> <hashtable> -> returns text (throws on error)
$script:McpBase = 'http://127.0.0.1:8000/mcp'
$script:McpHdr  = @{ Accept = 'application/json, text/event-stream' }
$script:McpId   = 1
$script:McpImgDir = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'Saved\AgentCaptures'

function Mcp-Rpc([string]$method, $params) {
    $id = $script:McpId++
    $obj = @{ jsonrpc = '2.0'; id = $id; method = $method }
    if ($null -ne $params) { $obj.params = $params }
    $body = $obj | ConvertTo-Json -Depth 20 -Compress
    $r = Invoke-WebRequest -Uri $script:McpBase -Method Post -ContentType 'application/json' -Headers $script:McpHdr -Body $body -TimeoutSec 900 -UseBasicParsing
    if ($r.Headers['Mcp-Session-Id']) { $script:McpHdr['Mcp-Session-Id'] = $r.Headers['Mcp-Session-Id'] }
    $txt = $r.Content
    if ($txt -match '(?m)^\s*(event|data):') { $txt = ($txt -split "`n" | Where-Object { $_ -like 'data:*' } | ForEach-Object { $_.Substring(5).Trim() }) -join "`n" }
    return $txt
}
function Mcp-Init {
    $null = Mcp-Rpc 'initialize' @{ protocolVersion = '2025-06-18'; capabilities = @{}; clientInfo = @{ name = 'airlock-runner'; version = '0.1' } }
    try { $null = Invoke-WebRequest -Uri $script:McpBase -Method Post -ContentType 'application/json' -Headers $script:McpHdr -Body '{"jsonrpc":"2.0","method":"notifications/initialized"}' -TimeoutSec 10 -UseBasicParsing } catch {}
}
function Mcp-Call([string]$toolset, [string]$tool, $arguments) {
    if ($null -eq $arguments) { $arguments = @{} }
    $callArgs = @{ tool_name = $tool; arguments = $arguments }
    if ($toolset) { $callArgs.toolset_name = $toolset }
    $raw = Mcp-Rpc 'tools/call' @{ name = 'call_tool'; arguments = $callArgs }
    $res = [pscustomobject]@{ ok = $false; text = ''; json = $null; images = @(); raw = $raw }
    try { $j = $raw | ConvertFrom-Json } catch { $res.text = "non-JSON response: " + $raw.Substring(0, [math]::Min(400, $raw.Length)); return $res }
    if ($j.error) { $res.text = "RPC error: " + ($j.error | ConvertTo-Json -Compress -Depth 5); return $res }
    $parts = @()
    foreach ($c in $j.result.content) {
        if ($c.type -eq 'text') { $parts += $c.text }
        elseif ($c.type -eq 'image' -and $c.data) {
            New-Item -ItemType Directory -Path $script:McpImgDir -Force | Out-Null
            $ext = if ($c.mimeType -match 'jpe?g') { 'jpg' } else { 'png' }
            $f = Join-Path $script:McpImgDir ("{0}-{1}.{2}" -f (Get-Date -Format 'HHmmss'), $tool, $ext)
            [IO.File]::WriteAllBytes($f, [Convert]::FromBase64String($c.data))
            $res.images += $f
        }
    }
    $res.text = ($parts -join "`n")
    $res.ok = -not [bool]$j.result.isError
    if ($j.result.structuredContent) { $res.json = $j.result.structuredContent }
    else { try { if ($res.text.Trim().StartsWith('{') -or $res.text.Trim().StartsWith('[')) { $res.json = $res.text | ConvertFrom-Json } } catch {} }
    # images returned inside returnValue (CaptureEditorImage / CaptureViewport). PS 5.1's ConvertFrom-Json chokes on
    # >2 MB strings, so pull the base64 out with a regex instead of relying on $res.json.
    try {
        if ($res.text -match '"mimeType"\s*:\s*"(image/[a-z]+)"' ) {
            $mime = $matches[1]
            if ($res.text -match '"data"\s*:\s*"([A-Za-z0-9+/=]{100,})"') {
                New-Item -ItemType Directory -Path $script:McpImgDir -Force | Out-Null
                $ext = if ($mime -match 'jpe?g') { 'jpg' } else { 'png' }
                $f = Join-Path $script:McpImgDir ("{0}-{1}.{2}" -f (Get-Date -Format 'HHmmss'), $tool, $ext)
                [IO.File]::WriteAllBytes($f, [Convert]::FromBase64String($matches[1]))
                $res.images += $f
                $res.text = "{image saved: $f, $([int]((Get-Item $f).Length/1KB)) KB}"
                $res.json = $null
            }
        }
    } catch { $res.text = "{image decode failed: $($_.Exception.Message)}" }
    return $res
}
function Mcp-Text([string]$toolset, [string]$tool, $arguments) {
    $r = Mcp-Call $toolset $tool $arguments
    if (-not $r.ok) { throw "$tool failed: $($r.text)" }
    return $r.text
}
function Mcp-Show([string]$label, $r) {
    $t = if ($r.text) { $r.text } else { '' }
    "  [$label] ok=$($r.ok) " + ($t -replace '\s+', ' ').Substring(0, [math]::Min(700, ($t -replace '\s+', ' ').Length)) + $(if ($r.images) { "  images: " + ($r.images -join ', ') } else { '' })
}
# toolset name constants
$TS_Scene   = 'editor_toolset.toolsets.scene.SceneTools'
$TS_Actor   = 'editor_toolset.toolsets.actor.ActorTools'
$TS_Prim    = 'editor_toolset.toolsets.primitive.PrimitiveTools'
$TS_Asset   = 'editor_toolset.toolsets.asset.AssetTools'
$TS_Object  = 'editor_toolset.toolsets.object.ObjectTools'
$TS_BP      = 'editor_toolset.toolsets.blueprint.BlueprintTools'
$TS_App     = 'EditorToolset.EditorAppToolset'
$TS_Logs    = 'EditorToolset.LogsToolset'
$TS_Tests   = 'AutomationTestToolset.AutomationTestToolset'
$TS_Config  = 'ConfigSettingsToolset.ConfigSettingsToolset'
$TS_Slate   = 'SlateInspectorToolset.SlateInspectorToolset'
