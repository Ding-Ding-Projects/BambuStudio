#!/usr/bin/env python3
"""Capture the page-level surfaces of one tuple from the built executable.

Runs entirely through the lowlevel-computer-use cheap CLI on a hidden Win32
desktop: launch the executable with an isolated --datadir, wait for the main
frame, click through the workspace tabs and the Preferences tabs, PrintWindow
each surface, then kill the process and release the desktop. Nothing touches
the visible desktop.

    py -3 scripts/md3/capture-tuple.py --exe <bambu-studio.exe> --datadir <dir>
        --tuple en-light-comfortable --out <folder> --suffix before [--probe <dir>]

Files are written as <surface>--<tuple>--<suffix>.png. With --probe, a
layout-probe dump is requested for the main frame and for Preferences into
<probe>/<tuple>--<suffix>-<surface>.jsonl (needs a build that carries the
probe; the request is harmless on one that does not).

Environment: LLCU_CHEAP points at lowlevel-computer-use-cheap.exe; the default
is the checkout beside this repository under the user's GitHub folder.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import time

DEFAULT_CHEAP = os.path.join(os.path.expanduser('~'), 'Documents', 'GitHub', 'lowlevel-computer-use-mcp', '.venv', 'Scripts', 'lowlevel-computer-use-cheap.exe')
CHEAP = os.environ.get('LLCU_CHEAP', DEFAULT_CHEAP)

# Client coordinates of the workspace tabs at 100% in a 1200x800 main frame.
TABS = {'home': (90, 119), 'prepare': (211, 119), 'preview': (346, 119), 'device': (483, 119), 'project': (619, 119), 'ink': (755, 119)}
GEAR = (1155, 121)
# Preferences sidebar rows at 100% in the 780-wide dialog.
PREF_TABS = {'appearance': (79, 75), 'general': (79, 120), 'user': (79, 166), '3d': (79, 212), 'other': (79, 258)}
PREF_CLOSE = (753, 21)


def cheap(tool, **kw):
    args = [CHEAP, tool]
    for k, v in kw.items():
        args += [f'--{k}', json.dumps(v) if not isinstance(v, str) else v]
    out = subprocess.run(args, capture_output=True, text=True, timeout=120)
    try:
        data = json.loads(out.stdout)
    except json.JSONDecodeError:
        raise SystemExit(f'{tool}: unreadable reply\n{out.stdout}\n{out.stderr}')
    if not data.get('ok'):
        raise SystemExit(f'{tool}: {data}')
    return data


def find_window(desktop, pid, pred):
    for w in cheap('list_headless_windows', name=desktop)['windows']:
        if w['process_id'] == pid and pred(w):
            return w
    return None


def wait_window(desktop, pid, pred, timeout, what):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        w = find_window(desktop, pid, pred)
        if w:
            return w
        time.sleep(1.0)
    raise SystemExit(f'timed out waiting for {what}')


def shot(hwnd, path):
    cheap('screenshot', hwnd=hwnd, output_path=path)
    size = os.path.getsize(path)
    if size < 2000:
        raise SystemExit(f'{path}: {size} bytes, not a rendered frame')
    print(f'  {os.path.basename(path)} ({size} bytes)')


def click(hwnd, xy, settle=1.5):
    cheap('mouse_click', hwnd=hwnd, x=xy[0], y=xy[1])
    time.sleep(settle)


def probe(hwnd, path, timeout=20, desktop='bscap'):
    # IsWindow / SendMessage fail across desktops, so the sender itself runs on
    # the hidden desktop: launched there through the cheap CLI, then this side
    # only waits for the dump file to appear.
    here = os.path.dirname(os.path.abspath(__file__))
    sender = os.path.join(here, 'send-layout-probe.py')
    cheap('launch_on_headless_desktop', name=desktop, command=f'"{sys.executable}" "{sender}" {hwnd} "{path}" --timeout {timeout}')
    deadline = time.monotonic() + timeout + 5
    while time.monotonic() < deadline:
        if os.path.exists(path) and os.path.getsize(path) > 0:
            print(f'  probe dump {os.path.basename(path)} ({os.path.getsize(path)} bytes)')
            return True
        time.sleep(0.5)
    print(f'  no probe dump for {os.path.basename(path)}')
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--datadir', required=True)
    ap.add_argument('--tuple', required=True, dest='tuple_id')
    ap.add_argument('--out', required=True)
    ap.add_argument('--suffix', default='after')
    ap.add_argument('--probe', default=None)
    ap.add_argument('--desktop', default='bscap')
    ap.add_argument('--startup-timeout', type=float, default=240)
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    if args.probe:
        os.makedirs(args.probe, exist_ok=True)

    def out(surface):
        return os.path.join(args.out, f'{surface}--{args.tuple_id}--{args.suffix}.png')

    if args.probe:
        # The probe is gated on this variable; a launch through the cheap CLI
        # inherits this process's environment, so set it here.
        os.environ['BAMBU_LAYOUT_PROBE'] = '1'
        os.environ['BAMBU_LAYOUT_PROBE_TAG'] = f'{args.tuple_id}--{args.suffix}'
    cheap('create_headless_desktop', name=args.desktop)
    launch = cheap('launch_on_headless_desktop', name=args.desktop, command=f'"{args.exe}" --datadir "{args.datadir}"')
    pid = launch['pid']
    print(f'launched pid {pid} on {args.desktop} for {args.tuple_id}')
    try:
        main_frame = wait_window(args.desktop, pid, lambda w: w['class'] == 'wxWindowNR' and w['width'] >= 1000 and w['height'] >= 600, args.startup_timeout, 'the main frame')
        hwnd = main_frame['handle']
        # Let the first layout and the plugin download dialog settle.
        time.sleep(8)
        for surface, xy in TABS.items():
            click(hwnd, xy, settle=2.5)
            shot(hwnd, out(surface))
            if args.probe and surface == 'prepare':
                probe(hwnd, os.path.join(args.probe, f'{args.tuple_id}--{args.suffix}-prepare.jsonl'), desktop=args.desktop)
        click(hwnd, GEAR, settle=2.5)
        # The title is localized; the dialog is the 780-wide #32770 the gear opens.
        prefs = wait_window(args.desktop, pid, lambda w: w['class'] == '#32770' and w['width'] >= 700 and w['height'] >= 560 and w['title'] != '', 20, 'Preferences')
        for tab, xy in PREF_TABS.items():
            click(prefs['handle'], xy, settle=1.5)
            shot(prefs['handle'], out(f'preferences-{tab}'))
        if args.probe:
            probe(hwnd, os.path.join(args.probe, f'{args.tuple_id}--{args.suffix}-preferences.jsonl'), desktop=args.desktop)
        click(prefs['handle'], PREF_CLOSE, settle=1.0)
    finally:
        cheap('kill_process', pid=pid, force=True)
        time.sleep(2)
        cheap('close_headless_desktop', name=args.desktop)
    print('done')
    return 0


if __name__ == '__main__':
    sys.exit(main())
