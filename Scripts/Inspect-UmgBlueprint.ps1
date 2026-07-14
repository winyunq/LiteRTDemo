param(
    [string]$Pattern = 'RequestCurrentAI|Clear Memory|ClearMemory|LEGAL_TARGETS|性格：|select_werewolf_target|submit_werewolf_speech',
    [int]$Port = 50757,
    [string]$ClientId = '950aaaf9-66cf-40af-b4e8-af303d6cd1f1',
    [switch]$Compact,
    [string]$CursorNode = ''
)

$ErrorActionPreference = 'Stop'

function Invoke-UmgRaw {
    param([string]$Command, [hashtable]$Params)

    $client = [System.Net.Sockets.TcpClient]::new()
    $client.ReceiveTimeout = 600000
    $client.SendTimeout = 30000
    $client.Connect('127.0.0.1', $Port)
    $stream = $client.GetStream()

    try {
        $payload = [ordered]@{
            command    = $Command
            params     = $Params
            client_id  = $ClientId
            request_id = [guid]::NewGuid().ToString()
        } | ConvertTo-Json -Depth 20 -Compress

        $bytes = [Text.Encoding]::UTF8.GetBytes($payload)
        $stream.Write($bytes, 0, $bytes.Length)
        $stream.WriteByte(0)
        $stream.Flush()

        $memory = [IO.MemoryStream]::new()
        $buffer = New-Object byte[] 65536
        $done = $false
        while (-not $done) {
            $read = $stream.Read($buffer, 0, $buffer.Length)
            if ($read -le 0) { break }
            $zero = [Array]::IndexOf($buffer, [byte]0, 0, $read)
            if ($zero -ge 0) {
                $memory.Write($buffer, 0, $zero)
                $done = $true
            }
            else {
                $memory.Write($buffer, 0, $read)
            }
        }

        $text = [Text.Encoding]::UTF8.GetString($memory.ToArray())
        $memory.Dispose()
        return $text | ConvertFrom-Json
    }
    finally {
        $stream.Dispose()
        $client.Dispose()
    }
}

if ($CursorNode) {
    $cursorResult = Invoke-UmgRaw -Command 'set_cursor_node' -Params @{ node_id = $CursorNode }
    "cursor_status=$($cursorResult.status);cursor_node=$($cursorResult.cursor_node)"
}

$result = Invoke-UmgRaw -Command 'manage_blueprint_graph' -Params @{ subAction = 'get_nodes' }
"keys=$($result.PSObject.Properties.Name -join ',')"
"node_count=$(@($result.nodes).Count)"

$matches = @($result.nodes) |
    Where-Object { ($_ | ConvertTo-Json -Depth 12 -Compress) -match $Pattern }

if ($Compact) {
    foreach ($node in $matches) {
        $inputs = @($node.inputs | ForEach-Object {
            [ordered]@{
                name = $_.name
                default = $_.default
                expression = $_.expression
                links = @($_.links | ForEach-Object { $_.connect })
            }
        })
        $outputs = @($node.outputs | ForEach-Object {
            [ordered]@{
                name = $_.name
                links = @($_.links | ForEach-Object { $_.connect })
            }
        })
        [ordered]@{
            id = $node.id
            title = $node.title
            class = $node.class
            member = $node.member
            inputs = $inputs
            outputs = $outputs
        } | ConvertTo-Json -Depth 8 -Compress
    }
}
else {
    $matches | ForEach-Object { $_ | ConvertTo-Json -Depth 12 -Compress }
}
