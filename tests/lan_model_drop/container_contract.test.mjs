// Source contracts for how the LAN model drop container is built, run and
// shipped: the pinned base image, the non-root user and health check, the
// locked-down compose service, the settings template, and the Windows install
// of the folder beside the application.
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import { dropRoot, repoRoot, tempDir } from './harness.mjs';

const read = (name) => fs.readFileSync(path.join(dropRoot, name), 'utf8');
const BASE_IMAGE = 'node:22.23.3-alpine3.24@sha256:0a7108bf6c7bf5de370ffb1a3ed6be93d405b43ff159f681a8d18c0e2bc2e402';

// Dockerfile instructions with comments and line continuations folded away.
function instructions(text) {
  return text.replace(/[ \t]*\\\r?\n\s*/g, ' ').split(/\r?\n/).map((line) => line.trim()).filter((line) => line && !line.startsWith('#'));
}

test('the Dockerfile pins its base image by tag and digest, runs as non-root and checks its health', () => {
  const lines = instructions(read('Dockerfile'));
  const from = lines.filter((line) => /^FROM\b/i.test(line));
  assert.deepEqual(from, [`FROM ${BASE_IMAGE}`]);
  assert.doesNotMatch(read('Dockerfile'), /^# syntax=/m, 'no front-end image is fetched to read the Dockerfile');
  const users = lines.filter((line) => /^USER\b/i.test(line));
  assert.deepEqual(users, ['USER node'], 'one USER, the image\'s unprivileged node user');
  assert.ok(lines.indexOf('USER node') > lines.findLastIndex((line) => /^(RUN|COPY)\b/.test(line)), 'nothing runs as root after USER');
  const health = lines.filter((line) => /^HEALTHCHECK\b/i.test(line));
  assert.equal(health.length, 1);
  assert.match(health[0], /^HEALTHCHECK --interval=\d+s --timeout=\d+s --start-period=\d+s --retries=\d+ CMD \["node", "server\/healthcheck\.mjs"\]$/);
  assert.ok(lines.includes('CMD ["node", "server/main.mjs"]'));
  assert.ok(lines.includes('EXPOSE 8080'));
  assert.ok(lines.includes('VOLUME ["/data"]'));
  // Node.js built-ins only: nothing is installed while building.
  assert.doesNotMatch(read('Dockerfile'), /\b(npm|yarn|pnpm|apk add|curl|wget|ADD)\b/);
  assert.ok(!fs.existsSync(path.join(dropRoot, 'package.json')), 'no package manifest, no dependencies');
  const copies = lines.filter((line) => /^COPY\b/.test(line));
  assert.deepEqual(copies.map((line) => line.replace(/--\S+ /g, '')), ['COPY server/ ./server/', 'COPY site/ ./site/']);
  for (const copy of copies) assert.match(copy, /--chown=root:root/, 'the program is owned by root, not the service user');
  // Every module the service imports is a Node built-in or its own file.
  for (const name of fs.readdirSync(path.join(dropRoot, 'server'))) {
    const text = read(path.join('server', name));
    for (const match of text.matchAll(/^import [^;]*? from '([^']+)';/gm)) {
      assert.match(match[1], /^(node:|\.\/)/, `${name} imports ${match[1]}`);
    }
  }
  // Only the program reaches the build context.
  assert.deepEqual(read('.dockerignore').split('\n').filter((line) => line && !line.startsWith('#')), ['*', '!server/', '!site/']);
});

// A small reader for the compose file's own shape: enough to find keys under
// the one service without a YAML library.
function serviceBlock(text, name) {
  const match = new RegExp(`\\n  ${name}:\\n([\\s\\S]*?)(?=\\n\\S|$)`).exec(text);
  return match ? match[1] : '';
}

test('compose runs the service locked down, limited and with one data volume', () => {
  const compose = read('compose.yaml');
  assert.match(compose, /^name: lan-model-drop$/m);
  const service = serviceBlock(compose, 'lan-model-drop');
  assert.ok(service, 'the lan-model-drop service exists');
  assert.match(service, /^ {4}read_only: true$/m);
  assert.match(service, /^ {4}cap_drop:\n {6}- ALL$/m);
  assert.match(service, /^ {4}security_opt:\n {6}- no-new-privileges:true$/m);
  assert.match(service, /^ {4}user: node$/m);
  assert.match(service, /^ {4}mem_limit: \d+m$/m);
  assert.match(service, /^ {4}memswap_limit: \d+m$/m);
  assert.match(service, /^ {4}cpus: [0-9.]+$/m);
  assert.match(service, /^ {4}pids_limit: \d+$/m);
  assert.match(service, /^ {4}restart: unless-stopped$/m);
  assert.match(service, /^ {4}tmpfs:\n {6}- \/tmp:size=\d+m,mode=1777,noexec,nosuid,nodev$/m);
  assert.match(service, /^ {4}volumes:\n {6}- drop-data:\/data$/m);
  assert.match(compose, /\nvolumes:\n {2}drop-data:\n?$/);
  assert.match(service, /^ {4}ports:\n {6}- "\$\{DROP_PORT:-8833\}:8080"$/m);
  assert.match(service, /^ {4}build:\n {6}context: \.$/m);
  assert.match(service, /^ {4}image: localhost\//m, 'the image name can never be pulled from a public registry');
  assert.match(service, /^ {4}logging:\n {6}driver: json-file\n {6}options:\n {8}max-size: \w+\n {8}max-file: "\d+"$/m);
  assert.doesNotMatch(service, /privileged|network_mode: host|cap_add|docker\.sock|pid: host/);
});

test('every setting the service reads is passed by compose and explained in .env.example', () => {
  const config = fs.readFileSync(path.join(dropRoot, 'server', 'config.mjs'), 'utf8');
  const read_ = new Set([...config.matchAll(/\b(DROP_[A-Z_]+)\b/g)].map((m) => m[1]));
  read_.delete('DROP_DATA_DIR'); // fixed to /data by the image
  const compose = serviceBlock(read('compose.yaml'), 'lan-model-drop');
  const example = read('.env.example');
  const passed = new Set([...compose.matchAll(/^ {6}(DROP_[A-Z_]+): \$\{\1:-[^}]*\}$/gm)].map((m) => m[1]));
  const listed = new Set([...example.matchAll(/^(DROP_[A-Z_]+)=/gm)].map((m) => m[1]));
  for (const name of read_) {
    assert.ok(passed.has(name), `compose passes ${name}`);
    assert.ok(listed.has(name), `.env.example lists ${name}`);
  }
  assert.ok(listed.has('DROP_PORT'));
  // The template never carries a secret, and a real .env is never committed.
  assert.match(example, /^DROP_STATION_KEY=$/m);
  assert.match(example, /^DROP_CODE=$/m);
  assert.match(example, /^DROP_PUBLIC_URL=$/m);
  assert.match(read('.gitignore'), /^\.env$/m);
  assert.ok(!fs.existsSync(path.join(dropRoot, '.env')) || spawnSync('git', ['-C', repoRoot, 'ls-files', '--error-unmatch', 'lan-model-drop/.env']).status !== 0,
    'no .env is tracked');
  // The defaults in compose match the service's own.
  assert.match(compose, /DROP_MAX_BYTES: \$\{DROP_MAX_BYTES:-268435456\}/);
  assert.match(compose, /DROP_TTL_HOURS: \$\{DROP_TTL_HOURS:-24\}/);
  assert.match(compose, /DROP_QUEUE_MAX_FILES: \$\{DROP_QUEUE_MAX_FILES:-50\}/);
  assert.match(compose, /DROP_QUEUE_MAX_BYTES: \$\{DROP_QUEUE_MAX_BYTES:-2147483648\}/);
  assert.match(compose, /DROP_STATION_NAME: \$\{DROP_STATION_NAME:-Bambu Studio\}/);
});

test('the service reads the station key with the command its log and documents name', () => {
  const command = 'docker compose exec lan-model-drop node server/station-key.mjs';
  for (const name of ['README.md', 'README.yue_HK.md', '.env.example', 'compose.yaml', path.join('server', 'app.mjs')]) {
    assert.ok(read(name).includes(command.replace('docker compose exec lan-model-drop ', '')), `${name} names the key command`);
  }
  const docs = path.join(repoRoot, 'docs', 'features', 'application-integration');
  for (const name of ['lan-model-drop-site.md', 'lan-model-drop-site.yue_HK.md']) {
    assert.ok(fs.readFileSync(path.join(docs, name), 'utf8').includes(command), `${name} names the key command`);
  }
});

function windowsInstallBlock() {
  const cmake = fs.readFileSync(path.join(repoRoot, 'CMakeLists.txt'), 'utf8');
  return /if \(WIN32\)\n([\s\S]*?)\nelseif \(SLIC3R_FHS\)/.exec(cmake)?.[1] ?? '';
}

test('the Windows install puts the container folder beside the application, without secrets', () => {
  const block = windowsInstallBlock();
  const install = /install\(DIRECTORY "\$\{CMAKE_CURRENT_SOURCE_DIR\}\/lan-model-drop\/" DESTINATION "\$\{CMAKE_INSTALL_PREFIX\}\/lan-model-drop"([\s\S]*?)\)(?:\n|$)/.exec(block);
  assert.ok(install, 'lan-model-drop is installed on Windows');
  for (const pattern of ['.env', '.gitignore', 'node_modules', '*.test.mjs']) {
    assert.ok(install[1].includes(`PATTERN "${pattern}" EXCLUDE`), `${pattern} is excluded`);
  }
  // The browser extension keeps its own line.
  assert.match(block, /install\(DIRECTORY "\$\{CMAKE_CURRENT_SOURCE_DIR\}\/browser-extension\/" DESTINATION "\$\{CMAKE_INSTALL_PREFIX\}\/browser-extension"\)/);
});

const cmakeAvailable = spawnSync('cmake', ['--version']).status === 0;

test('installing with that rule ships the container files and leaves a local .env behind', { skip: cmakeAvailable ? false : 'cmake not found' }, (t) => {
  const work = tempDir(t, 'lan-drop-install-');
  const source = path.join(work, 'source');
  fs.cpSync(dropRoot, path.join(source, 'lan-model-drop'), { recursive: true });
  fs.writeFileSync(path.join(source, 'lan-model-drop', '.env'), 'DROP_STATION_KEY=never-shipped-0123456789\n');
  fs.mkdirSync(path.join(source, 'lan-model-drop', 'node_modules', 'x'), { recursive: true });
  fs.writeFileSync(path.join(source, 'lan-model-drop', 'node_modules', 'x', 'index.js'), '');
  fs.writeFileSync(path.join(source, 'lan-model-drop', 'server', 'scratch.test.mjs'), '');
  const install = /(install\(DIRECTORY "\$\{CMAKE_CURRENT_SOURCE_DIR\}\/lan-model-drop\/"[\s\S]*?\)(?:\n|$))/.exec(windowsInstallBlock())[1];
  const project = path.join(work, 'project');
  fs.mkdirSync(project);
  const cmakePath = (value) => value.replace(/\\/g, '/');
  fs.writeFileSync(path.join(project, 'CMakeLists.txt'), [
    'cmake_minimum_required(VERSION 3.13)',
    'project(lan_drop_install NONE)',
    install.replace('${CMAKE_CURRENT_SOURCE_DIR}', cmakePath(source)),
  ].join('\n'));
  const prefix = path.join(work, 'prefix');
  const build = path.join(work, 'build');
  const configure = spawnSync('cmake', ['-S', project, '-B', build, `-DCMAKE_INSTALL_PREFIX=${cmakePath(prefix)}`], { encoding: 'utf8' });
  assert.equal(configure.status, 0, configure.stderr);
  const installed = spawnSync('cmake', ['--install', build], { encoding: 'utf8' });
  assert.equal(installed.status, 0, installed.stderr);

  const list = (dir, prefixPath = '') => fs.readdirSync(dir, { withFileTypes: true }).flatMap((entry) => {
    const relative = prefixPath ? `${prefixPath}/${entry.name}` : entry.name;
    return entry.isDirectory() ? list(path.join(dir, entry.name), relative) : [relative];
  }).sort();
  const shipped = list(path.join(prefix, 'lan-model-drop'));
  for (const name of ['Dockerfile', 'compose.yaml', '.env.example', '.dockerignore', 'README.md', 'README.yue_HK.md',
    'server/main.mjs', 'server/station-key.mjs', 'server/healthcheck.mjs', 'site/index.html', 'site/app.js',
    'site/fonts/MaterialSymbolsOutlined-subset.woff2']) {
    assert.ok(shipped.includes(name), `${name} is installed`);
  }
  for (const name of ['.env', '.gitignore', 'node_modules/x/index.js', 'server/scratch.test.mjs']) {
    assert.ok(!shipped.includes(name), `${name} is not installed`);
  }
  assert.deepEqual(shipped, list(dropRoot).filter((name) => name !== '.gitignore' && name !== '.env' && !name.startsWith('node_modules/')));
});
