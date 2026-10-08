import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Privacy and drift guard for the shared instruction mirror. A mirror is
// sanitized only when it names no absolute path outside the repository, user
// name or home directory, machine name or host inventory, network address,
// SSH target, container host, token or credential. The guard also catches a
// mirror that was edited by hand, refreshed in only one file, or left stale.
// Every sample below is synthetic; documentation address ranges and example
// domains are used wherever a sample would otherwise look real.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const scriptsDir = path.join(repoDir, 'scripts', 'instructions');
const refreshScript = path.join(scriptsDir, 'refresh-instruction-mirror.mjs');
const checkScript = path.join(scriptsDir, 'check-instruction-mirror.mjs');
const libraryUrl = new URL('../../scripts/instructions/instruction-mirror.mjs', import.meta.url);

const REVISION = 'fedcba9876543210fedcba9876543210fedcba98';
const SOURCE = `# Example shared agent instructions

Example defaults for the guard tests.

## Secrets and sensitive input

- Do not ask the user to paste secrets into chat.
- Use HTTPS for any non-loopback connection.
`;

const fake = (prefix, length) => prefix + 'A1b2C3d4E5'.repeat(8).slice(0, length);
// Private-range samples are assembled at run time so this public file carries
// no literal local-network address.
const dotted = (...parts) => parts.join('.');

const LEAKS = [
  ['absolute path outside the repository', 'Clone into C:\\Users\\someone\\source first.'],
  ['absolute path outside the repository', 'Use D:/work/tree for scratch.'],
  ['absolute path outside the repository', 'The share is \\\\fileserver\\builds today.'],
  ['absolute path outside the repository', 'Logs live in /home/someone/project/logs.'],
  ['absolute path outside the repository', 'Open /Users/someone/Library/Logs to look.'],
  ['absolute path outside the repository', 'Keys sit in /root/.ssh for now.'],
  ['absolute path outside the repository', 'Mount /mnt/data/cache read-only.'],
  ['absolute path outside the repository', 'Git Bash shows /c/Users/someone/repo.'],
  ['absolute path outside the repository', 'Open file:///opt/tools/readme.txt in a browser.'],
  ['machine name', 'Build on DESKTOP-AB12CD3 first.'],
  ['machine name', 'The printer answers at bench-printer.local today.'],
  ['machine name', 'Use nas.lan for archives.'],
  ['machine name', 'Reach ci-runner-2.internal over the tunnel.'],
  ['machine name', 'The router is gateway.home.arpa here.'],
  ['machine name', 'Its adapter is 3c:22:fb:0a:91:7e on the switch.'],
  ['host inventory', '    HostName build-box'],
  ['IP address', `The farm is at ${dotted(192, 168, 1, 20)}.`],
  ['IP address', `Ping ${dotted(10, 0, 0, 5)} before deploying.`],
  ['IP address', `The VPN hands out ${dotted(172, 20, 1, 1)} first.`],
  ['IP address', 'Carrier NAT gave 100.64.3.2 today.'],
  ['IP address', 'Resolve through 8.8.8.8 only.'],
  ['IP address', 'Link-local fe80::1ff:fe23:4567:890a answered.'],
  ['IP address', 'The host is 2001:db8:85a3:0:0:8a2e:370:7334 now.'],
  ['SSH target', 'Run ssh admin@build-host to look.'],
  ['SSH target', 'Fetch from ssh://deploy@example.com/repo.git nightly.'],
  ['SSH target', 'Copy to deploy@example.com:/srv/drop with scp.'],
  ['account address', 'Mail someone@example.org with the result.'],
  ['container host', 'Set DOCKER_HOST=unix:///run/remote.sock first.'],
  ['container host', 'Point the client at tcp://example.com:2376 instead.'],
  ['container host', 'Use docker -H example.com ps to list.'],
  ['token', `Use ${fake('ghp_', 36)} for the API.`],
  ['token', `Use ${fake('github_pat_', 40)} for the API.`],
  ['token', `Set ${fake('sk-ant-', 30)} in the shell.`],
  ['token', 'The key is AKIAABCDEFGHIJKLMNOP for now.'],
  ['token', `Post with ${fake('xoxb-', 24)} today.`],
  ['token', `The npm token ${fake('npm_', 36)} works.`],
  ['token', `Send eyJhbGciOiJIUzI1NiJ9.${fake('', 20)}.${fake('', 20)} upstream.`],
  ['token', 'Post to https://discord.com/api/webhooks/123456789/abcDEF-ghi_jkl now.'],
  ['private key', '-----BEGIN OPENSSH PRIVATE KEY-----'],
  ['credential', 'Log in with password=hunter2 once.'],
  ['credential', 'Configure api_key: 9f8e7d6c5b4a3210 there.'],
  ['credential', 'Clone https://someone:s3cret@example.com/repo.git now.'],
  ['credential', `Send Authorization: Bearer ${fake('', 24)} with it.`],
];

const CLEAN = `## Ordinary rules that must not be flagged

- Use HTTPS for any non-loopback connection; never offer only localhost, 127.0.0.1 or ::1.
- Commit as \`Claude Fable 5.1 <noreply@anthropic.com>\` and push with \`git push\`.
- Clone with \`git@github.com:Ding-Ding-Projects/BambuStudio.git\` or
  https://github.com/Ding-Ding-Projects/BambuStudio/releases/latest/download/Setup.exe.
- Post the start time as \`2026-07-27T04:18:33-04:00\` and quote version v2.8.4.57 or version 2.8.4.61.
- Read \`scripts/instructions/refresh-instruction-mirror.mjs\` and docs/features/README.md.
- Documentation addresses such as 192.0.2.10, 198.51.100.7 and 203.0.113.9 are reserved examples.
- The \`ORG_TOKEN\` rules decide which secret a workflow reads; a random single-use access token expires.
- Resolve the Documents folder through %USERPROFILE% or $HOME rather than hard-coding a user name.
- C++ names such as \`std::string\` and \`DocumentationBundle::Article\` are code, not addresses.
- A secret: never paste one. The password manager holds it. Run \`#!/usr/bin/env node\` scripts.
- Mount points like \`/usr/bin/env\` are system paths and say nothing about a person.
- A bare \`file://\` scheme name is not a path; upstream 2.8.4.57 and Bambu Studio 2.8.4.61 are versions.
`;

function fixtureRoot(t) {
  const root = mkdtempSync(path.join(tmpdir(), 'instruction-mirror-guard-'));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  writeFileSync(path.join(root, 'README.md'), '# Project\n\nIntro.\n\n# Report issue\n\nTracker.\n');
  writeFileSync(path.join(root, 'AGENTS.md'), '# Policy\n\nRules.\n');
  writeFileSync(path.join(root, 'shared.md'), SOURCE);
  return root;
}

function run(script, root, ...extra) {
  return spawnSync(process.execPath, [script, '--root', root, ...extra], {
    encoding: 'utf8', env: { ...process.env, INSTRUCTION_MIRROR_PRIVATE_TERMS: '' },
  });
}

const refresh = (root, ...extra) => run(refreshScript, root, '--source', path.join(root, 'shared.md'),
  '--source-revision', REVISION, '--date', '2026-10-08', ...extra);
const check = (root, ...extra) => run(checkScript, root, ...extra);
const read = (root, file) => readFileSync(path.join(root, file), 'utf8');

test('the privacy scan names the category and line of every private detail', async () => {
  const { scanPrivacy } = await import(libraryUrl);
  for (const [category, sample] of LEAKS) {
    const findings = scanPrivacy(`Intro line.\n\n${sample}\n`);
    assert.ok(findings.some((finding) => finding.category === category && finding.line === 3),
      `${category}: ${sample} -> ${JSON.stringify(findings)}`);
  }
});

test('the privacy scan leaves ordinary instruction text alone', async () => {
  const { scanPrivacy } = await import(libraryUrl);
  assert.deepEqual(scanPrivacy(CLEAN), []);
});

test('a private term list is honoured without echoing the terms', async (t) => {
  const { scanPrivacy, loadPrivateTerms } = await import(libraryUrl);
  const root = fixtureRoot(t);
  const textList = path.join(root, 'terms.txt');
  writeFileSync(textList, '# comment line\nZebra Mode\n\nquokka\n');
  const jsonList = path.join(root, 'terms.json');
  writeFileSync(jsonList, JSON.stringify({ schema: 1, entries: { 'ordinary words': 'Zebra Mode', other: ['quokka'] } }));
  for (const file of [textList, jsonList]) {
    const terms = loadPrivateTerms(file);
    assert.deepEqual([...terms].sort(), ['Zebra Mode', 'quokka']);
    const findings = scanPrivacy('Plain line.\nTurn on zebra mode now.\nA Quokka appeared.\nquokkas are plural.\n', { terms });
    assert.deepEqual(findings.map((f) => [f.line, f.category]), [[2, 'private term'], [3, 'private term']]);
    assert.ok(findings.every((f) => !JSON.stringify(f).toLowerCase().includes('zebra')), 'findings never echo a term');
    assert.deepEqual(scanPrivacy('ordinary words only\n', { terms }), [], 'JSON keys are not terms');
  }

  writeFileSync(path.join(root, 'shared.md'), `${SOURCE}\n- Never enable Zebra Mode.\n`);
  const refused = run(refreshScript, root, '--source', path.join(root, 'shared.md'), '--source-revision', REVISION,
    '--private-terms', textList);
  assert.notEqual(refused.status, 0);
  assert.doesNotMatch(refused.stdout + refused.stderr, /zebra/i, 'the refusal does not print the term');
  const missing = run(checkScript, root, '--private-terms', path.join(root, 'absent.txt'));
  assert.notEqual(missing.status, 0, 'an unreadable term list fails closed');
});

test('refresh refuses an export that still carries a private detail', async (t) => {
  const root = fixtureRoot(t);
  const before = [read(root, 'README.md'), read(root, 'AGENTS.md')];
  writeFileSync(path.join(root, 'shared.md'), `${SOURCE}\n- Deploy from /home/someone/build first.\n`);
  const refused = refresh(root);
  assert.notEqual(refused.status, 0);
  assert.match(refused.stderr, /line 10: absolute path outside the repository/);
  assert.doesNotMatch(refused.stderr, /someone/, 'the refusal does not repeat the private value');
  assert.deepEqual([read(root, 'README.md'), read(root, 'AGENTS.md')], before, 'nothing was written');
});

test('the check passes a fresh mirror and reports an absent one honestly', (t) => {
  const root = fixtureRoot(t);
  const absent = check(root);
  assert.equal(absent.status, 0, absent.stdout + absent.stderr);
  assert.match(absent.stdout, /ABSENT/);
  assert.equal(check(root, '--require').status, 1, '--require demands a published mirror');
  assert.equal(refresh(root).status, 0);
  const fresh = check(root, '--require', '--source', path.join(root, 'shared.md'));
  assert.equal(fresh.status, 0, fresh.stdout + fresh.stderr);
  assert.match(fresh.stdout, /PASS/);
});

test('the check catches hand edits, one-sided refreshes and stale mirrors', async (t) => {
  const { renderMirrorBlock, applyMirrorBlock } = await import(libraryUrl);
  const cases = {
    'edited body': (root) => writeFileSync(path.join(root, 'README.md'),
      read(root, 'README.md').replace('Do not ask the user', 'Feel free to ask the user')),
    'edited label': (root) => writeFileSync(path.join(root, 'AGENTS.md'),
      read(root, 'AGENTS.md').replace('Do not edit it here', 'Edit it freely here')),
    'one file only': (root) => writeFileSync(path.join(root, 'AGENTS.md'), '# Policy\n\nRules.\n'),
    'different copies': (root) => {
      const body = 'Example defaults for the guard tests.\n\n## Other\n\n- A different rule.\n';
      const block = renderMirrorBlock({ kind: 'agents', body, sourceRevision: REVISION, mirroredOn: '2026-10-08' });
      writeFileSync(path.join(root, 'AGENTS.md'), applyMirrorBlock(read(root, 'AGENTS.md'), 'agents', block));
    },
    'private detail rendered around the refresh': (root) => {
      for (const [file, kind] of [['README.md', 'readme'], ['AGENTS.md', 'agents']]) {
        const body = `Example defaults.\n\n## Hosts\n\n- Deploy to ${dotted(192, 168, 1, 20)} first.\n`;
        const block = renderMirrorBlock({ kind, body, sourceRevision: REVISION, mirroredOn: '2026-10-08' });
        writeFileSync(path.join(root, file), applyMirrorBlock(read(root, file), kind, block));
      }
    },
    'duplicated block': (root) => writeFileSync(path.join(root, 'README.md'),
      `${read(root, 'README.md')}\n<!-- shared-instructions-mirror:begin -->\n`),
  };
  for (const [name, damage] of Object.entries(cases)) {
    const root = fixtureRoot(t);
    assert.equal(refresh(root).status, 0);
    damage(root);
    const result = check(root);
    assert.equal(result.status, 1, `${name}: ${result.stdout}${result.stderr}`);
    assert.match(result.stdout, /FAIL/, name);
  }

  const root = fixtureRoot(t);
  assert.equal(refresh(root).status, 0);
  const newer = path.join(root, 'newer.md');
  writeFileSync(newer, `${SOURCE}\n## New rule\n\n- Something changed upstream.\n`);
  const stale = check(root, '--source', newer);
  assert.equal(stale.status, 1);
  assert.match(stale.stdout, /stale/i);
});

test('the check reads a CRLF checkout the same way as an LF one', (t) => {
  const root = fixtureRoot(t);
  assert.equal(refresh(root).status, 0);
  for (const file of ['README.md', 'AGENTS.md']) {
    writeFileSync(path.join(root, file), read(root, file).replace(/\n/g, '\r\n'));
  }
  const result = check(root, '--require', '--source', path.join(root, 'shared.md'));
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /PASS/);
});

test('this repository passes the mirror guard', () => {
  const result = spawnSync(process.execPath, [checkScript], {
    encoding: 'utf8', cwd: repoDir, env: { ...process.env, INSTRUCTION_MIRROR_PRIVATE_TERMS: '' },
  });
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /^(PASS|ABSENT)\b/m);
});
