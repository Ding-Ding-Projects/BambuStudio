// Behaviour and source-pin test for the local, read-only printer status API.
//
// Part 1 compiles the real, standard-library-only header StatusApi/StatusApiCore.hpp into the fixture
// tests/status_api_fixture.cpp and checks routing, authorization, the door mapping and the JSON.
// Part 2 pins the production wiring: loopback-only bind, off by default, GET only, token never logged,
// no printer object read on the server thread, and the Preferences, startup and build registrations.
// Native runtime behaviour (a real socket, the Preferences section, a printer) is not covered here.
//
// Pass --negative to prove the test has teeth: it removes one guard at a time (in the header, the
// server or the wiring) and requires the test to fail for each. It exits 0 only when every removal
// is caught.
import {readFileSync, writeFileSync, mkdirSync, mkdtempSync, copyFileSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {compileFixture} from './native_fixture_compiler.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (rel) => readFileSync(path.join(root, rel), 'utf8').replace(/\r\n/g, '\n');
const negative = process.argv.includes('--negative');

const FILES = {
  core: 'src/slic3r/GUI/StatusApi/StatusApiCore.hpp',
  server: 'src/slic3r/GUI/StatusApi/StatusApiServer.cpp',
  serverHeader: 'src/slic3r/GUI/StatusApi/StatusApiServer.hpp',
  ui: 'src/slic3r/GUI/StatusApi/StatusApiUi.cpp',
  prefs: 'src/slic3r/GUI/Preferences.cpp',
  app: 'src/slic3r/GUI/GUI_App.cpp',
  cmake: 'src/slic3r/CMakeLists.txt',
  palette: 'src/slic3r/GUI/CommandPaletteIndex.cpp',
};

function method(source, signature) {
  const start = source.indexOf(signature);
  if (start < 0) throw new Error('Missing production block: ' + signature);
  const open = source.indexOf('{', start);
  let depth = 0;
  for (let i = open; i < source.length; i++) {
    if (source[i] === '{') depth++;
    else if (source[i] === '}' && --depth === 0) return source.slice(start, i + 1);
  }
  throw new Error('Unterminated block: ' + signature);
}

// Runs every check against the (possibly mutated) sources; returns the number of failures.
function runChecks(src, quiet) {
  let failures = 0;
  const check = (ok, name) => {
    if (!quiet) console.log((ok ? 'PASS ' : 'FAIL ') + name);
    if (!ok) failures++;
  };

  // ----- Part 2: wiring pins -------------------------------------------------------------------
  const {core, server, ui, prefs, app, cmake, palette} = src;
  check(/constexpr const char \*kBindAddress = "127\.0\.0\.1";/.test(core), 'the bind address constant is 127.0.0.1');
  check(server.includes('asio::ip::make_address_v4(kBindAddress)') && !/tcp::v4\(\)|address_v4::any|0\.0\.0\.0|tcp::v6/.test(server),
        'the listener binds the loopback constant and never a wildcard address');
  check(!/reuse_address/.test(server), 'the listener does not share its port with another process');
  check(server.includes('peer.address().is_loopback()') && (server.match(/is_loopback\(\)/g) || []).length >= 2,
        'a non-loopback peer is dropped on accept and flagged for the handler');
  check(/bool\s+enabled\s*=\s*false;/.test(core) && /int\s+port\s*=\s*kDefaultPort;/.test(core), 'settings are off by default');
  check(core.includes('if (!settings.enabled || !token_usable(settings.token))'), 'a disabled API or an unusable token serves nothing');
  check(core.includes('if (request.method != "GET")') && core.includes('r.allow    = "GET";'), 'only GET is served; others get 405 with Allow: GET');
  check(core.includes('!constant_time_equal(presented, settings.token)') && core.includes('presented.empty() ||'), 'the token is required and compared in constant time');
  check(core.includes('if (!request.peer_is_loopback)') && core.includes('if (!host_header_is_loopback(request.host))'), 'peer and Host are checked');
  check(core.includes('DeviceLive::door_open_reading(p.door_bit, p.door_supported)'), 'door state reuses DeviceLive::door_open_reading');
  check(!/Access-Control/i.test(core) && !/access_control/i.test(server) && !/Access-Control/i.test(server), 'no CORS header is ever produced');
  check(server.includes('m_parser.header_limit(static_cast<std::uint32_t>(kMaxRequestHeader))') && server.includes('m_parser.body_limit(std::uint64_t(0))') &&
        server.includes('expires_after(std::chrono::seconds(kReadTimeoutSeconds))'), 'request size and time are bounded');
  check(server.includes('keep_alive(false)') && server.includes('"no-store"'), 'one request per connection, responses not cached');

  // Thread safety: printer objects are read only on the GUI thread, inside take_snapshot().
  const snapshot = method(server, 'std::vector<PrinterSnapshot> take_snapshot()');
  const outside = server.replace(snapshot, '');
  check(!/MachineObject|get_my_machine_list|getDeviceManager|->get_dev_id|->is_connected|door_open_bit/.test(outside), 'no printer object is read outside take_snapshot()');
  check(snapshot.includes('get_my_machine_list()') && snapshot.includes('obj->door_open_bit') && snapshot.includes('obj->support_door_open_check()'),
        'the snapshot reads the printer list, door support and the door bit');
  check(server.includes('class SnapshotTimer : public wxTimer') && server.includes('timer().Start(1000)'), 'the snapshot is refreshed by a GUI-thread timer');
  const session = method(server, 'class Session');
  check(session.includes('std::lock_guard<std::mutex> lock(shared().mutex)') && session.includes('handle_request(settings, printers, request)'),
        'the server thread reads a mutex-protected copy and calls the pure router');

  // The token is never logged.
  const logLines = [...(server + ui).matchAll(/BOOST_LOG_TRIVIAL\([^;]*;/g)].map((m) => m[0]);
  check(logLines.length > 0 && logLines.every((l) => !/token/i.test(l) && !/settings\.token/.test(l)), 'no log line mentions the token');
  check(ui.includes('register_sensitive(this)') && ui.includes('wxTE_PASSWORD'), 'the token row is registered sensitive and masked');

  // Startup, shutdown, Preferences, palette, build.
  check(app.includes('StatusApi::start_after_startup();') && (app.match(/StatusApi::shutdown\(\);/g) || []).length >= 2, 'started after startup and stopped on both shutdown paths');
  check(prefs.includes('add_tab("status_api", _L("Local status API"), create_status_api_tab());'), 'the Preferences section is registered');
  const tab = method(prefs, 'wxWindow *PreferencesDialog::create_status_api_tab()');
  check(tab.includes('StatusApi::kEnabledConfigKey') && tab.includes('StatusApi::kPortConfigKey') && tab.includes('create_token_row'), 'the section holds the switch, the port and the token');
  check(prefs.includes('if (param == StatusApi::kEnabledConfigKey)\n            StatusApi::apply_settings();'), 'the switch applies at once');
  for (const key of ['status_api_enabled', 'status_api_port', 'status_api_token', 'status_api_state'])
    check(palette.includes('{"' + key + '"'), 'command palette entry ' + key);
  for (const file of ['StatusApiCore.hpp', 'StatusApiServer.cpp', 'StatusApiServer.hpp', 'StatusApiUi.cpp', 'StatusApiUi.hpp'])
    check(cmake.includes('GUI/StatusApi/' + file), 'build registers ' + file);
  check(/constexpr const char \*kTokenConfigKey\s+= "status_api_token";/.test(core) && server.includes('RAND_bytes') && server.includes('unsigned char bytes[32]'),
        'the token is 32 random bytes from OpenSSL');

  // ----- Part 1: compile and run the fixture ---------------------------------------------------
  const output = mkdtempSync(path.join(tmpdir(), 'bambustudio-status-api-'));
  mkdirSync(path.join(output, 'StatusApi'));
  writeFileSync(path.join(output, 'StatusApi', 'StatusApiCore.hpp'), core);
  copyFileSync(path.join(root, 'src/slic3r/GUI/DeviceLiveReadings.hpp'), path.join(output, 'DeviceLiveReadings.hpp'));
  copyFileSync(path.join(root, 'tests/status_api_fixture.cpp'), path.join(output, 'fixture.cpp'));
  const cpp = path.join(output, 'fixture.cpp');
  const exe = path.join(output, 'fixture.exe');
  const stdio = quiet ? 'pipe' : 'inherit';
  let compile = compileFixture({source: cpp, exe, cwd: output, include: output});
  if (compile.error && compile.error.code === 'ENOENT')
    compile = spawnSync(process.env.CXX || 'g++', ['-std=c++17', '-Wall', '-Wextra', '-I' + output, cpp, '-o', exe], {cwd: output, stdio});
  if (compile.error) check(false, 'compile fixture (' + compile.error.message + ')');
  else if (compile.status !== 0) check(false, 'compile fixture');
  else {
    const run = spawnSync(exe, [], {cwd: output, stdio});
    check(run.status === 0, 'header fixture passes');
  }
  return failures;
}

const load = () => Object.fromEntries(Object.entries(FILES).map(([k, rel]) => [k, read(rel)]));

if (!negative) {
  const failures = runChecks(load(), false);
  console.log(failures ? failures + ' check(s) failed' : 'all checks passed');
  process.exit(failures ? 1 : 0);
}

// ----- Negative mode: remove one guard at a time -----------------------------------------------
const mutations = [
  ['core', 'if (!request.peer_is_loopback)', 'if (false)', 'peer loopback check'],
  ['core', 'if (!host_header_is_loopback(request.host))', 'if (false)', 'Host check'],
  ['core', 'if (request.method != "GET")', 'if (false)', 'GET-only check'],
  ['core', 'presented.empty() || !constant_time_equal(presented, settings.token)', 'presented.empty()', 'token comparison'],
  ['core', 'if (!settings.enabled || !token_usable(settings.token))', 'if (false)', 'enabled and token gate'],
  ['core', 'bool        enabled = false;', 'bool        enabled = true;', 'off-by-default'],
  ['core', 'constexpr const char *kBindAddress = "127.0.0.1";', 'constexpr const char *kBindAddress = "0.0.0.0";', 'loopback bind constant'],
  ['core', 'DeviceLive::door_open_reading(p.door_bit, p.door_supported)', 'DeviceLive::door_open_reading(p.door_bit, true)', 'door support gate'],
  ['server', 'if (!peer_ec && peer.address().is_loopback()) std::make_shared', 'if (!peer_ec) std::make_shared', 'peer drop on accept'],
  ['server', 'm_parser.body_limit(std::uint64_t(0));', '', 'body limit'],
  ['server', '    m_acceptor.bind(endpoint, ec);', '    m_acceptor.set_option(tcp::acceptor::reuse_address(true), ec);\n    m_acceptor.bind(endpoint, ec);', 'no shared port'],
  ['server', 'BOOST_LOG_TRIVIAL(info) << "[StatusApi] listening on "', 'BOOST_LOG_TRIVIAL(info) << "[StatusApi] token " << settings.token << " listening on "', 'token not logged'],
  ['app', '    StatusApi::start_after_startup();\n', '', 'startup wiring'],
  ['prefs', 'add_tab("status_api", _L("Local status API"), create_status_api_tab());', '', 'Preferences section'],
  ['cmake', '    GUI/StatusApi/StatusApiServer.cpp\n', '', 'build registration'],
];
let missed = 0;
for (const [file, from, to, label] of mutations) {
  const src = load();
  if (!src[file].includes(from)) {
    console.log('FAIL mutation target not found: ' + label);
    missed++;
    continue;
  }
  src[file] = src[file].replace(from, to);
  const failures = runChecks(src, true);
  console.log((failures ? 'CAUGHT ' : 'MISSED ') + 'removing the ' + label + (failures ? ' (' + failures + ' check(s) failed)' : ''));
  if (!failures) missed++;
}
// The unmutated sources must still pass, or "caught" would mean nothing.
const baseline = runChecks(load(), true);
console.log(baseline ? 'FAIL baseline has ' + baseline + ' failing check(s)' : 'PASS baseline passes');
console.log(missed || baseline ? 'negative run failed' : 'every removed guard was caught');
process.exit(missed || baseline ? 1 : 0);
