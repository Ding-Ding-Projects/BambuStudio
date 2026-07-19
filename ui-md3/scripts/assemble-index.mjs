#!/usr/bin/env node

import { readFile, writeFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const rootDir = path.resolve(scriptDir, '..');
const indexPath = path.join(rootDir, 'index.html');
const screenDir = path.join(rootDir, 'app', 'screens');
const screenIds = [
  'home', 'prepare', 'preview', 'device', 'multi',
  'project', 'calibration', 'filament', 'settings'
];

const checkOnly = process.argv.includes('--check');
const writeRequested = process.argv.includes('--write');
if (!checkOnly && !writeRequested) {
  console.error('Usage: node ui-md3/scripts/assemble-index.mjs --check|--write');
  process.exitCode = 2;
} else {
  const original = await readFile(indexPath, 'utf8');
  const eol = original.includes('\r\n') ? '\r\n' : '\n';
  let assembled = original.replace(/\r\n/g, '\n');

  for (const id of screenIds) {
    const sourcePath = path.join(screenDir, `${id}.template.html`);
    const source = (await readFile(sourcePath, 'utf8')).replace(/\r\n/g, '\n').trim();
    const indented = source.split('\n').map((line) => `  ${line}`).join('\n');
    const block = new RegExp(
      `^  <template data-screen="${id}">[\\s\\S]*?^  </template>(?=\\n(?:\\s*\\n)?(?:  <template data-screen=|  <script))`,
      'm'
    );
    const matches = assembled.match(new RegExp(block.source, 'gm')) || [];
    if (matches.length !== 1) {
      throw new Error(`Expected one assembled template block for "${id}", found ${matches.length}`);
    }
    assembled = assembled.replace(block, indented);
  }

  const output = eol === '\r\n' ? assembled.replace(/\n/g, '\r\n') : assembled;
  if (checkOnly) {
    if (output !== original) {
      console.error('ui-md3/index.html is out of sync with app/screens/*.template.html');
      process.exitCode = 1;
    } else {
      console.log('ui-md3/index.html screen templates are synchronized');
    }
  } else {
    await writeFile(indexPath, output, 'utf8');
    console.log('Updated ui-md3/index.html from modular screen templates');
  }
}
