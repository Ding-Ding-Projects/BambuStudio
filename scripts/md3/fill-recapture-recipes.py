#!/usr/bin/env python3
"""Assign a reproducible recipe to every row of docs/screenshots/recapture-manifest.json.

Recipe kinds:
  page        capture a whole top-level window after the listed steps
  crop-probe  capture the page, then crop the control the layout probe reports
              under the given label or name (plus padding)
  crop-gl     capture the Prepare canvas, then crop the GL toolbar or gizmo
              item by its identifier (needs the probe's GL item rects)
  pages       rendered by ui-md3/scripts/capture-site.mjs / capture-app.mjs
  historical  a "before" state recorded as evidence of a fixed defect; retaking
              it would show the fixed state and misname the file, so it is kept
              as-is and listed here so nobody mistakes it for a current capture

Rows keep their manual recipe when one is already set and --force is absent.
"""
from __future__ import annotations

import json
import re
import sys

PATH = 'docs/screenshots/recapture-manifest.json'
TUPLE = {'language': 'en', 'theme': 'light', 'density': 'comfortable'}

MENU_LABELS = {'file': 'File', 'edit': 'Edit', 'view': 'View', 'objects': 'Objects', 'calibration': 'Calibration', 'help': 'Help'}
NAV = {'home': 'Home', 'prepare': 'Prepare', 'preview': 'Preview', 'device': 'Device', 'project': 'Project', 'calibration': 'Calibration', 'filament': 'Ink', 'settings-gear': 'Settings'}
GL_ITEMS = {'gizmo-move': 'move', 'gizmo-scale': 'scale', 'gizmo-rotate': 'rotate', 'gizmo-place-on-face': 'place', 'gizmo-cut': 'cut', 'gizmo-measure': 'measure',
            'gizmo-assembly': 'assembly', 'gizmo-brim': 'brim_ears', 'gizmo-color-paint': 'mmu_painting', 'gizmo-fdm-support': 'fdm_supports', 'gizmo-mmu-segment': 'mmu_segmentation',
            'gizmo-seam': 'seam', 'gizmo-support-paint': 'fdm_supports', 'gizmo-text': 'text', 'gizmo-rail': '*rail*',
            'scene-arrange': 'arrange', 'scene-assembly-view': 'assembly_view', 'scene-cut2': 'cut', 'scene-import': 'add', 'scene-layers': 'layersediting', 'scene-orient': 'orient',
            'scene-split': 'splitobjects', 'scene-toolbar': '*toolbar*', 'scene-undo': 'undo', 'zoom-controls': '*zoom*'}


def page(surface, steps=None, tuple_=None, note=None):
    r = {'kind': 'page', 'surface': surface, 'steps': steps or [], 'tuple': tuple_ or TUPLE}
    if note: r['note'] = note
    return r


def crop(surface, label, steps=None, tuple_=None, pad=8):
    return {'kind': 'crop-probe', 'surface': surface, 'steps': steps or [], 'label': label, 'pad': pad, 'tuple': tuple_ or TUPLE}


def gl(item, steps=None):
    return {'kind': 'crop-gl', 'surface': 'prepare', 'steps': steps or ['nav:Prepare'], 'item': item, 'pad': 8, 'tuple': TUPLE}


def hist(note):
    return {'kind': 'historical', 'note': note}


def title(s):
    return s.replace('-', ' ').strip().title()


def recipe_for(row):
    f = row['file']
    if row['kind'] == 'pages':
        return {'kind': 'pages', 'script': 'ui-md3/scripts/capture-site.mjs' if '/pages/app/' not in f else 'ui-md3/scripts/capture-app.mjs'}
    grp = f.split('/')[2] if f.startswith('docs/screenshots/') else 'readme-assets'
    pg, ctl = row['page'], row['control']
    dark = {**TUPLE, 'theme': 'dark'}

    if grp == 'readme-assets':
        m = re.match(r'(?:native-)?material-([\w-]+?)-(light|dark)-(en|bilingual|yue-hk)$', pg)
        if m:
            surf, theme, lang = m.groups()
            t = {**TUPLE, 'theme': theme, 'language': {'en': 'en', 'bilingual': 'bilingual_en_yue_HK', 'yue-hk': 'yue_HK'}[lang]}
            if surf == 'device-plugin-gate': return page('device', ['nav:Device'], t, 'plugin gate visible when the network plugin is absent')
            if surf == 'filament-manager': return page('filament-manager', ['nav:Prepare', 'open:Ink manager'], t)
            if surf == 'project-history': return page('project-history', ['open:Project history'], t)
            return page(surf, [f'nav:{surf.title()}'], t)
        if pg == 'shot-home': return page('home', ['nav:Home'])
        if pg == 'shot-prepare-frame': return page('prepare', ['nav:Prepare'])
        if pg == 'shot-prepare-sidebar': return crop('prepare', 'Process', ['nav:Prepare'], pad=16)
        if pg in ('shot-wizard', 'yum-20260811-wizard-ca49'): return page('config-wizard', ['open:Config wizard'])
        if pg == 'yum-20260811-native-main-ca49': return page('prepare', ['nav:Prepare'])

    if grp == 'main-window':
        if pg.startswith('menu-'): return page('menu', [f'menu:{MENU_LABELS[pg[5:]]}'], note='popup menu, capture via popovercap')
        if pg == 'nav-rail': return crop('main', 'nav rail', pad=0) if not ctl else crop('main', NAV[ctl])
        if pg == 'topbar':
            if not ctl: return crop('main', 'topbar', pad=0)
            if ctl.startswith('menu-'): return crop('main', MENU_LABELS[ctl[5:]])
            return crop('main', {'history-chip-main': 'main', 'logo-bambu-studio': 'Bambu Studio', 'palette-icon': 'Command palette', 'project-chip-untitled': 'Untitled',
                                 'window-close': 'Close', 'window-maximize': 'Maximize', 'window-minimize': 'Minimize'}[ctl])
        if pg == 'page-device': return page('device', ['nav:Device'])
        if pg == 'page-preview': return page('preview', ['nav:Preview'])
        if pg == 'prepare-action-bar': return crop('prepare', 'Slice plate', ['nav:Prepare'], pad=24)
        if pg == 'preview-action-bar-print-button': return crop('preview', 'Print plate', ['nav:Preview'])
        if pg == 'ink-terminology-sidebar': return crop('prepare', 'Ink', ['nav:Prepare'], pad=24)
        if pg == 'sidebar-search-pills': return crop('prepare', 'Search settings', ['nav:Prepare'], pad=12)
        if pg == 'sidebar-prepare':
            if not ctl: return crop('prepare', 'sidebar', ['nav:Prepare'], pad=0)
            if ctl in GL_ITEMS: return gl(GL_ITEMS[ctl])
            label = {'add-filament': 'Add ink', 'advanced-settings': 'Advanced', 'filament-menu': 'Ink menu', 'filament-row-1': 'Ink 1', 'layer-height-edit': 'Layer height',
                     'plate-1-chip': 'Plate 1', 'plate-add': 'Add plate', 'printer-edit-pencil': 'Edit printer', 'printer-settings-gear': 'Printer settings', 'printer-wifi': 'Connect',
                     'process-preset': 'Process', 'seg-others': 'Others', 'seg-quality': 'Quality', 'seg-strength': 'Strength', 'seg-support': 'Support', 'slice-plate': 'Slice plate'}[ctl]
            return crop('prepare', label, ['nav:Prepare'])

    if grp == 'preferences':
        tab = pg.split('-tab')[0].replace('sidebar-', '')
        steps = ['open:Preferences', f'tab:{title(tab)}']
        if pg.startswith('sidebar-'): return crop('preferences', title(tab), ['open:Preferences'])
        if pg == 'general-tab-scrolled': return page('preferences', steps + ['scroll:end'])
        if not ctl: return page('preferences', steps)
        return crop('preferences', title(ctl.replace('toggle-', '').replace('-combo', '').replace('-field', '').replace('-browse', ' browse')), steps)

    if grp == 'appearance':
        steps = ['open:Preferences', 'tab:Appearance']
        if pg == 'custom-color-dialog': return page('custom-color', steps + ['click:Custom'])
        if not ctl: return page('preferences', steps)
        if ctl.startswith('accent-'): return crop('preferences', f'Accent {ctl[7:]}', steps)
        if ctl == 'theme-dark': return crop('preferences', 'Dark', steps, dark)
        if ctl == 'theme-light': return crop('preferences', 'Light', steps)
        if ctl == 'density-compact': return crop('preferences', 'Compact', steps, {**TUPLE, 'density': 'compact'})
        return crop('preferences', title(ctl.replace('-btn', '').replace('-combo', '')), steps)

    if grp == 'regex-builder':
        steps = ['nav:Prepare', 'click:Regex builder']
        if pg == 'search-field': return crop('prepare', 'Search settings', ['nav:Prepare'], pad=12) if not ctl else crop('prepare', title(ctl), ['nav:Prepare'])
        if ctl == 'flags-before-fix': return hist('flags row before the 2026-07 clipping fix')
        if not ctl: return page('regex-builder', steps)
        if ctl.startswith('token-'): return crop('regex-builder', title(ctl[6:]), steps + ['tab:Tokens'])
        return crop('regex-builder', title(ctl), steps)

    if grp == 'wizard':
        m = re.match(r'wizard-step-(\d+)', pg)
        if pg == 'wizard-index': return page('config-wizard', ['open:Config wizard'])
        step = m.group(1)
        steps = ['open:Config wizard', f'wizard-page:{step}']
        if not ctl: return page('config-wizard', steps)
        if ctl == 'nav-buttons': return crop('config-wizard', 'Next', steps, pad=24)
        return crop('config-wizard', title(ctl.replace('-btn', '').replace('-checkbox', '').replace('-filter', ' filter')), steps)

    if grp == 'config-profiles':
        steps = ['open:Config profiles']
        if not ctl: return page('config-profiles', steps)
        if 'armed' in ctl or 'confirmed' in ctl: return crop('config-profiles', 'Slide to confirm', steps + ['arm:Slide to confirm'], pad=12)
        return crop('config-profiles', title(ctl.replace('-btn', '')), steps)

    if grp == 'version-history':
        steps = ['open:Version history']
        if pg == 'crash-restore-prompt': return page('crash-restore', ['launch:with-crash-marker'])
        if pg == 'menu-entry': return crop('menu', 'Version history', ['menu:File'])
        if pg == 'topbar-chip': return crop('main', 'main')
        if pg == 'history-dialog-dark': return hist('dark card before fix') if ctl else page('version-history', steps, dark)
        if not ctl: return page('version-history', steps)
        return crop('version-history', title(ctl), steps)

    if grp == 'project-tabs':
        if pg == 'tab-bar-two-tabs': return crop('main', 'project tabs', ['open:New project'], pad=0)
        if not ctl: return crop('main', 'project tabs', pad=0)
        return crop('main', {'new-tab-plus': 'New project', 'tab-active': 'Untitled', 'tab-active-close-x': 'Close tab', 'tab-close-x': 'Close tab', 'tab-inactive': 'Untitled', 'tab-untitled': 'Untitled'}[ctl], ['open:New project'] if 'inactive' in ctl or ctl == 'tab-close-x' else [])

    if grp == 'md3-conversion':
        if pg.startswith('action-bar-before'): return hist('starved action bar before the sizer fix')
        if pg.startswith('action-bar-after'): return crop('prepare', 'Slice plate', ['nav:Prepare'], pad=24)
        if pg.startswith('gizmo-rail'): return gl('*rail*', ['nav:Prepare', 'open:cube.stl'])
        if pg.startswith('prepare-model-loaded'): return page('prepare', ['nav:Prepare', 'open:cube.stl'], dark)
        if pg.startswith('prepare-workspace'): return page('prepare', ['nav:Prepare'], dark)
        if pg.startswith('printer-settings'): return page('printer-settings', ['nav:Prepare', 'click:Printer settings'], dark)

    if grp == 'notifications':
        steps = ['nav:Prepare', 'trigger:info-toast']
        if pg == 'toast-presets-up-to-date': return page('toast', ['menu:Help', 'click:Check for presets updates'])
        if pg == 'toast-try-slice': return page('toast', ['nav:Prepare', 'open:cube.stl'])
        if not ctl: return page('toast', steps)
        return crop('toast', {'accent-bar': 'accent', 'close-x': 'Close', 'toast': 'toast'}[ctl], steps, pad=4)

    if grp == 'smart-home':
        if 'before' in pg: return hist('dialog before the text-action fix')
        m = re.match(r'dialog-(\d+)x(\d+)', pg)
        steps = ['open:Smart home'] + ([f'resize:{m.group(1)}x{m.group(2)}'] if m else [])
        return page('smart-home', steps)

    if grp == 'home':
        if not ctl: return page('home', ['nav:Home'])
        return crop('home', {'create-new-project-btn': 'Create new project', 'more-link': 'More', 'open-project-btn': 'Open project', 'recently-opened-header': 'Recently opened'}[ctl], ['nav:Home'])

    if grp == 'sidebar-process':
        if pg.startswith('before-'): return hist('sidebar before the starved-row fix')
        if pg == 'after-search-settings': return crop('prepare', 'Search settings', ['nav:Prepare'], pad=12)
        return crop('prepare', 'Process', ['nav:Prepare'], pad=16)

    if grp == 'dark':
        return {'config-profiles': page('config-profiles', ['open:Config profiles'], dark), 'menu-file': page('menu', ['menu:File'], dark, 'popup menu, capture via popovercap'),
                'preferences-appearance': page('preferences', ['open:Preferences', 'tab:Appearance'], dark), 'prepare': page('prepare', ['nav:Prepare'], dark)}[pg]

    if grp == 'dialog-chrome':
        return {'about': page('about', ['menu:Help', 'click:About']), 'export-preset-bundle': page('export-preset-bundle', ['menu:File', 'click:Export preset bundle']),
                'keyboard-shortcuts': page('keyboard-shortcuts', ['menu:Help', 'click:Keyboard shortcuts']), 'network-test': page('network-test', ['menu:Help', 'click:Network test'])}[pg]

    if grp == 'bulk-filament':
        return page('bulk-ink', ['nav:Prepare', 'click:Bulk ink']) if pg == 'dialog' else crop('prepare', 'Bulk ink', ['nav:Prepare'])
    if grp == 'filament-scanner':
        return page('ink-scanner', ['nav:Prepare', 'click:Ink scanner'] + (['upload:no-model'] if ctl else []))
    if grp == 'command-palette':
        return page('command-palette', ['key:ctrl+shift+f'])
    return None


def main():
    force = '--force' in sys.argv
    m = json.load(open(PATH, encoding='utf-8'))
    missing = []
    for row in m['captures']:
        if row.get('recipe') and not force: continue
        r = recipe_for(row)
        if r is None: missing.append(row['file'])
        row['recipe'] = r
    json.dump(m, open(PATH, 'w', encoding='utf-8', newline='\n'), indent=2)
    kinds = {}
    for row in m['captures']:
        k = (row['recipe'] or {}).get('kind', 'none')
        kinds[k] = kinds.get(k, 0) + 1
    print(kinds)
    if missing:
        print('no recipe for:', *missing, sep='\n  ')
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
