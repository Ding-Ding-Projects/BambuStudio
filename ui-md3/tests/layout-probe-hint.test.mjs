import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, rmSync, writeFileSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// md3-v158's What's new cut its date hint to "YYYY-MM-DD / D" in every language
// mode, and a nine-dialog sweep reported nothing, because the layout probe did not
// measure placeholder hints (clipping inventory CJ-027). These contracts keep the
// probe measuring them, and the sweep and the report counting a cut one.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (...parts) => readFile(path.join(repoDir, ...parts), 'utf8');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

const probe = await read('src', 'slic3r', 'GUI', 'LayoutProbe.cpp');
const sweep = await read('scripts', 'md3', 'sweep-dialogs.py');

test('the probe measures the hint of an empty single-line entry', () => {
  const writer = stripComments(probe.match(/void write_window\([\s\S]*?\n\}/)[0]);
  assert.match(writer, /if \(auto \*entry = dynamic_cast<wxTextEntry \*>\(w\)\)/);
  assert.match(writer, /const wxString shown_hint = entry->GetHint\(\);/);
  assert.match(writer, /multi == nullptr \|\| !multi->IsMultiLine\(\)/, 'a multi-line entry does not draw its hint on one line');
  assert.match(writer, /hint_clipped = visible && entry->IsEmpty\(\) && hint_width \+ w->FromDIP\(4\) > client\.x;/,
    'only a hint that shows (empty, visible entry) and does not fit is cut');
  for (const key of ['hint', 'hint_width', 'hint_clipped']) {
    assert.ok(writer.includes(`<< ",\\"${key}\\":"`), `the record carries ${key}`);
  }
});

test('the dialog sweep counts a cut hint as a finding', () => {
  assert.match(sweep, /flags = \[f for f in \('text_clipped', 'truncated', 'hint_clipped', 'clipped_by_parent', 'starved'\) if r\.get\(f\)\]/);
});

test('the layout report lists a cut hint', () => {
  const dir = mkdtempSync(path.join(os.tmpdir(), 'probe-hint-'));
  try {
    const dump = path.join(dir, 'dump.jsonl');
    const window = (extra) => JSON.stringify({ kind: 'window', hwnd: 11, parent: 10, top: 10, depth: 1, class: 'wxTextCtrl',
      type: 'wxTextCtrl', name: 'From date', label: '', shown: true, on_screen: true, rect: { x: 0, y: 0, w: 117, h: 30 },
      client: { w: 117, h: 30 }, min: { w: -1, h: -1 }, text_width: -1, ellipsized: false, text_clipped: false,
      truncated: false, clipped_by_parent: false, starved: false, zero_sized: false, sizer: null, ...extra });
    const top = JSON.stringify({ kind: 'window', hwnd: 10, parent: 0, top: 10, depth: 0, class: 'wxDialog', type: 'wxDialog',
      name: 'dialog', label: "What's new", shown: true, on_screen: true, rect: { x: 0, y: 0, w: 880, h: 640 },
      client: { w: 880, h: 640 }, min: { w: -1, h: -1 }, text_width: -1, ellipsized: false, text_clipped: false,
      truncated: false, clipped_by_parent: false, starved: false, zero_sized: false, sizer: null });
    writeFileSync(dump, [top, window({ hint: 'YYYY-MM-DD / DD/MM/YYYY', hint_width: 170, hint_clipped: true })].join('\n') + '\n');
    // The report exits 1 when it has findings, like any check that found something.
    const run = spawnSync(process.execPath, [path.join(testDir, 'layout-probe-report.mjs'), dump, '--json'], { encoding: 'utf8' });
    assert.equal(run.status, 1, 'a cut hint makes the report fail');
    const report = JSON.parse(run.stdout);
    const cut = report.findings.filter((f) => f.finding === 'hint_clipped');
    assert.equal(cut.length, 1, 'one cut hint, one finding');
    assert.equal(cut[0].hint_width, 170);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});
