#!/usr/bin/env node
// Privacy and drift guard for the shared instruction mirror.
//
//   node scripts/instructions/check-instruction-mirror.mjs [--require]
//     [--source <sanitized-export.md>] [--private-terms <file>] [--root <dir>]
//
// Passes when README.md and AGENTS.md carry the same intact mirror and its
// body names no private detail. Fails when either copy was edited by hand,
// only one file was refreshed, the copies differ, the body is not sanitized,
// or (with --source) the mirror is stale against the given export. A
// repository with no mirror at all is reported as ABSENT and passes unless
// --require is given. The private term list comes from --private-terms or
// INSTRUCTION_MIRROR_PRIVATE_TERMS and stays outside the repository; terms are
// never printed. Exit status: 0 pass or absent, 1 fail, 2 usage error.
import { readFileSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { MIRROR_TARGETS, MirrorError, checkMirror, resolvePrivateTerms } from './instruction-mirror.mjs';

const defaultRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const VALUE_OPTIONS = new Set(['--source', '--root', '--private-terms']);

function parseArguments(argv) {
  const options = { require: false };
  for (let index = 0; index < argv.length; index += 1) {
    const name = argv[index];
    if (name === '--require') {
      options.require = true;
    } else if (VALUE_OPTIONS.has(name)) {
      const value = argv[index + 1];
      if (value === undefined || value.startsWith('--')) throw new MirrorError(`${name} needs a value.`);
      options[name.slice(2)] = value;
      index += 1;
    } else {
      throw new MirrorError(`Unknown argument: ${name}`);
    }
  }
  return options;
}

function main() {
  const options = parseArguments(process.argv.slice(2));
  const root = path.resolve(options.root ?? defaultRoot);
  const files = {};
  for (const name of Object.keys(MIRROR_TARGETS)) {
    try {
      files[name] = readFileSync(path.join(root, name), 'utf8');
    } catch {
      // checkMirror reports the missing file.
    }
  }
  const sourceText = options.source === undefined ? undefined : readFileSync(path.resolve(options.source), 'utf8');
  const { terms, note } = resolvePrivateTerms(options['private-terms']);
  const result = checkMirror({ files, sourceText, terms });
  if (result.status === 'absent') {
    console.log('ABSENT: no shared instruction mirror is published in README.md or AGENTS.md.');
    if (options.require) {
      console.log('FAIL: --require was given, so a published mirror is required.');
      process.exitCode = 1;
    }
    return;
  }
  console.log(note);
  if (result.status === 'failed') {
    for (const problem of result.problems) console.log(`FAIL: ${problem}`);
    process.exitCode = 1;
    return;
  }
  const { sourceRevision, mirroredOn, bodySha256 } = result.meta;
  console.log(`PASS: README.md and AGENTS.md carry the same sanitized mirror (revision ${sourceRevision}, `
    + `mirrored ${mirroredOn}, body SHA-256 ${bodySha256}).`);
}

try {
  main();
} catch (error) {
  console.error(error instanceof MirrorError ? `Refused: ${error.message}` : error.stack);
  process.exitCode = 2;
}
