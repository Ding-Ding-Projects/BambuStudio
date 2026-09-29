#!/usr/bin/env python3
"""Show a real tooltip of the built app and check that it is the Material one.

Every wx tooltip comes from one shared Win32 tooltip control ("tooltips_class32").
Drawn with its visual style it is the system's pale box; the Material plain tooltip
is InverseSurface (#2f3036 in the light theme, #e3e2e9 in the dark one) behind
InverseOn text. The window class is the same either way, so this check reads the
colour: it opens Smart home on a hidden desktop, moves the pointer over one of its
buttons (a posted WM_MOUSEMOVE, which the tooltip control follows), captures the
tooltip that opens and compares the most common colour of the capture with the
theme's InverseSurface.

    py -3 scripts/md3/check-tooltips.py --exe <bambu-studio.exe> --datadir <dir>
        --tuple en-light-comfortable --out <folder> [--po bbl/i18n/yue_HK/BambuStudio_yue_HK.po]

Writes <out>/tooltip--<tuple>.png and <out>/tooltips--<tuple>.json; exits 1 when
no tooltip opened or its colour is not the Material one.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('sweep_dialogs', os.path.join(HERE, 'sweep-dialogs.py'))
sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sweep)
cheap, send, wait_for = sweep.cheap, sweep.send, sweep.wait_for

INVERSE_SURFACE = {'light': (0x2F, 0x30, 0x36), 'dark': (0xE3, 0xE2, 0xE9)}
TOOLTIP_CLASS = 'tooltips_class32'
WM_MOUSEMOVE = 0x0200


def post(desktop, hwnd, msg, wparam=0, lparam=0):
    # Window messages do not cross desktops: post from a helper on the hidden one.
    cheap('launch_on_headless_desktop', name=desktop,
          command=f'"{sys.executable}" "{os.path.abspath(__file__)}" --post {hwnd} {msg} {wparam} {lparam}')


def dominant_colour(png):
    from PIL import Image
    with Image.open(png) as image:
        colours = image.convert('RGB').getcolors(maxcolors=1 << 20)
    return max(colours)[1] if colours else None


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--datadir', required=True)
    ap.add_argument('--tuple', required=True, dest='tuple_id')
    ap.add_argument('--out', required=True)
    ap.add_argument('--po', default=None)
    ap.add_argument('--desktop', default='bstips')
    ap.add_argument('--startup-timeout', type=float, default=240)
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding='utf-8')
    os.makedirs(args.out, exist_ok=True)
    staging = os.path.join(os.environ.get('TEMP', os.path.expanduser('~')), 'bbtip')
    os.makedirs(staging, exist_ok=True)
    theme = 'dark' if '-dark-' in args.tuple_id else 'light'
    cantonese = sweep.load_po(args.po)
    exe_sha = hashlib.sha256(open(args.exe, 'rb').read()).hexdigest()
    os.environ['BAMBU_LAYOUT_PROBE'] = '1'
    os.environ['BAMBU_LAYOUT_PROBE_TAG'] = f'tooltips--{args.tuple_id}'

    cheap('create_headless_desktop', name=args.desktop)
    owned, row = set(), {'surface': 'Smart home'}
    try:
        launched = cheap('launch_on_headless_desktop', name=args.desktop,
                         command=f'"{args.exe}" --datadir "{args.datadir}"')['pid']
        owned.add(launched)

        def windows():
            return cheap('list_headless_windows', name=args.desktop)['windows']
        frame = wait_for(lambda: next((w for w in windows() if w['class'] == 'wxWindowNR'
                                       and w['width'] >= 1000 and w['height'] >= 600), None), args.startup_timeout, step=1.0)
        if not frame:
            raise SystemExit('timed out waiting for the main frame')
        owned.add(frame['process_id'])
        main_hwnd = frame['handle']
        time.sleep(10)  # first layout, plugin prompt, bilingual decoration

        before = {w['handle'] for w in windows()}
        form = (sweep.cantonese_form('Smart home', cantonese) if cantonese else None) or 'Smart home'
        send(args.desktop, main_hwnd, command=f'invoke {form}')
        dialog = wait_for(lambda: next((w for w in windows() if w['handle'] not in before and w['class'] == '#32770'), None), 15)
        if not dialog:
            raise SystemExit('Smart home did not open')
        time.sleep(5)
        staged = os.path.join(staging, f'{frame["process_id"]}.jsonl')
        if os.path.exists(staged):
            os.remove(staged)
        send(args.desktop, main_hwnd, dump=staged)
        if not wait_for(lambda: os.path.exists(staged) and
                        open(staged, encoding='utf-8').read().rstrip().endswith('{"kind":"end"}'), 30):
            raise SystemExit('no probe dump')
        records = [json.loads(line) for line in open(staged, encoding='utf-8') if line.strip()]
        # Smart home's action buttons all carry their label as tooltip; the footer's Close is the last one.
        buttons = [r for r in records if r.get('kind') == 'window' and r.get('top') == dialog['handle']
                   and r.get('type') == 'Button' and r.get('shown') and r.get('label')]
        if not buttons:
            raise SystemExit('no labelled button in Smart home')
        button = buttons[-1]
        row['button'] = button.get('label')
        x, y = button['rect']['w'] // 2, button['rect']['h'] // 2
        tips_before = {w['handle']: (w['width'], w['height']) for w in windows() if w['class'] == TOOLTIP_CLASS}
        for _ in range(3):
            post(args.desktop, button['hwnd'], WM_MOUSEMOVE, 0, (y << 16) | x)
            time.sleep(0.3)

        def shown_tip():
            for w in windows():
                if w['class'] == TOOLTIP_CLASS and w['width'] > 8 and w['height'] > 8 and w.get('visible', True) \
                        and tips_before.get(w['handle']) != (w['width'], w['height']):
                    return w
            return None
        tip = wait_for(shown_tip, 6)
        if not tip:
            row['result'] = 'no tooltip opened'
        else:
            time.sleep(0.5)
            png = os.path.join(args.out, f'tooltip--{args.tuple_id}.png')
            cheap('screenshot', hwnd=tip['handle'], output_path=png)
            colour = dominant_colour(png)
            expected = INVERSE_SURFACE[theme]
            distance = max(abs(a - b) for a, b in zip(colour, expected)) if colour else 999
            row.update({'screenshot': png, 'size': [tip['width'], tip['height']], 'dominant': '#%02x%02x%02x' % colour if colour else None,
                        'expected': '#%02x%02x%02x' % expected,
                        'result': 'material tooltip' if distance <= 24 else 'system tooltip'})
        print(f'  Smart home "{row.get("button")}": {row["result"]} {row.get("dominant", "")}', flush=True)
    finally:
        for pid in list(owned):
            try:
                cheap('kill_process', pid=pid, force=True)
            except SystemExit:
                pass  # already exited (the relaunch parent does)
        time.sleep(2)
        cheap('close_headless_desktop', name=args.desktop)

    report = {'tuple': args.tuple_id, 'theme': theme, 'exe': args.exe, 'exe_sha256': exe_sha,
              'route': 'cheap-lowlevel-headless', 'captured_at': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'tooltip': row}
    out_json = os.path.join(args.out, f'tooltips--{args.tuple_id}.json')
    with open(out_json, 'w', encoding='utf-8') as fh:
        json.dump(report, fh, ensure_ascii=False, indent=1)
    print(f'{row["result"]}; report {out_json}')
    return 0 if row.get('result') == 'material tooltip' else 1


if __name__ == '__main__':
    if len(sys.argv) == 6 and sys.argv[1] == '--post':
        # Helper mode, launched on the hidden desktop: post one message to one window.
        import ctypes
        hwnd, msg, wparam, lparam = (int(v, 0) for v in sys.argv[2:6])
        sys.exit(0 if ctypes.windll.user32.PostMessageW(hwnd, msg, wparam, lparam) else 1)
    sys.exit(main())
