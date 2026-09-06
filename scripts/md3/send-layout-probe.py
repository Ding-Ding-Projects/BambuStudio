#!/usr/bin/env python3
"""Ask a running BambuStudio to write a layout-probe dump.

The application answers WM_COPYDATA with dwData == 2 and a UTF-16 payload
"layout-probe [<path>]" by walking every top-level window and writing an NDJSON
dump (see docs/features/design-system/layout-probe.md). This sender needs only
the main window handle, which a headless driver obtains from its own window
list, so it works for a process on a hidden desktop in the same window station.

    py -3 scripts/md3/send-layout-probe.py <hwnd> [<out.jsonl>] [--timeout 30]

Exit code 0 when the dump file exists and is non-empty within the timeout,
1 otherwise. Only the standard library is used.
"""
from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import os
import sys
import time

WM_COPYDATA = 0x004A
LAYOUT_PROBE_COMMAND = 2


class COPYDATASTRUCT(ctypes.Structure):
    _fields_ = [
        ("dwData", ctypes.c_size_t),
        ("cbData", wt.DWORD),
        ("lpData", ctypes.c_void_p),
    ]


def send(hwnd: int, payload: str) -> int:
    user32 = ctypes.windll.user32
    if not user32.IsWindow(hwnd):
        raise SystemExit(f"{hwnd:#x} is not a window handle")
    buf = ctypes.create_unicode_buffer(payload)
    cds = COPYDATASTRUCT(LAYOUT_PROBE_COMMAND, ctypes.sizeof(buf), ctypes.cast(buf, ctypes.c_void_p))
    user32.SendMessageW.restype = ctypes.c_ssize_t
    user32.SendMessageW.argtypes = [wt.HWND, wt.UINT, wt.WPARAM, wt.LPARAM]
    return int(user32.SendMessageW(hwnd, WM_COPYDATA, 0, ctypes.addressof(cds)))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("hwnd", help="main window handle, decimal or 0x-hex")
    ap.add_argument("out", nargs="?", help="dump path; omitted means the app chooses under its log dir")
    ap.add_argument("--timeout", type=float, default=30.0)
    ap.add_argument("--command", default=None, help="send this payload instead of layout-probe (menu-popup <Title>, invoke <label>)")
    args = ap.parse_args()
    hwnd = int(args.hwnd, 0)
    payload = args.command if args.command else "layout-probe" + (f" {os.path.abspath(args.out)}" if args.out else "")
    result = send(hwnd, payload)
    print(f"sent {payload!r} to {hwnd:#x}; reply {result}")
    if not args.out or args.command:
        return 0
    deadline = time.monotonic() + args.timeout
    while time.monotonic() < deadline:
        if os.path.exists(args.out) and os.path.getsize(args.out) > 0:
            print(f"dump written: {args.out} ({os.path.getsize(args.out)} bytes)")
            return 0
        time.sleep(0.25)
    print(f"no dump at {args.out} after {args.timeout}s", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
