import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';

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
console.log(`Validated ${contracts.surfaces.length} explicit surface anatomy contracts and ${scopes.outsideAnchorFollowups.length} outside-anchor followups. Source inventory only; runtime and parity remain unverified.`);
