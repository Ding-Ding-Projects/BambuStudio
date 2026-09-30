import assert from 'node:assert/strict';
import { readdir, readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// One system message box, one start-up warning box and two bordered frames moved
// onto the kit:
//   1. Plater::priv::get_export_file asked "replace the file?" with a raw Win32
//      MessageBox; it now asks through the Material message box.
//   2. The export progress card was a wxFRAME with a 1 px system border and a
//      hard-coded white fill; it is now a borderless shaped frame on the
//      SurfaceContainerHigh role, like the busy notice.
//   3. The command palette was a dialog with a 1 px system border; it is now
//      borderless with the shared dialog chrome (rounded corners and one fade).
//   4. The blacklisted-library warning was a system box shown before any window
//      existed, and it blocked scripted command-line runs; it is now logged at
//      setup and shown by the main window with the Material message dialog.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const srcDir = path.join(repoDir, 'src');
const guiDir = path.join(srcDir, 'slic3r', 'GUI');

// Removes comments, and with `blankStrings` also the contents of string and
// character literals (raw strings included), so a name that only appears in a
// comment or in a message text never counts as code.
function strip(text, { blankStrings = false } = {}) {
  const src = text.replace(/\r\n/g, '\n');
  let out = '';
  let i = 0;
  while (i < src.length) {
    const c = src[i];
    const next = src[i + 1];
    if (c === '/' && next === '/') {
      while (i < src.length && src[i] !== '\n') i++;
    } else if (c === '/' && next === '*') {
      const end = src.indexOf('*/', i + 2);
      i = end === -1 ? src.length : end + 2;
    } else if (c === 'R' && next === '"' && /[^\w]/.test(src[i - 1] || ' ')) {
      const open = src.indexOf('(', i + 2);
      const delimiter = open === -1 ? '' : src.slice(i + 2, open);
      const close = open === -1 ? -1 : src.indexOf(`)${delimiter}"`, open + 1);
      if (close === -1) { out += c; i++; continue; }
      const end = close + delimiter.length + 2;
      out += blankStrings ? '""' : src.slice(i, end);
      i = end;
    } else if (c === '"' || (c === "'" && !/[0-9a-fA-F]/.test(src[i - 1] || ' '))) {
      let j = i + 1;
      while (j < src.length && src[j] !== c && src[j] !== '\n') j += src[j] === '\\' ? 2 : 1;
      out += blankStrings ? `${c}${c}` : src.slice(i, j + 1);
      i = j + 1;
    } else {
      out += c;
      i++;
    }
  }
  return out;
}

const read = async (...parts) => readFile(path.join(srcDir, ...parts), 'utf8');
const code = async (...parts) => strip(await read(...parts));
const codeOnly = async (...parts) => strip(await read(...parts), { blankStrings: true });

function between(source, from, to) {
  const start = source.indexOf(from);
  assert.notEqual(start, -1, `found ${from}`);
  const end = source.indexOf(to, start + from.length);
  assert.notEqual(end, -1, `found ${to} after ${from}`);
  return source.slice(start, end);
}

async function sources(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await sources(full));
    else if (/\.(cpp|hpp|h)$/.test(entry.name)) out.push(full);
  }
  return out;
}

test('the scanner itself ignores comments and strings but sees real calls', () => {
  const sample = [
    '// MessageBox(nullptr, a, b, 0);',
    '/* MessageBoxW(nullptr, a, b, 0); */',
    'const char *s = "MessageBoxA( in a string";',
    'const char *r = R"x(MessageBox( in a raw string)x";',
    '#define MessageBox MessageBoxA',
    'int real = ::MessageBoxW(nullptr, a, b, 0);',
  ].join('\n');
  const found = strip(sample, { blankStrings: true }).match(/\bMessageBox[AW]?\s*\(/g) || [];
  assert.deepEqual(found, ['MessageBoxW(']);
});

test('no raw Win32 message box call is left anywhere under src/slic3r/GUI', async () => {
  const found = [];
  for (const file of await sources(guiDir)) {
    const text = strip(await readFile(file, 'utf8'), { blankStrings: true });
    const hits = text.match(/\bMessageBox[AW]?\s*\(/g) || [];
    if (hits.length) found.push(`${path.relative(guiDir, file).replace(/\\/g, '/')}: ${hits.length}`);
  }
  assert.deepEqual(found, [], 'a raw MessageBox, MessageBoxA or MessageBoxW call shows the system box');
});

test('the save-as replace question goes through the Material message box', async () => {
  const plater = await code('slic3r', 'GUI', 'Plater.cpp');
  const body = between(plater, 'wxString Plater::priv::get_export_file(', 'const Selection& Plater::priv::get_selection() const');
  assert.match(body, /\bmd3_message_box\(/, 'the question is asked through md3_message_box');
  assert.match(body, /wxYES_NO \| wxICON_WARNING/, 'still a warning with Yes and No');
  assert.match(body, /\bq\s*\)\s*;/, 'the Plater is the parent of the box');
  assert.match(body, /!=\s*wxYES\b/, 'the answer is compared with wxYES, the value md3_message_box returns');
  assert.doesNotMatch(body, /\bIDYES\b|\bIDNO\b/, 'no system box id is mixed with the wx answers');
  assert.doesNotMatch(body, /\bMessageBox[AW]?\s*\(/, 'no raw system box');
  // The extension handling stays Windows only, and both message ids are unchanged
  // (the second one is spelled that way in every catalogue).
  assert.match(body, /#ifdef __WXMSW__[\s\S]*md3_message_box\([\s\S]*#endif/);
  assert.ok(body.includes('_L("The file %s already exists\\nDo you want to replace it?")'), 'message id unchanged');
  assert.ok(body.includes('_L("Comfirm Save As")'), 'caption id unchanged');
  const pot = (await readFile(path.join(repoDir, 'bbl', 'i18n', 'BambuStudio.pot'), 'utf8')).replace(/\r\n/g, '\n');
  assert.ok(pot.includes('msgid "Comfirm Save As"'), 'the catalogue still carries the caption');
  assert.ok(pot.includes('msgid ""\n"The file %s already exists\\n"\n"Do you want to replace it?"'), 'the catalogue still carries the message');
});

test('the export progress card is a borderless, shaped frame on Material roles', async () => {
  const source = await code('slic3r', 'GUI', 'Overview', 'AssemblyExportProgressWindow.cpp');
  const plain = await codeOnly('slic3r', 'GUI', 'Overview', 'AssemblyExportProgressWindow.cpp');
  assert.doesNotMatch(plain, /wxBORDER_SIMPLE/, 'no 1 px system border line');
  assert.match(plain, /wxFRAME_NO_TASKBAR \| wxFRAME_SHAPED \| wxBORDER_NONE \| wxSTAY_ON_TOP/, 'borderless; shaped so SetShape takes effect');
  assert.doesNotMatch(source, /wxColour\(\s*255\s*,\s*255\s*,\s*255\s*\)/, 'no hard-coded white plate');
  assert.doesNotMatch(source, /wxColour\(\s*107\s*,\s*107\s*,\s*107\s*\)/, 'no hard-coded grey text');
  assert.doesNotMatch(source, /\bbtn_bg\b|\bbtn_bd\b|\bbtn_txt\b/, 'the unused button colour locals are gone');
  assert.match(source, /MD3::Role::SurfaceContainerHigh/, 'the fill follows the theme');
  assert.match(source, /MD3::Role::OnSurface\b/);
  assert.match(source, /MD3::Role::OnSurfaceVariant/);
  // The rounded silhouette, redone when the size changes (update_progress fits the
  // frame on every tick) and not otherwise.
  const shape = between(source, 'void AssemblyExportProgressWindow::apply_shape()', '\n}\n');
  assert.match(shape, /DrawRoundedRectangle\([^;]*MD3::Metrics::radius_dialog/);
  assert.match(shape, /SetShape\(region\)/);
  assert.match(shape, /size == m_shape_size/, 'unchanged size keeps the region');
  const progress = between(source, 'void AssemblyExportProgressWindow::update_progress(', 'void AssemblyExportProgressWindow::position_near_anchor(');
  assert.ok(progress.indexOf('Fit();') !== -1 && progress.indexOf('apply_shape();') > progress.indexOf('Fit();'), 'the shape follows every Fit()');
  assert.ok(progress.indexOf('apply_shape();') < progress.indexOf('position_near_anchor(anchor);'), 'positioned with the final size');
  const header = await code('slic3r', 'GUI', 'Overview', 'AssemblyExportProgressWindow.hpp');
  assert.match(header, /void apply_shape\(\);/);
  assert.match(header, /wxSize\s+m_shape_size;/);
  // The Cancel button keeps its variant and corner only (no colour setters), so the
  // Button styling ratchet in md3-conversion-contracts.test.mjs is not raised.
  assert.match(source, /m_cancel->SetVariant\(Button::Variant::Outlined\)/);
  assert.doesNotMatch(source, /m_cancel->Set(?:Background|Border|Text)Color/);
});

test('the command palette is borderless with the shared dialog chrome', async () => {
  const source = await code('slic3r', 'GUI', 'CommandPalette.cpp');
  const plain = await codeOnly('slic3r', 'GUI', 'CommandPalette.cpp');
  assert.doesNotMatch(plain, /wxBORDER_SIMPLE/, 'no 1 px system border line');
  const ctor = between(source, 'CommandPalette::CommandPalette(MainFrame *frame)', 'void CommandPalette::ShowPalette(');
  assert.match(ctor, /wxDefaultSize,\s*wxBORDER_NONE\)/, 'the dialog is created borderless');
  assert.match(source, /#include "Widgets\/MD3DialogChrome\.hpp"/);
  assert.match(ctor, /MD3DialogCaption::FinishChrome\(this\);\s*\}\s*$/, 'the chrome is finished at the very end of the constructor');
  assert.ok(ctor.indexOf('apply_size(') < ctor.indexOf('FinishChrome(this)'), 'after the sizing');
  // FinishChrome already plays the entrance fade: a second one would fight it.
  const show = between(source, 'void CommandPalette::ShowPalette(', 'void CommandPalette::apply_size(');
  assert.doesNotMatch(show, /MD3::Motion::FadeIn/, 'one entrance fade, played by FinishChrome');
  assert.match(show, /palette\.ShowModal\(\)/);
  assert.match(show, /CenterOnParent\(\)/, 'the card is still centred');
  // The Esc handling and the Card versus FullWindow sizing are untouched.
  assert.match(source, /case WXK_ESCAPE: dismiss\(\); return;/);
  assert.match(source, /PaletteIndex::PaletteSize::FullWindow/);
  assert.match(source, /SetSize\(wxRect\(origin, area\.GetSize\(\)\)\)/);
});

test('the blacklisted-library warning is logged at setup and shown by the main window', async () => {
  const cli = await code('BambuStudio.cpp');
  const setup = between(cli, 'bool CLI::setup(int argc, char **argv)', 'boost::filesystem::path install_path;');
  assert.doesNotMatch(setup, /\bMessageBox[AW]?\s*\(/, 'CLI::setup no longer opens a system box');
  assert.match(setup, /BlacklistedLibraryCheck::get_instance\(\)\.perform_check\(\)/, 'the detection stays at setup');
  assert.match(setup, /BOOST_LOG_TRIVIAL\(warning\)[^;]*blacklisted_library_warning_text\(\)/, 'the warning is logged instead');
  // The text is built once, by a helper defined before its two users.
  const helper = cli.indexOf('static std::wstring blacklisted_library_warning_text()');
  assert.notEqual(helper, -1);
  assert.ok(helper < cli.indexOf('int CLI::run('), 'defined before CLI::run uses it');
  assert.ok(helper < cli.indexOf('bool CLI::setup('), 'defined before CLI::setup uses it');
  const text = between(cli, 'static std::wstring blacklisted_library_warning_text()', '\n}\n');
  assert.match(text, /Following DLLs have been injected into the BambuStudio process/);
  assert.match(text, /which makes BambuStudio/);
  // Carried to the GUI through the init parameters, in the GUI branch only.
  const run = between(cli, 'int CLI::run(int argc, char **argv)', 'std::vector<std::string>    gcode_files;');
  assert.match(run, /params\.startup_warning\s*=\s*blacklisted_library_warning_text\(\);/);
  assert.ok(run.lastIndexOf('#ifdef SLIC3R_GUI') < run.indexOf('params.startup_warning'), 'inside the GUI branch');
  const init = await code('slic3r', 'GUI', 'GUI_Init.hpp');
  assert.match(init, /std::wstring\s+startup_warning;/, 'GUI_InitParams declares the field');
  assert.match(init, /#include <string>/);
  // Shown once the main window exists, through the kit dialog, not a system box.
  const app = await code('slic3r', 'GUI', 'GUI_App.cpp');
  const post = between(app, 'void GUI_App::post_init()', '\n}\n');
  assert.match(post, /init_params->startup_warning\.empty\(\)/);
  assert.match(post, /CallAfter\(\[this, warning = this->init_params->startup_warning\]/, 'the text is copied into the closure');
  assert.match(post, /MessageDialog dlg\(mainframe, wxString\(warning\.c_str\(\)\), _L\("Warning"\), wxOK \| wxICON_WARNING\);\s*dlg\.ShowModal\(\);/);
  // The system boxes GUI_App keeps are still exactly the three that must precede the kit.
  const system = app.split('\n').filter((line) => /\bwxMessageBox\s*\(/.test(line)).join('\n');
  assert.equal((system.match(/\bwxMessageBox\s*\(/g) || []).length, 3);
});
