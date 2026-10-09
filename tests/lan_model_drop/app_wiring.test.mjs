// Source contracts for the station side of the LAN model drop
// (docs/features/application-integration/lan-model-drop.md). They read the C++ sources as text, so
// they run without a build: the option is visible (its own Preferences section, a File menu item,
// command palette rows, a title-bar indicator), the station key is protected with DPAPI and never
// logged, all HTTP happens on the worker thread and reaches windows only through CallAfter, the
// worker stops in shutdown before windows are destroyed, and nothing is opened, sliced or printed
// automatically. Every contract is also run against a mutated source to prove it can fail.
import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const repo = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..');
const read = (rel) => readFileSync(path.join(repo, rel), 'utf8');

const src = {
  station: read('src/slic3r/GUI/LanModelDrop/LanModelDropStation.cpp'),
  stationHpp: read('src/slic3r/GUI/LanModelDrop/LanModelDropStation.hpp'),
  ui: read('src/slic3r/GUI/LanModelDrop/LanModelDropUi.cpp'),
  keyStore: read('src/slic3r/GUI/LanModelDrop/LanModelDropKeyStore.cpp'),
  model: read('src/slic3r/GUI/LanModelDrop/LanModelDropModel.cpp'),
  prefs: read('src/slic3r/GUI/Preferences.cpp'),
  mainFrame: read('src/slic3r/GUI/MainFrame.cpp'),
  palette: read('src/slic3r/GUI/CommandPalette.cpp'),
  paletteIndex: read('src/slic3r/GUI/CommandPaletteIndex.cpp'),
  topbar: read('src/slic3r/GUI/BBLTopbar.cpp'),
  app: read('src/slic3r/GUI/GUI_App.cpp'),
  cmake: read('src/slic3r/CMakeLists.txt'),
  notifications: read('src/slic3r/GUI/NotificationManager.cpp'),
};

// The body of the function whose signature starts with `signature` (a definition, not a
// declaration), by brace matching.
function body(source, signature) {
  let start = source.indexOf(signature);
  while (start >= 0) {
    const brace = source.indexOf('{', start);
    const semicolon = source.indexOf(';', start);
    if (brace >= 0 && (semicolon < 0 || brace < semicolon)) break;
    start = source.indexOf(signature, start + signature.length);
  }
  assert.ok(start >= 0, `missing: ${signature}`);
  const open = source.indexOf('{', start);
  let depth = 0;
  for (let i = open; i < source.length; ++i) {
    if (source[i] === '{') ++depth;
    else if (source[i] === '}' && --depth === 0) return source.slice(open, i + 1);
  }
  assert.fail(`unbalanced: ${signature}`);
}

// Source with comments and string contents blanked, so a word in a comment proves nothing.
function code(source) {
  return source
    .replace(/\/\*[\s\S]*?\*\//g, ' ')
    .replace(/\/\/[^\n]*/g, ' ')
    .replace(/"(?:\\.|[^"\\\n])*"/g, '""');
}

function visibleSection(prefs) {
  assert.match(prefs, /\n    add_tab\("lan_drop", _L\("LAN model drop"\), create_lan_drop_tab\(\)\);/);
  const tab = body(prefs, 'wxWindow *PreferencesDialog::create_lan_drop_tab()');
  assert.match(tab, /create_item_checkbox\(_L\("Receive models from the LAN drop site"\)[\s\S]*?"lan_drop_enabled"\)/);
  // The invite comes right after the switch, before the connection rows.
  assert.ok(tab.indexOf('"lan_drop_enabled"') < tab.indexOf('LanModelDrop::InvitePanel'));
  assert.ok(tab.indexOf('LanModelDrop::InvitePanel') < tab.indexOf('create_status_row'));
  for (const key of ['lan_drop_invite', 'lan_drop_status', 'lan_drop_address', 'lan_drop_station_key', 'lan_drop_code'])
    assert.match(tab, new RegExp(`register_option_row\\("${key}", nullptr, \\w+\\);`));
  // Not hidden behind a developer, advanced or release gate.
  assert.doesNotMatch(tab, /developer_mode|BBL_RELEASE_TO_PUBLIC|advanced/i);
  const section = prefs.slice(prefs.indexOf('add_tab("other"'), prefs.indexOf('add_tab("lan_drop"'));
  assert.doesNotMatch(section, /#if/);
  // The switch starts or stops polling at once.
  assert.match(prefs, /if \(param == LanModelDrop::kEnabledConfigKey\)\s*LanModelDrop::apply_settings\(\);/);
}

function menuItems(mainFrame) {
  assert.match(mainFrame, /append_menu_item\(fileMenu, wxID_ANY, _L\("LAN model drop"\) \+ dots,[\s\S]{0,200}?LanModelDrop::open_preferences_section\(\)/);
  assert.match(mainFrame, /append_menu_item\(fileMenu, wxID_ANY, _L\("Invite someone to send a model"\) \+ dots,[\s\S]{0,200}?LanModelDrop::open_invite_dialog\(this\)/);
}

function paletteEntries(index, palette) {
  assert.match(index, /PageLanDrop/);
  assert.match(index, /L\("Developer Tools"\), L\("LAN model drop"\),/);
  for (const key of ['lan_drop_enabled', 'lan_drop_invite', 'lan_drop_status', 'lan_drop_address', 'lan_drop_station_key', 'lan_drop_code'])
    assert.match(index, new RegExp(`\\{"${key}", L\\("[^"]+"\\), L\\("[^"]+"\\), PageLanDrop\\}`));
  assert.match(index, /\{"docs\/features\/application-integration\/lan-model-drop\.md", L\("LAN model drop"\)\}/);
  assert.match(palette, /_L\("Invite someone to send a model"\)[\s\S]{0,400}?LanModelDrop::open_invite_dialog\(frame\)/);
  assert.match(palette, /_L\("LAN model drop settings"\)[\s\S]{0,300}?LanModelDrop::open_preferences_section\(\)/);
}

function indicator(topbar) {
  assert.match(topbar, /AddTool\(ID_LAN_DROP,/);
  const visibility = body(topbar, 'void BBLTopbar::apply_lan_drop_visibility()');
  assert.match(visibility, /LanModelDrop::view\(\)\.enabled/);
  assert.match(visibility, /GetSizerItem\(\)->Show\(shown\)/);
  // Every Realize goes through the helper that hides the indicator again while the option is off.
  assert.equal(code(topbar).match(/\bRealize\(\);/g).length, 1);
  assert.match(body(topbar, 'void BBLTopbar::realize_with_hidden_items()'), /Realize\(\);\s*apply_lan_drop_visibility\(\);/);
  assert.match(body(topbar, 'void BBLTopbar::OnLanDropIndicator('), /CallAfter\([\s\S]*LanModelDrop::open_invite_dialog/);
  assert.match(body(topbar, 'BBLTopbar::~BBLTopbar()'), /LanModelDrop::remove_listener\(m_lan_drop_listener\)/);
}

function protectedKey(keyStore, station, cmake, ui) {
  const windows = keyStore.slice(keyStore.indexOf('#ifdef _WIN32\n\nbool key_store_available'), keyStore.indexOf('#else'));
  assert.match(windows, /::CryptProtectData\(&input, [^,]+, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output\)/);
  assert.match(windows, /::CryptUnprotectData\(&input, nullptr, &entropy, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output\)/);
  assert.doesNotMatch(windows, /CRYPTPROTECT_LOCAL_MACHINE/);
  assert.match(windows, /::SecureZeroMemory\(output\.pbData, output\.cbData\);/);
  assert.match(keyStore, /#pragma comment\(lib, "Crypt32\.lib"\)/);
  assert.match(cmake, /iphlpapi\.lib user32\.lib version\.lib Crypt32\.lib\)/);
  // Elsewhere there is no plain-text fallback.
  const other = keyStore.slice(keyStore.indexOf('#else'));
  assert.match(other, /KeyStoreResult save_station_key\(const std::filesystem::path &, const std::string &\) \{ return KeyStoreResult::Unavailable; \}/);
  // The key never goes into the application settings: only the three settings keys are written.
  for (const s of [station, keyStore, ui]) {
    for (const call of code(s).matchAll(/->set(?:_bool)?\(([^,]+),/g))
      assert.match(call[1].trim(), /^(kEnabledConfigKey|kAddressConfigKey|kInviteIpv4ConfigKey)$/, call[0]);
  }
  assert.doesNotMatch(station, /"lan_drop_station_key"/);
}

function keyNeverLogged(sources) {
  for (const s of sources) {
    for (const line of s.split('\n').filter((l) => /BOOST_LOG|wxLog|printf|std::cerr|std::cout/.test(l)))
      assert.doesNotMatch(line, /key|authorization|Bearer|secret/i, line.trim());
  }
  const exchange = body(sources[0], 'Exchange exchange(const WorkerConfig &cfg');
  assert.match(exchange, /\.verbose\(false\)/);
  assert.match(exchange, /\.follow_redirects\(false\)/);
  assert.match(exchange, /\.clear_headers\(\)\s*\.header\("Authorization", authorization\)/);
  assert.match(exchange, /wipe\(authorization\);/);
}

function workerThreadHttp(station) {
  const plain = code(station);
  // Every request is made by exchange(), and exchange() is only called from worker functions.
  assert.equal((plain.match(/Http::(get|post|del)\(/g) || []).length, 3);
  assert.equal((plain.match(/\.perform_sync\(\)/g) || []).length, 1);
  assert.doesNotMatch(plain, /\.perform\(\)/);
  const workerOnly = ['download(', 'check_station(', 'test_station(', 'flush_deletes(', 'void worker_main()'];
  const callers = [...plain.matchAll(/\bexchange\(cfg,/g)].map((m) => m.index);
  for (const at of callers) {
    const before = plain.slice(0, at);
    const owner = workerOnly.map((sig) => [sig, before.lastIndexOf(sig)]).sort((a, b) => b[1] - a[1])[0];
    const gui = ['void apply_settings()', 'void test_connection()', 'void request_new_code()', 'void open_item(', 'void discard_item('];
    const lastGui = Math.max(...gui.map((sig) => before.lastIndexOf(sig)));
    assert.ok(owner[1] > lastGui, `exchange() called outside the worker near offset ${at}`);
  }
  assert.match(plain, /g_thread = std::thread\(worker_main\);/);
  // Results reach windows only through CallAfter, and only while the main frame is alive.
  const post = body(station, 'void post(std::function<void()> fn)');
  assert.match(post, /wxTheApp->CallAfter\(/);
  assert.match(post, /if \(!alive->load\(\) \|\| !gui_alive\(\)\) return;/);
  assert.match(body(station, 'bool gui_alive()'), /wxGetApp\(\)\.mainframe != nullptr/);
  const worker = body(station, 'void worker_main()');
  assert.doesNotMatch(code(worker), /wxGetApp\(\)|notifications\(\)/);
  for (const call of code(worker).matchAll(/\bon_(received|refused|given_up|link_state|test_result|code_result)\(/g)) {
    const before = code(worker).slice(0, call.index);
    assert.ok(before.lastIndexOf('post([') > before.lastIndexOf(';'), 'a GUI callback outside post()');
  }
}

function shutdownStops(station, app) {
  const stop = body(station, 'void shutdown()');
  const order = ['g_alive->store(false);', 'g_stopping.store(true);', 'g_shared.stop = true;', 'g_shared.wake.notify_all();', 'g_thread.join();'];
  let last = -1;
  for (const step of order) {
    const at = stop.indexOf(step);
    assert.ok(at > last, `shutdown step out of order: ${step}`);
    last = at;
  }
  const appShutdown = body(app, 'void GUI_App::shutdown()');
  assert.ok(appShutdown.indexOf('LanModelDrop::shutdown();') > appShutdown.indexOf('if (m_is_recreating_gui) return;'));
  assert.ok(appShutdown.indexOf('LanModelDrop::shutdown();') < appShutdown.indexOf('set_closing(true);'));
  assert.match(body(app, 'int GUI_App::OnExit()'), /LanModelDrop::shutdown\(\);/);
  assert.match(body(app, 'void GUI_App::post_init()'), /LanModelDrop::start_after_startup\(\);/);
  // Transfers in flight are cancelled when stopping.
  assert.match(body(station, 'bool operator()() const'), /if \(g_stopping\.load\(\)\) return true;/);
}

function nothingAutomatic(station) {
  const plain = code(station);
  assert.equal((plain.match(/load_files\(/g) || []).length, 1);
  assert.match(body(station, 'void open_item(const std::string &id)'), /plater->load_files\(files\);/);
  assert.doesNotMatch(plain, /reslice|export_gcode|send_gcode|print_job|start_print|background_process|load_project\(|select_view_3D|PrintJob/);
  // open_item runs only from the notification's Open link: one call besides its declaration and
  // its definition.
  const openCalls = [...plain.matchAll(/(?<!void )\bopen_item\(/g)];
  assert.equal(openCalls.length, 1);
  const push = body(station, 'void push_waiting_notification(const Waiting &w)');
  assert.match(push, /_u8L\("Open"\)[\s\S]*wxGetApp\(\)\.CallAfter\(\[id\] \{ open_item\(id\); \}\)/);
  assert.match(push, /_u8L\("Discard"\)[\s\S]*discard_item\(id\)/);
  // Open, Discard and failed checks remove the file from the drop site.
  assert.match(body(station, 'void open_item(const std::string &id)'), /decide\(id\);/);
  assert.match(body(station, 'void discard_item(const std::string &id)'), /decide\(id\);/);
  assert.match(body(station, 'void worker_main()'), /Download::Kind::Refused\) \{[\s\S]*?tracker\.queue_delete\(item\.id\);/);
}

function invite(ui, station) {
  // The link and the QR code come from the same string.
  const refresh = body(ui, 'void InvitePanel::refresh()');
  assert.match(refresh, /wxString::FromUTF8\(v\.link\)/);
  assert.match(refresh, /m_qr->SetText\(has_link \? v\.link : std::string\(\)\)/);
  assert.match(ui, /LocalSecurityUI::PairingQr::encode_text\(text\)/);
  assert.match(body(ui, 'void QrView::render()'), /\*wxWHITE_BRUSH[\s\S]*\*wxBLACK_BRUSH/);
  assert.match(body(ui, 'void InvitePanel::copy_link()'), /wxTheClipboard->SetData\(new wxTextDataObject\(wxString::FromUTF8\(link\)\)\)/);
  assert.match(body(ui, 'void InvitePanel::announce(const wxString &text)'), /wxACC_EVENT_OBJECT_NAMECHANGE/);
  assert.match(body(station, 'void recompute_invite()'), /choose_invite_base\(v\.public_url, address\.value_or\(BaseAddress\{\}\), gui\(\)\.lan_candidates, preferred\)/);
  assert.match(body(station, 'void recompute_invite()'), /invite_link\(v\.invite\.base, v\.drop_code\)/);
  // Kit widgets only.
  assert.doesNotMatch(code(ui), /new wx(Button|StaticText|TextCtrl|CheckBox|Choice|ComboBox|BitmapButton|HyperlinkCtrl|RadioButton|SpinCtrl|Slider|ListBox)\b/);
}

const contracts = [
  ['the option has its own visible Preferences section', () => visibleSection(src.prefs), () => visibleSection(src.prefs.replace('add_tab("lan_drop"', 'if (developer_mode) add_tab("lan_drop"'))],
  ['the File menu opens the section and the invite', () => menuItems(src.mainFrame), () => menuItems(src.mainFrame.replace('_L("Invite someone to send a model") + dots', '_L("Invite") + dots'))],
  ['the command palette lists the section, every row and the invite', () => paletteEntries(src.paletteIndex, src.palette), () => paletteEntries(src.paletteIndex.replace('"lan_drop_station_key"', '"lan_drop_key"'), src.palette)],
  ['the title bar shows an indicator only while the option is on', () => indicator(src.topbar), () => indicator(src.topbar.replace('void BBLTopbar::SetHistoryInfo(const wxString& branch, const wxString& head)\n{', 'void BBLTopbar::SetHistoryInfo(const wxString& branch, const wxString& head)\n{\n    Realize();'))],
  ['the station key is protected with DPAPI and never stored in plain text', () => protectedKey(src.keyStore, src.station, src.cmake, src.ui), () => protectedKey(src.keyStore.replace('CRYPTPROTECT_UI_FORBIDDEN, &output))\n        return KeyStoreResult::Failed;\n    std::vector', 'CRYPTPROTECT_LOCAL_MACHINE, &output))\n        return KeyStoreResult::Failed;\n    std::vector'), src.station, src.cmake, src.ui)],
  ['the station key is never logged', () => keyNeverLogged([src.station, src.keyStore, src.ui, src.model]), () => keyNeverLogged([src.station.replace('BOOST_LOG_TRIVIAL(info) << "LAN model drop: received item "', 'BOOST_LOG_TRIVIAL(info) << cfg.key << "LAN model drop: received item "'), src.keyStore, src.ui, src.model])],
  ['all HTTP runs on the worker thread and reaches windows through CallAfter', () => workerThreadHttp(src.station), () => workerThreadHttp(src.station.replace('void test_connection()\n{', 'void test_connection()\n{\n    exchange(cfg, Method::Get, std::string(), 0, 0, CancelWhen{});'))],
  ['shutdown cancels, wakes and joins the worker before windows go', () => shutdownStops(src.station, src.app), () => shutdownStops(src.station, src.app.replace('    LanModelDrop::shutdown();\n    Schedule::Scheduler::instance().shutdown();', '    Schedule::Scheduler::instance().shutdown();'))],
  ['nothing is opened, sliced or printed automatically', () => nothingAutomatic(src.station), () => nothingAutomatic(src.station.replace('post([item, file = d.file] { on_received(item, file); });', 'post([item, file = d.file] { on_received(item, file); open_item(item.id); });'))],
  ['the invite shows exactly one link as text and as a QR code, with kit widgets only', () => invite(src.ui, src.station), () => invite(src.ui.replace('m_qr->SetText(has_link ? v.link : std::string())', 'm_qr->SetText(v.invite.base)'), src.station)],
];

for (const [name, check, mutant] of contracts) {
  test(name, () => check());
  test(`${name}: the contract rejects a mutated source`, () => assert.throws(mutant));
}

test('the notification manager closes one LAN model drop notification by its text', () => {
  const close = body(src.notifications, 'void NotificationManager::close_lan_model_drop_notification(const std::string& text)');
  assert.match(close, /NotificationType::LanModelDropReceived && notification->compare_text\(text\)/);
  // Duration 0 keeps the file waiting for a choice; the type allows several at once.
  assert.match(body(src.notifications, 'void NotificationManager::push_lan_model_drop_notification('), /NotificationLevel::ImportantNotificationLevel, 0,/);
});

test('the station header documents the GUI-thread contract and the settings keys', () => {
  assert.match(src.stationHpp, /constexpr const char \*kEnabledConfigKey\s+= "lan_drop_enabled";/);
  assert.match(src.stationHpp, /constexpr const char \*kAddressConfigKey\s+= "lan_drop_address";/);
  assert.match(src.stationHpp, /Every function below is\s*\n\/\/ called on the GUI thread\./);
});
