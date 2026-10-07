import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The README screenshot workflow is the one hosted capture route that uploads
// plain images, from a public repository. These are the rules that keep that
// safe: dispatch only, read-only, pinned, the token on the install step alone,
// a fail-closed privacy check before a three-day upload of an allowlist of PNG
// files, and nothing else in the upload.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = async (...parts) => (await readFile(path.join(repoDir, ...parts), 'utf8')).replace(/\r\n/g, '\n');
const workflowPath = ['.github', 'workflows', 'readme-screenshots.yml'];

// The workflow's jobs and steps, from its fixed two-space layout.
function jobsOf(workflow) {
  const body = workflow.slice(workflow.indexOf('\njobs:\n') + 7);
  const jobs = {};
  for (const chunk of body.split(/\n(?=  [a-z][a-z0-9-]*:\n)/)) {
    const name = chunk.match(/^  ([a-z][a-z0-9-]*):\n/)[1];
    const steps = chunk.slice(chunk.indexOf('\n    steps:\n') + 12).split(/\n(?=      - (?:name|uses): )/);
    jobs[name] = { text: chunk, header: chunk.slice(0, chunk.indexOf('\n    steps:\n')), steps };
  }
  return jobs;
}

function allowlist(job) {
  const block = job.header.match(/\n      README_ALLOWLIST: \|\n((?: {8}\S.*\n?)+)/);
  assert.ok(block, 'the job declares README_ALLOWLIST as a literal block');
  return block[1].split('\n').map((line) => line.trim()).filter(Boolean);
}

test('the README screenshot workflow is dispatch-only, read-only and pinned', async () => {
  const workflow = await read(...workflowPath);
  const on = workflow.slice(workflow.indexOf('\non:\n'), workflow.indexOf('\npermissions:'));
  assert.match(on, /^\non:\n  workflow_dispatch:\n    inputs:\n/, 'dispatched by hand only');
  assert.doesNotMatch(on, /^  (?!workflow_dispatch)[a-z_]+:/m, 'no other trigger');
  assert.match(on, /\n      release_tag:\n[^]*?required: true\n/);
  assert.match(on, /\n      expected_source_commit:\n[^]*?required: true\n/);
  assert.match(on, /\n      rows:\n[^]*?required: false\n        default: ''\n/);
  assert.match(workflow, /\npermissions:\n  contents: read\n\n/);
  assert.doesNotMatch(workflow.slice(workflow.indexOf('\njobs:')), /permissions:/, 'no job widens the permissions');
  assert.doesNotMatch(workflow, /secrets\./, 'only the run token, which reads the release');

  const uses = workflow.match(/uses: \S+/g);
  assert.ok(uses.length >= 6);
  for (const use of uses) assert.match(use, /^uses: [\w-]+\/[\w-]+@[0-9a-f]{40}$/, `${use} is pinned to a commit`);
  const others = (await Promise.all(['verify-release-evidence.yml', 'trace-release-startup.yml', 'ui-md3-pages.yml']
    .map((name) => read('.github', 'workflows', name)))).join('\n');
  for (const use of new Set(uses)) assert.ok(others.includes(use), `${use} is the revision the other workflows pin`);

  const jobs = jobsOf(workflow);
  assert.deepEqual(Object.keys(jobs), ['native-app', 'design-references']);
  assert.match(jobs['native-app'].header, /\n    if: \$\{\{ inputs\.target == 'native-app' \}\}\n    runs-on: windows-2025\n    timeout-minutes: 60\n/);
  assert.match(jobs['design-references'].header, /\n    if: \$\{\{ inputs\.target == 'design-references' \}\}\n    runs-on: ubuntu-latest\n/);
  for (const line of workflow.split('\n').filter((text) => text.includes('${{ inputs.'))) {
    assert.match(line, /^ {10}[A-Z_]+: \$\{\{ inputs\.[a-z_]+ \}\}$|^    if: \$\{\{ inputs\.target == '[a-z-]+' \}\}$|^run-name: /,
      'inputs reach a script only through the environment');
  }
});

test('the token is on the install step alone and never reaches the application', async () => {
  const workflow = await read(...workflowPath);
  assert.equal(workflow.match(/GH_TOKEN/g).length, 1, 'one GH_TOKEN in the whole workflow');
  const install = jobsOf(workflow)['native-app'].steps.filter((step) => step.includes('GH_TOKEN'));
  assert.equal(install.length, 1);
  assert.match(install[0], /^      - name: Install and verify the published release\n        env:\n(?: {10}#.*\n)*          GH_TOKEN: \$\{\{ github\.token \}\}\n/);
  assert.match(install[0], /& scripts\/ci\/Verify-HostedSquirrelInstall\.ps1 `\n\s+-Tag \$env:RELEASE_TAG `\n\s+-Repository \$env:GITHUB_REPOSITORY `\n\s+-ExpectedCommit \$env:SOURCE_COMMIT `\n\s+-OutputPath \(Join-Path \$env:RUNNER_TEMP 'install\.json'\) `\n\s+-CiExecutionApproved\n/);
  assert.doesNotMatch(install[0], /-Interactive/, 'the silent default install');
  // The verifier clears the credentials before anything it downloaded runs.
  const verify = await read('scripts', 'ci', 'Verify-HostedSquirrelInstall.ps1');
  assert.ok(verify.indexOf("'GH_TOKEN', 'GITHUB_TOKEN', 'ORG_TOKEN', 'RELEASE_TOKEN'") < verify.indexOf("'Setup.exe') -ArgumentList '--silent'"));
});

test('the privacy check runs before a three-day upload of the staged directory only', async () => {
  const workflow = await read(...workflowPath);
  const jobs = jobsOf(workflow);
  const staged = { 'native-app': 'readme-screenshots-upload', 'design-references': 'readme-design-references-upload' };
  const names = { 'native-app': 'readme-screenshots', 'design-references': 'readme-design-references' };
  for (const [name, job] of Object.entries(jobs)) {
    const check = job.steps.findIndex((step) => step.includes('scripts/md3/check-readme-screenshot-privacy.py'));
    const upload = job.steps.findIndex((step) => step.includes('uses: actions/upload-artifact@'));
    assert.ok(check >= 0 && upload >= 0, `${name} checks and uploads`);
    assert.equal(upload, job.steps.length - 1, `${name} uploads last`);
    assert.ok(check < upload, `${name} runs the privacy check before the upload`);
    assert.equal(job.steps.filter((step) => step.includes('upload-artifact')).length, 1, `${name} uploads once`);
    const step = job.steps[upload];
    assert.doesNotMatch(step, /\n        if:|continue-on-error/, 'the upload runs only after every earlier step passed');
    assert.match(step, new RegExp(`\\n          name: ${names[name]}-\\$\\{\\{ github\\.run_id \\}\\}\\n`));
    const paths = step.match(/\n          path: (.*)\n/);
    assert.ok(paths, 'one single-line upload path');
    assert.equal(paths[1], `\${{ runner.temp }}/${staged[name]}/`, 'the upload is the staged directory and nothing else');
    assert.doesNotMatch(paths[1], /probe|evidence|log|install|receipt|report/i);
    assert.match(step, /\n          if-no-files-found: error\n/);
    assert.match(step, /\n          retention-days: 3\n?$/);
    // The check stages into exactly that directory, away from the evidence it reads.
    const checker = job.steps[check];
    assert.match(checker, new RegExp(`--out [^\\n]*${staged[name]}`));
    assert.match(checker, /--allowlist-env README_ALLOWLIST/);
    assert.doesNotMatch(checker.match(/--evidence-dir [^\n]*/)[0], new RegExp(staged[name]));
  }
  assert.equal(workflow.match(/retention-days:/g).length, 2);
});

test('the allowlists name only README images the right route can regenerate', async () => {
  const workflow = await read(...workflowPath);
  const jobs = jobsOf(workflow);
  const readme = await read('README.md');
  const manifest = JSON.parse(await read('docs', 'screenshots', 'recapture-manifest.json'));
  const rows = new Map(manifest.captures.map((row) => [row.file, row]));
  const native = allowlist(jobs['native-app']);
  const design = allowlist(jobs['design-references']);
  assert.equal(native.length, 23);
  assert.equal(design.length, 3);
  assert.equal(new Set([...native, ...design]).size, 26, 'no path is listed twice');
  for (const file of [...native, ...design]) {
    assert.match(file, /^docs\/(?:readme-assets|screenshots)\/[a-z0-9_-]+(?:\/[a-z0-9_-]+)*\.png$/, `${file} is a PNG path`);
    assert.ok(readme.includes(`(${file})`), `${file} is a README image`);
    assert.ok(rows.has(file), `${file} has a recipe`);
    assert.doesNotMatch(file, /\/before-/, 'historical evidence is never retaken');
  }
  for (const file of native) {
    const { recipe } = rows.get(file);
    assert.ok(['page', 'crop-probe'].includes(recipe.kind), `${file} is a native page or crop`);
    assert.ok(!recipe.blocked || /needs the Mesa route/.test(recipe.blocked), `${file} is reachable with --mesa`);
    assert.deepEqual(recipe.tuple, { language: 'en', theme: 'light', density: 'comfortable' }, `${file} uses the one prepared tuple`);
  }
  for (const file of design) assert.equal(rows.get(file).recipe.kind, 'pages', `${file} is a Pages render`);
  // Every README image is either retaken or deliberately historical.
  const referenced = [...readme.matchAll(/\((docs\/[A-Za-z0-9_./-]+\.png)\)/g)].map((match) => match[1]);
  for (const file of new Set(referenced)) {
    assert.ok(native.includes(file) || design.includes(file) || rows.get(file)?.recipe?.kind === 'historical',
      `${file} is covered`);
  }
  assert.match(jobs['native-app'].text, /prepare-capture-datadirs\.py [^\n]*`\n\s+--languages en --themes light --densities comfortable\n/);
  assert.match(jobs['native-app'].text, /recapture\.py --exe \$env:README_EXE `\n[^]*?--kinds page,crop-probe --mesa --commit \$env:SOURCE_COMMIT `\n[^]*?--match \$env:README_MATCH --evidence-probe\n/);
});

test('the privacy check and the capture scripts carry the evidence it needs', async () => {
  const checker = await read('scripts', 'md3', 'check-readme-screenshot-privacy.py');
  const privacyTest = await read('ui-md3', 'tests', 'evidence-privacy.test.mjs');
  // JavaScript writes a slash as \/ in a regex literal; Python needs no escape. Pairs are read
  // left to right, so an escaped backslash before a slash stays escaped.
  const jsProfile = privacyTest.match(/export const USER_PROFILE_PATH = \/(.+)\/;\n/)[1]
    .replace(/\\(.)/g, (pair, next) => (next === '/' ? '/' : pair));
  const pyProfile = checker.match(/USER_PROFILE_PATH = re\.compile\(r'(.+)'\)\n/)[1];
  assert.equal(pyProfile, jsProfile, 'the same user-profile pattern as the published-evidence test');
  for (const needle of ["r'gh[pousr]_[A-Za-z0-9]{20,}|github_pat_'", "RUNNER_ACCOUNT = 'runneradmin'", "'USERNAME'", "'COMPUTERNAME'",
    "('email', ", "if distinct < 12 or max(ImageStat.Stat(rgb).stddev) < 10:", "MAX_BYTES = 32 * 1024 * 1024",
    "last.get('kind') != 'end'", "raise UnusableInput('the upload directory must not exist yet')"]) {
    assert.ok(checker.includes(needle), needle);
  }

  const recapture = await read('scripts', 'md3', 'recapture.py');
  assert.match(recapture, /ap\.add_argument\('--evidence-probe', action='store_true',/);
  assert.match(recapture, /if args\.evidence_probe and status == 'done':\n\s+# [^]*?app\.probe\(\)\n\s+row\['evidence_probe'\] = os\.path\.basename\(app\.last_probe\)/);
  assert.match(recapture, /RAIL_NAMES = \('Workspace navigation', 'Navigation rail'\)/);

  // The design references are the exact URLs the README links them to.
  const capture = await read('ui-md3', 'scripts', 'capture-app.mjs');
  const readme = await read('README.md');
  const references = [...capture.matchAll(/\{ name: '(material-[a-z-]+)', query: '([^']+)' \}/g)];
  assert.equal(references.length, 3);
  for (const [, name, query] of references) {
    assert.ok(readme.includes(`(docs/readme-assets/${name}.png)](https://ding-ding-projects.github.io/BambuStudio/app/?${query})`),
      `${name} renders the README's own link`);
  }
  assert.match(capture, /const README_REFERENCE_VIEWPORT = \{ width: 1600, height: 1000 \};/);
  assert.match(capture, /Object\.assign\(row, \{ status: 'done', evidence_probe: evidenceName \}\);/);
});
