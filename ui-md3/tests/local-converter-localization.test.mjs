import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The local file converter showed adapter names, change disclosures, unavailable
// reasons, queue states and result codes as untranslatable English straight from
// libslic3r, and none of its panel copy had a Cantonese entry. These tests pin the
// localized surface: every catalogue source reaches the message catalogue and has
// Hong Kong Cantonese, every stable result code has a translatable sentence, the
// panel translates registry text at display time, and only non-factual lines take
// a funny-level voice.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = async (relative) => (await readFile(path.join(repoDir, relative), 'utf8')).replace(/\r\n/g, '\n');
const stripComments = (text) => text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');

const PANEL = 'src/slic3r/GUI/LocalConverter/LocalConverterPanel.cpp';
const COPY = 'src/slic3r/GUI/LocalConverter/LocalConverterCopy.hpp';
const LIBRARY = 'src/libslic3r/LocalConverter/Converter.cpp';
const PRODUCERS = ['Converter.cpp', 'Worker.cpp', 'WorkerMain.cpp', 'ArchiveAdapter.cpp', 'PdfAdapter.cpp', 'PdfRequest.cpp', 'PdfPackage.cpp']
  .map((name) => `src/libslic3r/LocalConverter/${name}`);

const unescape = (s) => s.replace(/\\(.)/g, (_, c) => ({ n: '\n', t: '\t' }[c] ?? c));

// Minimal PO reader: msgid -> msgstr for entries without a context.
function parsePo(text) {
  const entries = new Map();
  let field = null;
  let current = {};
  const flush = () => {
    if (current.msgid !== undefined && current.msgctxt === undefined) entries.set(current.msgid, current.msgstr ?? '');
    current = {};
  };
  for (const line of text.split('\n')) {
    const start = line.match(/^(msgctxt|msgid|msgid_plural|msgstr(?:\[\d+\])?) "(.*)"$/);
    if (start) {
      if (start[1] === 'msgid' || (start[1] === 'msgctxt')) { if (current.msgstr !== undefined) flush(); }
      field = start[1].startsWith('msgstr') ? 'msgstr' : start[1];
      current[field] = (current[field] ?? '') + unescape(start[2]);
      continue;
    }
    const more = line.match(/^"(.*)"$/);
    if (more && field) { current[field] += unescape(more[1]); continue; }
    if (line.trim() === '') { flush(); field = null; }
  }
  flush();
  return entries;
}

// Catalogue sources marked with L() or _L() in a source file.
function catalogueSources(text) {
  const found = new Set();
  for (const m of stripComments(text).matchAll(/(?:^|[^A-Za-z0-9_])_?L\("((?:[^"\\]|\\.)*)"\)/g)) found.add(unescape(m[1]));
  return found;
}

// Stable result codes the converter can store or show.
function producedCodes(text) {
  const code = String.raw`"([a-z][a-z0-9_]*)"`;
  const patterns = [
    new RegExp(String.raw`\b(?:check|require)\([^;]*?,\s*${code}\s*\)`, 'g'),
    new RegExp(String.raw`\b(?:reject|fail|rejected)\(\s*${code}\s*\)`, 'g'),
    new RegExp(String.raw`record_rejected\([^;]*?,\s*${code}\s*\)`, 'g'),
    new RegExp(String.raw`Outcome::\w+\s*,\s*${code}`, 'g'),
    new RegExp(String.raw`\bcode\s*=\s*${code}`, 'g'),
    new RegExp(String.raw`Problem\{\s*${code}\s*\}`, 'g'),
    new RegExp(String.raw`\breason\s*=\s*${code}`, 'g'),
  ];
  const codes = new Set();
  for (const pattern of patterns) for (const m of stripComments(text).matchAll(pattern)) codes.add(m[1]);
  return codes;
}

test('the converter sources are extracted into the message catalogue', async () => {
  const list = (await read('bbl/i18n/list.txt')).split('\n').map((line) => line.trim());
  for (const file of [PANEL, COPY, LIBRARY]) assert.ok(list.includes(file), `${file} must be listed for xgettext`);
});

test('every converter catalogue source has English and Hong Kong Cantonese entries', async () => {
  const pot = parsePo(await read('bbl/i18n/BambuStudio.pot'));
  const english = parsePo(await read('bbl/i18n/en/BambuStudio_en.po'));
  const cantonese = parsePo(await read('bbl/i18n/yue_HK/BambuStudio_yue_HK.po'));
  let checked = 0;
  for (const file of [PANEL, COPY, LIBRARY]) {
    for (const source of catalogueSources(await read(file))) {
      assert.ok(pot.has(source), `${file}: "${source}" is not in BambuStudio.pot`);
      assert.ok(english.has(source), `${file}: "${source}" is not in the English catalogue`);
      assert.ok((cantonese.get(source) ?? '').trim(), `${file}: "${source}" has no Hong Kong Cantonese translation`);
      ++checked;
    }
  }
  assert.ok(checked > 300, `expected the registry, result messages and panel copy, found ${checked}`);
});

test('every stable result code has a translatable sentence', async () => {
  const library = stripComments(await read(LIBRARY));
  const body = library.slice(library.indexOf('const char *result_message(const std::string &code)'));
  assert.ok(body.length > 0, 'result_message is defined in Converter.cpp');
  const described = new Set([...body.matchAll(/\{"([a-z][a-z0-9_]*)",L\("/g)].map((m) => m[1]));
  const codes = new Set();
  for (const file of [...PRODUCERS, PANEL]) for (const c of producedCodes(await read(file))) codes.add(c);
  for (const expected of ['converted', 'destination_exists', 'pdf_validation_page_order', 'isolated_worker_start_', 'incompatible_source_signature'])
    assert.ok(codes.has(expected), `the code scan finds ${expected}`);
  for (const c of codes) assert.ok(described.has(c), `result code ${c} has no message in result_message()`);
  // Numbered Windows codes are matched by prefix and must keep a digit suffix.
  assert.match(body, /code\.find_first_not_of\("0123456789",length\) == std::string::npos/);
});

test('registry text is fixed catalogue English with diagnostics kept apart', async () => {
  const library = stripComments(await read(LIBRARY));
  const catalog = library.slice(library.indexOf('std::vector<Adapter> catalog('), library.indexOf('Conversion transform('));
  assert.doesNotMatch(catalog, /"PDF "\s*\+/, 'PDF tool names are whole catalogue strings');
  assert.doesNotMatch(catalog, /":\s*"\s*\+|\+\s*probe\.code|\+\s*pdf_runtime/, 'no diagnostic is concatenated into a reason');
  assert.match(catalog, /unavailable_detail=code_or_invalid\(probe\.code\)/, 'the sandbox start code travels as a detail');
  assert.match(catalog, /pdf_detail=pdf_runtime_code/, 'the PDF runtime code travels as a detail');
  for (const fn of ['category_name', 'kind_name', 'state_name']) {
    const start = library.indexOf(`const char *${fn}(`);
    const names = library.slice(start, library.indexOf('}', start));
    assert.doesNotMatch(names, /[{,]\s*"[A-Z]/, `${fn} marks every name for the catalogue`);
  }
  const header = await read('src/libslic3r/LocalConverter/Converter.hpp');
  assert.match(header, /std::string detail;/);
  assert.match(header, /const char \*result_message\(const std::string &code\);/);
});

test('the panel translates registry text at display time and never shows raw codes alone', async () => {
  const panel = stripComments(await read(PANEL));
  assert.doesNotMatch(panel, /utf\(a\.(name|reason|disclosure|validator)/, 'adapter text is translated, not shown raw');
  assert.doesNotMatch(panel, /utf\(LC::(state_name|kind_name)/, 'states and kinds are translated');
  assert.doesNotMatch(panel, /add_row\(m_jobs,[^;]*utf\(j\.code\)/, 'a result cell carries its sentence, not only the code');
  assert.match(panel, /fact\(a\.name\)/);
  assert.match(panel, /fact\(a\.disclosure\)/);
  assert.match(panel, /fact\(a\.validator\)/);
  assert.match(panel, /fact\(a\.reason\)/);
  assert.match(panel, /fact\(LC::state_name\(j\.state\)\)/);
  assert.match(panel, /fact\(LC::kind_name\(/);
  assert.match(panel, /result_text\(j\.code\)/);
  assert.match(panel, /LC::result_message\(code\)/);
  // Factual copy keeps the language selection but never takes a voice variant.
  assert.match(panel, /Text fact\(const char \*source\) \{ return I18N::language_mode_service\(\)\.factual\(/);
  // Bilingual mode: the panel renders both languages itself for the lines it owns.
  assert.match(panel, /options\.presentation = I18N::LocalizedTextPresentation::Stacked;/);
  assert.match(panel, /I18N::apply_localized_text\(\*label, text\.finalize_without_arguments\(\), options\)/);
  assert.match(panel, /wxString cell\(const Text &text\)/);
  assert.doesNotMatch(panel, /update_status\(_L\(/, 'status lines go through the language-mode helpers');
});

test('only non-factual lines take a funny-level voice, per language', async () => {
  const panel = stripComments(await read(PANEL));
  const copy = stripComments(await read(COPY));
  const lines = copy.slice(copy.indexOf('enum class Line'), copy.indexOf('Count', copy.indexOf('enum class Line')))
    .match(/\b[A-Z]\w+(?=\s*,)/g);
  assert.deepEqual(lines, ['Intro', 'EmptyQueue', 'NoMatches', 'NoAdapterSelected', 'Checking', 'ChecksFinished', 'Converting', 'Stopped', 'Exported']);
  for (const line of lines) assert.match(panel, new RegExp(`voice\\(Copy::Line::${line}\\)`), `${line} is shown with its voice ladder`);
  assert.match(panel, /funny_level\(I18N::FunnyLanguage::English\)/);
  assert.match(panel, /funny_level\(I18N::FunnyLanguage::Cantonese\)/);
  assert.match(panel, /BilingualEnglishCantoneseHongKong: return Text\{english\.primary, cantonese\.secondary\}/,
    'bilingual mode pairs each language with the variant for its own level');
  assert.doesNotMatch(panel, /voice\([^)]*(disclosure|reason|result|state)/i, 'disclosures, reasons, results and states stay factual');
});
