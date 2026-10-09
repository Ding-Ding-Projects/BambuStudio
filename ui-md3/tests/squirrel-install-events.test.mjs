import assert from 'node:assert/strict';
import { readFile, readdir } from 'node:fs/promises';
import path from 'node:path';
import test from 'node:test';
import { fileURLToPath } from 'node:url';

// The installer (Squirrel.Windows) makes a shortcut for, and starts, every executable in the
// package unless one of them is marked aware of it. Unmarked, the regex helper got the shortcut
// named after the application, and the application's own shortcut was named "BambuStudio" in a
// Start Menu folder "Bambu Research" (issue #52). The launcher is now the one marked executable:
// it handles the install events itself and makes its own shortcuts.

const testDir = path.dirname(fileURLToPath(import.meta.url));
const repoDir = path.resolve(testDir, '..', '..');
const read = async (...parts) => (await readFile(path.join(repoDir, ...parts), 'utf8')).replace(/\r\n/g, '\n');
const code = (text) => text.replace(/\/\*[\s\S]*?\*\//g, ' ').replace(/^[ \t]*\/\/.*$/gm, ' ');
const block = (rc, id) => {
  const start = rc.indexOf(`BLOCK "${id}"`);
  assert.notEqual(start, -1, `version block ${id} exists`);
  return rc.slice(start, rc.indexOf('}', start));
};

test('the launcher is marked aware of the installer, in the block the installer reads', async () => {
  const rc = code(await read('src', 'platform', 'msw', 'BambuStudio.rc.in'));
  // Squirrel.Windows queries \StringFileInfo\040904B0\SquirrelAwareVersion and no other block.
  assert.match(block(rc, '040904B0'), /VALUE "SquirrelAwareVersion", "1"/);
  assert.match(rc, /VALUE "Translation", 0x409, 1252, 0x409, 1200/);
  // The installer names a shortcut after ProductName and files it under CompanyName.
  for (const id of ['040904E4', '040904B0']) {
    assert.match(block(rc, id), /VALUE "ProductName", "Bambu Studio MD3"/, `${id} product name`);
    assert.match(block(rc, id), /VALUE "CompanyName", "codingmachineedge"/, `${id} company name`);
  }
  // No other executable may be marked, or it would get install events and a shortcut of its own.
  const msw = path.join(repoDir, 'src', 'platform', 'msw');
  for (const name of await readdir(msw)) {
    if (name === 'BambuStudio.rc.in' || !/\.rc(\.in)?$/.test(name)) continue;
    assert.doesNotMatch(await read('src', 'platform', 'msw', name), /SquirrelAwareVersion/, name);
  }
});

test('the launcher handles every install event before it loads anything', async () => {
  const launcher = code(await read('src', 'BambuStudio_app_msvc.cpp'));
  const handler = launcher.slice(launcher.indexOf('static int handle_squirrel_event('), launcher.indexOf('extern "C" {'));
  assert.match(handler, /const bool installed = wcscmp\(event, L"--squirrel-install"\) == 0;/);
  assert.match(handler, /const bool updated   = wcscmp\(event, L"--squirrel-updated"\) == 0;/);
  assert.match(handler, /const bool uninstall = wcscmp\(event, L"--squirrel-uninstall"\) == 0;/);
  assert.match(handler, /uninstall \? L"--removeShortcut=" : L"--createShortcut="\) \+ exe_name \+\s*L" --shortcut-locations=" \+ locations;/,
    'the shortcuts are made and removed by Update.exe, for this executable only');
  assert.match(handler, /if \(wcscmp\(event, L"--squirrel-firstrun"\) == 0\)\s*return squirrel_first_run\(\);/,
    'the first run after an install is handed to the install root stub');
  // An update keeps only the places that still hold one of this application's shortcuts.
  assert.match(handler, /const bool desktop = installed \|\| uninstall \|\| squirrel_link_exists\(CSIDL_DESKTOPDIRECTORY, kSquirrelDesktopLink\) \|\|\s*squirrel_link_exists\(CSIDL_DESKTOPDIRECTORY, kLegacyDesktopLink\);/);
  assert.match(handler, /const bool start   = installed \|\| uninstall \|\| squirrel_link_exists\(CSIDL_PROGRAMS, kSquirrelStartLink\) \|\|\s*squirrel_link_exists\(CSIDL_PROGRAMS, kLegacyStartLink\);/);
  // Legacy shortcuts go only when the new ones were made, or on uninstall.
  assert.match(handler, /if \(done \|\| uninstall\) \{[\s\S]*?remove_legacy_squirrel_shortcut\(CSIDL_DESKTOPDIRECTORY, kLegacyDesktopLink, root\);\s*remove_legacy_squirrel_shortcut\(CSIDL_PROGRAMS, kLegacyStartLink, root\);/);
  assert.match(launcher, /kSquirrelDesktopLink = L"Bambu Studio MD3\.lnk";/);
  assert.match(launcher, /kSquirrelStartLink   = L"codingmachineedge\\\\Bambu Studio MD3\.lnk";/);
  assert.match(launcher, /kLegacyDesktopLink   = L"BambuStudio\.lnk";/);
  assert.match(launcher, /kLegacyStartLink     = L"Bambu Research\\\\BambuStudio\.lnk";/);
  // Legacy shortcuts are removed only when they point into this installation.
  assert.match(launcher, /ours = _wcsnicmp\(target, root\.c_str\(\), root\.size\(\)\) == 0;[\s\S]*?if \(!ours\)\s*return;\s*::DeleteFileW\(link\.c_str\(\)\);/);

  const main = launcher.slice(launcher.indexOf('_set_error_mode(_OUT_TO_MSGBOX);'));
  const event = main.indexOf('handle_squirrel_event(argc, argv)');
  assert.ok(event !== -1 && event < main.indexOf('OpenGLVersionCheck opengl_version_check;') && event < main.indexOf('BambuStudio.dll'),
    'an install event returns before the OpenGL check opens a window and before BambuStudio.dll loads');
  assert.match(main, /if \(squirrel_exit >= 0\)\s*return squirrel_exit;/);
  assert.match(main, /if \(wcscmp\(argv\[i\], L"--squirrel-firstrun"\) == 0\)\s*continue;/, 'the first-run argument never reaches the app');
});

// After an interactive install Squirrel starts app-<version>\bambu-studio.exe --squirrel-firstrun
// itself and does not wait for it. That start has been reported not to show the application, so
// the launcher waits (bounded) for Update.exe to finish and starts the install root's stub, the
// target of the working shortcuts, without any Squirrel argument. Any failure is a normal start.
test('the first run after an install hands over to the install root stub', async () => {
  const launcher = code(await read('src', 'BambuStudio_app_msvc.cpp'));
  const start = launcher.indexOf('static int squirrel_first_run()');
  const end = launcher.indexOf('static int handle_squirrel_event(');
  assert.ok(start !== -1 && end > start, 'squirrel_first_run is defined before the event handler that calls it');
  const firstRun = launcher.slice(start, end);

  // Only the parent that is Update.exe is waited for, and only for a bounded minute.
  assert.match(firstRun, /CreateToolhelp32Snapshot\(TH32CS_SNAPPROCESS, 0\)/);
  assert.match(firstRun, /if \(entry\.th32ProcessID == self\) \{\s*parent = entry\.th32ParentProcessID;/);
  assert.match(firstRun, /const bool from_update = _wcsicmp\(parent_image\.c_str\(\), L"Update\.exe"\) == 0;/);
  assert.match(firstRun, /if \(from_update\) \{[\s\S]*?::OpenProcess\(SYNCHRONIZE, FALSE, parent\)[\s\S]*?::WaitForSingleObject\(update, 60000\)/,
    'the wait for Update.exe is bounded to a minute');
  assert.doesNotMatch(firstRun, /INFINITE/, 'no unbounded wait');

  // The stub is the install root's executable of the same name, started with nothing but its own
  // path: no Squirrel argument, so the application it starts makes a normal start.
  assert.match(firstRun, /if \(!squirrel_paths\(exe_name, root\)\) \{[\s\S]*?return -1;/, 'no install root: a normal start');
  assert.match(firstRun, /const std::wstring stub = root \+ exe_name;/);
  assert.match(firstRun, /if \(::GetFileAttributesW\(stub\.c_str\(\)\) == INVALID_FILE_ATTRIBUTES\) \{[\s\S]*?return -1;/, 'no stub: a normal start');
  assert.match(firstRun, /const std::wstring command = L"\\"" \+ stub \+ L"\\"";/, 'the stub gets no argument');
  assert.doesNotMatch(firstRun.replace(/L"squirrel event --squirrel-firstrun: /g, ''), /--squirrel-/,
    'no Squirrel argument appears anywhere but in the trace text');
  assert.match(firstRun, /std::vector<wchar_t> buffer\(command\.begin\(\), command\.end\(\)\);/);
  assert.match(firstRun, /::CreateProcessW\(stub\.c_str\(\), buffer\.data\(\), nullptr, nullptr, FALSE, attempts\[attempt\], nullptr, root\.c_str\(\),/,
    'started from the install root, like a shortcut');
  assert.match(firstRun, /const DWORD attempts\[\] = \{ CREATE_BREAKAWAY_FROM_JOB, 0 \};/, 'breaks away from a job, else starts without the flag');
  assert.match(firstRun, /for \(int attempt = 0; attempt < 2 && !started; \+\+attempt\)/);
  assert.match(firstRun, /startup\.dwFlags = STARTF_USESHOWWINDOW;\s*startup\.wShowWindow = SW_SHOWNORMAL;/, 'shown normally');
  assert.match(firstRun, /if \(!started\) \{\s*outcome\([^)]*\);\s*return -1;\s*\}/, 'a failed start is a normal start');
  assert.match(firstRun, /::AllowSetForegroundWindow\(process\.dwProcessId\)/);
  assert.match(firstRun, /outcome\(stub, process\.dwProcessId, [^;]*\);\s*return 0;\s*\}\s*$/, 'a started stub ends this process');
  assert.equal(firstRun.match(/return -1;/g).length, 3, 'exactly the three failures fall back to a normal start');
  assert.equal(firstRun.match(/return 0;/g).length, 1);

  // The launcher keeps to the Windows version it declares: nothing newer than Windows Server 2003.
  assert.match(launcher, /^#define _WIN32_WINNT 0x0502$/m);
  assert.match(launcher, /#include <tlhelp32\.h>/);
  for (const newer of ['QueryFullProcessImageName', 'PROCESS_QUERY_LIMITED_INFORMATION', 'PROC_THREAD_ATTRIBUTE', 'EXTENDED_STARTUPINFO_PRESENT', 'GetTickCount64']) {
    assert.doesNotMatch(firstRun, new RegExp(newer), `${newer} needs a newer Windows than the launcher declares`);
  }

  // A first run that could not be handed over is a normal start without the Squirrel argument.
  const main = launcher.slice(launcher.indexOf('_set_error_mode(_OUT_TO_MSGBOX);'));
  assert.match(main, /if \(squirrel_exit >= 0\)\s*return squirrel_exit;/);
  assert.match(main, /if \(wcscmp\(argv\[i\], L"--squirrel-firstrun"\) == 0\)\s*continue;/);
});

test('the launcher trace says when, which process, and how every start ended', async () => {
  const launcher = code(await read('src', 'BambuStudio_app_msvc.cpp'));
  const trace = launcher.slice(launcher.indexOf('static void launcher_trace('), launcher.indexOf('#include <objbase.h>'));
  assert.match(trace, /_wfopen\(path, L"a, ccs=UTF-8"\)/, 'the log stays append-only');
  assert.match(trace, /::GetLocalTime\(&now\);\s*fwprintf\(f, L"%04u-%02u-%02uT%02u:%02u:%02u\.%03u pid=%lu ",[\s\S]*?::GetCurrentProcessId\(\)\);\s*va_list args;/,
    'every line starts with the local time and the process id');
  assert.match(launcher, /launcher_trace\(L"launcher start: %ls", ::GetCommandLineW\(\)\);\s*const int squirrel_exit = handle_squirrel_event\(argc, argv\);/,
    'every launcher process records its command line before anything else');
  assert.match(launcher, /if \(bambustu_main == nullptr\) \{[\s\S]*?launcher_trace\(L"EXIT -1: bambustu_main not exported"\);\s*return -1;/);
  assert.match(launcher, /const int result = bambustu_main\([^;]*\);\s*launcher_trace\(L"bambustu_main returned %d", result\);\s*return result;/);
  const firstRun = launcher.slice(launcher.indexOf('static int squirrel_first_run()'), launcher.indexOf('static int handle_squirrel_event('));
  assert.match(firstRun, /launcher_trace\(L"squirrel event --squirrel-firstrun: waiting up to 60 s for Update\.exe pid=%lu"/);
  assert.match(firstRun, /L"squirrel event --squirrel-firstrun: parent=%lu %ls update=%d wait=%ls wait_error=%lu stub=%ls "\s*L"started=%d pid=%lu error=%lu foreground=%d; %ls"/,
    'one outcome line carries the parent, the wait, the start and its error');
  assert.match(firstRun, /launcher_trace\(L"squirrel event --squirrel-firstrun: CreateProcess flags=0x%08lx failed, error=%lu"/);
  // Each way out of squirrel_first_run is traced.
  for (const why of ['no install root, starting here', 'no stub, starting here', 'start failed, starting here', 'handed over, exiting']) {
    assert.ok(firstRun.includes(`L"${why}"`), why);
  }
});

test('the first-run diagnostic is dispatch-only, pinned, bounded and text-only', async () => {
  const workflow = await read('.github', 'workflows', 'diagnose-installer-first-run.yml');
  const on = workflow.slice(workflow.indexOf('\non:'), workflow.indexOf('\npermissions:'));
  assert.match(on, /^\non:\n  workflow_dispatch:\n/, 'dispatched by hand only');
  assert.doesNotMatch(on, /^  (push|pull_request|schedule|workflow_run|release):/m);
  assert.match(on, /tag:\n[\s\S]*?default: md3-v225\n/);
  assert.match(on, /observe_seconds:\n[\s\S]*?default: '180'\n/);
  assert.match(workflow, /\npermissions:\n  contents: read\n\n/);
  // Each action at the revision the startup trace workflow pins it to.
  const trace = await read('.github', 'workflows', 'trace-release-startup.yml');
  const pins = (text) => [...new Set(text.match(/uses: [^\s@]+@[0-9a-f]{40}/g))].sort();
  assert.deepEqual(pins(workflow), ['uses: actions/checkout@df4cb1c069e1874edd31b4311f1884172cec0e10',
    'uses: actions/upload-artifact@b7c566a772e6b6bfb58ed0dc250532a479d7789f']);
  for (const pin of pins(workflow)) assert.ok(pins(trace).includes(pin), `${pin} matches the startup trace workflow`);
  assert.doesNotMatch(workflow.replace(/uses: [^\s@]+@[0-9a-f]{40}/g, ''), /uses: /, 'every action is pinned to a commit');
  const jobs = workflow.slice(workflow.indexOf('\njobs:'));
  assert.equal(jobs.match(/\n    runs-on: windows-2025\n/g).length, 2);
  assert.equal(jobs.match(/\n    timeout-minutes: 30\n/g).length, 2);
  assert.match(jobs, /-Arm Interactive\b/);
  assert.match(jobs, /-Arm Silent\b/);
  assert.equal(jobs.match(/\n          retention-days: 7\n/g).length, 2);
  assert.equal(jobs.match(/\n          path: diagnostic\/installer-first-run\/\n/g).length, 2);
  assert.doesNotMatch(workflow, /secrets\./, 'only the run token, which reads the release');
  for (const line of workflow.split('\n').filter((text) => text.includes('${{ inputs.'))) {
    assert.match(line, /^ {10}[A-Z_]+: \$\{\{ inputs\.[a-z_]+ \}\}$/, 'inputs reach the script only through the environment');
  }

  const diagnose = await read('scripts', 'ci', 'Diagnose-InstallerFirstRun.ps1');
  assert.match(diagnose, /\$ErrorActionPreference = 'Stop'\nSet-StrictMode -Version Latest\n/);
  assert.match(diagnose, /\$env:RUNNER_ENVIRONMENT -ne 'github-hosted'/, 'refuses to run off a disposable hosted runner');
  assert.match(diagnose, /\[ValidateRange\(30, 420\)\]\[int\] \$ObserveSeconds/);
  assert.match(diagnose, /\$PollSeconds = 5\n/);
  assert.match(diagnose, /Interactive = \(\$Arm -eq 'Interactive'\)/, 'the interactive arm installs through the verifier with -Interactive');
  for (const source of [/Join-Path \$env:LOCALAPPDATA 'SquirrelTemp'/, /Join-Path \$env:TEMP 'bbs-launcher-trace\.log'/,
    /Join-Path \(Join-Path \$env:APPDATA 'BambuStudio'\) 'log'/, /LogName = 'Application'/, /Win32_VideoController/, /WTSGetActiveConsoleSessionId/]) {
    assert.match(diagnose, source);
  }
  for (const verdict of ['not_started', 'started_crashed', 'started_exited', 'started_hidden', 'started_visible']) {
    assert.match(diagnose, new RegExp(`return & \\$verdict '${verdict}' `), verdict);
  }
  assert.match(diagnose, /function Start-InstalledStub \{[\s\S]*?'GH_TOKEN', 'GITHUB_TOKEN', 'ORG_TOKEN', 'RELEASE_TOKEN'[\s\S]*?Start-Process -FilePath \$stubPath/,
    'no workflow credential reaches installed code');
  assert.doesNotMatch(diagnose, /CopyFromScreen|\.png|PrintWindow|BitBlt/i, 'no screenshots');
});

test('the first-run diagnostic lets a crash win and names the faulting process', async () => {
  // md3-v230 crashed about 3 s into every start with an access violation, after an untitled
  // splash; the phase still read started_visible. A crash now wins over every other value.
  const diagnose = await read('scripts', 'ci', 'Diagnose-InstallerFirstRun.ps1');
  const fn = (name) => {
    const start = diagnose.indexOf(`\nfunction ${name} {`);
    assert.notEqual(start, -1, `${name} exists`);
    return diagnose.slice(start, diagnose.indexOf('\n}\n', start) + 3);
  };
  // An NTSTATUS error exit code is a crash; the launcher's own -1 (0xFFFFFFFF) is not.
  assert.match(fn('Test-CrashExitCode'), /\$value -ge 3221225472 -and \$value -ne 4294967295/);
  // The collected Application log text is parsed back into crash entries.
  const parse = fn('ConvertFrom-ApplicationEventText');
  assert.match(parse, /\$entry\.provider -eq 'Application Error' -and \$entry\.id -eq 1000/);
  assert.match(parse, /\$entry\.provider -eq 'Windows Error Reporting' -and \$entry\.id -eq 1001/);
  assert.match(parse, /\$eventName -in @\('APPCRASH', 'MoAppCrash', 'BEX', 'BEX64'\)/);
  for (const name of ['Faulting application name', 'Faulting module name', 'Exception code', 'Fault offset', 'Faulting process id', 'Report Id']) {
    assert.ok(parse.includes(`'${name}'`), name);
  }
  const classify = fn('Get-FirstRunClassification');
  assert.ok(classify.indexOf("'started_crashed'") < classify.indexOf("'started_visible'"), 'the crash is decided first');
  // A crash entry whose process then ended by itself (the launcher's -1) is a handled fault.
  assert.match(classify, /\$handled = \$null -ne \$process -and \$null -ne \$process\.exit_code -and -not \(Test-CrashExitCode \$process\.exit_code\)/);
  // Not counted: a process from before the phase, and the copy a faulting process makes at its fault.
  assert.match(classify, /reason = 'started_before_phase'/);
  assert.match(classify, /reason = 'fault_copy'/);
  // A window is a visible start only from a process still running at the end, or the main frame.
  assert.match(classify, /\$_\.visible_window -and \$_\.alive_at_end/);
  assert.match(classify, /\$_\.visible_window -and -not \$_\.alive_at_end -and \(Test-MainFrameShown \$_\)/);
  for (const field of ['pid', 'exception_code', 'fault_offset', 'faulting_module', 'window_shown_before_crash']) {
    assert.match(classify, new RegExp(`\\n {12}${field} = `), `the crash record names ${field}`);
  }
  // The receipt keeps its fields and adds the crash; the classification reads the collected log.
  const phase = fn('Invoke-FirstRunPhase');
  for (const field of ['classification', 'basis', 'exit', 'crash', 'application_faults', 'not_counted', 'processes']) {
    assert.match(phase, new RegExp(`\\n {8}${field} = `), `receipt.${field}`);
  }
  assert.ok(phase.indexOf('Save-PhaseEvidence -Directory') < phase.indexOf('Get-FirstRunClassification -Processes'),
    'the Application log is collected before the classification');
  assert.match(phase, /Get-FirstRunClassification -Processes \$processes -ApplicationEvents \$applicationEvents -Since \$started -Until \$collected/);
});

test('the hosted install check installs silently unless asked for the interactive install', async () => {
  const verify = code(await read('scripts', 'ci', 'Verify-HostedSquirrelInstall.ps1'));
  assert.match(verify, /\[switch\] \$Interactive\n\)/);
  assert.match(verify, /if \(\$Interactive\) \{\s*\$setup = Start-Process -FilePath \(Join-Path \$downloadRoot 'Setup\.exe'\) -PassThru\n/,
    'the interactive install passes no argument and shows its window');
  assert.match(verify, /Assert-True \(\$setup\.WaitForExit\(600000\)\)/);
  assert.match(verify, /else \{\s*\$setup = Start-Process -FilePath \(Join-Path \$downloadRoot 'Setup\.exe'\) -ArgumentList '--silent' -PassThru -WindowStyle Hidden\s*Wait-Process -Id \$setup\.Id -Timeout 600\s*\}/,
    'the default stays the silent, hidden install');
  // Credentials are cleared before either install starts.
  assert.ok(verify.indexOf("'GH_TOKEN', 'GITHUB_TOKEN', 'ORG_TOKEN', 'RELEASE_TOKEN'") < verify.indexOf('if ($Interactive)'));
});

test('the hosted install check reads the shortcuts back', async () => {
  const verify = await read('scripts', 'ci', 'Verify-HostedSquirrelInstall.ps1');
  assert.match(verify, /\$desktopLink = Join-Path \(\[Environment\]::GetFolderPath\('Desktop'\)\) 'Bambu Studio MD3\.lnk'/);
  assert.match(verify, /\$startLink = Join-Path \(\[Environment\]::GetFolderPath\('Programs'\)\) 'codingmachineedge\\Bambu Studio MD3\.lnk'/);
  assert.match(verify, /Assert-True \(\$target -ieq \$expectedTarget\)/, 'each shortcut starts the application');
  assert.match(verify, /Assert-True \(\$strayLinks\.Count -eq 0\)/, 'no shortcut starts another executable of the installation');
});
