// Import preferences... must read what Export preferences... writes as YAML,
// TOML and XML, and refuse anything else.
//
// 1. Compiles and runs tests/preference_formats_test.cpp: the exporter's real
//    output with and without the schema header, hand edits, escapes, refused
//    files and format detection, each converted file planned by plan_import.
// 2. Builds it again with SLIC3R_PREFERENCE_FORMATS_NEGATIVE_FIXTURE, in which
//    a file that could not be read is passed on as if it had been; that build
//    must fail.
// 3. Checks that the exporter still writes these formats the way the
//    fixtures record (the YAML directive, two-space indents, quoted scalars,
//    the [data] table, the <data> element), and that the check notices a
//    change.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (rel) => readFileSync(path.join(root, rel), 'utf8').replaceAll('\r\n', '\n');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-preference-formats-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + read('tests/preference_formats_test.cpp'));
  const exe = path.join(output, 'fixture.exe');
  const compile = compileFixture({source, exe, cwd: output, include: [root, path.join(root, 'src')]});
  if (compile.error) throw compile.error;
  if (compile.status !== 0) process.exit(compile.status ?? 1);
  return {exe, output};
}

const normal = build('normal', '');
const green = spawnSync(normal.exe, [], {cwd: normal.output, stdio: 'inherit'});
if (green.error) throw green.error;
if (green.status !== 0) process.exit(green.status ?? 1);

const mutated = build('negative', '#define SLIC3R_PREFERENCE_FORMATS_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the suite cannot notice a broken file passed on as read.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (an unreadable file was passed on).');

const markers = [
  'out += "%YAML 1.2\\n---\\n";',
  'std::string indent_of(int n) { return std::string(static_cast<std::size_t>(n) * 2, \' \'); }',
  'return "\\"" + json_escape(text) + "\\"";',
  'out += "\\n[data]\\n";',
  'out += "\\n[" + child + "]\\n";',
  'out += "  <data>\\n";',
  'emit_xml_value(d.root, nullptr, out, 2);',
  'if (key != nullptr) out += " key=\\"" + xml_escape(*key) + "\\"";',
];
function missing(source) {
  return markers.filter((marker) => !source.includes(marker));
}
const writer = read('src/slic3r/GUI/Export/ExportEverything.cpp');
const gone = missing(writer);
if (gone.length) {
  console.error('The exporter no longer writes the way tests/preference_formats_test.cpp records; regenerate its fixtures and recheck the reader:\n' + gone.join('\n'));
  process.exit(1);
}
if (missing(writer.replace('out += "%YAML 1.2\\n---\\n";', 'out += "---\\n";')).length !== 1) {
  console.error('The writer check did not notice a changed YAML header.');
  process.exit(1);
}
console.log('The exporter still writes YAML, TOML and XML as the fixtures record.');
