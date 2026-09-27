#!/usr/bin/env python3
"""Retain one hosted hidden desktop and the exact packaged app process handle."""
from __future__ import annotations

import argparse
import ctypes
import hashlib
import json
import os
import time
from ctypes import wintypes
from datetime import datetime, timezone
from pathlib import Path


CREATE_NO_WINDOW = 0x08000000
WAIT_OBJECT_0 = 0
WAIT_TIMEOUT = 258
GENERIC_ALL = 0x10000000


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
         stop_path: Path, timeout: int) -> int:
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

    data = {"schema": 1, "helper_pid": os.getpid(), "desktop": desktop,
            "exe_sha256": file_sha256(exe), "profile": str(datadir),
            "app_pid": None, "launch_started_at_utc": None,
            "app_exit_code": None, "app_exited_at_utc": None,
            "app_terminated_by_holder": False,
            "holder_finished_at_utc": None, "status": "starting"}
    handle = user32.OpenDesktopW(desktop, 0, False, GENERIC_ALL)
    if not handle:
        data["status"] = "desktop_open_failed"
        data["win32_error"] = ctypes.get_last_error()
        atomic_receipt(receipt_path, data)
        return 2
    process = PROCESS_INFORMATION()
    try:
        startup = STARTUPINFO()
        startup.cb = ctypes.sizeof(startup)
        startup.lpDesktop = f"WinSta0\\{desktop}"
        command = ctypes.create_unicode_buffer(f'"{exe}" --datadir "{datadir}"')
        data["launch_started_at_utc"] = utc_now()
        ok = kernel32.CreateProcessW(str(exe), command, None, None, False,
                                     CREATE_NO_WINDOW, None, str(exe.parent),
                                     ctypes.byref(startup), ctypes.byref(process))
        if not ok:
            data["status"] = "app_launch_failed"
            data["win32_error"] = ctypes.get_last_error()
            atomic_receipt(receipt_path, data)
            return 3
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
                data["status"] = "app_exited_holder_alive"
                atomic_receipt(receipt_path, data)
            elif result not in (WAIT_OBJECT_0, WAIT_TIMEOUT):
                data["status"] = "process_wait_failed"
                data["win32_error"] = ctypes.get_last_error()
                atomic_receipt(receipt_path, data)
                break
        if data["app_exited_at_utc"] is None:
            if kernel32.WaitForSingleObject(process.hProcess, 0) == WAIT_TIMEOUT:
                data["app_terminated_by_holder"] = bool(kernel32.TerminateProcess(process.hProcess, 1))
            kernel32.WaitForSingleObject(process.hProcess, 5000)
            code = wintypes.DWORD()
            if kernel32.GetExitCodeProcess(process.hProcess, ctypes.byref(code)):
                data["app_exit_code"] = int(code.value)
            data["app_exited_at_utc"] = utc_now()
        data["holder_finished_at_utc"] = utc_now()
        data["status"] = "holder_stopped" if stop_path.exists() else "holder_timeout"
        atomic_receipt(receipt_path, data)
        return 0
    finally:
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
    parser.add_argument("--timeout", type=int, default=300)
    args = parser.parse_args()
    if (args.timeout < 30 or args.timeout > 600 or args.receipt.exists()
            or args.stop.exists() or not args.exe.is_file() or not args.datadir.is_dir()):
        parser.error("Invalid or reused hosted launch inputs")
    return hold(args.exe, args.datadir, args.desktop, args.receipt, args.stop, args.timeout)


if __name__ == "__main__":
    raise SystemExit(main())
