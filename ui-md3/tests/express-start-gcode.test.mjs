import assert from 'node:assert/strict';
import { readdirSync, readFileSync } from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Express print mode (process option `express_print_mode`) keeps heating, one
// homing pass, the AMS filament load, the purge line and the temperature
// waits, and skips every other preparation step. The firmware already gates
// bed leveling, extrusion calibration and toolhead offset calibration behind
// send-time flags (M1002 judge_flag + M622 J1/J2 ... M623), which print setup
// sends off. The sections that run unconditionally are wrapped in
// `{if !express_print_mode}` ... `{endif}` by
// scripts/profiles/express_start_gcode_guards.py. These contracts evaluate
// every template in both modes with an evaluator of their own, so a guard
// that lands on the wrong line, drops a required command or hides a section
// from the normal start sequence turns the test red without a build.

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const machineDir = path.join(root, 'resources', 'profiles', 'BBL', 'machine');
const guardScript = path.join(root, 'scripts', 'profiles', 'express_start_gcode_guards.py');
const GUARD_OPEN = '{if !express_print_mode}';
const EXPRESS_OPEN = '{if express_print_mode}';
const EXPRESS_COMMENT = '; express print mode: preparation skipped except homing, filament loading, the purge line and the temperature waits';

const templateFiles = () => readdirSync(machineDir)
  .filter((name) => /^Bambu Lab .* template machine_start_gcode\.json$/.test(name))
  .sort();

const loadGcode = (name) => JSON.parse(readFileSync(path.join(machineDir, name), 'utf8')).machine_start_gcode;

// Resolve only the express guards and the firmware flag blocks; every other
// placeholder stays as written.
const renderView = (gcode, mode) => {
  const out = [];
  let skipDepth = 0;
  const kept = [];
  let skipFirmware = 0;
  for (const line of gcode.split('\n')) {
    const s = line.trim();
    if (skipDepth) {
      if (s.startsWith('{if')) skipDepth += 1;
      else if (s.startsWith('{endif')) skipDepth -= 1;
      continue;
    }
    if (s === GUARD_OPEN) { if (mode === 'express') skipDepth = 1; else kept.push(true); continue; }
    if (s === EXPRESS_OPEN) { if (mode === 'express') kept.push(true); else skipDepth = 1; continue; }
    if (s.startsWith('{endif') && kept.length && kept[kept.length - 1] === true) { kept.pop(); continue; }
    if (mode === 'express') {
      if (skipFirmware) {
        if (s.startsWith('M622')) skipFirmware += 1;
        else if (s.startsWith('M623')) skipFirmware -= 1;
        continue;
      }
      if (/^M622 J[12]\b/.test(s)) { skipFirmware = 1; continue; }
    }
    out.push(line);
    if (s.startsWith('{if')) kept.push(false);
    else if (s.startsWith('{endif') && kept.length) kept.pop();
  }
  assert.equal(skipDepth, 0, 'a dropped block must close before the template ends');
  assert.equal(skipFirmware, 0, 'a firmware block must close before the template ends');
  return out.map((line) => line.trim());
};

const balanced = (gcode) => {
  let depth = 0;
  for (const raw of gcode.split('\n')) {
    const s = raw.trim();
    if (s.startsWith('{if')) depth += 1;
    else if (s.startsWith('{endif')) depth -= 1;
    assert.ok(depth >= 0, 'an {endif} without an {if}');
  }
  assert.equal(depth, 0, 'every {if} needs its {endif}');
};

// Commands that never belong to an express start: leveling, vibration checks,
// camera, laser, scanner and plate detections, the sound, cog noise reduction
// and the z offset calibration.
const FORBIDDEN = /^(G29(\s|$)|M970|M971|M972|M974|M1006|M982\.2|G383(\s|$))/;
const SKIPPED_MARKERS = /^;=+\s*(wipe nozzle|wipe right nozzle|mech mode|check scanner|laser and rgb|detection start|z ofst cali|start printer sound|printer start sound)/;

const families = [
  { match: /^Bambu Lab X1E /, prime: /^;=+\s*(purge line to wipe the nozzle|noo?zle load line)/, normal: ['M970.3', 'M972 S5', 'M973 S3 P14', 'M974'] },
  { match: /^Bambu Lab X1 /, prime: /^;=+\s*purge line to wipe the nozzle/, normal: ['M1006 S1', 'M970.3', 'M972 S5', 'M973 S3 P14', 'M974'] },
  { match: /^Bambu Lab P1[PS] /, prime: /^;=+\s*nozzle load line/, normal: ['M970.3', 'M974'] },
  { match: /^Bambu Lab A1 mini /, prime: /^;=+\s*extrude cali test/, normal: ['M1006 S1', 'M982.2 S1', 'M970.3', 'M622 S1', 'M974'] },
  { match: /^Bambu Lab A1 /, prime: /^;=+\s*extrude cali test/, normal: ['M1006 S1', 'M982.2 S1', 'M970.3', 'M622 S1', 'M974'] },
  { match: /^Bambu Lab A2L /, prime: /^;=+\s*nozzle load line/, normal: ['M1006 S1', 'M982.2 S1', 'M970.3', 'M622 S1', 'M974'] },
  { match: /^Bambu Lab H2D /, prime: /^;=+\s*nozzle load line/, normal: ['M1006 S1', 'M982.2 S1', 'M972 S24', 'M972 S41', 'M972 S19', 'M972 S14', 'G383 O0', 'M970.3', 'M974'] },
  { match: /^Bambu Lab H2C /, prime: /^;=+\s*nozzle load line/, normal: ['M1006 S1', 'M982.2 S1', 'M972 S46', 'M972 S41', 'M972 S19', 'M972 S14', 'M970.3', 'M974'] },
  { match: /^Bambu Lab H2S /, prime: /^;=+\s*nozzle load line/, normal: ['M1006 S1', 'M982.2 S1', 'M972 S24', 'M972 S41', 'M972 S19', 'M970.3', 'M974'] },
  { match: /^Bambu Lab P2S /, prime: /^;=+\s*nozzle load line/, normal: ['M1006 S1', 'M982.2 S1', 'M972 S24', 'M972 S42', 'M972 S19', 'M970.3', 'M974'] },
  { match: /^Bambu Lab X2D /, prime: /^;=+\s*nozzle load line/, normal: ['M1006 S1', 'M982.2 S1', 'M972 S24', 'M972 S26', 'M972 S19', 'G383 O0', 'M970.3', 'M974'] },
];

const familyOf = (name) => {
  const family = families.find((f) => f.match.test(name));
  assert.ok(family, `${name} needs a family entry in this test`);
  return family;
};

const checkTemplate = (name, gcode) => {
  const family = familyOf(name);
  balanced(gcode);
  const guards = gcode.split('\n').filter((line) => line.trim() === GUARD_OPEN).length;
  assert.ok(guards >= 2, `${name}: at least the sound or wipe and the vibration check must be guarded (${guards} guards)`);

  const express = renderView(gcode, 'express');
  assert.equal(express[0], EXPRESS_COMMENT, `${name}: the express view starts with the express comment`);
  for (const line of express) {
    assert.ok(!FORBIDDEN.test(line), `${name}: express view still runs "${line}"`);
    assert.ok(!SKIPPED_MARKERS.test(line), `${name}: express view still contains the section "${line}"`);
  }
  const has = (re, what) => assert.ok(express.some((line) => re.test(line)), `${name}: express view lost ${what}`);
  // A homing pass that includes Z: a bare G28, G28 R, G28 T<temp>, or an axis list naming Z.
  has(/^G28\s*(;.*)?$|^G28 R\b|^G28 T\d|^G28[^;]*\bZ\b/, 'the homing pass');
  has(/^M190\b/, 'the bed temperature wait');
  has(/^M109\b/, 'the nozzle temperature wait');
  has(/^M620 S/, 'the AMS filament load (M620)');
  has(/^M621 S/, 'the AMS filament load (M621)');
  has(family.prime, 'the prime line section');
  has(/^M1002 gcode_claim_action : 0$/, 'the final claim action');

  const normal = renderView(gcode, 'normal');
  assert.ok(!normal.includes(EXPRESS_COMMENT), `${name}: the normal view must not carry the express comment`);
  for (const needle of family.normal)
    assert.ok(normal.some((line) => line.startsWith(needle)), `${name}: the normal view lost "${needle}"`);
  // Everything the express view keeps is also in the normal view.
  for (const line of express.filter((l) => l && !l.startsWith('{') && l !== EXPRESS_COMMENT))
    assert.ok(normal.includes(line), `${name}: "${line}" is only in the express view`);
};

const names = templateFiles();

test('every Bambu Lab start template is guarded for express print mode', () => {
  assert.equal(names.length, 29, 'the 29 start templates are all checked');
  for (const name of names) checkTemplate(name, loadGcode(name));
});

test('the guard script reports the committed templates as current', () => {
  const run = spawnSync('python3', ['-I', guardScript, '--check'], { cwd: root, encoding: 'utf8' });
  assert.equal(run.status, 0, `${run.stdout}\n${run.stderr}`);
});

test('each contract rejects the defect it pins', () => {
  const x1c = 'Bambu Lab X1 Carbon 0.4 nozzle template machine_start_gcode.json';
  const h2d = 'Bambu Lab H2D 0.4 nozzle template machine_start_gcode.json';
  const mutations = [
    // A guard that is missing lets the vibration check run in express mode.
    [x1c, (g) => g.replace(`${GUARD_OPEN}\n;===== mech mode fast check`, ';===== mech mode fast check').replace(/;===== mech mode fast check=+\n\{endif\}/, ';===== mech mode fast check')],
    // A guard that swallows the homing pass.
    [x1c, (g) => g.replace(';===== home after wipe mouth============================', `${GUARD_OPEN}\n;===== home after wipe mouth============================`).replace(';===== home after wipe mouth end =======================', ';===== home after wipe mouth end =======================\n{endif}')],
    // Leveling written outside a firmware block.
    [x1c, (g) => g.replace(';===== purge line to wipe the nozzle', 'G29 A1\n;===== purge line to wipe the nozzle')],
    // The express comment lost.
    [x1c, (g) => g.replace(EXPRESS_COMMENT, '; nothing')],
    // A detection kept on the H2D.
    [h2d, (g) => g.replace(`${GUARD_OPEN}\n;===== detection start`, ';===== detection start').replace(/;===== detection end =====\n\{endif\}/, ';===== detection end =====')],
    // An unbalanced guard.
    [h2d, (g) => g.replace(`${GUARD_OPEN}\nM982.2 S1`, 'M982.2 S1')],
  ];
  for (const [name, mutate] of mutations) {
    const original = loadGcode(name);
    const mutated = mutate(original);
    assert.notEqual(mutated, original, `the mutation of ${name} must change the template`);
    assert.throws(() => checkTemplate(name, mutated), `${name} mutation must be rejected`);
  }
});
