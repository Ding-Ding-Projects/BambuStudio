import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { execFileSync, spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';

const directory = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(directory, '../..');
const read = name => JSON.parse(fs.readFileSync(path.join(directory, name), 'utf8'));
const contracts = read('surface-contracts.json');
const manifest = read('manifest.json');
const scopes = read('implementation-scopes.json');
// Explicit expected inventory, deliberately independent of discovery and the input files.
const expected = 'shell prepare preview print monitor farm home-web ink-web printer-web project calibration preferences parameters wizard menus appearance-editor palette regex notifications history documentation changelog import-export model-creator smart-home schedules confirmations canonical-tools'.split(' ');
if (process.env.NATIVE_DESIGN_REMOVE_ANATOMY === '1') contracts.surfaces = contracts.surfaces.filter(row => row.id !== 'confirmations');
assert.equal(contracts.completionClaim, false);
assert.equal(contracts.runtimeStatus, 'unverified');
assert.equal(contracts.captureStatus, 'missing');
assert.deepEqual(contracts.surfaces.map(row => row.id).sort(), [...expected].sort(), 'Missing explicit surface anatomy');
const scopeIds = new Set([scopes.currentScope.id, ...scopes.nextScopes.map(row => row.id), ...scopes.outsideAnchorFollowups.map(row => row.id)]);
for (const row of contracts.surfaces) {
  for (const field of ['zones','anatomy','narrow','stateDelta','preserve']) assert.ok(typeof row[field] === 'string' && row[field].length > 40, `${row.id}: missing ${field}`);
  assert.ok(row.scopeIds.length > 0, `${row.id}: missing ownership scope`);
  for (const scope of row.scopeIds) assert.ok(scopeIds.has(scope), `${row.id}: unknown scope ${scope}`);
  for (const file of row.sourceAnchors) assert.ok(fs.existsSync(path.join(root,file)), `${row.id}: missing source anchor ${file}`);
  assert.ok(manifest.surfaces.some(item => item.id === row.id), `${row.id}: missing manifest surface`);
}
for (const scope of [...scopes.nextScopes, ...scopes.outsideAnchorFollowups]) for (const file of scope.sourceAnchors) {
  assert.ok(fs.existsSync(path.join(root,file)), `${scope.id}: missing source anchor ${file}`);
}
for (const receipt of manifest.sourceImplementationReceipts) {
  assert.equal(receipt.evidence, 'source-only');
  assert.equal(receipt.runtimeStatus, 'unverified');
  assert.equal(receipt.captureStatus, 'missing');
}
assert.equal(manifest.runtimeStatus, 'unverified');
assert.equal(manifest.referenceViewerStatus, 'missing');
assert.equal(manifest.productionFixtureStatus, 'missing');
assert.equal(manifest.parityStatus, 'blocked');
const ledger = JSON.parse(fs.readFileSync(path.join(root,'design/native-feature-delivery.json'),'utf8'));
assert.equal(ledger.completionClaim, false);
assert.equal(ledger.rows.length, 1204);
const review = manifest.sourceReview;
const reversal = read('change-ledger.json');
assert.equal(review.reviewedCandidate, reversal.reviewedCandidate);
assert.equal(review.reviewedCandidate, scopes.reviewedCandidate);
assert.equal(review.reviewedCandidate, contracts.latestSourceReview);
let handoff = fs.readFileSync(path.join(root, 'design/workflow-refresh.md'), 'utf8');
if (process.env.NATIVE_DESIGN_STALE_COMPOSITION === '1') handoff += '\nShell/tab work is reported but absent';
assert.ok(handoff.includes('reconciled against `' + review.reviewedCandidate + '`'), 'Composition summary candidate is stale');
assert.ok(!handoff.includes('Shell/tab work is reported but absent'), 'Composition summary still excludes incorporated shell work');
assert.ok(!handoff.includes('confirmations remain separate'), 'Composition summary still excludes incorporated confirmation work');
assert.ok(handoff.includes('Missing Model Creator, external-source or canonical-tool engines remain their existing incomplete feature obligations.'));
const expectedFamilies = 'native-palette shared-controls fields-presets prepare preferences-project-setup renderer-preview native-monitor native-device-popups readers-overlays live-notifications workspace calibration-children setup-index embedded-palette embedded-composition workflow-navigation print-workspace print-setup shell-tabs confirmations selection-controls transform-inspector humidity-details appearance-properties device-name-editor gizmo-inspector-framing reader-details ams-drying nozzle-rack shared-list-rows print-continuations'.split(' ');
if (process.env.NATIVE_DESIGN_REMOVE_RECEIPT === '1') review.units = review.units.filter(unit => unit.family !== 'workspace');
assert.equal(review.units.length, 67, 'Missing incorporated source receipt');
assert.deepEqual([...new Set(review.units.map(unit => unit.family))].sort(), [...expectedFamilies].sort());
assert.equal(review.completionClaim, false);
assert.equal(review.runtimeStatus, 'unverified');
assert.equal(review.captureStatus, 'missing');
assert.equal(review.parityStatus, 'blocked');
assert.equal(new Set(review.units.map(unit => unit.commit)).size, review.units.length);
assert.deepEqual(review.units, reversal.incorporatedSourceUnits, 'Reversal ledger must match reviewed source receipts');
for (const unit of review.units) {
  assert.ok(Object.hasOwn(review.reversalRules, unit.classification));
  assert.ok(fs.existsSync(path.join(root, unit.article)), `Missing source article ${unit.article}`);
  execFileSync('git', ['merge-base', '--is-ancestor', unit.commit, review.reviewedCandidate], {cwd: root});
  if (unit.companion) assert.ok(review.units.some(candidate => candidate.commit === unit.companion));
}
if (process.env.NATIVE_DESIGN_RESTORE_HELD === '1') review.notInReviewedCandidate.push({commit: 'b60bd4c8b8fa5eb0d3e5a9f706621e73f8a1a476'});
for (const pending of review.notInReviewedCandidate) {
  const result = spawnSync('git', ['merge-base', '--is-ancestor', pending.commit, review.reviewedCandidate], {cwd: root});
  assert.equal(result.status, 1, 'Pending source receipt is not absent from the candidate');
}
assert.equal(review.notInReviewedCandidate.length, 0, 'Formerly held units are now incorporated');
assert.equal(review.units.find(unit => unit.commit === '59405fc894f96525db8f8edc3e45a3839e3001fd')?.classification, 'verification-only');
assert.ok(review.units.some(unit => unit.commit === 'b60bd4c8b8fa5eb0d3e5a9f706621e73f8a1a476'));
assert.ok(!handoff.includes('remains held'), 'Current handoff retains a stale held status');
assert.deepEqual(review.notInReviewedCandidate, reversal.notInReviewedCandidate);
const pureAppearance = [
  'b748affe0f687f6cfbe6068a82d32988047f790a',
  'df300fb9d93991751e840bdc586d2f770b47b9cf',
  '30031b21eedd58c56b83dc474465d47a3ccc8099',
  '8cfce63ae05d7be03823b3eb9e4b86c4488245b1',
  'a2be7df26cd5981da2c1e50d4c64c5e3a86609b9',
  '3e765ab09a7d25a1808d437034b8b0e12e67e7bb',
  'ecbbb99fb0c5c84ef58c636cce270e5eda13b8c8'
];
assert.deepEqual(review.units.filter(unit => unit.classification === 'appearance-only').map(unit => unit.commit).sort(), pureAppearance.sort(), 'Mixed layout or repair must not become paint-only');
assert.deepEqual(review.documentationReceipts, reversal.documentationReceipts);
assert.equal(review.documentationReceipts.length, 5);
for (const receipt of review.documentationReceipts) {
  assert.equal(receipt.classification, 'localization');
  execFileSync('git', ['merge-base', '--is-ancestor', receipt.commit, review.reviewedCandidate], {cwd: root});
}
const indexDirectory = 'docs/features/design-system';
let englishIndex = fs.readFileSync(path.join(root, indexDirectory, 'README.md'), 'utf8');
const cantoneseIndex = fs.readFileSync(path.join(root, indexDirectory, 'README.yue_HK.md'), 'utf8');
const indexHash = createHash('sha256').update(englishIndex.replaceAll('\r\n', '\n')).digest('hex');
assert.ok(cantoneseIndex.includes(`source-sha256: ${indexHash}`), 'Cantonese index source hash is stale');
if (process.env.NATIVE_DESIGN_REMOVE_INDEX_LINK === '1') englishIndex = englishIndex.replaceAll('studio-atlas-selection-controls.md', 'missing-selection-article.md');
for (const article of new Set(review.units.map(unit => unit.article))) {
  const relative = path.posix.relative(indexDirectory, article);
  const translated = article.replace(/\.md$/, '.yue_HK.md');
  const cantoneseTarget = fs.existsSync(path.join(root, translated)) ? path.posix.relative(indexDirectory, translated) : relative;
  assert.ok(englishIndex.includes(`](${relative})`), `Missing incorporated article link: ${relative}`);
  assert.ok(cantoneseIndex.includes(`](${cantoneseTarget})`), `Missing Cantonese index source link: ${cantoneseTarget}`);
}
assert.ok(cantoneseIndex.includes('calibration-viewport-layout.md'));
assert.ok(!cantoneseIndex.includes('calibration-viewport-layout.yue_HK.md'), 'Do not invent a separate calibration viewport companion');
assert.deepEqual(scopes.followupSurfaceFamilies.map(row => row.id), ['shared-list-rows', 'ams-drying-pages', 'nozzle-rack-details']);
for (const row of scopes.followupSurfaceFamilies) {
  assert.equal(row.status, 'source-incorporated-partial');
  assert.equal(row.unchangedAtCandidate, 'c7868b48536e8f8277d6872bd7d8def9d8463d02');
  for (const commit of row.sourceUnits) assert.ok(review.units.some(unit => unit.commit === commit));
  for (const file of row.sourceAnchors) {
    assert.ok(fs.existsSync(path.join(root, file)));
    execFileSync('git', ['diff', '--quiet', manifest.baselineCommit, row.unchangedAtCandidate, '--', file], {cwd: root});
  }
  for (const route of row.reachableFrom)
    assert.ok(fs.readFileSync(path.join(root, route.path), 'utf8').includes(route.needle), `Missing reachable route ${row.id}: ${route.path}`);
}
console.log(`Validated ${contracts.surfaces.length} explicit surface anatomy contracts and ${scopes.outsideAnchorFollowups.length} outside-anchor followups. Source inventory only; runtime and parity remain unverified.`);
console.log(`Verified local ancestry for ${review.units.length} source receipts across ${expectedFamilies.length} families at ${review.reviewedCandidate}; no rendered acceptance.`);
console.log('Verified five documentation receipts, both article indexes and three incorporated followup families; connection/send repairs are incorporated, runtime remains unverified.');
