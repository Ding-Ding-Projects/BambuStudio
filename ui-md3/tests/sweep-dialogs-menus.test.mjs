import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// md3-v162's full dialog sweep opened nothing for "Show Tip of the Day" in any
// language mode, because that item draws the Daily Tips panel inside the 3D
// canvas instead of opening a window, and nothing for "About" in Cantonese mode,
// because the catalogue builds that label from "&About %s" and the sweep only
// looked up exact labels. These contracts keep both reachable.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const sweep = (await readFile(path.join(repoDir, 'scripts', 'md3', 'sweep-dialogs.py'), 'utf8')).replace(/\r\n/g, '\n');
const entries = sweep.slice(sweep.indexOf('ENTRIES = ['), sweep.indexOf(']\n', sweep.indexOf('ENTRIES = [')));
const branch = (start) => sweep.slice(sweep.indexOf(start), sweep.indexOf('\n            el', sweep.indexOf(start) + start.length));

test('Show Tip of the Day is swept as a canvas panel, not as a dialog', () => {
  assert.match(entries, /'canvas:Show Tip of the Day'/);
  assert.doesNotMatch(entries, /[,\[]\s*'Show Tip of the Day'/, 'as a dialog entry it can only ever report no-dialog');
  const canvas = branch("elif entry.startswith('canvas:'):");
  assert.match(canvas, /form = \(cantonese_form\(label, cantonese\) if cantonese else None\) or label/,
    'invoked once, in the language the menus show');
  assert.match(canvas, /cheap\('mouse_click', hwnd=main_hwnd, x=PREPARE_TAB\[0\], y=PREPARE_TAB\[1\]\)[\s\S]*send\(args\.desktop, main_hwnd, command=f'invoke \{form\}'\)/,
    'the canvas only shows on Prepare, and the app starts on Home');
  assert.match(canvas, /cheap\('screenshot', hwnd=main_hwnd, output_path=frame_png\)/, 'the main frame is captured as context');
  assert.match(canvas, /send\(args\.desktop, main_hwnd, command=f'canvas-png \{staged\}'\)/,
    'PrintWindow leaves the OpenGL canvas blank, so the canvas saves its own frame');
  assert.match(canvas, /'canvas_png': png if saved else None/, 'a build that saves nothing is recorded as such');
  assert.match(canvas, /'result': 'canvas-capture'/);
  assert.match(canvas, /continue/, 'no dialog handling follows');
});

test('a catalogue label with a placeholder still gives a Cantonese menu form', () => {
  const fixed = sweep.slice(sweep.indexOf('def fixed_words(label):'), sweep.indexOf('\ndef cantonese_form('));
  assert.match(fixed, /re\.sub\(r'\\\(&\[A-Za-z0-9\]\\\)\$', '', /, 'a trailing Cantonese mnemonic such as (&A) is dropped');
  assert.match(fixed, /re\.sub\(r'%\[sd\]', '', text\)/, 'printf placeholders are dropped');
  const form = sweep.slice(sweep.indexOf('def cantonese_form('), sweep.indexOf('\ndef findings_in('));
  assert.match(form, /if '%' in k and fixed_words\(k\) == entry/, '"&About %s" answers for About');
  assert.match(form, /return fixed_words\(yue\) if yue else None/);
  assert.match(sweep, /yue = cantonese_form\(entry, cantonese\)\n\s*if yue and yue != entry:\n\s*forms\.append\(yue\)/,
    'dialog entries use the same lookup');
});
