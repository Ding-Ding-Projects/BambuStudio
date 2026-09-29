#!/usr/bin/env python3
"""Open every menu-reachable dialog of the built app and report clipped text.

Runs through the lowlevel-computer-use cheap CLI on a hidden Win32 desktop,
like capture-tuple.py: launch the executable with an isolated --datadir and
the layout probe armed, then for each entry send "invoke <menu label>" (or a
probe command such as "config-wizard"), wait for the new top-level window,
capture it with PrintWindow, write a layout-probe dump, close it with
"close <hwnd>" and move on. Nothing touches the visible desktop.

    py -3 scripts/md3/sweep-dialogs.py --exe <bambu-studio.exe> --datadir <dir>
        --tuple bilingual_en_yue_HK-light-comfortable --out <folder>
        [--po bbl/i18n/yue_HK/BambuStudio_yue_HK.po] [--only "Keyboard Shortcuts,About"]

Writes <out>/dialog-<slug>--<tuple>.png, <out>/probe/dialog-<slug>--<tuple>.jsonl
and <out>/sweep--<tuple>.json. A finding is a shown window inside the dialog
whose record says text_clipped, truncated (builds from 4dc449e62 on),
hint_clipped (a placeholder hint wider than its empty entry), clipped_by_parent
or starved; on a build without "truncated", a window with a
label that is more than 2 px narrower than its own best width is reported as
suspect_shortened (the kit Button that may shrink caches its full label width
as its best size). With --po, a label that opens nothing in English is tried
again in its Cantonese form, which is what the menus show in Cantonese mode.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time

DEFAULT_CHEAP = os.path.join(os.path.expanduser('~'), 'Documents', 'GitHub', 'lowlevel-computer-use-mcp', '.venv', 'Scripts', 'lowlevel-computer-use-cheap.exe')
CHEAP = os.environ.get('LLCU_CHEAP', DEFAULT_CHEAP)
HERE = os.path.dirname(os.path.abspath(__file__))
SENDER = os.path.join(HERE, 'send-layout-probe.py')

# Menu items that open an in-app dialog (not a file picker, browser or new
# window). "probe:" entries are layout-probe commands; "gear" clicks the
# caption bar's Preferences button like capture-tuple.py does.
ENTRIES = [
    'Keyboard Shortcuts', 'Show Tip of the Day', "What's new / Changelog", 'About',
    'Config profiles & backup', 'AI filament scanner', 'Smart home', 'Model Creator',
    'Open Network Test', 'Version history', 'Check for Update',
    # "Flow rate" is a submenu whose items (Coarse, Fine) put test objects on the
    # plate and open no dialog; "Max flowrate" and "VFA" open the calibration
    # dialogs whose fields carry the widest units (mm³/s, mm/s).
    'Temperature', 'Pressure advance', 'Retraction test', 'Max flowrate', 'VFA',
    'Export preferences', 'Export object list', 'Export print statistics',
    'probe:config-wizard', 'gear',
]
GEAR = (1155, 121)  # client coordinates in the 1200x800 main frame the datadirs configure


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


def windows_of(desktop, pid):
    return [w for w in cheap('list_headless_windows', name=desktop)['windows'] if w['process_id'] == pid]


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


def slug(text):
    return re.sub(r'[^a-z0-9]+', '-', text.lower()).strip('-') or 'entry'


def load_po(path):
    """msgid -> msgstr for single-line entries (menu labels are single-line)."""
    table = {}
    if not path:
        return table
    text = open(path, encoding='utf-8').read()
    for m in re.finditer(r'^msgid "((?:[^"\\]|\\.)*)"\r?\nmsgstr "((?:[^"\\]|\\.)*)"', text, re.M):
        if m.group(2):
            table[m.group(1)] = m.group(2)
    return table


def menu_text(label):
    return label.replace('&', '').split('\\t')[0].rstrip('. …')


def findings_in(dump_path, dialog_hwnd):
    found, has_truncated_field = [], False
    for line in open(dump_path, encoding='utf-8'):
        try:
            r = json.loads(line)
        except json.JSONDecodeError:
            continue
        if r.get('kind') != 'window' or r.get('top') != dialog_hwnd or not r.get('shown'):
            continue
        has_truncated_field = has_truncated_field or 'truncated' in r
        flags = [f for f in ('text_clipped', 'truncated', 'hint_clipped', 'clipped_by_parent', 'starved') if r.get(f)]
        if 'truncated' not in r and r.get('label') and r.get('class') == 'wxWindow':
            if r['rect']['w'] + 2 < r['best']['w']:
                flags.append('suspect_shortened')
        if flags:
            found.append({'flags': flags, 'type': r.get('type', r.get('class')), 'label': r.get('label', ''),
                          'name': r.get('name', ''), 'rect': r.get('rect'), 'best': r.get('best'),
                          'hwnd': r.get('hwnd')})
    return found, has_truncated_field


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--datadir', required=True)
    ap.add_argument('--tuple', required=True, dest='tuple_id')
    ap.add_argument('--out', required=True)
    ap.add_argument('--po', default=None)
    ap.add_argument('--only', default=None, help='comma-separated subset of the entries')
    ap.add_argument('--source-commit', default='')
    ap.add_argument('--desktop', default='bsdialogs')
    ap.add_argument('--startup-timeout', type=float, default=240)
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding='utf-8')  # dialog titles can be Cantonese
    probe_dir = os.path.join(args.out, 'probe')
    os.makedirs(probe_dir, exist_ok=True)
    # The app writes the dump itself, and a Windows path past 260 characters fails to
    # open: the bilingual tuple's long name pushed deep output folders over the limit
    # and every bilingual dump went missing. Dump to a short folder, then move it.
    staging_dir = os.path.join(os.environ.get('TEMP', os.path.expanduser('~')), 'bbsp')
    os.makedirs(staging_dir, exist_ok=True)
    entries = [e.strip() for e in args.only.split(',')] if args.only else ENTRIES
    cantonese = load_po(args.po)
    exe_sha = hashlib.sha256(open(args.exe, 'rb').read()).hexdigest()

    os.environ['BAMBU_LAYOUT_PROBE'] = '1'
    os.environ['BAMBU_LAYOUT_PROBE_TAG'] = f'dialogs--{args.tuple_id}'
    cheap('create_headless_desktop', name=args.desktop)
    owned = set()

    def start():
        """Launch the app; return (pid, main frame handle)."""
        launched = cheap('launch_on_headless_desktop', name=args.desktop,
                         command=f'"{args.exe}" --datadir "{args.datadir}"')['pid']
        owned.add(launched)
        print(f'launched pid {launched} on {args.desktop} for {args.tuple_id}', flush=True)

        # Without OpenGL 2.0 the app stages its Mesa fallback and relaunches
        # itself once, so the main frame can belong to a new process. The
        # desktop is task-owned: adopt the frame from whichever process shows it.
        def any_main_frame():
            for w in cheap('list_headless_windows', name=args.desktop)['windows']:
                if w['class'] == 'wxWindowNR' and w['width'] >= 1000 and w['height'] >= 600:
                    return w
            return None
        frame = wait_for(any_main_frame, args.startup_timeout, step=1.0)
        if not frame:
            raise SystemExit('timed out waiting for the main frame')
        owned.add(frame['process_id'])
        if frame['process_id'] != launched:
            print(f'main frame belongs to relaunched pid {frame["process_id"]}', flush=True)
        time.sleep(10)  # first layout, plugin prompt, bilingual decoration
        return frame['process_id'], frame['handle']

    def stop():
        for pid_ in list(owned):
            try:
                cheap('kill_process', pid=pid_, force=True)
            except SystemExit as gone:  # already exited (the relaunch parent does)
                print(f'  pid {pid_}: {gone}', flush=True)
            owned.discard(pid_)
        time.sleep(2)

    results = []
    try:
        pid, main_hwnd = start()
        for entry in entries:
            before = {w['handle'] for w in windows_of(args.desktop, pid)}

            def new_window():
                for w in windows_of(args.desktop, pid):
                    if w['handle'] not in before and w['width'] >= 120 and w['height'] >= 60:
                        return w
                return None

            tried = []
            dialog = None
            if entry == 'gear':
                cheap('mouse_click', hwnd=main_hwnd, x=GEAR[0], y=GEAR[1])
                tried.append('gear')
                dialog = wait_for(new_window, 15)
            elif entry.startswith('probe:'):
                send(args.desktop, main_hwnd, command=entry[len('probe:'):])
                tried.append(entry)
                dialog = wait_for(new_window, 20)
            else:
                forms = [entry]
                # The app's English says "ink" where the catalogue says "filament".
                if 'filament' in entry:
                    forms.append(entry.replace('filament', 'ink'))
                yue = cantonese.get(entry) or next((v for k, v in cantonese.items() if menu_text(k) == entry), None)
                if yue and menu_text(yue) != entry:
                    forms.append(menu_text(yue))
                for form in forms:
                    send(args.desktop, main_hwnd, command=f'invoke {form}')
                    tried.append(form)
                    dialog = wait_for(new_window, 12)
                    if dialog:
                        break
            row = {'entry': entry, 'tried': tried}
            if not dialog:
                row['result'] = 'no-dialog'
                print(f'  {entry}: no new window', flush=True)
                results.append(row)
                continue
            # The bilingual decorator labels a dialog on its first pass and checks the
            # settled layout on its next full sweep (about every 3 s), which can send
            # labels back and lay the dialog out again. At 4.5 s two md3-v153/v154
            # captures caught a label or a search field mid-repaint; 6.5 s is two sweeps.
            time.sleep(6.5)
            dialog = next((w for w in windows_of(args.desktop, pid) if w['handle'] == dialog['handle']), dialog)
            name = f'dialog-{slug(entry)}--{args.tuple_id}'
            png = os.path.join(args.out, name + '.png')
            dump = os.path.join(probe_dir, name + '.jsonl')
            staged = os.path.join(staging_dir, f'{pid}-{len(results)}.jsonl')
            if os.path.exists(staged):
                os.remove(staged)
            shot = cheap('screenshot', hwnd=dialog['handle'], output_path=png)
            send(args.desktop, main_hwnd, dump=staged)
            dumped = wait_for(lambda: os.path.exists(staged) and os.path.getsize(staged) > 0 and
                              open(staged, encoding='utf-8').read().rstrip().endswith('{"kind":"end"}'), 30)
            if dumped:
                shutil.move(staged, dump)
            found, has_truncated = findings_in(dump, dialog['handle']) if dumped else ([], False)
            row.update({'result': 'opened', 'title': dialog.get('title', ''), 'class': dialog.get('class'),
                        'size': [dialog['width'], dialog['height']], 'screenshot': png,
                        'rendered_ok': shot.get('rendered_ok'), 'dump': dump if dumped else None,
                        'has_truncated_field': has_truncated, 'findings': found})
            if dumped:
                print(f'  {entry}: "{row["title"]}" {row["size"]} findings {len(found)}', flush=True)
            else:
                # No dump means no measurement, which is not the same as nothing found.
                print(f'  {entry}: "{row["title"]}" {row["size"]} NO PROBE DUMP (findings unknown)', flush=True)
            def closed():
                return all(w['handle'] != dialog['handle'] for w in windows_of(args.desktop, pid))
            send(args.desktop, main_hwnd, command=f'close {dialog["handle"]}')
            if not wait_for(closed, 8):
                # Some dialogs ignore the probe's close; a plain WM_CLOSE reaches
                # their own close handler. Posted from the hidden desktop, since
                # window messages do not cross desktops.
                cheap('launch_on_headless_desktop', name=args.desktop,
                      command=f'"{sys.executable}" "{os.path.abspath(__file__)}" --post-close {dialog["handle"]}')
                if wait_for(closed, 8):
                    row['close'] = 'closed by WM_CLOSE'
                else:
                    # Still open: restart the app rather than lose the rest of the sweep.
                    row['close'] = 'still open; app restarted'
                    print(f'  {entry}: window did not close; restarting the app', flush=True)
                    stop()
                    pid, main_hwnd = start()
            results.append(row)
            time.sleep(1.0)
    finally:
        stop()
        cheap('close_headless_desktop', name=args.desktop)
    report = {'tuple': args.tuple_id, 'exe': args.exe, 'exe_sha256': exe_sha, 'source_commit': args.source_commit,
              'captured_at': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()), 'route': 'cheap-lowlevel-headless',
              'dialogs': results}
    out_json = os.path.join(args.out, f'sweep--{args.tuple_id}.json')
    with open(out_json, 'w', encoding='utf-8') as fh:
        json.dump(report, fh, ensure_ascii=False, indent=1)
    opened = [r for r in results if r.get('result') == 'opened']
    flagged = [r for r in opened if r['findings']]
    unmeasured = [r for r in opened if r.get('dump') is None]
    print(f'{len(opened)} dialogs opened, {len(flagged)} with findings, {len(unmeasured)} without a probe dump; report {out_json}')
    return 1 if unmeasured else 0


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--post-close':
        # Helper mode, launched on the hidden desktop: post WM_CLOSE to one window.
        import ctypes
        sys.exit(0 if ctypes.windll.user32.PostMessageW(int(sys.argv[2], 0), 0x0010, 0, 0) else 1)
    sys.exit(main())
