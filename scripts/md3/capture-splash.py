#!/usr/bin/env python3
"""Capture the startup splash of the built app from a hidden desktop.

The splash lives for well under a second, so a screenshot of its window comes
back before it paints (the md3-v148 attempt produced only black frames). With
BAMBU_LAYOUT_PROBE set, a build that carries the probe's splash hook saves the
exact bitmap the splash shows as <data dir>/log/splash.png; this launches the
executable with the probe on, waits for that file, copies it to --out and closes
everything. For an older build without the hook it also tries a burst of
PrintWindow frames while the splash is up and keeps the last one with real
pixels, and says which route produced the file. Nothing touches the visible
desktop.

    py -3 scripts/md3/capture-splash.py --exe <bambu-studio.exe> --datadir <dir> --out <file.png>
        [--desktop bssplash] [--interval 0.25] [--frames 40] [--timeout 120]
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time

DEFAULT_CHEAP = os.path.join(os.path.expanduser('~'), 'Documents', 'GitHub', 'lowlevel-computer-use-mcp', '.venv', 'Scripts', 'lowlevel-computer-use-cheap.exe')
CHEAP = os.environ.get('LLCU_CHEAP', DEFAULT_CHEAP)


def cheap(tool, **kw):
    args = [CHEAP, tool]
    for k, v in kw.items():
        args += [f'--{k}', json.dumps(v) if not isinstance(v, str) else v]
    out = subprocess.run(args, capture_output=True, text=True, timeout=120)
    data = json.loads(out.stdout)
    if not data.get('ok'):
        raise SystemExit(f'{tool}: {data}')
    return data


def painted_fraction(path):
    """Share of pixels that are not pure black: 0.0 for a frame taken before the splash painted."""
    from PIL import Image
    with Image.open(path) as im:
        pixels = im.convert('RGB').getdata()
        return sum(1 for p in pixels if p != (0, 0, 0)) / max(1, len(pixels))


def burst(w, args):
    """Fallback for a build without the hook: frames every --interval seconds until the splash closes."""
    stem, ext = os.path.splitext(args.out)
    frames = []
    for i in range(args.frames):
        path = f'{stem}--frame{i:02d}{ext}'
        try:
            cheap('screenshot', hwnd=w['handle'], output_path=path)
        except SystemExit:
            break  # the splash closed
        frames.append((path, painted_fraction(path)))
        time.sleep(args.interval)
    painted = [f for f in frames if f[1] > 0.05]
    return frames, (painted[-1][0] if painted else None)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--datadir', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--desktop', default='bssplash')
    ap.add_argument('--interval', type=float, default=0.25, help='seconds between fallback burst frames')
    ap.add_argument('--frames', type=int, default=40, help='most fallback frames while the splash is up')
    ap.add_argument('--timeout', type=float, default=120)
    args = ap.parse_args()

    hooked = os.path.join(args.datadir, 'log', 'splash.png')
    if os.path.exists(hooked):
        os.remove(hooked)  # a file from an earlier run would prove nothing
    # The launched process inherits this environment.
    os.environ['BAMBU_LAYOUT_PROBE'] = '1'
    os.environ['BAMBU_LAYOUT_PROBE_TAG'] = 'splash'

    cheap('create_headless_desktop', name=args.desktop)
    launched = cheap('launch_on_headless_desktop', name=args.desktop, command=f'"{args.exe}" --datadir "{args.datadir}"')['pid']
    pids = {launched}
    result = {'exe': args.exe, 'datadir': args.datadir}
    try:
        deadline = time.monotonic() + args.timeout
        burst_frames, burst_kept = [], None
        while time.monotonic() < deadline:
            if os.path.exists(hooked) and os.path.getsize(hooked) > 0:
                time.sleep(0.5)  # let the write finish
                shutil.copyfile(hooked, args.out)
                result.update(route='probe bitmap', out=args.out, painted_fraction=round(painted_fraction(args.out), 3))
                break
            windows = cheap('list_headless_windows', name=args.desktop)['windows']
            main_up = any(w['class'] == 'wxWindowNR' and w['width'] >= 1000 and w['height'] >= 600 for w in windows)
            for w in windows:
                if not burst_frames and w['class'] == 'wxWindowNR' and w['width'] >= 300 and abs(w['width'] - w['height']) <= 4:
                    pids.add(w['process_id'])
                    burst_frames, burst_kept = burst(w, args)
            if main_up and not os.path.exists(hooked):
                # The splash is gone: without the hook, the burst is all there is.
                time.sleep(2)
                if os.path.exists(hooked):
                    continue
                result.update(route='window burst (build without the splash hook)', frames=len(burst_frames),
                              painted_fraction=[round(f[1], 3) for f in burst_frames])
                if burst_kept:
                    shutil.copyfile(burst_kept, args.out)
                    result['out'] = args.out
                break
            time.sleep(0.2)
        print(json.dumps(result))
        if 'out' not in result:
            raise SystemExit('no painted splash: the build predates the splash hook and every burst frame was black'
                             if 'route' in result else 'no splash within the timeout')
    finally:
        for pid in pids:
            try:
                cheap('kill_process', pid=pid, force=True)
            except SystemExit:
                pass
        # The software-OpenGL relaunch is a separate process: kill whatever still runs on the desktop.
        try:
            for w in cheap('list_headless_windows', name=args.desktop)['windows']:
                if w['process_id'] not in pids:
                    pids.add(w['process_id'])
                    try:
                        cheap('kill_process', pid=w['process_id'], force=True)
                    except SystemExit:
                        pass
        except SystemExit:
            pass
        time.sleep(2)
        cheap('close_headless_desktop', name=args.desktop)
    return 0


if __name__ == '__main__':
    sys.exit(main())
