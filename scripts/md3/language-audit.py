#!/usr/bin/env python3
"""Run the layout probe's language-audit on Prepare and on Preferences > General, bilingual mode.

The audit lists every shown native label that has a Cantonese translation yet
shows English only (docs/features/design-system/layout-probe.md). It runs
inside the application, so the request is sent from the hidden desktop itself,
the same way capture-tuple.py asks for a layout dump.

    py -3 scripts/md3/language-audit.py --exe <bambu-studio.exe> --datadir <bilingual datadir>
        --out <folder> [--desktop bsaudit]

Writes <out>/language-audit--prepare.json and
<out>/language-audit--preferences-general.json. Environment: LLCU_CHEAP points
at lowlevel-computer-use-cheap.exe; the default is the checkout beside this
repository under the user's GitHub folder.
"""
import argparse
import ctypes
from ctypes import wintypes
import json
import os
import shutil
import subprocess
import sys
import time
import uuid
from pathlib import Path

import psutil

from recapture import App, find_control

DEFAULT_CHEAP = os.path.join(os.path.expanduser('~'), 'Documents', 'GitHub', 'lowlevel-computer-use-mcp', '.venv', 'Scripts', 'lowlevel-computer-use-cheap.exe')
CHEAP = os.environ.get('LLCU_CHEAP', DEFAULT_CHEAP)
SENDER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'send-layout-probe.py')
def cheap(tool, **kw):
    args = [CHEAP, tool]
    for k, v in kw.items():
        args += [f'--{k}', json.dumps(v) if not isinstance(v, str) else v]
    out = subprocess.run(args, capture_output=True, text=True, timeout=120)
    data = json.loads(out.stdout)
    if not data.get('ok'):
        raise SystemExit(f'{tool}: {data}')
    return data


def visible_target_windows(desktop, pid):
    """Read only the target PID's top-level captions on the named desktop.

    The cheap CLI's list operation sends synchronous WM_GETTEXT to every
    window. A busy splash can block that operation before the PID is filtered.
    GetWindowTextW reads a foreign process's cached top-level caption without
    asking its event loop to respond. All input and launches still use Lowlevel.
    """
    user = ctypes.WinDLL('user32', use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    user.OpenDesktopW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    user.OpenDesktopW.restype = wintypes.HANDLE
    user.CloseDesktop.argtypes = [wintypes.HANDLE]
    user.EnumDesktopWindows.argtypes = [wintypes.HANDLE, callback_type, wintypes.LPARAM]
    user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
    user.GetWindowRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user.IsWindowVisible.argtypes = [wintypes.HWND]
    for function in (user.GetClassNameW, user.GetWindowTextW):
        function.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
    handle = user.OpenDesktopW(desktop, 0, False, 0x0001)  # DESKTOP_READOBJECTS
    if not handle:
        raise ctypes.WinError(ctypes.get_last_error())
    windows = []

    @callback_type
    def collect(hwnd, _):
        owner = wintypes.DWORD()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user.IsWindowVisible(hwnd):
            cls = ctypes.create_unicode_buffer(128)
            title = ctypes.create_unicode_buffer(512)
            rect = wintypes.RECT()
            if user.GetClassNameW(hwnd, cls, len(cls)) and user.GetWindowRect(hwnd, ctypes.byref(rect)):
                user.GetWindowTextW(hwnd, title, len(title))
                windows.append({'process_id': pid, 'handle': hwnd, 'class': cls.value, 'title': title.value,
                                'width': rect.right - rect.left, 'height': rect.bottom - rect.top})
        return True

    try:
        if not user.EnumDesktopWindows(handle, collect, 0):
            raise ctypes.WinError(ctypes.get_last_error())
    finally:
        user.CloseDesktop(handle)
    return windows


def wait_window(desktop, pid, pred, timeout, what):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for w in visible_target_windows(desktop, pid):
            if pred(w):
                return w
        time.sleep(1.0)
    raise SystemExit(f'timed out waiting for {what}')


def click_control(probe, hwnd, label, *, scope=None):
    records = probe.probe()
    control = find_control(records, label, scope or hwnd)
    if not control or not control.get('enabled', True):
        raise SystemExit(f'No enabled {label!r} control on the target surface.')
    bounds = control['rect']
    # Address the target's own client area, never a guessed point on the frame.
    cheap('mouse_click', hwnd=control['hwnd'], x=bounds['w'] // 2, y=bounds['h'] // 2)


def audit(desktop, hwnd, audit_file, out_file):
    if os.path.exists(audit_file):
        os.remove(audit_file)
    cheap('launch_on_headless_desktop', name=desktop, command=f'"{sys.executable}" "{SENDER}" {hwnd} --command language-audit')
    deadline = time.monotonic() + 30
    while time.monotonic() < deadline:
        if os.path.exists(audit_file) and os.path.getsize(audit_file) > 0:
            time.sleep(0.5)
            shutil.copyfile(audit_file, out_file)
            return json.load(open(out_file, encoding='utf-8'))
        time.sleep(0.5)
    raise SystemExit(f'no language-audit.json for {out_file}')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', required=True)
    ap.add_argument('--datadir', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--desktop', default='bsaudit-' + uuid.uuid4().hex)
    args = ap.parse_args()
    exe = Path(args.exe).resolve(strict=True)
    profile = Path(args.datadir).resolve(strict=True)
    if not (profile / 'BambuStudio.conf').is_file():
        raise SystemExit('The synthetic bilingual profile is missing its configuration.')
    os.makedirs(args.out, exist_ok=True)
    os.environ['BAMBU_LAYOUT_PROBE'] = '1'
    os.environ['BAMBU_LAYOUT_PROBE_TAG'] = 'audit'
    audit_file = os.path.join(args.datadir, 'log', 'language-audit.json')
    # launch_on_headless_desktop creates the named desktop. A separate one-shot
    # create call cannot keep a handle alive after the cheap CLI exits.
    command = subprocess.list2cmdline([str(exe), '--datadir', str(profile)])
    pid = cheap('launch_on_headless_desktop', name=args.desktop, command=command)['pid']
    process = None
    summary = {}
    labels = []
    try:
        candidate = psutil.Process(pid)
        arguments = candidate.cmdline()
        if Path(candidate.exe()).resolve() != exe or arguments[1:] != ['--datadir', str(profile)]:
            raise SystemExit('Launched process does not match the target executable and profile.')
        process = candidate
        frame = wait_window(args.desktop, pid, lambda w: w['class'] == 'wxWindowNR' and w['width'] >= 1000 and w['height'] >= 600 and 'Bambu Studio' in w['title'], 240, 'the main frame')
        time.sleep(8)
        probe = App(str(exe), str(profile), args.desktop, args.out)
        probe.main = frame['handle']
        probe.pid = pid
        click_control(probe, frame['handle'], 'Prepare')
        time.sleep(6)  # the decorator sweeps every shown window about every 3 s
        if not find_control(probe.probe(), 'Add ink', frame['handle']):
            raise SystemExit('Prepare did not expose its ink controls.')
        result = audit(args.desktop, frame['handle'], audit_file, os.path.join(args.out, 'language-audit--prepare.json'))
        if result.get('mode') != 'bilingual' or not result.get('windows') or any(
                window['class'] != 'wxFrame' for window in result['windows']):
            raise SystemExit('Prepare audit is blocked by another window or the wrong language.')
        summary['prepare'] = result['totals']
        labels.extend(result['english_only'])
        click_control(probe, frame['handle'], 'Settings')
        time.sleep(2.5)
        prefs = wait_window(args.desktop, pid, lambda w: w['class'] == '#32770' and w['width'] >= 700 and w['height'] >= 560 and ('Preferences' in w['title'] or '偏好設定' in w['title']), 20, 'Preferences')
        click_control(probe, prefs['handle'], 'General')
        time.sleep(6)
        if not find_control(probe.probe(), 'General Settings', prefs['handle']):
            raise SystemExit('Preferences did not expose the General settings section.')
        result = audit(args.desktop, frame['handle'], audit_file, os.path.join(args.out, 'language-audit--preferences-general.json'))
        if result.get('mode') != 'bilingual' or not any(
                window['class'] == 'wxDialog' and ('Preferences' in window['title'] or '偏好設定' in window['title'])
                for window in result.get('windows', [])):
            raise SystemExit('Preferences was absent from the audited windows.')
        summary['preferences-general'] = result['totals']
        labels.extend(result['english_only'])
    finally:
        if process is not None:
            try:
                # psutil checks creation time as well as PID on termination.
                process.terminate()
                process.wait(timeout=10)
            except psutil.NoSuchProcess:
                pass
        # Other windows may belong to unrelated processes. Never terminate
        # their owners; only release our named desktop handle.
        cheap('close_headless_desktop', name=args.desktop)
    print(json.dumps(summary))
    if labels:
        raise SystemExit(f"{len(labels)} translated controls still show only English.")


if __name__ == '__main__':
    main()
