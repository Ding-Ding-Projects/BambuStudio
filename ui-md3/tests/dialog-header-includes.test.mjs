import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// A source that uses the Material dialogs has to reach the header that declares
// them, MsgDialog.hpp, through its own includes. The forced precompiled header
// does not include it, so a missing include is a compile error on the next build
// and nothing short of a build notices. The appearance editor once called
// md3_message_box() with no route to its declaration.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const srcDir = path.join(repoDir, 'src');
const slic3rDir = path.join(srcDir, 'slic3r');
// The quoted-include search path of libslic3r_gui (src/slic3r/CMakeLists.txt):
// the including file's own folder first, then src/slic3r/Utils and src.
const includeDirs = [path.join(slic3rDir, 'Utils'), srcDir];
const HEADER = path.join(slic3rDir, 'GUI', 'MsgDialog.hpp');
const DIALOGS = ['md3_message_box', 'MessageDialog', 'RichMessageDialog', 'InfoDialog', 'ErrorDialog', 'WarningDialog',
  'TextEntryDialog', 'NumberEntryDialog', 'MultiChoiceDialog', 'BusyInfo'];
const USE = new RegExp(`\\b(?:${DIALOGS.join('|')})\\b`, 'g');

const strip = (text) => text.replace(/\r\n/g, '\n')
  .replace(/\/\*[\s\S]*?\*\//g, '')
  .replace(/\/\/.*$/gm, '')
  .replace(/"(?:[^"\\\n]|\\.)*"/g, '""');

const cache = new Map();
async function includesOf(file) {
  if (!cache.has(file)) {
    const text = (await readFile(file, 'utf8')).replace(/\r\n/g, '\n').replace(/\/\*[\s\S]*?\*\//g, '');
    const found = [];
    for (const match of text.matchAll(/^\s*#\s*include\s*"([^"]+)"/gm)) {
      const name = match[1];
      const target = [path.dirname(file), ...includeDirs].map((dir) => path.resolve(dir, name)).find((p) => existsSync(p));
      if (target) found.push(target);
    }
    cache.set(file, found);
  }
  return cache.get(file);
}

async function reaches(file, header) {
  const seen = new Set([file]);
  const queue = [file];
  while (queue.length) {
    for (const next of await includesOf(queue.shift())) {
      if (next === header) return true;
      if (!seen.has(next)) {
        seen.add(next);
        queue.push(next);
      }
    }
  }
  return false;
}

async function sources(dir) {
  const out = [];
  for (const entry of await readdir(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...await sources(full));
    else if (/\.(cpp|hpp)$/.test(entry.name)) out.push(full);
  }
  return out;
}

test('every source that uses a Material dialog reaches MsgDialog.hpp', async () => {
  const missing = [];
  let users = 0;
  for (const file of await sources(slic3rDir)) {
    if (file === HEADER) continue;
    const code = strip(await readFile(file, 'utf8'));
    const used = new Set(code.match(USE) ?? []);
    // A header may name a dialog through its own forward declaration (a pointer
    // member, say); the source that constructs it is the one that needs the header.
    const declared = new Set([...code.matchAll(/\b(?:class|struct)\s+(\w+)\s*;/g)].map((m) => m[1]));
    if (used.size === 0 || [...used].every((name) => declared.has(name))) continue;
    users += 1;
    if (!await reaches(file, HEADER)) missing.push(path.relative(repoDir, file).replaceAll('\\', '/'));
  }
  assert.ok(users > 50, `expected the dialogs in use across the GUI, found ${users} files`);
  assert.deepEqual(missing, [], 'these use a Material dialog with no include path to MsgDialog.hpp');
});
