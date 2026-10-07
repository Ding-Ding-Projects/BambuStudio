import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The hosted Windows build restores the Ninja build tree of the latest main
// build from a draft release and saves its own there, in parts of at most
// 1.5 GB (docs/features/releases/windows-release-supply-chain.md, Build cache).
// sccache could not help: it cannot cache a compile that uses the precompiled
// header, which is nearly every source, so every build compiled them all.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = (...parts) => readFile(path.join(repoDir, ...parts), 'utf8');
const lf = (text) => text.replace(/\r\n/g, '\n');

const workflow = lf(await read('.github', 'workflows', 'build_bambu.yml'));
const restore = lf(await read('scripts', 'ci', 'Restore-BuildCache.ps1'));
const save = lf(await read('scripts', 'ci', 'Save-BuildCache.ps1'));

// One step's text, from its "- name:" line to the next step.
const step = (name) => {
  const start = workflow.indexOf(`      - name: ${name}\n`);
  assert.notEqual(start, -1, `step "${name}" exists`);
  const next = workflow.indexOf('\n      - name: ', start + 1);
  return workflow.slice(start, next === -1 ? undefined : next);
};

test('the build tree is restored before the compile and saved after it, from main only', () => {
  const restoreAt = workflow.indexOf('      - name: Restore the build tree from the build cache release\n');
  const buildAt = workflow.indexOf('      - name: Build slicer Win\n');
  const startAt = workflow.indexOf('      - name: Start saving the build tree to the build cache release\n');
  assert.ok(restoreAt > 0 && restoreAt < buildAt && buildAt < startAt, 'restore, compile, then start saving');

  const restoreStep = step('Restore the build tree from the build cache release');
  assert.match(restoreStep, /GH_TOKEN: \$\{\{ secrets\.TOKEN_GITHUB \}\}/, 'the draft release is read with the owner token');
  assert.match(restoreStep, /\.\/scripts\/ci\/Restore-BuildCache\.ps1/);
  assert.match(restoreStep, /-DependencyCacheKey '\$\{\{ inputs\.cache-key \}\}'/, 'a new dependency build invalidates the tree');
  assert.match(restoreStep, /-Cold:\$\$\{\{ contains\(github\.event\.head_commit\.message, '\[cold build\]'\) \}\}/,
    'a commit message can ask for a build from scratch');

  const build = step('Build slicer Win');
  // The first configure also tees its output into the retained diagnostics.
  assert.match(build, /cmake @configure(?: 2>&1 \| Tee-Object -FilePath "[^"\n]+")?\n\s*# [^\n]*\n\s*# [^\n]*\n\s*if \(\$LASTEXITCODE -ne 0 -and '\$\{\{ steps\.build_cache\.outputs\.state \}\}' -eq 'warm'\) \{/,
    'a restored tree that will not configure is dropped');
  assert.match(build, /Remove-Item -LiteralPath build -Recurse -Force\n\s*cmake @configure\n/, 'and the build configures from scratch');

  const startStep = step('Start saving the build tree to the build cache release');
  assert.match(startStep, /if: inputs\.os == 'windows-latest' && !inputs\.debug-symbols && github\.event_name == 'push' && github\.ref == 'refs\/heads\/main'/,
    'only main builds without symbols write the cache');
  assert.match(startStep, /Start-Process -FilePath pwsh -WindowStyle Hidden/, 'the save runs in the background while the payload is packaged');
  // A child started with redirected output inherits the step's output pipe, and
  // the runner kills whatever still holds it once the step ends.
  assert.doesNotMatch(startStep, /-RedirectStandard/, 'the background save never inherits the step output');
  assert.match(startStep, /"& '\$script' \*> '\$log'"/, 'it writes its own log');
  assert.match(startStep, /Save-BuildCache\.ps1/);
  assert.match(startStep, /BUILD_CACHE_SAVE_PID=/);

  const lastStepAt = workflow.lastIndexOf('\n      - name: ');
  assert.equal(workflow.slice(lastStepAt + 1, workflow.indexOf('\n', lastStepAt + 1)),
    '      - name: Finish saving the build tree to the build cache release', 'the job waits for the save last');
  const finish = step('Finish saving the build tree to the build cache release');
  assert.match(finish, /if: always\(\) && inputs\.os == 'windows-latest' && env\.BUILD_CACHE_SAVE_PID != ''/);
  assert.match(finish, /Wait-Process -Timeout 1800/);
});

test('the verified qpdf SDK and runtime are staged after the restore and passed to every configure', () => {
  const restoreAt = workflow.indexOf('      - name: Restore the build tree from the build cache release\n');
  const stageAt = workflow.indexOf('      - name: Stage the verified qpdf SDK and PDF runtime\n');
  const buildAt = workflow.indexOf('      - name: Build slicer Win\n');
  assert.ok(restoreAt > 0 && restoreAt < stageAt && stageAt < buildAt, 'restore, stage qpdf, then configure and compile');

  const stage = step('Stage the verified qpdf SDK and PDF runtime');
  assert.match(stage, /if: inputs\.os == 'windows-latest'\n/, 'every Windows build stages it, symbol builds included');
  assert.match(stage, /GH_TOKEN: \$\{\{ github\.token \}\}/, 'the job token reads the public release; HTTPS is the fallback');
  assert.match(stage, /\.\/scripts\/windows\/Install-LocalPdfTools\.ps1\n/, 'the hash-pinned installer does the staging');
  assert.match(stage, /-Destination '\$\{\{ github\.workspace \}\}\\install-dir\\tools\\pdf'/,
    'the runtime lands in the payload where the converter worker loads it');
  assert.match(stage, /-SdkDestination '\$\{\{ github\.workspace \}\}\\artifacts\\local-pdf\\sdk'/);
  assert.doesNotMatch(stage, /-Offline|-VerifyOnly|-TrustedManifestPath/, 'the hosted build acquires the pinned archive');

  const build = step('Build slicer Win');
  assert.match(build, /\$configure = @\([^)]*'-DLOCAL_CONVERTER_QPDF_SDK:PATH=\$\{\{ github\.workspace \}\}\\artifacts\\local-pdf\\sdk',/,
    'the warm and cold configure both receive the staged SDK');
  // The cache holds only the build directory, so a restore never replaces the staged runtime.
  assert.match(save, /\$archive \$BuildDirectory "-x!/, 'the build cache archives the build directory alone');
});

test('the owner token reaches the build job through every reusable workflow', async () => {
  const all = lf(await read('.github', 'workflows', 'build_all.yml'));
  const check = lf(await read('.github', 'workflows', 'build_check_cache.yml'));
  const deps = lf(await read('.github', 'workflows', 'build_deps.yml'));
  assert.match(all, /uses: \.\/\.github\/workflows\/build_check_cache\.yml\n(?:\s*#.*\n)*\s*secrets: inherit\n/);
  assert.match(check, /uses: \.\/\.github\/workflows\/build_deps\.yml\n\s*secrets: inherit\n/);
  assert.match(deps, /uses: \.\/\.github\/workflows\/build_bambu\.yml\n\s*secrets: inherit\n/);
});

test('the cache is saved in parts of at most 1.5 GB to a draft that is never published', () => {
  assert.match(save, /\[long\] \$PartBytes = 1500000000,/, 'parts default to 1.5 GB');
  assert.match(save, /if \(\$PartBytes -le 0 -or \$PartBytes -gt 1500000000\) \{ throw/, 'and may never be larger');
  assert.match(save, /& 7z a -t7z -mx=1 -mmt=on "-v\$\(\$PartBytes\)b"/, '7-Zip writes the volumes at that size');
  assert.match(save, /if \(\$part\.Length -gt \$PartBytes\) \{ throw/, 'every part is checked against the limit');
  assert.match(save, /& gh release create \$Tag --repo \$Repository --draft `/, 'the release is created as a draft');
  assert.match(save, /the build cache only lives in a draft/, 'and refused if it was ever published');
  assert.doesNotMatch(save, /--draft=false|gh release edit/, 'nothing here publishes it');
  assert.match(save, /sha256 = \(Get-FileHash -LiteralPath \$_\.FullName -Algorithm SHA256\)/, 'the manifest records every part\'s SHA-256');
  assert.match(save, /\[long\]\$asset\.size -ne \[long\]\$part\.size/, 'every part is read back from GitHub at the size written');
  assert.match(save, /\[int\]\$current\.run_number -le \$RunNumber/, 'the pointer never moves back to an older run');
  assert.match(save, /if \(\$free -lt \[long\]\$treeBytes \+ 10GB\) \{/, 'the archive never fills the drive the payload is packaged on');
  assert.match(save, /'-xr!\.pnpm-store'/, 'the device page package store is not cached');
  assert.match(save, /\$cutoff = \[DateTime\]::UtcNow\.AddHours\(-2\)/, 'a run still uploading keeps its parts');
  assert.match(save, /\} catch \{\s*Write-Host "::warning::Build cache not saved:/, 'a failed save is a warning');
  assert.match(save, /\nexit 0\n$/, 'and never fails the build');
});

test('a restored tree is checked, keyed and made out of date exactly where the sources changed', () => {
  for (const part of ['vc=$env:VCToolsVersion', 'sdk=$(', 'cmake=$(Get-ToolVersion cmake)', 'ninja=$(Get-ToolVersion ninja)', 'deps=$DependencyCacheKey'])
    assert.ok(restore.includes(part), `the key includes ${part}`);
  assert.match(restore, /"BUILD_CACHE_KEY=\$key" \| Out-File -Append -FilePath \$env:GITHUB_ENV/, 'the save step gets the same key');
  assert.match(restore, /if \(\[string\]\$manifest\.key -cne \$key\)/, 'a tree built with another toolchain is not used');
  assert.match(restore, /if \(\$size -ne \[long\]\$part\.size\)/, 'every part size is checked');
  assert.match(restore, /if \(\$hash -cne \[string\]\$part\.sha256\)/, 'and every part SHA-256');
  assert.ok(restore.includes('"ws=$([System.IO.Path]::GetFullPath($Workspace)'), 'the key includes the workspace path');
  assert.match(restore, /git -C \$Workspace diff --name-only --no-renames -z \$commit\)/,
    'the changed files are the diff from the cached commit to the working tree');
  assert.match(restore, /\[datetime\]::new\(2020, 1, 1, 0, 0, 0, \[DateTimeKind\]::Utc\)/,
    'every tracked file is made older than its object, at a time Ninja on Windows sees as positive');
  assert.match(restore, /\$needed = \$partBytes \+ \[long\]\$manifest\.tree_bytes \+ 4GB/, 'the restore checks the free space first');
  assert.match(restore, /if \(\$Cold\) \{ throw/, 'a cold build skips the restore but still writes the key');
  assert.match(restore, /foreach \(\$relative in \$changed\) \{[\s\S]*?SetLastWriteTimeUtc\(\$path, \$now\)/, 'and every changed file newer');
  assert.match(restore, /-Filter 'device_page\.stamp'[\s\S]*?Remove-Item -Force/, 'the device page bundle, built into the source tree, is rebuilt');
  assert.match(restore, /if \(\$extracting -and \(Test-Path -LiteralPath \$tree\)\)/, 'only a tree it extracted is removed on failure');
  assert.match(restore, /Write-State 'cold'/, 'a failure falls back to a build from scratch');
  assert.match(restore, /\nexit 0\n$/, 'and never fails the build');
});

test('both cache scripts parse', { skip: spawnSync('pwsh', ['-NoProfile', '-Command', 'exit 0']).status !== 0 && 'pwsh is not installed' }, () => {
  for (const script of ['Restore-BuildCache.ps1', 'Save-BuildCache.ps1']) {
    const file = path.join(repoDir, 'scripts', 'ci', script);
    const result = spawnSync('pwsh', ['-NoProfile', '-Command',
      `$e = $null; [void][System.Management.Automation.Language.Parser]::ParseFile('${file.replaceAll("'", "''")}', [ref]$null, [ref]$e); if ($e) { $e | ForEach-Object { $_.ToString() }; exit 1 }`],
      { encoding: 'utf8' });
    assert.equal(result.status, 0, `${script} does not parse:\n${result.stdout}${result.stderr}`);
  }
});
