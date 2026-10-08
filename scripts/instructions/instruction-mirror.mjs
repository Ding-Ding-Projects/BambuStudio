// Shared instruction mirror: one generated, clearly labelled copy of the
// maintainer's sanitized shared agent instructions in README.md and AGENTS.md.
//
// The canonical instructions are private and are never read from here. The
// maintainer exports a sanitized copy outside this repository and hands that
// file to refresh-instruction-mirror.mjs, which renders the same body into
// both files between fixed markers. See
// docs/features/documentation/instruction-mirror.md.
import { createHash } from 'node:crypto';

export const MIRROR_BEGIN = '<!-- shared-instructions-mirror:begin -->';
export const MIRROR_END = '<!-- shared-instructions-mirror:end -->';
export const BODY_BEGIN = '<!-- shared-instructions-mirror:body:begin -->';
export const BODY_END = '<!-- shared-instructions-mirror:body:end -->';
const META_PREFIX = '<!-- shared-instructions-mirror:meta ';
const MARKER_PATTERN = /<!--\s*shared-instructions-mirror:/u;

export const MIRROR_TARGETS = Object.freeze({ 'README.md': 'readme', 'AGENTS.md': 'agents' });
export const MIRROR_HEADING = '# Shared agent instructions (mirror)';
export const ARTICLE_PATH = 'docs/features/documentation/instruction-mirror.md';
const README_SUMMARY = 'Show the mirrored shared agent instructions';
// The README block goes before this heading when it exists, else at the end.
const README_ANCHOR = /^# Report issue\b/mu;

const REVISION = /^[0-9a-f]{7,64}$/u;
const DATE = /^\d{4}-\d{2}-\d{2}$/u;
const SHA256 = /^[0-9a-f]{64}$/u;

export class MirrorError extends Error {}

export function bodyDigest(body) {
  return createHash('sha256').update(body, 'utf8').digest('hex');
}

export function validateRevision(value) {
  if (typeof value !== 'string' || !REVISION.test(value)) {
    throw new MirrorError('--source-revision must be the canonical source revision: 7 to 64 lowercase hexadecimal characters.');
  }
  return value;
}

export function validateDate(value) {
  const parsed = typeof value === 'string' && DATE.test(value) ? new Date(`${value}T00:00:00Z`) : null;
  if (!parsed || Number.isNaN(parsed.getTime()) || parsed.toISOString().slice(0, 10) !== value) {
    throw new MirrorError('--date must be a calendar date written as YYYY-MM-DD.');
  }
  return value;
}

// Lines outside fenced code blocks, so a quoted heading or marker inside a
// fence is treated as content.
function proseLines(lines) {
  const result = [];
  let fence = null;
  lines.forEach((line, index) => {
    const opener = /^ {0,3}(`{3,}|~{3,})/u.exec(line);
    if (fence) {
      if (opener && opener[1][0] === fence[0] && opener[1].length >= fence.length && line.trim() === opener[1]) fence = null;
      return;
    }
    if (opener) {
      fence = opener[1];
      return;
    }
    result.push(index);
  });
  return result;
}

// Turn the exported file into the mirror body: UTF-8 without a byte-order
// mark, LF line endings, no trailing blank lines, and no level-one heading.
// A single leading title is replaced by the mirror heading; any other
// level-one heading would break the mirror's outline and is refused.
export function normalizeSource(text) {
  if (typeof text !== 'string') throw new MirrorError('The instruction source must be text.');
  let body = text.replace(/^﻿/u, '').replace(/\r\n?/gu, '\n');
  if (MARKER_PATTERN.test(body)) {
    throw new MirrorError('The instruction source already contains mirror markers; export it again from the canonical instructions.');
  }
  let lines = body.split('\n').map((line) => line.replace(/[ \t]+$/u, ''));
  while (lines.length && lines[0].trim() === '') lines.shift();
  if (lines.length && /^# \S/u.test(lines[0])) {
    lines.shift();
    while (lines.length && lines[0].trim() === '') lines.shift();
  }
  for (const index of proseLines(lines)) {
    if (/^ {0,3}#(?:\s|$)/u.test(lines[index])) {
      throw new MirrorError(`Source line ${index + 1}: a level-one heading inside the instructions; sections must start at level two.`);
    }
  }
  body = lines.join('\n').replace(/\n+$/u, '');
  if (!body.trim()) throw new MirrorError('The instruction source has no content after its title.');
  return `${body}\n`;
}

function labelLines(meta) {
  return [
    '> [!NOTE]',
    "> This section is a generated mirror of the maintainer's shared agent instructions, so that",
    '> agents and contributors working here can read the rules without access to the canonical copy.',
    '> Do not edit it here: a change made here is overwritten by the next refresh and never reaches',
    '> the canonical instructions. Change the canonical instructions first, then refresh this mirror',
    `> as described in [Shared instruction mirror](${ARTICLE_PATH}).`,
    '>',
    '> This public copy is sanitized. Private locations, machines, accounts, network addresses and',
    '> credentials are generalized, and the private conversation vocabulary is omitted entirely, as',
    '> the canonical instructions require for a public repository. Repository-specific rules',
    '> elsewhere in this file still apply.',
    '>',
    `> Source revision \`${meta.sourceRevision}\`, mirrored ${meta.mirroredOn}, body SHA-256`,
    `> \`${meta.bodySha256}\`.`,
  ];
}

export function renderMirrorBlock({ kind, body, sourceRevision, mirroredOn }) {
  if (kind !== 'readme' && kind !== 'agents') throw new MirrorError(`Unknown mirror target kind: ${kind}`);
  validateRevision(sourceRevision);
  validateDate(mirroredOn);
  const meta = { sourceRevision, mirroredOn, bodySha256: bodyDigest(body) };
  const lines = [
    MIRROR_BEGIN,
    `${META_PREFIX}source-revision=${meta.sourceRevision} mirrored=${meta.mirroredOn} body-sha256=${meta.bodySha256} -->`,
    MIRROR_HEADING,
    '',
    ...labelLines(meta),
    '',
  ];
  if (kind === 'readme') lines.push('<details>', `<summary>${README_SUMMARY}</summary>`, '');
  lines.push(BODY_BEGIN, '', body.replace(/\n$/u, ''), '', BODY_END);
  if (kind === 'readme') lines.push('', '</details>');
  lines.push(MIRROR_END);
  return `${lines.join('\n')}\n`;
}

function occurrences(text, needle) {
  return text.split(needle).length - 1;
}

// Locate the one mirror block in a file. Returns null when the file has none,
// and throws when the markers are duplicated, unbalanced or out of order.
export function findMirrorBlock(text) {
  const begins = occurrences(text, MIRROR_BEGIN);
  const ends = occurrences(text, MIRROR_END);
  if (begins === 0 && ends === 0) return null;
  if (begins !== 1 || ends !== 1) {
    throw new MirrorError(`Expected one mirror block, found ${begins} begin and ${ends} end markers.`);
  }
  const start = text.indexOf(MIRROR_BEGIN);
  const endMarker = text.indexOf(MIRROR_END);
  if (endMarker < start) throw new MirrorError('The mirror end marker comes before its begin marker.');
  let end = endMarker + MIRROR_END.length;
  if (text[end] === '\n') end += 1;
  return { start, end, block: text.slice(start, end) };
}

export function parseMirrorBlock(block) {
  const metaMatch = /<!-- shared-instructions-mirror:meta source-revision=(\S+) mirrored=(\S+) body-sha256=(\S+) -->/u.exec(block);
  if (!metaMatch) throw new MirrorError('The mirror block has no readable source metadata.');
  const [, sourceRevision, mirroredOn, bodySha256] = metaMatch;
  if (!REVISION.test(sourceRevision) || !DATE.test(mirroredOn) || !SHA256.test(bodySha256)) {
    throw new MirrorError('The mirror block metadata is malformed.');
  }
  const bodyStart = block.indexOf(`${BODY_BEGIN}\n\n`);
  const bodyEnd = block.indexOf(`\n\n${BODY_END}`);
  if (bodyStart < 0 || bodyEnd < bodyStart || occurrences(block, BODY_BEGIN) !== 1 || occurrences(block, BODY_END) !== 1) {
    throw new MirrorError('The mirror block has no single delimited body.');
  }
  const body = `${block.slice(bodyStart + BODY_BEGIN.length + 2, bodyEnd)}\n`;
  return { meta: { sourceRevision, mirroredOn, bodySha256 }, body };
}

export function extractMirror(text) {
  const found = findMirrorBlock(text);
  return found ? { ...found, ...parseMirrorBlock(found.block) } : null;
}

// Replace the existing block, or insert a new one: before the README's
// "Report issue" heading, or at the end of the file.
export function applyMirrorBlock(text, kind, block) {
  const existing = findMirrorBlock(text);
  if (existing) return text.slice(0, existing.start) + block + text.slice(existing.end);
  const anchor = kind === 'readme' ? README_ANCHOR.exec(text) : null;
  if (anchor) {
    const before = text.slice(0, anchor.index).replace(/\n*$/u, '\n\n');
    return `${before}${block}\n${text.slice(anchor.index)}`;
  }
  const before = text.replace(/\n*$/u, '');
  return `${before}\n\n${block}`;
}

// Compute the new README.md and AGENTS.md contents for a refresh. An unchanged
// body and revision keep the recorded mirror date unless one is given, so a
// repeated refresh rewrites nothing.
export function planRefresh({ files, sourceText, sourceRevision, mirroredOn }) {
  validateRevision(sourceRevision);
  if (mirroredOn !== undefined) validateDate(mirroredOn);
  const body = normalizeSource(sourceText);
  const digest = bodyDigest(body);
  const targets = Object.entries(MIRROR_TARGETS).map(([name, kind]) => {
    if (typeof files[name] !== 'string') throw new MirrorError(`${name} is missing from the repository root.`);
    return { name, kind, current: files[name], existing: extractMirror(files[name]) };
  });
  // Keep the recorded date only when both files already mirror this exact
  // body and revision on the same date.
  const dates = new Set(targets.map(({ existing }) => existing
    && existing.meta.bodySha256 === digest && existing.meta.sourceRevision === sourceRevision
    ? existing.meta.mirroredOn : null));
  const recorded = dates.size === 1 ? [...dates][0] : null;
  const date = mirroredOn ?? recorded ?? new Date().toISOString().slice(0, 10);
  const plan = targets.map(({ name, kind, current }) => {
    const next = applyMirrorBlock(current, kind, renderMirrorBlock({ kind, body, sourceRevision, mirroredOn: date }));
    return { name, kind, current, next, changed: next !== current };
  });
  return { body, digest, mirroredOn: date, plan };
}
