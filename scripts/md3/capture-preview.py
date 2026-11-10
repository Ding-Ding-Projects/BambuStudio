#!/usr/bin/env python3
"""Capture the sliced Preview tab (and the Prepare sidebar) of the built app in one language mode.

Like capture-canvas.py this runs through the lowlevel-computer-use cheap CLI on a hidden
Win32 desktop and asks the canvas for its own frame with the layout probe's "canvas-png"
command, because PrintWindow leaves the OpenGL canvas blank on the real graphics driver.
On top of that it prepares a dual-nozzle printer, loads three copies of the sample cube on top
of each other, gives the second one the second ink and slices; the stacked cubes make the slicer
warn about colliding paths, which puts a real warning banner in the notification column. It then
arranges the cubes, slices again and saves the states below. Every name gets "--<tuple>.png"
appended.

  prepare-objects            whole window, Prepare tab, Objects page of the sidebar, before slicing
  preview-warning-canvas     Preview canvas at 1200x800 with the slicer warning banner
  preview-wide-warning-canvas        the same at 1700x900
  preview-wide-warning-toast-canvas  the same with one extra notification stacked under the warning
  prepare-canvas             Prepare canvas with the three cubes after Arrange (canvas frame)
  preview-window             whole window at 1200x800 after the second slice (the canvas is blank in it)
  preview-canvas             the Preview canvas at that size, ImGui panels included
  preview-wide-window        whole window at 1700x900
  preview-wide-canvas        the Preview canvas at that size
  preview-wide-toast-canvas  the same canvas after the probe pushed one extra notification
  preview-tall-window        whole window at 1700x1200
  preview-tall-canvas        the Preview canvas at that size, tall enough for the whole legend dock
  preview-plates-window      whole window after a second plate was added, loaded and sliced
  preview-plates-canvas      its canvas: plate strip with the All Plates Stats tile
  preview-stats-canvas       its canvas after a click on the All Plates Stats tile
  prepare-plate-settings     whole window, Process page with the plate selected (see the report)

A window capture taken after the window was resized can show chrome that was not repainted yet
(stale or blank pieces of the bottom bar), so judge layout on the canvas frames and on the
1200x800 window capture. Even an idle window is not captured repeatably: pieces of the bottom
action bar come out blank in some captures. Each window capture is therefore taken five times,
the most frequent picture is kept, and the report records how many different pictures were seen.

    py -3 scripts/md3/capture-preview.py --exe <bambu-studio.exe> --source-datadir <dir>
        --datadir <new dir> --tuple yue_HK-light-comfortable --out <folder>
        [--printer "Bambu Lab H2D 0.4 nozzle"]

--source-datadir is one of the directories prepare-capture-datadirs.py wrote. It is copied to
--datadir (replaced when it exists), the vendor presets shipped beside the executable are copied
into its system folder, and its configuration is edited to select the printer, two filament
presets and the matching process preset and to silence the first-use tip dialogs, so the
original is never touched. The data directory must have a short path: the app cannot open the
preset files when their full path passes 260 characters.

The report <out>/preview-capture--<tuple>.json lists every capture that was saved and every
state that could not be reached, each with the reason. A state the driver cannot reach is
recorded, never replaced by another picture.
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
REPO = os.path.abspath(os.path.join(HERE, '..', '..'))
SENDER = os.path.join(HERE, 'send-layout-probe.py')
CUBE = os.path.join(REPO, '.claude', 'skills', 'run-bambustudio', 'cube.stl')
PREPARE_TAB = (211, 119)  # client coordinates in the 1200x800 main frame the capture datadirs configure
NARROW = (1200, 800)
WIDE = (1700, 900)
TALL = (1700, 1200)

PRINTER = 'Bambu Lab H2D 0.4 nozzle'
PRINTER_MODEL = 'Bambu Lab H2D'
PROCESS = '0.20mm Standard @BBL H2D'
FILAMENTS = ['Bambu PLA Basic @BBL H2D', 'Bambu PLA Matte @BBL H2D']
FILAMENT_COLOURS = ['#00AE42', '#FF6A13']
# First-use tip dialogs that would otherwise stop the first slice on a dual-nozzle printer.
TIP_FLAGS = ['show_wrapping_detect_dialog', 'show_support_recommend_dialog', 'show_fila_switch_tips',
             'play_tpu_printing_video', 'prompt_for_brittle_filaments', 'show_daily_tips',
             'play_slicing_video_model_Bambu_Lab_H2D', 'play_slicing_video_model_Bambu_Lab_H2D_Pro',
             'play_slicing_video_model_Bambu_Lab_H2C']
TIME_TEXT = re.compile(r'^\s*(\d+d\s*)?(\d+h\s*)?(\d+m\s*)?(\d+s)?\s*$')


def cheap(tool, **kw):
    args = [CHEAP, tool]
    for k, v in kw.items():
        args += [f'--{k}', json.dumps(v) if not isinstance(v, str) else v]
    out = subprocess.run(args, capture_output=True, text=True, timeout=180)
    try:
        data = json.loads(out.stdout)
    except json.JSONDecodeError:
        raise RuntimeError(f'{tool}: unreadable reply\n{out.stdout}\n{out.stderr}')
    if not data.get('ok'):
        raise RuntimeError(f'{tool}: {data}')
    return data


def wait_for(predicate, timeout, step=0.5):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(step)
    return None


def prepare_datadir(source, target, exe, printer=PRINTER, model=PRINTER_MODEL):
    """Copy a prepared data directory and select a printer in the copy's configuration.

    A data directory that never ran the setup guide has no vendor presets, so the printer name
    would be dropped for "Default Printer". The vendor profile tree shipped beside the
    executable is therefore copied into the copy's system folder, which is what the setup
    guide does for the printers a person picks.
    """
    # The vendor presets sit five levels below the data directory, and the app cannot open a path
    # past 260 characters, so a deep scratch folder makes every preset file fail to load.
    if len(os.path.abspath(target)) > 150:
        raise SystemExit(f'the data directory path is {len(os.path.abspath(target))} characters long; '
                         'use a short one (for example under C:\\Users\\Public\\bbsdd) because the app cannot '
                         'open preset files whose full path passes 260 characters')
    if os.path.isdir(target):
        # The project history folder is a repository whose objects are read-only files.
        def writable(func, path, _exc):
            os.chmod(path, 0o700)
            func(path)
        # A file the previous run wrote can stay open for a moment (a scanner, a closing process).
        for attempt in range(6):
            try:
                if sys.version_info >= (3, 12):
                    shutil.rmtree(target, onexc=writable)
                else:
                    shutil.rmtree(target, onerror=writable)
                break
            except PermissionError:
                if attempt == 5:
                    raise
                time.sleep(5)
    shutil.copytree(source, target, ignore=shutil.ignore_patterns('log', '*.bak', 'system', 'project_history'))
    os.makedirs(os.path.join(target, 'log'), exist_ok=True)
    profiles = os.path.join(os.path.dirname(os.path.abspath(exe)), 'resources', 'profiles')
    system = os.path.join(target, 'system')
    os.makedirs(system, exist_ok=True)
    shutil.copy2(os.path.join(profiles, 'BBL.json'), system)
    shutil.copytree(os.path.join(profiles, 'BBL'), os.path.join(system, 'BBL'))
    path = os.path.join(target, 'BambuStudio.conf')
    text = open(path, encoding='utf-8').read()
    # A trailing "# MD5 checksum" comment belongs to the original text; the edited file has none.
    text = '\n'.join(line for line in text.splitlines() if not line.startswith('# MD5 checksum'))
    conf = json.loads(text)
    app = conf.setdefault('app', {})
    downloads = os.path.join(target, 'downloads')
    os.makedirs(downloads, exist_ok=True)
    app['download_path'] = downloads
    for flag in TIP_FLAGS:
        app[flag] = False
    # Keys an earlier run may have filled with paths of the machine's account.
    for key in ('last_backup_path', 'preset_folder'):
        app.pop(key, None)
    conf.pop('recent', None)
    conf.pop('model_creator', None)
    conf['models'] = [{'model': model, 'nozzle_diameter': '0.4', 'vendor': 'BBL'}]
    presets = conf.setdefault('presets', {})
    for key in [k for k in presets if k.startswith('filament_0') or k in ('flush_volumes_matrix', 'flush_volumes_vector')]:
        presets.pop(key)
    presets['machine'] = printer
    presets['process'] = PROCESS
    presets['filament'] = FILAMENTS[0]
    for i, name in enumerate(FILAMENTS[1:], start=1):
        presets[f'filament_{i:02d}'] = name
    presets['filaments'] = list(FILAMENTS)
    presets['filament_colors'] = ','.join(FILAMENT_COLOURS)
    presets['filament_multi_colors'] = ','.join(FILAMENT_COLOURS)
    presets['filament_color_types'] = ','.join('1' for _ in FILAMENTS)
    presets['filament_is_mixed'] = ','.join('0' for _ in FILAMENTS)
    conf.pop('nozzle_volume_types', None)
    with open(path, 'w', encoding='utf-8', newline='\n') as fh:
        json.dump(conf, fh, indent=4)
        fh.write('\n')
    return path


class Session:
    """One app process on its own hidden desktop, driven through the cheap CLI."""

    def __init__(self, exe, datadir, desktop, tag, staging):
        self.exe, self.datadir, self.desktop, self.tag, self.staging = exe, datadir, desktop, tag, staging
        self.pid = None
        self.main = None
        self.owned = set()
        self.n = 0

    def start(self, timeout=240):
        os.environ['BAMBU_LAYOUT_PROBE'] = '1'
        os.environ['BAMBU_LAYOUT_PROBE_TAG'] = self.tag
        cheap('create_headless_desktop', name=self.desktop)
        launched = cheap('launch_on_headless_desktop', name=self.desktop,
                         command=f'"{self.exe}" --datadir "{self.datadir}"')['pid']
        self.owned.add(launched)

        def frame():
            # The app may relaunch itself once (Mesa fallback): adopt whichever process shows the frame.
            for w in cheap('list_headless_windows', name=self.desktop)['windows']:
                if w['class'] == 'wxWindowNR' and w['width'] >= 1000 and w['height'] >= 600:
                    return w
            return None
        found = wait_for(frame, timeout, step=1.0)
        if not found:
            raise RuntimeError('timed out waiting for the main frame')
        self.pid, self.main = found['process_id'], found['handle']
        self.owned.add(self.pid)
        time.sleep(10)  # first layout, plugin prompt, bilingual decoration

    def stop(self):
        for pid in list(self.owned):
            try:
                cheap('kill_process', pid=pid, force=True)
            except RuntimeError:
                pass  # already exited (the relaunch parent does)
        time.sleep(2)
        try:
            cheap('close_headless_desktop', name=self.desktop)
        except RuntimeError:
            pass

    def alive(self):
        try:
            return any(w['handle'] == self.main for w in self.windows())
        except RuntimeError:
            return False

    def windows(self):
        return [w for w in cheap('list_headless_windows', name=self.desktop)['windows'] if w['process_id'] == self.pid]

    def send(self, command=None, dump=None, timeout=20):
        # SendMessage does not cross desktops, so the sender runs on the hidden one.
        cmd = f'"{sys.executable}" "{SENDER}" {self.main}'
        if dump:
            cmd += f' "{dump}" --timeout {timeout}'
        if command:
            cmd += f' --command "{command}"'
        cheap('launch_on_headless_desktop', name=self.desktop, command=cmd)

    def command(self, payload, settle=1.5):
        self.send(command=payload)
        time.sleep(settle)

    def probe(self):
        """Write a layout-probe dump and return its records (empty when none completed)."""
        self.n += 1
        staged = os.path.join(self.staging, f'{self.pid}-probe{self.n}.jsonl')
        if os.path.exists(staged):
            os.remove(staged)
        self.send(dump=staged)

        def complete():
            if not os.path.exists(staged):
                return None
            try:
                lines = [json.loads(line) for line in open(staged, encoding='utf-8') if line.strip()]
            except json.JSONDecodeError:
                return None
            return lines if lines and lines[-1].get('kind') == 'end' else None
        return wait_for(complete, 40) or []

    def click(self, hwnd, x, y, settle=1.5):
        cheap('mouse_click', hwnd=hwnd, x=int(x), y=int(y))
        time.sleep(settle)

    def click_record(self, rec, settle=1.5):
        """Click the middle of a probe window record, on that window itself."""
        client = rec.get('client') or rec['rect']
        self.click(rec['hwnd'], client['w'] // 2, client['h'] // 2, settle)

    def shot(self, path, hwnd=None):
        cheap('screenshot', hwnd=hwnd or self.main, output_path=path)
        if os.path.getsize(path) < 2000:
            raise RuntimeError(f'{path}: not a rendered frame')

    def canvas_png(self, path):
        staged = os.path.join(self.staging, f'{self.pid}-canvas{self.n}.png')
        self.n += 1
        for leftover in (staged, staged + '.part'):
            if os.path.exists(leftover):
                os.remove(leftover)
        self.send(command=f'canvas-png {staged}')
        if not wait_for(lambda: os.path.exists(staged) and os.path.getsize(staged) > 0, 20):
            return False
        time.sleep(0.5)
        shutil.move(staged, path)
        return True


def shown(records, **want):
    """Shown window records; `type_contains` and `parent` narrow them."""
    type_contains = want.get('type_contains')
    parent = want.get('parent')
    out = []
    for r in records:
        if r.get('kind') != 'window' or not r.get('shown') or r.get('on_screen') is False:
            continue
        if type_contains and type_contains.lower() not in str(r.get('type')).lower():
            continue
        if parent is not None and r.get('parent') != parent:
            continue
        out.append(r)
    return out


class Run:
    def __init__(self, args, session):
        self.args, self.s = args, session
        self.saved, self.unreached, self.notes = [], [], []
        self.first_plate_cubes = 3

    # -- bookkeeping -------------------------------------------------------------------------
    def target(self, name):
        return os.path.join(self.args.out, f'{name}--{self.args.tuple_id}.png')

    def save_window(self, name, what, samples=5):
        """Capture the whole window several times and keep the most frequent picture.

        PrintWindow of the same idle window is not repeatable: pieces of the bottom action bar
        (a label, an icon, a whole disabled button) come out blank in some of the captures and
        painted in the others. Keeping the most frequent picture of several gives the state the
        window is usually in, and the report states how many different pictures were seen.
        """
        path = self.target(name)
        try:
            tries = []
            for i in range(samples):
                temp = f'{path[:-4]}.sample{i}.png'
                self.s.shot(temp)
                with open(temp, 'rb') as fh:
                    tries.append((hashlib.md5(fh.read()).hexdigest(), temp))
                time.sleep(1.0)
            counts = {}
            for digest, _ in tries:
                counts[digest] = counts.get(digest, 0) + 1
            best = max(counts, key=lambda d: (counts[d], max(i for i, (h, _) in enumerate(tries) if h == d)))
            chosen = next(temp for digest, temp in reversed(tries) if digest == best)
            shutil.move(chosen, path)
            for _, temp in tries:
                if os.path.exists(temp):
                    os.remove(temp)
            self.saved.append({'capture': name, 'kind': 'window', 'png': path, 'shows': what,
                               'samples': samples, 'different_pictures': len(counts), 'chosen_seen': counts[best]})
            print(f'  {name}: saved ({len(counts)} different of {samples})', flush=True)
            return True
        except RuntimeError as exc:
            self.unreached.append({'state': name, 'reason': f'window capture failed: {exc}'})
            return False

    def save_canvas(self, name, what):
        path = self.target(name)
        if self.s.canvas_png(path):
            self.saved.append({'capture': name, 'kind': 'canvas', 'png': path, 'shows': what})
            print(f'  {name}: saved', flush=True)
            return True
        self.unreached.append({'state': name, 'reason': 'the canvas wrote no frame (no canvas-png in this build?)'})
        return False

    def cannot(self, state, reason):
        self.unreached.append({'state': state, 'reason': reason})
        print(f'  {state}: NOT REACHED: {reason}', flush=True)

    def dismiss_dialogs(self):
        """Close any dialog the app opened, after keeping a picture of it."""
        records = self.s.probe()
        closed = []
        for r in records:
            if r.get('kind') == 'toplevel' and r.get('shown') and r.get('class') == 'wxDialog':
                name = f'dialog-{r["hwnd"]}'
                try:
                    self.s.shot(self.target(name), hwnd=r['hwnd'])
                except RuntimeError:
                    pass
                self.s.command(f'close {r["hwnd"]}', settle=2)
                closed.append({'title': r.get('title'), 'png': self.target(name)})
        if closed:
            self.notes.append({'dialogs_closed': closed})
        return closed

    # -- building blocks ---------------------------------------------------------------------
    def dpi_scale(self):
        """The display scale the probe reports (1.5 at 150 %), read once."""
        if getattr(self, '_dpi_scale', None) is None:
            header = next((r for r in self.s.probe() if r.get('kind') == 'header'), {})
            self._dpi_scale = float(header.get('dpi_scale') or 1.0)
        return self._dpi_scale

    def resize(self, size):
        # The sizes are design pixels, like the 1200x800 the capture datadirs configure; the
        # resize command takes physical pixels, so scale them, or at 150 % the narrow window
        # leaves the canvas a strip a hundred pixels wide.
        scale = self.dpi_scale()
        self.s.command(f'resize {self.s.main} {round(size[0] * scale)} {round(size[1] * scale)}', settle=4)

    def go_prepare(self):
        """Click the Prepare entry of the navigation rail.

        It is found by its label in the probe, because its position follows the layout and the
        display scale; the fixed point is only the fallback for a probe without the rail.
        """
        def is_prepare(r):
            text = f"{r.get('label') or ''} {r.get('name') or ''}".casefold()
            return 'prepare' in text or '準備' in text
        records = self.s.probe()
        frame = next((r for r in records if r.get('kind') == 'toplevel' and r.get('hwnd') == self.s.main), None)

        def inside(r):
            if frame is None:
                return True
            f, p = frame['rect'], r['screen']
            return f['x'] <= p['x'] < f['x'] + f['w'] and f['y'] <= p['y'] < f['y'] + f['h']
        # A hidden page keeps a second Prepare button far below the window, so keep the one on screen.
        rail = sorted((r for r in shown(records, type_contains='Button') if is_prepare(r) and inside(r)),
                      key=lambda r: (r['screen']['x'], r['screen']['y']))
        if rail:
            self.s.click_record(rail[0], settle=3)
        else:
            self.s.click(self.s.main, *PREPARE_TAB, settle=3)

    def sidebar_tabs(self, records):
        """The sidebar's tab strip buttons from top to bottom: Ink, Process, Objects."""
        # The project strip above the window also uses this button type, so keep only the ones
        # that lie inside the sidebar.
        sidebar = next((r for r in shown(records, type_contains='Sidebar') if r.get('type') == 'Slic3r::GUI::Sidebar'), None)
        tabs = [r for r in shown(records, type_contains='TabStripButton')
                if sidebar is None or (sidebar['screen']['x'] <= r['screen']['x'] < sidebar['screen']['x'] + sidebar['screen']['w']
                                       and sidebar['screen']['y'] <= r['screen']['y'] < sidebar['screen']['y'] + sidebar['screen']['h'])]
        return sorted(tabs, key=lambda r: r['screen']['y'])

    def select_sidebar_tab(self, index, label):
        records = self.s.probe()
        tabs = self.sidebar_tabs(records)
        if len(tabs) <= index:
            self.cannot(label, f'the sidebar shows {len(tabs)} tab buttons, need index {index}')
            return False
        self.s.click_record(tabs[index], settle=2.5)
        return True

    def action_buttons(self, records):
        """Slice plate, Slice and print, Print plate: the wide side buttons, left to right."""
        buttons = [r for r in shown(records, type_contains='SideButton') if r['rect']['w'] > 60]
        return sorted(buttons, key=lambda r: r['screen']['x'])

    def gl_item(self, records, name):
        return next((r for r in records if r.get('kind') == 'gl_item' and r.get('name') == name), None)

    def plate_buttons(self, records):
        """The buttons of the bottom bar that are not wide actions: the plate button, the add button."""
        frame = next((r for r in records if r.get('kind') == 'toplevel' and r.get('hwnd') == self.s.main), None)
        if not frame:
            return []
        band = frame['rect']['y'] + frame['rect']['h'] - 120
        return [r for r in shown(records, type_contains='Button')
                if r['screen']['y'] >= band and r['rect']['h'] >= 38]

    def add_plate(self):
        """Click the add button of the bottom bar and check that a button labelled with 2 appears."""
        records = self.s.probe()
        buttons = self.plate_buttons(records)
        # The button is named Add Plate; a square button of the bar is the fallback for a
        # translated name. Its size follows the display scale, so only the shape is checked.
        adders = [r for r in buttons if str(r.get('name') or '').casefold() == 'add plate']
        adders = adders or [r for r in buttons if abs(r['rect']['w'] - r['rect']['h']) <= 4
                            and not str(r.get('label') or '').strip() and r['type'] == 'Button']
        if not adders:
            self.cannot('second-plate', 'the bottom bar shows no add-plate button')
            return False
        self.s.click_record(adders[0], settle=3)
        labels = [str(r.get('label') or '') for r in self.plate_buttons(self.s.probe())]
        if not any(re.search(r'(^|\D)2(\D|$)', label) for label in labels):
            self.cannot('second-plate', f'no plate button with the number 2 appeared after the click (labels {labels})')
            return False
        return True

    def load_cubes(self, count):
        for i in range(count):
            self.s.command(f'load {CUBE}', settle=7)
        self.notes.append({'loaded_cubes': count})

    def arrange(self):
        """Open the canvas Arrange options, press its Arrange button and close the options again.

        The options are an ImGui panel the layout probe cannot see, so its button is found on a
        canvas frame: the panel is the block of one colour under the toolbar (white in light
        mode, dark in dark mode), and its first button sits near the bottom left corner of it.
        """
        try:
            from PIL import Image
        except ImportError:
            self.cannot('arrange', 'Pillow is not installed, so the Arrange button cannot be located')
            return False
        def near(a, b):
            return max(abs(a[i] - b[i]) for i in range(3)) <= 6

        def grab():
            frame = os.path.join(self.s.staging, f'{self.s.pid}-arrange.png')
            if not self.s.canvas_png(frame):
                return None
            image = Image.open(frame).convert('RGB')
            os.remove(frame)
            return image

        # A frame before the click: the canvas background also runs wide in one colour, so the
        # panel is only open where that row has changed. Taking it first also makes the canvas
        # draw at its current size, so the toolbar rects the probe reads next are current (they
        # are kept from the last frame, which after a resize in Preview is the old size).
        before = grab()
        item = self.gl_item(self.s.probe(), 'arrange')
        if not item:
            self.cannot('arrange', 'the canvas toolbar reports no arrange item')
            return False
        rect = item['rect']
        centre = (rect['x'] + rect['w'] // 2, rect['y'] + rect['h'] // 2)
        # Every length below is in units of the toolbar item, so it follows the display scale.
        unit = rect['h'] / 60.0

        def panel_frame():
            """The canvas frame and a row inside the panel, or None when the panel is not open."""
            image = grab()
            if image is None:
                return None
            if not (0 <= centre[0] < image.width and rect['y'] + rect['h'] < image.height):
                sizes.append((image.size, rect))
                return None
            pixels = image.load()
            old = before.load() if before is not None and before.size == image.size else None
            x_lo, x_hi = max(0, rect['x'] - int(40 * unit)), min(image.width, rect['x'] + int(420 * unit))
            # The first panel row: a little below the toolbar, where the panel colour runs wide.
            for y in range(rect['y'] + rect['h'] + int(8 * unit), min(image.height, rect['y'] + rect['h'] + int(60 * unit))):
                colour = pixels[centre[0], y]
                if old is not None and near(old[centre[0], y], colour):
                    continue
                run = [x for x in range(x_lo, x_hi) if near(pixels[x, y], colour)]
                if len(run) >= int(240 * unit):
                    return image, pixels, y, colour, min(run)
            return None

        sizes = []
        self.s.click(item['host'], *centre, settle=2)
        found = panel_frame()
        if found is None:
            # The first click after switching back from Preview can go to the canvas focus only.
            self.s.click(item['host'], *centre, settle=2)
            found = panel_frame()
        if found is None:
            if sizes:
                self.cannot('arrange', f'the toolbar item lies outside the canvas frame (frame size, item rect: {sizes[-1]})')
                return False
            self.cannot('arrange', 'the Arrange options panel did not open')
            return False
        image, pixels, top, colour, left = found
        # The panel's inner left margin has no content and keeps the panel colour down to the
        # panel's bottom, except for the thin separators, so the bottom is where more than a few
        # rows of another colour follow.
        bottom, gap = top, 0
        for y in range(top + 1, image.height):
            if near(pixels[left + 4, y], colour):
                bottom, gap = y, 0
            else:
                gap += 1
                if gap > 3:
                    break
        if bottom - top < int(60 * unit):
            self.cannot('arrange', 'the Arrange options panel is too short to hold its buttons')
            return False
        self.s.click(item['host'], left + int(42 * unit), bottom - int(19 * unit), settle=10)
        self.s.click(item['host'], *centre, settle=2)  # the item is a toggle: close the options
        self.notes.append({'arrange': 'pressed the Arrange button of the canvas options'})
        return True

    def ink_chips(self, listing):
        """The ink chips of the Objects list, top to bottom, as (x, y, ink) in the list's own
        coordinates; None without PIL. The chips are found by their ink colours, so the row
        height and the column position may follow the density and the display scale."""
        try:
            from PIL import Image
        except ImportError:
            return None
        probe = os.path.join(self.s.staging, f'{self.s.pid}-chips.png')
        self.s.shot(probe, hwnd=listing['hwnd'])
        image = Image.open(probe).convert('RGB')
        os.remove(probe)
        pixels = image.load()
        inks = [tuple(int(c[i:i + 2], 16) for i in (1, 3, 5)) for c in FILAMENT_COLOURS]

        def ink_of(p):
            for n, ink in enumerate(inks, start=1):
                if max(abs(p[i] - ink[i]) for i in range(3)) <= 24:
                    return n
            return None

        chips, band = [], None
        for y in range(image.height):
            hits = [(x, ink_of(pixels[x, y])) for x in range(image.width)]
            hits = [(x, n) for x, n in hits if n]
            if len(hits) >= 12:
                xs = [x for x, _ in hits]
                if band is None:
                    band = {'y0': y, 'x0': min(xs), 'x1': max(xs), 'ink': hits[len(hits) // 2][1]}
                band['y1'] = y
            elif band is not None:
                chips.append(((band['x0'] + band['x1']) // 2, (band['y0'] + band['y1']) // 2, band['ink']))
                band = None
        if band is not None:
            chips.append(((band['x0'] + band['x1']) // 2, (band['y0'] + band['y1']) // 2, band['ink']))
        return chips

    def assign_ink(self, row, ink, label):
        """Give the object in list row `row` (0 is the plate row) the ink number `ink`."""
        records = self.s.probe()
        listing = next(iter(shown(records, type_contains='wxDataViewMainWindow')), None)
        if not listing:
            self.cannot(label, 'the Objects list is not on screen')
            return False
        chips = self.ink_chips(listing)
        if chips is None:
            self.cannot(label, 'Pillow is not installed, so the ink chips cannot be located')
            return False
        if len(chips) < row:
            self.cannot(label, f'the Objects list shows {len(chips)} ink chips, need {row}')
            return False
        chip_x, y, _ = chips[row - 1]
        self.s.click(listing['hwnd'], chip_x // 2, y, settle=1.2)  # select the row on its name
        self.s.click(listing['hwnd'], chip_x, y, settle=1.2)
        self.s.click(listing['hwnd'], chip_x, y, settle=1.8)  # a second click on the cell starts editing
        records = self.s.probe()
        combo = next(iter(shown(records, type_contains='ComboBox', parent=listing['hwnd'])), None)
        if not combo:
            self.cannot(label, 'the ink editor did not open in the Objects list')
            return False
        self.s.click_record(combo, settle=1.5)
        # The dropdown list window is created with the editor and only shown by the click, so it is
        # recognised by its shape: one row per ink, each row as tall as the display scale makes it.
        rows = len(FILAMENTS)
        candidates = [w for w in self.s.windows()
                      if w['class'] == 'wxWindowNR' and 100 <= w['width'] <= 400 and 24 * rows <= w['height'] <= 80 * rows]
        if not candidates:
            self.cannot(label, 'the ink dropdown list did not appear')
            return False
        for menu in candidates:
            step = menu['height'] / rows
            self.s.click(menu['handle'], 30, int(step * (ink - 0.5)), settle=2)
            state = self.chip_ink(row, ink)
            if state is not False:
                self.notes.append({'ink_chip': state})
                return True
        self.cannot(label, 'the ink chip did not change after choosing from the dropdown')
        return False

    def chip_ink(self, row, ink=2):
        """True when the ink chip of list row `row` shows ink `ink`, False when it does not, None without PIL."""
        listing = next(iter(shown(self.s.probe(), type_contains='wxDataViewMainWindow')), None)
        if not listing:
            return False
        chips = self.ink_chips(listing)
        if chips is None:
            return None
        return len(chips) >= row and chips[row - 1][2] == ink

    def slice_plate(self, label, timeout=240):
        records = self.s.probe()
        buttons = self.action_buttons(records)
        if not buttons:
            self.cannot(label, 'no Slice plate button is on screen')
            return False
        self.s.click_record(buttons[0], settle=3)
        self.dismiss_dialogs()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            records = self.s.probe()
            if not self.s.alive():
                self.cannot(label, 'the application exited while slicing')
                return False
            labels = [r for r in shown(records, type_contains='Label') if TIME_TEXT.match(str(r.get('label') or '')) and str(r.get('label')).strip()]
            if labels:
                time.sleep(4)  # the switch to Preview and the first frame of it
                return True
            time.sleep(4)
        self.cannot(label, f'no print time appeared within {timeout} s of the click')
        return False

    def canvas_window(self, records):
        return next(iter(shown(records, type_contains='wxGLCanvas')), None)

    def preview_captures(self, prefix, what):
        self.save_window(f'{prefix}-window', f'{what}; the OpenGL canvas is blank in a window capture')
        self.save_canvas(f'{prefix}-canvas', what)

    # -- the flow ----------------------------------------------------------------------------
    def run(self):
        s = self.s
        self.go_prepare()
        self.load_cubes(3)
        # The cubes load on top of each other, so the first slice makes the slicer warn about
        # colliding paths: a real warning banner in the notification column.
        if self.select_sidebar_tab(2, 'prepare-objects'):
            if self.assign_ink(2, 2, 'ink-assignment'):
                self.notes.append({'ink_assignment': 'second cube set to ink 2'})
                # A click on the empty part of the list ends the in-place editor.
                listing = next(iter(shown(s.probe(), type_contains='wxDataViewMainWindow')), None)
                if listing:
                    s.click(listing['hwnd'], 300, 150, settle=1.5)
            self.save_window('prepare-objects', 'Prepare tab, Objects page: list rows, ink chips, plate row')
        if self.slice_plate('slice-overlapping-cubes'):
            self.save_canvas('preview-warning-canvas', 'Preview at 1200x800 with the slicer warning banner in the notification column')
            self.resize(WIDE)
            self.save_canvas('preview-wide-warning-canvas', 'Preview at 1700x900 with the slicer warning banner')
            s.command('notify Sample notification pushed by the capture driver: the plate was sliced', settle=3)
            self.save_canvas('preview-wide-warning-toast-canvas', 'Preview at 1700x900 with the warning banner and an extra notification stacked')
            self.resize(NARROW)
        # Arrange separates the cubes, and the slice that follows has no warning.
        self.go_prepare()
        self.arrange()
        self.save_canvas('prepare-canvas', 'Prepare canvas: three cubes on the dual-nozzle plate after Arrange')
        if not self.slice_plate('slice-arranged-cubes'):
            return
        time.sleep(2)
        self.preview_captures('preview', 'Preview after slicing, 1200x800 window')
        self.resize(WIDE)
        self.preview_captures('preview-wide', 'Preview after slicing, 1700x900 window')
        s.command('notify Sample notification pushed by the capture driver: the plate was sliced', settle=3)
        self.save_canvas('preview-wide-toast-canvas', 'Preview at 1700x900 with an extra notification in the column')
        self.resize(TALL)
        self.preview_captures('preview-tall', 'Preview after slicing, 1700x1200 window, tall enough for the whole legend dock')
        self.resize(NARROW)
        # A second plate, loaded and sliced, brings up the plate strip with its All Plates Stats tile.
        self.go_prepare()
        if self.add_plate():
            self.load_cubes(1)
            if self.slice_plate('slice-second-plate'):
                self.resize(WIDE)
                self.preview_captures('preview-plates', 'Preview with two plates: strip, All Plates Stats tile, status chip, dock')
                canvas = self.canvas_window(s.probe())
                if canvas:
                    s.click(canvas['hwnd'], 70, 80, settle=3)
                    self.save_canvas('preview-stats-canvas', 'All Plates Stats page after a click on its tile')
                else:
                    self.cannot('preview-stats-canvas', 'no canvas window in the probe')
                self.resize(NARROW)
        self.plate_settings()

    def plate_settings(self):
        """Try to reach the plate settings page; it is the last step because the app may exit here."""
        s = self.s
        if not s.alive():
            self.cannot('prepare-plate-settings', 'the application is no longer running')
            return
        self.go_prepare()
        if not self.select_sidebar_tab(2, 'prepare-plate-settings'):
            return
        records = s.probe()
        listing = next(iter(shown(records, type_contains='wxDataViewMainWindow')), None)
        if not listing:
            self.cannot('prepare-plate-settings', 'the Objects list is not on screen')
            return
        # The row of the second plate: the first plate row, its cubes, then the plate row itself.
        s.click(listing['hwnd'], 100, 10 + 22 * (self.first_plate_cubes + 1), settle=2)
        if not self.select_sidebar_tab(1, 'prepare-plate-settings'):
            return
        switch = next(iter(shown(s.probe(), type_contains='SwitchButton')), None)
        if not switch:
            self.cannot('prepare-plate-settings', 'the Global and Objects switch is not on screen')
            return
        client = switch.get('client') or switch['rect']
        s.click(switch['hwnd'], int(client['w'] * 0.8), client['h'] // 2, settle=4)
        if not s.alive():
            self.cannot('prepare-plate-settings',
                        'the application exited when the Global and Objects switch on the Process page was clicked '
                        '(the second plate row was selected in the Objects list before)')
            return
        self.save_window('prepare-plate-settings', 'Process page switched to Objects with the second plate selected')
        # Which settings page the switch brought up: the plate's own page is TabPrintPlate.
        tabs = sorted({r.get('type') for r in shown(s.probe(), type_contains='Slic3r::GUI::Tab')})
        self.notes.append({'plate_settings_tabs_shown': tabs})


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--exe', required=True)
    ap.add_argument('--source-datadir', required=True)
    ap.add_argument('--datadir', required=True)
    ap.add_argument('--tuple', required=True, dest='tuple_id')
    ap.add_argument('--out', required=True)
    ap.add_argument('--printer', default=PRINTER)
    ap.add_argument('--desktop', default='bspreview')
    ap.add_argument('--startup-timeout', type=float, default=240)
    args = ap.parse_args()
    sys.stdout.reconfigure(encoding='utf-8')
    os.makedirs(args.out, exist_ok=True)
    # The app writes its dump files itself; keep them on a short path and move them afterwards.
    staging = os.path.join(os.environ.get('TEMP', os.path.expanduser('~')), 'bbpv')
    os.makedirs(staging, exist_ok=True)
    exe_sha = hashlib.sha256(open(args.exe, 'rb').read()).hexdigest()
    prepare_datadir(args.source_datadir, args.datadir, args.exe, printer=args.printer)
    session = Session(args.exe, args.datadir, args.desktop, f'preview--{args.tuple_id}', staging)
    run = Run(args, session)
    crashed = None
    try:
        session.start(args.startup_timeout)
        print(f'main frame {session.main} (pid {session.pid}) for {args.tuple_id}', flush=True)
        run.run()
    except Exception as exc:  # the report must still be written
        crashed = f'{type(exc).__name__}: {exc}'
        print(f'driver stopped: {crashed}', flush=True)
        import traceback
        traceback.print_exc()
    finally:
        session.stop()
    report = {'tuple': args.tuple_id, 'exe': args.exe, 'exe_sha256': exe_sha, 'printer': args.printer,
              'route': 'cheap-lowlevel-headless, canvas-png, background clicks on the controls own windows',
              'captured_at': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
              'captures': run.saved, 'not_reached': run.unreached, 'notes': run.notes,
              'driver_stopped': crashed}
    out_json = os.path.join(args.out, f'preview-capture--{args.tuple_id}.json')
    with open(out_json, 'w', encoding='utf-8') as fh:
        json.dump(report, fh, ensure_ascii=False, indent=1)
    print(f'{len(run.saved)} captures saved, {len(run.unreached)} states not reached; report {out_json}')
    return 0 if run.saved and not run.unreached and not crashed else 1


if __name__ == '__main__':
    sys.exit(main())
