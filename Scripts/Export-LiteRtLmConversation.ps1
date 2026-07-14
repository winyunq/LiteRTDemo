param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

function Encode-Html([AllowNull()][object]$Value) {
    if ($null -eq $Value) {
        return ''
    }
    return [System.Net.WebUtility]::HtmlEncode([string]$Value)
}

function Get-MessageText([AllowNull()][object]$Content) {
    if ($null -eq $Content) {
        return ''
    }
    if ($Content -is [string]) {
        return $Content
    }

    $parts = [System.Collections.Generic.List[string]]::new()
    foreach ($part in @($Content)) {
        if ($null -ne $part.text) {
            $parts.Add([string]$part.text)
        }
        else {
            $parts.Add(($part | ConvertTo-Json -Depth 20 -Compress))
        }
    }
    return ($parts -join "`n")
}

$resolvedInput = (Resolve-Path -LiteralPath $InputPath).Path
$agents = @{}

$inputStream = [System.IO.FileStream]::new(
    $resolvedInput,
    [System.IO.FileMode]::Open,
    [System.IO.FileAccess]::Read,
    ([System.IO.FileShare]::ReadWrite -bor [System.IO.FileShare]::Delete)
)
$reader = [System.IO.StreamReader]::new($inputStream, [System.Text.Encoding]::UTF8, $true)
try {
while (-not $reader.EndOfStream) {
    $line = $reader.ReadLine()
    if ([string]::IsNullOrWhiteSpace($line)) {
        continue
    }

    try {
        $record = $line | ConvertFrom-Json
    }
    catch {
        continue
    }

    if ([string]::IsNullOrWhiteSpace([string]$record.agent_id)) {
        continue
    }

    $agentId = [string]$record.agent_id
    if (-not $agents.ContainsKey($agentId)) {
        $agents[$agentId] = [ordered]@{
            Id = $agentId
            Name = [string]$record.agent_name
            Starts = 0
            Completes = 0
            Successes = 0
            Failures = 0
            MaxContextTokens = 0
            MaxMemoryBytes = 0
            LatestMemoryUtc = ''
            LatestMemoryJson = ''
            FirstError = ''
            LastError = ''
            TimeoutError = ''
        }
    }

    $agent = $agents[$agentId]
    if ($record.event -eq 'request_start') {
        $agent.Starts++
        $memoryBytes = [int64]$record.memory_bytes_utf16
        if ($memoryBytes -ge $agent.MaxMemoryBytes -and -not [string]::IsNullOrWhiteSpace([string]$record.memory_json)) {
            $agent.MaxMemoryBytes = $memoryBytes
            $agent.LatestMemoryUtc = [string]$record.utc
            $agent.LatestMemoryJson = [string]$record.memory_json
        }
    }
    elseif ($record.event -eq 'request_complete') {
        $agent.Completes++
        if ($record.success -eq $true) {
            $agent.Successes++
        }
        else {
            $agent.Failures++
            $errorText = '{0} | {1} | {2}' -f $record.utc, $record.note, $record.error_message
            if ([string]::IsNullOrWhiteSpace($agent.FirstError)) {
                $agent.FirstError = $errorText
            }
            $agent.LastError = $errorText
            if ([string]$record.error_message -match 'Timed out') {
                $agent.TimeoutError = $errorText
            }
        }
        $contextTokens = [int64]$record.context_tokens
        if ($contextTokens -gt $agent.MaxContextTokens) {
            $agent.MaxContextTokens = $contextTokens
        }
    }
}
}
finally {
    $reader.Dispose()
    $inputStream.Dispose()
}

$builder = [System.Text.StringBuilder]::new()
[void]$builder.AppendLine('<!doctype html>')
[void]$builder.AppendLine('<html lang="zh-CN"><head><meta charset="utf-8">')
[void]$builder.AppendLine('<meta name="viewport" content="width=device-width,initial-scale=1">')
[void]$builder.AppendLine('<title>LiteRT-LM Agent 对话记录</title>')
[void]$builder.AppendLine(@'
<style>
body{margin:0;background:#101827;color:#e5edf7;font:15px/1.55 "Segoe UI","Microsoft YaHei",sans-serif}
main{max-width:1280px;margin:auto;padding:24px}.top{position:sticky;top:0;z-index:3;background:#101827ee;padding:10px 0 14px}
h1,h2{margin:.2em 0}.meta{color:#9db0c7}.summary{display:grid;grid-template-columns:repeat(auto-fit,minmax(170px,1fr));gap:10px;margin:16px 0}
.stat,.message{border:1px solid #31415a;border-radius:8px;background:#192438}.stat{padding:10px}.message{margin:10px 0;overflow:hidden}
.head{display:flex;gap:12px;align-items:center;padding:7px 11px;background:#24344c;font-weight:600}.index{color:#8aa4c5}.role{color:#71e4d3}
.body{white-space:pre-wrap;word-break:break-word;padding:11px 14px}.system{border-color:#7b6cb7}.user{border-color:#4b8cb8}.assistant{border-color:#5b9c72}.tool{border-color:#c39a4e}
.toolcall{margin:8px 14px 12px;padding:9px;background:#0e1725;border-left:3px solid #e2b45b;white-space:pre-wrap;word-break:break-word}
.error{color:#ff9f9f}.warning{padding:12px;border:1px solid #b07c3b;background:#332718;border-radius:8px}.agent{margin:28px 0 48px}
details{margin:12px 0}summary{cursor:pointer;color:#8dded0}code{color:#f4cf7d}
</style></head><body><main>
'@)
[void]$builder.AppendLine('<div class="top"><h1>LiteRT-LM Agent 对话记录</h1>')
[void]$builder.AppendLine(('<div class="meta">源日志：{0}</div></div>' -f (Encode-Html $resolvedInput)))
[void]$builder.AppendLine('<div class="warning">这是开发诊断记录，包含角色私密信息、模型实际输入和工具参数；不要作为玩家公屏内容发布。</div>')

$activeAgents = @($agents.Values | Where-Object { $_.Starts -gt 0 } | Sort-Object Starts -Descending)
foreach ($agent in $activeAgents) {
    [void]$builder.AppendLine('<section class="agent">')
    [void]$builder.AppendLine(('<h2>{0}</h2><div class="meta">Agent ID: <code>{1}</code> · 最新记忆快照: {2}</div>' -f (Encode-Html $agent.Name), (Encode-Html $agent.Id), (Encode-Html $agent.LatestMemoryUtc)))
    [void]$builder.AppendLine('<div class="summary">')
    foreach ($entry in @(
        @('实际开始请求', $agent.Starts),
        @('完成事件', $agent.Completes),
        @('成功请求', $agent.Successes),
        @('失败事件', $agent.Failures),
        @('最大上下文 token', $agent.MaxContextTokens),
        @('最大记忆 UTF-16 bytes', $agent.MaxMemoryBytes)
    )) {
        [void]$builder.AppendLine(('<div class="stat"><div class="meta">{0}</div><strong>{1}</strong></div>' -f (Encode-Html $entry[0]), (Encode-Html $entry[1])))
    }
    [void]$builder.AppendLine('</div>')

    if (-not [string]::IsNullOrWhiteSpace($agent.TimeoutError)) {
        [void]$builder.AppendLine(('<div class="warning error"><strong>超时：</strong> {0}</div>' -f (Encode-Html $agent.TimeoutError)))
    }
    if (-not [string]::IsNullOrWhiteSpace($agent.LastError)) {
        [void]$builder.AppendLine(('<details><summary>最后一个错误</summary><div class="body error">{0}</div></details>' -f (Encode-Html $agent.LastError)))
    }

    try {
        $messages = $agent.LatestMemoryJson | ConvertFrom-Json
    }
    catch {
        $messages = @()
        [void]$builder.AppendLine(('<div class="warning error">无法解析 memory_json：{0}</div>' -f (Encode-Html $_.Exception.Message)))
    }

    $messageIndex = 0
    foreach ($message in $messages) {
        $messageIndex++
        $role = [string]$message.role
        if ([string]::IsNullOrWhiteSpace($role)) {
            $role = 'unknown'
        }
        $text = Get-MessageText $message.content
        [void]$builder.AppendLine(('<article class="message {0}"><div class="head"><span class="index">#{1}</span><span class="role">{2}</span></div>' -f (Encode-Html $role), $messageIndex, (Encode-Html $role)))
        [void]$builder.AppendLine(('<div class="body">{0}</div>' -f (Encode-Html $text)))

        foreach ($call in @($message.tool_calls)) {
            if ($null -eq $call) {
                continue
            }
            $name = [string]$call.function.name
            $arguments = $call.function.arguments | ConvertTo-Json -Depth 20 -Compress
            [void]$builder.AppendLine(('<div class="toolcall"><strong>ToolCall:</strong> {0}<br><strong>Arguments:</strong> {1}</div>' -f (Encode-Html $name), (Encode-Html $arguments)))
        }
        [void]$builder.AppendLine('</article>')
    }
    [void]$builder.AppendLine('</section>')
}

[void]$builder.AppendLine('</main></body></html>')

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$outputDirectory = [System.IO.Path]::GetDirectoryName($resolvedOutput)
[System.IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
[System.IO.File]::WriteAllText($resolvedOutput, $builder.ToString(), [System.Text.UTF8Encoding]::new($false))

Get-Item -LiteralPath $resolvedOutput | Select-Object FullName, Length, LastWriteTime
