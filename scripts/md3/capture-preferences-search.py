#!/usr/bin/env python3
"""Capture Preferences filtered by its search field, from the built executable.

Runs entirely through the lowlevel-computer-use cheap CLI on a hidden Win32
desktop: launch the executable with an isolated --datadir, open Preferences
from the gear, select the General page, type the query into the search field's
native Edit child and PrintWindow the dialog. A row below the fold (such as
"Update automatically") is shown this way without scrolling, which the hidden
desktop cannot do: the wheel tool moves the real cursor, and synthetic key
presses do not reach the page, while typed text does reach an Edit child.

    py -3 scripts/md3/capture-preferences-search.py --exe <bambu-studio.exe> --datadir <dir>
        --query "Update automatically" --out <file.png> [--desktop bsprefsearch]

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

# Client coordinates at 100%: the gear in a 1200x800 main frame, the General row
# in the 780-wide Preferences dialog (same values as capture-tuple.py).
GEAR = (1155, 121)
GENERAL = (79, 120)


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


def wait_window(desktop, pid, pred, timeout, what):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for w in cheap('list_headless_windows', name=desktop)['windows']:
            if w['process_id'] == pid and pred(w):
                return w
        time.sleep(1.0)
    raise SystemExit(f'timed out waiting for {what}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--datadir', required=True)
    ap.add_argument('--query', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--desktop', default='bsprefsearch')
    ap.add_argument('--startup-timeout', type=float, default=240)
    args = ap.parse_args()

    cheap('create_headless_desktop', name=args.desktop)
    pid = cheap('launch_on_headless_desktop', name=args.desktop, command=f'"{args.exe}" --datadir "{args.datadir}"')['pid']
    result = {'pid': pid, 'query': args.query}
    try:
        frame = wait_window(args.desktop, pid, lambda w: w['class'] == 'wxWindowNR' and w['width'] >= 1000 and w['height'] >= 600,
                            args.startup_timeout, 'the main frame')
        time.sleep(8)  # first layout and the plugin download dialog
        cheap('mouse_click', hwnd=frame['handle'], x=GEAR[0], y=GEAR[1])
        time.sleep(2.5)
        # The title is localized; the dialog is the 780-wide #32770 the gear opens.
        prefs = wait_window(args.desktop, pid, lambda w: w['class'] == '#32770' and w['width'] >= 700 and w['height'] >= 560 and w['title'] != '',
                            20, 'Preferences')
        cheap('mouse_click', hwnd=prefs['handle'], x=GENERAL[0], y=GENERAL[1])
        time.sleep(1.5)
        # The search field is the topmost visible Edit child of the dialog.
        edits = [c for c in cheap('list_child_windows', hwnd=prefs['handle'])['children'] if c['class'].lower() == 'edit' and c.get('visible', True)]
        if not edits:
            raise SystemExit('no Edit child in Preferences')
        search = min(edits, key=lambda c: (c['top'], c['left']))
        cheap('type_text', hwnd=search['handle'], text=args.query)
        time.sleep(2.5)
        shot = cheap('screenshot', hwnd=prefs['handle'], output_path=args.out)
        if shot.get('rendered_ok') is not True:
            raise SystemExit(f'{args.out}: PrintWindow did not confirm rendered content')
        result.update(out=args.out, bytes=os.path.getsize(args.out))
    finally:
        try:
            cheap('kill_process', pid=pid, force=True)
        except SystemExit:
            pass
        # The software-OpenGL relaunch is a separate process: end whatever still runs there.
        try:
            for w in cheap('list_headless_windows', name=args.desktop)['windows']:
                if w['process_id'] != pid:
                    try:
                        cheap('kill_process', pid=w['process_id'], force=True)
                    except SystemExit:
                        pass
        except SystemExit:
            pass
        time.sleep(2)
        cheap('close_headless_desktop', name=args.desktop)
    print(json.dumps(result, ensure_ascii=False))
    return 0


if __name__ == '__main__':
    sys.exit(main())
