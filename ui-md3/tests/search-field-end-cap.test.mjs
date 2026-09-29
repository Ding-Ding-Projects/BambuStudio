import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The kit SearchField is a pill whose rounded ends have a radius of half its
// height. Its trailing icon buttons are child windows, and a child window
// paints its whole square: placed too close to the right end, the last one
// covered the end's arc, so the outline stopped short and a sliver of the
// arc floated beside it (clipping inventory CJ-015, seen in Smart home).
// The trailing padding must keep the button's corner clear of the arc.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const source = await readFile(path.join(repoDir, 'src', 'slic3r', 'GUI', 'Widgets', 'SearchField.cpp'), 'utf8');

const constant = (name) => {
  const match = source.match(new RegExp(`constexpr int ${name}\\s*=\\s*(\\d+);`));
  assert.ok(match, `${name} must be a constexpr int in SearchField.cpp`);
  return Number(match[1]);
};

test('the trailing icon button stays clear of the pill\'s rounded end', () => {
  const height = constant('kHeight');
  const action = constant('kActionPx');
  const padRight = constant('kPadRight');
  const radius = height / 2;
  const inset = (height - action) / 2;           // the button's top and bottom margin
  const dy = radius - inset;                      // corner height measured from the pill's centre line
  const arcReach = radius - Math.sqrt(radius * radius - dy * dy); // how far in from the end the arc sits at that height
  assert.ok(padRight >= Math.ceil(arcReach),
    `kPadRight ${padRight} lets a ${action} px button cover the arc; it needs at least ${Math.ceil(arcReach)}`);
});
