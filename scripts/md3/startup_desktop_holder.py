"""Retain one pinned compatibility MCP desktop session for a hosted trace."""
import argparse
import asyncio
import ctypes
from ctypes import wintypes
from datetime import timedelta
from importlib.metadata import version
import json
import os
from pathlib import Path
import re
import sys
import subprocess
import time


def require(value):
    if not value:
        raise RuntimeError("Desktop lifecycle unavailable")


def absent_response(code, response, desktop):
    return (code == 0 and isinstance(response, dict) and response.get("ok") is False
            and isinstance(response.get("error"), str)
            and re.fullmatch(re.escape(f"OpenDesktopW('{desktop}') failed (GetLastError=2:")
                             + r"[^\r\n]*\)", response["error"]) is not None)


def plain(path):
    for ancestor in (path, *path.parents):
        if ancestor.exists():
            require(not (ancestor.lstat().st_file_attributes & 0x400))
    return path


def publish(path, value):
    plain(path.parent)
    require(not path.exists())
    pending = path.with_suffix(".pending")
    with pending.open("x", encoding="utf-8") as stream:
        json.dump(value, stream)
        stream.flush()
        os.fsync(stream.fileno())
    os.replace(pending, path)


def read_release(path, binding):
    plain(path)
    require(path.is_file() and path.stat().st_size <= 2048)
    value = json.loads(path.read_text(encoding="utf-8"))
    require(value == {**binding, "worker_tree_termination_verified": True})


def member(job_name, pid):
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenJobObjectW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
    kernel.OpenJobObjectW.restype = wintypes.HANDLE
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.IsProcessInJob.argtypes = [wintypes.HANDLE, wintypes.HANDLE, ctypes.POINTER(wintypes.BOOL)]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    job, process = kernel.OpenJobObjectW(4, False, job_name), kernel.OpenProcess(0x1000, False, pid)
    try:
        value = wintypes.BOOL()
        require(job and process and kernel.IsProcessInJob(process, job, ctypes.byref(value)) and value.value)
    finally:
        if process:
            kernel.CloseHandle(process)
        if job:
            kernel.CloseHandle(job)


async def hold(args):
    import psutil
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client
    sdk = re.fullmatch(r"1\.(\d+)\.(\d+)", version("mcp"))
    require(sdk and int(sdk[1]) >= 2)
    root = plain(args.root.absolute()).resolve()
    root.relative_to(Path(os.environ["RUNNER_TEMP"]).resolve())
    require(root.is_dir())
    binding = {"nonce": args.nonce, "desktop": args.desktop, "source": args.source}
    member(args.job_name, os.getpid())
    started = time.time()
    parameters = StdioServerParameters(command=sys.executable,
        args=["-m", "lowlevel_computer_use_mcp.server"],
        env={key: os.environ[key] for key in ("PATH", "SYSTEMROOT", "WINDIR", "TEMP", "TMP",
             "USERPROFILE", "LOCALAPPDATA", "APPDATA", "HOMEDRIVE", "HOMEPATH") if key in os.environ})
    with open(os.devnull, "w") as errors:
        async with stdio_client(parameters, errlog=errors) as (read, write):
            async with ClientSession(read, write, read_timeout_seconds=timedelta(seconds=15)) as session:
                await session.initialize()
                servers = [p for p in psutil.Process().children() if p.create_time() >= started and
                    p.cmdline()[1:] == ["-m", "lowlevel_computer_use_mcp.server"] and
                    Path(p.exe()).resolve() == Path(sys.executable).resolve()]
                require(len(servers) == 1)
                server = servers[0]
                identity = (server.pid, server.create_time())
                member(args.job_name, server.pid)

                async def call(name):
                    require(psutil.Process(identity[0]).create_time() == identity[1])
                    member(args.job_name, identity[0])
                    result = await session.call_tool(name, {"params": {"name": args.desktop}})
                    require(not result.isError and len(result.content) == 1 and
                            result.content[0].type == "text" and len(result.content[0].text) <= 65536)
                    return json.loads(result.content[0].text)

                require(absent_response(0, await call("list_headless_windows"), args.desktop))
                created = await call("create_headless_desktop")
                require(created.get("ok") is True and created.get("name") == args.desktop and
                        created.get("already_exists") is False and type(created.get("handle")) is int and
                        created["handle"] > 0 and created.get("full") == "WinSta0\\" + args.desktop)
                publish(root / "ready.json", {**binding, "created": True})
                if args.lifecycle_contract:
                    # No worker or product exists in this explicit contract mode.
                    observed = subprocess.run([os.environ["LLCU_CHEAP"], "list_headless_windows", "--name", args.desktop],
                        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=10)
                    require(observed.returncode == 0 and len(observed.stdout) <= 65536)
                    live = json.loads(observed.stdout)
                    require(live.get("ok") is True and live.get("name") == args.desktop and live.get("count") == 0)
                else:
                    deadline = time.monotonic() + 135
                    while not (root / "release.json").exists():
                        require(time.monotonic() < deadline)
                        require(psutil.Process(identity[0]).create_time() == identity[1])
                        await asyncio.sleep(0.1)
                    read_release(root / "release.json", binding)
                closed = await call("close_headless_desktop")
                require(closed.get("ok") is True and closed.get("name") == args.desktop and closed.get("closed") is True)
    try:
        require(psutil.Process(identity[0]).create_time() != identity[1])
    except psutil.NoSuchProcess:
        pass
    if args.lifecycle_contract:
        observed = subprocess.run([os.environ["LLCU_CHEAP"], "list_headless_windows", "--name", args.desktop],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=10)
        require(len(observed.stdout) <= 65536 and absent_response(observed.returncode, json.loads(observed.stdout), args.desktop))
    publish(root / "closed.json", {**binding, "handle_closed": True, "server_exit_verified": True})


def main():
    require(os.environ.get("GITHUB_ACTIONS") == "true" and
            os.environ.get("RUNNER_ENVIRONMENT") == "github-hosted" and os.environ.get("RUNNER_OS") == "Windows")
    if sys.argv[1:] == ["--absence-contract"]:
        desktop = "startup-missing-" + os.urandom(16).hex()
        result = subprocess.run([os.environ["LLCU_CHEAP"], "list_headless_windows", "--name", desktop],
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=10)
        require(len(result.stdout) <= 65536 and absent_response(result.returncode, json.loads(result.stdout), desktop))
        print("Pinned desktop absence contract passed: exit=0, ok=false, native=2")
        return 0
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--nonce", required=True)
    parser.add_argument("--desktop", required=True)
    parser.add_argument("--source", required=True)
    parser.add_argument("--job-name", required=True)
    parser.add_argument("--lifecycle-contract", action="store_true")
    args = parser.parse_args()
    require(re.fullmatch(r"[0-9a-f]{32}", args.nonce) and re.fullmatch(r"[0-9a-f]{40}", args.source)
            and re.fullmatch(r"startup-loader-[0-9]+-[0-9]+", args.desktop)
            and re.fullmatch(r"Local\\BambuNativeScale-[0-9a-f]{64}", args.job_name))
    asyncio.run(asyncio.wait_for(hold(args), timeout=165))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception:
        raise SystemExit(2)
