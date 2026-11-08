// The Local tools page must list only tools that open, keep pins in the order
// chosen and keep the newest activity.
//
// 1. Compiles and runs tests/local_tools_hub_test.cpp (tools, chips, pins,
//    filters, cleaning, the activity file and its limit).
// 2. Builds it again with SLIC3R_LOCAL_TOOLS_HUB_NEGATIVE_FIXTURE, in which a
//    full log drops the newest entry; that build must fail.
// 3. Checks that each tool's description is the tool's own menu help in
//    MainFrame.cpp, so the page and the menu say the same translated words,
//    and that the same check reports a description that drifted.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (rel) => readFileSync(path.join(root, rel), 'utf8').replaceAll('\r\n', '\n');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-local-tools-hub-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + read('tests/local_tools_hub_test.cpp'));
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

const mutated = build('negative', '#define SLIC3R_LOCAL_TOOLS_HUB_NEGATIVE_FIXTURE 1\n');
const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
if (red.error) throw red.error;
if (red.status === 0) {
  console.error('Negative fixture stayed green: the suite cannot notice a full activity log that drops the newest entry.');
  process.exit(1);
}
console.log('Negative fixture turned red as required (the newest activity was dropped).');

function drifted(hub, frame) {
  const summaries = [...hub.matchAll(/\{"([a-z0-9-]+)", "([a-z]+)", L\("((?:[^"\\]|\\.)*)"\)\}/g)].map((m) => ({id: m[1], summary: m[3]}));
  if (summaries.length < 15) return [`only ${summaries.length} tools were parsed`];
  return summaries.filter(({summary}) => !frame.includes(`_L("${summary}")`)).map(({id}) => id);
}

const hub = read('src/slic3r/GUI/FeatureFinder/LocalToolsHub.cpp');
const frame = read('src/slic3r/GUI/MainFrame.cpp');
const problems = drifted(hub, frame);
if (problems.length) {
  console.error('Descriptions that are not the menu help in MainFrame.cpp: ' + problems.join(', '));
  process.exit(1);
}
const changed = hub.replace('L("Convert local files with verified bundled adapters")', 'L("Convert files")');
if (drifted(changed, frame).length !== 1) {
  console.error('The description check did not notice a description that drifted from its menu help.');
  process.exit(1);
}
console.log('Every tool description is its menu help.');
