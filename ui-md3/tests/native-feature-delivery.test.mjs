import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const input = JSON.parse(fs.readFileSync(path.join(root, 'design/native-feature-delivery.json'), 'utf8').replace(/^\uFEFF/, ''));
const design = JSON.parse(fs.readFileSync(path.join(root, 'design/workflow-refresh/manifest.json'), 'utf8'));
// Independent, hand-written obligations. Never derive either list from the ledger.
const surfaces = 'shell prepare preview print monitor farm home-web ink-web printer-web project calibration preferences parameters wizard menus appearance-editor palette regex notifications history documentation changelog import-export model-creator smart-home schedules confirmations canonical-tools'.split(' ');
const families = 'language funny-emoji personal-vocabulary school-mode narration scheduled-settings dim-sum material-design workflow-navigation motion appearance-editor color-picker logo-customization file-converter ollama-suite tabs element-locks support-tickets unlock-ladder authenticator adhd-modes local-history notifications guided-forms rich-controls regex-search command-palette offline-docs changelog external-editor exports bulk-actions super-confirmation overlay-panels progress-recovery download-handoff forge-publishing blank-editors collapse-filters front-provenance product-evidence completeness-parity display-name'.split(' ');
const states = 'normal hover focus pressed selected disabled dragged validation loading success warning error'.split(' ');
const proofPairs = [['documentationStatus','documentationReferences'], ['testStatus','testReferences'], ['runtimeStatus','interactionReferences'], ['captureStatus','captureReferences']];

function validate(ledger) {
  assert.equal(ledger.schemaVersion, 1);
  assert.equal(ledger.completionClaim, false, 'This inventory does not establish product completion');
  for (const [key, expected] of [['surfaces', surfaces], ['families', families]]) {
    const ids = ledger[key].map(item => item.id);
    assert.equal(new Set(ids).size, ids.length, `duplicate ${key}`);
    assert.deepEqual([...ids].sort(), [...expected].sort(), `missing or unexpected ${key}`);
  }
  assert.deepEqual(ledger.requiredElementStates, states);
  for (const surface of ledger.surfaces) {
    assert.ok(surface.requiredStates.length > 0, `${surface.id}: no explicit states`);
    assert.ok(fs.existsSync(path.join(root, surface.sourceAnchor)), `missing surface anchor ${surface.sourceAnchor}`);
  }
  const ids = new Set();
  for (const row of ledger.rows) {
    const id = `${row.surface}/${row.family}`;
    assert.ok(surfaces.includes(row.surface) && families.includes(row.family), `unexpected row ${id}`);
    assert.ok(!ids.has(id), `duplicate row ${id}`);
    ids.add(id);
    assert.ok(['present','partial','missing','unknown'].includes(row.sourceStatus), id);
    for (const [status, references] of proofPairs) {
      assert.ok(['verified','unverified','missing','blocked'].includes(row[status]), `${id}: ${status}`);
      assert.ok(Array.isArray(row[references]), `${id}: ${references}`);
      if (row[status] === 'verified') assert.ok(row[references].length > 0, `${id}: verified without proof`);
    }
    assert.ok(['verified','unverified','missing','blocked'].includes(row.localizationStatus), `${id}: localization`);
    assert.ok(['verified','unverified','missing','blocked'].includes(row.persistenceStatus), `${id}: persistence`);
    assert.ok(typeof row.nextAction === 'string' && row.nextAction.length > 0, `${id}: next action`);
    assert.ok(row.sourceStatus !== 'present' || row.sourceReferences.length > 0, `${id}: source presence needs exact references`);
    assert.ok(row.deviation === null || (row.deviation.reason && row.deviation.approvalReference), `${id}: undocumented deviation`);
  }
  for (const surface of surfaces) for (const family of families) {
    assert.ok(ids.has(`${surface}/${family}`), `missing row ${surface}/${family}`);
  }
  for (const family of ledger.families) {
    assert.ok(['present','partial','missing','unknown'].includes(family.sourceStatus));
    assert.ok(family.gap.length > 0, family.id);
    for (const reference of [...family.sourceReferences, ...family.documentationReferences, ...family.testReferences]) {
      assert.ok(fs.existsSync(path.join(root, reference)), `missing candidate reference ${reference}`);
    }
  }
  assert.deepEqual(ledger.tupleContract.displayScales, [100,125,150,200]);
  assert.equal(ledger.tupleContract.languages.length, 3);
  assert.equal(ledger.tupleContract.themes.length, 2);
  assert.equal(ledger.tupleContract.densities.length, 2);
}

test('native canonical ledger keeps every explicit surface and feature obligation', () => {
  const ledger = structuredClone(input);
  // Explicit developer fixture used to demonstrate an actual failing test run.
  if (process.env.NATIVE_LEDGER_REMOVE_ROW === '1') ledger.rows.shift();
  validate(ledger);
});
test('deleting a whole feature family cannot disappear through discovery', () => {
  const ledger = structuredClone(input);
  ledger.families = ledger.families.filter(row => row.id !== 'school-mode');
  assert.throws(() => validate(ledger), /missing or unexpected families/);
});
test('deleting a surface cannot silently erase its obligations', () => {
  const ledger = structuredClone(input);
  ledger.surfaces = ledger.surfaces.filter(row => row.id !== 'printer-web');
  assert.throws(() => validate(ledger), /missing or unexpected surfaces/);
});
test('deleting a nested embedded-web obligation fails', () => {
  const ledger = structuredClone(input);
  ledger.rows = ledger.rows.filter(row => !(row.surface === 'ink-web' && row.family === 'personal-vocabulary'));
  assert.throws(() => validate(ledger), /missing row ink-web\/personal-vocabulary/);
});
test('a verified runtime claim without interaction proof fails', () => {
  const ledger = structuredClone(input);
  ledger.rows[0].runtimeStatus = 'verified';
  assert.throws(() => validate(ledger), /verified without proof/);
});
test('duplicate obligations and false completion fail', () => {
  const duplicate = structuredClone(input);
  duplicate.rows.push(duplicate.rows[0]);
  assert.throws(() => validate(duplicate), /duplicate row/);
  const complete = structuredClone(input);
  complete.completionClaim = true;
  assert.throws(() => validate(complete), /does not establish product completion/);
});

function validateDesign(spec) {
  assert.equal(spec.kind, 'static-source-reference');
  assert.equal(spec.completionClaim, false);
  assert.deepEqual(spec.surfaces.map(row => row.id).sort(), [...surfaces].sort(), 'missing design surface');
  assert.equal(spec.baselineCommit, 'd048cfc3040a1566b78e03334f3871f1dd0144bb');
  assert.deepEqual(spec.workflowOrder, ['prepare', 'preview', 'print', 'monitor']);
  assert.equal(spec.preserveNumericPageIds, true);
  assert.equal(spec.runtimeStatus, 'unverified');
  assert.equal(spec.captureStatus, 'missing');
  assert.equal(spec.referenceViewerStatus, 'missing');
  assert.equal(spec.productionFixtureStatus, 'missing');
  assert.equal(spec.parityStatus, 'blocked');
  assert.deepEqual(spec.referenceViewportDIP, [1200, 800]);
  assert.deepEqual(spec.themes, ['light', 'dark']);
  for (const row of spec.surfaces) {
    assert.ok(row.states.length >= 8, `${row.id}: unresolved state inventory`);
    assert.equal(new Set(row.states).size, row.states.length, `${row.id}: duplicate state`);
    assert.ok(row.preserve.length > 30, `${row.id}: no behavior preservation contract`);
    assert.ok(fs.existsSync(path.join(root, row.anchor)), `${row.id}: missing implementation destination`);
    assert.ok(row.sections.length >= 4, `${row.id}: missing composition`);
    assert.equal(row.references.length, 2, `${row.id}: missing themed references`);
    for (const theme of ['light', 'dark']) {
      const ref = row.references.find(item => item.theme === theme);
      assert.ok(ref, `${row.id}: missing ${theme} reference`);
      assert.equal(ref.path, `design/workflow-refresh/references/${row.id}-${theme}.svg`);
      const svg = fs.readFileSync(path.join(root, ref.path), 'utf8');
      assert.ok(svg.includes('Not a screenshot or a working application.'), `${row.id}: misleading reference`);
      assert.ok(svg.includes(`Reference: ${row.id}/${theme}`), `${row.id}: wrong board`);
      assert.ok(!svg.includes('<image') && !svg.includes('<script'), `${row.id}: unexpected external content`);
    }
  }
  const requiredStates = {
    shell: ['secondary-destinations', 'overflow', 'pinned-tabs', 'reorder'],
    prepare: ['dual-nozzle', 'single-nozzle', 'stale-result', 'slice-cancel'],
    print: ['stale', 'mapping', 'final-confirmation', 'partial-result'],
    monitor: ['camera-stopped', 'fan-pending', 'telemetry-stale'],
    project: ['calendar-month', 'checklist', 'missing-member'],
    preferences: ['local-vocabulary', 'narrator', 'school-mode'],
    confirmations: ['one-key', 'both-keys', 'partial-slider', 'cancel'],
    'canonical-tools': ['converter', 'assistant', 'authenticator', 'locks', 'support']
  };
  for (const [surface, required] of Object.entries(requiredStates)) {
    const row = spec.surfaces.find(item => item.id === surface);
    for (const state of required) assert.ok(row.states.includes(state), `${surface}: missing ${state}`);
  }
  assert.equal(spec.tokens.motionMs.reduced, 0);
}

test('complete visual design retains every named surface and honest evidence boundary', () => {
  const spec = structuredClone(design);
  if (process.env.NATIVE_DESIGN_REMOVE_SURFACE === '1') spec.surfaces.pop();
  validateDesign(spec);
});
test('design cannot lose a surface, state, reference or stable page identity', () => {
  const missingSurface = structuredClone(design);
  missingSurface.surfaces.pop();
  assert.throws(() => validateDesign(missingSurface), /missing design surface/);
  const missingState = structuredClone(design);
  missingState.surfaces.find(row => row.id === 'print').states = missingState.surfaces.find(row => row.id === 'print').states.filter(state => state !== 'final-confirmation');
  assert.throws(() => validateDesign(missingState), /missing final-confirmation/);
  const missingReference = structuredClone(design);
  missingReference.surfaces[0].references.pop();
  assert.throws(() => validateDesign(missingReference), /missing themed references/);
  const changedIdentity = structuredClone(design);
  changedIdentity.preserveNumericPageIds = false;
  assert.throws(() => validateDesign(changedIdentity));
});
test('design references cannot be promoted into runtime or parity evidence', () => {
  for (const key of ['runtimeStatus','captureStatus','referenceViewerStatus','productionFixtureStatus','parityStatus']) {
    const spec = structuredClone(design);
    spec[key] = 'verified';
    assert.throws(() => validateDesign(spec));
  }
});
