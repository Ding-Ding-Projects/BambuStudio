// What a real LAN model drop container answered (tests/lan_model_drop/container_fixtures, captured by
// container_capture.py from lan-model-drop/ running under docker compose) against both halves:
//
// - the station: lan_model_drop_container_interop_test.cpp feeds every captured status, inbox,
//   file download (raw header lines and bytes), error, new code and delete answer through the
//   application's own pure model and checks every field and verdict;
// - the sender page: its own outcome rules (site/outcome.js) read every captured answer to a drop,
//   and its invite reader (site/invite.js) reads the invite links the station builds;
// - the request format: the requests curl sent carry exactly the protocol headers the real page
//   sent from headless Chromium for the same models.
import { test } from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdirSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const fixtures = path.join(repo, 'tests', 'lan_model_drop', 'container_fixtures');
const manifest = JSON.parse(readFileSync(path.join(fixtures, 'manifest.json'), 'utf8'));
const { outcomeFor } = await import(path.join(repo, 'lan-model-drop', 'site', 'outcome.js'));
const { codeFromFragment, takeInviteCode } = await import(path.join(repo, 'lan-model-drop', 'site', 'invite.js'));
const hasGxx = spawnSync('g++', ['--version'], { encoding: 'utf8' }).status === 0;

const scenario = (name) => manifest.scenarios.find((s) => s.name === name);
const fixture = (relative) => readFileSync(path.join(fixtures, relative));

// A header of raw response lines, as the station reads it.
function header(head, name) {
  let value = null;
  for (const line of head.split('\r\n').slice(1)) {
    const colon = line.indexOf(':');
    if (colon > 0 && line.slice(0, colon).toLowerCase() === name.toLowerCase()) value = line.slice(colon + 1).trim();
  }
  return value;
}

// The protocol headers of one drop request, by lower-case name.
function protocolHeaders(entries) {
  const out = {};
  for (const [name, value] of entries) out[name.toLowerCase()] = value;
  return out;
}

test('the station reads every captured answer as the drop box meant it', { skip: hasGxx ? false : 'g++ is not installed', timeout: 300000 }, () => {
  const dir = mkdtempSync(path.join(tmpdir(), 'lan-model-drop-interop-'));
  try {
    const exe = path.join(dir, process.platform === 'win32' ? 'interop.exe' : 'interop');
    const build = spawnSync('g++', [
      '-std=c++17', '-Wall', '-Wextra', '-Werror', '-Isrc',
      'tests/lan_model_drop/lan_model_drop_container_interop_test.cpp',
      'src/slic3r/GUI/LanModelDrop/LanModelDropModel.cpp',
      'src/slic3r/GUI/LanModelDrop/LanModelDropFiles.cpp',
      '-o', exe,
    ], { cwd: repo, encoding: 'utf8' });
    assert.equal(build.status, 0, build.stderr);
    const scratch = path.join(dir, 'received');
    mkdirSync(scratch);
    const summaryFile = path.join(dir, 'summary.json');
    const run = spawnSync(exe, [fixtures, scratch, summaryFile], { cwd: repo, encoding: 'utf8' });
    assert.equal(run.status, 0, run.stderr || run.stdout);
    assert.match(run.stdout, /lan_model_drop_container_interop_test: \d+ assertions passed/);

    // The sender page reads the station's invite links back to the same drop code, and takes the
    // code out of the address bar.
    const { links } = JSON.parse(readFileSync(summaryFile, 'utf8'));
    assert.deepEqual(links.map((l) => l.scenario).sort(), ['custom', 'default']);
    for (const { link, code } of links) {
      const url = new URL(link);
      assert.equal(url.search, '', 'the code never travels in the query');
      assert.deepEqual(codeFromFragment(url.hash), { present: true, code });
      const replaced = [];
      const win = { location: { hash: url.hash, pathname: url.pathname, search: url.search }, history: { state: null, replaceState: (...args) => replaced.push(args) } };
      assert.deepEqual(takeInviteCode(win), { present: true, code });
      assert.deepEqual(replaced, [[null, '', url.pathname]]);
    }
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('the sender page understands every answer the container gave to a drop', () => {
  let checked = 0;
  for (const s of manifest.scenarios) {
    const maxBytes = Number(s.env.DROP_MAX_BYTES ?? 268435456);
    for (const drop of s.drops) {
      const head = fixture(drop.head).toString('latin1');
      const text = fixture(drop.body).toString('utf8');
      const answer = { status: drop.status, body: JSON.parse(text), retryAfter: header(head, 'Retry-After') };
      const outcome = outcomeFor(answer, maxBytes);
      const { expect } = drop;
      assert.equal(drop.status, expect.status, `${s.name} ${drop.label}`);
      if (drop.status === 201) {
        assert.deepEqual(outcome, { sent: true }, `${s.name} ${drop.label}`);
      } else {
        assert.equal(outcome.sent, false);
        assert.equal(outcome.reason.key, expect.siteReason, `${s.name} ${drop.label}: ${text}`);
        assert.equal(Boolean(outcome.stop), Boolean(expect.siteStop), `${s.name} ${drop.label}: stop`);
        if (expect.siteLockMinutes) assert.deepEqual(outcome.live, { key: 'liveLocked', values: [expect.siteLockMinutes] });
        if (expect.siteReason === 'reasonTooLarge') assert.equal(outcome.reason.size, maxBytes);
      }
      ++checked;
    }
    for (const drop of s.page?.drops ?? []) {
      assert.deepEqual(outcomeFor({ status: drop.status, body: JSON.parse(drop.responseBody) }, maxBytes), { sent: true }, drop.name);
      ++checked;
    }
  }
  assert.ok(checked >= 30, `${checked} answers`);
  // Every kind of answer the page distinguishes was captured.
  const reasons = new Set(manifest.scenarios.flatMap((s) => s.drops.map((d) => d.expect.siteReason).filter(Boolean)));
  for (const reason of ['reasonWrongCode', 'reasonType', 'reasonName', 'reasonLocked', 'reasonTooLarge', 'reasonFull', 'reasonServer']) {
    assert.ok(reasons.has(reason), reason);
  }
});

test('curl sent each model with exactly the protocol headers the real sender page sent', () => {
  const s = scenario('default');
  assert.ok(s.page, 'the page run was captured');
  assert.equal(s.page.drops.length, manifest.samples.length);
  // The page sends the sender as typed, with white space runs made one space and trimmed.
  const pageSender = s.page.sender.replace(/\s+/g, ' ').trim();
  assert.equal(pageSender, 'Mo Li');
  for (const sample of manifest.samples) {
    const page = s.page.drops.find((d) => d.name === sample.name);
    const curl = s.drops.find((d) => d.name === sample.name && d.expect.sample === sample.file && d.status === 201);
    assert.ok(page && curl, sample.name);
    const fromPage = protocolHeaders(Object.entries(page.requestHeaders));
    const fromCurl = protocolHeaders(curl.requestLines.slice(1).map((line) => {
      const colon = line.indexOf(':');
      return [line.slice(0, colon), line.slice(colon + 1).trim()];
    }));
    assert.equal(curl.requestLines[0], 'POST /api/drop HTTP/1.1');
    for (const headers of [fromPage, fromCurl]) {
      for (const absent of ['expect', 'transfer-encoding', 'authorization', 'cookie', 'referer']) assert.equal(headers[absent], undefined, `${sample.name}: ${absent}`);
      assert.equal(headers['content-length'], String(sample.bytes), `${sample.name}: Content-Length`);
      assert.equal(headers['x-drop-filename'], encodeURIComponent(sample.name), `${sample.name}: X-Drop-Filename`);
      assert.equal(headers['x-drop-code'], s.dropCode, `${sample.name}: X-Drop-Code`);
    }
    for (const name of ['content-type', 'origin', 'accept', 'host']) assert.equal(fromCurl[name], fromPage[name], `${sample.name}: ${name}`);
    assert.equal(fromPage['x-drop-sender'], encodeURIComponent(pageSender));
    assert.equal(fromCurl['x-drop-sender'], curl.sender ? encodeURIComponent(curl.sender) : undefined, `${sample.name}: X-Drop-Sender`);
    // Nothing else of the protocol: the same header names apart from the sender.
    const names = (headers) => Object.keys(headers).filter((n) => n.startsWith('x-') || n.startsWith('content-')).filter((n) => n !== 'x-drop-sender').sort();
    assert.deepEqual(names(fromCurl), names(fromPage), sample.name);
  }
});

test('the fixtures hold the samples that were sent, every security header, and never a key', () => {
  for (const sample of manifest.samples) {
    const bytes = fixture(sample.file);
    assert.equal(bytes.length, sample.bytes, sample.file);
    assert.equal(createHash('sha256').update(bytes).digest('hex'), sample.sha256, sample.file);
  }
  const types = new Set(manifest.samples.map((sample) => sample.type));
  assert.deepEqual([...types].sort(), ['3mf', 'amf', 'obj', 'step', 'stl']);
  let heads = 0;
  for (const s of manifest.scenarios) {
    const records = Object.values(s).flatMap((value) => (Array.isArray(value) ? value : [value]))
      .filter((value) => value && typeof value === 'object' && typeof value.head === 'string');
    for (const record of records) {
      const head = fixture(record.head).toString('latin1');
      assert.match(head, /^HTTP\/1\.1 \d{3} /);
      assert.match(header(head, 'Content-Security-Policy') ?? '', /^default-src 'self';.*frame-ancestors 'none'/);
      assert.equal(header(head, 'X-Content-Type-Options'), 'nosniff');
      assert.equal(header(head, 'Referrer-Policy'), 'no-referrer');
      for (const line of record.requestLines ?? []) assert.doesNotMatch(line, /^authorization:/i);
      ++heads;
    }
  }
  assert.ok(heads > 60, `${heads} answers`);
  assert.deepEqual(manifest.refusedSettings.map((r) => r.exitCode), manifest.refusedSettings.map(() => 2));
  assert.equal(manifest.acceptedLimits.exitCode, 0);
});
