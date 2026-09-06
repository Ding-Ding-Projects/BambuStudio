#!/usr/bin/env python3
"""Retake the captures listed in docs/screenshots/recapture-manifest.json.

Drives the built executable on a hidden Win32 desktop through the
lowlevel-computer-use cheap CLI, one process per tuple, and executes each
row's recipe:

  page        run the steps, PrintWindow the surface's top-level window
  crop-probe  run the steps, ask the layout probe for a dump, find the control
              whose label or name matches, crop the page capture to it (+pad)
  crop-gl     not handled here (needs the GL item rectangles); reported as skipped
  pages       handled by ui-md3/scripts/capture-site.mjs; reported as skipped
  historical  kept as-is; reported as skipped

Steps understood: nav:<Tab label>, open:Preferences, open:<Dialog title or
menu-item label>, tab:<Preferences sidebar label>, click:<label>, menu:<label>,
key:<combo>, resize:WxH, scroll:end, arm:<label>, launch:<mode>, trigger:<what>,
wizard-page:<n>, upload:<what>. A step the runner cannot perform marks the row
"blocked: <step>" in the report and leaves the file untouched.

    py -3 scripts/md3/recapture.py --exe <bambu-studio.exe> --datadir-root <root>
        [--only <substring>] [--report <path>] [--desktop bsrecap]

The report (JSON) lists every attempted row with done / blocked / failed and
the reason; the manifest's statuses are updated for rows that succeeded, with
the run's provenance recorded once.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
MANIFEST = os.path.join(REPO, 'docs', 'screenshots', 'recapture-manifest.json')
DEFAULT_CHEAP = os.path.join(os.path.expanduser('~'), 'Documents', 'GitHub', 'lowlevel-computer-use-mcp', '.venv', 'Scripts', 'lowlevel-computer-use-cheap.exe')
CHEAP = os.environ.get('LLCU_CHEAP', DEFAULT_CHEAP)

TAB_LABELS = {'en': {'home': 'Home', 'prepare': 'Prepare', 'preview': 'Preview', 'device': 'Device', 'project': 'Project', 'ink': 'Ink'}}
GEAR = (1155, 121)
PREF_CLOSE = (753, 21)
PREF_TABS = {'appearance': (79, 75), 'general': (79, 120), 'user': (79, 166), '3d': (79, 212), 'other': (79, 258)}
# Surfaces that are the main frame itself.
MAIN_SURFACES = {'main', 'home', 'prepare', 'preview', 'device', 'project', 'ink', 'toast', 'menu'}
# Surface -> predicate on a top-level window record from the probe / window list.
DIALOG_SURFACES = {
    'preferences': lambda w: w['class'] == '#32770' and w['width'] >= 700 and w['height'] >= 560,
    'config-wizard': lambda w: w['class'] == '#32770' and w['width'] >= 900,
    'regex-builder': lambda w: w['class'] == 'wxWindowNR' and 300 <= w['width'] <= 700 and w['height'] >= 300,
}


def cheap(tool, **kw):
    args = [CHEAP, tool]
    for k, v in kw.items():
        args += [f'--{k}', v if isinstance(v, str) else json.dumps(v)]
    out = subprocess.run(args, capture_output=True, text=True, timeout=180)
    try:
        data = json.loads(out.stdout)
    except json.JSONDecodeError as e:
        raise RuntimeError(f'{tool}: unreadable reply: {out.stdout[:200]} {out.stderr[:200]}') from e
    if not data.get('ok'):
        raise RuntimeError(f'{tool}: {data}')
    return data


class App:
    def __init__(self, exe, datadir, desktop, probe_dir):
        self.exe, self.datadir, self.desktop, self.probe_dir = exe, datadir, desktop, probe_dir
        self.pid = None
        self.main = None
        self.n = 0

    def start(self, timeout=240):
        # The probe is gated on this variable; the cheap CLI launch inherits it.
        os.environ['BAMBU_LAYOUT_PROBE'] = '1'
        os.environ['BAMBU_LAYOUT_PROBE_TAG'] = os.path.basename(self.datadir)
        cheap('create_headless_desktop', name=self.desktop)
        self.pid = cheap('launch_on_headless_desktop', name=self.desktop, command=f'"{self.exe}" --datadir "{self.datadir}"')['pid']
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            w = self.find(lambda w: w['class'] == 'wxWindowNR' and w['width'] >= 1000 and w['height'] >= 600)
            if w:
                self.main = w['handle']
                time.sleep(8)
                return
            time.sleep(1)
        raise RuntimeError('main frame did not appear')

    def stop(self):
        if self.pid:
            try:
                cheap('kill_process', pid=self.pid, force=True)
            except RuntimeError:
                pass
        time.sleep(2)
        cheap('close_headless_desktop', name=self.desktop)

    def windows(self):
        return [w for w in cheap('list_headless_windows', name=self.desktop)['windows'] if w['process_id'] == self.pid]

    def find(self, pred):
        for w in self.windows():
            if pred(w):
                return w
        return None

    def wait(self, pred, timeout, what):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            w = self.find(pred)
            if w:
                return w
            time.sleep(0.5)
        raise RuntimeError(f'no window for {what}')

    def probe(self):
        self.n += 1
        path = os.path.join(self.probe_dir, f'recapture-{self.pid}-{self.n}.jsonl')
        # The sender must live on the hidden desktop (IsWindow fails across
        # desktops), so it is launched there through the cheap CLI.
        sender = os.path.join(HERE, 'send-layout-probe.py')
        cheap('launch_on_headless_desktop', name=self.desktop, command=f'"{sys.executable}" "{sender}" {self.main} "{path}" --timeout 20')
        # The app streams the dump while we poll, so a non-empty file is not a
        # finished one: the first read came back with a torn last line and
        # every row failed on a JSON delimiter. Wait until the size has held
        # still, then parse; a torn tail on a still-growing file is retried.
        # A file made of whole lines parses fine while still short (the first
        # fix here accepted one that ended before the tab strip), so completion
        # is the probe's own end record when the build writes one, else a size
        # that has held still for three polls.
        deadline = time.monotonic() + 30
        history = []
        while time.monotonic() < deadline:
            size = os.path.getsize(path) if os.path.exists(path) else 0
            history.append(size)
            if size > 0:
                try:
                    records = []
                    with open(path, encoding='utf-8') as fh:
                        for line in fh:
                            line = line.strip()
                            if line:
                                records.append(json.loads(line))
                    if records and records[-1].get('kind') == 'end':
                        return records
                    if len(history) >= 3 and history[-1] == history[-2] == history[-3]:
                        return records
                except json.JSONDecodeError:
                    pass
            time.sleep(0.5)
        raise RuntimeError('layout probe produced no complete dump (is this build carrying the probe?)')

    def click(self, hwnd, x, y, settle=1.5):
        cheap('mouse_click', hwnd=hwnd, x=int(x), y=int(y))
        time.sleep(settle)

    def shot(self, hwnd, path):
        cheap('screenshot', hwnd=hwnd, output_path=path)
        if os.path.getsize(path) < 2000:
            raise RuntimeError(f'{path}: not a rendered frame')


def norm(s):
    return ' '.join(str(s or '').replace('&', '').split()).strip().lower()


def find_control(records, label, toplevel_hwnd=None):
    """The shown window whose label or name matches, preferring an exact match and then the smallest one.

    Probe window records carry `top` (the owning top-level's handle), `rect`
    (parent-relative) and `screen` (absolute). Matching uses label and name;
    the returned record gets an `image_rect` in the top-level window's image
    coordinates, which is what PrintWindow captures and what a click on that
    top-level expects.
    """
    wanted = norm(label)
    best = None
    tops = {r['hwnd']: r for r in records if r.get('kind') == 'toplevel'}
    for r in records:
        if r.get('kind') != 'window' or not r.get('shown', True):
            continue
        if toplevel_hwnd is not None and r.get('top') != toplevel_hwnd:
            continue
        cands = [norm(r.get('label')), norm(r.get('name'))]
        exact = wanted in cands
        contains = any(wanted and wanted in c for c in cands)
        if not (exact or contains):
            continue
        screen = r.get('screen')
        if not screen or screen['w'] <= 0 or screen['h'] <= 0:
            continue
        area = screen['w'] * screen['h']
        score = (0 if exact else 1, area)
        if best is None or score < best[0]:
            best = (score, r)
    if not best:
        return None
    r = best[1]
    top = tops.get(r.get('top'))
    origin = top['rect'] if top else {'x': 0, 'y': 0}
    s = r['screen']
    r['image_rect'] = {'x': s['x'] - origin['x'], 'y': s['y'] - origin['y'], 'w': s['w'], 'h': s['h']}
    return r


class Runner:
    def __init__(self, app, lang):
        self.app, self.lang = app, lang
        self.front = None  # hwnd of the dialog most recently opened

    def target_for(self, surface):
        if surface in MAIN_SURFACES:
            return self.app.main
        pred = DIALOG_SURFACES.get(surface)
        if pred:
            w = self.app.find(lambda w: pred(w) and w['title'] != '')
            if w:
                return w['handle']
        if self.front:
            return self.front
        raise RuntimeError(f'no window for surface {surface}')

    def step(self, s):
        kind, _, arg = s.partition(':')
        if kind == 'nav':
            label = arg
            records = self.app.probe()
            c = find_control(records, label, self.app.main)
            if not c:
                raise RuntimeError(f'blocked: nav {label} (no tab labelled so)')
            self.click_control(self.app.main, c)
        elif kind == 'open' and arg.lower() == 'preferences':
            self.app.click(self.app.main, *GEAR, settle=2.5)
            self.front = self.app.wait(lambda w: DIALOG_SURFACES['preferences'](w) and w['title'] != '', 20, 'Preferences')['handle']
        elif kind == 'tab':
            xy = PREF_TABS.get(arg.lower().replace(' ', '-').replace('3d', '3d'))
            if not xy:
                # Any other tabbed dialog: click the label through the probe.
                records = self.app.probe()
                c = find_control(records, arg, self.front)
                if not c:
                    raise RuntimeError(f'blocked: tab {arg}')
                self.click_control(self.front, c)
            else:
                self.app.click(self.front or self.app.main, *xy)
        elif kind == 'click':
            records = self.app.probe()
            c = find_control(records, arg, self.front) or find_control(records, arg)
            if not c:
                raise RuntimeError(f'blocked: click {arg} (no control labelled so)')
            owner = c.get('top') or (self.front or self.app.main)
            self.click_control(owner, c)
            time.sleep(1.5)
            # A click may have opened a new top-level; adopt the newest dialog.
            dlg = self.app.find(lambda w: w['class'] == '#32770' and w['title'] != '' and w['handle'] not in (self.app.main, self.front))
            if dlg:
                self.front = dlg['handle']
        elif kind == 'key':
            keys = [k.strip() for k in arg.split('+')]
            cheap('press_keys', keys=keys, hwnd=self.front or self.app.main)
            time.sleep(1.5)
        else:
            raise RuntimeError(f'blocked: {s}')

    def click_control(self, owner_hwnd, control):
        rect = control.get('image_rect')
        if rect is None:
            raise RuntimeError('control without a rectangle')
        # image_rect is in the owning top-level's window coordinates, which is
        # the frame a background click on that top-level addresses.
        self.app.click(owner_hwnd, rect['x'] + rect['w'] / 2, rect['y'] + rect['h'] / 2)

    def run(self, recipe, out_path):
        self.front = None
        for s in recipe.get('steps', []):
            self.step(s)
        time.sleep(1.5)
        target = self.target_for(recipe['surface'])
        if recipe['kind'] == 'page':
            self.app.shot(target, out_path)
            return 'done'
        if recipe['kind'] == 'crop-probe':
            page = out_path + '.page.png'
            self.app.shot(target, page)
            records = self.app.probe()
            c = find_control(records, recipe['label'], target)
            if not c:
                os.remove(page)
                raise RuntimeError(f"blocked: no control labelled '{recipe['label']}' on {recipe['surface']}")
            r = c['image_rect']; pad = int(recipe.get('pad', 8))
            cheap('crop_image', input_path=page, left=max(0, r['x'] - pad), top=max(0, r['y'] - pad), width=r['w'] + 2 * pad, height=r['h'] + 2 * pad, output_path=out_path)
            os.remove(page)
            return 'done'
        if recipe['kind'] == 'crop-gl':
            # GL toolbar / gizmo rail items come from the probe's gl_item
            # records: name, host canvas handle, and screen rectangle. The crop
            # is taken from the main frame's capture in its image coordinates.
            page = out_path + '.page.png'
            self.app.shot(self.app.main, page)
            records = self.app.probe()
            tops = {r['hwnd']: r for r in records if r.get('kind') == 'toplevel'}
            wanted = recipe['item']
            items = [r for r in records if r.get('kind') == 'gl_item']
            if wanted.startswith('*'):
                # A whole toolbar or rail: the union of its items.
                key = 'gizmo' if 'rail' in wanted else ('main' if 'toolbar' in wanted else None)
                group = [r for r in items if key is None or r['toolbar'] == key]
                if not group:
                    os.remove(page)
                    raise RuntimeError(f'blocked: no gl_item records for {wanted}')
                xs = [r['screen']['x'] for r in group]; ys = [r['screen']['y'] for r in group]
                xe = [r['screen']['x'] + r['screen']['w'] for r in group]; ye = [r['screen']['y'] + r['screen']['h'] for r in group]
                s = {'x': min(xs), 'y': min(ys), 'w': max(xe) - min(xs), 'h': max(ye) - min(ys)}
            else:
                match = [r for r in items if norm(r.get('name')) == norm(wanted)] or [r for r in items if norm(wanted) in norm(r.get('name'))]
                if not match:
                    os.remove(page)
                    raise RuntimeError(f"blocked: no gl_item named '{wanted}' (have: {', '.join(sorted(set(r['name'] for r in items)))})")
                s = match[0]['screen']
            origin = tops.get(self.app.main, {}).get('rect', {'x': 0, 'y': 0})
            pad = int(recipe.get('pad', 8))
            cheap('crop_image', input_path=page, left=max(0, s['x'] - origin['x'] - pad), top=max(0, s['y'] - origin['y'] - pad), width=s['w'] + 2 * pad, height=s['h'] + 2 * pad, output_path=out_path)
            os.remove(page)
            return 'done'
        raise RuntimeError(f"blocked: kind {recipe['kind']}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--datadir-root', required=True)
    ap.add_argument('--only', default=None)
    ap.add_argument('--report', default=os.path.join(REPO, 'artifacts', 'recapture-report.json'))
    ap.add_argument('--desktop', default='bsrecap')
    ap.add_argument('--probe-dir', default=os.path.join(REPO, 'artifacts', 'probe'))
    ap.add_argument('--commit', default=None, help='source commit of the executable, recorded in the manifest provenance')
    args = ap.parse_args()
    os.makedirs(args.probe_dir, exist_ok=True)
    os.makedirs(os.path.dirname(args.report), exist_ok=True)
    manifest = json.load(open(MANIFEST, encoding='utf-8'))
    rows = [r for r in manifest['captures'] if r['recipe'] and r['recipe']['kind'] in ('page', 'crop-probe')]
    if args.only:
        rows = [r for r in rows if args.only in r['file']]
    # Group by tuple so each tuple is one process.
    by_tuple = {}
    for r in rows:
        t = r['recipe'].get('tuple', {})
        key = f"{t.get('language', 'en')}-{t.get('theme', 'light')}-{t.get('density', 'comfortable')}"
        by_tuple.setdefault(key, []).append(r)
    report = {'exe': args.exe, 'commit': args.commit, 'started': time.strftime('%Y-%m-%dT%H:%M:%S'), 'rows': []}
    dll = os.path.join(os.path.dirname(args.exe), 'BambuStudio.dll')
    if os.path.exists(dll):
        report['dll_sha256'] = hashlib.sha256(open(dll, 'rb').read()).hexdigest()
    for tuple_id, trows in by_tuple.items():
        datadir = os.path.join(args.datadir_root, tuple_id)
        if not os.path.isdir(datadir):
            for r in trows:
                report['rows'].append({'file': r['file'], 'status': f'blocked: no datadir for {tuple_id}'})
            continue
        app = App(args.exe, datadir, args.desktop, args.probe_dir)
        print(f'== {tuple_id}: {len(trows)} rows')
        try:
            app.start()
            runner = Runner(app, tuple_id.split('-')[0])
            for r in trows:
                out_path = os.path.join(REPO, r['file'])
                try:
                    status = runner.run(r['recipe'], out_path)
                    r['status'] = 'done'
                except Exception as e:  # noqa: BLE001 - every row reports, none aborts the run
                    status = str(e) if str(e).startswith('blocked') else f'failed: {e}'
                print(f'  {status} :: {r["file"]}')
                report['rows'].append({'file': r['file'], 'status': status})
                # Return to a known state between rows: close any dialog we opened.
                if runner.front and runner.front != app.main:
                    # press_keys by handle does not reach a window on another
                    # desktop (the same wall SendMessage hit), so Escape never
                    # closed anything and the dialogs stacked up. A background
                    # click does cross: close Preferences by its caption glyph,
                    # anything else by the same corner, then forget the handle.
                    try:
                        app.click(runner.front, *PREF_CLOSE, settle=1.0)
                    except (RuntimeError, SystemExit):
                        pass
                    runner.front = None
        except Exception as e:  # noqa: BLE001
            for r in trows:
                report['rows'].append({'file': r['file'], 'status': f'failed: {e}'})
        finally:
            app.stop()
    done = sum(1 for r in report['rows'] if r['status'] == 'done')
    report['finished'] = time.strftime('%Y-%m-%dT%H:%M:%S')
    report['summary'] = {'attempted': len(report['rows']), 'done': done}
    json.dump(report, open(args.report, 'w', encoding='utf-8'), indent=2)
    if done:
        manifest['provenance'] = {'sourceCommit': args.commit, 'dllSha256': report.get('dll_sha256'), 'run': report['started']}
        json.dump(manifest, open(MANIFEST, 'w', encoding='utf-8', newline='\n'), indent=2)
    print(f"{done}/{len(report['rows'])} rows retaken; report {args.report}")
    return 0 if done == len(report['rows']) else 1


if __name__ == '__main__':
    sys.exit(main())
