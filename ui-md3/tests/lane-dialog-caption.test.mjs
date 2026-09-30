import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Dialogs that still wore the native Windows title bar, and the sweep that keeps the
// rest of the GUI honest.
//
// The older check in md3-conversion-contracts.test.mjs only asks "does this FILE
// mention the kit caption anywhere". A file that holds one converted dialog and one
// native dialog passes it, and so does a dialog built with the two-argument wxDialog
// constructor, which gets the default style without ever writing wxCAPTION. This file
// asks the question per class: every dialog class must reach the kit caption from its
// own constructor, directly or through the member functions the constructor calls.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const guiDir = path.join(repoDir, 'src', 'slic3r', 'GUI');

// Short failure messages: a regex assertion over a whole constructor would print the
// constructor, which buries the one line that matters.
const expect = (condition, message) => assert.ok(condition, message);

// Comments and the inside of string and character literals are not behaviour, and a brace
// or a parenthesis inside a literal would fool the matcher below. Comments become spaces
// and every literal collapses to its bare quotes; the line structure is kept.
function clean(text) {
  const src = text.replace(/\r\n/g, '\n');
  let out = '';
  let i = 0;
  while (i < src.length) {
    const c = src[i];
    const n = src[i + 1];
    if (c === '/' && n === '/') {
      while (i < src.length && src[i] !== '\n') i++;
      continue;
    }
    if (c === '/' && n === '*') {
      const e = src.indexOf('*/', i + 2);
      const end = e < 0 ? src.length : e + 2;
      out += src.slice(i, end).replace(/[^\n]/g, ' ');
      i = end;
      continue;
    }
    if (c === 'R' && n === '"') {
      const open = src.indexOf('(', i + 2);
      if (open > 0) {
        const delim = src.slice(i + 2, open);
        const close = src.indexOf(`)${delim}"`, open);
        if (close > 0) {
          out += '""';
          i = close + delim.length + 2;
          continue;
        }
      }
    }
    // A quote right after a digit or a hex letter is a C++14 digit separator, not a literal.
    if (c === '"' || (c === "'" && !/[0-9A-Fa-f]/.test(src[i - 1] ?? ''))) {
      let j = i + 1;
      while (j < src.length && src[j] !== c) {
        if (src[j] === '\\') j++;
        j++;
      }
      out += c + c;
      i = j + 1;
      continue;
    }
    out += c;
    i++;
  }
  return out;
}

// Index of the bracket that closes the one at `open`; -1 when it never closes.
function matchClose(text, open, left, right) {
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    if (text[i] === left) depth++;
    else if (text[i] === right && --depth === 0) return i;
  }
  return -1;
}

const skipSpace = (text, i) => {
  while (i < text.length && /\s/.test(text[i])) i++;
  return i;
};

// Given the '(' that opens a constructor's parameter list, return the initializer list and
// the body, or null for a declaration, "= default" and "= delete". A constructor whose
// first initializer is the class itself only delegates, and is reported as such.
function constructorAt(text, paren, cls) {
  const paramsEnd = matchClose(text, paren, '(', ')');
  if (paramsEnd < 0) return null;
  let i = skipSpace(text, paramsEnd + 1);
  if (text[i] === ';' || text[i] === '=') return null;
  let init = '';
  let delegating = false;
  if (text[i] === ':') {
    const initStart = i;
    i++;
    for (;;) {
      i = skipSpace(text, i);
      const name = /^[\w:]+(?:<[^<>(){};]*>)?/.exec(text.slice(i, i + 200));
      if (!name) return null;
      if (init === '' && name[0] === cls) delegating = true;
      i = skipSpace(text, i + name[0].length);
      const left = text[i];
      if (left !== '(' && left !== '{') return null;
      const end = matchClose(text, i, left, left === '(' ? ')' : '}');
      if (end < 0) return null;
      i = skipSpace(text, end + 1);
      init = text.slice(initStart, i);
      if (text[i] === ',') {
        i++;
        continue;
      }
      break;
    }
  }
  i = skipSpace(text, i);
  if (text[i] !== '{') return null;
  const end = matchClose(text, i, '{', '}');
  if (end < 0) return null;
  return { init, body: text.slice(i, end + 1), delegating, end: end + 1 };
}

// The text of each class body declared for `cls` (forward declarations have no braces).
function classBodies(cls, files) {
  const bodies = [];
  const decl = new RegExp(`\\b(?:class|struct)\\s+${cls}\\b[^;{]*\\{`, 'g');
  for (const { text } of files) {
    for (const d of text.matchAll(decl)) {
      const open = d.index + d[0].length - 1;
      const close = matchClose(text, open, '{', '}');
      if (close > 0) bodies.push(text.slice(open, close + 1));
    }
  }
  return bodies;
}

// Offsets inside a class body that sit directly in it, not in a nested type or a function.
const depthAt = (body, index) => [...body.slice(0, index)].reduce((n, ch) => n + (ch === '{') - (ch === '}'), 0);

// Every defined constructor of `cls`: out-of-line (Cls::Cls(...)) and inline in the class.
function constructorsOf(cls, files) {
  const found = [];
  const outOfLine = new RegExp(`\\b${cls}::${cls}\\s*\\(`, 'g');
  for (const { text } of files) {
    for (const m of text.matchAll(outOfLine)) {
      const ctor = constructorAt(text, m.index + m[0].length - 1, cls);
      if (ctor) found.push(ctor);
    }
  }
  const inline = new RegExp(`(?<![~\\w:])${cls}\\s*\\(`, 'g');
  for (const body of classBodies(cls, files)) {
    let resume = 0; // a delegating constructor names the class again inside its own initializer list
    for (const m of body.matchAll(inline)) {
      if (m.index < resume || depthAt(body, m.index) !== 1) continue;
      const ctor = constructorAt(body, m.index + m[0].length - 1, cls);
      if (!ctor) continue;
      found.push(ctor);
      resume = ctor.end;
    }
  }
  return found;
}

// The body of member function `name` of `cls`: out-of-line (Cls::name(...) { }) and inline.
function methodBody(cls, name, files) {
  const bodies = [];
  const QUALIFIER = /^(?:const|override|noexcept|final)(?!\w)/;
  const bodyAfter = (text, paramsEnd) => {
    let i = skipSpace(text, paramsEnd + 1);
    for (let q = QUALIFIER.exec(text.slice(i, i + 10)); q; q = QUALIFIER.exec(text.slice(i, i + 10)))
      i = skipSpace(text, i + q[0].length);
    if (text[i] !== '{') return null;
    const end = matchClose(text, i, '{', '}');
    return end < 0 ? null : text.slice(i, end + 1);
  };
  const outOfLine = new RegExp(`\\b${cls}::${name}\\s*\\(`, 'g');
  for (const { text } of files) {
    for (const m of text.matchAll(outOfLine)) {
      const paramsEnd = matchClose(text, m.index + m[0].length - 1, '(', ')');
      const body = paramsEnd < 0 ? null : bodyAfter(text, paramsEnd);
      if (body) bodies.push(body);
    }
  }
  const inline = new RegExp(`(?<![\\w:.>~])${name}\\s*\\(`, 'g');
  for (const classBody of classBodies(cls, files)) {
    for (const m of classBody.matchAll(inline)) {
      if (depthAt(classBody, m.index) !== 1) continue;
      // An initializer item (": Base(...)" or ", m_x(...)") is not a member function definition.
      const before = classBody.slice(0, m.index).trimEnd().slice(-1);
      if (before === ':' || before === ',') continue;
      const paramsEnd = matchClose(classBody, m.index + m[0].length - 1, '(', ')');
      const body = paramsEnd < 0 ? null : bodyAfter(classBody, paramsEnd);
      if (body) bodies.push(body);
    }
  }
  return bodies;
}

const ADOPTS = /MD3DialogCaption\s*::\s*(?:Adopt|FinishChrome)\s*\(|\bFinishChrome\s*\(|\bMD3DialogCaption\s*\(|\bMD3Dialog\s*\(/;

// Does this code, or any member function of `cls` it calls (followed three calls deep),
// reach the kit caption? A constructor that hands its layout to Create() or build() is
// still the constructor adopting.
function reachesCaption(cls, code, files, depth = 3, seen = new Set()) {
  if (ADOPTS.test(code)) return true;
  if (depth === 0) return false;
  for (const call of code.matchAll(/(?<![\w:.>~])(\w+)\s*\(/g)) {
    const name = call[1];
    if (seen.has(name)) continue;
    seen.add(name);
    for (const body of methodBody(cls, name, files))
      if (reachesCaption(cls, body, files, depth - 1, seen)) return true;
  }
  return false;
}

// Every dialog class that derives the stock wx dialog or the DPI-aware one directly. A class on
// the kit's own MD3Dialog base is not matched: that base draws the strip itself.
const DIALOG_BASES = new Set(['wxDialog', 'DPIDialog', 'GUI::DPIDialog', 'Slic3r::GUI::DPIDialog', 'DPIAware<wxDialog>']);

function dialogClasses(files) {
  const classes = [];
  const pattern = /\b(?:class|struct)\s+(\w+)\s*(?:final\s*)?:\s*(?:public|protected|private)?\s*([\w:]+(?:<\w+>)?)\s*(?=[{\s/])/g;
  for (const { file, text } of files) {
    for (const m of text.matchAll(pattern))
      if (DIALOG_BASES.has(m[2])) classes.push({ name: m[1], file });
  }
  return classes;
}

// Only the files that mention the class can define any part of it.
function adoptionReport(cls, allFiles) {
  const files = allFiles.filter((f) => f.text.includes(cls));
  const ctors = constructorsOf(cls, files).filter((c) => !c.delegating);
  // The initializer list can name the kit base (MD3Dialog(...)); only the body is followed into
  // the member functions it calls.
  const adopts = (c) => ADOPTS.test(c.init) || reachesCaption(cls, c.body, files);
  return { ctors: ctors.length, missing: ctors.filter((c) => !adopts(c)).length };
}

let cachedFiles;
async function guiFiles() {
  if (cachedFiles) return cachedFiles;
  const files = [];
  async function walk(dir) {
    for (const entry of await readdir(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        await walk(full);
      } else if (/\.(cpp|hpp|h)$/.test(entry.name)) {
        const raw = await readFile(full, 'utf8');
        files.push({ file: path.relative(guiDir, full).replaceAll('\\', '/'), raw, text: clean(raw) });
      }
    }
  }
  await walk(guiDir);
  cachedFiles = files;
  return files;
}

const fileNamed = async (name) => (await guiFiles()).find((f) => f.file === name);

// Dialog classes that reach the caption somewhere other than their own constructor, that draw
// no native title bar, or that no person can open, each with the reason the sweep leaves them
// out. Adding a class here is a decision somebody has to justify in this list, never a way to
// turn the sweep green.
const ALLOWED_WITHOUT_CAPTION = new Map([
  ['AMSTraySettingBase', 'abstract base: AMSMaterialsSetting, the derived class, adopts in its own constructor'],
  ['AmsControlWebDebugDialog', 'developer-only page of the web AMS view, adopts outside its constructor, in the function that builds it'],
  ['BedShapeDialog', 'its constructor is empty; build_dialog(), which every caller runs before showing it, adopts'],
  ['CommandPalette', 'borderless (wxBORDER_SIMPLE) quick-open overlay with no title bar, owned by the message boxes and frames lane'],
  ['DPIDialog', 'the DPI-aware base class itself: it draws no chrome and has no constructor of its own'],
  ['DownPluginFrame', 'plug-in download page that nothing constructs, so no person can reach it'],
  ['FilamentPickerDialog', 'borderless popup chooser (wxBORDER_NONE), so there is no native title bar to replace'],
  ['ImportDlg', 'SLA archive import: Plater::import_sl1_archive has no caller, so no person can reach it'],
  ['MD3Dialog', 'the kit dialog base: it draws its own header strip'],
  ['SettingsDialog', 'frame-style settings window (wxDEFAULT_FRAME_STYLE) with its own chrome'],
]);

// The five dialogs this lane converted, written out by hand. The per-class sweep below catches a
// dialog that does the thing wrongly; it cannot catch a dialog that was never listed, so the
// list of what must be converted lives here in full.
const CONVERTED = [
  { cls: 'ZUserLogin', file: 'WebUserLoginDialog.cpp', title: 'Login', step: null, adopts: 2 },
  { cls: 'uiAmsPercentHumidityDryPopup', file: 'DeviceTab/uiAmsHumidityPopup.cpp', title: 'Current AMS humidity', step: 'Create', adopts: 1 },
  { cls: 'TextureImportAddFilamentDialog', file: 'TextureImportDialog.cpp', title: 'Add Filament', step: null, adopts: 1 },
  { cls: 'FilaManagerPromptDialog', file: 'AMSMaterialsSetting.cpp', title: 'Add to Filament Library?', step: null, adopts: 1 },
  { cls: 'AMSNewFilamentRecordedDlg', file: 'AMSMaterialsSetting.cpp', title: 'New Filament', step: null, adopts: 1 },
];

test('the five dialogs that wore the native title bar adopt the kit caption with their own title', async () => {
  const files = await guiFiles();
  for (const { cls, file, title, step, adopts } of CONVERTED) {
    const bodies = step ? methodBody(cls, step, files) : constructorsOf(cls, files).map((c) => c.init + c.body);
    expect(bodies.length === 1, `${cls}: expected exactly one ${step ?? 'constructor'} body, found ${bodies.length}`);
    const calls = bodies[0].match(/MD3DialogCaption::Adopt\(/g) || [];
    expect(calls.length === adopts, `${cls}: ${step ?? 'the constructor'} must call MD3DialogCaption::Adopt ${adopts} time(s), found ${calls.length}`);
    // The title is a string literal, which the cleaned text no longer holds: read it from the file.
    const raw = (await fileNamed(file)).raw;
    const escaped = title.replace(/[?]/g, '\\?');
    const titled = raw.match(new RegExp(`MD3DialogCaption::Adopt\\(this, _L\\("${escaped}"\\)\\)`, 'g')) || [];
    expect(titled.length === adopts, `${cls}: Adopt must carry the existing message "${title}" (${titled.length} of ${adopts} found)`);
    expect(/#include\s+"(?:slic3r\/GUI\/)?Widgets\/MD3DialogChrome\.hpp"/.test(raw), `${file} must include Widgets/MD3DialogChrome.hpp`);
  }
});

test('the titles the strips show are messages the catalogue already has, so no new string is added', async () => {
  const pot = (await readFile(path.join(repoDir, 'bbl', 'i18n', 'BambuStudio.pot'), 'utf8')).replace(/\r\n/g, '\n');
  for (const { title } of CONVERTED)
    expect(pot.includes(`\nmsgid "${title}"\n`), `msgid "${title}" must already exist in bbl/i18n/BambuStudio.pot`);
});

test('per-class sweep: every dialog class reaches the kit caption from its constructor', async () => {
  const files = await guiFiles();
  const classes = dialogClasses(files);
  expect(classes.length > 100, `the sweep must actually find the dialog classes (found ${classes.length})`);
  const offenders = [];
  for (const { name, file } of classes) {
    if (ALLOWED_WITHOUT_CAPTION.has(name)) continue;
    const { ctors, missing } = adoptionReport(name, files);
    if (ctors === 0) offenders.push(`${name} (${file}): no constructor body found, so the sweep cannot vouch for it`);
    else if (missing > 0) offenders.push(`${name} (${file}): ${missing} of ${ctors} constructors never reach the kit caption`);
  }
  assert.deepEqual(offenders, []);
});

test('every allowlisted dialog class still exists and still needs its entry', async () => {
  const files = await guiFiles();
  const names = new Set(dialogClasses(files).map((c) => c.name));
  for (const [name, reason] of ALLOWED_WITHOUT_CAPTION) {
    expect(names.has(name), `${name} is allowlisted (${reason}) but no longer derives a dialog base`);
    expect(reason.length > 20, `${name} needs a real reason`);
    // An entry that the sweep would pass anyway is stale: the constructor adopts now.
    const { ctors, missing } = adoptionReport(name, files);
    expect(ctors === 0 || missing > 0, `${name} now adopts the caption in its constructor, so it no longer needs an allowlist entry`);
  }
});

test('the Add to Filament Library prompt treats the caption close as Cancel', async () => {
  // The kit caption ends the modal with wxID_CANCEL, which is none of the class's own
  // RESULT_ values (they start at wxID_HIGHEST + 1). A caller that only bails out on
  // RESULT_CANCEL would read the caption X as "Save Only" and unbind the spool.
  const { text } = await fileNamed('AMSMaterialsSetting.cpp');
  const at = text.indexOf('FilaManagerPromptDialog dlg(this);');
  expect(at !== -1, 'the caller builds the prompt on the stack');
  const caller = text.slice(at, at + 400);
  expect(/if\s*\(\s*result\s*!=\s*FilaManagerPromptDialog::RESULT_ADD_TO_LIBRARY\s*&&\s*result\s*!=\s*FilaManagerPromptDialog::RESULT_SAVE_ONLY\s*\)\s*return;/.test(caller),
    'only the two explicit choices (Add to Library, Save Only) may continue');
  expect(!/result\s*==\s*FilaManagerPromptDialog::RESULT_CANCEL/.test(caller),
    'comparing against RESULT_CANCEL alone lets the caption close fall through to Save Only');
});

test('the sign-in window keeps its web view under the caption strip', async () => {
  const ctor = constructorsOf('ZUserLogin', await guiFiles())[0].body;
  expect(!/m_browser->SetSize\(0,\s*0\)/.test(ctor), 'a web view sized 0 x 0 has nothing for the root sizer to restore');
  expect(/m_browser->Hide\(\);/.test(ctor), 'the web view is still created hidden until the page has loaded');
  const adopts = [...ctor.matchAll(/MD3DialogCaption::Adopt\(/g)].map((m) => m.index);
  expect(adopts.length === 2, 'both the web view and the missing plug-in notice adopt the strip');
  // Adopt wraps the root sizer, so the web view has to sit in one first, and the window is placed
  // from its final size because the strip grows the frame.
  const sizer = ctor.indexOf('Add(m_browser, 1, wxEXPAND)');
  const setSizer = ctor.indexOf('SetSizer(browser_sizer)');
  expect(sizer > 0 && setSizer > sizer, 'the web view fills a root sizer');
  expect(setSizer < adopts[1], 'the root sizer exists before the strip is adopted');
  const move = ctor.indexOf('Move(tmpPT)');
  expect(move > adopts[1], 'the window is placed after the strip has changed its size');
  expect(/pSize\s*=\s*GetSize\(\);/.test(ctor.slice(adopts[1], move)), 'the placement uses the size the frame has now');
  // Both the modal result and the modal end stay as they were: the strip close ends the modal
  // with wxID_CANCEL, which ZUserLogin::run already reads as "not signed in".
  const source = (await fileNamed('WebUserLoginDialog.cpp')).text;
  expect(/ShowModal\(\)\s*==\s*wxID_OK/.test(source), 'run() still treats only wxID_OK as success');
});

test('the humidity popup has one title, theme colours and a size that includes the strip', async () => {
  const files = await guiFiles();
  const { raw, text } = await fileNamed('DeviceTab/uiAmsHumidityPopup.cpp');
  const create = methodBody('uiAmsPercentHumidityDryPopup', 'Create', files)[0];
  expect((raw.match(/_L\("Current AMS humidity"\)/g) || []).length === 1, 'the title appears once, in the caption strip');
  expect(!/\*wxWHITE|\*wxBLACK/.test(text), 'no fixed white body or black text that stays bright in dark mode');
  expect(/SetBackgroundColour\(surface\)/.test(create) && /const wxColour surface = StateColor::semantic\(MD3::Role::SurfaceContainerLowest\);/.test(create),
    'the body sits on the kit surface role');
  expect(/SetForegroundColour\(StateColor::semantic\(MD3::Role::OnSurface\)\)/.test(create), 'the drying label reads in the kit text role');
  // The strip adds its own height, so the footprint is a floor on the body sizer, taken before
  // Adopt; a frame-sized pin made before the strip existed would push the last row out.
  const floor = create.indexOf('m_sizer->SetMinSize(FromDIP(400), FromDIP(270) - MD3DialogCaption::Height(this));');
  const adopt = create.indexOf('MD3DialogCaption::Adopt(');
  expect(floor > 0 && adopt > floor, 'the body floor leaves room for the strip and precedes Adopt');
  expect(!/SetM(?:in|ax)Size\(wxSize\(FromDIP\(400\), FromDIP\(270\)\)\)/.test(create), 'the frame pins made before the strip existed are gone');
  expect(/Refresh\(\);\s*}$/.test(create.trimEnd()) && create.indexOf('Fit();') < adopt, 'nothing is added to the window after Adopt');
});

test('the add-filament chooser adds the strip to its minimum height after Adopt', async () => {
  const ctor = constructorsOf('TextureImportAddFilamentDialog', await guiFiles())[0].body;
  const adopt = ctor.indexOf('MD3DialogCaption::Adopt(');
  const min = ctor.indexOf('SetMinSize(wxSize(GetSize().x, FromDIP(180) + MD3DialogCaption::Height(this)))');
  expect(adopt > 0 && min > adopt, 'the minimum height adds the strip and is taken after the frame changed');
  expect(!/SetMinSize\(wxSize\(GetSize\(\)\.x, FromDIP\(180\)\)\)/.test(ctor), 'the old minimum that counted the strip against the body is gone');
  expect(adopt > ctor.lastIndexOf('Fit();'), 'Adopt is after the last Fit, so it is the last layout act');
});

test('the recorded-filament notice sits on the kit surface and is centred after the strip', async () => {
  const files = await guiFiles();
  const create = methodBody('AMSNewFilamentRecordedDlg', 'create', files)[0];
  expect(!/\*wxWHITE/.test(create), 'no fixed white body');
  expect(/SetBackgroundColour\(StateColor::semantic\(MD3::Role::SurfaceContainerLowest\)\)/.test(create), 'the body sits on the kit surface role');
  const ctor = constructorsOf('AMSNewFilamentRecordedDlg', files)[0].body;
  expect(/MD3DialogCaption::Adopt\([^;]*\);\s*Centre\(\);\s*}$/.test(ctor.trimEnd()), 'Adopt is the last layout act and centring follows it');
});

// The scanner is what the sweep stands on, so it is tested on its own against source shaped
// like the real thing: a constructor whose initializer list holds braces and quotes, an inline
// constructor, one that adopts through a member function it calls, and one whose only mention
// of the caption sits in a comment.
test('the scanner tells a converted dialog from a native one', () => {
  const source = clean(`
    class Native : public wxDialog { public: Native(wxWindow* p) : wxDialog(p, wxID_ANY, "t") { SetSizer(new wxBoxSizer(wxVERTICAL)); } };
    class Kit : public DPIDialog { public: Kit(wxWindow* p) : DPIDialog(p, wxID_ANY, "t", wxDefaultPosition, wxDefaultSize, wxCAPTION), m_x{ 0 }
      { MD3DialogCaption::Adopt(this, "t"); } int m_x; };
    class Later : public wxDialog { public: Later(wxWindow* p); void Create(); void build(); };
    Later::Later(wxWindow* p) : wxDialog(p, wxID_ANY, "{") { Create(); }
    void Later::Create() { build(); }
    void Later::build() { MD3DialogCaption::Adopt(this, "}"); }
    class Bare : public wxDialog { public: Bare(wxWindow* p); };
    Bare::Bare(wxWindow* p) : wxDialog(p, wxID_ANY, "t") { /* MD3DialogCaption::Adopt(this); */ }
    class Twice : public wxDialog { public: Twice(int a) : wxDialog(nullptr, wxID_ANY, "t") { MD3DialogCaption::Adopt(this); }
      Twice() : Twice(1) {} Twice(const char* s) : wxDialog(nullptr, wxID_ANY, s) { Layout(); } };
  `);
  const files = [{ file: 'fixture.cpp', text: source }];
  assert.deepEqual(dialogClasses(files).map((c) => c.name), ['Native', 'Kit', 'Later', 'Bare', 'Twice']);
  assert.deepEqual(adoptionReport('Native', files), { ctors: 1, missing: 1 });
  assert.deepEqual(adoptionReport('Kit', files), { ctors: 1, missing: 0 });
  assert.deepEqual(adoptionReport('Later', files), { ctors: 1, missing: 0 }, 'adopting two calls deep still counts');
  assert.deepEqual(adoptionReport('Bare', files), { ctors: 1, missing: 1 }, 'a commented-out call is not an adoption');
  assert.deepEqual(adoptionReport('Twice', files), { ctors: 2, missing: 1 },
    'a delegating constructor is skipped and a second constructor that skips the strip is still caught');
});
