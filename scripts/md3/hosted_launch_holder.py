#!/usr/bin/env python3
"""Retain one hosted hidden desktop and the exact packaged app process handle."""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import msvcrt
import os
import sys
import threading
import time
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path


CREATE_NO_WINDOW = 0x08000000
WAIT_OBJECT_0 = 0
WAIT_TIMEOUT = 258
GENERIC_ALL = 0x10000000
HANDLE_FLAG_INHERIT = 1
STARTF_USESTDHANDLES = 0x00000100
STREAM_LIMIT = 1_048_576


class STARTUPINFO(ctypes.Structure):
    _fields_ = [("cb", wintypes.DWORD), ("lpReserved", wintypes.LPWSTR),
                ("lpDesktop", wintypes.LPWSTR), ("lpTitle", wintypes.LPWSTR),
                ("dwX", wintypes.DWORD), ("dwY", wintypes.DWORD),
                ("dwXSize", wintypes.DWORD), ("dwYSize", wintypes.DWORD),
                ("dwXCountChars", wintypes.DWORD), ("dwYCountChars", wintypes.DWORD),
                ("dwFillAttribute", wintypes.DWORD), ("dwFlags", wintypes.DWORD),
                ("wShowWindow", wintypes.WORD), ("cbReserved2", wintypes.WORD),
                ("lpReserved2", ctypes.c_void_p), ("hStdInput", wintypes.HANDLE),
                ("hStdOutput", wintypes.HANDLE), ("hStdError", wintypes.HANDLE)]


class PROCESS_INFORMATION(ctypes.Structure):
    _fields_ = [("hProcess", wintypes.HANDLE), ("hThread", wintypes.HANDLE),
                ("dwProcessId", wintypes.DWORD), ("dwThreadId", wintypes.DWORD)]


class SECURITY_ATTRIBUTES(ctypes.Structure):
    _fields_ = [("nLength", wintypes.DWORD),
                ("lpSecurityDescriptor", ctypes.c_void_p),
                ("bInheritHandle", wintypes.BOOL)]


def drain_pipe(handle: int, path: Path, result: dict) -> None:
    """Drain all bytes while retaining at most one MiB of original output."""
    try:
        saved = bytearray()
        total = 0
        fd = msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
        with os.fdopen(fd, "rb", buffering=0) as stream:
            while True:
                block = stream.read(65536)
                if not block:
                    break
                total += len(block)
                saved.extend(block[:max(0, STREAM_LIMIT - len(saved))])
        path.write_bytes(saved)
        result.update({"bytes_total": total, "bytes_saved": len(saved),
                       "truncated": total > len(saved)})
    except Exception as exc:
        result["capture_error"] = type(exc).__name__


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def atomic_receipt(path: Path, value: dict) -> None:
    temporary = path.with_name(path.name + ".pending")
    temporary.write_text(json.dumps(value, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def hold(exe: Path, datadir: Path, desktop: str, receipt_path: Path,
         stop_path: Path, stdout_path: Path, stderr_path: Path,
         timeout: int) -> int:
    user32 = ctypes.WinDLL("user32", use_last_error=True)
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    user32.OpenDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD,
                                    wintypes.BOOL, wintypes.DWORD]
    user32.OpenDesktopW.restype = wintypes.HANDLE
    user32.CloseDesktop.argtypes = [wintypes.HANDLE]
    kernel32.CreateProcessW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR,
                                        ctypes.c_void_p, ctypes.c_void_p,
                                        wintypes.BOOL, wintypes.DWORD,
                                        ctypes.c_void_p, wintypes.LPCWSTR,
                                        ctypes.POINTER(STARTUPINFO),
                                        ctypes.POINTER(PROCESS_INFORMATION)]
    kernel32.CreateProcessW.restype = wintypes.BOOL
    kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.WaitForSingleObject.restype = wintypes.DWORD
    kernel32.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
    kernel32.GetExitCodeProcess.restype = wintypes.BOOL
    kernel32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    kernel32.CreatePipe.argtypes = [ctypes.POINTER(wintypes.HANDLE), ctypes.POINTER(wintypes.HANDLE),
                                    ctypes.POINTER(SECURITY_ATTRIBUTES), wintypes.DWORD]
    kernel32.CreatePipe.restype = wintypes.BOOL
    kernel32.SetHandleInformation.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.DWORD]
    kernel32.SetHandleInformation.restype = wintypes.BOOL

    data = {"schema": 1, "helper_pid": os.getpid(), "desktop": desktop,
            "helper_executable_sha256": file_sha256(Path(sys.executable)),
            "exe_sha256": file_sha256(exe), "profile": str(datadir),
            "app_pid": None, "launch_started_at_utc": None,
            "app_exit_code": None, "app_exited_at_utc": None,
            "app_exit_confirmed": False,
            "app_terminated_by_holder": False,
            "holder_deadline_seconds": timeout, "deadline_fired": False,
            "holder_finished_at_utc": None, "status": "starting"}
    stream_stats = {"stdout": {}, "stderr": {}}
    data["streams"] = stream_stats
    handle = user32.OpenDesktopW(desktop, 0, False, GENERIC_ALL)
    if not handle:
        data["status"] = "desktop_open_failed"
        data["win32_error"] = ctypes.get_last_error()
        atomic_receipt(receipt_path, data)
        return 2
    process = PROCESS_INFORMATION()
    pipe_handles = []
    readers = []
    try:
        security = SECURITY_ATTRIBUTES(ctypes.sizeof(SECURITY_ATTRIBUTES), None, True)
        pipes = []
        for path, key in ((stdout_path, "stdout"), (stderr_path, "stderr")):
            read_handle, write_handle = wintypes.HANDLE(), wintypes.HANDLE()
            if not kernel32.CreatePipe(ctypes.byref(read_handle), ctypes.byref(write_handle),
                                       ctypes.byref(security), 0):
                raise OSError(ctypes.get_last_error(), "CreatePipe failed")
            if not kernel32.SetHandleInformation(read_handle, HANDLE_FLAG_INHERIT, 0):
                raise OSError(ctypes.get_last_error(), "SetHandleInformation failed")
            pipe_handles.extend([read_handle, write_handle])
            pipes.append((read_handle, write_handle, path, key))
        stdin_read, stdin_write = wintypes.HANDLE(), wintypes.HANDLE()
        if not kernel32.CreatePipe(ctypes.byref(stdin_read), ctypes.byref(stdin_write),
                                   ctypes.byref(security), 0):
            raise OSError(ctypes.get_last_error(), "CreatePipe for stdin failed")
        if not kernel32.SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0):
            raise OSError(ctypes.get_last_error(), "SetHandleInformation for stdin failed")
        pipe_handles.extend([stdin_read, stdin_write])
        startup = STARTUPINFO()
        startup.cb = ctypes.sizeof(startup)
        startup.lpDesktop = f"WinSta0\\{desktop}"
        startup.dwFlags = STARTF_USESTDHANDLES
        startup.hStdInput = stdin_read
        startup.hStdOutput = pipes[0][1]
        startup.hStdError = pipes[1][1]
        command = ctypes.create_unicode_buffer(f'"{exe}" --datadir "{datadir}"')
        data["launch_started_at_utc"] = utc_now()
        ok = kernel32.CreateProcessW(str(exe), command, None, None, True,
                                     CREATE_NO_WINDOW, None, str(exe.parent),
                                     ctypes.byref(startup), ctypes.byref(process))
        if not ok:
            data["status"] = "app_launch_failed"
            data["win32_error"] = ctypes.get_last_error()
            atomic_receipt(receipt_path, data)
            return 3
        for handle_to_close in (stdin_read, stdin_write):
            kernel32.CloseHandle(handle_to_close)
            pipe_handles.remove(handle_to_close)
        for read_handle, write_handle, path, key in pipes:
            kernel32.CloseHandle(write_handle)
            pipe_handles.remove(write_handle)
            pipe_handles.remove(read_handle)
            reader = threading.Thread(target=drain_pipe,
                                      args=(int(read_handle.value), path, stream_stats[key]),
                                      daemon=True)
            reader.start()
            readers.append(reader)
        data["app_pid"] = int(process.dwProcessId)
        data["status"] = "app_launched"
        atomic_receipt(receipt_path, data)
        kernel32.CloseHandle(process.hThread)
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline and not stop_path.exists():
            result = kernel32.WaitForSingleObject(process.hProcess, 250)
            if result == WAIT_OBJECT_0 and data["app_exited_at_utc"] is None:
                code = wintypes.DWORD()
                if kernel32.GetExitCodeProcess(process.hProcess, ctypes.byref(code)):
                    data["app_exit_code"] = int(code.value)
                data["app_exited_at_utc"] = utc_now()
                data["app_exit_confirmed"] = True
                data["status"] = "app_exited_holder_alive"
                atomic_receipt(receipt_path, data)
            elif result == WAIT_OBJECT_0:
                # The handle stays signaled after exit. Keep the desktop alive
                # without spinning until the controller signals teardown.
                time.sleep(0.25)
            elif result not in (WAIT_OBJECT_0, WAIT_TIMEOUT):
                data["status"] = "process_wait_failed"
                data["win32_error"] = ctypes.get_last_error()
                atomic_receipt(receipt_path, data)
                break
        if data["app_exited_at_utc"] is None:
            if kernel32.WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT:
                data["app_terminated_by_holder"] = bool(kernel32.TerminateProcess(process.hProcess, 1))
            data["app_exit_confirmed"] = (kernel32.WaitForSingleObject(process.hProcess, 5000)
                                          == WAIT_OBJECT_0)
            code = wintypes.DWORD()
            if data["app_exit_confirmed"] and kernel32.GetExitCodeProcess(process.hProcess, ctypes.byref(code)):
                data["app_exit_code"] = int(code.value)
            if data["app_exit_confirmed"]:
                data["app_exited_at_utc"] = utc_now()
        data["holder_finished_at_utc"] = utc_now()
        for reader in readers:
            reader.join(timeout=5)
        data["stream_capture_complete"] = (all(not reader.is_alive() for reader in readers)
                                           and all("bytes_total" in stream_stats[key]
                                                   for key in ("stdout", "stderr")))
        data["deadline_fired"] = not stop_path.exists()
        if not data["app_exit_confirmed"]:
            data["status"] = "app_termination_unverified"
        elif not data["stream_capture_complete"]:
            data["status"] = "stream_capture_incomplete"
        else:
            data["status"] = "holder_timeout" if data["deadline_fired"] else "holder_stopped"
        atomic_receipt(receipt_path, data)
        return 0 if data["app_exit_confirmed"] and data["stream_capture_complete"] else 4
    finally:
        for remaining in pipe_handles:
            kernel32.CloseHandle(remaining)
        if process.hProcess:
            kernel32.CloseHandle(process.hProcess)
        user32.CloseDesktop(handle)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--datadir", type=Path, required=True)
    parser.add_argument("--desktop", required=True)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--stop", type=Path, required=True)
    parser.add_argument("--stdout", type=Path, required=True)
    parser.add_argument("--stderr", type=Path, required=True)
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()
    if (os.environ.get("GITHUB_ACTIONS") != "true"
            or os.environ.get("RUNNER_ENVIRONMENT") != "github-hosted"):
        parser.error("The launch holder requires a disposable GitHub-hosted runner")
    if (args.timeout < 30 or args.timeout > 2400 or args.receipt.exists()
            or args.stop.exists() or args.stdout.exists() or args.stderr.exists()
            or args.stdout.parent.resolve() != args.receipt.parent.resolve()
            or args.stderr.parent.resolve() != args.receipt.parent.resolve()
            or not args.exe.is_file() or not args.datadir.is_dir()):
        parser.error("Invalid or reused hosted launch inputs")
    return hold(args.exe, args.datadir, args.desktop, args.receipt, args.stop,
                args.stdout, args.stderr, args.timeout)


if __name__ == "__main__":
    raise SystemExit(main())
