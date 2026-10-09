import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { readFile } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// Issue #46: no run had shown that an installed copy updates itself. The self-update diagnostic
// installs an older release on a disposable hosted Windows runner, starts it the way a shortcut does
// and records whether it stages the latest release. These tests pin its guard rails: dispatch only,
// read-only, pinned actions, text evidence only, the helpers it shares with the installer first-run
// diagnostic, and the five classifications. A newer folder counts as staged only when Update.exe is
// shown to have finished it; scripts/ci/Test-SelfUpdateClassification.ps1 runs the classifier on
// synthetic facts wherever pwsh is installed, and the workflow runs it before the diagnostic.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = async (...parts) => (await readFile(path.join(repoDir, ...parts), 'utf8')).replace(/\r\n/g, '\n');
const functionBody = (source, name) => {
  const start = source.indexOf(`\nfunction ${name} {`);
  assert.notEqual(start, -1, `${name} exists`);
  return source.slice(start, source.indexOf('\n}\n', start) + 3);
};
const classifications = ['app_crashed', 'updated_staged', 'update_failed', 'update_offered_not_staged', 'no_update_seen'];

test('the self-update workflow is dispatch-only, read-only, pinned, bounded and uploads text only', async () => {
  const workflow = await read('.github', 'workflows', 'diagnose-self-update.yml');
  const on = workflow.slice(workflow.indexOf('\non:'), workflow.indexOf('\npermissions:'));
  assert.match(on, /^\non:\n  workflow_dispatch:\n/, 'dispatched by hand only');
  assert.doesNotMatch(on, /^  (push|pull_request|pull_request_target|schedule|workflow_run|workflow_call|release|repository_dispatch):/m);
  assert.match(on, /from_tag:\n[\s\S]*?default: md3-v231\n/);
  assert.match(on, /observe_seconds:\n[\s\S]*?default: '900'\n/);
  assert.match(workflow, /\npermissions:\n  contents: read\n\n/);
  assert.equal(workflow.match(/permissions:/g).length, 1, 'no job widens the permissions');

  // Each action at the revision the installer first-run diagnostic pins it to.
  const firstRun = await read('.github', 'workflows', 'diagnose-installer-first-run.yml');
  const pins = (text) => [...new Set(text.match(/uses: [^\s@]+@[0-9a-f]{40}/g))].sort();
  assert.deepEqual(pins(workflow), pins(firstRun));
  assert.doesNotMatch(workflow.replace(/uses: [^\s@]+@[0-9a-f]{40}/g, ''), /uses: /, 'every action is pinned to a commit');

  const jobs = workflow.slice(workflow.indexOf('\njobs:'));
  assert.equal(jobs.match(/\n    runs-on: /g).length, 1, 'one job');
  assert.match(jobs, /\n    runs-on: windows-2025\n/);
  const timeout = Number(jobs.match(/\n    timeout-minutes: (\d+)\n/)[1]);
  assert.ok(timeout > 0 && timeout <= 45, `the job is limited to ${timeout} minutes, at most 45`);
  assert.match(jobs, /\n          persist-credentials: false\n/);
  assert.doesNotMatch(workflow, /secrets\./, 'only the run token, which reads the releases');
  assert.equal(workflow.match(/\$\{\{ github\.token \}\}/g).length, 1, 'the token reaches one step');
  for (const line of workflow.split('\n').filter((text) => text.includes('${{ inputs.'))) {
    assert.match(line, /^ {10}[A-Z_]+: \$\{\{ inputs\.[a-z_]+ \}\}$/, 'inputs reach the script only through the environment');
  }
  assert.match(jobs, /\.\/scripts\/ci\/Diagnose-SelfUpdate\.ps1\n\s+-FromTag \$env:DIAGNOSE_FROM_TAG -Repository \$env:GITHUB_REPOSITORY\n\s+-ObserveSeconds \$env:DIAGNOSE_OBSERVE_SECONDS\n\s+-OutputDirectory diagnostic\/self-update -CiExecutionApproved\n/);
  // The PowerShell it runs, and the helpers that script loads, are parsed before anything runs.
  for (const script of ['Diagnose-SelfUpdate.ps1', 'Diagnose-InstallerFirstRun.ps1', 'Verify-HostedSquirrelInstall.ps1',
    'Test-SelfUpdateClassification.ps1']) {
    assert.ok(jobs.includes(`'scripts/ci/${script}'`), `${script} is parse-checked`);
  }
  // The classification is checked on synthetic facts after the parse and before the install.
  const synthetic = jobs.indexOf('\n        run: ./scripts/ci/Test-SelfUpdateClassification.ps1\n');
  assert.ok(synthetic > jobs.indexOf('- name: Parse the diagnostic PowerShell'), 'the synthetic check follows the parse');
  assert.ok(synthetic < jobs.indexOf('./scripts/ci/Diagnose-SelfUpdate.ps1\n'), 'and comes before the diagnostic');

  // Only text leaves the runner, and only for seven days.
  assert.match(jobs, /- name: Upload text evidence\n        if: \$\{\{ always\(\) \}\}\n        uses: actions\/upload-artifact@/,
    'the evidence is uploaded whatever the outcome');
  const upload = jobs.slice(jobs.indexOf('uses: actions/upload-artifact@'));
  assert.match(upload, /\n          retention-days: 7\n/);
  const listed = upload.slice(upload.indexOf('path: |\n') + 'path: |\n'.length, upload.indexOf('\n          if-no-files-found:'));
  assert.deepEqual(listed.split('\n').map((line) => line.trim()), ['diagnostic/self-update/**/*.json',
    'diagnostic/self-update/**/*.jsonl', 'diagnostic/self-update/**/*.txt', 'diagnostic/self-update/**/*.log']);
  assert.doesNotMatch(workflow, /\.(png|jpe?g|gif|bmp|webp|mp4)\b/i, 'no image or video is collected');
});

test('the self-update diagnostic loads the first-run helpers instead of copying them', async () => {
  const diagnose = await read('scripts', 'ci', 'Diagnose-SelfUpdate.ps1');
  const firstRun = await read('scripts', 'ci', 'Diagnose-InstallerFirstRun.ps1');
  const start = diagnose.indexOf('$ReusedFunctions = @(');
  assert.notEqual(start, -1);
  const reused = [...diagnose.slice(start, diagnose.indexOf(')\n', start)).matchAll(/'([A-Za-z]+-[A-Za-z]+)'/g)].map((m) => m[1]);
  for (const name of ['Get-FirstRunSample', 'Merge-FirstRunProcesses', 'Register-ProcessTraces', 'Read-ProcessTraces',
    'ConvertFrom-ApplicationEventText', 'Get-FirstRunClassification', 'Save-PhaseEvidence', 'Start-InstalledStub',
    'Stop-InstalledProcesses']) {
    assert.ok(reused.includes(name), `${name} is reused`);
  }
  for (const name of reused) {
    assert.match(firstRun, new RegExp(`\\nfunction ${name} \\{`), `${name} is a top-level function of the first-run diagnostic`);
    assert.doesNotMatch(diagnose, new RegExp(`\\nfunction ${name} \\{`), `${name} is loaded, not copied`);
  }
  // Taken from the parsed source, with its window type; a helper that is gone stops the run.
  assert.match(diagnose, /\[System\.Management\.Automation\.Language\.Parser\]::ParseFile\(\$firstRunPath, /);
  assert.match(diagnose, /\$firstRunPath = Join-Path \$PSScriptRoot 'Diagnose-InstallerFirstRun\.ps1'/);
  assert.match(diagnose, /throw "The installer first-run diagnostic no longer defines \$name\."/);
  assert.match(diagnose, /\. \(\[scriptblock\]::Create\(\$firstRunFunctions\[\$name\]\.Extent\.Text\)\)/);
  assert.match(diagnose, /GetCommandName\(\) -eq 'Add-Type'/);
  assert.equal(firstRun.match(/^Add-Type -TypeDefinition @'$/gm).length, 1, 'the first-run diagnostic has one top-level window type');
  // The variables those helpers read are defined under the same names.
  for (const name of ['watchedNames', 'MaxCollectedBytes', 'MaxAppLogs', 'installRoot', 'stubPath', 'squirrelTemp', 'launcherTrace', 'appLogDirectory']) {
    assert.match(diagnose, new RegExp(`\\n\\$${name} = `), `$${name} is defined`);
  }
  // The package entry hash is the hosted install check's, loaded from its parsed source in the same way.
  const verify = await read('scripts', 'ci', 'Verify-HostedSquirrelInstall.ps1');
  assert.match(verify, /\nfunction Get-EntrySha256 \{/);
  assert.doesNotMatch(diagnose, /\nfunction Get-EntrySha256 \{/, 'Get-EntrySha256 is loaded, not copied');
  assert.match(diagnose, /\$installCheckPath = Join-Path \$PSScriptRoot 'Verify-HostedSquirrelInstall\.ps1'/);
  assert.match(diagnose, /throw 'The hosted install check no longer defines Get-EntrySha256\.'/);
  assert.match(diagnose, /\. \(\[scriptblock\]::Create\(\$entryHashFunction\[0\]\.Extent\.Text\)\)/);
  assert.match(functionBody(diagnose, 'Test-StagedFiles'), /Get-EntrySha256 -Entry \$entry/);
  // The install is the hosted install check's, which verifies the release and the shortcuts.
  assert.match(diagnose, /& \(Join-Path \$PSScriptRoot 'Verify-HostedSquirrelInstall\.ps1'\) -Tag \$FromTag -Repository \$Repository `\n\s+-ExpectedCommit \$sourceCommit -OutputPath \$installReceiptPath -CiExecutionApproved\n/);
  assert.doesNotMatch(diagnose, /-Interactive\b/, 'the install is silent, so the first start is the one a shortcut makes');
  assert.match(diagnose, /installedFolders\[0\]\.name -ne "app-\$installedVersion"/, 'the installed version is checked');
});

test('the self-update diagnostic runs only on a hosted runner, hands no credential on and changes no preference', async () => {
  const diagnose = await read('scripts', 'ci', 'Diagnose-SelfUpdate.ps1');
  assert.match(diagnose, /\$ErrorActionPreference = 'Stop'\nSet-StrictMode -Version Latest\n/);
  assert.match(diagnose, /\$env:RUNNER_ENVIRONMENT -ne 'github-hosted'/, 'refuses to run off a disposable hosted runner');
  assert.match(diagnose, /\[ValidatePattern\('\^md3-v\\d\+\$'\)\]\[string\] \$FromTag/);
  assert.match(diagnose, /\[ValidateRange\(120, 1500\)\]\[int\] \$ObserveSeconds/);
  assert.match(diagnose, /\n\$PollSeconds = 5\n/);

  // Every release read comes before the install check, which clears the token from the process.
  const verify = diagnose.indexOf("& (Join-Path $PSScriptRoot 'Verify-HostedSquirrelInstall.ps1')");
  const reads = [...diagnose.matchAll(/& gh release /g)].map((m) => m.index);
  assert.equal(reads.length, 3, 'the release to install, the latest release and its RELEASES file');
  assert.ok(reads.every((index) => index < verify), 'gh runs only before anything downloaded does');
  // The application starts only through the first-run helper, which clears the credentials first.
  assert.doesNotMatch(diagnose, /Start-Process/);
  assert.match(diagnose, /\$null = Start-InstalledStub\n/);
  const firstRun = await read('scripts', 'ci', 'Diagnose-InstallerFirstRun.ps1');
  assert.match(functionBody(firstRun, 'Start-InstalledStub'), /'GH_TOKEN', 'GITHUB_TOKEN', 'ORG_TOKEN', 'RELEASE_TOKEN'[\s\S]*?Start-Process -FilePath \$stubPath/);

  // Update automatically is on by default, so nothing is enabled; the configuration is only read.
  const appConfig = await read('src', 'libslic3r', 'AppConfig.cpp');
  assert.match(appConfig, /if \(get\("auto_update"\)\.empty\(\)\) \{\s*set_bool\("auto_update", true\);/);
  assert.match(diagnose, /changed_by_diagnostic = \$false/);
  assert.doesNotMatch(diagnose, /(Set-Content|Add-Content|Out-File|WriteAll\w*|Copy-Item|Move-Item|Remove-Item)[^\n]*\$appConfigPath/,
    'BambuStudio.conf is never written');

  // On a fresh profile the modal first-run guide stands before the startup update check, so the
  // diagnostic closes it with WM_CLOSE and records each close.
  const gui = await read('src', 'slic3r', 'GUI', 'GUI_App.cpp');
  const startup = gui.slice(gui.indexOf('this->config_wizard_startup();'));
  const check = startup.indexOf('this->check_new_version();');
  assert.ok(check > 0 && check < startup.indexOf('});'), 'the update check follows the guide in the same startup step');
  assert.match(diagnose, /\$SetupWizardTitle = 'Setup Wizard\|/);
  const phase = functionBody(diagnose, 'Invoke-ObservationPhase');
  assert.match(phase, /if \(\$CloseSetupWizard\) \{[\s\S]*?\[SelfUpdateWindow\]::PostMessageW\(\[IntPtr\] \$window\.Handle, \[SelfUpdateWindow\]::WM_CLOSE,[\s\S]*?\$wizardCloses\.Add\(/);
  assert.match(phase, /setup_wizard_closes = \$wizardCloses\.ToArray\(\)/);
  assert.match(diagnose, /Invoke-ObservationPhase -Name 'observe' -Seconds \$ObserveSeconds -InstalledVersion \$installedVersion `\n\s+-CloseSetupWizard -StopWhenStaged -CloseAtEnd/);
});

test('the self-update diagnostic decides five values in order, a crash first, from text evidence only', async () => {
  const diagnose = await read('scripts', 'ci', 'Diagnose-SelfUpdate.ps1');
  const classify = functionBody(diagnose, 'Get-SelfUpdateClassification');
  const positions = classifications.map((value) => classify.indexOf(`return & $result '${value}' `));
  classifications.forEach((value, index) => assert.notEqual(positions[index], -1, `${value} is a result`));
  assert.deepEqual([...positions].sort((a, b) => a - b), positions, 'decided in the documented order');
  assert.equal(classify.match(/return & \$result '/g).length, classifications.length, 'no other value');

  // A crash is the first-run classifier's verdict, for the observation and for the next start.
  assert.match(classify, /\$phase\.verdict\.classification -ne 'started_crashed'/);
  assert.match(classify, /verdict = \$Facts\.observe_verdict/);
  assert.match(classify, /verdict = \$Facts\.next_start_verdict/);
  assert.match(functionBody(diagnose, 'Invoke-ObservationPhase'),
    /\$verdict = Get-FirstRunClassification -Processes \$processes -ApplicationEvents \$applicationEvents -Since \$started -Until \$collected/);
  assert.ok(diagnose.indexOf('$evidence = Save-PhaseEvidence') < diagnose.indexOf('$verdict = Get-FirstRunClassification'),
    'the Application log is collected before the verdict');
  // Update.exe crashes and its .NET unhandled exceptions are failures.
  assert.match(functionBody(diagnose, 'Save-UpdateExeEvents'), /'Application Error', 'Windows Error Reporting', '\.NET Runtime', 'Application Hang'/);
  assert.match(classify, /foreach \(\$crash in @\(\$Facts\.update_exe_crashes \| Where-Object \{ \$null -ne \$_ \}\)\)/);
  // A newer folder with its executable, once no Update.exe --update is at work, is only a candidate.
  assert.match(diagnose, /\$candidate = if \(\$null -ne \$newest -and \$newest\.has_executable -and -not \$stillUpdating\) \{ \$newest \}/);
  // It counts as staged only with no failure and nothing missing, as the application counts an update
  // only when Update.exe exited with 0 and a newer folder is there.
  const gui = await read('src', 'slic3r', 'GUI', 'GUI_App.cpp');
  assert.match(gui, /updated = exit_code == 0 && squirrel_newer_version_staged\(update_exe\);/);
  assert.match(classify, /if \(\$null -ne \$candidate -and \$failures\.Count -eq 0 -and \$gaps\.Count -eq 0\) \{[\s\S]*?return & \$result 'updated_staged' /);
  assert.ok(classify.indexOf('$failures = [System.Collections.Generic.List[string]]::new()') < classify.indexOf("return & $result 'updated_staged' "),
    'every failure is collected before a version can count as staged');
  // The gaps: no exit 0 seen, nothing newer logged, local RELEASES without the version, files that
  // differ from the package, and a next start that did not run the folder.
  assert.match(classify, /if \(\$cleanExits\.Count -eq 0 -and -not \$loggedClean\)/);
  assert.match(classify, /\$loggedClean = \$null -ne \$log\.update_exit_code -and \[int64\] \$log\.update_exit_code -eq 0 -and -not \$log\.nothing_newer/);
  assert.match(classify, /@\(\$Facts\.squirrel\.local_releases\)/);
  assert.match(classify, /\$Facts\.staged_files\.problems/);
  assert.match(classify, /\$ran -notcontains \$candidate\.name/);
  assert.match(classify, /partial staging: \$\(\$candidate\.name\)/);
  // Squirrel's record and the file comparison reach the classification, compared before the next start.
  const facts = diagnose.slice(diagnose.indexOf('$facts = [ordered]@{'));
  assert.match(facts, /\n        squirrel = \$squirrel\n/);
  assert.match(facts, /\n        staged_files = \$stagedFiles\n/);
  assert.match(facts, /\n        candidate = \$candidate\n/);
  assert.ok(diagnose.indexOf('Test-StagedFiles -Folder $candidate') < diagnose.indexOf("Invoke-ObservationPhase -Name 'next-start'"),
    'the files are compared before the next start runs them');
  const staged = functionBody(diagnose, 'Test-StagedFiles');
  assert.match(staged, /\[System\.IO\.Compression\.ZipFile\]::OpenRead\(\$package\)/);
  assert.ok(staged.includes(String.raw`[regex]::new('lib[\\/][^\\/]*[\\/]'`), "Squirrel's lib/<framework>/ rule");
  assert.match(staged, /_ExecutionStub\.exe/);
  assert.match(staged, /Get-FileHash -LiteralPath \$package -Algorithm SHA1/);
  // Update.exe exiting with 0 while the feed holds a newer version and nothing was staged is a failure.
  assert.match(classify, /if \(\$null -eq \$candidate -and \$feed\.newer -and \$stillRunning\.Count -eq 0 -and \(\$cleanExits\.Count -gt 0 -or \$log\.nothing_newer\)\)/);

  // The receipt keeps the evidence the issue asks for.
  const receipt = diagnose.slice(diagnose.indexOf('$receipt = [ordered]@{'));
  for (const field of ['classification', 'basis', 'installed', 'feed', 'auto_update', 'setup_wizard', 'observation', 'staged',
    'staged_candidate', 'staged_files', 'update_exe', 'app_log', 'squirrel', 'banner', 'application', 'next_start']) {
    assert.match(receipt, new RegExp(`\\n {8}${field} = `), `receipt.${field}`);
  }
  assert.doesNotMatch(diagnose, /CopyFromScreen|\.png|PrintWindow|BitBlt|Graphics\.FromImage/i, 'no screenshots');
});

test('the application log is decoded with the key read from the source, never copied into the diagnostic', async () => {
  const diagnose = await read('scripts', 'ci', 'Diagnose-SelfUpdate.ps1');
  const logSink = await read('src', 'libslic3r', 'LogSink.cpp');
  const values = [...logSink.matchAll(/^#define DEFAULT_KEY_(?:STR|IV)_[A-Z]+_\d+\s+"([^"]+)"/gm)].map((m) => m[1]);
  assert.ok(values.length >= 2, 'LogSink.cpp defines the local keys');
  for (const value of values) assert.ok(!diagnose.includes(value), 'no key value is copied into the diagnostic');
  assert.match(functionBody(diagnose, 'Get-LocalLogKeys'), /Join-Path 'libslic3r' 'LogSink\.cpp'/);

  // The diagnostic reads the format LogSinkBackend writes: a plain header, then one CBC chain.
  assert.match(logSink, /#define HEADER_BEGIN_MARKER "BEGIN_HEADER\\n"/);
  assert.match(logSink, /#define HEADER_END_MARKER\s+"\\nEND_HEADER\\n"/);
  assert.match(logSink, /header_json\["enc_key_tag"\] = m_log_enc_key_tag;/);
  assert.match(logSink, /m_log_enc_key_iv\.assign\(reinterpret_cast<const char\*>\(last_block\), AES_BLOCK_SIZE\);/);
  const decode = functionBody(diagnose, 'ConvertFrom-ApplicationLog');
  assert.match(decode, /\$beginMarker = "BEGIN_HEADER`n"/);
  assert.match(decode, /\$endMarker = "`nEND_HEADER`n"/);
  assert.match(decode, /\$aes\.DecryptCbc\(\$cipher, \$key\.iv, \[System\.Security\.Cryptography\.PaddingMode\]::None\)/);

  // The lines it reads are the ones the application writes.
  const gui = await read('src', 'slic3r', 'GUI', 'GUI_App.cpp');
  for (const line of ['"check new version: "', '"check new version error ("', '"auto update: updating to "',
    '"auto update: Update.exe started, waiting for it to finish"', '"auto update: Update.exe exited with code "',
    '"auto update: Update.exe found nothing newer to install"', '"auto update: could not start Update.exe, error "']) {
    assert.ok(gui.includes(line), `GUI_App.cpp writes ${line}`);
  }
  // The ready banner is drawn in the 3D view; the notification history records it by type name.
  const notifications = await read('src', 'slic3r', 'GUI', 'NotificationManager.cpp');
  assert.match(notifications, /case NotificationType::AppUpdateReady: return "AppUpdateReady";/);
  assert.match(notifications, /m_history_path = data_dir\(\) \+ "\/notification_history\.json";/);
  assert.match(functionBody(diagnose, 'Read-NotificationHistory'), /\(Get-JsonValue \$_ 'type'\) -eq 'AppUpdateReady'/);
});

test('a staged folder counts only when Update.exe is shown to have finished it, on synthetic facts', async () => {
  // The cases live in one PowerShell script, which the workflow runs too; these lines pin the ones
  // the rule rests on, so the script cannot lose them unnoticed.
  const cases = await read('scripts', 'ci', 'Test-SelfUpdateClassification.ps1');
  assert.match(cases, /\$ast = \[System\.Management\.Automation\.Language\.Parser\]::ParseFile\(/, 'the classifier is taken from the parsed script');
  assert.match(cases, /'Get-SelfUpdateClassification'/);
  assert.doesNotMatch(cases, /\nfunction Get-SelfUpdateClassification \{/, 'not copied');
  const expect = (name, value) => {
    const call = `Test-Case '${name}' `;
    const at = cases.indexOf(call);
    assert.notEqual(at, -1, `the case '${name}' exists`);
    const line = cases.slice(at + call.length, cases.indexOf('\n', at));
    assert.match(line, new RegExp(`^(?:\\$facts|\\(New-FinishedFacts\\)) '${value}'`), `${name}: ${value}`);
  };
  expect('an update Update.exe finished', 'updated_staged');
  expect('a staged folder and Update.exe exiting with 0xFFFFFFFF', 'update_failed');
  expect('a staged folder and Update.exe exiting with 1', 'update_failed');
  expect('Update.exe failing part of the way through the extraction', 'update_failed');
  expect('a staged folder and an Update.exe crash entry', 'update_failed');
  expect('a staged folder that local RELEASES does not list', 'update_failed');
  expect('a staged bambu-studio.exe that is not the package one', 'update_failed');
  expect('the next start ran the installed folder', 'update_failed');
  assert.ok(cases.includes("$facts = New-FinishedFacts\n$facts.update_runs = @(New-UpdateRun -ExitCode -1)\n" +
    "Test-Case 'a staged folder and Update.exe exiting with 0xFFFFFFFF'"), 'the non-zero exit case keeps every other fact of a finished update');
});

test('the synthetic classification cases pass', { skip: spawnSync('pwsh', ['-NoProfile', '-Command', 'exit 0']).status !== 0 && 'pwsh is not installed' }, () => {
  const script = path.join(repoDir, 'scripts', 'ci', 'Test-SelfUpdateClassification.ps1');
  const result = spawnSync('pwsh', ['-NoProfile', '-NonInteractive', '-File', script], { encoding: 'utf8' });
  assert.equal(result.status, 0, `${result.stdout}${result.stderr}`);
  assert.match(result.stdout, /All \d+ self-update classification cases passed\./);
});

test('the self-update article documents every classification', async () => {
  const article = await read('docs', 'features', 'automation', 'self-update-diagnostic.md');
  for (const value of classifications) assert.ok(article.includes(`\`${value}\``), value);
  assert.match(article, /gh workflow run diagnose-self-update\.yml -f from_tag=md3-v231 -f observe_seconds=900/);
  const twin = await read('docs', 'features', 'automation', 'self-update-diagnostic.yue_HK.md');
  assert.match(twin, /^---\ntranslation-of: self-update-diagnostic\.md\nsource-sha256: [0-9a-f]{64}\n/);
  for (const value of classifications) assert.ok(twin.includes(`\`${value}\``), `${value} in the Cantonese twin`);
  // The staged row states the rule the classifier applies, in both languages.
  const row = (text, value) => text.split('\n').find((line) => line.startsWith(`| \`${value}\` |`)) ?? '';
  for (const fragment of ['exited with 0', '`packages\\RELEASES`', 'SHA-256', 'next start']) {
    assert.ok(row(article, 'updated_staged').includes(fragment), `the updated_staged row names ${fragment}`);
  }
  assert.ok(row(article, 'update_failed').includes('partial staging'), 'the update_failed row names partial staging');
  for (const fragment of ['`packages\\RELEASES`', 'SHA-256']) {
    assert.ok(row(twin, 'updated_staged').includes(fragment), `the Cantonese updated_staged row names ${fragment}`);
  }
});
