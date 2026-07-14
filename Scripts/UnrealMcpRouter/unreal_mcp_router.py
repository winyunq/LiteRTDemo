"""Project-aware router for Epic's native Unreal Engine MCP server.

Epic's UE 5.8 MCP server requires a concrete non-zero port.  This router keeps
that implementation untouched: the launcher chooses an unused loopback port,
UE receives it through ``-ModelContextProtocolPort=N``, and discovery correlates
UnrealEditor processes, ``.uproject`` command lines, and listening TCP ports.

The stdio MCP surface adds explicit discovery/connection tools and forwards the
three native UE tools after a project has been selected.  Selection is retained
by project path, so a restarted editor can be rediscovered on a different port.
"""

from __future__ import annotations

import argparse
import asyncio
import base64
import json
import logging
import os
import re
import socket
import subprocess
import sys
import time
from contextlib import asynccontextmanager
from dataclasses import dataclass
from pathlib import Path
from typing import Any, AsyncIterator, Iterable

import httpx
import psutil
from mcp.server.fastmcp import FastMCP
from mcp.server.fastmcp.exceptions import ToolError
from mcp.types import CallToolResult, LATEST_PROTOCOL_VERSION, ListToolsResult, TextContent


NATIVE_TOOL_NAMES = frozenset({"list_toolsets", "describe_toolset", "call_tool"})
DEFAULT_EDITOR_EXE = Path(r"D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe")
PROBE_TIMEOUT_SECONDS = float(os.environ.get("UNREAL_MCP_PROBE_TIMEOUT", "1.25"))
DISCOVERY_CACHE_SECONDS = float(os.environ.get("UNREAL_MCP_DISCOVERY_CACHE", "2.0"))
PORT_ARGUMENT = re.compile(r"^-ModelContextProtocolPort=(\d+)$", re.IGNORECASE)

PROJECT_ROOT = Path(__file__).resolve().parents[2]
LOG_DIR = Path(os.environ.get("UNREAL_MCP_ROUTER_LOG_DIR", PROJECT_ROOT / "Saved" / "UnrealMcpRouter"))
LOG_DIR.mkdir(parents=True, exist_ok=True)
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s %(levelname)s %(name)s %(message)s",
    handlers=[logging.FileHandler(LOG_DIR / "router.log", encoding="utf-8")],
)
LOGGER = logging.getLogger("UnrealNativeMcpRouter")


@dataclass(frozen=True)
class EditorProcess:
    pid: int
    project_path: str
    project_name: str
    command_line: str
    candidate_ports: tuple[int, ...]


@dataclass(frozen=True)
class NativeEndpoint:
    pid: int
    project_path: str
    project_name: str
    command_line: str
    host: str
    port: int
    tools: tuple[str, ...]

    @property
    def url(self) -> str:
        return f"http://{self.host}:{self.port}/mcp"

    def to_dict(self, selected: bool = False) -> dict[str, Any]:
        return {
            "project_name": self.project_name,
            "project_path": self.project_path,
            "pid": self.pid,
            "host": self.host,
            "port": self.port,
            "url": self.url,
            "native_tools": list(self.tools),
            "selected": selected,
        }


@dataclass(frozen=True)
class Selection:
    project_path: str
    project_name: str
    pid: int
    host: str
    port: int


_selection: Selection | None = None
_discovery_cache: tuple[float, list[NativeEndpoint]] = (0.0, [])
_discovery_lock = asyncio.Lock()
_forward_lock = asyncio.Lock()

mcp = FastMCP(
    "Unreal Native MCP Connect",
    instructions=(
        "Call list_unreal_mcp_servers, then connect_unreal_mcp for the intended .uproject. "
        "Only after selection should list_toolsets, describe_toolset, or call_tool be used."
    ),
    log_level="ERROR",
)


def _normalise_path(value: str | Path) -> str:
    try:
        return os.path.normcase(os.path.abspath(os.fspath(value)))
    except (OSError, TypeError, ValueError):
        return os.path.normcase(os.fspath(value))


def _extract_project_path(command: Iterable[str]) -> str:
    for argument in command:
        cleaned = argument.strip().strip('"')
        if cleaned.lower().endswith(".uproject"):
            return str(Path(cleaned).resolve(strict=False))
    return ""


def _extract_requested_port(command: Iterable[str]) -> int | None:
    for argument in command:
        match = PORT_ARGUMENT.match(argument.strip().strip('"'))
        if match:
            port = int(match.group(1))
            if 1 <= port <= 65535:
                return port
    return None


def _listener_ports_by_pid() -> dict[int, set[int]]:
    ports: dict[int, set[int]] = {}
    try:
        connections = psutil.net_connections(kind="tcp")
    except (psutil.AccessDenied, psutil.Error, OSError) as exc:
        LOGGER.warning("Unable to enumerate TCP listeners: %s", exc)
        return ports

    for connection in connections:
        if connection.status != psutil.CONN_LISTEN or not connection.pid or not connection.laddr:
            continue
        port = int(connection.laddr.port)
        if port > 0:
            ports.setdefault(int(connection.pid), set()).add(port)
    return ports


def _editor_processes() -> list[EditorProcess]:
    listener_ports = _listener_ports_by_pid()
    editors: list[EditorProcess] = []
    for process in psutil.process_iter(["pid", "name", "cmdline"]):
        try:
            name = (process.info.get("name") or "").lower()
            if name not in {"unrealeditor.exe", "unrealeditor"}:
                continue
            command = tuple(process.info.get("cmdline") or ())
            project_path = _extract_project_path(command)
            if not project_path:
                continue
            requested_port = _extract_requested_port(command)
            ordered_ports: list[int] = []
            if requested_port:
                ordered_ports.append(requested_port)
            ordered_ports.extend(sorted(listener_ports.get(int(process.info["pid"]), set())))
            candidate_ports = tuple(dict.fromkeys(ordered_ports))
            editors.append(
                EditorProcess(
                    pid=int(process.info["pid"]),
                    project_path=project_path,
                    project_name=Path(project_path).stem,
                    command_line=subprocess.list2cmdline(list(command)),
                    candidate_ports=candidate_ports,
                )
            )
        except (psutil.AccessDenied, psutil.NoSuchProcess, psutil.ZombieProcess, OSError):
            continue
    return sorted(editors, key=lambda item: (item.project_name.casefold(), item.pid))


class NativeHttpSession:
    """Minimal Streamable HTTP client for UE's synchronous POST responses.

    The generic Python MCP transport also opens a long-lived GET stream. UE 5.8
    returns 405 for that optional stream, causing needless reconnect delays while
    probing ports. Native UE tools complete through POST, so this client performs
    only the required initialize/initialized/request sequence.
    """

    def __init__(self, url: str, timeout_seconds: float):
        self.url = url
        self.timeout_seconds = max(0.25, timeout_seconds)
        self.client: httpx.AsyncClient | None = None
        self.session_id = ""
        self.protocol_version = LATEST_PROTOCOL_VERSION
        self.request_id = 0

    async def __aenter__(self) -> "NativeHttpSession":
        self.client = httpx.AsyncClient(
            timeout=httpx.Timeout(self.timeout_seconds),
            headers={"Accept": "application/json, text/event-stream"},
            trust_env=False,
        )
        result, response_headers = await self._request(
            "initialize",
            {
                "protocolVersion": LATEST_PROTOCOL_VERSION,
                "capabilities": {},
                "clientInfo": {"name": "unreal-native-mcp-router", "version": "0.1.0"},
            },
            include_response=True,
        )
        self.session_id = response_headers.get("mcp-session-id", "")
        if not self.session_id:
            raise RuntimeError("Native MCP initialize response did not contain mcp-session-id")
        self.protocol_version = str(result.get("protocolVersion") or LATEST_PROTOCOL_VERSION)
        self.client.headers.update(
            {
                "Mcp-Session-Id": self.session_id,
                "MCP-Protocol-Version": self.protocol_version,
            }
        )
        await self._notification("notifications/initialized")
        return self

    async def __aexit__(self, exc_type: Any, exc: Any, traceback: Any) -> None:
        if self.client is None:
            return
        if self.session_id:
            try:
                await self.client.delete(self.url)
            except (httpx.HTTPError, OSError):
                pass
        await self.client.aclose()
        self.client = None

    async def _notification(self, method: str, params: dict[str, Any] | None = None) -> None:
        if self.client is None:
            raise RuntimeError("Native MCP HTTP session is not open")
        response = await self.client.post(
            self.url,
            json={"jsonrpc": "2.0", "method": method, "params": params or {}},
        )
        if response.status_code not in {200, 202, 204}:
            raise RuntimeError(f"Native MCP notification failed with HTTP {response.status_code}")

    async def _request(
        self,
        method: str,
        params: dict[str, Any] | None = None,
        include_response: bool = False,
    ) -> Any:
        if self.client is None:
            raise RuntimeError("Native MCP HTTP session is not open")
        self.request_id += 1
        request_id = self.request_id
        async with self.client.stream(
            "POST",
            self.url,
            json={
                "jsonrpc": "2.0",
                "id": request_id,
                "method": method,
                "params": params or {},
            },
        ) as response:
            payload = await self._decode_response(response, request_id)
            response_headers = dict(response.headers)
        if "error" in payload:
            error = payload.get("error") or {}
            raise RuntimeError(
                f"Native MCP JSON-RPC error {error.get('code')}: {error.get('message')}"
            )
        if response.status_code >= 400:
            raise RuntimeError(f"Native MCP returned HTTP {response.status_code}")
        result = payload.get("result") or {}
        if include_response:
            return result, response_headers
        return result

    @staticmethod
    async def _decode_response(response: httpx.Response, request_id: int) -> dict[str, Any]:
        content_type = response.headers.get("content-type", "").casefold()
        if "text/event-stream" not in content_type:
            await response.aread()
            try:
                payload = response.json()
            except ValueError as exc:
                raise RuntimeError(
                    f"Native MCP returned HTTP {response.status_code} with a non-JSON body; "
                    f"content-type={content_type!r}; body={response.text[:240]!r}"
                ) from exc
            if isinstance(payload, dict):
                return payload
            raise RuntimeError("Native MCP returned a JSON body that is not an object")

        # UE uses text/event-stream for tool calls that complete asynchronously.
        # Stop as soon as this request's result arrives instead of waiting for UE's
        # ~15 second keep-alive timeout to close the response.
        data_lines: list[str] = []
        preview: list[str] = []

        def flush_event() -> dict[str, Any] | None:
            if not data_lines:
                return None
            raw = "\n".join(data_lines).strip()
            data_lines.clear()
            if not raw or raw == "[DONE]":
                return None
            try:
                message = json.loads(raw)
            except ValueError:
                return None
            if isinstance(message, dict) and message.get("id") == request_id:
                return message
            return None

        async for line in response.aiter_lines():
            if len(preview) < 12:
                preview.append(line)
            if not line.strip():
                message = flush_event()
                if message is not None:
                    return message
            elif line.startswith("data:"):
                data_lines.append(line[5:].lstrip())
        message = flush_event()
        if message is not None:
            return message
        raise RuntimeError(
            f"Native MCP returned HTTP {response.status_code} without JSON-RPC result "
            f"for request {request_id}; content-type={content_type!r}; "
            f"body-preview={' | '.join(preview)[:240]!r}"
        )

    async def list_tools(self) -> ListToolsResult:
        result = await self._request("tools/list", {})
        return ListToolsResult.model_validate(result)

    async def call_tool(self, name: str, arguments: dict[str, Any]) -> CallToolResult:
        result = await self._request("tools/call", {"name": name, "arguments": arguments})
        return CallToolResult.model_validate(result)


@asynccontextmanager
async def _native_session(url: str, timeout_seconds: float) -> AsyncIterator[NativeHttpSession]:
    async with NativeHttpSession(url, timeout_seconds) as session:
        yield session


async def _probe_endpoint(editor: EditorProcess, port: int) -> NativeEndpoint | None:
    url = f"http://127.0.0.1:{port}/mcp"

    async def probe() -> NativeEndpoint | None:
        async with _native_session(url, PROBE_TIMEOUT_SECONDS) as session:
            result = await session.list_tools()
            names = tuple(sorted(tool.name for tool in result.tools))
            if not NATIVE_TOOL_NAMES.issubset(names):
                return None
            return NativeEndpoint(
                pid=editor.pid,
                project_path=editor.project_path,
                project_name=editor.project_name,
                command_line=editor.command_line,
                host="127.0.0.1",
                port=port,
                tools=names,
            )

    try:
        return await asyncio.wait_for(probe(), timeout=PROBE_TIMEOUT_SECONDS + 0.75)
    except (asyncio.TimeoutError, OSError, RuntimeError, ValueError, psutil.Error) as exc:
        LOGGER.debug("Port %s for PID %s is not a native MCP endpoint: %s", port, editor.pid, exc)
        return None
    except Exception as exc:  # MCP transports can surface task-group exception wrappers.
        LOGGER.debug("Native MCP probe failed for PID %s port %s: %r", editor.pid, port, exc)
        return None


async def _discover_endpoints(force: bool = False) -> list[NativeEndpoint]:
    global _discovery_cache
    now = time.monotonic()
    cached_at, cached = _discovery_cache
    if not force and now - cached_at <= DISCOVERY_CACHE_SECONDS:
        return list(cached)

    async with _discovery_lock:
        now = time.monotonic()
        cached_at, cached = _discovery_cache
        if not force and now - cached_at <= DISCOVERY_CACHE_SECONDS:
            return list(cached)

        endpoints: list[NativeEndpoint] = []
        for editor in _editor_processes():
            for port in editor.candidate_ports:
                endpoint = await _probe_endpoint(editor, port)
                if endpoint is not None:
                    endpoints.append(endpoint)
                    break
        _discovery_cache = (time.monotonic(), endpoints)
        return list(endpoints)


def _matches_project(endpoint: NativeEndpoint, project: str) -> bool:
    wanted = project.strip()
    if not wanted:
        return True
    wanted_path = _normalise_path(wanted)
    endpoint_path = _normalise_path(endpoint.project_path)
    if wanted_path == endpoint_path:
        return True
    folded = wanted.casefold()
    return folded == endpoint.project_name.casefold() or folded in endpoint.project_path.casefold()


def _select_from(
    endpoints: list[NativeEndpoint],
    project: str = "",
    pid: int = 0,
    port: int = 0,
) -> NativeEndpoint:
    matches = [
        endpoint
        for endpoint in endpoints
        if _matches_project(endpoint, project)
        and (pid <= 0 or endpoint.pid == pid)
        and (port <= 0 or endpoint.port == port)
    ]
    if len(matches) == 1:
        return matches[0]
    if not matches:
        raise ToolError(
            "No live native Unreal MCP endpoint matched the requested project/PID/port. "
            "Call list_unreal_mcp_servers and select one returned instance."
        )
    raise ToolError(
        "The selector matched multiple Unreal editors. Pass the exact project_path or PID from "
        "list_unreal_mcp_servers; the router will not guess."
    )


async def _connect(project: str = "", pid: int = 0, port: int = 0) -> NativeEndpoint:
    global _selection
    endpoints = await _discover_endpoints(force=True)
    endpoint = _select_from(endpoints, project=project, pid=pid, port=port)
    _selection = Selection(
        project_path=endpoint.project_path,
        project_name=endpoint.project_name,
        pid=endpoint.pid,
        host=endpoint.host,
        port=endpoint.port,
    )
    LOGGER.info(
        "Selected project=%s pid=%s endpoint=%s",
        endpoint.project_path,
        endpoint.pid,
        endpoint.url,
    )
    return endpoint


async def _resolve_selection(force: bool = False) -> NativeEndpoint:
    global _selection
    if _selection is None:
        raise ToolError(
            "No Unreal project is selected. Call list_unreal_mcp_servers and connect_unreal_mcp first."
        )
    endpoints = await _discover_endpoints(force=force)
    exact = [
        endpoint
        for endpoint in endpoints
        if endpoint.pid == _selection.pid and endpoint.port == _selection.port
    ]
    if len(exact) == 1:
        return exact[0]

    # A restarted editor gets a new PID and random port. Project path is stable.
    same_project = [
        endpoint
        for endpoint in endpoints
        if _normalise_path(endpoint.project_path) == _normalise_path(_selection.project_path)
    ]
    if len(same_project) == 1:
        endpoint = same_project[0]
        _selection = Selection(
            project_path=endpoint.project_path,
            project_name=endpoint.project_name,
            pid=endpoint.pid,
            host=endpoint.host,
            port=endpoint.port,
        )
        LOGGER.info("Rediscovered selected project at %s", endpoint.url)
        return endpoint
    raise ToolError(
        f"The selected Unreal project '{_selection.project_path}' is no longer uniquely available. "
        "Call list_unreal_mcp_servers and connect_unreal_mcp again."
    )


def _result_error_text(result: Any) -> str:
    parts: list[str] = []
    for content in getattr(result, "content", ()):
        text = getattr(content, "text", None)
        if text:
            parts.append(text)
    return "\n".join(parts) or "The upstream Unreal MCP tool returned an error."


async def _forward(native_tool_name: str, arguments: dict[str, Any]):
    async with _forward_lock:
        endpoint = await _resolve_selection()
        try:
            async with _native_session(endpoint.url, 300.0) as session:
                result = await session.call_tool(native_tool_name, arguments)
        except Exception as first_error:
            LOGGER.warning("Forward to %s failed; forcing rediscovery: %r", endpoint.url, first_error)
            endpoint = await _resolve_selection(force=True)
            try:
                async with _native_session(endpoint.url, 300.0) as session:
                    result = await session.call_tool(native_tool_name, arguments)
            except Exception as second_error:
                raise ToolError(
                    f"Native Unreal MCP call failed after project rediscovery: {second_error}"
                ) from second_error

        if getattr(result, "isError", False):
            raise ToolError(_result_error_text(result))
        content = list(getattr(result, "content", ()) or ())
        if content:
            return content
        structured = getattr(result, "structuredContent", None)
        if structured is not None:
            return [TextContent(type="text", text=json.dumps(structured, ensure_ascii=False))]
        return []


def _free_loopback_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.bind(("127.0.0.1", 0))
        return int(listener.getsockname()[1])


def _launch_editor_process(project_path: str, editor_exe: str = "") -> tuple[subprocess.Popen[Any], int]:
    project = Path(project_path).resolve(strict=False)
    editor = Path(editor_exe or os.environ.get("UNREAL_EDITOR_EXE", "") or DEFAULT_EDITOR_EXE).resolve(strict=False)
    if not project.is_file() or project.suffix.lower() != ".uproject":
        raise ToolError(f"Unreal project does not exist: {project}")
    if not editor.is_file():
        raise ToolError(f"UnrealEditor executable does not exist: {editor}")

    normalised_project = _normalise_path(project)
    running = [item for item in _editor_processes() if _normalise_path(item.project_path) == normalised_project]
    if running:
        raise ToolError(
            f"The project is already open in UnrealEditor (PID {running[0].pid}). "
            "Use list_unreal_mcp_servers/connect_unreal_mcp instead of launching a duplicate."
        )

    port = _free_loopback_port()
    command = [
        str(editor),
        str(project),
        "-log",
        "-ModelContextProtocolStartServer",
        f"-ModelContextProtocolPort={port}",
    ]
    creation_flags = getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0)
    process = subprocess.Popen(
        command,
        cwd=str(project.parent),
        creationflags=creation_flags,
        close_fds=True,
    )
    LOGGER.info("Launched %s PID %s with native MCP port %s", project, process.pid, port)
    return process, port


async def _launch_and_wait(
    project_path: str,
    editor_exe: str = "",
    wait_seconds: float = 120.0,
) -> dict[str, Any]:
    process, port = _launch_editor_process(project_path, editor_exe)
    deadline = time.monotonic() + max(0.0, wait_seconds)
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise ToolError(f"UnrealEditor exited during startup with code {process.returncode}.")
        endpoints = await _discover_endpoints(force=True)
        matches = [endpoint for endpoint in endpoints if endpoint.pid == process.pid and endpoint.port == port]
        if len(matches) == 1:
            endpoint = await _connect(pid=process.pid, port=port)
            return {
                "status": "ready",
                "endpoint": endpoint.to_dict(selected=True),
            }
        await asyncio.sleep(1.0)
    return {
        "status": "starting",
        "pid": process.pid,
        "port": port,
        "project_path": str(Path(project_path).resolve(strict=False)),
        "message": "Editor is still starting; call list_unreal_mcp_servers, then connect_unreal_mcp.",
    }


@mcp.tool(
    name="list_unreal_mcp_servers",
    description=(
        "Discovers live Epic native Unreal MCP servers and identifies each by .uproject, PID, and "
        "its current port. This is read-only and does not select an editor."
    ),
)
async def list_unreal_mcp_servers(force_refresh: bool = True) -> dict[str, Any]:
    endpoints = await _discover_endpoints(force=force_refresh)
    selected = _selection
    return {
        "status": "success",
        "servers": [
            endpoint.to_dict(
                selected=bool(
                    selected
                    and _normalise_path(endpoint.project_path) == _normalise_path(selected.project_path)
                    and endpoint.pid == selected.pid
                    and endpoint.port == selected.port
                )
            )
            for endpoint in endpoints
        ],
    }


@mcp.tool(
    name="connect_unreal_mcp",
    description=(
        "Selects exactly one discovered Unreal editor. Prefer the exact project_path returned by "
        "list_unreal_mcp_servers; PID or port may also disambiguate."
    ),
)
async def connect_unreal_mcp(project: str = "", pid: int = 0, port: int = 0) -> dict[str, Any]:
    endpoint = await _connect(project=project, pid=pid, port=port)
    return {"status": "connected", "endpoint": endpoint.to_dict(selected=True)}


@mcp.tool(
    name="get_unreal_mcp_connection",
    description="Shows the selected Unreal project and revalidates its current random endpoint.",
)
async def get_unreal_mcp_connection(force_refresh: bool = False) -> dict[str, Any]:
    endpoint = await _resolve_selection(force=force_refresh)
    return {"status": "connected", "endpoint": endpoint.to_dict(selected=True)}


@mcp.tool(
    name="launch_unreal_project",
    description=(
        "Launches one .uproject with Epic native MCP on an OS-selected free loopback port, waits for "
        "discovery, and selects it. It refuses to launch a duplicate project."
    ),
)
async def launch_unreal_project(
    project_path: str,
    editor_exe: str = "",
    wait_seconds: float = 120.0,
) -> dict[str, Any]:
    return await _launch_and_wait(project_path, editor_exe, wait_seconds)


@mcp.tool(name="list_toolsets", description="Forwards Epic native MCP list_toolsets to the selected Unreal project.")
async def list_toolsets():
    return await _forward("list_toolsets", {})


@mcp.tool(
    name="describe_toolset",
    description="Forwards Epic native MCP describe_toolset to the selected Unreal project.",
)
async def describe_toolset(toolset_name: str):
    return await _forward("describe_toolset", {"toolset_name": toolset_name})


@mcp.tool(
    name="call_tool",
    description="Forwards Epic native MCP call_tool to the selected Unreal project.",
)
async def call_tool(
    tool_name: str,
    arguments: dict[str, Any] | None = None,
    toolset_name: str = "",
):
    forwarded: dict[str, Any] = {
        "tool_name": tool_name,
        "arguments": arguments or {},
    }
    if toolset_name:
        forwarded["toolset_name"] = toolset_name
    return await _forward("call_tool", forwarded)


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    action = parser.add_mutually_exclusive_group()
    action.add_argument("--list", action="store_true", help="Print discovered endpoints as JSON and exit.")
    action.add_argument("--launch", metavar="UPROJECT", help="Launch a project on a free native MCP port.")
    action.add_argument(
        "--invoke",
        choices=sorted(NATIVE_TOOL_NAMES),
        help="Connect to --project, invoke one native MCP tool, print JSON, and exit.",
    )
    parser.add_argument("--editor", default="", help="Optional UnrealEditor.exe path for --launch.")
    parser.add_argument("--wait", type=float, default=120.0, help="Seconds to wait for --launch readiness.")
    parser.add_argument("--project", default="", help="Project path or unique name used by --invoke.")
    parser.add_argument("--toolset", default="", help="Toolset name for describe_toolset/call_tool.")
    parser.add_argument("--tool", default="", help="Tool name for call_tool.")
    parser.add_argument("--arguments", default="{}", help="JSON object passed to call_tool.")
    parser.add_argument(
        "--arguments-base64",
        default="",
        help="UTF-8 Base64 JSON object for call_tool; avoids Windows command-line quote rewriting.",
    )
    return parser


async def _run_cli(args: argparse.Namespace) -> int:
    if args.list:
        endpoints = await _discover_endpoints(force=True)
        print(json.dumps({"servers": [item.to_dict() for item in endpoints]}, ensure_ascii=False, indent=2))
        return 0
    if args.launch:
        result = await _launch_and_wait(args.launch, args.editor, args.wait)
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0
    if args.invoke:
        endpoint = await _connect(project=args.project)
        if args.invoke == "list_toolsets":
            native_arguments: dict[str, Any] = {}
        elif args.invoke == "describe_toolset":
            if not args.toolset:
                raise ToolError("--toolset is required for describe_toolset")
            native_arguments = {"toolset_name": args.toolset}
        else:
            if not args.tool:
                raise ToolError("--tool is required for call_tool")
            raw_arguments = args.arguments
            if args.arguments_base64:
                try:
                    raw_arguments = base64.b64decode(args.arguments_base64).decode("utf-8")
                except (ValueError, UnicodeDecodeError) as exc:
                    raise ToolError(f"--arguments-base64 is invalid: {exc}") from exc
            try:
                tool_arguments = json.loads(raw_arguments)
            except ValueError as exc:
                raise ToolError(f"--arguments is not valid JSON: {exc}") from exc
            if not isinstance(tool_arguments, dict):
                raise ToolError("--arguments must decode to a JSON object")
            native_arguments = {"tool_name": args.tool, "arguments": tool_arguments}
            if args.toolset:
                native_arguments["toolset_name"] = args.toolset
        content = await _forward(args.invoke, native_arguments)
        print(
            json.dumps(
                {
                    "endpoint": endpoint.to_dict(selected=True),
                    "content": [block.model_dump(mode="json", exclude_none=True) for block in content],
                },
                ensure_ascii=False,
                indent=2,
            )
        )
        return 0
    return -1


def main() -> int:
    args = _parser().parse_args()
    if args.list or args.launch or args.invoke:
        return asyncio.run(_run_cli(args))
    mcp.run(transport="stdio")
    return 0


if __name__ == "__main__":
    sys.exit(main())
