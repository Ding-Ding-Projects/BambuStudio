import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// md3-v230 crashed on every GUI start, at the same instruction inside
// Plater::request_sidebar_width. Plater::Plater built priv in its initializer
// list, priv built the Sidebar in its own, and the Sidebar constructor ended
// with SettingsDraftPanel::Activate(saved_section). Activate always ran the
// sidebar's show_page callback, which asked the Plater for a sidebar width.
// At that moment Plater::p had not been constructed yet (it held whatever bytes
// that memory last contained), and priv's sidebar and dock manager did not exist. A start after
// a force-stopped run read 0 there and faulted at 0x268; the first runs read
// garbage and faulted too, behind a splash screen.
//
// The build cannot run the GUI here, so these checks pin the construction order
// in the source.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const gui = path.join(repoDir, 'src', 'slic3r', 'GUI');
const read = async (name) => (await readFile(path.join(gui, name), 'utf8')).replace(/\r\n/g, '\n');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
const plater = stripComments(await read('Plater.cpp'));
const platerHpp = stripComments(await read('Plater.hpp'));
const draftCpp = stripComments(await read('SettingsDraftPanel.cpp'));
const draftHpp = stripComments(await read('SettingsDraftPanel.hpp'));

// Text of a function definition: from its signature to the first closing brace
// at column zero.
const body = (code, signature) => {
  const at = code.indexOf(signature);
  assert.notEqual(at, -1, `missing ${signature}`);
  const end = code.indexOf('\n}', at);
  assert.notEqual(end, -1, `unterminated ${signature}`);
  return code.slice(at, end + 2);
};

// Drop every brace-balanced lambda body, leaving the statements the function
// runs itself.
const withoutLambdas = (code) => {
  let out = '';
  let i = 0;
  const opener = /\[[^\[\]]*\]\s*\([^()]*\)\s*(?:mutable\s*)?(?:->\s*[\w:<>]+\s*)?\{/g;
  for (;;) {
    opener.lastIndex = i;
    const m = opener.exec(code);
    if (!m) return out + code.slice(i);
    out += code.slice(i, m.index) + '[lambda]';
    let depth = 1;
    let j = m.index + m[0].length;
    for (; j < code.length && depth > 0; ++j) {
      if (code[j] === '{') ++depth;
      else if (code[j] === '}') --depth;
    }
    i = j;
  }
};

test('Plater::p is null, not leftover memory, while priv is being built', () => {
  const ctor = body(plater, 'Plater::Plater(wxWindow *parent, MainFrame *main_frame)');
  const [init, rest] = ctor.split(/\n\{/);
  assert.ok(rest, 'Plater::Plater has a body');
  assert.doesNotMatch(init, /\bp\s*\(\s*new\s+priv\b/, 'priv built in the initializer list leaves p unwritten while priv runs');
  const statements = rest.trim().split(';').map((s) => s.trim()).filter(Boolean);
  assert.equal(statements[0], 'p.reset(new priv(this, main_frame))', 'priv is the first thing the body builds');
});

test('a sidebar width request is refused until the Plater has its priv', () => {
  const request = body(plater, 'bool Plater::request_sidebar_width(int width_px, bool grow_only)');
  const guard = request.search(/if \(!p\)\s*return false;/);
  const firstUse = request.search(/p->/);
  assert.notEqual(guard, -1, 'request_sidebar_width checks p first and reports "not ready"');
  assert.ok(guard < firstUse, 'the check comes before the first p-> dereference');
});

test('the Sidebar constructor never runs the draft panel host callback', () => {
  const ctor = withoutLambdas(body(plater, 'Sidebar::Sidebar(Plater *parent)'));
  assert.doesNotMatch(ctor, /m_draft_panel->Activate\(/, 'Activate runs show_page, which reaches the Plater');
  assert.doesNotMatch(ctor, /request_sidebar_width\(/, 'no width request while the Plater is under construction');
  assert.match(ctor, /const bool restored_draft = p->m_draft_panel->Restore\(saved_section\);/);
  assert.match(ctor, /p->scrolled->Show\(!restored_draft\);\s*p->m_draft_panel->Show\(restored_draft\);/);
  assert.match(ctor, /if \(!restored_draft\)\s*apply_prepare_section\(saved_section\.empty\(\) \? "ink" : saved_section\);/);

  assert.match(draftHpp, /bool Restore\(const std::string &id\);/);
  const restore = body(draftCpp, 'bool SettingsDraftPanel::Restore(const std::string &id)');
  assert.doesNotMatch(restore, /m_show_page/, 'Restore leaves showing the page to the host');
  assert.match(restore, /m_active = id; Rebuild\(\);/);
});

test('switching between ordinary sections does not call the host', () => {
  const activate = body(draftCpp, 'bool SettingsDraftPanel::Activate(const std::string &id)');
  const miss = activate.match(/if \(!m_store\.find\(id\)\) \{([\s\S]*?)return false;/);
  assert.ok(miss, 'Activate has a not-a-draft branch');
  assert.match(miss[1], /if \(IsShown\(\)\) m_show_page\(false\);/, 'only a visible draft page is handed back');
});

test('a restored draft page gets the wide dock from the first laid-out size', () => {
  assert.match(platerHpp, /bool is_draft_page_shown\(\) const;/);
  assert.match(plater, /bool Sidebar::is_draft_page_shown\(\) const \{ return p->m_draft_panel && p->m_draft_panel->IsShown\(\); \}/);
  const latch = plater.match(/this->q->Bind\(wxEVT_SIZE, \[this\]\(wxSizeEvent &e\) \{[\s\S]*?m_advanced_width_applied = true;/);
  assert.ok(latch, 'the startup size latch exists');
  assert.match(latch[0], /this->sidebar->is_process_advanced\(\) \|\| this->sidebar->is_draft_page_shown\(\)/);
});
