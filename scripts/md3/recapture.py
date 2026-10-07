#!/usr/bin/env python3
"""Retake the captures listed in docs/screenshots/recapture-manifest.json.

Drives the built executable on a hidden Win32 desktop through the
lowlevel-computer-use cheap CLI, one process per tuple, and executes each
row's recipe:

  page        run the steps, PrintWindow the surface's top-level window
  crop-probe  run the steps, ask the layout probe for a dump, find the control
              whose label or name matches, crop the page capture to it (+pad);
              "parent": N crops to its Nth enclosing window instead
  crop-gl     not handled here (needs the GL item rectangles); reported as skipped
  pages       handled by ui-md3/scripts/capture-site.mjs; reported as skipped
  historical  kept as-is; reported as skipped

Steps understood: nav:<Tab label>, open:Preferences, open:<Dialog title or
menu-item label>, tab:<Preferences sidebar label>, click:<label>, menu:<label>,
key:<combo>, resize:WxH, scroll:end, arm:<label>, launch:<mode>, trigger:<what>,
wizard-page:<n>, upload:<what>. A step the runner cannot perform marks the row
"blocked: <step>" in the report and leaves the file untouched.

    py -3 scripts/md3/recapture.py --exe <bambu-studio.exe> --datadir-root <root>
        [--only <substring>] [--report <path>] [--desktop bsrecap] [--hosted-holder]

The report (JSON) lists every attempted row with done / blocked / failed and
the reason; the manifest's statuses are updated for rows that succeeded, with
the run's provenance recorded once. With --evidence-probe each finished row
also names the layout dump taken right after its capture.

When a tuple's application does not start, the run prints "start failed:
<type>: <message>" once, followed by the launch holder's receipt (status and
exit code) and the launcher trace lines of that launch, all redacted with the
README privacy check's patterns, because every row of the tuple fails with it.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
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
# Rail geometry at 1200x800, language-independent; the nav fallback for tuples
# whose tab labels are not English.
TABS = {'home': (90, 119), 'prepare': (211, 119), 'preview': (346, 119), 'device': (483, 119), 'project': (619, 119), 'ink': (755, 119)}
PREF_TABS = {'appearance': (79, 75), 'general': (79, 120), 'user': (79, 166), '3d': (79, 212), 'other': (79, 258)}
# Names the workspace tab strip has carried in the layout probe, newest first.
RAIL_NAMES = ('Workspace navigation', 'Navigation rail')
# Surfaces that are the main frame itself.
MAIN_SURFACES = {'main', 'home', 'prepare', 'preview', 'device', 'project', 'ink', 'toast', 'menu'}
# Surface -> predicate on a top-level window record from the probe / window list.
# Sample files for open:<file> steps.
SAMPLES = os.path.join(REPO, '.claude', 'skills', 'run-bambustudio')

# open:<name> -> substring of the menu item label the app should fire.
OPEN_ITEMS = {
    'config wizard': 'wizard',
    'config profiles': 'config profiles',
    'ink manager': 'ink manager',
    'project history': 'history',
    'about': 'about',
    'keyboard shortcuts': 'keyboard shortcuts',
    'network test': 'network test',
    'export preset bundle': 'export preset bundle',
}

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
        self.last_probe = None

    def start(self, timeout=None):
        timeout = timeout or getattr(self, 'startup_timeout', 240)
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
        self.last_probe = path
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

    def command(self, payload):
        """Send a driver command (menu-popup <Title>, invoke <label>) through the
        probe sender on the hidden desktop; the app defers the action, so this
        returns at once and the caller waits for the resulting window."""
        sender = os.path.join(HERE, 'send-layout-probe.py')
        cheap('launch_on_headless_desktop', name=self.desktop, command=f'"{sys.executable}" "{sender}" {self.main} --command "{payload}"')
        time.sleep(1.5)

    def click(self, hwnd, x, y, settle=1.5):
        cheap('mouse_click', hwnd=hwnd, x=int(x), y=int(y))
        time.sleep(settle)

    def shot(self, hwnd, path):
        cheap('screenshot', hwnd=hwnd, output_path=path)
        if os.path.getsize(path) < 2000:
            raise RuntimeError(f'{path}: not a rendered frame')


def load_script(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


_PRIVACY = []


def printable(text, width):
    """Text for a public log: the README privacy check's findings replaced by their category, then cut."""
    if not _PRIVACY:
        try:
            module = load_script('readme_privacy', os.path.join(HERE, 'check-readme-screenshot-privacy.py'))
            _PRIVACY.append((module.redact, module.literals_from_environment([])))
        except Exception:  # noqa: BLE001 - without the patterns nothing is printed
            _PRIVACY.append(None)
    if _PRIVACY[0] is None:
        return '<withheld: the privacy patterns could not be loaded>'
    redact, literals = _PRIVACY[0]
    return redact(text, literals)[:width]


def hosted_app_class():
    """HostedApp from drive-packaged-behavior.py: the launch route the hosted verifiers use.

    hosted_launch_holder.py starts the application from a process that keeps the
    hidden desktop open for the whole tuple and records the application's exit
    code, and the window search accepts a verified relaunch of it. A direct
    one-shot launch leaves nothing holding the desktop, so an early exit takes
    the desktop with it and the next call can only report that it is missing.
    """
    sys.modules.setdefault('recapture', sys.modules[__name__])
    return load_script('packaged_behavior', os.path.join(HERE, 'drive-packaged-behavior.py')).HostedApp


def launch_facts(app):
    """Short printable facts about a launch that failed: the holder's receipt and the launcher's trace."""
    facts = []
    receipt_path = getattr(app, 'holder_receipt_path', None)
    if receipt_path is not None:
        try:
            receipt = json.loads(Path(receipt_path).read_text(encoding='utf-8'))
        except (OSError, ValueError):
            receipt = None
        if isinstance(receipt, dict):
            code = receipt.get('app_exit_code')
            code = f'{code} (0x{code & 0xFFFFFFFF:08X})' if isinstance(code, int) else 'none'
            facts.append(f"holder: status={receipt.get('status')} app_exit_code={code} "
                         f"exit_confirmed={receipt.get('app_exit_confirmed')} terminated_by_holder={receipt.get('app_terminated_by_holder')}")
        else:
            facts.append('holder: no readable receipt')
        facts.append(f"startup state: {getattr(app, 'startup_state', None)}")
    # The launcher mirrors each early decision to %TEMP%\bbs-launcher-trace.log,
    # one line per decision, each tagged with its process id.
    pids = {app.pid, getattr(app, 'launch_pid', None), *getattr(app, 'seen_owned', {})} - {None}
    trace = os.path.join(os.environ.get('TEMP', ''), 'bbs-launcher-trace.log')
    try:
        with open(trace, encoding='utf-8-sig', errors='replace') as fh:
            lines = [line.strip() for line in fh if any(f' pid={pid} ' in line for pid in pids)]
    except OSError:
        lines = None
    if lines is None:
        facts.append('launcher trace: none')
    else:
        facts += [f'launcher: {line}' for line in lines[-12:]] or ['launcher trace: no line from this launch']
    return [printable(fact, 300) for fact in facts]


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
    byh = {r['hwnd']: r for r in records if r.get('kind') == 'window'}
    # A target that is not a top-level (the regex popover is a child of the
    # frame) scopes the search to its descendants and anchors image_rect to it.
    scope_win = byh.get(toplevel_hwnd) if toplevel_hwnd is not None and toplevel_hwnd not in tops else None

    def within(r):
        cur = r
        while cur is not None:
            if cur.get('parent') == toplevel_hwnd:
                return True
            cur = byh.get(cur.get('parent'))
        return False

    for r in records:
        if r.get('kind') not in ('window', 'tool') or not r.get('shown', True):
            continue
        # A shown flag says nothing about the ancestors; a control inside an
        # unmapped dialog matched 'custom colour' and produced a click at
        # x = -272. Builds since ae2273d84 record IsShownOnScreen.
        if r.get('on_screen') is False:
            continue
        if scope_win is not None:
            if r.get('kind') != 'window' or not within(r):
                continue
        elif toplevel_hwnd is not None and r.get('top') != toplevel_hwnd:
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
    origin = scope_win['screen'] if scope_win is not None else (top['rect'] if top else {'x': 0, 'y': 0})
    s = r['screen']
    r['image_rect'] = {'x': s['x'] - origin['x'], 'y': s['y'] - origin['y'], 'w': s['w'], 'h': s['h']}
    # Always keep the rectangle relative to the owning top-level as well: a
    # scoped search (rail, popover) anchors image_rect to the scope, but a
    # click is delivered to the top-level window.
    top_origin = top['rect'] if top else {'x': 0, 'y': 0}
    r['top_rect'] = {'x': s['x'] - top_origin['x'], 'y': s['y'] - top_origin['y'], 'w': s['w'], 'h': s['h']}
    return r


def enclosing_window(records, control, levels):
    """The window `levels` parents above a find_control result, in the same image frame.

    A region with no name of its own (the process settings header row is an
    unnamed panel) is reached through a named control inside it; recipes ask
    for that with "parent": <levels>.
    """
    byh = {r['hwnd']: r for r in records if r.get('kind') == 'window'}
    cur = control
    for _ in range(levels):
        cur = byh.get(cur.get('parent'))
        if cur is None or cur.get('on_screen') is False or not cur.get('screen') or cur['screen']['w'] <= 0 or cur['screen']['h'] <= 0:
            return None
    ox = control['screen']['x'] - control['image_rect']['x']
    oy = control['screen']['y'] - control['image_rect']['y']
    s = cur['screen']
    return dict(cur, image_rect={'x': s['x'] - ox, 'y': s['y'] - oy, 'w': s['w'], 'h': s['h']})


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
            # Scope to the navigation rail: 'Ink' is also the sidebar section
            # header, which is smaller and therefore won every exact match,
            # so nav:Ink collapsed the Ink section instead of switching tabs.
            # The rail was renamed 'Workspace navigation' (624e2ed52); both
            # names resolve. Once the rail is found the label must be one of
            # its tabs: falling back to the whole frame is how nav:Ink landed
            # on the sidebar's Ink header and the Prepare page was captured.
            rail = next((r for r in records if r.get('kind') == 'window' and r.get('name') in RAIL_NAMES), None)
            c = find_control(records, label, rail['hwnd'] if rail else self.app.main)
            if not c and label.lower() in TABS and self.lang != 'en':
                # Non-English tuples label the rail in that language; the rail
                # geometry is language-independent at 1200x800.
                self.app.click(self.app.main, *TABS[label.lower()], settle=2.5)
                return
            if not c:
                raise RuntimeError(f'blocked: nav {label} (no tab labelled so)')
            self.click_control(self.app.main, c)
            # The first switch to Prepare or Preview brings the GL canvas up,
            # which takes longer than a click settle; the bulk-ink rows probed
            # the sidebar while it was still off screen. Wait for a control
            # that only that page shows.
            marker = {'prepare': 'Search inks', 'preview': 'Search inks', 'device': None, 'home': None, 'project': None, 'ink': None}.get(label.lower())
            if marker:
                for _ in range(8):
                    records = self.app.probe()
                    hit = find_control(records, marker, self.app.main)
                    if hit and hit.get('on_screen', True):
                        break
                    time.sleep(1.0)
                # A collapsed Ink section (its content panel hidden under a shown
                # scroller) hides every ink row; expand it by its header.
                if label.lower() == 'prepare':
                    records = self.app.probe()
                    add = [r for r in records if r.get('kind') == 'window' and r.get('label') == 'Add ink']
                    if add and not add[0].get('on_screen', True):
                        # Scope to the sidebar: an unscoped exact 'Ink' fell
                        # through to the rail's Ink tab and navigated away.
                        sidebar = next((r for r in records if r.get('kind') == 'window' and r.get('name') == 'Sidebar'), None)
                        header = find_control(records, 'Ink', sidebar['hwnd']) if sidebar else None
                        if header and header.get('on_screen', True):
                            self.click_control(self.app.main, header)
                            time.sleep(2.0)
        elif kind == 'wizard-page':
            # The Setup Wizard is the web guide; its steps are URL targets.
            self.app.command(f'guide-page {int(arg)}')
            time.sleep(4.0)
        elif kind == 'resize':
            # Only a dialog the row opened may be resized: with nothing in
            # front this once shrank the main frame to its 1000x600 minimum
            # and every later row inherited the wrong geometry.
            if not self.front:
                raise RuntimeError('blocked: resize with no dialog in front')
            w, h = [int(v) for v in arg.lower().split('x')]
            self.app.command(f'resize {self.front} {w} {h}')
            time.sleep(2.0)
        elif kind == 'open' and arg.lower() == 'config wizard':
            # The native configuration wizard has no menu item (Help > Setup
            # Wizard is the web guide); the app runs it on this command.
            before = {w['handle'] for w in self.app.windows() if w['class'] == '#32770'}
            self.app.command('config-wizard')
            self.front = self.app.wait(lambda w: w['class'] == '#32770' and w['handle'] not in before and w['width'] >= 700, 30, 'config-wizard')['handle']
            time.sleep(2.0)
        elif kind == 'open' and arg.lower() == 'command palette':
            self.app.command('palette')
            self.front = self.app.wait(lambda w: w['class'] == '#32770' and 'palette' in w['title'].lower(), 10, 'command-palette')['handle']
        elif kind == 'invoke':
            # Fire a menu item that raises no dialog (a toast, a background check).
            self.app.command(f'invoke {OPEN_ITEMS.get(arg.lower(), arg)}')
            time.sleep(3.0)
        elif kind == 'menu':
            # Pop a top-bar menu; the popup is its own top-level window
            # (class #32768) and becomes the front for the capture.
            self.app.command(f'menu-popup {arg}')
            self.front = self.app.wait(lambda w: w['class'] == '#32768' and w['width'] > 40, 10, f'menu {arg}')['handle']
        elif kind == 'open' and arg.lower().endswith(('.stl', '.3mf', '.obj', '.step')):
            # Load a sample model; the app answers with the object in the scene.
            path = os.path.join(SAMPLES, os.path.basename(arg))
            if not os.path.exists(path):
                raise RuntimeError(f'blocked: no sample file {path}')
            self.app.command(f'load {path}')
            time.sleep(6)
        elif kind == 'trigger' and arg.lower().endswith('toast'):
            self.app.command('notify Sample notification from the capture driver')
            time.sleep(2)
        elif kind == 'scroll' and arg.lower() == 'end':
            # Scroll the front dialog's largest scrolled window to its end.
            records = self.app.probe()
            cands = [r for r in records if r.get('kind') == 'window' and r['class'] == 'wxScrolledWindow' and r.get('on_screen') and r.get('top') == (self.front or self.app.main)]
            if not cands:
                raise RuntimeError('blocked: scroll:end (no scrolled window on the front surface)')
            best = max(cands, key=lambda r: r['rect']['w'] * r['rect']['h'])
            self.app.command(f"scroll-end {best['hwnd']}")
            time.sleep(1.5)
        elif kind == 'open' and arg.lower() not in ('preferences',):
            # Fire the menu item whose label contains the name and wait for a
            # dialog whose title contains it (or any new dialog if the title
            # differs, e.g. the wizard).
            before = {w['handle'] for w in self.app.windows() if w['class'] == '#32770'}
            self.app.command(f'invoke {OPEN_ITEMS.get(arg.lower(), arg)}')
            want = arg.lower().split()[0]
            self.front = self.app.wait(lambda w: w['class'] == '#32770' and w['handle'] not in before and w['width'] >= 200 and (want in w['title'].lower() or w['title'] != ''), 20, f'dialog for {arg}')['handle']
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
            before = {w['handle'] for w in self.app.windows() if w['class'] == '#32770'}
            self.click_control(owner, c)
            # A click that opens a dialog makes it the front for the capture
            # (the colour picker, bulk actions, ...).
            for w in self.app.windows():
                if w['class'] == '#32770' and w['handle'] not in before and w['width'] >= 200 and w['title'] != '':
                    self.front = w['handle']
                    time.sleep(1.0)
                    break
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
        # Clicks go to the owning top-level, so use the rectangle relative to
        # it (a rail-scoped nav lookup otherwise clicked near the frame's
        # top-left corner and never left the Home tab).
        rect = control.get('top_rect') or control.get('image_rect')
        if rect is None:
            raise RuntimeError('control without a rectangle')
        # image_rect is in the owning top-level's window coordinates, which is
        # the frame a background click on that top-level addresses.
        self.app.click(owner_hwnd, rect['x'] + rect['w'] / 2, rect['y'] + rect['h'] / 2)

    def run(self, recipe, out_path):
        self.front = None
        if recipe.get('blocked'):
            raise RuntimeError('blocked: ' + recipe['blocked'])
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
            if recipe.get('parent'):
                c = enclosing_window(records, c, int(recipe['parent']))
                if not c:
                    os.remove(page)
                    raise RuntimeError(f"blocked: '{recipe['label']}' has no shown ancestor {recipe['parent']} up")
            r = c['image_rect']; pad = int(recipe.get('pad', 8))
            cheap('crop_image', input_path=page, left=max(0, r['x'] - pad), top=max(0, r['y'] - pad), width=r['w'] + 2 * pad, height=r['h'] + 2 * pad, output_path=out_path)
            os.remove(page)
            return 'done'
        if recipe['kind'] == 'crop-rect':
            # A fixed rectangle in the surface's image coordinates, for pages
            # the probe cannot see into (the Home webview at 1200x800).
            page = out_path + '.page.png'
            self.app.shot(target, page)
            x, y, w, h = recipe['rect']
            cheap('crop_image', input_path=page, left=int(x), top=int(y), width=int(w), height=int(h), output_path=out_path)
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
                # Recipes name items in snake_case, the toolbar in CamelCase or
                # with spaces (brim_ears vs BrimEars, mmu_painting vs Color
                # Painting): compare with case, spaces and underscores removed.
                squash = lambda v: norm(v).replace(' ', '').replace('_', '')
                match = [r for r in items if squash(r.get('name')) == squash(wanted)] or [r for r in items if squash(wanted) in squash(r.get('name'))]
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
    ap.add_argument('--match', default=None, help='regex on the row path (in addition to --only)')
    ap.add_argument('--kinds', default='page,crop-probe', help='comma-separated recipe kinds to attempt')
    ap.add_argument('--mesa', action='store_true', help='software GL: the exe must have the Mesa pair beside it; enables the canvas rows')
    ap.add_argument('--report', default=os.path.join(REPO, 'artifacts', 'recapture-report.json'))
    ap.add_argument('--desktop', default='bsrecap')
    ap.add_argument('--probe-dir', default=os.path.join(REPO, 'artifacts', 'probe'))
    ap.add_argument('--commit', default=None, help='source commit of the executable, recorded in the manifest provenance')
    ap.add_argument('--evidence-probe', action='store_true',
                    help='take one more layout dump after each finished row, while its surface is still up, and name it '
                         'in the report row as evidence_probe (text evidence for check-readme-screenshot-privacy.py)')
    ap.add_argument('--hosted-holder', action='store_true',
                    help='GitHub-hosted runners only: launch through hosted_launch_holder.py, as the hosted verifiers do, '
                         'so the hidden desktop outlives an early exit and the exit code is reported')
    args = ap.parse_args()
    app_class = hosted_app_class() if args.hosted_holder else App
    os.makedirs(args.probe_dir, exist_ok=True)
    os.makedirs(os.path.dirname(args.report), exist_ok=True)
    manifest = json.load(open(MANIFEST, encoding='utf-8'))
    kinds = set(k.strip() for k in args.kinds.split(',') if k.strip())
    rows = [r for r in manifest['captures'] if r['recipe'] and r['recipe']['kind'] in kinds]
    if args.only:
        rows = [r for r in rows if args.only in r['file']]
    if args.match:
        import re
        rows = [r for r in rows if re.search(args.match, r['file'])]
    if args.mesa:
        # Software GL renders the canvas into a DIB that PrintWindow can read,
        # so the rows blocked on the GL route are attempted here.
        os.environ['GALLIUM_DRIVER'] = 'llvmpipe'
        os.environ['MESA_GL_VERSION_OVERRIDE'] = '3.3'
        os.environ['LIBGL_ALWAYS_SOFTWARE'] = '1'
        for r in rows:
            if 'needs the Mesa route' in (r['recipe'].get('blocked') or ''):
                r['recipe'] = dict(r['recipe']); r['recipe'].pop('blocked', None)
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
        desktop = f'{args.desktop}-{os.getpid()}-{tuple_id}' if args.hosted_holder else args.desktop
        app = app_class(args.exe, datadir, desktop, args.probe_dir)
        if args.hosted_holder:
            # The holder refuses to reuse its files, so each tuple gets its own.
            files = {name: Path(args.probe_dir) / f'hosted-launch-{tuple_id}{name}' for name in ('.json', '.stop', '-stdout.log', '-stderr.log')}
            app.holder_receipt_path, app.holder_stop_path = files['.json'], files['.stop']
            app.holder_stdout_path, app.holder_stderr_path = files['-stdout.log'], files['-stderr.log']
            app.holder_lifetime = 2400  # the holder's own ceiling, inside the job's hour
        app.startup_timeout = 300 if args.mesa else 240
        print(f'== {tuple_id}: {len(trows)} rows')
        reported, started = set(), False
        try:
            try:
                app.start(timeout=app.startup_timeout)
                started = True
            except Exception as e:
                # Every row of the tuple fails with this, so say why once.
                print(f'  start failed: {type(e).__name__}: {printable(e, 400)}')
                for fact in launch_facts(app):
                    print(f'    {fact}')
                raise
            runner = Runner(app, tuple_id.split('-')[0])
            for r in trows:
                out_path = os.path.join(REPO, r['file'])
                # Known state before every row: no stray dialog (one appears
                # late after the plugin-gate row), frame back at 1200x800.
                for w in app.windows():
                    if w['class'] == '#32770' and w['width'] >= 200 and w['title'] not in ('', 'Downloading Bambu Network Plug-in'):
                        try:
                            app.command(f"close {w['handle']}")
                        except (RuntimeError, SystemExit):
                            pass
                mainw = app.find(lambda w: w['handle'] == app.main)
                if mainw and (mainw['width'], mainw['height']) != (1200, 800):
                    app.command(f'resize {app.main} 1200 800')
                try:
                    status = runner.run(r['recipe'], out_path)
                    r['status'] = 'done'
                except Exception as e:  # noqa: BLE001 - every row reports, none aborts the run
                    status = str(e) if str(e).startswith('blocked') else f'failed: {e}'
                print(f'  {status} :: {r["file"]}')
                row = {'file': r['file'], 'kind': r['recipe']['kind'], 'status': status}
                if args.evidence_probe and status == 'done':
                    # The dump records every label on screen, so it is the text
                    # evidence of what the image shows. No dump, no evidence: the
                    # privacy check then withholds the image.
                    try:
                        app.probe()
                        row['evidence_probe'] = os.path.basename(app.last_probe)
                    except Exception as e:  # noqa: BLE001 - recorded, never fatal to the run
                        row['evidence_probe'] = None
                        row['evidence_error'] = type(e).__name__
                report['rows'].append(row)
                reported.add(r['file'])
                # Return to a known state between rows: close any dialog we opened.
                if runner.front and runner.front != app.main:
                    # press_keys by handle does not reach a window on another
                    # desktop (the same wall SendMessage hit), so Escape never
                    # closed anything and the dialogs stacked up. A background
                    # click does cross: close Preferences by its caption glyph,
                    # anything else by the same corner, then forget the handle.
                    try:
                        # Menus are dismissed by a click on the frame; dialogs
                        # and popups close through the app (EndModal / Close),
                        # which works whatever their size.
                        if any(w['handle'] == runner.front and w['class'] == '#32768' for w in app.windows()):
                            app.click(app.main, 600, 400, settle=1.0)
                        else:
                            app.command(f'close {runner.front}')
                    except (RuntimeError, SystemExit):
                        pass
                    runner.front = None
                    # Anything else left open by the row (a second dialog, a
                    # popover) is closed too, so rows cannot stack windows.
                    for w in app.windows():
                        if w['class'] == '#32770' and w['handle'] != app.main and w['width'] >= 200 and w['title'] not in ('', 'Downloading Bambu Network Plug-in'):
                            try:
                                app.command(f"close {w['handle']}")
                            except (RuntimeError, SystemExit):
                                pass
        except Exception as e:  # noqa: BLE001
            if started:
                print(f'  tuple failed: {type(e).__name__}: {printable(e, 400)}')
            # Rows already reported keep their own result; a duplicate row would be withheld.
            for r in trows:
                if r['file'] not in reported:
                    report['rows'].append({'file': r['file'], 'kind': r['recipe']['kind'], 'status': f'failed: {type(e).__name__}: {e}'})
        finally:
            try:
                app.stop()
            except Exception as e:  # noqa: BLE001 - the captures are taken; the report must still be written
                print(f'  teardown failed: {type(e).__name__}: {printable(e, 400)}')
                report.setdefault('teardown_failures', []).append({'tuple': tuple_id, 'error': f'{type(e).__name__}: {e}'})
    done = sum(1 for r in report['rows'] if r['status'] == 'done')
    report['finished'] = time.strftime('%Y-%m-%dT%H:%M:%S')
    report['summary'] = {'attempted': len(report['rows']), 'done': done}
    json.dump(report, open(args.report, 'w', encoding='utf-8'), indent=2)
    if done:
        manifest['provenance'] = {'sourceCommit': args.commit, 'dllSha256': report.get('dll_sha256'), 'run': report['started']}
        # Re-read before writing: a run holds the manifest for an hour or more,
        # and recipes edited meanwhile were clobbered by this dump once (run 5).
        fresh = json.load(open(MANIFEST, encoding='utf-8'))
        fresh['provenance'] = manifest['provenance']
        json.dump(fresh, open(MANIFEST, 'w', encoding='utf-8', newline='\n'), indent=2)
    print(f"{done}/{len(report['rows'])} rows retaken; report {args.report}")
    return 0 if done == len(report['rows']) else 1


if __name__ == '__main__':
    sys.exit(main())
