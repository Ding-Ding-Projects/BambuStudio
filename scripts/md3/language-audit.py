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
import json
import os
import shutil
import subprocess
import sys
import time

DEFAULT_CHEAP = os.path.join(os.path.expanduser('~'), 'Documents', 'GitHub', 'lowlevel-computer-use-mcp', '.venv', 'Scripts', 'lowlevel-computer-use-cheap.exe')
CHEAP = os.environ.get('LLCU_CHEAP', DEFAULT_CHEAP)
SENDER = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'send-layout-probe.py')
# Client coordinates at 100% in the 1200x800 main frame and the 780-wide
# Preferences dialog (same values as capture-tuple.py).
PREPARE = (211, 119)
GEAR = (1155, 121)
GENERAL = (79, 120)


def cheap(tool, **kw):
    args = [CHEAP, tool]
    for k, v in kw.items():
        args += [f'--{k}', json.dumps(v) if not isinstance(v, str) else v]
    out = subprocess.run(args, capture_output=True, text=True, timeout=120)
    data = json.loads(out.stdout)
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
    ap.add_argument('--desktop', default='bsaudit')
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)
    os.environ['BAMBU_LAYOUT_PROBE'] = '1'
    os.environ['BAMBU_LAYOUT_PROBE_TAG'] = 'audit'
    audit_file = os.path.join(args.datadir, 'log', 'language-audit.json')
    cheap('create_headless_desktop', name=args.desktop)
    pid = cheap('launch_on_headless_desktop', name=args.desktop, command=f'"{args.exe}" --datadir "{args.datadir}"')['pid']
    summary = {}
    try:
        frame = wait_window(args.desktop, pid, lambda w: w['class'] == 'wxWindowNR' and w['width'] >= 1000 and w['height'] >= 600, 240, 'the main frame')
        time.sleep(8)
        cheap('mouse_click', hwnd=frame['handle'], x=PREPARE[0], y=PREPARE[1])
        time.sleep(6)  # the decorator sweeps every shown window about every 3 s
        result = audit(args.desktop, frame['handle'], audit_file, os.path.join(args.out, 'language-audit--prepare.json'))
        summary['prepare'] = result.get('totals')
        cheap('mouse_click', hwnd=frame['handle'], x=GEAR[0], y=GEAR[1])
        time.sleep(2.5)
        prefs = wait_window(args.desktop, pid, lambda w: w['class'] == '#32770' and w['width'] >= 700 and w['height'] >= 560 and w['title'] != '', 20, 'Preferences')
        cheap('mouse_click', hwnd=prefs['handle'], x=GENERAL[0], y=GENERAL[1])
        time.sleep(6)
        result = audit(args.desktop, frame['handle'], audit_file, os.path.join(args.out, 'language-audit--preferences-general.json'))
        summary['preferences-general'] = result.get('totals')
    finally:
        try:
            cheap('kill_process', pid=pid, force=True)
        except SystemExit:
            pass
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
    print(json.dumps(summary))


if __name__ == '__main__':
    main()
