import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { existsSync } from 'node:fs';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const inventoryPath = path.join(repoDir, 'docs', 'features', 'design-system', 'cheap-jor-inventory.md');
const captureRoot = path.join(repoDir, 'docs', 'screenshots', 'md3-everything');

// Hand-written: every clipping defect that has been found must keep its row.
// A rule alone passes on a row that was deleted; this list does not.
const REQUIRED_ROWS = ['CJ-001', 'CJ-002', 'CJ-003', 'CJ-004', 'CJ-005', 'CJ-006'];
const STATUSES = new Set(['fixed-unverified', 'verified', 'open']);

const normalise = (text) => text.replace(/\r\n|\r/g, '\n');

export function parseInventory(markdown) {
  const text = normalise(markdown);
  const begin = text.indexOf('<!-- cheap-jor-inventory:begin -->');
  const end = text.indexOf('<!-- cheap-jor-inventory:end -->');
  assert.ok(begin >= 0 && end > begin, 'inventory markers missing');
  const block = text.slice(begin, end);
  const rows = [];
  for (const line of block.split('\n')) {
    if (!line.startsWith('| CJ-')) continue;
    const cells = line.split('|').slice(1, -1).map((c) => c.trim());
    assert.equal(cells.length, 9, 'row has ' + cells.length + ' cells: ' + line);
    const [id, surface, tuple, symptom, cause, commit, before, after, status] = cells;
    rows.push({ id, surface, tuple, symptom, cause, commit, before, after, status });
  }
  return rows;
}

function commitExists(sha) {
  try {
    execFileSync('git', ['cat-file', '-e', sha + '^{commit}'], { cwd: repoDir, stdio: 'ignore' });
    return true;
  } catch {
    return false;
  }
}

const inventory = await readFile(inventoryPath, 'utf8');
const rows = parseInventory(inventory);

test('every found clipping defect keeps its inventory row', () => {
  const ids = rows.map((r) => r.id);
  for (const required of REQUIRED_ROWS) {
    assert.ok(ids.includes(required), required + ' missing from the inventory');
  }
  assert.equal(new Set(ids).size, ids.length, 'duplicate row ids');
});

test('every row is complete and its fix commit exists', () => {
  assert.ok(rows.length >= REQUIRED_ROWS.length);
  for (const row of rows) {
    assert.match(row.id, /^CJ-\d{3}$/, row.id);
    for (const key of ['surface', 'tuple', 'symptom', 'cause']) {
      assert.ok(row[key].length >= 8, row.id + ' ' + key + ' is too short to mean anything');
    }
    assert.ok(STATUSES.has(row.status), row.id + ' status ' + row.status);
    assert.match(row.commit, /^[0-9a-f]{7,40}$/, row.id + ' fix commit');
    assert.ok(commitExists(row.commit), row.id + ' fix commit ' + row.commit + ' does not exist');
  }
});

test('a verified row has both captures on disk; an unverified row says pending', () => {
  for (const row of rows) {
    if (row.status === 'verified') {
      for (const key of ['before', 'after']) {
        const rel = row[key];
        assert.notEqual(rel, 'pending', row.id + ' ' + key + ' capture is pending but row is verified');
        assert.ok(existsSync(path.join(captureRoot, rel)), row.id + ' ' + key + ' capture ' + rel + ' missing');
      }
    } else {
      assert.ok(
        row.before === 'pending' || existsSync(path.join(captureRoot, row.before)),
        row.id + ' before capture ' + row.before + ' named but missing'
      );
    }
  }
});

test('the tuple matrix table never records a run that did not happen as clean', () => {
  const text = normalise(inventory);
  const matrix = text.slice(text.indexOf('## Tuple matrix'));
  for (const run of ['baseline', 'after']) {
    const line = matrix.split('\n').find((l) => l.startsWith('| ' + run + ' |'));
    assert.ok(line, run + ' run row missing');
    const cells = line.split('|').slice(1, -1).map((c) => c.trim());
    const [, artifact, , , , , findings] = cells;
    if (artifact === 'not run') {
      assert.equal(findings, '', run + ' claims findings without an artifact commit');
    } else {
      assert.match(artifact, /^[0-9a-f]{7,40}$/, run + ' artifact commit');
      assert.ok(commitExists(artifact), run + ' artifact commit ' + artifact + ' does not exist');
      assert.match(findings, /^\d+$/, run + ' findings must be a count');
    }
  }
});
