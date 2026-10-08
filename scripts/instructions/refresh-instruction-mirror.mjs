#!/usr/bin/env node
// Refresh the shared instruction mirror in README.md and AGENTS.md.
//
//   node scripts/instructions/refresh-instruction-mirror.mjs \
//     --source <sanitized-export.md> --source-revision <canonical-revision> [--date YYYY-MM-DD]
//   node scripts/instructions/refresh-instruction-mirror.mjs --check --source ... --source-revision ...
//
// --source is the maintainer's sanitized export of the canonical shared
// instructions, kept outside this repository. --source-revision records the
// canonical revision it was exported from. --check writes nothing and exits 1
// when either file would change. --root selects another repository root (tests).
import { readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { MIRROR_TARGETS, MirrorError, planRefresh } from './instruction-mirror.mjs';

const defaultRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const VALUE_OPTIONS = new Set(['--source', '--source-revision', '--date', '--root']);

function parseArguments(argv) {
  const options = { check: false };
  for (let index = 0; index < argv.length; index += 1) {
    const name = argv[index];
    if (name === '--check') {
      options.check = true;
    } else if (VALUE_OPTIONS.has(name)) {
      const value = argv[index + 1];
      if (value === undefined || value.startsWith('--')) throw new MirrorError(`${name} needs a value.`);
      options[name.slice(2)] = value;
      index += 1;
    } else {
      throw new MirrorError(`Unknown argument: ${name}`);
    }
  }
  if (!options.source) throw new MirrorError('--source is required: the sanitized export of the shared instructions.');
  if (!options['source-revision']) throw new MirrorError('--source-revision is required: the canonical revision the export came from.');
  return options;
}

function main() {
  const options = parseArguments(process.argv.slice(2));
  const root = path.resolve(options.root ?? defaultRoot);
  const files = {};
  for (const name of Object.keys(MIRROR_TARGETS)) files[name] = readFileSync(path.join(root, name), 'utf8');
  const sourceText = readFileSync(path.resolve(options.source), 'utf8');
  const { digest, mirroredOn, plan } = planRefresh({
    files, sourceText, sourceRevision: options['source-revision'], mirroredOn: options.date,
  });
  const changed = plan.filter((entry) => entry.changed);
  if (options.check) {
    for (const entry of changed) console.log(`STALE: ${entry.name} does not carry the current mirror.`);
    if (changed.length) {
      process.exitCode = 1;
      return;
    }
    console.log(`CURRENT: README.md and AGENTS.md mirror body ${digest}.`);
    return;
  }
  for (const entry of changed) writeFileSync(path.join(root, entry.name), entry.next, 'utf8');
  const names = changed.map((entry) => entry.name).join(', ') || 'nothing (already current)';
  console.log(`REFRESHED: ${names}; body SHA-256 ${digest}, revision ${options['source-revision']}, mirrored ${mirroredOn}.`);
}

try {
  main();
} catch (error) {
  console.error(error instanceof MirrorError ? `Refused: ${error.message}` : error.stack);
  process.exitCode = 2;
}
