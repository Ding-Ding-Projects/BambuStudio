// Shared instruction mirror: one generated, clearly labelled copy of the
// maintainer's sanitized shared agent instructions in README.md and AGENTS.md.
//
// The canonical instructions are private and are never read from here. The
// maintainer exports a sanitized copy outside this repository and hands that
// file to refresh-instruction-mirror.mjs, which renders the same body into
// both files between fixed markers. See
// docs/features/documentation/instruction-mirror.md.
import { createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';

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

function toLf(text) {
  return text.replace(/\r\n/gu, '\n');
}

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

// Lines outside fenced code blocks, so a quoted heading inside a fence is
// treated as content.
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
  let body = text.replace(/^\uFEFF/u, '').replace(/\r\n?/gu, '\n');
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

// ---------------------------------------------------------------------------
// Privacy guard. A sanitized mirror names no absolute path outside the
// repository, user name or home directory, machine name or host inventory,
// network address, SSH target, container host, token or credential. Generic
// references stay allowed: environment variables such as %USERPROFILE% or
// $HOME, `~/` paths, system paths such as /usr/bin/env, loopback addresses,
// the documentation address ranges, and the role addresses listed below.
// Findings carry a line, a column and a category, never the matched text, so
// a refusal cannot repeat a secret into a log.

const ALLOWED_ADDRESSES = new Set(['noreply@anthropic.com', 'noreply@github.com', 'git@github.com']);
const OCTET = '(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]?\\d)';
const IPV4_TEXT = `${OCTET}(?:\\.${OCTET}){3}`;
const IPV4_ONLY = new RegExp(`^${IPV4_TEXT}$`, 'u');

function allowedIpv4(text) {
  const [a, b, c] = text.split('.').map(Number);
  if (a === 127 || text === '0.0.0.0' || text === '255.255.255.255') return true;
  return (a === 192 && b === 0 && c === 2) || (a === 198 && b === 51 && c === 100) || (a === 203 && b === 0 && c === 113);
}

function credentialValue(value) {
  return /\d/u.test(value) || value.length >= 12;
}

function classifyAddress(match, before) {
  const [, user, host, path] = match;
  if (ALLOWED_ADDRESSES.has(`${user}@${host}`.toLowerCase())) return null;
  if (/ssh:\/\/$/iu.test(before)) return null; // reported by the ssh:// rule
  if (/\b(?:ssh|scp|sftp|rsync|mosh)\s+(?:-\S+\s+)*$/iu.test(before)) return 'SSH target';
  if (IPV4_ONLY.test(host)) return 'SSH target';
  const looksLikeHost = /[A-Za-z]/u.test(host) && (host.includes('.') ? /\.[A-Za-z][A-Za-z0-9-]*$/u.test(host) : true);
  if (path && looksLikeHost) return 'SSH target';
  if (host.includes('.') && looksLikeHost) return 'account address';
  return null;
}

const PATH_END = '[^\\s`\'")\\]]';
const PRIVACY_RULES = [
  { category: 'absolute path outside the repository', pattern: /(?<![\w.+-])[A-Za-z]:[\\/](?![\\/])/gu },
  { category: 'absolute path outside the repository', pattern: /(?<![\w\\])\\\\[A-Za-z0-9][\w.$-]*\\[\w.$-]+/gu },
  { category: 'absolute path outside the repository',
    pattern: new RegExp(`(?<![\\w.:/~-])/(?:[A-Za-z]/)?(?:home|Users|root|mnt|media|Volumes|tmp|private|srv|workspaces?|data)/${PATH_END}*`, 'gu') },
  { category: 'absolute path outside the repository', pattern: new RegExp(`\\bfile://${PATH_END}+`, 'giu') },
  { category: 'machine name', pattern: /\b(?:DESKTOP|LAPTOP|WIN|PC)-[A-Z0-9]{5,15}\b/gu },
  { category: 'machine name',
    pattern: /(?<![\w.-])(?:[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.)+(?:local|lan|internal|intranet|localdomain|home\.arpa|corp)(?![\w-])/giu },
  { category: 'machine name', pattern: /(?<![\w:-])[0-9a-f]{2}([:-])(?:[0-9a-f]{2}\1){4}[0-9a-f]{2}(?![\w:-])/giu },
  { category: 'host inventory', pattern: /^[ \t]*HostName[ \t]+\S+/gmu },
  { category: 'host inventory', pattern: /^[ \t]*Host[ \t]+[\w.*?-]+[ \t]*$/gmu },
  { category: 'IP address', pattern: new RegExp(`(?<![\\w.])${IPV4_TEXT}(?!\\w|\\.\\d)`, 'gu'),
    // A four-part version number is written after a word that says so, which
    // may end the previous line of a wrapped paragraph.
    accept: (match, before, context) => allowedIpv4(match[0])
      || /\b(?:version|release|build|upstream|studio)\s*$/iu.test(context) },
  { category: 'IP address', pattern: /(?<![\w:.])(?:[0-9a-f]{1,4}:){7}[0-9a-f]{1,4}(?![\w:])/giu },
  { category: 'IP address',
    pattern: /(?<![\w:.])(?:[0-9a-f]{1,4}(?::[0-9a-f]{1,4}){0,6})?::(?:[0-9a-f]{1,4}(?::[0-9a-f]{1,4}){0,6})?(?![\w:])/giu,
    accept: (match) => match[0] === '::' || match[0] === '::1' },
  { category: 'SSH target', pattern: new RegExp(`\\bssh://${PATH_END}+`, 'giu') },
  { category: 'account address', pattern: /(?<![\w.+-])([\w.+-]+)@([A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*)(:(?!\/\/)[\w~./-]*)?/gu,
    classify: classifyAddress },
  { category: 'container host', pattern: /\bDOCKER_HOST\s*=\s*\S+/gu },
  { category: 'container host', pattern: new RegExp(`\\btcp://${PATH_END}+`, 'giu') },
  { category: 'container host', pattern: /\bdocker(?:[ \t]+[^\s`]+)*?[ \t]+(?:-H|--host)(?:[ \t]+|=)[^\s`]+/giu },
  { category: 'container host', pattern: /--docker[ \t]+host=\S+/giu },
  { category: 'token', pattern: /\bgh[pousr]_[A-Za-z0-9]{30,}/gu },
  { category: 'token', pattern: /\bgithub_pat_[A-Za-z0-9_]{30,}/gu },
  { category: 'token', pattern: /\bsk-(?:ant-|proj-)?[A-Za-z0-9_-]{20,}/gu },
  { category: 'token', pattern: /\b(?:AKIA|ASIA)[0-9A-Z]{16}\b/gu },
  { category: 'token', pattern: /\bxox[abposr]-[A-Za-z0-9-]{10,}/gu },
  { category: 'token', pattern: /\bAIza[0-9A-Za-z_-]{35}/gu },
  { category: 'token', pattern: /\bnpm_[A-Za-z0-9]{36}/gu },
  { category: 'token', pattern: /\beyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}/gu },
  { category: 'token', pattern: /discord(?:app)?\.com\/api\/webhooks\/\d+\/[\w-]+/giu },
  { category: 'token', pattern: /hooks\.slack\.com\/services\/[\w/]+/giu },
  { category: 'private key', pattern: /-----BEGIN [A-Z ]*PRIVATE KEY-----/gu },
  { category: 'credential',
    pattern: /\b(?:password|passwd|pwd|secret|api[_-]?key|access[_-]?key|client[_-]?secret|auth[_-]?token|secret[_-]?key|token)\s*[:=]\s*['"]?(?![<$%{])([^\s`'",;)]{4,})/giu,
    accept: (match) => !credentialValue(match[1]) },
  { category: 'credential', pattern: /\b[a-z][a-z0-9+.-]*:\/\/[^\s/:@`]+:[^\s/@`]+@/giu },
  { category: 'credential', pattern: /\bAuthorization:\s*(?:Bearer|Basic|token)\s+[A-Za-z0-9._~+/=-]{8,}/giu },
  { category: 'credential', pattern: /\bBearer\s+[A-Za-z0-9._~+/-]{20,}=*/gu },
];

function escapeRegExp(text) {
  return text.replace(/[.*+?^${}()|[\]\\]/gu, '\\$&');
}

// Case-insensitive, and bounded by word edges where a term starts or ends
// with an ASCII word character, so a term never matches inside a longer word.
function termPattern(term) {
  const lead = /^\w/u.test(term) ? '\\b' : '';
  const tail = /\w$/u.test(term) ? '\\b' : '';
  return new RegExp(`${lead}${escapeRegExp(term)}${tail}`, 'giu');
}

function lineStarts(text) {
  const starts = [0];
  for (let index = text.indexOf('\n'); index >= 0; index = text.indexOf('\n', index + 1)) starts.push(index + 1);
  return starts;
}

function position(starts, offset) {
  let low = 0;
  let high = starts.length - 1;
  while (low < high) {
    const middle = (low + high + 1) >> 1;
    if (starts[middle] <= offset) low = middle; else high = middle - 1;
  }
  return { line: low + 1, column: offset - starts[low] + 1 };
}

export function scanPrivacy(text, { terms = [] } = {}) {
  const source = String(text).replace(/\r\n?/gu, '\n');
  const starts = lineStarts(source);
  const findings = [];
  const seen = new Set();
  const record = (offset, category) => {
    const at = position(starts, offset);
    const key = `${at.line}:${at.column}:${category}`;
    if (!seen.has(key)) {
      seen.add(key);
      findings.push({ ...at, category });
    }
  };
  for (const rule of PRIVACY_RULES) {
    for (const match of source.matchAll(rule.pattern)) {
      const lineStart = source.lastIndexOf('\n', match.index - 1) + 1;
      const before = source.slice(lineStart, match.index);
      const context = source.slice(Math.max(0, match.index - 40), match.index);
      if (rule.accept && rule.accept(match, before, context)) continue;
      const category = rule.classify ? rule.classify(match, before) : rule.category;
      if (category) record(match.index, category);
    }
  }
  for (const term of terms) {
    for (const match of source.matchAll(termPattern(term))) record(match.index, 'private term');
  }
  return findings.sort((a, b) => a.line - b.line || a.column - b.column || a.category.localeCompare(b.category));
}

function collectStrings(value, out) {
  if (typeof value === 'string') {
    if (value.trim()) out.push(value.trim());
  } else if (Array.isArray(value)) {
    for (const item of value) collectStrings(item, out);
  } else if (value && typeof value === 'object') {
    for (const item of Object.values(value)) collectStrings(item, out);
  }
  return out;
}

// A private term list kept outside the repository: either a JSON document,
// whose string values (never its keys) are the terms, or plain text with one
// term per line, where blank lines and lines starting with # are ignored.
export function loadPrivateTerms(file) {
  let text;
  try {
    text = readFileSync(file, 'utf8').replace(/^\uFEFF/u, '');
  } catch (error) {
    throw new MirrorError(`The private term list could not be read (${error.code ?? 'error'}).`);
  }
  let terms;
  try {
    terms = collectStrings(JSON.parse(text), []);
  } catch {
    terms = text.split(/\r?\n/u).map((line) => line.trim()).filter((line) => line && !line.startsWith('#'));
  }
  if (!terms.length) throw new MirrorError('The private term list is empty.');
  return [...new Set(terms)];
}

// Resolve the term list from an explicit path or INSTRUCTION_MIRROR_PRIVATE_TERMS.
// Without either, the term scan is skipped and the reason is returned.
export function resolvePrivateTerms(explicitPath, env = process.env) {
  const file = explicitPath ?? (env.INSTRUCTION_MIRROR_PRIVATE_TERMS || undefined);
  if (!file) {
    return { terms: [], note: 'Private term scan skipped: no term list given (pass --private-terms or set INSTRUCTION_MIRROR_PRIVATE_TERMS).' };
  }
  const terms = loadPrivateTerms(file);
  return { terms, note: `Private term scan: ${terms.length} terms checked.` };
}

export function describeFindings(findings, prefix = 'line ') {
  return findings.map((finding) => `${prefix}${finding.line}: ${finding.category} (column ${finding.column})`);
}

// Drift and privacy check of both files. Status is 'absent' when neither file
// carries a mirror, 'ok' when both carry the same intact, sanitized mirror,
// and 'failed' with a list of problems otherwise.
export function checkMirror({ files, sourceText, terms = [] }) {
  const problems = [];
  const found = {};
  for (const [name, kind] of Object.entries(MIRROR_TARGETS)) {
    if (typeof files[name] !== 'string') {
      problems.push(`${name} is missing from the repository root.`);
      continue;
    }
    try {
      // A Windows checkout may convert the files to CRLF; the mirror is
      // compared with LF line endings, which keeps every line number.
      const text = toLf(files[name]);
      const mirror = extractMirror(text);
      if (mirror) found[name] = { kind, text, ...mirror };
    } catch (error) {
      if (!(error instanceof MirrorError)) throw error;
      problems.push(`${name}: ${error.message}`);
    }
  }
  const names = Object.keys(found);
  if (names.length === 0) return problems.length ? { status: 'failed', problems } : { status: 'absent', problems };
  for (const name of Object.keys(MIRROR_TARGETS)) {
    if (!found[name] && !problems.some((problem) => problem.startsWith(name))) {
      problems.push(`${name} carries no mirror while the other file does; refresh both together.`);
    }
  }
  for (const name of names) {
    const { kind, text, start, block, body, meta } = found[name];
    if (bodyDigest(body) !== meta.bodySha256) {
      problems.push(`${name}: the mirrored body does not match its recorded SHA-256; it was edited by hand.`);
    } else if (renderMirrorBlock({ kind, body, sourceRevision: meta.sourceRevision, mirroredOn: meta.mirroredOn }) !== block) {
      problems.push(`${name}: the mirror label or layout differs from the generated form; it was edited by hand.`);
    }
    const bodyOffset = start + block.indexOf(`${BODY_BEGIN}\n\n`) + BODY_BEGIN.length + 2;
    const firstBodyLine = text.slice(0, bodyOffset).split('\n').length;
    for (const line of describeFindings(scanPrivacy(body, { terms }).map((finding) => ({
      ...finding, line: finding.line + firstBodyLine - 1,
    })), `${name}:`)) {
      problems.push(`${line} is not sanitized.`);
    }
  }
  if (found['README.md'] && found['AGENTS.md']) {
    const [readme, agents] = [found['README.md'], found['AGENTS.md']];
    if (readme.body !== agents.body) problems.push('README.md and AGENTS.md mirror different bodies; refresh both from one export.');
    if (JSON.stringify(readme.meta) !== JSON.stringify(agents.meta)) {
      problems.push('README.md and AGENTS.md record different source metadata; refresh both from one export.');
    }
  }
  if (sourceText !== undefined && names.length) {
    const expected = normalizeSource(sourceText);
    for (const name of names) {
      if (found[name].body !== expected) problems.push(`${name}: the mirror is stale against the given export; run the refresh.`);
    }
  }
  return problems.length
    ? { status: 'failed', problems }
    : { status: 'ok', problems, meta: found['README.md'].meta };
}

// Compute the new README.md and AGENTS.md contents for a refresh. An unchanged
// body and revision keep the recorded mirror date unless one is given, so a
// repeated refresh rewrites nothing.
// The export is scanned as given, title included, so reported line numbers
// match the export file; any finding refuses the whole refresh.
export function planRefresh({ files, sourceText, sourceRevision, mirroredOn, terms = [] }) {
  validateRevision(sourceRevision);
  if (mirroredOn !== undefined) validateDate(mirroredOn);
  const body = normalizeSource(sourceText);
  const findings = scanPrivacy(sourceText.replace(/^\uFEFF/u, ''), { terms });
  if (findings.length) {
    throw new MirrorError(['The export is not sanitized, so nothing was written:',
      ...describeFindings(findings).map((line) => `  ${line}`)].join('\n'));
  }
  const digest = bodyDigest(body);
  const targets = Object.entries(MIRROR_TARGETS).map(([name, kind]) => {
    if (typeof files[name] !== 'string') throw new MirrorError(`${name} is missing from the repository root.`);
    return { name, kind, current: files[name], existing: extractMirror(toLf(files[name])) };
  });
  // Keep the recorded date only when both files already mirror this exact
  // body and revision on the same date.
  const dates = new Set(targets.map(({ existing }) => existing
    && existing.meta.bodySha256 === digest && existing.meta.sourceRevision === sourceRevision
    ? existing.meta.mirroredOn : null));
  const recorded = dates.size === 1 ? [...dates][0] : null;
  const date = mirroredOn ?? recorded ?? new Date().toISOString().slice(0, 10);
  // The block is rendered with LF and written back in the file's own line
  // endings, so a CRLF checkout stays CRLF and a repeated refresh is a no-op.
  const plan = targets.map(({ name, kind, current }) => {
    const lf = toLf(current);
    const rendered = applyMirrorBlock(lf, kind, renderMirrorBlock({ kind, body, sourceRevision, mirroredOn: date }));
    const next = lf === current ? rendered : rendered.replace(/\n/gu, '\r\n');
    return { name, kind, current, next, changed: next !== current };
  });
  return { body, digest, mirroredOn: date, plan };
}
