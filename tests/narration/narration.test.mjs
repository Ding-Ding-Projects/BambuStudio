// Compiles and runs tests/narration/narration_fixture.cpp.
// `--negative` copies the headers to a scratch tree, breaks one rule at a time and
// requires the fixture to fail for each, then requires the unbroken copy to pass.
import {cpSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from '../native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const negative = process.argv.includes('--negative');
const headers = ['src/slic3r/GUI/Narration/NarrationEvents.hpp', 'src/slic3r/GUI/Narration/NarrationTransitions.hpp',
  'src/slic3r/GUI/Narration/NarrationPolicy.hpp', 'src/slic3r/GUI/Narration/VoiceCatalog.hpp',
  'src/slic3r/GUI/PersonalModes/SpeechQueue.hpp'];

function build(include, label) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-narration-' + label + '-'));
  const exe = path.join(output, 'fixture.exe');
  const compile = compileFixture({source: path.join(root, 'tests/narration/narration_fixture.cpp'), exe, cwd: output, include});
  if (compile.error) throw compile.error;
  if (compile.status !== 0) return {status: compile.status ?? 1, compiled: false};
  const run = spawnSync(exe, [], {cwd: output, stdio: 'inherit'});
  if (run.error) throw run.error;
  return {status: run.status ?? 1, compiled: true};
}

if (!negative) process.exit(build(root, 'main').status);

const mutations = [
  ['transitions fire every tick', 'Narration/NarrationTransitions.hpp', 'if (is != was) {', 'if (true) {'],
  ['printer switch not reseeded', 'Narration/NarrationTransitions.hpp', 'if (!m_seeded || now.machine != m_last.machine) {', 'if (!m_seeded) {'],
  ['dedupe window ignored', 'Narration/NarrationTransitions.hpp', 'now - it->second < m_window', 'false'],
  ['quiet hours never wrap', 'Narration/NarrationPolicy.hpp', 'return minute_of_day >= start_minute || minute_of_day < end_minute;', 'return false;'],
  ['focus assist ignored', 'Narration/NarrationPolicy.hpp', 'if (in.respect_focus_assist &&', 'if (false &&'],
  ['held environment lets errors through', 'Narration/NarrationPolicy.hpp', 'if (in.environment == EnvironmentState::Held) return false;', ''],
  ['tracks share cooldown keys', 'PersonalModes/SpeechQueue.hpp', 'inline std::string speech_key(const SpeechEvent& e) { return e.track + "/" + e.category; }', 'inline std::string speech_key(const SpeechEvent& e) { return e.category; }'],
  ['zh-HK not recognised', 'Narration/VoiceCatalog.hpp', 'id == 0x0c04', 'false'],
  ['tone ladder collapsed', 'Narration/NarrationEvents.hpp', 'return level <= 2 ? Tone::Serious : level == 3 ? Tone::Friendly : Tone::Playful;', 'return Tone::Playful;'],
];

let failed = 0;
for (const [name, file, from, to] of mutations) {
  const scratch = mkdtempSync(path.join(tmpdir(), 'bambustudio-narration-neg-'));
  for (const h of headers) { mkdirSync(path.dirname(path.join(scratch, h)), {recursive: true}); cpSync(path.join(root, h), path.join(scratch, h)); }
  const target = path.join(scratch, 'src/slic3r/GUI', file);
  const text = readFileSync(target, 'utf8');
  if (!text.includes(from)) { console.log('MUTATION ANCHOR MISSING: ' + name); failed++; continue; }
  writeFileSync(target, text.replace(from, to));
  const result = build(scratch, 'neg');
  if (result.status === 0) { console.log('NOT DETECTED: ' + name); failed++; } else console.log('detected: ' + name);
}
const clean = mkdtempSync(path.join(tmpdir(), 'bambustudio-narration-clean-'));
for (const h of headers) { mkdirSync(path.dirname(path.join(clean, h)), {recursive: true}); cpSync(path.join(root, h), path.join(clean, h)); }
if (build(clean, 'clean').status !== 0) { console.log('Unmutated copy failed'); failed++; }
console.log(failed ? failed + ' negative check(s) failed' : 'all ' + mutations.length + ' mutations detected, clean copy green');
process.exit(failed ? 1 : 0);
