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
  files: read('src/slic3r/GUI/LanModelDrop/LanModelDropFiles.cpp'),
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
  // Straight to the drop site, never through a proxy named in the environment.
  assert.match(exchange, /\.no_proxy\(\)/);
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
  // A process that ends without shutdown() never destroys a joinable thread (std::terminate).
  assert.match(station, /~WorkerThread\(\)\s*\{\s*if \(thread\.joinable\(\)\) thread\.detach\(\);\s*\}/);
  assert.match(station, /std::thread\s+&g_thread = g_worker\.thread;/);
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
  // A link clicked while the application closes does nothing.
  assert.match(body(station, 'void open_item(const std::string &id)'), /^\{\s*\/\/[^\n]*\n\s*if \(!gui_alive\(\)\) return;/);
  assert.match(body(station, 'void discard_item(const std::string &id)'), /^\{\s*if \(!gui_alive\(\) \|\|/);
  // Open, Discard and failed checks remove the file from the drop site that listed it.
  assert.match(body(station, 'void open_item(const std::string &id)'), /decide\(id, base\);/);
  assert.match(body(station, 'void discard_item(const std::string &id)'), /decide\(id, base\);/);
  assert.match(body(station, 'void worker_main()'), /Download::Kind::Refused\) \{[\s\S]*?tracker\.queue_delete\(item\.id, base\);/);
}

// A received folder that waits is never downloaded into again, and one that was opened belongs to
// the user: the station never writes into it, never brings its marker back and never removes it.
// Each delete goes only to the site that listed the item, and pending deletes are kept on disk.
function receivedFolders(station, files) {
  const worker = body(station, 'void worker_main()');
  const fresh = worker.slice(worker.indexOf('for (const InboxItem &item : tracker.fresh(listing.items))'));
  const check = fresh.indexOf('received_state(cfg.received_root, item.id)');
  assert.ok(check > 0 && check < fresh.indexOf('download(cfg, item, polling)'), 'the folder is checked before the download');
  assert.match(fresh, /if \(local == ReceivedState::Waiting\) continue;/);
  assert.match(fresh, /if \(local == ReceivedState::Opened\) \{\s*tracker\.queue_delete\(item\.id, base\);\s*continue;\s*\}/);
  assert.match(body(station, 'Download download(const WorkerConfig &cfg'),
    /write_received\(cfg\.received_root, item\.id, item\.file_name, item\.type, e\.body, result\.file\)/);
  // The station itself never writes a file or makes a path from a sender's name.
  assert.doesNotMatch(code(station), /ofstream|u8path\((item\.)?file_name\)/);
  assert.match(body(station, 'void open_item(const std::string &id)'), /mark_opened\(file\);/);
  assert.doesNotMatch(code(station), /remove_all\(/);
  // Deletes are tied to their site and survive a restart.
  const flush = body(station, 'void flush_deletes(const WorkerConfig &cfg');
  assert.match(flush, /tracker\.pending_deletes\(base\)/);
  assert.match(flush, /tracker\.delete_confirmed\(id, base\)/);
  assert.match(worker, /load_pending_deletes\(cfg\.pending_file\)/);
  assert.match(worker, /save_pending_deletes\(cfg\.pending_file, tracker\.all_pending_deletes\(\)\)/);
  // On disk: an existing folder is never written into; the bytes go to the station's own name first.
  const write = body(files, 'WriteResult write_received(');
  assert.match(write, /case ReceivedState::Waiting:\s*case ReceivedState::Opened: return WriteResult::Exists;/);
  assert.match(write, /if \(!fs::create_directory\(folder, ec\)\) return ec \? WriteResult::Failed : WriteResult::Exists;/);
  assert.match(write, /write_file\(incoming, bytes\)/);
  assert.ok(write.indexOf('write_file(incoming, bytes)') < write.indexOf('is_clean_file_name(file_name)'));
  assert.match(body(files, 'void remove_waiting('), /if \(received_state\(root, id\) != ReceivedState::Waiting\) return;/);
}

// Bilingual mode: the decorator writes English and Cantonese into a label, so a live label is never
// compared with what it shows. Each keeps the English it was given; an unchanged poll sets nothing
// and lays nothing out, and the station tells the windows only when a poll changed the view.
function liveLabels(ui, station) {
  const plain = code(ui);
  assert.equal((plain.match(/GetUnwrappedLabel\(\)/g) || []).length, 1, 'read once, when the LiveText is made');
  assert.match(body(ui, 'LiveText::LiveText(Label *label)'), /m_english = m_label->GetUnwrappedLabel\(\);/);
  assert.doesNotMatch(plain, /GetLabel\(\)\s*[!=]=|[!=]=\s*[\w>.-]*GetLabel\(\)/);
  const set = body(ui, 'bool LiveText::set(const wxString &english)');
  assert.match(set, /if \(m_label == nullptr \|\| english == m_english\) return false;/);
  assert.match(set, /I18N::refresh_bilingual_decoration\(m_label\);/);
  // Only LiveText::set and the QR view's fixed accessible name set label text.
  assert.equal((plain.match(/\bSetLabel\(/g) || []).length, 2);
  assert.doesNotMatch(plain, /\bset_wrapped\(/);
  for (const name of ['m_pending_note', 'm_manual', 'm_source_note', 'm_fixed_note', 'm_live', 'm_waiting_label'])
    assert.match(read('src/slic3r/GUI/LanModelDrop/LanModelDropUi.hpp'), new RegExp(`LiveText\\s+${name};`));
  // A tooltip the decorator extends is set only when its English changes.
  assert.match(body(ui, 'class CodeRow'), /if \(tip != m_tip\) \{\s*m_tip = tip;\s*m_new->SetToolTip\(tip\);/);
  const link = body(station, 'void on_link_state(std::uint64_t generation');
  assert.match(link, /const View before = v;/);
  assert.match(link, /if \(!same_poll_view\(before, v\)\) notify_listeners\(\);/);
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
  ['nothing is opened, sliced or printed automatically', () => nothingAutomatic(src.station), () => nothingAutomatic(src.station.replace('post([item, file = d.file, base] { on_received(item, file, base); });', 'post([item, file = d.file, base] { on_received(item, file, base); open_item(item.id); });'))],
  ['the invite shows exactly one link as text and as a QR code, with kit widgets only', () => invite(src.ui, src.station), () => invite(src.ui.replace('m_qr->SetText(has_link ? v.link : std::string())', 'm_qr->SetText(v.invite.base)'), src.station)],
  ['an opened or waiting received folder is never written again, and deletes go only to their own site', () => receivedFolders(src.station, src.files), () => receivedFolders(src.station.replace('if (local == ReceivedState::Opened) {', 'if (false) {'), src.files)],
  ['an opened received folder is never written again: the on-disk rule', () => receivedFolders(src.station, src.files), () => receivedFolders(src.station, src.files.replace('case ReceivedState::Waiting:\n    case ReceivedState::Opened: return WriteResult::Exists;', 'case ReceivedState::Waiting:\n    case ReceivedState::Opened: break;'))],
  ['deletes go only to the site that listed the item', () => receivedFolders(src.station, src.files), () => receivedFolders(src.station.replace('tracker.pending_deletes(base)', 'tracker.pending_deletes(tracker.all_pending_deletes().front().base)'), src.files)],
  ['live labels keep their English, so bilingual text holds between polls', () => liveLabels(src.ui, src.station), () => liveLabels(src.ui.replace('if (m_label == nullptr || english == m_english) return false;', 'if (m_label == nullptr || m_label->GetUnwrappedLabel() == english) return false;'), src.station)],
  ['an unchanged poll does not refresh the windows', () => liveLabels(src.ui, src.station), () => liveLabels(src.ui, src.station.replace('if (!same_poll_view(before, v)) notify_listeners();', 'notify_listeners();'))],
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
