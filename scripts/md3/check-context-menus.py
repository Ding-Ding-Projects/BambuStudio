#!/usr/bin/env python3
"""Open the context menu of real text fields in the built app and record which menu it is.

Every context menu of the app is meant to be the Material menu (MD3::PopupMenu).
A menu drawn by Windows itself has the window class "#32768"; the Material menu
is one of the app's own popup windows. So a text field whose menu request opens
a "#32768" window still shows the native Undo / Cut / Copy / Paste menu.

Like sweep-dialogs.py this runs through the lowlevel-computer-use cheap CLI on a
hidden Win32 desktop: launch the executable with an isolated --datadir and the
layout probe armed, open each surface (Smart home, and Preferences through the
caption bar's gear), find its shown text fields in a probe dump, and ask each
field for its menu twice: a right-click (a posted right button press and
release, which Windows turns into the menu request) and the keyboard request
(WM_CONTEXTMENU without a position, as the Menu key and Shift+F10 send it).
Each menu that opens is captured and dismissed.

    py -3 scripts/md3/check-context-menus.py --exe <bambu-studio.exe> --datadir <dir>
        --tuple en-light-comfortable --out <folder> [--po bbl/i18n/yue_HK/BambuStudio_yue_HK.po] [--fields 2]

Writes <out>/context-menu-<surface>-<n>-<how>--<tuple>.png and
<out>/context-menus--<tuple>.json; exits 1 when any request opened a native
menu or nothing at all.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
import shutil
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location('sweep_dialogs', os.path.join(HERE, 'sweep-dialogs.py'))
sweep = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(sweep)
cheap, send, wait_for = sweep.cheap, sweep.send, sweep.wait_for

NATIVE_MENU_CLASS = '#32768'
WM_CONTEXTMENU, WM_ACTIVATE, WM_CANCELMODE = 0x007B, 0x0006, 0x001F


def post(desktop, hwnd, msg, wparam=0, lparam=0):
    # Window messages do not cross desktops: post from a helper on the hidden one.
    cheap('launch_on_headless_desktop', name=desktop,
          command=f'"{sys.executable}" "{os.path.abspath(__file__)}" --post {hwnd} {msg} {wparam} {lparam}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--datadir', required=True)
    ap.add_argument('--tuple', required=True, dest='tuple_id')
    ap.add_argument('--out', required=True)
    ap.add_argument('--po', default=None)
    ap.add_argument('--fields', type=int, default=2, help='text fields to try per surface')
    ap.add_argument('--desktop', default='bsctx')
    ap.add_argument('--startup-timeout', type=float, default=240)
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding='utf-8')
    os.makedirs(args.out, exist_ok=True)
    staging = os.path.join(os.environ.get('TEMP', os.path.expanduser('~')), 'bbctx')
    os.makedirs(staging, exist_ok=True)
    cantonese = sweep.load_po(args.po)
    exe_sha = hashlib.sha256(open(args.exe, 'rb').read()).hexdigest()
    os.environ['BAMBU_LAYOUT_PROBE'] = '1'
    os.environ['BAMBU_LAYOUT_PROBE_TAG'] = f'context-menus--{args.tuple_id}'

    cheap('create_headless_desktop', name=args.desktop)
    owned, results = set(), []
    state = {}

    def windows():
        return [w for w in cheap('list_headless_windows', name=args.desktop)['windows'] if w['process_id'] == state['pid']]

    def start():
        launched = cheap('launch_on_headless_desktop', name=args.desktop,
                         command=f'"{args.exe}" --datadir "{args.datadir}"')['pid']
        owned.add(launched)

        def main_frame():
            for w in cheap('list_headless_windows', name=args.desktop)['windows']:
                if w['class'] == 'wxWindowNR' and w['width'] >= 1000 and w['height'] >= 600:
                    return w
            return None
        frame = wait_for(main_frame, args.startup_timeout, step=1.0)
        if not frame:
            raise SystemExit('timed out waiting for the main frame')
        owned.add(frame['process_id'])
        state.update(pid=frame['process_id'], main=frame['handle'])
        time.sleep(10)  # first layout, plugin prompt, bilingual decoration

    def stop():
        for pid in list(owned):
            try:
                cheap('kill_process', pid=pid, force=True)
            except SystemExit:
                pass  # already exited (the relaunch parent does)
            owned.discard(pid)
        time.sleep(2)

    def open_surface(surface):
        before = {w['handle'] for w in windows()}
        if surface == 'gear':
            cheap('mouse_click', hwnd=state['main'], x=sweep.GEAR[0], y=sweep.GEAR[1])
        else:
            form = (sweep.cantonese_form(surface, cantonese) if cantonese else None) or surface
            send(args.desktop, state['main'], command=f'invoke {form}')
        # The dialog itself, not a popup it brings along (the gear shows a small
        # "panel" before Preferences).
        return wait_for(lambda: next((w for w in windows() if w['handle'] not in before
                                      and w['class'] == '#32770'), None), 15)

    def text_fields(dialog):
        staged = os.path.join(staging, f'{state["pid"]}-{len(results)}.jsonl')
        if os.path.exists(staged):
            os.remove(staged)
        send(args.desktop, state['main'], dump=staged)
        if not wait_for(lambda: os.path.exists(staged) and
                        open(staged, encoding='utf-8').read().rstrip().endswith('{"kind":"end"}'), 30):
            return []
        records = [json.loads(line) for line in open(staged, encoding='utf-8') if line.strip()]
        return [r for r in records if r.get('kind') == 'window' and r.get('top') == dialog['handle']
                and r.get('class') == 'wxTextCtrl' and r.get('shown') and r.get('on_screen', True)
                and r['rect']['w'] >= 20 and r['rect']['h'] >= 10]

    try:
        start()
        for surface in ('Smart home', 'gear'):
            dialog = open_surface(surface)
            if not dialog:
                results.append({'surface': surface, 'result': 'surface did not open'})
                print(f'  {surface}: did not open', flush=True)
                continue
            time.sleep(5)
            fields = text_fields(dialog)[:args.fields]
            print(f'  {surface}: "{dialog.get("title", "")}", {len(fields)} text field(s) tried', flush=True)
            for n, field in enumerate(fields, 1):
                for how in ('right-click', 'keyboard'):
                    before = {w['handle'] for w in windows()}
                    if how == 'right-click':
                        cheap('mouse_click', hwnd=field['hwnd'], x=min(12, field['rect']['w'] - 2),
                              y=field['rect']['h'] // 2, button='right')
                    else:
                        post(args.desktop, field['hwnd'], WM_CONTEXTMENU, field['hwnd'], 0xFFFFFFFF)
                    popup = wait_for(lambda: next((w for w in windows() if w['handle'] not in before
                                                   and w['width'] >= 40 and w['height'] >= 30), None), 6)
                    row = {'surface': surface, 'field': field.get('name', ''), 'hwnd': field['hwnd'], 'how': how}
                    if not popup:
                        row['result'] = 'no menu opened'
                    else:
                        time.sleep(1.0)
                        png = os.path.join(args.out, f'context-menu-{sweep.slug(surface)}-{n}-{how}--{args.tuple_id}.png')
                        cheap('screenshot', hwnd=popup['handle'], output_path=png)
                        native = popup['class'] == NATIVE_MENU_CLASS
                        row.update({'result': 'native menu' if native else 'material menu', 'class': popup['class'],
                                    'size': [popup['width'], popup['height']], 'screenshot': png})
                        gone = lambda: all(w['handle'] != popup['handle'] for w in windows())
                        # A Material menu closes when it is deactivated; a native one when
                        # its owner cancels the mode. Either way, a menu that stays costs a restart.
                        post(args.desktop, popup['handle'] if not native else dialog['handle'],
                             WM_ACTIVATE if not native else WM_CANCELMODE)
                        if not wait_for(gone, 5):
                            row['close'] = 'still open; app restarted'
                            results.append(row)
                            print(f'    field {n} {how}: {row["result"]} ({row.get("class")}); restarting', flush=True)
                            stop()
                            start()
                            dialog = open_surface(surface)
                            if not dialog:
                                break
                            time.sleep(5)
                            continue
                    results.append(row)
                    print(f'    field {n} {how}: {row["result"]} ({row.get("class", "-")})', flush=True)
            if dialog:
                send(args.desktop, state['main'], command=f'close {dialog["handle"]}')
                time.sleep(2)
    finally:
        stop()
        cheap('close_headless_desktop', name=args.desktop)

    report = {'tuple': args.tuple_id, 'exe': args.exe, 'exe_sha256': exe_sha, 'route': 'cheap-lowlevel-headless',
              'captured_at': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'requests': results}
    out_json = os.path.join(args.out, f'context-menus--{args.tuple_id}.json')
    with open(out_json, 'w', encoding='utf-8') as fh:
        json.dump(report, fh, ensure_ascii=False, indent=1)
    tried = [r for r in results if 'how' in r]
    material = [r for r in tried if r['result'] == 'material menu']
    print(f'{len(material)} of {len(tried)} menu requests opened the Material menu; report {out_json}')
    return 0 if tried and len(material) == len(tried) else 1


if __name__ == '__main__':
    if len(sys.argv) == 6 and sys.argv[1] == '--post':
        # Helper mode, launched on the hidden desktop: post one message to one window.
        import ctypes
        hwnd, msg, wparam, lparam = (int(v, 0) for v in sys.argv[2:6])
        sys.exit(0 if ctypes.windll.user32.PostMessageW(hwnd, msg, wparam, lparam) else 1)
    sys.exit(main())
