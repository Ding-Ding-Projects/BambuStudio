/*
 * Contract tests for the Pages site's data and logic layers.
 *
 * These run without a browser: the modules are loaded into a Node global that
 * carries just enough of a window for their top level, and only the pure parts
 * (copy resolution, regex evaluation, date parsing, changelog data) are called.
 * Anything that needs layout is covered by runtime-layout-clipping.mjs instead.
 */
import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

const testDir = path.dirname(fileURLToPath(import.meta.url));
const uiDir = path.resolve(testDir, '..');
const siteDir = path.join(uiDir, 'site');

// No DOM, no storage, and no Worker: the inline evaluation path is the one
// exercised here, which is also the path a hardened browser falls back to.
globalThis.window = globalThis;
globalThis.Worker = undefined;
globalThis.location = { search: '', href: 'https://example.invalid/', hostname: 'example.invalid' };

await import('../site/copy.js');
await import('../site/changelog.data.js');
await import('../site/dimsum.data.js');
await import('../site/core.js');
await import('../site/regex.js');
await import('../site/changelog.js');

const copy = globalThis.BAMBU_SITE_COPY;
const site = globalThis.BambuSite;
const regex = globalThis.BambuRegex;
const changelog = globalThis.BAMBU_CHANGELOG;
const changelogView = globalThis.BambuChangelog;

const placeholders = (value) => (String(value).match(/\{[A-Za-z0-9_]+\}/g) || []).sort().join(',');

/* ------------------------------------------------------------------ copy */

test('every copy entry carries both languages with a usable ladder length', () => {
  const keys = Object.keys(copy.entries);
  assert.ok(keys.length > 100, `expected a substantial catalog, found ${keys.length}`);
  for (const key of keys) {
    const entry = copy.entries[key];
    for (const language of ['en', 'yue']) {
      assert.ok(Array.isArray(entry[language]), `${key}.${language} must be an array`);
      assert.ok(entry[language].length >= 1 && entry[language].length <= 5,
        `${key}.${language} has ${entry[language].length} variants; 1-5 are supported`);
      for (const variant of entry[language]) {
        assert.equal(typeof variant, 'string');
        assert.ok(variant.trim().length > 0, `${key}.${language} has an empty variant`);
      }
    }
  }
});

test('a ladder never changes the placeholders a message interpolates', () => {
  for (const [key, entry] of Object.entries(copy.entries)) {
    const expected = placeholders(entry.en[0]);
    for (const language of ['en', 'yue']) {
      for (const [index, variant] of entry[language].entries()) {
        assert.equal(placeholders(variant), expected,
          `${key}.${language}[${index}] changes the placeholder set`);
      }
    }
  }
});

test('funny levels select a variant in range for every supported ladder length', () => {
  for (let count = 1; count <= 5; count++) {
    for (let level = 1; level <= 5; level++) {
      const index = site.variantIndex(count, level);
      assert.ok(Number.isInteger(index) && index >= 0 && index < count,
        `variantIndex(${count}, ${level}) returned ${index}`);
    }
    // Level 1 is always the most serious variant and level 5 the most playful.
    assert.equal(site.variantIndex(count, 1), 0);
    assert.equal(site.variantIndex(count, 5), count - 1);
  }
  // Out-of-range levels are clamped rather than producing undefined text.
  assert.equal(site.variantIndex(5, 0), 0);
  assert.equal(site.variantIndex(5, 99), 4);
});

test('every message-shaped entry actually varies with the funny level', () => {
  /*
   * The site tells the user the tone setting applies to every message. A
   * single-variant entry silently opts out of that promise — it reads
   * identically at level 1 and level 5 — so anything message-shaped must carry
   * a real ladder. Atomic control labels ("Copy", "Font") legitimately do not,
   * which is why this matches by shape rather than sweeping the whole catalog.
   */
  const MESSAGE_SHAPED = /^notify\.[a-z]|\.desc$|\.body$|\.hint$|empty$|baseline$|derivation$|dateformat$|escapenote$|\.line$/;
  const ALLOWED_FLAT = new Set([
    'notify.tone.action', // a button label, not a message
    'footer.disclaimer' // legal text: identical wording at every level is the point
  ]);
  const flat = Object.keys(copy.entries)
    .filter((key) => MESSAGE_SHAPED.test(key))
    .filter((key) => !ALLOWED_FLAT.has(key))
    .filter((key) => copy.entries[key].en.length < 2 || copy.entries[key].yue.length < 2);
  assert.deepEqual(flat, [], 'message-shaped copy must carry a tone ladder in both languages');
});

test('warnings keep every fact at every funny level, in both languages', () => {
  const contracts = [
    ['hero.integrity', 'en', [/unsigned/i, /SHA-256/, /per user/i]],
    ['hero.integrity', 'yue', [/未簽名/, /SHA-256/]],
    ['settings.reset.desc', 'en', [/undo/i, /tab order/i, /funny level/i]],
    ['settings.reset.desc', 'yue', [/(無法復原|冇得返轉頭|冇 undo)/, /分頁次序/, /搞笑/]],
    ['settings.storage.blocked', 'en', [/(local storage|storage)/i, /reload/i]],
    ['settings.storage.blocked', 'yue', [/儲存/, /(重新載入|reload)/]],
    ['regex.timeout', 'en', [/\{ms\}/, /(unchanged|untouched)/i]],
    ['regex.timeout', 'yue', [/\{ms\}/, /(原狀|原封不動)/]],
    ['regex.invalid', 'en', [/\{message\}/]],
    ['regex.invalid', 'yue', [/\{message\}/]],
    ['settings.funny.desc', 'en', [/error/i, /warning/i, /never change/i]],
    ['settings.funny.desc', 'yue', [/錯誤/, /警告/]],
    ['changelog.dateinvalid', 'en', [/YYYY-MM-DD/]],
    ['changelog.dateinvalid', 'yue', [/YYYY-MM-DD/]]
  ];
  for (const [key, language, patterns] of contracts) {
    const variants = copy.entries[key][language];
    for (const [index, variant] of variants.entries()) {
      for (const pattern of patterns) {
        assert.match(variant, pattern, `${key}.${language}[${index}] dropped a required fact`);
      }
    }
  }
});

test('every copy key referenced by the site exists in the catalog', async () => {
  const sources = [
    path.join(uiDir, 'landing.html'),
    ...(await readdir(siteDir))
      .filter((name) => name.endsWith('.js'))
      .map((name) => path.join(siteDir, name))
  ];
  const missing = [];
  for (const source of sources) {
    const text = await readFile(source, 'utf8');
    const referenced = new Set();
    for (const match of text.matchAll(/data-copy="([^"{]+)"/g)) referenced.add(match[1]);
    for (const match of text.matchAll(/data-copy-attr="([^"]+)"/g)) {
      for (const rule of match[1].split(';')) {
        const key = rule.split(':')[1];
        if (key && !key.includes('{')) referenced.add(key.trim());
      }
    }
    for (const match of text.matchAll(/site\.(?:text|pair|preview)\(\s*'([a-z][A-Za-z0-9._]+)'/g)) {
      referenced.add(match[1]);
    }
    for (const match of text.matchAll(/notify\(\s*'[a-z]+',\s*'([A-Za-z0-9._]+)'/g)) referenced.add(match[1]);
    for (const key of referenced) {
      // Keys assembled at runtime (`'build.step' + index`) are covered by the
      // per-family test below; only literal keys can be checked here.
      if (!/^[a-z][A-Za-z0-9]*(\.[A-Za-z0-9]+)+$/.test(key)) continue;
      if (!site.known(key)) missing.push(`${path.basename(source)} -> ${key}`);
    }
  }
  assert.deepEqual(missing, [], 'copy keys referenced but not defined');
});

test('every copy key the site assembles at runtime exists too', () => {
  const families = [
    ...['added', 'fixed', 'changed', 'removed', 'documented'].map((c) => `changelog.cat.${c}`),
    ...[1, 2, 3, 4].flatMap((n) => [`build.step${n}.title`, `build.step${n}.body`]),
    ...['group.product', 'group.tools', 'group.project'],
    ...['home', 'prepare', 'preview', 'device', 'multidevice', 'project', 'calibration',
      'filament', 'settings', 'smarthome'].flatMap((s) => [`screen.${s}`, `screen.${s}.body`]),
    ...['tab.overview', 'tab.screens', 'tab.materialyou', 'tab.download', 'tab.changelog',
      'tab.regex', 'tab.settings', 'tab.build'],
    ...['notify.saved', 'notify.reset.done', 'notify.copied', 'notify.copyfailed',
      'notify.exported', 'notify.tab.pinned', 'notify.tab.unpinned', 'notify.tab.reset',
      'notify.dimsum.off'],
    ...['changelog.baseline', 'changelog.nochanges', 'changelog.samecommit',
      'settings.search.empty', 'settings.search.elsewhere'],
    ...['regex.unsafe', 'regex.enginefailed', 'regex.unsupported', 'regex.invalid',
      'regex.timeout', 'regex.nomatch', 'regex.needsunicode', 'shell.notifications.dismiss']
  ];
  const missing = families.filter((key) => !site.known(key));
  assert.deepEqual(missing, [], 'runtime-assembled copy keys missing from the catalog');
});

test('language modes render primary and companion text as documented', () => {
  site.set('funnyEn', 1);
  site.set('funnyYue', 1);
  const both = site.pair('tab.settings');
  assert.equal(both.en, 'Settings');
  assert.equal(both.yue, '設定');
  // Without a language runtime the site stays on English and never blanks out.
  assert.equal(site.text('tab.settings'), 'Settings');
  assert.equal(site.preview('hero.headline', 'yue', 5).length > 0, true);
  assert.notEqual(site.preview('hero.headline', 'en', 1), site.preview('hero.headline', 'en', 5));
  assert.notEqual(site.preview('hero.headline', 'yue', 1), site.preview('hero.headline', 'yue', 5));
});

/* ----------------------------------------------------------------- regex */

test('literal escaping neutralises every ECMAScript metacharacter', () => {
  const escaped = regex.escapeLiteral('a.b?c*d+e(f)g[h]i{j}k|l^m$n\\o/p');
  const compiled = new RegExp(escaped);
  assert.equal(compiled.test('a.b?c*d+e(f)g[h]i{j}k|l^m$n\\o/p'), true);
  assert.equal(compiled.test('aXbYcZ'), false);
});

test('plain text is the default matcher and regex is opt-in', () => {
  const plain = regex.createMatcher('v2.', {});
  assert.equal(plain.test('release v2.7'), true);
  assert.equal(plain.test('release v27'), false, 'plain mode must not treat . as a wildcard');
  const asRegex = regex.createMatcher('v2.', { regex: true, flags: 'i' });
  assert.equal(asRegex.test('release v27'), true);
});

test('an invalid pattern reports the engine message and matches everything', () => {
  const broken = regex.createMatcher('([unclosed', { regex: true });
  assert.equal(broken.ok, false);
  assert.ok(broken.message.length > 0);
  assert.equal(broken.test('anything'), true, 'a broken pattern must not empty the list');
});

test('evaluation reports ok, nomatch and invalid without throwing', async () => {
  const ok = await regex.evaluate('md3-v(\\d+)', 'g', 'md3-v26 md3-v27');
  assert.equal(ok.status, 'ok');
  assert.equal(ok.matches.length, 2);
  assert.deepEqual(ok.matches[0].groups[0], { n: 1, name: null, value: '26' });

  const none = await regex.evaluate('zzz', 'g', 'md3-v27');
  assert.equal(none.status, 'nomatch');

  const bad = await regex.evaluate('(', 'g', 'md3-v27');
  assert.equal(bad.status, 'invalid');
  assert.ok(bad.message.length > 0);

  const empty = await regex.evaluate('', 'g', 'sample');
  assert.equal(empty.status, 'empty');
});

test('capture groups keep the engine numbering and name the right group', async () => {
  const result = await regex.evaluate('(\\d+)-(?<word>\\w+)', 'g', '12-abc');
  assert.equal(result.status, 'ok');
  const [match] = result.matches;
  assert.deepEqual(match.groups, [
    { n: 1, name: null, value: '12' },
    { n: 2, name: 'word', value: 'abc' }
  ], 'a named group must not land on its neighbour');

  const optional = await regex.evaluate('(a)?(b)', 'g', 'b');
  assert.deepEqual(optional.matches[0].groups, [
    { n: 1, name: null, value: null },
    { n: 2, name: null, value: 'b' }
  ], 'a group that did not participate must keep its number, not be dropped');
});

test('a pattern that nests quantifiers is refused instead of run inline', async () => {
  const result = await regex.evaluate('(a+)+$', 'g', 'a'.repeat(30) + 'b');
  assert.equal(result.status, 'unsafe');
  assert.deepEqual(result.matches, []);
});

test('a zero-width match cannot spin forever', async () => {
  const result = await regex.evaluate('a*', 'g', 'aaa bbb');
  assert.equal(result.status, 'ok');
  assert.ok(result.matches.length <= 500);
});

test('the sample is bounded before matching', async () => {
  const result = await regex.evaluate('x', 'g', 'x'.repeat(50000));
  assert.ok(result.sampleTruncated, 'an oversized sample must be reported as truncated');
  assert.ok(result.matches.length <= 500);
});

/* ------------------------------------------------------------- changelog */

test('every date this view produces or reads is the same UTC calendar day', () => {
  // A release published at 01:41Z is filed under that UTC day. Building filter
  // bounds from the local calendar dropped the newest release out of "last 7
  // days" for anyone west of Greenwich, so both sides use UTC.
  assert.equal(changelogView.dayOf('2026-07-28').toISOString().slice(0, 10), '2026-07-28');
  assert.equal(changelogView.isoOf(new Date('2026-07-28T01:41:08Z')), '2026-07-28');
  assert.equal(changelogView.isoOf(new Date('2026-07-28T23:59:59Z')), '2026-07-28');
  assert.equal(changelogView.isoOf(changelogView.dayOf('2026-01-01')), '2026-01-01');
  const latest = changelog.releases[0];
  const bound = changelogView.isoOf(new Date(Date.parse(latest.published)));
  assert.equal(bound, latest.published.slice(0, 10),
    'the day a release is filed under must equal the day a filter bound computes');
});

test('typed dates accept ISO input and refuse impossible ones', () => {
  assert.equal(changelogView.parseTypedDate('2026-07-28'), '2026-07-28');
  assert.equal(changelogView.parseTypedDate('2026-7-8'), '2026-07-08');
  assert.equal(changelogView.parseTypedDate(''), '');
  assert.equal(changelogView.parseTypedDate('2026-02-30'), null);
  assert.equal(changelogView.parseTypedDate('2026-13-01'), null);
  assert.equal(changelogView.parseTypedDate('not a date'), null);
});

test('the generated changelog carries only sourced facts', () => {
  assert.ok(changelog.releases.length >= 30, 'every published release must be present');
  assert.equal(changelog.releaseCount, changelog.releases.length);
  const seen = new Set();
  for (const release of changelog.releases) {
    assert.match(release.published, /^\d{4}-\d{2}-\d{2}T/);
    assert.ok(release.tag && !seen.has(release.tag), `duplicate or missing tag: ${release.tag}`);
    seen.add(release.tag);
    assert.match(release.url, /^https:\/\/github\.com\//);
    for (const change of release.changes) {
      assert.match(change.sha, /^[0-9a-f]{40}$/);
      assert.equal(change.short, change.sha.slice(0, 9));
      assert.ok(['added', 'fixed', 'changed', 'removed', 'documented'].includes(change.category));
    }
    for (const asset of release.assets) {
      assert.ok(asset.name.length > 0);
      assert.equal(typeof asset.bytes, 'number');
    }
    assert.equal(typeof release.sameCommit, 'boolean');
    // "v02" is the app version 02.08.01.55 leaking out of the release name; it
    // is not a release number and must never be presented as one.
    assert.notEqual(release.version, 'v02');
    if (/^md3-v\d+$/.test(release.tag)) assert.equal(release.version, release.tag.slice(4));
    else assert.equal(release.version, release.tag, 'a release with no vN carries its tag');
  }
  // Newest first, so the viewer never has to sort at render time.
  const dates = changelog.releases.map((release) => release.published);
  assert.deepEqual(dates, [...dates].sort().reverse());
});

/* ------------------------------------------------- the site's own claims */

test('the site states its real layout-case count, computed from the harness', async () => {
  // The site advertises how thoroughly it is checked. That number is a claim
  // like any other, and it went stale the moment the per-tab matrix was added:
  // the Overview stat and the "How it is built" step both still said 156.
  const harness = await readFile(path.join(testDir, 'runtime-layout-clipping.mjs'), 'utf8');
  // Only the two suites that measure the SITE count toward the site's own
  // claim; the prototype at /app/ is a different surface with its own gate.
  const countFor = (title) => {
    const start = harness.indexOf(`test('${title}`);
    assert.notEqual(start, -1, `harness must contain the "${title}" suite`);
    const expression = /assert\.equal\(cases, ([\d *]+)\)/.exec(harness.slice(start));
    assert.ok(expression, `"${title}" must assert its own case count`);
    return expression[1].split('*').map(Number).reduce((product, value) => product * value, 1);
  };
  const landing = countFor('landing page stays inside every supported width');
  const perTab = countFor('every tab renders inside the viewport');
  const total = landing + perTab;
  assert.equal(landing, 156);
  assert.equal(perTab, 288);
  assert.equal(total, 444);

  const views = await readFile(path.join(siteDir, 'views.js'), 'utf8');
  assert.match(views, new RegExp(`stat\\('overview\\.stat\\.cases', '${total}'\\)`),
    `the Overview stat must say ${total}`);
  for (const language of ['en', 'yue']) {
    const title = copy.entries['build.step4.title'][language].join(' ');
    assert.ok(title.includes(String(total)),
      `build.step4.title (${language}) must say ${total}, not something older`);
  }
  const body = copy.entries['build.step4.body'];
  for (const language of ['en', 'yue']) {
    assert.ok(body[language].join(' ').includes('156'),
      `build.step4.body (${language}) must still break the total down`);
    assert.ok(body[language].join(' ').includes('288'),
      `build.step4.body (${language}) must name the per-tab half`);
  }
});

/* ---------------------------------------------------------------- dimsum */

test('every dim sum dish is bundled, named in both languages, and drawn locally', () => {
  const catalogue = globalThis.BAMBU_DIM_SUM;
  assert.equal(catalogue.chance, 0.01, 'the stated one-in-a-hundred odds are the implemented odds');
  assert.ok(catalogue.dishes.length >= 8);
  const ids = new Set();
  for (const dish of catalogue.dishes) {
    assert.ok(dish.en.length > 0 && dish.yue.length > 0, `${dish.id} needs both names`);
    assert.ok(!ids.has(dish.id));
    ids.add(dish.id);
    assert.ok(dish.art.length > 0);
    assert.doesNotMatch(dish.art, /https?:|url\(|<script|<image/i, `${dish.id} must be self-contained artwork`);
  }
});

/*
 * The deploy job is gated on the github-pages environment, which this repository
 * restricts to master. A run at any other ref is rejected before its first step,
 * with no steps and no log to explain it — so the failure is silent unless
 * something asserts the shape. The release event fires at the tag ref, which is
 * exactly that case, and it failed 12 times for 12 releases before this test.
 */
test('no release-ref run is sent to the environment-gated deploy job', async () => {
  const raw = await readFile(
    path.resolve(uiDir, '..', '.github', 'workflows', 'ui-md3-pages.yml'), 'utf8');

  /*
   * Normalize CRLF. Windows is this project's only supported platform, and git
   * checks this file out with CRLF here — a regex written against \n silently
   * matches nothing, so every assertion below would pass vacuously on a fresh
   * clone while appearing to be thorough.
   */
  const workflow = raw.replace(/\r\n/g, '\n');

  /*
   * Strip comments before asserting. This file explains at length why the
   * release event cannot deploy, and those comments quote the very settings
   * being asserted — "if: github.event_name != 'release'" appears in prose as
   * well as in YAML. An unanchored match is satisfied by the explanation of a
   * setting that has been deleted.
   */
  const code = workflow.split('\n').filter((line) => !/^\s*#/.test(line)).join('\n');

  // The workflow still listens for releases; it just must not deploy from one.
  assert.match(code, /^ {2}release:\n {4}types: \[published\]$/m);

  const jobs = code.slice(code.indexOf('\njobs:'));
  const jobOf = (name) => {
    const start = jobs.indexOf(`\n  ${name}:\n`);
    assert.notEqual(start, -1, `the ${name} job is missing`);
    const rest = jobs.slice(start + 1);
    // Job ids may start with any word character, not just a lowercase letter —
    // a terminator that only recognises [a-z] lets one job's slice swallow the
    // next and be satisfied by ITS settings.
    const next = rest.slice(1).search(/\n {2}[\w-]+:\n/);
    return next === -1 ? rest : rest.slice(0, next + 1);
  };

  const deploy = jobOf('deploy');
  const redeploy = jobOf('redeploy-on-release');
  // Prove the slices are disjoint, or an assertion about one can be satisfied
  // by the other's text.
  assert.equal(deploy.includes('redeploy-on-release:'), false);
  assert.equal(redeploy.includes('\n  deploy:'), false);

  // Anchored to line starts so only real YAML keys count.
  assert.match(deploy, /^ {4}environment:\n {6}name: github-pages$/m,
    'deploy is the environment-gated job');
  assert.match(deploy, /^ {4}if: github\.event_name != 'release'$/m,
    'deploy must refuse the release event, whose ref is a tag the environment rejects');

  assert.match(redeploy, /^ {4}if: github\.event_name == 'release'$/m);
  assert.doesNotMatch(redeploy, /^ {4}environment:$/m,
    'the redeploy job must not be environment-gated, or it is rejected too');
  assert.match(redeploy, /--ref master/);
  assert.match(redeploy, /^ {4}permissions:\n {6}actions: write$/m,
    'dispatching a workflow needs actions: write');

  /*
   * GITHUB_TOKEN must remain in the chain. It CAN dispatch: GitHub's
   * recursive-trigger prevention explicitly exempts workflow_dispatch and
   * repository_dispatch, which "always create workflow runs". An earlier
   * version of this test asserted the opposite and enforced a fail-soft path
   * that did nothing whenever no PAT was configured.
   */
  assert.match(redeploy, /secrets\.GITHUB_TOKEN/,
    'GITHUB_TOKEN is a valid dispatch fallback and must not be excluded');

  // Both jobs are conditioned on the same event, and between them they must
  // cover every event the workflow listens for.
  for (const event of ['push', 'workflow_dispatch']) {
    assert.doesNotMatch(deploy, new RegExp(`!= '${event}'`), `${event} must still deploy`);
  }

  /*
   * The release run only dispatches, so it must not share the deploy's
   * cancel-in-progress group. Match the whole expression, not its prefix: a
   * group that begins correctly and then resolves to one constant for every
   * event rejoins the deploy's group while still matching a prefix test.
   */
  assert.match(code,
    /^concurrency:\n {2}group: ui-md3-pages-\$\{\{ github\.event_name == 'release' && 'dispatch' \|\| 'deploy' \}\}$/m,
    'the release dispatcher needs its own concurrency group, resolved per event');
});

/*
 * The MD3 UI kit under design-system/ is published at /app/design-system/ and is
 * as public as any other page. Nothing was checking its copy: the terminology
 * script walks the templates and screen logic, these suites covered the
 * prototype and the site modules, and the kit sat outside all of them — so it
 * kept saying "Filament Manager" and "AMS mapping" through three passes that
 * each reported the rename finished.
 */
test('the published UI kit says ink, not filament, in everything it renders', async () => {
  const root = path.join(uiDir, 'design-system');
  const kit = path.join(root, 'ui_kits', 'bambu-studio');

  // Sweep all of design-system/, not just the kit. Scoping this to the kit was
  // the same mistake one level down: the typography specimen page under
  // guidelines/ used "Filament Manager" and "Export filaments" as its sample
  // strings, and a kit-only sweep would have walked straight past them.
  // Every extension the composer publishes. Sweeping only .jsx and .html left
  // 20 published .d.ts files unread, three of which documented the old
  // vocabulary to anyone authoring a new component against them.
  const PUBLISHED = /\.(jsx|html|ts|css|js)$/;
  const walk = async (dir) => {
    const found = [];
    for (const entry of await readdir(dir, { withFileTypes: true })) {
      const full = path.join(dir, entry.name);
      if (entry.isDirectory()) found.push(...await walk(full));
      else if (PUBLISHED.test(entry.name)) found.push(full);
    }
    return found;
  };
  const files = (await walk(root)).map((f) => path.relative(root, f));
  // Pin the real size. A size floor of 9 against a 74-file tree would let the
  // entire kit drop out of the sweep and still pass.
  assert.ok(files.length >= 70, `expected the design system's pages, found ${files.length}`);
  for (const required of ['guidelines', 'components', path.join('ui_kits', 'bambu-studio')]) {
    assert.ok(files.some((f) => f.includes(required)), `${required} must be swept too`);
  }
  assert.ok(files.some((f) => f.endsWith('.d.ts')), 'the published type declarations must be swept');

  /*
   * Bindings, not copy. These are STRIPPED from the line and the residue
   * re-tested — never used to exempt the whole line. TabBar.jsx is why:
   *
   *   { id: 'filament', label: 'Filament', icon: 'palette' }
   *
   * A whole-line exemption clears that on `id: 'filament'` and ships the
   * display label "Filament" on a published page, which is exactly what
   * happened until this was rewritten.
   */
  const IDENTIFIERS = [
    /window\.Screens\.Filament/g, /function Filament/g, /AddFilamentDialog/g,
    // `filaments.map` is a local in Device.jsx. It was written here as
    // `{filaments.map` back when index.html inlined raw JSX; the assembler
    // compiles that brace away, so the binding is matched without it.
    /KIT_FILAMENTS/g, /\bFILAMENTS\b/g, /const filaments/g, /\bfilaments\.map/g,
    /id: 'filament'/g, /'addfil'/g, /Filament\.jsx/g, /filament\.logic\.js/g,
  ];

  const offenders = [];
  for (const name of files) {
    const text = await readFile(path.join(root, name), 'utf8');
    text.split('\n').forEach((line, index) => {
      if (!/\b(filaments?|AMS)\b/i.test(line)) return;
      let residue = line;
      for (const identifier of IDENTIFIERS) residue = residue.replace(identifier, ' ');
      if (!/\b(filaments?|AMS)\b/i.test(residue)) return;
      offenders.push(`${name}:${index + 1} ${line.trim().slice(0, 80)}`);
    });
  }
  assert.deepEqual(offenders, []);

  // The kit's index.html is generated from the .jsx sources by
  // ui-md3/scripts/assemble-ui-kit.mjs, and CI runs that script with --check, so
  // a page that disagrees with its sources cannot deploy. This stays as the
  // reader-facing half of the contract: the vocabulary above is checked in the
  // sources, and these are the strings the assembled page must actually carry.
  const assembled = await readFile(path.join(kit, 'index.html'), 'utf8');
  for (const phrase of ['Ink Manager', 'Search inks', 'New ink', 'Ink Dispenser mapping',
    'Sync Ink Dispenser', 'No inks match your filter.']) {
    assert.ok(assembled.includes(phrase), `assembled kit is missing "${phrase}"`);
  }
});
