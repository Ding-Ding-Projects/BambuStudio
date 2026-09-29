#!/usr/bin/env python3
"""Capture the 3D canvas of the built app, with its ImGui panels, in one language mode.

On the real graphics driver PrintWindow leaves the OpenGL canvas blank, so this
driver asks the canvas for its own frame with the layout probe's "canvas-png"
command (builds from a40082726 on). Like sweep-dialogs.py it runs through the
lowlevel-computer-use cheap CLI on a hidden Win32 desktop: launch the
executable with an isolated --datadir and the probe armed, switch to Prepare,
load the sample cube, then save

  canvas-prepare--<tuple>.png        the plate with the cube, the scene toolbar and the gizmo rail
  canvas-gizmo-<name>--<tuple>.png   each requested gizmo, opened by clicking its rail item

    py -3 scripts/md3/capture-canvas.py --exe <bambu-studio.exe> --datadir <dir>
        --tuple yue_HK-light-comfortable --out <folder> [--gizmos move,rotate,scale]

The gizmo rail's items come from the probe's gl_item records (name, canvas
handle, rectangle in canvas pixels). Writes <out>/canvas-capture--<tuple>.json
with each capture's result; a build without canvas-png saves no image, and the
report says so instead of passing a blank canvas off as a capture.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

DEFAULT_CHEAP = os.path.join(os.path.expanduser('~'), 'Documents', 'GitHub', 'lowlevel-computer-use-mcp', '.venv', 'Scripts', 'lowlevel-computer-use-cheap.exe')
CHEAP = os.environ.get('LLCU_CHEAP', DEFAULT_CHEAP)
HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
SENDER = os.path.join(HERE, 'send-layout-probe.py')
CUBE = os.path.join(REPO, '.claude', 'skills', 'run-bambustudio', 'cube.stl')
PREPARE_TAB = (211, 119)  # client coordinates in the 1200x800 main frame the capture datadirs configure


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


def send(desktop, hwnd, command=None, dump=None, timeout=20):
    # SendMessage does not cross desktops, so the sender runs on the hidden one.
    cmd = f'"{sys.executable}" "{SENDER}" {hwnd}'
    if dump:
        cmd += f' "{dump}" --timeout {timeout}'
    if command:
        cmd += f' --command "{command}"'
    cheap('launch_on_headless_desktop', name=desktop, command=cmd)


def wait_for(predicate, timeout, step=0.5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(step)
    return None


def squash(text):
    return (text or '').lower().replace(' ', '').replace('_', '')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--datadir', required=True)
    ap.add_argument('--tuple', required=True, dest='tuple_id')
    ap.add_argument('--out', required=True)
    ap.add_argument('--gizmos', default='move,rotate,scale', help='comma-separated gizmo rail item names')
    ap.add_argument('--desktop', default='bscanvas')
    ap.add_argument('--startup-timeout', type=float, default=240)
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding='utf-8')
    probe_dir = os.path.join(args.out, 'probe')
    os.makedirs(probe_dir, exist_ok=True)
    # The app writes its files itself; keep them on a short path (the long tuple names
    # pushed deep output folders past 260 characters) and move them afterwards.
    staging = os.path.join(os.environ.get('TEMP', os.path.expanduser('~')), 'bbcv')
    os.makedirs(staging, exist_ok=True)
    exe_sha = hashlib.sha256(open(args.exe, 'rb').read()).hexdigest()
    os.environ['BAMBU_LAYOUT_PROBE'] = '1'
    os.environ['BAMBU_LAYOUT_PROBE_TAG'] = f'canvas--{args.tuple_id}'

    cheap('create_headless_desktop', name=args.desktop)
    owned, results = set(), []
    try:
        launched = cheap('launch_on_headless_desktop', name=args.desktop,
                         command=f'"{args.exe}" --datadir "{args.datadir}"')['pid']
        owned.add(launched)

        def main_frame():
            # The app may relaunch itself once (Mesa fallback): adopt whichever process shows the frame.
            for w in cheap('list_headless_windows', name=args.desktop)['windows']:
                if w['class'] == 'wxWindowNR' and w['width'] >= 1000 and w['height'] >= 600:
                    return w
            return None
        frame = wait_for(main_frame, args.startup_timeout, step=1.0)
        if not frame:
            raise SystemExit('timed out waiting for the main frame')
        owned.add(frame['process_id'])
        main_hwnd = frame['handle']
        print(f'main frame {main_hwnd} (pid {frame["process_id"]}) for {args.tuple_id}', flush=True)
        time.sleep(10)  # first layout, plugin prompt, bilingual decoration

        cheap('mouse_click', hwnd=main_hwnd, x=PREPARE_TAB[0], y=PREPARE_TAB[1])
        time.sleep(2.5)
        send(args.desktop, main_hwnd, command=f'load {CUBE}')
        time.sleep(8)

        def canvas_png(name):
            staged = os.path.join(staging, f'{frame["process_id"]}-{len(results)}.png')
            if os.path.exists(staged):
                os.remove(staged)
            send(args.desktop, main_hwnd, command=f'canvas-png {staged}')
            saved = wait_for(lambda: os.path.exists(staged) and os.path.getsize(staged) > 0, 15)
            target = os.path.join(args.out, f'{name}--{args.tuple_id}.png')
            if saved:
                shutil.move(staged, target)
            results.append({'capture': name, 'png': target if saved else None,
                            'result': 'saved' if saved else 'not saved (no canvas-png in this build?)'})
            print(f'  {name}: {results[-1]["result"]}', flush=True)
            return saved

        def dump(name):
            staged = os.path.join(staging, f'{frame["process_id"]}-{name}.jsonl')
            if os.path.exists(staged):
                os.remove(staged)
            send(args.desktop, main_hwnd, dump=staged)
            done = wait_for(lambda: os.path.exists(staged) and
                            open(staged, encoding='utf-8').read().rstrip().endswith('{"kind":"end"}'), 30)
            if not done:
                return []
            target = os.path.join(probe_dir, f'{name}--{args.tuple_id}.jsonl')
            shutil.move(staged, target)
            return [json.loads(line) for line in open(target, encoding='utf-8') if line.strip()]

        canvas_png('canvas-prepare')
        records = dump('canvas-prepare')
        rail = [r for r in records if r.get('kind') == 'gl_item' and r.get('toolbar') == 'gizmo']
        print(f'  gizmo rail: {", ".join(r["name"] for r in rail) or "no items"}', flush=True)
        for wanted in [g.strip() for g in args.gizmos.split(',') if g.strip()]:
            item = next((r for r in rail if squash(r['name']) == squash(wanted)), None)
            if item is None:
                results.append({'capture': f'canvas-gizmo-{wanted}', 'png': None, 'result': 'no such rail item'})
                print(f'  {wanted}: no such rail item', flush=True)
                continue
            rect = item['rect']
            cheap('mouse_click', hwnd=item['host'], x=rect['x'] + rect['w'] // 2, y=rect['y'] + rect['h'] // 2)
            time.sleep(2.5)
            canvas_png(f'canvas-gizmo-{squash(wanted)}')
    finally:
        for pid in list(owned):
            try:
                cheap('kill_process', pid=pid, force=True)
            except SystemExit:
                pass  # already exited (the relaunch parent does)
        time.sleep(2)
        cheap('close_headless_desktop', name=args.desktop)

    report = {'tuple': args.tuple_id, 'exe': args.exe, 'exe_sha256': exe_sha, 'route': 'cheap-lowlevel-headless, canvas-png',
              'captured_at': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'captures': results}
    out_json = os.path.join(args.out, f'canvas-capture--{args.tuple_id}.json')
    with open(out_json, 'w', encoding='utf-8') as fh:
        json.dump(report, fh, ensure_ascii=False, indent=1)
    saved = [r for r in results if r['png']]
    print(f'{len(saved)} of {len(results)} canvas captures saved; report {out_json}')
    return 0 if saved and len(saved) == len(results) else 1


if __name__ == '__main__':
    sys.exit(main())
