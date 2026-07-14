# Unreal Native MCP Connect

This router leaves Epic's UE 5.8 native MCP plugin unchanged. Unreal's native
server rejects port `0`, so the launcher asks Windows for an unused loopback
port and starts the editor with:

```text
-ModelContextProtocolStartServer -ModelContextProtocolPort=<free-port>
```

The router discovers `UnrealEditor` listeners, correlates them with each
process's `.uproject` command line, probes only endpoints exposing Epic's three
native tools, and requires an explicit project selection before forwarding.

## Smoke test

```powershell
uv run python unreal_mcp_router.py --list
uv run python unreal_mcp_router.py --launch D:\UE5Project\LiteRTDemo\LiteRTDemo.uproject
uv run python unreal_mcp_router.py --invoke list_toolsets --project LiteRTDemo
```

## Codex MCP configuration

Add this as a stdio MCP server. It can coexist with an existing fixed URL while
being tested:

```toml
[mcp_servers.unreal-connect]
command = "uv"
args = [
  "--directory",
  "D:\\UE5Project\\LiteRTDemo\\Scripts\\UnrealMcpRouter",
  "run",
  "python",
  "unreal_mcp_router.py",
]
```

Usage order:

1. `list_unreal_mcp_servers`
2. `connect_unreal_mcp(project="D:\\...\\LiteRTDemo.uproject")`
3. Native `list_toolsets`, `describe_toolset`, and `call_tool`

The selected identity is the project path, not the port. If the selected editor
restarts on another port, the next routed call rediscovers that same project.
