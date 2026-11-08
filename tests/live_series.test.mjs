// Compiles and runs tests/live_series_test.cpp, the standalone assertions for
// src/slic3r/GUI/LiveCharts/LiveSeries.cpp (readings, windows, gaps, trends,
// candles, feed state, crosshair, range, CSV, momentum, new highs, particles,
// the clock and the saved options), with MSVC on Windows and $CXX (or c++)
// elsewhere. Three more builds each define a negative-fixture switch and must
// fail: one joins readings across a gap (a chart drawing points the printer
// never sent), one bursts on every new high (no cooldown) and one shows
// midnight as 0 on a 12-hour clock.
import {mkdtempSync, readFileSync, writeFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

function build(name, prefix) {
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-live-series-' + name + '-'));
  const source = path.join(output, 'fixture.cpp');
  writeFileSync(source, prefix + readFileSync(path.join(root, 'tests/live_series_test.cpp'), 'utf8'));
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

const negatives = [
  ['SLIC3R_LIVE_SERIES_NEGATIVE_FIXTURE', 'a line was drawn across a gap'],
  ['SLIC3R_LIVE_SERIES_HIGHS_NEGATIVE_FIXTURE', 'particles burst on every new high, with no cooldown'],
  ['SLIC3R_LIVE_SERIES_CLOCK_NEGATIVE_FIXTURE', 'midnight read 0 on a 12-hour clock'],
];
for (const [macro, what] of negatives) {
  const mutated = build(macro.toLowerCase(), `#define ${macro} 1\n`);
  const red = spawnSync(mutated.exe, [], {cwd: mutated.output, encoding: 'utf8'});
  if (red.error) throw red.error;
  if (red.status === 0) {
    console.error(`Negative fixture ${macro} stayed green: the live series suite cannot notice that ${what}.`);
    process.exit(1);
  }
  console.log(`Negative fixture ${macro} turned red as required (${what}).`);
}
