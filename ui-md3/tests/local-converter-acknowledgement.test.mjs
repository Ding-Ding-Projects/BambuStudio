import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The converter disclosed lossy and metadata/encoding changes but admitted files
// without asking. These tests pin the explicit acknowledgement: the registry
// marks every adapter that changes or leaves out data and says exactly what, the
// panel asks for consent bound to that disclosure before any picker opens, and
// the queue itself refuses or skips work whose consent is missing or stale.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = async (relative) => (await readFile(path.join(repoDir, relative), 'utf8'))
  .replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '').replace(/^[ \t]*\/\/.*$/gm, '');
const PANEL = 'src/slic3r/GUI/LocalConverter/LocalConverterPanel.cpp';
const HEADER = 'src/slic3r/GUI/LocalConverter/LocalConverterPanel.hpp';
const LIBRARY = 'src/libslic3r/LocalConverter/Converter.cpp';
const API = 'src/libslic3r/LocalConverter/Converter.hpp';
const body = (source, signature) => {
  const start = source.indexOf(signature);
  assert.ok(start >= 0, `${signature} exists`);
  return source.slice(start, source.indexOf('\n}\n', start));
};

test('the registry marks and describes every adapter that changes or leaves out data', async () => {
  const library = await read(LIBRARY);
  const catalog = body(library, 'std::vector<Adapter> catalog(');
  const flags = catalog.match(/a\.changes_encoding = ([^;]+);/);
  assert.ok(flags, 'metadata/encoding-changing adapters are flagged');
  const changing = [...flags[1].matchAll(/a\.id == "([^"]+)"/g)].map((m) => m[1]);
  assert.deepEqual(changing.sort(), ['csv.json', 'json.csv', 'json.pretty', 'json.tsv', 'tsv.json', 'zip.decode']);
  const lossy = catalog.match(/a\.lossy = ([^;]+);/);
  assert.ok(lossy && /text\.lf/.test(lossy[1]) && /Category::Images/.test(lossy[1]) && /zip\.encode/.test(lossy[1]));
  // Each such disclosure says what changes, is omitted or is not kept.
  const disclosures = new Map();
  for (const m of catalog.matchAll(/add\(\s*"([^"]+)"[^;]*?,\s*L\("((?:[^"\\]|\\.)*)"\),\s*L\("(?:[^"\\]|\\.)*"\)\);/g)) disclosures.set(m[1], m[2]);
  for (const m of catalog.matchAll(/add\(\s*sep \+ "\.json"[^;]*?L\("TSV to JSON rows"\)[^;]*?,\s*L\("((?:[^"\\]|\\.)*)"\)/g)) disclosures.set('csv.json', m[1]);
  for (const m of catalog.matchAll(/add\(\s*"json\." \+ sep[^;]*?L\("JSON rows to TSV"\)[^;]*?,\s*L\("((?:[^"\\]|\\.)*)"\)/g)) disclosures.set('json.csv', m[1]);
  for (const id of ['text.lf', 'text.crlf', 'json.pretty', 'csv.json', 'json.csv', 'bmp.ppm', 'ppm.bmp', 'zip.encode', 'zip.decode'])
    assert.match(disclosures.get(id) ?? '', /\b(changes?|omitted|not kept|normalizes|no color profile)\b/i, `${id} discloses exactly what changes`);
  assert.match(catalog, /Rewrites PDF structure/, 'PDF tools disclose the structural rewrite');
});

test('the queue refuses unacknowledged admission and skips stale consent', async () => {
  const api = await read(API);
  assert.match(api, /bool requires_acknowledgement\(const Adapter &\);/);
  assert.match(api, /std::string acknowledgement_token\(const Adapter &\);/);
  assert.match(api, /const std::string &acknowledgement = ""\);/, 'enqueue takes the accepted token');
  assert.match(api, /std::string acknowledgement;/, 'each job keeps its accepted token');
  const library = await read(LIBRARY);
  assert.match(library, /bool requires_acknowledgement\(const Adapter &a\) \{ return a\.lossy \|\| a\.changes_encoding; \}/);
  const enqueue = body(library, 'std::uint64_t Queue::enqueue(');
  assert.match(enqueue, /check\(!requires_acknowledgement\(\*terms\) \|\| acknowledgement == acknowledgement_token\(\*terms\), "disclosure_not_acknowledged"\)/);
  assert.ok(enqueue.indexOf('disclosure_not_acknowledged') < enqueue.indexOf('write(j)'), 'refusal happens before any record is written');
  const step = body(library, 'bool Queue::step(');
  assert.match(step, /acknowledgement_problem\(job\)/);
  assert.ok(step.indexOf('acknowledgement_problem(job)') < step.indexOf('State::Running'), 'consent is checked before the job runs');
  assert.match(library, /\{"acknowledgement",j\.acknowledgement\}/, 'the token is saved in the record');
  assert.match(library, /result\.acknowledgement=j\.value\("acknowledgement",std::string\(\)\)/, 'and read back after a restart');
  for (const code of ['disclosure_not_acknowledged', 'disclosure_changed_since_acknowledgement'])
    assert.match(body(library, 'const char *result_message('), new RegExp(`\\{"${code}",L\\(`), `${code} has a result sentence`);
});

test('the panel asks for explicit consent with a named kit check box before any picker', async () => {
  const header = await read(HEADER);
  assert.match(header, /LabeledCheckBox \*m_acknowledge = nullptr;/);
  assert.match(header, /std::set<std::string> m_acknowledged;/);
  const panel = await read(PANEL);
  assert.match(panel, /#include "slic3r\/GUI\/Widgets\/LabeledCheckBox\.hpp"/);
  assert.match(panel, /m_acknowledge = new LabeledCheckBox\(body,_L\("I reviewed what this conversion changes or leaves out, and I want to convert with these changes\."\)\)/);
  assert.match(panel, /m_acknowledge->SetName\(_L\(/);
  assert.match(panel, /m_acknowledge->GetCheckBox\(\)->SetName\(_L\(/, 'the focusable glyph carries the accessible name');
  assert.match(panel, /m_acknowledge->Bind\(wxEVT_CHECKBOX,/);
  const update = body(panel, 'void LocalConverterPanel::update_acknowledgement()');
  assert.match(update, /m_adapters\[m_selected\]\.enabled && LC::requires_acknowledgement\(m_adapters\[m_selected\]\)/);
  assert.match(update, /m_acknowledge->Show\(needed\)/);
  assert.match(body(panel, 'void LocalConverterPanel::select_adapter('), /update_acknowledgement\(\);/);
  const choose = body(panel, 'void LocalConverterPanel::choose_source(');
  const gate = choose.indexOf('m_acknowledged.count(LC::acknowledgement_token(chosen))==0');
  assert.ok(gate > 0, 'adding files checks the consent for the selected disclosure');
  for (const picker of ['wxDirDialog', 'wxFileDialog']) assert.ok(gate < choose.indexOf(picker), `consent is checked before the ${picker} opens`);
  assert.match(choose, /m_acknowledge->GetCheckBox\(\)->SetFocus\(\); return;/, 'keyboard focus moves to the consent');
  const admit = body(panel, 'void LocalConverterPanel::admit(');
  assert.match(admit, /m_queue->enqueue\(path,target,adapter\.id,options,additional,acknowledgement\)/);
});
