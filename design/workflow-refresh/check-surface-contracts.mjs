import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { execFileSync, spawnSync } from 'node:child_process';

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
const expectedFamilies = 'native-palette shared-controls fields-presets prepare preferences-project-setup renderer-preview native-monitor native-device-popups readers-overlays live-notifications workspace calibration-children setup-index embedded-palette embedded-composition workflow-navigation print-workspace print-setup'.split(' ');
if (process.env.NATIVE_DESIGN_REMOVE_RECEIPT === '1') review.units = review.units.filter(unit => unit.family !== 'workspace');
assert.equal(review.units.length, 31, 'Missing incorporated source receipt');
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
for (const pending of review.notInReviewedCandidate) {
  const result = spawnSync('git', ['merge-base', '--is-ancestor', pending.commit, review.reviewedCandidate], {cwd: root});
  assert.equal(result.status, 1, 'Pending source receipt is not absent from the candidate');
}
const pureAppearance = [
  'b748affe0f687f6cfbe6068a82d32988047f790a',
  'df300fb9d93991751e840bdc586d2f770b47b9cf',
  '30031b21eedd58c56b83dc474465d47a3ccc8099',
  '8cfce63ae05d7be03823b3eb9e4b86c4488245b1',
  'a2be7df26cd5981da2c1e50d4c64c5e3a86609b9'
];
assert.deepEqual(review.units.filter(unit => unit.classification === 'appearance-only').map(unit => unit.commit).sort(), pureAppearance.sort(), 'Mixed layout or repair must not become paint-only');
console.log(`Validated ${contracts.surfaces.length} explicit surface anatomy contracts and ${scopes.outsideAnchorFollowups.length} outside-anchor followups. Source inventory only; runtime and parity remain unverified.`);
console.log(`Verified local ancestry for ${review.units.length} source receipts across ${expectedFamilies.length} families at ${review.reviewedCandidate}; no rendered acceptance.`);
