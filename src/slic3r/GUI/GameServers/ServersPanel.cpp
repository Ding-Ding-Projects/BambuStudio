#include "ServersPanel.hpp"

#include "ServerModel.hpp"
#include "ServerSystem.hpp"

#include "../DocsBrowserDialog.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../GUI_Utils.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../MsgDialog.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/CheckBox.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/LabeledCheckBox.hpp"
#include "../Widgets/MD3DataView.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/SuperConfirmGate.hpp"
#include "../Widgets/TextArea.hpp"
#include "../Widgets/TextInput.hpp"

#include "libslic3r/Utils.hpp"

#include <wx/dcbuffer.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/scrolwin.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/timer.h>
#include <wx/utils.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <deque>
#include <filesystem>
#include <map>
#include <random>
#include <set>
#include <thread>

namespace Slic3r { namespace GUI { namespace GameServers {

namespace fs = std::filesystem;

namespace {

constexpr const char *kGuide = "docs/features/windows/minecraft-servers.md";

enum : std::uint32_t {
    GlyphServer   = 0xE875, // dns
    GlyphAdd      = 0xE145,
    GlyphFind     = 0xE2DB, // travel_explore
    GlyphPlay     = 0xE037,
    GlyphStop     = 0xEF71, // stop_circle
    GlyphRestart  = 0xF053, // restart_alt
    GlyphBackup   = 0xE864,
    GlyphFolder   = 0xE2C8,
    GlyphRemove   = 0xE872,
    GlyphHelp     = 0xE887,
    GlyphCancel   = 0xE5C9,
    GlyphRefresh  = 0xE5D5,
    GlyphDice     = 0xEB40, // casino
    GlyphUp       = 0xE5D8,
    GlyphDown     = 0xE5DB,
    GlyphDownload = 0xF090,
    GlyphPower    = 0xE8AC,
    GlyphRun      = 0xE5C8, // arrow_forward
};

wxColour role(MD3::Role r) { return StateColor::semantic(r); }
wxString u8(const std::string &s) { return wxString::FromUTF8(s); }
// " · ", built from UTF-8 so it never depends on the system code page.
wxString dot() { return wxString::FromUTF8("  \xC2\xB7  "); }
std::string leaf(const std::string &path) { return fs::u8path(path).filename().u8string(); }

// A model message, translated and filled in.
wxString say(const Message &m)
{
    if (m.empty()) return wxString();
    return u8(fill(into_u8(wxGetTranslation(u8(m.text))), m.values));
}
wxString tr(const std::string &english) { return english.empty() ? wxString() : wxGetTranslation(u8(english)); }

Button *button(wxWindow *parent, const wxString &text, std::uint32_t glyph, Button::Variant variant = Button::Variant::Outlined)
{
    auto *b = new Button(parent, text);
    b->SetVariant(variant);
    b->SetButtonSize(Button::Size::Small);
    if (glyph) b->SetGlyph(glyph);
    return b;
}

Label *text(wxWindow *parent, const wxFont &font, const wxString &t, bool muted = false)
{
    auto *l = new Label(parent, font, t);
    l->SetForegroundColour(role(muted ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface));
    return l;
}

ComboBox *choice(wxWindow *parent, const std::vector<wxString> &items, int selected, int width, bool read_only = true)
{
    auto *c = new ComboBox(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(width, -1), 0, nullptr, read_only ? wxCB_READONLY : 0);
    for (const wxString &i : items) c->Append(i);
    if (selected >= 0 && selected < int(items.size())) c->SetSelection(selected);
    return c;
}

wxString size_text(std::uint64_t bytes)
{
    const double b = double(bytes);
    if (b >= 1024.0 * 1024 * 1024) return wxString::Format("%.1f GB", b / (1024.0 * 1024 * 1024));
    if (b >= 1024.0 * 1024) return wxString::Format("%.0f MB", b / (1024.0 * 1024));
    if (b >= 1024.0) return wxString::Format("%.0f KB", b / 1024.0);
    return wxString::Format("%llu B", (unsigned long long) bytes);
}

wxString when_text(std::time_t t)
{
    if (t <= 0) return wxString::FromUTF8("\xE2\x80\x93");
    return wxDateTime(t).Format("%Y-%m-%d %H:%M");
}

wxString duration_text(std::time_t seconds)
{
    if (seconds < 60) return wxString::Format(_L("%d s"), int(seconds));
    const long long m = seconds / 60, h = m / 60;
    if (h == 0) return wxString::Format(_L("%lld min"), m);
    return wxString::Format(_L("%lld h %lld min"), h, m % 60);
}

std::string now_stamp()
{
    return into_u8(wxDateTime::Now().Format("%Y-%m-%d %H-%M-%S"));
}

// A dialog with the kit caption and a body sizer under it.
class Dialog : public DPIDialog
{
public:
    Dialog(wxWindow *parent, const wxString &title) : DPIDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxRESIZE_BORDER)
    {
        SetBackgroundColour(role(MD3::Role::Surface));
        root = new wxBoxSizer(wxVERTICAL);
        root->Add(new MD3DialogCaption(this, title), 0, wxEXPAND);
        body = new wxBoxSizer(wxVERTICAL);
        root->Add(body, 1, wxEXPAND | wxALL, FromDIP(18));
        SetSizer(root);
    }
    void finish(const wxString &ok_label, bool ok_enabled = true)
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->AddStretchSpacer(1);
        auto *cancel = button(this, _L("Cancel"), 0);
        cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
        ok = button(this, ok_label, 0, Button::Variant::Filled);
        ok->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_OK); });
        ok->Enable(ok_enabled);
        row->Add(cancel, 0, wxRIGHT, FromDIP(8));
        row->Add(ok, 0);
        root->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(18));
        Fit();
        MD3DialogCaption::FinishChrome(this);
        CenterOnParent();
        wxGetApp().UpdateDlgDarkUI(this);
    }
    wxBoxSizer *root = nullptr, *body = nullptr;
    Button     *ok   = nullptr;

protected:
    void on_dpi_changed(const wxRect &) override { Layout(); }
};

// ---------------------------------------------------------------------------
// The runtime: servers keep running while the page is closed.

struct RunState
{
    std::unique_ptr<ServerProcess> process;
    enum class Phase { Stopped, Starting, Running, Stopping } phase = Phase::Stopped;
    std::deque<std::string>                          console;
    std::uint64_t                                    total_lines = 0; // every line ever added, for incremental views
    std::deque<std::pair<std::time_t, std::string>> events;          // what is worth seeing at a glance
    std::set<std::string>                            online;
    std::time_t                                      started = 0;
    bool                                             restart_after = false;
    bool                                             backing_up    = false;
    std::function<void()>                            after_saved;
    std::time_t                                      next_restart_at = 0;
    bool                                             warned = false;
    std::time_t                                      backup_retry_at = 0; // after a failed automatic backup
    std::string                                      java;
    int                                              port = 0;
};

class Live;
class LiveTimer : public wxTimer
{
public:
    void Notify() override;
};

class Live
{
public:
    static Live &get()
    {
        static Live live;
        return live;
    }

    Library                         library;
    std::map<std::string, RunState> runs;
    bool                            closing = false;
    std::vector<JavaInstall>        java;
    bool                            java_read = false;
    Manifest                        manifest;
    std::time_t                     manifest_read = 0;

    void load()
    {
        if (m_loaded) return;
        m_loaded = true;
        std::string text;
        if (read_file((fs::u8path(data_dir()) / "minecraft-servers.json").u8string(), text)) from_json(text, library);
        if (read_file(manifest_cache(), text, 32 * 1024 * 1024)) parse_manifest(text, manifest); // the list as last read
        m_timer = new LiveTimer();
        m_timer->Start(30 * 1000);
    }
    void save()
    {
        write_file_atomic((fs::u8path(data_dir()) / "minecraft-servers.json").u8string(), to_json(library));
    }
    std::string manifest_cache() const { return (fs::u8path(data_dir()) / "minecraft-version-list.json").u8string(); }

    const std::vector<JavaInstall> &javas(bool again = false)
    {
        if (!java_read || again) {
            java      = find_java();
            java_read = true;
        }
        return java;
    }

    int  add_listener(std::function<void(const std::string &, bool)> fn) { m_listeners[++m_next] = std::move(fn); return m_next; }
    void remove_listener(int id) { m_listeners.erase(id); }
    void notify(const std::string &id, bool console_only)
    {
        auto copy = m_listeners;
        for (auto &l : copy) l.second(id, console_only);
    }

    void record(const std::string &server, const std::string &action, const std::string &detail, const std::string &outcome)
    {
        library.record({std::time(nullptr), server, action, detail, outcome});
        save();
    }

    void event(RunState &r, const std::string &what)
    {
        r.events.push_back({std::time(nullptr), what});
        while (r.events.size() > 200) r.events.pop_front();
    }

    void add_line(const std::string &id, const std::string &line)
    {
        if (closing) return;
        RunState &r = runs[id];
        r.console.push_back(line);
        ++r.total_lines;
        while (r.console.size() > 3000) r.console.pop_front();
        const LogEvent e     = parse_log_line(line);
        bool           state = false;
        switch (e.kind) {
        case LogEvent::Kind::Started:
            r.phase = RunState::Phase::Running;
            event(r, into_u8(wxString::Format(_L("Started in %.1f s"), e.seconds)));
            state = true;
            break;
        case LogEvent::Kind::Joined:
            r.online.insert(e.player);
            event(r, into_u8(wxString::Format(_L("%s joined"), u8(e.player))));
            state = true;
            break;
        case LogEvent::Kind::Left:
            r.online.erase(e.player);
            event(r, into_u8(wxString::Format(_L("%s left"), u8(e.player))));
            state = true;
            break;
        case LogEvent::Kind::PlayerList:
            r.online = std::set<std::string>(e.players.begin(), e.players.end());
            state    = true;
            break;
        case LogEvent::Kind::Saved:
            if (r.after_saved) {
                auto then     = std::move(r.after_saved);
                r.after_saved = nullptr;
                then();
            }
            break;
        case LogEvent::Kind::Stopping:
            if (r.phase != RunState::Phase::Stopped) r.phase = RunState::Phase::Stopping;
            state = true;
            break;
        case LogEvent::Kind::PortInUse: event(r, into_u8(_L("The port is in use by another program"))); state = true; break;
        case LogEvent::Kind::EulaNeeded: event(r, into_u8(_L("The server asks for the EULA to be accepted"))); state = true; break;
        case LogEvent::Kind::Advancement: event(r, e.text); state = true; break;
        case LogEvent::Kind::Error: event(r, into_u8(wxString::Format(_L("Error: %s"), u8(e.text)))); state = true; break;
        default: break;
        }
        notify(id, !state);
    }

    void exited(const std::string &id, int code)
    {
        if (closing) return;
        RunState &r     = runs[id];
        const bool again = r.restart_after;
        r.phase         = RunState::Phase::Stopped;
        r.online.clear();
        r.restart_after = false;
        r.after_saved   = nullptr;
        r.backing_up    = false;
        event(r, into_u8(wxString::Format(_L("Stopped (exit code %d)"), code)));
        const ServerRecord *s = library.find(id);
        record(s ? s->name : id, "Stop", code == 0 ? "" : "exit code " + std::to_string(code), code == 0 ? "Done" : "Failed");
        notify(id, false);
        if (again) {
            std::string error;
            if (!start(id, error)) {
                event(runs[id], into_u8(wxString::Format(_L("Could not start again: %s"), u8(error))));
                notify(id, false);
            }
        }
    }

    // Starts a server after every check; false with what stopped it.
    bool start(const std::string &id, std::string &error, LaunchCheck *check_out = nullptr)
    {
        ServerRecord *s = library.find(id);
        if (!s) { error = into_u8(_L("That server is no longer in the list.")); return false; }
        RunState &r = runs[id];
        if (r.process && r.process->running()) { error = into_u8(_L("The server is already running.")); return false; }
        if (!processes_supported()) { error = into_u8(_L("Running a server needs Windows in this build.")); return false; }
        const ServerFolder f = inspect_folder(s->folder);
        if (f.world_locked) { error = into_u8(_L("Another program has this server's world open. Stop it there first.")); return false; }
        ServerVersion v;
        v.id           = f.info.version.empty() ? s->version : f.info.version;
        v.data_version = f.info.data_version;
        const int needed = java_needed(v, f.info.java_version);
        LaunchSettings launch = s->launch;
        if (launch.jar.empty() || !fs::exists(fs::u8path(s->folder) / fs::u8path(launch.jar))) launch.jar = f.jar;
        const std::vector<JavaInstall> &installs = javas();
        launch.java = s->java_choice == "auto" || s->java_choice.empty() ? pick_java(installs, needed) : s->java_choice;
        launch.java_version = 0;
        for (const JavaInstall &j : installs)
            if (j.path == launch.java) launch.java_version = j.feature;
        int port = 25565;
        if (auto p = f.properties.get("server-port"); p && !p->empty()) port = std::atoi(p->c_str());
        const bool        jar_exists = !launch.jar.empty() && fs::exists(fs::u8path(s->folder) / fs::u8path(launch.jar));
        const LaunchCheck check      = check_launch(launch, needed, f.eula.accepted, jar_exists, port_free(port), port, physical_memory_mb());
        if (check_out) *check_out = check;
        if (!check.ok()) {
            error.clear();
            for (const Message &m : check.errors) error += (error.empty() ? "" : "\n") + into_u8(say(m));
            return false;
        }
        r.process = std::make_unique<ServerProcess>();
        r.console.push_back(into_u8(wxString::Format(_L("-- Starting with %s --"), u8(launch.java))));
        ++r.total_lines;
        const std::string server_id = id;
        std::string       start_error;
        const bool        ok = r.process->start(
            launch_arguments(launch), s->folder,
            [server_id](const std::string &line) {
                wxTheApp->CallAfter([server_id, line]() { Live::get().add_line(server_id, line); });
            },
            [server_id](int code) { wxTheApp->CallAfter([server_id, code]() { Live::get().exited(server_id, code); }); }, &start_error);
        if (!ok) {
            r.process.reset();
            error = start_error;
            record(s->name, "Start", start_error, "Failed");
            return false;
        }
        r.phase           = RunState::Phase::Starting;
        r.started         = std::time(nullptr);
        r.java            = launch.java;
        r.port            = port;
        r.online.clear();
        r.next_restart_at = next_restart(s->schedule, std::time(nullptr));
        r.warned          = false;
        s->launch.java         = launch.java;
        s->launch.java_version = launch.java_version;
        s->launch.jar          = launch.jar;
        if (!f.info.version.empty()) s->version = f.info.version;
        s->flavour = flavour_name(f.info.flavour);
        event(r, into_u8(_L("Starting")));
        record(s->name, "Start", launch.java, "Done");
        notify(id, false);
        return true;
    }

    void stop(const std::string &id, bool restart)
    {
        RunState &r = runs[id];
        if (!r.process || !r.process->running()) return;
        r.restart_after = restart;
        r.phase         = RunState::Phase::Stopping;
        r.process->send("stop");
        event(r, into_u8(restart ? _L("Restarting") : _L("Stopping safely")));
        notify(id, false);
    }

    void kill(const std::string &id)
    {
        RunState &r = runs[id];
        if (r.process && r.process->running()) {
            r.restart_after = false;
            r.process->kill();
            event(r, into_u8(_L("Force stopped")));
            notify(id, false);
        }
    }

    bool running(const std::string &id) const
    {
        auto it = runs.find(id);
        return it != runs.end() && it->second.process && it->second.process->running();
    }

    bool send(const std::string &id, const std::string &command)
    {
        auto it = runs.find(id);
        if (it == runs.end() || !it->second.process || !it->second.process->running()) return false;
        it->second.console.push_back("> " + command);
        ++it->second.total_lines;
        const bool ok = it->second.process->send(command);
        notify(id, true);
        return ok;
    }

    std::string backup_folder(const ServerRecord &s) const
    {
        if (library.backup_folder.empty()) return (fs::u8path(s.folder) / "backups").u8string();
        return (fs::u8path(library.backup_folder) / fs::u8path(safe_file_name(s.name))).u8string();
    }

    // A backup while running: saving off, everything written, the zip, saving on again.
    void backup(const std::string &id, bool automatic, std::function<void(std::uint64_t, std::uint64_t)> progress,
                std::function<void(const ZipOutcome &)> done)
    {
        ServerRecord *s = library.find(id);
        RunState     &r = runs[id];
        if (!s || r.backing_up) {
            ZipOutcome busy;
            busy.error = into_u8(_L("A backup of this server is already being made."));
            if (done) done(busy);
            return;
        }
        r.backing_up              = true;
        const std::string folder  = s->folder;
        const std::string name    = s->name;
        const std::string target  = (fs::u8path(backup_folder(*s)) / fs::u8path(backup_file_name(name, std::time(nullptr), automatic))).u8string();
        const int         keep    = s->schedule.backup_keep;
        const bool        live    = running(id);
        auto              zip_now = [this, id, folder, name, target, automatic, keep, live, progress, done]() {
            const ServerFolder f       = inspect_folder(folder);
            const auto         entries = backup_entries(f.level_name, f.entries);
            std::thread([this, id, folder, name, target, automatic, keep, live, progress, done, entries]() {
                const ZipOutcome out = write_zip(folder, entries, target, [progress](std::uint64_t a, std::uint64_t b) {
                    if (progress) wxTheApp->CallAfter([progress, a, b]() { progress(a, b); });
                });
                wxTheApp->CallAfter([this, id, name, target, automatic, keep, live, done, out]() {
                    if (closing) return;
                    RunState &r  = runs[id];
                    r.backing_up = false;
                    if (!out.ok && automatic) r.backup_retry_at = std::time(nullptr) + 600;
                    if (live && running(id)) send(id, "save-on");
                    if (ServerRecord *s = library.find(id); s && out.ok) s->last_backup = std::time(nullptr);
                    if (out.ok && automatic) {
                        std::vector<std::string> autos;
                        for (const BackupFile &b : list_backups(fs::u8path(target).parent_path().u8string()))
                            if (b.automatic) autos.push_back(b.name);
                        for (const std::string &old : backups_to_prune(autos, keep)) {
                            std::error_code ec;
                            fs::remove(fs::u8path(target).parent_path() / fs::u8path(old), ec);
                        }
                    }
                    event(r, out.ok ? into_u8(wxString::Format(_L("Backup made, %s, checked"), size_text(out.bytes))) : into_u8(wxString::Format(_L("Backup failed: %s"), u8(out.error))));
                    record(name, automatic ? "Automatic backup" : "Backup", out.ok ? leaf(target) + ", " + into_u8(size_text(out.bytes)) : out.error,
                           out.ok ? "Done" : (out.cancelled ? "Cancelled" : "Failed"));
                    notify(id, false);
                    if (done) done(out);
                });
            }).detach();
        };
        if (live) {
            send(id, "save-off");
            r.after_saved = zip_now;
            send(id, "save-all flush");
        } else {
            zip_now();
        }
    }

    // Every 30 seconds: due backups and daily restarts.
    void tick()
    {
        if (closing) return;
        const std::time_t now = std::time(nullptr);
        for (ServerRecord &s : library.servers) {
            RunState &r = runs[s.id];
            if (s.schedule.backup_on && !r.backing_up && now >= r.backup_retry_at) {
                // The first automatic backup is one period after the schedule starts.
                if (s.last_backup == 0) s.last_backup = now;
                const std::time_t due = next_backup(s.schedule, s.last_backup, now);
                if (due > 0 && due <= now) backup(s.id, true, {}, {});
            }
            if (s.schedule.restart_on && running(s.id) && r.phase == RunState::Phase::Running) {
                if (r.next_restart_at == 0) r.next_restart_at = next_restart(s.schedule, now);
                const int warn = std::clamp(s.schedule.warn_minutes, 0, 30);
                if (!r.warned && warn > 0 && now >= r.next_restart_at - warn * 60) {
                    send(s.id, "say " + into_u8(wxString::Format(_L("The server restarts in %d minutes."), warn)));
                    r.warned = true;
                }
                if (now >= r.next_restart_at) {
                    r.next_restart_at = next_restart(s.schedule, now + 60);
                    r.warned          = false;
                    record(s.name, "Scheduled restart", "", "Done");
                    stop(s.id, true);
                }
            }
        }
    }

    void shutdown()
    {
        if (closing) return;
        closing = true;
        if (m_timer) {
            m_timer->Stop();
            delete m_timer;
            m_timer = nullptr;
        }
        m_listeners.clear();
        std::vector<ServerProcess *> waiting;
        for (auto &[id, r] : runs)
            if (r.process && r.process->running()) {
                r.process->send("stop");
                waiting.push_back(r.process.get());
            }
        // Each server saves its world as it stops; give them a little time.
        for (int i = 0; i < 100 && std::any_of(waiting.begin(), waiting.end(), [](ServerProcess *p) { return p->running(); }); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        // A server still stopping finishes on its own; its process object is
        // let go now rather than during static destruction.
        for (auto &[id, r] : runs)
            if (r.process) {
                if (r.process->running()) (void) r.process.release();
                else r.process.reset();
            }
        save();
    }

private:
    Live() = default;
    bool                                                          m_loaded = false;
    std::map<int, std::function<void(const std::string &, bool)>> m_listeners;
    int                                                           m_next = 0;
    LiveTimer                                                    *m_timer = nullptr; // deleted at shutdown, never at static destruction
};

void LiveTimer::Notify() { Live::get().tick(); }

// ---------------------------------------------------------------------------
// Dialogs

// Create a server: the version from the Mojang list, the folder, memory, the
// port and the EULA, accepted here by the person or not at all.
class CreateDialog : public Dialog
{
public:
    CreateDialog(wxWindow *parent, const Manifest &manifest) : Dialog(parent, _L("Create a Minecraft server"))
    {
        auto *lead = text(this, Label::Body_13, _L("Downloads the official server file from Mojang, checks its SHA-1, and sets up a new folder. Nothing outside that folder changes."), true);
        lead->Wrap(FromDIP(600));
        body->Add(lead, 0, wxBOTTOM, FromDIP(12));
        auto *grid = new wxFlexGridSizer(2, FromDIP(8), FromDIP(12));
        grid->AddGrowableCol(1, 1);
        const auto row = [&](const wxString &label, wxWindow *control) {
            grid->Add(text(this, Label::Body_13, label), 0, wxALIGN_CENTER_VERTICAL);
            grid->Add(control, 1, wxEXPAND);
        };
        m_name = new TextInput(this, _L("My server"), wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(320), -1));
        row(_L("Name"), m_name);
        std::vector<wxString> versions;
        for (const ManifestVersion &v : manifest.versions)
            if (v.type == "release") {
                versions.push_back(u8(v.id));
                m_ids.push_back(v.id);
            }
        m_version = choice(this, versions, 0, FromDIP(200));
        row(_L("Version"), m_version);
        const wxString start = u8((fs::u8path(into_u8(wxStandardPaths::Get().GetDocumentsDir())) / "Minecraft servers").u8string());
        auto *where = new wxBoxSizer(wxHORIZONTAL);
        m_parent = new TextInput(this, start, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(320), -1));
        auto *browse = button(this, _L("Browse..."), GlyphFolder);
        browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            wxDirDialog d(this, _L("Where the server folder goes"), m_parent->GetTextCtrl()->GetValue());
            if (d.ShowModal() == wxID_OK) m_parent->GetTextCtrl()->SetValue(d.GetPath());
        });
        where->Add(m_parent, 1, wxRIGHT, FromDIP(8));
        where->Add(browse, 0);
        grid->Add(text(this, Label::Body_13, _L("In the folder")), 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(where, 1, wxEXPAND);
        const std::uint64_t phys = physical_memory_mb();
        const int           max  = int(std::clamp<std::uint64_t>(phys / 4 / 512 * 512, 2048, 8192));
        m_memory = new SpinInput(this, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(120), -1), 0, 512, 65536, max);
        row(_L("Most memory (MB)"), m_memory);
        m_port = new SpinInput(this, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(120), -1), 0, 1, 65535, std::max(1, free_port_near(25565)));
        row(_L("Port"), m_port);
        body->Add(grid, 0, wxEXPAND);
        auto *eula_row = new wxBoxSizer(wxHORIZONTAL);
        m_eula = new LabeledCheckBox(this, _L("I accept the Minecraft EULA for this server"));
        auto *read = button(this, _L("Read the EULA"), 0, Button::Variant::Text);
        read->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { wxLaunchDefaultBrowser("https://aka.ms/MinecraftEULA"); });
        eula_row->Add(m_eula, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
        eula_row->Add(read, 0, wxALIGN_CENTER_VERTICAL);
        body->Add(eula_row, 0, wxTOP, FromDIP(12));
        m_note = text(this, Label::Body_12, wxEmptyString, true);
        body->Add(m_note, 0, wxTOP, FromDIP(8));
        finish(_L("Create"), false);
        const auto update = [this](wxCommandEvent &e) { refresh(); e.Skip(); };
        m_eula->Bind(wxEVT_CHECKBOX, update);
        m_name->GetTextCtrl()->Bind(wxEVT_TEXT, update);
        m_parent->GetTextCtrl()->Bind(wxEVT_TEXT, update);
        refresh();
    }

    std::string name() const { return into_u8(m_name->GetTextCtrl()->GetValue()).empty() ? "server" : into_u8(m_name->GetTextCtrl()->GetValue()); }
    std::string version() const { const int i = m_version->GetSelection(); return i >= 0 && i < int(m_ids.size()) ? m_ids[std::size_t(i)] : ""; }
    std::string folder() const { return (fs::u8path(into_u8(m_parent->GetTextCtrl()->GetValue())) / fs::u8path(safe_file_name(name()))).u8string(); }
    int         memory() const { return m_memory->GetValue(); }
    int         port() const { return m_port->GetValue(); }

private:
    void refresh()
    {
        std::error_code ec;
        const bool      exists = fs::exists(fs::u8path(folder()), ec) && !fs::is_empty(fs::u8path(folder()), ec);
        m_note->SetLabel(exists ? _L("That folder already has files in it; choose another name.")
                                : wxString::Format(_L("The server goes into %s."), u8(folder())));
        ok->Enable(m_eula->GetValue() && !exists && !version().empty());
        Layout();
    }
    TextInput               *m_name = nullptr, *m_parent = nullptr;
    ComboBox                *m_version = nullptr;
    SpinInput               *m_memory = nullptr, *m_port = nullptr;
    LabeledCheckBox         *m_eula = nullptr;
    Label                   *m_note = nullptr;
    std::vector<std::string> m_ids;
};

// Found server folders, each to adopt or not. Adopting changes nothing in the folder.
class AdoptDialog : public Dialog
{
public:
    AdoptDialog(wxWindow *parent, const std::vector<std::string> &found, const Library &library) : Dialog(parent, _L("Adopt servers"))
    {
        auto *lead = text(this, Label::Body_13, _L("Found servers stay untouched: adopting one adds it to this list and changes nothing in its folder."), true);
        lead->Wrap(FromDIP(600));
        body->Add(lead, 0, wxBOTTOM, FromDIP(12));
        for (const std::string &f : found) {
            const bool known = std::any_of(library.servers.begin(), library.servers.end(), [&](const ServerRecord &s) { return s.folder == f; });
            const ServerFolder info = inspect_folder(f);
            wxString label = u8(f) + dot() + u8(info.jar);
            if (!info.info.version.empty()) label << dot() << u8(flavour_name(info.info.flavour)) << " " << u8(info.info.version);
            if (known) label << "  " << _L("(already in the list)");
            auto *box = new LabeledCheckBox(this, label);
            box->SetValue(!known);
            box->Enable(!known);
            body->Add(box, 0, wxBOTTOM, FromDIP(6));
            m_boxes.push_back({f, box});
        }
        if (found.empty()) body->Add(text(this, Label::Body_13, _L("No server folders were found there.")), 0);
        finish(_L("Adopt"), !found.empty());
    }
    std::vector<std::string> chosen() const
    {
        std::vector<std::string> out;
        for (const auto &[f, box] : m_boxes)
            if (box->IsEnabled() && box->GetValue()) out.push_back(f);
        return out;
    }

private:
    std::vector<std::pair<std::string, LabeledCheckBox *>> m_boxes;
};

// The area a world generator run covers, with its batches; done batches fill in.
class AreaPreview : public wxPanel
{
public:
    explicit AreaPreview(wxWindow *parent) : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(parent->FromDIP(300), parent->FromDIP(300)))
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, [this](wxPaintEvent &) { paint(); });
    }
    void set(const GeneratorSettings &g, std::size_t done)
    {
        m_g    = g;
        m_done = done;
        Refresh();
    }

private:
    void paint()
    {
        wxAutoBufferedPaintDC dc(this);
        const wxSize          sz = GetClientSize();
        dc.SetBackground(wxBrush(role(MD3::Role::SurfaceContainerLow)));
        dc.Clear();
        const int w = std::max(1, m_g.width_chunks), d = std::max(1, m_g.depth_chunks);
        const int pad = FromDIP(12);
        const double cell = std::min(double(sz.x - 2 * pad) / w, double(sz.y - 2 * pad) / d);
        const int ox = int((sz.x - cell * w) / 2), oz = int((sz.y - cell * d) / 2);
        if (m_g.width_chunks <= 0 || m_g.depth_chunks <= 0) {
            dc.SetTextForeground(role(MD3::Role::OnSurfaceVariant));
            dc.SetFont(Label::Body_12);
            const wxString t = _L("Only the spawn area");
            const wxSize   ts = dc.GetTextExtent(t);
            dc.DrawText(t, (sz.x - ts.x) / 2, (sz.y - ts.y) / 2);
            return;
        }
        // Batches made so far, filled.
        const std::vector<ChunkBatch> batches = plan_batches(m_g);
        const int x1 = batches.empty() ? 0 : batches.front().chunk_x1, z1 = batches.empty() ? 0 : batches.front().chunk_z1;
        wxColour fill = role(MD3::Role::PrimaryContainer);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(fill));
        for (std::size_t i = 0; i < batches.size() && i < m_done; ++i) {
            const ChunkBatch &b = batches[i];
            dc.DrawRectangle(ox + int((b.chunk_x1 - x1) * cell), oz + int((b.chunk_z1 - z1) * cell), int(std::ceil((b.chunk_x2 - b.chunk_x1 + 1) * cell)),
                             int(std::ceil((b.chunk_z2 - b.chunk_z1 + 1) * cell)));
        }
        // Fine lines every 4 chunks, strong lines at the batch edges.
        dc.SetPen(wxPen(role(MD3::Role::OutlineVariant), 1));
        if (cell * 4 >= 3)
            for (int i = 0; i <= w; i += 4) dc.DrawLine(ox + int(i * cell), oz, ox + int(i * cell), oz + int(d * cell));
        if (cell * 4 >= 3)
            for (int i = 0; i <= d; i += 4) dc.DrawLine(ox, oz + int(i * cell), ox + int(w * cell), oz + int(i * cell));
        dc.SetPen(wxPen(role(MD3::Role::Outline), std::max(1, FromDIP(2))));
        for (int i = 0; i <= w; i += 16) dc.DrawLine(ox + int(i * cell), oz, ox + int(i * cell), oz + int(d * cell));
        for (int i = 0; i <= d; i += 16) dc.DrawLine(ox, oz + int(i * cell), ox + int(w * cell), oz + int(i * cell));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(ox, oz, int(w * cell) + 1, int(d * cell) + 1);
        // Spawn, the centre of the area.
        dc.SetBrush(wxBrush(role(MD3::Role::Primary)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawCircle(ox + int(w * cell / 2), oz + int(d * cell / 2), std::max(3, FromDIP(4)));
    }
    GeneratorSettings m_g;
    std::size_t       m_done = 0;
};

} // namespace

// ---------------------------------------------------------------------------
// The page

struct ServersPanel::Ui
{
    ServersPanel         *self = nullptr;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
    int                   listener = 0;
    std::string           selected;
    std::uint64_t         console_shown = 0;
    std::string           console_for;
    std::vector<std::string> history_cmds;
    int                   history_at = -1;
    bool                  busy = false;
    std::shared_ptr<std::atomic<bool>> cancel = std::make_shared<std::atomic<bool>>(false);

    // Shell
    Label               *system_line = nullptr;
    MD3DataViewListCtrl *list = nullptr;
    wxPanel             *detail_page = nullptr; // not "detail": MSVC finds boost and nlohmann detail namespaces from lambdas
    Label               *title = nullptr, *status = nullptr;
    Button              *b_start = nullptr, *b_stop = nullptr, *b_restart = nullptr, *b_kill = nullptr, *b_backup = nullptr, *b_folder = nullptr,
           *b_remove = nullptr;
    std::vector<Button *> tabs;
    wxSimplebook        *book = nullptr;
    wxPanel             *busy_bar = nullptr;
    Label               *busy_what = nullptr, *busy_step = nullptr;
    Button              *busy_cancel = nullptr;

    // Overview
    Label               *ov_state = nullptr, *ov_players = nullptr, *ov_memory = nullptr, *ov_up = nullptr, *ov_version = nullptr, *ov_java = nullptr,
          *ov_backup = nullptr, *ov_restart = nullptr;
    MD3DataViewListCtrl *ov_events = nullptr;
    wxTimer              ov_timer;
    std::string          ov_mark; // which events the list shows

    // Console
    TextArea   *console = nullptr;
    TextInput  *command = nullptr;

    // Properties
    wxScrolledWindow *props = nullptr;
    wxBoxSizer       *props_sizer = nullptr;
    TextInput        *props_search = nullptr;
    Label            *props_note = nullptr, *props_unknown = nullptr;
    struct PropRow
    {
        const PropertySpec *spec = nullptr;
        wxWindow           *row = nullptr;
        std::function<std::string()> get;
    };
    std::vector<PropRow>                  prop_rows;
    std::vector<std::pair<wxWindow *, PropertyGroup>> prop_headers;
    std::map<std::string, std::string>    prop_loaded;
    std::map<std::string, std::string>    prop_undo; // server id -> the file before the last save

    // Players
    int                  players_list = 0; // allowlist, operators, bans
    std::vector<Button *> players_chips;
    MD3DataViewListCtrl *players = nullptr;
    Label               *players_online = nullptr, *players_note = nullptr;
    SpinInput           *players_level = nullptr;

    // Worlds
    MD3DataViewListCtrl *worlds = nullptr;
    std::vector<std::string> world_names;

    // Versions
    int                  versions_kind = 0; // releases, snapshots, old
    std::vector<Button *> versions_chips;
    MD3DataViewListCtrl *versions = nullptr;
    Label               *versions_note = nullptr;
    std::vector<std::string> version_ids;

    // Backups
    MD3DataViewListCtrl   *backups = nullptr;
    Label                 *backups_where = nullptr;
    std::vector<BackupFile> backup_files;

    // Schedule
    LabeledCheckBox *sc_backup = nullptr, *sc_restart = nullptr;
    SpinInput       *sc_hours = nullptr, *sc_keep = nullptr, *sc_hour = nullptr, *sc_minute = nullptr, *sc_warn = nullptr;
    Label           *sc_next = nullptr;

    // World generator
    GeneratorSettings gen;
    Label           *gen_version = nullptr, *gen_summary = nullptr, *gen_problems = nullptr, *gen_progress = nullptr;
    TextInput       *gen_seed = nullptr, *gen_output = nullptr;
    ComboBox        *gen_preset = nullptr, *gen_mode = nullptr, *gen_difficulty = nullptr, *gen_biome = nullptr, *gen_write = nullptr,
             *gen_block = nullptr;
    LabeledCheckBox *gen_structures = nullptr, *gen_hardcore = nullptr, *gen_lakes = nullptr, *gen_features = nullptr, *gen_eula = nullptr;
    SpinInput       *gen_width = nullptr, *gen_depth = nullptr, *gen_cx = nullptr, *gen_cz = nullptr, *gen_height = nullptr;
    MD3DataViewListCtrl *gen_layers = nullptr;
    wxPanel         *gen_flat = nullptr;
    std::vector<std::pair<std::string, LabeledCheckBox *>> gen_sets;
    wxScrolledWindow *gen_rules = nullptr;
    std::map<std::string, std::function<std::string()>> gen_rule_get;
    std::string      gen_rules_for = "\x01"; // the version the rule list was built for
    AreaPreview     *gen_preview = nullptr;
    Button          *gen_go = nullptr, *gen_stop = nullptr;
    struct Run
    {
        std::unique_ptr<ServerProcess> process;
        std::string temp, output, server;
        bool        zip = false;
        enum class Step { Starting, Batches, Saving, Stopping, Packaging, Done, Failed, Cancelled } step = Step::Starting;
        std::vector<ChunkBatch> batches;
        std::size_t batch = 0, probe = 0;
        std::vector<std::string> probes;
        std::time_t batch_started = 0, probe_failed_at = 0, cancel_at = 0;
        int         ready = 0;
        std::deque<std::string> tail;
    };
    std::unique_ptr<Run> gen_run;
    wxTimer              gen_timer;

    // History
    MD3DataViewListCtrl *history = nullptr;

    // ------------------------------------------------------------------
    ServerRecord *server() { return Live::get().library.find(selected); }

    void build();
    void build_overview(wxWindow *page);
    void build_console(wxWindow *page);
    void build_properties(wxWindow *page);
    void build_players(wxWindow *page);
    void build_worlds(wxWindow *page);
    void build_versions(wxWindow *page);
    void build_backups(wxWindow *page);
    void build_schedule(wxWindow *page);
    void build_generator(wxWindow *page);
    void build_history(wxWindow *page);

    void refresh_list();
    void refresh_detail();
    void selection_changed();
    void refresh_overview();
    void refresh_console(bool reload);
    void load_properties();
    void filter_properties();
    void save_properties();
    void refresh_players();
    void refresh_worlds();
    void refresh_versions();
    void refresh_backups();
    void refresh_schedule();
    void refresh_generator_version();
    void read_generator();
    void refresh_history();
    void show_tab(int i);

    void create_server();
    void adopt();
    void start();
    void stop(bool restart);
    void force_stop();
    void backup_now();
    void restore();
    void delete_backup();
    void remove_server();
    void read_manifest();
    void switch_version();
    void players_add();
    void players_remove();
    void players_set_level();
    void generate();
    void generation_line(const std::string &line);
    void generation_tick();
    void generation_exit(int code);
    void generation_finish(bool ok, const wxString &why);
    void set_busy(bool on, const wxString &what = wxString());
};

void ServersPanel::Ui::set_busy(bool on, const wxString &what)
{
    busy = on;
    if (on) *cancel = false;
    busy_bar->Show(on);
    busy_what->SetLabel(what);
    busy_step->SetLabel(wxEmptyString);
    self->Layout();
}

void ServersPanel::Ui::build()
{
    wxWindow *p   = self;
    const int pad = p->FromDIP(18);
    auto     *root = new wxBoxSizer(wxVERTICAL);

    auto *head = new wxBoxSizer(wxHORIZONTAL);
    auto *titles = new wxBoxSizer(wxVERTICAL);
    titles->Add(text(p, Label::Head_20, _L("Minecraft servers")), 0);
    auto *sub = text(p, Label::Body_13, _L("Create, adopt and run Java servers on this PC. Every setting has its own control."), true);
    titles->Add(sub, 0, wxTOP, p->FromDIP(2));
    head->Add(titles, 1, wxALIGN_CENTER_VERTICAL);
    const auto head_button = [&](const wxString &label, std::uint32_t glyph, Button::Variant v, std::function<void()> fn) {
        auto *b = button(p, label, glyph, v);
        b->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
        head->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, p->FromDIP(8));
        return b;
    };
    head_button(_L("Find and adopt..."), GlyphFind, Button::Variant::Outlined, [this]() { adopt(); });
    head_button(_L("Create server..."), GlyphAdd, Button::Variant::Filled, [this]() { create_server(); });
    head_button(_L("How it works"), GlyphHelp, Button::Variant::Text, []() { DocsBrowserDialog::ShowArticle(wxGetApp().mainframe, kGuide); });
    root->Add(head, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);
    system_line = text(p, Label::Body_12, wxEmptyString, true);
    root->Add(system_line, 0, wxLEFT | wxRIGHT | wxTOP, pad);

    auto *main = new wxBoxSizer(wxHORIZONTAL);
    list = new MD3DataViewListCtrl(p, wxID_ANY, wxDefaultPosition, wxSize(p->FromDIP(300), -1), wxDV_ROW_LINES | wxDV_SINGLE);
    list->AppendTextColumn(_L("Server"), wxDATAVIEW_CELL_INERT, p->FromDIP(150), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    list->AppendTextColumn(_L("State"), wxDATAVIEW_CELL_INERT, p->FromDIP(140), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &) {
        const int row = list->GetSelectedRow();
        const auto &servers = Live::get().library.servers;
        const std::string now = row >= 0 && row < int(servers.size()) ? servers[std::size_t(row)].id : "";
        if (now == selected) return;
        selected = now;
        selection_changed();
    });
    main->Add(list, 0, wxEXPAND | wxRIGHT, p->FromDIP(14));

    detail_page = new wxPanel(p);
    detail_page->SetBackgroundColour(role(MD3::Role::Surface));
    auto *d = new wxBoxSizer(wxVERTICAL);
    title  = text(detail_page, Label::Head_16, wxEmptyString);
    status = text(detail_page, Label::Body_12, wxEmptyString, true);
    d->Add(title, 0);
    d->Add(status, 0, wxTOP, p->FromDIP(2));
    auto *actions = new wxWrapSizer(wxHORIZONTAL);
    const auto act = [&](const wxString &label, std::uint32_t glyph, Button::Variant v, std::function<void()> fn) {
        auto *b = button(detail_page, label, glyph, v);
        b->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
        actions->Add(b, 0, wxRIGHT | wxTOP, p->FromDIP(8));
        return b;
    };
    b_start   = act(_L("Start"), GlyphPlay, Button::Variant::Filled, [this]() { start(); });
    b_stop    = act(_L("Stop safely"), GlyphStop, Button::Variant::Outlined, [this]() { stop(false); });
    b_restart = act(_L("Restart"), GlyphRestart, Button::Variant::Outlined, [this]() { stop(true); });
    b_kill    = act(_L("Force stop..."), GlyphPower, Button::Variant::Danger, [this]() { force_stop(); });
    b_backup  = act(_L("Back up now"), GlyphBackup, Button::Variant::Outlined, [this]() { backup_now(); });
    b_folder  = act(_L("Open folder"), GlyphFolder, Button::Variant::Text, [this]() {
        if (ServerRecord *s = server()) wxLaunchDefaultApplication(u8(s->folder));
    });
    b_remove = act(_L("Remove from list..."), GlyphRemove, Button::Variant::Text, [this]() { remove_server(); });
    d->Add(actions, 0, wxEXPAND | wxTOP, p->FromDIP(4));

    auto *tab_row = new wxWrapSizer(wxHORIZONTAL);
    const wxString names[] = {_L("Overview"), _L("Console"), _L("Properties"), _L("Players"), _L("Worlds"), _L("Versions"),
                              _L("Backups"), _L("Schedule"), _L("World generator"), _L("History")};
    for (int i = 0; i < 10; ++i) {
        auto *b = button(detail_page, names[i], 0, Button::Variant::Text);
        b->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent &) { show_tab(i); });
        tab_row->Add(b, 0, wxRIGHT | wxTOP, p->FromDIP(4));
        tabs.push_back(b);
    }
    d->Add(tab_row, 0, wxEXPAND | wxTOP, p->FromDIP(10));
    book = new wxSimplebook(detail_page);
    book->SetBackgroundColour(role(MD3::Role::Surface));
    const auto page = [&](void (Ui::*fn)(wxWindow *)) {
        auto *pg = new wxPanel(book);
        pg->SetBackgroundColour(role(MD3::Role::Surface));
        (this->*fn)(pg);
        book->AddPage(pg, wxEmptyString);
    };
    page(&Ui::build_overview);
    page(&Ui::build_console);
    page(&Ui::build_properties);
    page(&Ui::build_players);
    page(&Ui::build_worlds);
    page(&Ui::build_versions);
    page(&Ui::build_backups);
    page(&Ui::build_schedule);
    page(&Ui::build_generator);
    page(&Ui::build_history);
    d->Add(book, 1, wxEXPAND | wxTOP, p->FromDIP(10));
    detail_page->SetSizer(d);
    main->Add(detail_page, 1, wxEXPAND);
    root->Add(main, 1, wxEXPAND | wxALL, pad);

    busy_bar = new wxPanel(p);
    busy_bar->SetBackgroundColour(role(MD3::Role::SurfaceContainer));
    auto *bb = new wxBoxSizer(wxHORIZONTAL);
    busy_what = text(busy_bar, Label::Head_13, wxEmptyString);
    busy_step = text(busy_bar, Label::Body_12, wxEmptyString, true);
    busy_cancel = button(busy_bar, _L("Cancel"), GlyphCancel, Button::Variant::Text);
    busy_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { *cancel = true; busy_step->SetLabel(_L("Cancelling...")); });
    bb->Add(busy_what, 0, wxALIGN_CENTER_VERTICAL | wxALL, p->FromDIP(10));
    bb->Add(busy_step, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, p->FromDIP(10));
    bb->Add(busy_cancel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, p->FromDIP(10));
    busy_bar->SetSizer(bb);
    busy_bar->Hide();
    root->Add(busy_bar, 0, wxEXPAND);
    self->SetSizer(root);
}

void ServersPanel::Ui::show_tab(int i)
{
    for (std::size_t k = 0; k < tabs.size(); ++k) tabs[k]->SetVariant(int(k) == i ? Button::Variant::Tonal : Button::Variant::Text);
    book->SetSelection(std::size_t(i));
    switch (i) {
    case 1: refresh_console(true); break;
    case 2: load_properties(); break;
    case 3: refresh_players(); break;
    case 4: refresh_worlds(); break;
    case 5: refresh_versions(); break;
    case 6: refresh_backups(); break;
    case 7: refresh_schedule(); break;
    case 8: refresh_generator_version(); break;
    case 9: refresh_history(); break;
    default: refresh_overview();
    }
    detail_page->Layout();
}

void ServersPanel::Ui::refresh_list()
{
    Live &live = Live::get();
    list->DeleteAllItems();
    int select = -1;
    for (std::size_t i = 0; i < live.library.servers.size(); ++i) {
        const ServerRecord &s = live.library.servers[i];
        const auto          it = live.runs.find(s.id);
        wxString            state = _L("Stopped");
        if (it != live.runs.end()) {
            switch (it->second.phase) {
            case RunState::Phase::Starting: state = _L("Starting"); break;
            case RunState::Phase::Running: state = wxString::Format(_L("Running, %d online"), int(it->second.online.size())); break;
            case RunState::Phase::Stopping: state = _L("Stopping"); break;
            default: break;
            }
        }
        if (!s.version.empty()) state << dot() << u8(s.version);
        wxVector<wxVariant> cells;
        cells.push_back(wxVariant(u8(s.name)));
        cells.push_back(wxVariant(state));
        list->AppendItem(cells);
        if (s.id == selected) select = int(i);
    }
    if (select < 0 && !live.library.servers.empty()) {
        select   = 0;
        selected = live.library.servers.front().id;
    }
    if (select >= 0) list->SelectRow(unsigned(select));
    const auto &java = live.javas();
    wxString    line;
    if (java.empty()) line = _L("No Java was found on this PC. Current versions need Java 21: install it from adoptium.net or any Java vendor, then use Find Java again on a server's Overview.");
    else {
        line = _L("Java here:");
        for (std::size_t i = 0; i < java.size() && i < 4; ++i) line << " " << u8(java[i].version.empty() ? "?" : java[i].version) << (i + 1 < std::min<std::size_t>(java.size(), 4) ? "," : "");
    }
    if (const std::uint64_t mem = physical_memory_mb()) line << dot() << wxString::Format(_L("%llu GB memory"), (unsigned long long) (mem / 1024));
    if (!processes_supported()) line << dot() << _L("Running servers needs Windows in this build.");
    system_line->SetLabel(line);
}

void ServersPanel::Ui::refresh_detail()
{
    ServerRecord *s = server();
    detail_page->Show(s != nullptr);
    if (!s) {
        self->Layout();
        return;
    }
    Live      &live  = Live::get();
    RunState  &r     = live.runs[s->id];
    const bool on    = live.running(s->id);
    title->SetLabel(u8(s->name));
    wxString st = on ? wxString::Format(_L("%s, process %lu"), u8(leaf(r.java)), r.process->pid()) : _L("Stopped");
    if (on) st << dot() << wxString::Format("localhost:%d", r.port) << dot() << wxString::Format(_L("up %s"), duration_text(std::time(nullptr) - r.started));
    st << dot() << u8(s->folder);
    status->SetLabel(st);
    b_start->Enable(!on && !busy);
    b_stop->Enable(on);
    b_restart->Enable(on);
    b_kill->Enable(on);
    b_backup->Enable(!busy && !r.backing_up);
    b_remove->Enable(!on);
    // Only what changes with the server's state; a page being edited is left alone.
    switch (book->GetSelection()) {
    case 0: refresh_overview(); break;
    case 1: refresh_console(false); break;
    case 3: refresh_players(); break;
    default: break;
    }
    detail_page->Layout();
    self->Layout();
}

void ServersPanel::Ui::selection_changed()
{
    refresh_detail();
    show_tab(book->GetSelection() == wxNOT_FOUND ? 0 : book->GetSelection());
}

// ---- Overview ----------------------------------------------------------------

void ServersPanel::Ui::build_overview(wxWindow *page)
{
    auto *s     = new wxBoxSizer(wxVERTICAL);
    auto *cards = new wxWrapSizer(wxHORIZONTAL);
    const auto card = [&](const wxString &caption, Label *&value) {
        auto *c = new wxPanel(page);
        c->SetBackgroundColour(role(MD3::Role::SurfaceContainerLow));
        auto *cs = new wxBoxSizer(wxVERTICAL);
        cs->Add(text(c, Label::Body_12, caption, true), 0, wxLEFT | wxRIGHT | wxTOP, page->FromDIP(10));
        value = text(c, Label::Head_14, wxString::FromUTF8("\xE2\x80\x93"));
        cs->Add(value, 0, wxALL, page->FromDIP(10));
        c->SetSizer(cs);
        c->SetMinSize(wxSize(page->FromDIP(200), -1));
        cards->Add(c, 0, wxRIGHT | wxBOTTOM, page->FromDIP(10));
    };
    card(_L("State"), ov_state);
    card(_L("Players"), ov_players);
    card(_L("Memory"), ov_memory);
    card(_L("Up for"), ov_up);
    card(_L("Version"), ov_version);
    card(_L("Java"), ov_java);
    card(_L("Next backup"), ov_backup);
    card(_L("Next restart"), ov_restart);
    s->Add(cards, 0, wxEXPAND);
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(text(page, Label::Head_13, _L("Recent events")), 1, wxALIGN_CENTER_VERTICAL);
    auto *find = button(page, _L("Find Java again"), GlyphRefresh, Button::Variant::Text);
    find->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        Live::get().javas(true);
        refresh_list();
        refresh_overview();
    });
    row->Add(find, 0, wxRIGHT, page->FromDIP(8));
    auto *log = button(page, _L("Open the full log"), GlyphFolder, Button::Variant::Text);
    log->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (ServerRecord *sv = server()) {
            const fs::path latest = fs::u8path(sv->folder) / "logs" / "latest.log";
            std::error_code ec;
            if (fs::exists(latest, ec)) wxLaunchDefaultApplication(u8(latest.u8string()));
        }
    });
    row->Add(log, 0);
    s->Add(row, 0, wxEXPAND | wxTOP, page->FromDIP(6));
    ov_events = new MD3DataViewListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, page->FromDIP(220)), wxDV_ROW_LINES | wxDV_SINGLE);
    ov_events->AppendTextColumn(_L("When"), wxDATAVIEW_CELL_INERT, page->FromDIP(150));
    ov_events->AppendTextColumn(_L("What"), wxDATAVIEW_CELL_INERT, page->FromDIP(500));
    s->Add(ov_events, 1, wxEXPAND | wxTOP, page->FromDIP(6));
    page->SetSizer(s);
    ov_timer.SetOwner(page);
    page->Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
        if (book->GetSelection() == 0) refresh_overview();
    }, ov_timer.GetId());
    ov_timer.Start(5000);
}

void ServersPanel::Ui::refresh_overview()
{
    ServerRecord *s = server();
    if (!s) return;
    Live      &live = Live::get();
    RunState  &r    = live.runs[s->id];
    const bool on   = live.running(s->id);
    switch (r.phase) {
    case RunState::Phase::Starting: ov_state->SetLabel(_L("Starting")); break;
    case RunState::Phase::Running: ov_state->SetLabel(_L("Running")); break;
    case RunState::Phase::Stopping: ov_state->SetLabel(_L("Stopping")); break;
    default: ov_state->SetLabel(_L("Stopped"));
    }
    wxString players = wxString::Format("%d", int(r.online.size()));
    if (!r.online.empty()) {
        players << dot();
        for (const std::string &n : r.online) players << " " << u8(n);
    }
    ov_players->SetLabel(on ? players : wxString::FromUTF8("\xE2\x80\x93"));
    const std::uint64_t mem = on ? r.process->memory_bytes() : 0;
    ov_memory->SetLabel(on && mem ? wxString::Format(_L("%s of %d GB"), size_text(mem), std::max(1, s->launch.memory_max_mb / 1024)) : wxString::FromUTF8("\xE2\x80\x93"));
    ov_up->SetLabel(on ? duration_text(std::time(nullptr) - r.started) : wxString::FromUTF8("\xE2\x80\x93"));
    ov_version->SetLabel(s->version.empty() ? _L("Not read yet") : u8(s->flavour + " " + s->version));
    ov_java->SetLabel(on ? u8(leaf(fs::u8path(r.java).parent_path().parent_path().u8string())) : (s->java_choice == "auto" ? _L("Picked at start") : u8(s->java_choice)));
    const std::time_t nb = next_backup(s->schedule, s->last_backup, std::time(nullptr));
    ov_backup->SetLabel(s->schedule.backup_on ? when_text(nb) : _L("Not scheduled"));
    ov_restart->SetLabel(s->schedule.restart_on ? when_text(next_restart(s->schedule, std::time(nullptr))) : _L("Not scheduled"));
    const std::string mark = s->id + ":" + std::to_string(r.events.size()) + ":" + (r.events.empty() ? std::string() : std::to_string(r.events.back().first));
    if (mark == ov_mark) return;
    ov_mark = mark;
    ov_events->DeleteAllItems();
    for (auto it = r.events.rbegin(); it != r.events.rend(); ++it) {
        wxVector<wxVariant> cells;
        cells.push_back(wxVariant(when_text(it->first)));
        cells.push_back(wxVariant(u8(it->second)));
        ov_events->AppendItem(cells);
    }
}

// ---- Console -----------------------------------------------------------------

void ServersPanel::Ui::build_console(wxWindow *page)
{
    auto *s = new wxBoxSizer(wxVERTICAL);
    console = new TextArea(page, wxEmptyString, wxDefaultSize, wxTE_READONLY | wxTE_RICH2);
    console->SetMonospace(true);
    s->Add(console, 1, wxEXPAND);
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    command = new TextInput(page, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    command->GetTextCtrl()->SetHint(_L("A server command, such as list or time set day"));
    const auto run = [this]() {
        const std::string cmd = into_u8(command->GetTextCtrl()->GetValue().Trim().Trim(false));
        if (cmd.empty() || !server()) return;
        const std::string plain = cmd[0] == '/' ? cmd.substr(1) : cmd;
        if (!Live::get().send(server()->id, plain)) {
            MessageDialog(self, _L("The server is not running."), _L("Console"), wxOK | wxICON_INFORMATION).ShowModal();
            return;
        }
        history_cmds.push_back(plain);
        history_at = -1;
        command->GetTextCtrl()->Clear();
    };
    command->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [run](wxCommandEvent &) { run(); });
    command->GetTextCtrl()->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
        if ((e.GetKeyCode() == WXK_UP || e.GetKeyCode() == WXK_DOWN) && !history_cmds.empty()) {
            if (e.GetKeyCode() == WXK_UP) history_at = history_at < 0 ? int(history_cmds.size()) - 1 : std::max(0, history_at - 1);
            else history_at = history_at < 0 ? -1 : (history_at + 1 >= int(history_cmds.size()) ? -1 : history_at + 1);
            command->GetTextCtrl()->SetValue(history_at < 0 ? wxString() : u8(history_cmds[std::size_t(history_at)]));
            command->GetTextCtrl()->SetInsertionPointEnd();
            return;
        }
        e.Skip();
    });
    row->Add(command, 1, wxRIGHT, page->FromDIP(8));
    auto *go = button(page, _L("Run"), GlyphRun, Button::Variant::Filled);
    go->Bind(wxEVT_BUTTON, [run](wxCommandEvent &) { run(); });
    row->Add(go, 0, wxRIGHT, page->FromDIP(8));
    for (const char *quick : {"list", "save-all"}) {
        auto *q = button(page, u8(quick), 0, Button::Variant::Text);
        const std::string c = quick;
        q->Bind(wxEVT_BUTTON, [this, c](wxCommandEvent &) {
            if (server()) Live::get().send(server()->id, c);
        });
        row->Add(q, 0, wxRIGHT, page->FromDIP(4));
    }
    s->Add(row, 0, wxEXPAND | wxTOP, page->FromDIP(8));
    s->Add(text(page, Label::Body_12, _L("Commands run as the server console, with every permission. Up and Down bring back earlier commands."), true), 0, wxTOP,
           page->FromDIP(6));
    page->SetSizer(s);
}

void ServersPanel::Ui::refresh_console(bool reload)
{
    ServerRecord *s = server();
    if (!s) return;
    RunState &r = Live::get().runs[s->id];
    if (reload || console_for != s->id) {
        console->Clear();
        console_for   = s->id;
        console_shown = r.total_lines - r.console.size();
    }
    const std::uint64_t first = r.total_lines - r.console.size();
    if (console_shown < first) console_shown = first;
    wxString add;
    for (std::uint64_t i = console_shown; i < r.total_lines; ++i) add << u8(r.console[std::size_t(i - first)]) << "\n";
    console_shown = r.total_lines;
    if (!add.IsEmpty()) {
        console->AppendText(add);
        // Keep the control from growing without end.
        if (console->GetTextCtrl()->GetLastPosition() > 600000) console->GetTextCtrl()->Remove(0, 200000);
    }
}

// ---- Properties --------------------------------------------------------------

void ServersPanel::Ui::build_properties(wxWindow *page)
{
    auto *s   = new wxBoxSizer(wxVERTICAL);
    auto *top = new wxBoxSizer(wxHORIZONTAL);
    props_search = new TextInput(page, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(page->FromDIP(280), -1));
    props_search->GetTextCtrl()->SetHint(_L("Find a setting"));
    props_search->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { filter_properties(); });
    top->Add(props_search, 0, wxRIGHT, page->FromDIP(8));
    top->AddStretchSpacer(1);
    auto *undo = button(page, _L("Undo last save"), 0, Button::Variant::Text);
    undo->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        ServerRecord *sv = server();
        if (!sv || !prop_undo.count(sv->id)) {
            MessageDialog(self, _L("There is no save to undo in this session."), _L("Properties"), wxOK | wxICON_INFORMATION).ShowModal();
            return;
        }
        std::string error;
        if (write_file_atomic((fs::u8path(sv->folder) / "server.properties").u8string(), prop_undo[sv->id], &error)) {
            prop_undo.erase(sv->id);
            Live::get().record(sv->name, "Undo settings", "", "Done");
            load_properties();
            props_note->SetLabel(_L("The last save was undone. A running server picks it up when it restarts."));
        } else {
            MessageDialog(self, u8(error), _L("Properties"), wxOK | wxICON_WARNING).ShowModal();
        }
    });
    top->Add(undo, 0, wxRIGHT, page->FromDIP(8));
    auto *reload = button(page, _L("Reload"), GlyphRefresh, Button::Variant::Text);
    reload->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { load_properties(); });
    top->Add(reload, 0, wxRIGHT, page->FromDIP(8));
    auto *save = button(page, _L("Save changes"), 0, Button::Variant::Filled);
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { save_properties(); });
    top->Add(save, 0);
    s->Add(top, 0, wxEXPAND);
    props_note = text(page, Label::Body_12, wxEmptyString, true);
    s->Add(props_note, 0, wxTOP, page->FromDIP(6));
    props = new MD3ScrolledWindow(page, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    props->SetScrollRate(0, page->FromDIP(12));
    props->SetBackgroundColour(role(MD3::Role::Surface));
    props_sizer = new wxBoxSizer(wxVERTICAL);
    props->SetSizer(props_sizer);
    s->Add(props, 1, wxEXPAND | wxTOP, page->FromDIP(8));
    props_unknown = text(page, Label::Body_12, wxEmptyString, true);
    s->Add(props_unknown, 0, wxTOP, page->FromDIP(6));
    page->SetSizer(s);
}

void ServersPanel::Ui::load_properties()
{
    ServerRecord *sv = server();
    if (!sv) return;
    props->Freeze();
    props_sizer->Clear(true);
    prop_rows.clear();
    prop_headers.clear();
    prop_loaded.clear();
    const ServerFolder f = inspect_folder(sv->folder);
    ServerVersion      v;
    v.id           = f.info.version.empty() ? sv->version : f.info.version;
    v.data_version = f.info.data_version;
    const bool on  = Live::get().running(sv->id);
    if (!f.has_properties)
        props_note->SetLabel(_L("This server has no server.properties yet; it writes one the first time it starts. Values saved here are used from then on."));
    else
        props_note->SetLabel(on ? _L("Settings marked Live change at once while the server runs; the others need a restart.")
                                : _L("Changes are written to server.properties when you save. Lines you did not change stay exactly as they were."));
    const wxString groups[] = {_L("Gameplay"), _L("World"), _L("Players"), _L("Network"), _L("Performance"), _L("Resource packs"), _L("Advanced")};
    const int      w        = props->FromDIP(200);
    for (int g = 0; g <= int(PropertyGroup::Advanced); ++g) {
        auto *header = text(props, Label::Head_13, groups[g]);
        props_sizer->Add(header, 0, wxTOP | wxBOTTOM, props->FromDIP(8));
        prop_headers.push_back({header, PropertyGroup(g)});
        for (const PropertySpec &spec : property_specs()) {
            if (int(spec.group) != g || !property_known_in(spec, v)) continue;
            const std::string value = f.properties.get(spec.key).value_or(spec.def);
            prop_loaded[spec.key]   = value;
            auto *row = new wxPanel(props);
            row->SetBackgroundColour(role(MD3::Role::Surface));
            auto *rs = new wxBoxSizer(wxHORIZONTAL);
            auto *names = new wxBoxSizer(wxVERTICAL);
            auto *label = text(row, Label::Body_13, tr(spec.label));
            names->Add(label, 0);
            names->Add(text(row, Label::Mono_11, u8(spec.key), true), 0);
            rs->Add(names, 0, wxALIGN_CENTER_VERTICAL);
            rs->SetItemMinSize(names, wxSize(props->FromDIP(260), -1));
            PropRow pr;
            pr.spec = &spec;
            pr.row  = row;
            switch (spec.type) {
            case PropertyType::Bool: {
                auto *c = new CheckBox(row);
                c->SetValue(value == "true");
                pr.get = [c]() { return std::string(c->GetValue() ? "true" : "false"); };
                rs->Add(c, 0, wxALIGN_CENTER_VERTICAL);
                break;
            }
            case PropertyType::Enum: {
                std::vector<wxString> items;
                for (const std::string &e : spec.values) items.push_back(u8(e));
                const std::string norm = normalise_enum(spec, value);
                const auto        at   = std::find(spec.values.begin(), spec.values.end(), norm);
                auto             *c    = choice(row, items, at == spec.values.end() ? -1 : int(at - spec.values.begin()), w);
                pr.get = [c, &spec, value]() {
                    const int i = c->GetSelection();
                    return i >= 0 ? spec.values[std::size_t(i)] : value;
                };
                rs->Add(c, 0, wxALIGN_CENTER_VERTICAL);
                break;
            }
            case PropertyType::Int:
            case PropertyType::Port: {
                const long long lo = std::max<long long>(spec.min, -2147483647LL), hi = std::min<long long>(spec.max, 2147483647LL);
                long long       n  = std::atoll(value.c_str());
                auto *c = new SpinInput(row, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(props->FromDIP(150), -1), 0, int(lo), int(hi), int(std::clamp(n, lo, hi)));
                pr.get = [c]() { return std::to_string(c->GetValue()); };
                rs->Add(c, 0, wxALIGN_CENTER_VERTICAL);
                if (spec.key == "server-port") {
                    const bool busy_port = !on && !port_free(int(n));
                    if (busy_port) rs->Add(text(row, Label::Body_12, _L("In use by another program"), true), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, props->FromDIP(8));
                }
                break;
            }
            default: {
                auto *c = new TextInput(row, u8(value), wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(props->FromDIP(320), -1),
                                        spec.type == PropertyType::Password ? wxTE_PASSWORD : 0);
                pr.get = [c]() { return into_u8(c->GetTextCtrl()->GetValue()); };
                rs->Add(c, 0, wxALIGN_CENTER_VERTICAL);
            }
            }
            auto *tag = text(row, Label::Body_12, spec.live_command.empty() ? _L("Needs restart") : _L("Live"), true);
            rs->Add(tag, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, props->FromDIP(12));
            row->SetToolTip(tr(spec.help) + "\n" + wxString::Format(_L("Default: %s"), spec.def.empty() ? _L("(empty)") : u8(spec.def)) +
                            (spec.since.empty() ? wxString() : "\n" + wxString::Format(_L("Since %s"), u8(spec.since))));
            label->SetToolTip(row->GetToolTipText());
            row->SetSizer(rs);
            props_sizer->Add(row, 0, wxEXPAND | wxBOTTOM, props->FromDIP(6));
            prop_rows.push_back(pr);
        }
    }
    std::vector<std::string> unknown;
    for (const std::string &k : f.properties.keys())
        if (!find_property(k)) unknown.push_back(k);
    wxString un;
    if (!unknown.empty()) {
        un = wxString::Format(_L("%d settings this version does not know are kept exactly as written:"), int(unknown.size()));
        for (const std::string &k : unknown) un << " " << u8(k);
    }
    props_unknown->SetLabel(un);
    props->Thaw();
    filter_properties();
}

void ServersPanel::Ui::filter_properties()
{
    const wxString q = props_search->GetTextCtrl()->GetValue().Lower();
    std::set<PropertyGroup> shown;
    for (PropRow &r : prop_rows) {
        const bool match = q.IsEmpty() || tr(r.spec->label).Lower().Contains(q) || u8(r.spec->key).Lower().Contains(q) || tr(r.spec->help).Lower().Contains(q);
        r.row->Show(match);
        if (match) shown.insert(r.spec->group);
    }
    for (auto &[header, group] : prop_headers) header->Show(shown.count(group) > 0);
    props->Layout();
    props->FitInside();
}

void ServersPanel::Ui::save_properties()
{
    ServerRecord *sv = server();
    if (!sv) return;
    const fs::path path = fs::u8path(sv->folder) / "server.properties";
    std::string    before;
    read_file(path.u8string(), before, 4 * 1024 * 1024);
    PropertiesFile file = parse_properties(before);
    std::vector<std::string> changed, live_cmds;
    wxString                 problems;
    for (const PropRow &r : prop_rows) {
        const std::string now = r.get();
        if (now == prop_loaded[r.spec->key]) continue;
        const Message problem = property_problem(*r.spec, now);
        if (!problem.empty()) {
            problems << tr(r.spec->label) << ": " << say(problem) << "\n";
            continue;
        }
        file.set(r.spec->key, now);
        changed.push_back(r.spec->key);
        const std::string cmd = live_command_for(*r.spec, now);
        if (!cmd.empty()) live_cmds.push_back(cmd);
    }
    if (!problems.IsEmpty()) {
        MessageDialog(self, problems, _L("Some values cannot be used"), wxOK | wxICON_WARNING).ShowModal();
        return;
    }
    if (changed.empty()) {
        props_note->SetLabel(_L("Nothing changed."));
        return;
    }
    std::string error;
    if (!write_file_atomic(path.u8string(), write_properties(file), &error)) {
        MessageDialog(self, u8(error), _L("Properties"), wxOK | wxICON_WARNING).ShowModal();
        return;
    }
    prop_undo[sv->id] = before;
    std::string keys;
    for (const std::string &k : changed) keys += (keys.empty() ? "" : ", ") + k;
    Live::get().record(sv->name, "Settings", keys, "Done");
    int restart = int(changed.size());
    if (Live::get().running(sv->id)) {
        for (const std::string &cmd : live_cmds) Live::get().send(sv->id, cmd);
        restart -= int(live_cmds.size());
    }
    load_properties();
    props_note->SetLabel(Live::get().running(sv->id) && restart > 0
                             ? wxString::Format(_L("Saved %d changes; %d of them take effect when the server restarts."), int(changed.size()), restart)
                             : wxString::Format(_L("Saved %d changes."), int(changed.size())));
}

// ---- Players -----------------------------------------------------------------

void ServersPanel::Ui::build_players(wxWindow *page)
{
    auto *s     = new wxBoxSizer(wxVERTICAL);
    auto *chips = new wxBoxSizer(wxHORIZONTAL);
    const wxString names[] = {_L("Allowlist"), _L("Operators"), _L("Bans")};
    for (int i = 0; i < 3; ++i) {
        auto *b = button(page, names[i], 0, i == 0 ? Button::Variant::Tonal : Button::Variant::Text);
        b->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent &) {
            players_list = i;
            for (int k = 0; k < 3; ++k) players_chips[std::size_t(k)]->SetVariant(k == i ? Button::Variant::Tonal : Button::Variant::Text);
            refresh_players();
        });
        chips->Add(b, 0, wxRIGHT, page->FromDIP(6));
        players_chips.push_back(b);
    }
    s->Add(chips, 0);
    players_online = text(page, Label::Body_13, wxEmptyString);
    s->Add(players_online, 0, wxTOP, page->FromDIP(8));
    players = new MD3DataViewListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, page->FromDIP(260)), wxDV_ROW_LINES | wxDV_SINGLE);
    players->AppendTextColumn(_L("Player"), wxDATAVIEW_CELL_INERT, page->FromDIP(180));
    players->AppendTextColumn(_L("Details"), wxDATAVIEW_CELL_INERT, page->FromDIP(420));
    s->Add(players, 1, wxEXPAND | wxTOP, page->FromDIP(8));
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    auto *add = button(page, _L("Add..."), GlyphAdd, Button::Variant::Outlined);
    add->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { players_add(); });
    auto *remove = button(page, _L("Remove"), GlyphRemove, Button::Variant::Outlined);
    remove->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { players_remove(); });
    row->Add(add, 0, wxRIGHT, page->FromDIP(8));
    row->Add(remove, 0, wxRIGHT, page->FromDIP(16));
    row->Add(text(page, Label::Body_13, _L("Operator level")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, page->FromDIP(6));
    players_level = new SpinInput(page, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(page->FromDIP(90), -1), 0, 1, 4, 4);
    row->Add(players_level, 0, wxRIGHT, page->FromDIP(8));
    auto *set = button(page, _L("Set level"), 0, Button::Variant::Text);
    set->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { players_set_level(); });
    row->Add(set, 0);
    s->Add(row, 0, wxTOP, page->FromDIP(8));
    players_note = text(page, Label::Body_12, wxEmptyString, true);
    s->Add(players_note, 0, wxTOP, page->FromDIP(6));
    page->SetSizer(s);
}

void ServersPanel::Ui::refresh_players()
{
    ServerRecord *sv = server();
    if (!sv) return;
    const ServerFolder f  = inspect_folder(sv->folder);
    Live              &live = Live::get();
    const bool         on = live.running(sv->id);
    const RunState    &r  = live.runs[sv->id];
    wxString online = on ? wxString::Format(_L("Online now: %d"), int(r.online.size())) : _L("The server is stopped.");
    for (const std::string &n : r.online) online << "  " << u8(n);
    players_online->SetLabel(online);
    const std::vector<Player> &list = players_list == 0 ? f.allowlist : players_list == 1 ? f.operators : f.bans;
    players->DeleteAllItems();
    for (const Player &p : list) {
        wxString details;
        if (players_list == 1) details = wxString::Format(_L("Level %d: %s"), p.level, tr(operator_level_meaning(p.level)));
        else if (players_list == 2) details = p.reason.empty() ? u8(p.expires) : u8(p.reason) + dot() + u8(p.expires);
        else details = u8(p.uuid);
        wxVector<wxVariant> cells;
        cells.push_back(wxVariant(u8(p.name)));
        cells.push_back(wxVariant(details));
        players->AppendItem(cells);
    }
    if (!f.lists_readable) players_note->SetLabel(_L("A player list file is not valid JSON; it is shown as found and is not rewritten here."));
    else players_note->SetLabel(on ? _L("Adding and removing go through the server, which looks the player up and writes its own lists.")
                                   : _L("Start the server to add players, so it can look them up. Removing works while it is stopped too."));
}

void ServersPanel::Ui::players_add()
{
    ServerRecord *sv = server();
    if (!sv) return;
    if (!Live::get().running(sv->id)) {
        MessageDialog(self, _L("Start the server first: it looks each new player up and writes the list itself."), _L("Players"), wxOK | wxICON_INFORMATION).ShowModal();
        return;
    }
    Dialog d(self, players_list == 0 ? _L("Add to the allowlist") : players_list == 1 ? _L("Make an operator") : _L("Ban a player"));
    auto *name = new TextInput(&d, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(d.FromDIP(260), -1));
    d.body->Add(text(&d, Label::Body_13, _L("Player name")), 0, wxBOTTOM, d.FromDIP(4));
    d.body->Add(name, 0, wxEXPAND);
    TextInput *reason = nullptr;
    if (players_list == 2) {
        d.body->Add(text(&d, Label::Body_13, _L("Reason (optional)")), 0, wxTOP | wxBOTTOM, d.FromDIP(4));
        reason = new TextInput(&d, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(d.FromDIP(260), -1));
        d.body->Add(reason, 0, wxEXPAND);
    }
    d.finish(_L("Add"), false);
    name->GetTextCtrl()->Bind(wxEVT_TEXT, [&](wxCommandEvent &) { d.ok->Enable(valid_player_name(into_u8(name->GetTextCtrl()->GetValue()))); });
    if (d.ShowModal() != wxID_OK) return;
    const std::string n = into_u8(name->GetTextCtrl()->GetValue());
    const std::string cmd = players_list == 0 ? "whitelist add " + n : players_list == 1 ? "op " + n
                                                                                          : "ban " + n + (reason ? " " + into_u8(reason->GetTextCtrl()->GetValue()) : "");
    Live::get().send(sv->id, cmd);
    Live::get().record(sv->name, players_list == 0 ? "Allowlist add" : players_list == 1 ? "Operator add" : "Ban", n, "Done");
    // The server writes its list a moment later.
    std::weak_ptr<bool> guard = alive;
    std::thread([this, guard]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        wxTheApp->CallAfter([this, guard]() {
            if (!guard.expired()) refresh_players();
        });
    }).detach();
}

void ServersPanel::Ui::players_remove()
{
    ServerRecord *sv = server();
    const int     row = players->GetSelectedRow();
    if (!sv || row < 0) return;
    const ServerFolder f    = inspect_folder(sv->folder);
    std::vector<Player> list = players_list == 0 ? f.allowlist : players_list == 1 ? f.operators : f.bans;
    if (row >= int(list.size())) return;
    const std::string n = list[std::size_t(row)].name;
    if (Live::get().running(sv->id)) {
        Live::get().send(sv->id, players_list == 0 ? "whitelist remove " + n : players_list == 1 ? "deop " + n : "pardon " + n);
    } else {
        if (!f.lists_readable) return;
        list.erase(list.begin() + row);
        const char *file = players_list == 0 ? "whitelist.json" : players_list == 1 ? "ops.json" : "banned-players.json";
        const std::string json = players_list == 0 ? write_allowlist(list) : players_list == 1 ? write_operators(list) : write_bans(list);
        std::string error;
        if (!write_file_atomic((fs::u8path(sv->folder) / file).u8string(), json, &error)) {
            MessageDialog(self, u8(error), _L("Players"), wxOK | wxICON_WARNING).ShowModal();
            return;
        }
    }
    Live::get().record(sv->name, players_list == 0 ? "Allowlist remove" : players_list == 1 ? "Operator remove" : "Unban", n, "Done");
    refresh_players();
}

void ServersPanel::Ui::players_set_level()
{
    ServerRecord *sv  = server();
    const int     row = players->GetSelectedRow();
    if (!sv || row < 0 || players_list != 1) {
        players_note->SetLabel(_L("Choose an operator in the Operators list first."));
        return;
    }
    if (Live::get().running(sv->id)) {
        MessageDialog(self, _L("Stop the server to change an operator's level: it keeps the list in memory while it runs."), _L("Players"),
                      wxOK | wxICON_INFORMATION)
            .ShowModal();
        return;
    }
    ServerFolder f = inspect_folder(sv->folder);
    if (!f.lists_readable || row >= int(f.operators.size())) return;
    f.operators[std::size_t(row)].level = players_level->GetValue();
    std::string error;
    if (write_file_atomic((fs::u8path(sv->folder) / "ops.json").u8string(), write_operators(f.operators), &error)) {
        Live::get().record(sv->name, "Operator level", f.operators[std::size_t(row)].name + " " + std::to_string(players_level->GetValue()), "Done");
        refresh_players();
    } else {
        MessageDialog(self, u8(error), _L("Players"), wxOK | wxICON_WARNING).ShowModal();
    }
}

// ---- Worlds ------------------------------------------------------------------

void ServersPanel::Ui::build_worlds(wxWindow *page)
{
    auto *s = new wxBoxSizer(wxVERTICAL);
    s->Add(text(page, Label::Body_13, _L("Every folder here with a level.dat. The server runs the one named by level-name."), true), 0);
    worlds = new MD3DataViewListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, page->FromDIP(260)), wxDV_ROW_LINES | wxDV_SINGLE);
    worlds->AppendTextColumn(_L("World"), wxDATAVIEW_CELL_INERT, page->FromDIP(220));
    worlds->AppendTextColumn(_L("State"), wxDATAVIEW_CELL_INERT, page->FromDIP(300));
    s->Add(worlds, 1, wxEXPAND | wxTOP, page->FromDIP(8));
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    auto *use = button(page, _L("Run this world"), GlyphPlay, Button::Variant::Outlined);
    use->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        ServerRecord *sv = server();
        const int     r  = worlds->GetSelectedRow();
        if (!sv || r < 0 || r >= int(world_names.size())) return;
        const fs::path path = fs::u8path(sv->folder) / "server.properties";
        std::string    text_in;
        read_file(path.u8string(), text_in);
        PropertiesFile file = parse_properties(text_in);
        file.set("level-name", world_names[std::size_t(r)]);
        std::string error;
        if (write_file_atomic(path.u8string(), write_properties(file), &error)) {
            prop_undo[sv->id] = text_in;
            Live::get().record(sv->name, "Settings", "level-name " + world_names[std::size_t(r)], "Done");
            refresh_worlds();
        } else {
            MessageDialog(self, u8(error), _L("Worlds"), wxOK | wxICON_WARNING).ShowModal();
        }
    });
    auto *open = button(page, _L("Open folder"), GlyphFolder, Button::Variant::Text);
    open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        ServerRecord *sv = server();
        const int     r  = worlds->GetSelectedRow();
        if (sv && r >= 0 && r < int(world_names.size())) wxLaunchDefaultApplication(u8((fs::u8path(sv->folder) / fs::u8path(world_names[std::size_t(r)])).u8string()));
    });
    row->Add(use, 0, wxRIGHT, page->FromDIP(8));
    row->Add(open, 0);
    s->Add(row, 0, wxTOP, page->FromDIP(8));
    s->Add(text(page, Label::Body_12, _L("Switching worlds takes effect at the next start. Nether and End folders of a Bukkit-style server travel with their world."), true), 0,
           wxTOP, page->FromDIP(6));
    page->SetSizer(s);
}

void ServersPanel::Ui::refresh_worlds()
{
    ServerRecord *sv = server();
    if (!sv) return;
    const ServerFolder f = inspect_folder(sv->folder);
    worlds->DeleteAllItems();
    world_names = f.worlds;
    for (const std::string &w : f.worlds) {
        wxString state = w == f.level_name ? _L("Runs on this server") : wxString();
        if (w == f.level_name && f.world_locked) state << dot() << _L("open now");
        wxVector<wxVariant> cells;
        cells.push_back(wxVariant(u8(w)));
        cells.push_back(wxVariant(state));
        worlds->AppendItem(cells);
    }
}

// ---- Versions ----------------------------------------------------------------

void ServersPanel::Ui::build_versions(wxWindow *page)
{
    auto *s   = new wxBoxSizer(wxVERTICAL);
    auto *top = new wxBoxSizer(wxHORIZONTAL);
    const wxString kinds[] = {_L("Releases"), _L("Snapshots"), _L("Old alpha and beta")};
    for (int i = 0; i < 3; ++i) {
        auto *b = button(page, kinds[i], 0, i == 0 ? Button::Variant::Tonal : Button::Variant::Text);
        b->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent &) {
            versions_kind = i;
            for (int k = 0; k < 3; ++k) versions_chips[std::size_t(k)]->SetVariant(k == i ? Button::Variant::Tonal : Button::Variant::Text);
            refresh_versions();
        });
        top->Add(b, 0, wxRIGHT, page->FromDIP(6));
        versions_chips.push_back(b);
    }
    top->AddStretchSpacer(1);
    auto *read = button(page, _L("Read the Mojang list"), GlyphRefresh, Button::Variant::Outlined);
    read->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { read_manifest(); });
    top->Add(read, 0);
    s->Add(top, 0, wxEXPAND);
    versions = new MD3DataViewListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, page->FromDIP(280)), wxDV_ROW_LINES | wxDV_SINGLE);
    versions->AppendTextColumn(_L("Version"), wxDATAVIEW_CELL_INERT, page->FromDIP(160));
    versions->AppendTextColumn(_L("Released"), wxDATAVIEW_CELL_INERT, page->FromDIP(140));
    versions->AppendTextColumn(_L("Needs"), wxDATAVIEW_CELL_INERT, page->FromDIP(160));
    versions->AppendTextColumn(_L("This server"), wxDATAVIEW_CELL_INERT, page->FromDIP(160));
    s->Add(versions, 1, wxEXPAND | wxTOP, page->FromDIP(8));
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    auto *sw = button(page, _L("Switch to this version..."), GlyphDownload, Button::Variant::Filled);
    sw->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { switch_version(); });
    row->Add(sw, 0);
    s->Add(row, 0, wxTOP, page->FromDIP(8));
    versions_note = text(page, Label::Body_12, wxEmptyString, true);
    s->Add(versions_note, 0, wxTOP, page->FromDIP(6));
    page->SetSizer(s);
}

void ServersPanel::Ui::refresh_versions()
{
    ServerRecord   *sv   = server();
    const Manifest &m    = Live::get().manifest;
    versions->DeleteAllItems();
    version_ids.clear();
    for (const ManifestVersion &v : m.versions) {
        const bool want = versions_kind == 0 ? v.type == "release" : versions_kind == 1 ? v.type == "snapshot" : (v.type == "old_alpha" || v.type == "old_beta");
        if (!want) continue;
        ServerVersion sv_;
        sv_.id           = v.id;
        sv_.release_time = v.release_time;
        const int java   = java_needed(sv_);
        bool      found  = false;
        for (const JavaInstall &j : Live::get().javas())
            if (j.feature >= java) found = true;
        wxVector<wxVariant> cells;
        cells.push_back(wxVariant(u8(v.id)));
        cells.push_back(wxVariant(u8(v.release_time.substr(0, 10))));
        cells.push_back(wxVariant(wxString::Format(found ? _L("Java %d, found") : _L("Java %d, not found"), java)));
        cells.push_back(wxVariant(sv && sv->version == v.id ? _L("Running this") : wxString()));
        versions->AppendItem(cells);
        version_ids.push_back(v.id);
    }
    wxString note = m.versions.empty() ? _L("Read the Mojang list to see every version.")
                                       : wxString::Format(_L("%d versions in the Mojang list."), int(m.versions.size()));
    if (sv && !sv->flavour.empty() && sv->flavour != "Vanilla")
        note << " " << wxString::Format(_L("This is a %s server; switching versions here is for Vanilla servers."), u8(sv->flavour));
    note << " " << _L("Going back to an older version is not proven safe: worlds saved by a newer one may not load. A full backup is made before any switch.");
    versions_note->SetLabel(note);
    versions_note->Wrap(self->FromDIP(760));
}

void ServersPanel::Ui::read_manifest()
{
    if (busy) return;
    set_busy(true, _L("Reading the Mojang version list"));
    std::weak_ptr<bool> guard = alive;
    auto                stop  = cancel;
    std::thread([this, guard, stop]() {
        Download d = fetch(kManifestUrl, {}, stop.get());
        wxTheApp->CallAfter([this, guard, d]() {
            if (guard.expired()) return;
            set_busy(false);
            Manifest m;
            if (!d.ok || !parse_manifest(d.body, m)) {
                if (!d.cancelled)
                    MessageDialog(self, wxString::Format(_L("The version list could not be read: %s"), u8(d.error.empty() ? "not a version list" : d.error)), _L("Versions"),
                                  wxOK | wxICON_WARNING)
                        .ShowModal();
                return;
            }
            Live::get().manifest      = m;
            Live::get().manifest_read = std::time(nullptr);
            write_file_atomic(Live::get().manifest_cache(), d.body);
            refresh_versions();
        });
    }).detach();
}

// Fetches one version's record, then its server jar, checked by SHA-1.
static void fetch_server_jar(const std::string &record_url, const std::string &target, std::shared_ptr<std::atomic<bool>> stop,
                             std::function<void(std::uint64_t, std::uint64_t)> progress, std::function<void(bool, std::string, VersionRecord)> done)
{
    std::thread([=]() {
        VersionRecord rec;
        Download      d = fetch(record_url, {}, stop.get());
        if (!d.ok || !parse_version_record(d.body, rec)) {
            done(false, d.cancelled ? "" : (d.error.empty() ? std::string("The version record could not be read.") : d.error), rec);
            return;
        }
        if (rec.server_url.empty()) {
            done(false, "Mojang publishes no server for this version.", rec);
            return;
        }
        std::error_code ec;
        if (fs::exists(fs::u8path(target), ec)) {
            std::string have;
            if (read_file(target, have, 512ull * 1024 * 1024) && sha1_hex(have) == rec.server_sha1) {
                done(true, "", rec); // already here and intact
                return;
            }
            done(false, "A different file with that name is already in the folder.", rec);
            return;
        }
        const Download jar = fetch_to_file(rec.server_url, target, rec.server_sha1, progress, stop.get());
        done(jar.ok, jar.cancelled ? "" : jar.error, rec);
    }).detach();
}

void ServersPanel::Ui::switch_version()
{
    ServerRecord *sv  = server();
    const int     row = versions->GetSelectedRow();
    if (!sv || row < 0 || row >= int(version_ids.size()) || busy) return;
    if (Live::get().running(sv->id)) {
        MessageDialog(self, _L("Stop the server before switching versions."), _L("Versions"), wxOK | wxICON_INFORMATION).ShowModal();
        return;
    }
    if (!sv->flavour.empty() && sv->flavour != "Vanilla" && sv->flavour != "Unknown") {
        MessageDialog(self, wxString::Format(_L("This is a %s server. Switching versions here is for Vanilla servers; use %s's own downloads."), u8(sv->flavour), u8(sv->flavour)),
                      _L("Versions"), wxOK | wxICON_INFORMATION)
            .ShowModal();
        return;
    }
    const std::string id = version_ids[std::size_t(row)];
    const ManifestVersion *mv = nullptr;
    for (const ManifestVersion &v : Live::get().manifest.versions)
        if (v.id == id) mv = &v;
    if (!mv) return;
    const bool older = !sv->version.empty() && compare_releases(id, sv->version) < 0 && parse_version(id).kind == VersionId::Kind::Release &&
                       parse_version(sv->version).kind == VersionId::Kind::Release;
    wxString question = wxString::Format(_L("Switch %s from %s to %s? A full backup is made first, and the current server file stays in the folder."), u8(sv->name),
                                         u8(sv->version.empty() ? "?" : sv->version), u8(id));
    if (older) question << "\n\n" << _L("This goes back to an older version. Worlds saved by a newer version may not load; the backup lets you return.");
    if (MessageDialog(self, question, _L("Switch version"), wxYES_NO | wxICON_QUESTION).ShowModal() != wxID_YES) return;
    const std::string id_copy = sv->id, url = mv->url, target = (fs::u8path(sv->folder) / ("minecraft_server." + id + ".jar")).u8string();
    set_busy(true, wxString::Format(_L("Switching to %s"), u8(id)));
    busy_step->SetLabel(_L("Making a full backup first"));
    std::weak_ptr<bool> guard = alive;
    auto                stop  = cancel;
    Live::get().backup(id_copy, false, {}, [this, guard, stop, id_copy, url, target, id](const ZipOutcome &b) {
        if (guard.expired()) return;
        if (!b.ok || *stop) {
            set_busy(false);
            if (!b.ok) MessageDialog(self, wxString::Format(_L("The backup failed, so nothing was switched: %s"), u8(b.error)), _L("Switch version"), wxOK | wxICON_WARNING).ShowModal();
            return;
        }
        busy_step->SetLabel(_L("Downloading the server file"));
        fetch_server_jar(
            url, target, stop,
            [this, guard](std::uint64_t a, std::uint64_t t) {
                wxTheApp->CallAfter([this, guard, a, t]() {
                    if (!guard.expired()) busy_step->SetLabel(wxString::Format(_L("Downloading: %s of %s"), size_text(a), size_text(t)));
                });
            },
            [this, guard, id_copy, target, id](bool ok, std::string error, VersionRecord) {
                wxTheApp->CallAfter([this, guard, id_copy, target, id, ok, error]() {
                    if (guard.expired()) return;
                    set_busy(false);
                    ServerRecord *s = Live::get().library.find(id_copy);
                    if (!ok || !s) {
                        if (!error.empty()) MessageDialog(self, u8(error), _L("Switch version"), wxOK | wxICON_WARNING).ShowModal();
                        if (s) Live::get().record(s->name, "Switch version", id + ": " + error, error.empty() ? "Cancelled" : "Failed");
                        return;
                    }
                    s->launch.jar = leaf(target);
                    s->version    = id;
                    s->flavour    = "Vanilla";
                    Live::get().save();
                    Live::get().record(s->name, "Switch version", id, "Done");
                    refresh_list();
                    refresh_detail();
                });
            });
    });
}

// ---- Backups -----------------------------------------------------------------

void ServersPanel::Ui::build_backups(wxWindow *page)
{
    auto *s = new wxBoxSizer(wxVERTICAL);
    backups_where = text(page, Label::Body_12, wxEmptyString, true);
    s->Add(backups_where, 0);
    backups = new MD3DataViewListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, page->FromDIP(260)), wxDV_ROW_LINES | wxDV_SINGLE);
    backups->AppendTextColumn(_L("Backup"), wxDATAVIEW_CELL_INERT, page->FromDIP(320));
    backups->AppendTextColumn(_L("Size"), wxDATAVIEW_CELL_INERT, page->FromDIP(100));
    backups->AppendTextColumn(_L("Made"), wxDATAVIEW_CELL_INERT, page->FromDIP(150));
    backups->AppendTextColumn(_L("Kind"), wxDATAVIEW_CELL_INERT, page->FromDIP(110));
    s->Add(backups, 1, wxEXPAND | wxTOP, page->FromDIP(8));
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    auto *now = button(page, _L("Back up now"), GlyphBackup, Button::Variant::Filled);
    now->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { backup_now(); });
    auto *restore_b = button(page, _L("Restore..."), GlyphRestart, Button::Variant::Outlined);
    restore_b->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { restore(); });
    auto *del = button(page, _L("Delete..."), GlyphRemove, Button::Variant::Text);
    del->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { delete_backup(); });
    auto *open = button(page, _L("Open folder"), GlyphFolder, Button::Variant::Text);
    open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (ServerRecord *sv = server()) {
            const std::string f = Live::get().backup_folder(*sv);
            std::error_code   ec;
            fs::create_directories(fs::u8path(f), ec);
            wxLaunchDefaultApplication(u8(f));
        }
    });
    auto *where = button(page, _L("Keep backups in..."), GlyphFolder, Button::Variant::Text);
    where->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        wxDirDialog d(self, _L("Where every server's backups go (a folder per server)"));
        if (d.ShowModal() != wxID_OK) return;
        Live::get().library.backup_folder = into_u8(d.GetPath());
        Live::get().save();
        refresh_backups();
    });
    for (Button *b : {now, restore_b, del, open, where}) row->Add(b, 0, wxRIGHT, page->FromDIP(8));
    s->Add(row, 0, wxTOP, page->FromDIP(8));
    s->Add(text(page, Label::Body_12, _L("A backup holds the worlds and the settings and player files. It is written under a temporary name, renamed when complete, and every file is read back and checked before it counts. While the server runs, saving pauses for the backup."), true),
           0, wxTOP, page->FromDIP(6));
    page->SetSizer(s);
}

void ServersPanel::Ui::refresh_backups()
{
    ServerRecord *sv = server();
    if (!sv) return;
    const std::string folder = Live::get().backup_folder(*sv);
    backups_where->SetLabel(wxString::Format(_L("Backups go to %s"), u8(folder)));
    backup_files = list_backups(folder);
    backups->DeleteAllItems();
    for (const BackupFile &b : backup_files) {
        wxVector<wxVariant> cells;
        cells.push_back(wxVariant(u8(b.name)));
        cells.push_back(wxVariant(size_text(b.size)));
        cells.push_back(wxVariant(when_text(b.when)));
        cells.push_back(wxVariant(b.automatic ? _L("Automatic") : _L("By hand")));
        backups->AppendItem(cells);
    }
}

void ServersPanel::Ui::backup_now()
{
    ServerRecord *sv = server();
    if (!sv || busy) return;
    set_busy(true, wxString::Format(_L("Backing up %s"), u8(sv->name)));
    busy_cancel->Hide();
    std::weak_ptr<bool> guard = alive;
    Live::get().backup(
        sv->id, false,
        [this, guard](std::uint64_t a, std::uint64_t t) {
            if (!guard.expired()) busy_step->SetLabel(wxString::Format(_L("%s of %s"), size_text(a), size_text(t)));
        },
        [this, guard](const ZipOutcome &out) {
            if (guard.expired()) return;
            busy_cancel->Show();
            set_busy(false);
            if (!out.ok) MessageDialog(self, wxString::Format(_L("The backup failed: %s"), u8(out.error)), _L("Back up"), wxOK | wxICON_WARNING).ShowModal();
            refresh_backups();
            refresh_detail();
        });
}

void ServersPanel::Ui::restore()
{
    ServerRecord *sv  = server();
    const int     row = backups->GetSelectedRow();
    if (!sv || row < 0 || row >= int(backup_files.size()) || busy) return;
    if (Live::get().running(sv->id)) {
        MessageDialog(self, _L("Stop the server before restoring a backup."), _L("Restore"), wxOK | wxICON_INFORMATION).ShowModal();
        return;
    }
    const BackupFile b = backup_files[std::size_t(row)];
    const std::vector<std::string> top = zip_top_entries(b.path);
    SuperConfirmGate::Spec spec;
    spec.action      = _L("Restore this backup");
    spec.consequence = wxString::Format(_L("The worlds and files in %s replace the ones in the server folder. The current ones are kept beside them, renamed \"before restore\", so nothing is lost."), u8(b.name));
    for (const std::string &t : top) spec.affected.push_back(u8(t));
    spec.completed_label = _L("Restoring");
    if (!SuperConfirmGate::Run(nullptr, spec)) return;
    const std::string folder = sv->folder, name = sv->name, stamp = now_stamp();
    set_busy(true, wxString::Format(_L("Restoring %s"), u8(b.name)));
    std::weak_ptr<bool> guard = alive;
    auto                stop  = cancel;
    std::thread([this, guard, stop, folder, name, stamp, b, top]() {
        const fs::path   temp = fs::u8path(folder) / (".restore " + stamp);
        ZipOutcome       out  = extract_zip(b.path, temp.u8string(), stop.get());
        std::string      error = out.error;
        if (out.ok) {
            for (const std::string &t : top) {
                const fs::path current = fs::u8path(folder) / fs::u8path(t), restored = temp / fs::u8path(t);
                std::error_code ec;
                if (fs::exists(current, ec)) fs::rename(current, fs::u8path(folder) / fs::u8path(t + " before restore " + stamp), ec);
                if (!ec) fs::rename(restored, current, ec);
                if (ec) { error = "Could not move " + t + " into place: " + ec.message(); break; }
            }
            std::error_code ec;
            fs::remove_all(temp, ec);
        }
        wxTheApp->CallAfter([this, guard, name, error, out, b]() {
            if (guard.expired()) return;
            set_busy(false);
            const bool ok = out.ok && error.empty();
            Live::get().record(name, "Restore", b.name + (ok ? "" : ": " + error), ok ? "Done" : (out.cancelled ? "Cancelled" : "Failed"));
            if (!ok && !out.cancelled) MessageDialog(self, u8(error), _L("Restore"), wxOK | wxICON_WARNING).ShowModal();
            refresh_backups();
            refresh_worlds();
        });
    }).detach();
}

void ServersPanel::Ui::delete_backup()
{
    ServerRecord *sv  = server();
    const int     row = backups->GetSelectedRow();
    if (!sv || row < 0 || row >= int(backup_files.size())) return;
    const BackupFile b = backup_files[std::size_t(row)];
    SuperConfirmGate::Spec spec;
    spec.action      = _L("Delete this backup");
    spec.consequence = wxString::Format(_L("%s is deleted from the disk for good."), u8(b.name));
    spec.affected    = {u8(b.name)};
    if (!SuperConfirmGate::Run(nullptr, spec)) return;
    std::error_code ec;
    fs::remove(fs::u8path(b.path), ec);
    Live::get().record(sv->name, "Delete backup", b.name, ec ? "Failed" : "Done");
    refresh_backups();
}

// ---- Schedule ----------------------------------------------------------------

void ServersPanel::Ui::build_schedule(wxWindow *page)
{
    auto *s = new wxBoxSizer(wxVERTICAL);
    const auto spin = [&](int lo, int hi, int v) { return new SpinInput(page, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(page->FromDIP(90), -1), 0, lo, hi, v); };
    auto *r1 = new wxBoxSizer(wxHORIZONTAL);
    sc_backup = new LabeledCheckBox(page, _L("Back up every"));
    sc_hours  = spin(1, 168, 6);
    sc_keep   = spin(1, 100, 8);
    r1->Add(sc_backup, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, page->FromDIP(8));
    r1->Add(sc_hours, 0, wxRIGHT, page->FromDIP(6));
    r1->Add(text(page, Label::Body_13, _L("hours, keeping the newest")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, page->FromDIP(6));
    r1->Add(sc_keep, 0, wxRIGHT, page->FromDIP(6));
    r1->Add(text(page, Label::Body_13, _L("automatic backups")), 0, wxALIGN_CENTER_VERTICAL);
    s->Add(r1, 0, wxBOTTOM, page->FromDIP(10));
    auto *r2 = new wxBoxSizer(wxHORIZONTAL);
    sc_restart = new LabeledCheckBox(page, _L("Restart daily at"));
    sc_hour    = spin(0, 23, 5);
    sc_minute  = spin(0, 59, 0);
    sc_warn    = spin(0, 30, 5);
    r2->Add(sc_restart, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, page->FromDIP(8));
    r2->Add(sc_hour, 0, wxRIGHT, page->FromDIP(4));
    r2->Add(text(page, Label::Body_13, ":"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, page->FromDIP(4));
    r2->Add(sc_minute, 0, wxRIGHT, page->FromDIP(10));
    r2->Add(text(page, Label::Body_13, _L("warning players")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, page->FromDIP(6));
    r2->Add(sc_warn, 0, wxRIGHT, page->FromDIP(6));
    r2->Add(text(page, Label::Body_13, _L("minutes before")), 0, wxALIGN_CENTER_VERTICAL);
    s->Add(r2, 0, wxBOTTOM, page->FromDIP(10));
    auto *save = button(page, _L("Save schedule"), 0, Button::Variant::Filled);
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        ServerRecord *sv = server();
        if (!sv) return;
        sv->schedule.backup_on      = sc_backup->GetValue();
        sv->schedule.backup_hours   = sc_hours->GetValue();
        sv->schedule.backup_keep    = sc_keep->GetValue();
        sv->schedule.restart_on     = sc_restart->GetValue();
        sv->schedule.restart_hour   = sc_hour->GetValue();
        sv->schedule.restart_minute = sc_minute->GetValue();
        sv->schedule.warn_minutes   = sc_warn->GetValue();
        if (sv->schedule.backup_on && sv->last_backup == 0) sv->last_backup = std::time(nullptr);
        RunState &r       = Live::get().runs[sv->id];
        r.next_restart_at = next_restart(sv->schedule, std::time(nullptr));
        r.warned          = false;
        Live::get().save();
        Live::get().record(sv->name, "Schedule", "", "Done");
        refresh_schedule();
    });
    s->Add(save, 0);
    sc_next = text(page, Label::Body_13, wxEmptyString);
    s->Add(sc_next, 0, wxTOP, page->FromDIP(10));
    s->Add(text(page, Label::Body_12, _L("Schedules run while Bambu Studio is open. A backup while the server runs pauses saving for the copy; a restart says so in chat first."), true), 0,
           wxTOP, page->FromDIP(6));
    page->SetSizer(s);
}

void ServersPanel::Ui::refresh_schedule()
{
    ServerRecord *sv = server();
    if (!sv) return;
    sc_backup->SetValue(sv->schedule.backup_on);
    sc_hours->SetValue(sv->schedule.backup_hours);
    sc_keep->SetValue(sv->schedule.backup_keep);
    sc_restart->SetValue(sv->schedule.restart_on);
    sc_hour->SetValue(sv->schedule.restart_hour);
    sc_minute->SetValue(sv->schedule.restart_minute);
    sc_warn->SetValue(sv->schedule.warn_minutes);
    wxString next;
    next << _L("Next backup:") << " " << (sv->schedule.backup_on ? when_text(next_backup(sv->schedule, sv->last_backup, std::time(nullptr))) : _L("not scheduled"));
    next << "    " << _L("Next restart:") << " " << (sv->schedule.restart_on ? when_text(next_restart(sv->schedule, std::time(nullptr))) : _L("not scheduled"));
    sc_next->SetLabel(next);
}

// ---- World generator -----------------------------------------------------------

void ServersPanel::Ui::build_generator(wxWindow *page)
{
    auto *outer = new wxBoxSizer(wxHORIZONTAL);
    auto *form_win = new MD3ScrolledWindow(page, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    form_win->SetScrollRate(0, page->FromDIP(12));
    form_win->SetBackgroundColour(role(MD3::Role::Surface));
    auto *f = new wxBoxSizer(wxVERTICAL);
    wxWindow *w = form_win;
    const int cw = page->FromDIP(180);
    gen_version = text(w, Label::Body_13, wxEmptyString, true);
    f->Add(gen_version, 0, wxBOTTOM, page->FromDIP(10));
    auto *grid = new wxFlexGridSizer(2, page->FromDIP(8), page->FromDIP(12));
    const auto row = [&](const wxString &label, wxWindow *control) {
        grid->Add(text(w, Label::Body_13, label), 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(control, 0);
    };
    auto *seed_row = new wxBoxSizer(wxHORIZONTAL);
    gen_seed = new TextInput(w, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(page->FromDIP(220), -1));
    gen_seed->GetTextCtrl()->SetHint(_L("Empty picks one at random"));
    auto *dice = button(w, wxEmptyString, GlyphDice, Button::Variant::Text);
    dice->SetToolTip(_L("A new random seed"));
    dice->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        std::random_device rd;
        gen_seed->GetTextCtrl()->SetValue(u8(random_seed((std::uint64_t(rd()) << 32) ^ rd() ^ std::uint64_t(std::time(nullptr)))));
    });
    seed_row->Add(gen_seed, 0, wxRIGHT, page->FromDIP(6));
    seed_row->Add(dice, 0);
    grid->Add(text(w, Label::Body_13, _L("Seed")), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(seed_row, 0);
    gen_preset = choice(w, {_L("Normal"), _L("Flat"), _L("Large biomes"), _L("Amplified"), _L("Single biome")}, 0, cw);
    row(_L("World type"), gen_preset);
    gen_mode = choice(w, {_L("Survival"), _L("Creative"), _L("Adventure")}, 0, cw);
    row(_L("Mode"), gen_mode);
    gen_difficulty = choice(w, {_L("Peaceful"), _L("Easy"), _L("Normal"), _L("Hard")}, 2, cw);
    row(_L("Difficulty"), gen_difficulty);
    std::vector<wxString> biomes;
    for (const NamedId &b : overworld_biomes()) biomes.push_back(tr(b.label));
    gen_biome = choice(w, biomes, 0, cw);
    row(_L("Biome (flat and single biome)"), gen_biome);
    f->Add(grid, 0, wxBOTTOM, page->FromDIP(8));
    auto *checks = new wxWrapSizer(wxHORIZONTAL);
    gen_structures = new LabeledCheckBox(w, _L("Structures"));
    gen_structures->SetValue(true);
    gen_hardcore = new LabeledCheckBox(w, _L("Hardcore"));
    for (LabeledCheckBox *c : {gen_structures, gen_hardcore}) checks->Add(c, 0, wxRIGHT | wxBOTTOM, page->FromDIP(12));
    f->Add(checks, 0);

    gen_flat = new wxPanel(w);
    gen_flat->SetBackgroundColour(role(MD3::Role::Surface));
    auto *fl = new wxBoxSizer(wxVERTICAL);
    fl->Add(text(gen_flat, Label::Head_13, _L("Flat layers, top first")), 0, wxBOTTOM, page->FromDIP(4));
    gen_layers = new MD3DataViewListCtrl(gen_flat, wxID_ANY, wxDefaultPosition, wxSize(page->FromDIP(380), page->FromDIP(140)), wxDV_ROW_LINES | wxDV_SINGLE);
    gen_layers->AppendTextColumn(_L("Block"), wxDATAVIEW_CELL_INERT, page->FromDIP(260));
    gen_layers->AppendTextColumn(_L("Height"), wxDATAVIEW_CELL_INERT, page->FromDIP(80));
    fl->Add(gen_layers, 0, wxEXPAND);
    auto *edit = new wxBoxSizer(wxHORIZONTAL);
    std::vector<wxString> blocks;
    for (const NamedId &b : flat_blocks()) blocks.push_back(u8(b.id));
    gen_block  = choice(gen_flat, blocks, 0, page->FromDIP(200), false);
    gen_height = new SpinInput(gen_flat, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(page->FromDIP(80), -1), 0, 1, 384, 1);
    edit->Add(gen_block, 0, wxRIGHT, page->FromDIP(6));
    edit->Add(gen_height, 0, wxRIGHT, page->FromDIP(6));
    const auto layer_button = [&](const wxString &label, std::uint32_t glyph, std::function<void()> fn) {
        auto *b = button(gen_flat, label, glyph, Button::Variant::Text);
        b->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
        edit->Add(b, 0, wxRIGHT, page->FromDIP(2));
    };
    const auto show_layers = [this]() {
        gen_layers->DeleteAllItems();
        for (const FlatLayer &l : gen.layers) {
            wxVector<wxVariant> cells;
            cells.push_back(wxVariant(u8(l.block)));
            cells.push_back(wxVariant(wxString::Format("%d", l.height)));
            gen_layers->AppendItem(cells);
        }
        read_generator();
    };
    layer_button(_L("Add layer"), GlyphAdd, [this, show_layers]() {
        const std::string id = block_id(into_u8(gen_block->GetValue()));
        if (id.empty()) return;
        const int at = gen_layers->GetSelectedRow();
        gen.layers.insert(at >= 0 ? gen.layers.begin() + at : gen.layers.end(), FlatLayer{id, gen_height->GetValue()});
        show_layers();
    });
    layer_button(wxEmptyString, GlyphUp, [this, show_layers]() {
        const int at = gen_layers->GetSelectedRow();
        if (at <= 0 || at >= int(gen.layers.size())) return;
        std::swap(gen.layers[std::size_t(at)], gen.layers[std::size_t(at - 1)]);
        show_layers();
        gen_layers->SelectRow(unsigned(at - 1));
    });
    layer_button(wxEmptyString, GlyphDown, [this, show_layers]() {
        const int at = gen_layers->GetSelectedRow();
        if (at < 0 || at + 1 >= int(gen.layers.size())) return;
        std::swap(gen.layers[std::size_t(at)], gen.layers[std::size_t(at + 1)]);
        show_layers();
        gen_layers->SelectRow(unsigned(at + 1));
    });
    layer_button(wxEmptyString, GlyphRemove, [this, show_layers]() {
        const int at = gen_layers->GetSelectedRow();
        if (at < 0 || at >= int(gen.layers.size())) return;
        gen.layers.erase(gen.layers.begin() + at);
        show_layers();
    });
    fl->Add(edit, 0, wxTOP, page->FromDIP(6));
    auto *classic = button(gen_flat, _L("Start from classic flat"), 0, Button::Variant::Text);
    classic->Bind(wxEVT_BUTTON, [this, show_layers](wxCommandEvent &) {
        gen.layers = classic_flat();
        show_layers();
    });
    fl->Add(classic, 0, wxTOP, page->FromDIP(4));
    auto *flat_checks = new wxBoxSizer(wxHORIZONTAL);
    gen_lakes    = new LabeledCheckBox(gen_flat, _L("Lakes"));
    gen_features = new LabeledCheckBox(gen_flat, _L("Features (trees, ores)"));
    flat_checks->Add(gen_lakes, 0, wxRIGHT, page->FromDIP(12));
    flat_checks->Add(gen_features, 0);
    fl->Add(flat_checks, 0, wxTOP, page->FromDIP(6));
    fl->Add(text(gen_flat, Label::Head_13, _L("Structures in the flat world")), 0, wxTOP, page->FromDIP(10));
    auto *sets = new wxWrapSizer(wxHORIZONTAL);
    for (const NamedId &sid : structure_sets()) {
        auto *c = new LabeledCheckBox(gen_flat, tr(sid.label));
        c->SetValue(sid.id == "minecraft:villages" || sid.id == "minecraft:strongholds");
        sets->Add(c, 0, wxRIGHT | wxBOTTOM, page->FromDIP(10));
        gen_sets.push_back({sid.id, c});
    }
    fl->Add(sets, 0, wxEXPAND | wxTOP, page->FromDIP(4));
    gen_flat->SetSizer(fl);
    f->Add(gen_flat, 0, wxEXPAND | wxTOP, page->FromDIP(8));

    f->Add(text(w, Label::Head_13, _L("Area to make ahead of time, in chunks, centred on spawn")), 0, wxTOP, page->FromDIP(12));
    auto *area = new wxBoxSizer(wxHORIZONTAL);
    const auto area_spin = [&](const wxString &label, int lo, int hi, int v, SpinInput *&out) {
        area->Add(text(w, Label::Body_13, label), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, page->FromDIP(4));
        out = new SpinInput(w, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(page->FromDIP(96), -1), 0, lo, hi, v);
        area->Add(out, 0, wxRIGHT, page->FromDIP(12));
    };
    area_spin(_L("Width"), 0, 1000, 0, gen_width);
    area_spin(_L("Depth"), 0, 1000, 0, gen_depth);
    area_spin(_L("Centre X"), -29999984, 29999984, 0, gen_cx);
    area_spin(_L("Centre Z"), -29999984, 29999984, 0, gen_cz);
    f->Add(area, 0, wxTOP, page->FromDIP(6));
    gen_summary = text(w, Label::Body_12, wxEmptyString, true);
    f->Add(gen_summary, 0, wxTOP, page->FromDIP(6));

    f->Add(text(w, Label::Head_13, _L("Game rules")), 0, wxTOP, page->FromDIP(12));
    gen_rules = new MD3ScrolledWindow(w, wxID_ANY, wxDefaultPosition, wxSize(-1, page->FromDIP(200)), wxVSCROLL);
    gen_rules->SetScrollRate(0, page->FromDIP(12));
    gen_rules->SetBackgroundColour(role(MD3::Role::SurfaceContainerLow));
    gen_rules->SetSizer(new wxBoxSizer(wxVERTICAL));
    f->Add(gen_rules, 0, wxEXPAND | wxTOP, page->FromDIP(6));

    f->Add(text(w, Label::Head_13, _L("Write the world")), 0, wxTOP, page->FromDIP(12));
    auto *out = new wxBoxSizer(wxHORIZONTAL);
    gen_write = choice(w, {_L("As a folder"), _L("As a ZIP file")}, 0, page->FromDIP(140));
    gen_output = new TextInput(w, u8((fs::u8path(into_u8(wxStandardPaths::Get().GetDocumentsDir())) / "Minecraft worlds").u8string()), wxEmptyString, wxEmptyString,
                               wxDefaultPosition, wxSize(page->FromDIP(300), -1));
    auto *browse = button(w, _L("Browse..."), GlyphFolder, Button::Variant::Text);
    browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        wxDirDialog d(self, _L("Where the new world goes"), gen_output->GetTextCtrl()->GetValue());
        if (d.ShowModal() == wxID_OK) gen_output->GetTextCtrl()->SetValue(d.GetPath());
    });
    out->Add(gen_write, 0, wxRIGHT, page->FromDIP(8));
    out->Add(gen_output, 1, wxRIGHT, page->FromDIP(8));
    out->Add(browse, 0);
    f->Add(out, 0, wxEXPAND | wxTOP, page->FromDIP(6));
    auto *eula = new wxBoxSizer(wxHORIZONTAL);
    gen_eula = new LabeledCheckBox(w, _L("I accept the Minecraft EULA for this run"));
    auto *read = button(w, _L("Read the EULA"), 0, Button::Variant::Text);
    read->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { wxLaunchDefaultBrowser("https://aka.ms/MinecraftEULA"); });
    eula->Add(gen_eula, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, page->FromDIP(10));
    eula->Add(read, 0, wxALIGN_CENTER_VERTICAL);
    f->Add(eula, 0, wxTOP, page->FromDIP(8));
    gen_problems = text(w, Label::Body_12, wxEmptyString);
    gen_problems->SetForegroundColour(role(MD3::Role::Error));
    f->Add(gen_problems, 0, wxTOP, page->FromDIP(6));
    auto *go_row = new wxBoxSizer(wxHORIZONTAL);
    gen_go   = button(w, _L("Make the world"), GlyphPlay, Button::Variant::Filled);
    gen_stop = button(w, _L("Cancel"), GlyphCancel, Button::Variant::Outlined);
    gen_go->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { generate(); });
    gen_stop->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (gen_run && gen_run->process && gen_run->process->running()) {
            gen_run->step      = Run::Step::Cancelled;
            gen_run->cancel_at = std::time(nullptr);
            gen_run->process->send("stop");
            gen_progress->SetLabel(_L("Cancelling: the generation server is stopping..."));
        }
    });
    gen_stop->Enable(false);
    go_row->Add(gen_go, 0, wxRIGHT, page->FromDIP(8));
    go_row->Add(gen_stop, 0);
    f->Add(go_row, 0, wxTOP, page->FromDIP(8));
    gen_progress = text(w, Label::Body_13, wxEmptyString);
    f->Add(gen_progress, 0, wxTOP, page->FromDIP(6));
    f->Add(text(w, Label::Body_12, _L("Never overwrites: the world is made in a folder of its own with a server that only this PC can reach, then copied, or zipped under a temporary name and checked, before it counts as done."), true),
           0, wxTOP | wxBOTTOM, page->FromDIP(6));
    form_win->SetSizer(f);
    outer->Add(form_win, 1, wxEXPAND | wxRIGHT, page->FromDIP(12));
    auto *side = new wxBoxSizer(wxVERTICAL);
    gen_preview = new AreaPreview(page);
    side->Add(gen_preview, 0);
    side->Add(text(page, Label::Body_12, _L("Thin lines every 4 chunks; thick lines mark the batches of 16 x 16 chunks the server makes one after another."), true), 0, wxTOP,
              page->FromDIP(6));
    outer->Add(side, 0);
    page->SetSizer(outer);

    gen.layers = classic_flat();
    show_layers();
    const auto changed = [this](wxCommandEvent &e) { read_generator(); e.Skip(); };
    for (ComboBox *c : {gen_preset, gen_mode, gen_difficulty, gen_biome}) c->Bind(wxEVT_COMBOBOX, changed);
    for (LabeledCheckBox *c : {gen_structures, gen_hardcore, gen_lakes, gen_features, gen_eula}) c->Bind(wxEVT_CHECKBOX, changed);
    for (SpinInput *s : {gen_width, gen_depth, gen_cx, gen_cz}) s->Bind(wxEVT_SPINCTRL, changed);
    for (auto &[id, c] : gen_sets) c->Bind(wxEVT_CHECKBOX, changed);
    gen_seed->GetTextCtrl()->Bind(wxEVT_TEXT, changed);
    gen_timer.SetOwner(page);
    page->Bind(wxEVT_TIMER, [this](wxTimerEvent &) { generation_tick(); }, gen_timer.GetId());
}

void ServersPanel::Ui::refresh_generator_version()
{
    ServerRecord *sv = server();
    const bool    has = sv && !sv->version.empty();
    gen.version = has ? sv->version : "";
    gen_version->SetLabel(has ? wxString::Format(_L("Makes a new world with %s %s, using this server's own server file, without touching %s's world."), u8(sv->flavour),
                                                 u8(sv->version), u8(sv->name))
                              : _L("Start this server once (or adopt one whose version can be read) so the generator knows which version makes the world."));
    gen_version->Wrap(self->FromDIP(640));
    if (gen_rules_for == gen.version) {
        read_generator();
        return;
    }
    gen_rules_for = gen.version;
    // The game rules this version has, each typed.
    gen_rules->Freeze();
    gen_rules->GetSizer()->Clear(true);
    gen_rule_get.clear();
    for (const GameRule *r : game_rules_for(gen.version)) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        auto *name = text(gen_rules, Label::Mono_12, u8(r->name));
        name->SetToolTip(tr(r->help));
        row->Add(name, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, gen_rules->FromDIP(8));
        const std::string def = r->def;
        if (r->is_bool) {
            auto *c = new CheckBox(gen_rules);
            c->SetValue(def == "true");
            c->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) { read_generator(); e.Skip(); });
            gen_rule_get[r->name] = [c, def]() { const std::string v = c->GetValue() ? "true" : "false"; return v == def ? std::string() : v; };
            row->Add(c, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gen_rules->FromDIP(8));
        } else {
            const int lo = int(std::max<long long>(r->min, -2147483647LL)), hi = int(std::min<long long>(r->max, 2147483647LL));
            auto *c = new SpinInput(gen_rules, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(gen_rules->FromDIP(110), -1), 0, lo, hi, std::atoi(def.c_str()));
            c->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &e) { read_generator(); e.Skip(); });
            gen_rule_get[r->name] = [c, def]() { const std::string v = std::to_string(c->GetValue()); return v == def ? std::string() : v; };
            row->Add(c, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gen_rules->FromDIP(8));
        }
        gen_rules->GetSizer()->Add(row, 0, wxEXPAND | wxTOP | wxBOTTOM, gen_rules->FromDIP(3));
    }
    gen_rules->Thaw();
    gen_rules->FitInside();
    read_generator();
}

void ServersPanel::Ui::read_generator()
{
    if (!gen_preset) return;
    static const WorldPreset presets[] = {WorldPreset::Normal, WorldPreset::Flat, WorldPreset::LargeBiomes, WorldPreset::Amplified, WorldPreset::SingleBiome};
    static const char *modes[] = {"survival", "creative", "adventure"};
    static const char *difficulties[] = {"peaceful", "easy", "normal", "hard"};
    gen.seed        = into_u8(gen_seed->GetTextCtrl()->GetValue());
    gen.preset      = presets[std::clamp(gen_preset->GetSelection(), 0, 4)];
    gen.gamemode    = modes[std::clamp(gen_mode->GetSelection(), 0, 2)];
    gen.difficulty  = difficulties[std::clamp(gen_difficulty->GetSelection(), 0, 3)];
    gen.biome       = overworld_biomes()[std::size_t(std::clamp(gen_biome->GetSelection(), 0, int(overworld_biomes().size()) - 1))].id;
    gen.structures  = gen_structures->GetValue();
    gen.hardcore    = gen_hardcore->GetValue();
    gen.lakes       = gen_lakes->GetValue();
    gen.features    = gen_features->GetValue();
    gen.structure_sets.clear();
    for (auto &[id, c] : gen_sets)
        if (c->GetValue()) gen.structure_sets.push_back(id);
    gen.width_chunks = gen_width->GetValue();
    gen.depth_chunks = gen_depth->GetValue();
    gen.centre_x     = gen_cx->GetValue();
    gen.centre_z     = gen_cz->GetValue();
    gen.game_rules.clear();
    for (auto &[name, get] : gen_rule_get)
        if (const std::string v = get(); !v.empty()) gen.game_rules[name] = v;
    const bool flat = gen.preset == WorldPreset::Flat;
    gen_flat->Show(flat);
    gen_biome->Enable(flat || gen.preset == WorldPreset::SingleBiome);
    const int chunks = gen.width_chunks * gen.depth_chunks;
    const std::vector<ChunkBatch> batches = plan_batches(gen);
    gen_summary->SetLabel(chunks > 0 ? wxString::Format(_L("%d chunks (%d x %d blocks) in %d batches, about %s and %s"), chunks, gen.width_chunks * 16, gen.depth_chunks * 16,
                                                        int(batches.size()), duration_text(estimate_seconds(chunks)), size_text(estimate_bytes(chunks)))
                                     : _L("0: only the area around spawn, as the server makes it on its own."));
    wxString problems;
    for (const Message &m : generator_problems(gen)) problems << say(m) << "\n";
    if (!gen_eula->GetValue()) problems << _L("Accept the EULA for this run to make the world.") << "\n";
    if (!processes_supported()) problems << _L("Making a world runs a server, which needs Windows in this build.") << "\n";
    gen_problems->SetLabel(problems.Trim());
    gen_problems->Wrap(self->FromDIP(640));
    const bool running_now = gen_run && gen_run->step != Run::Step::Done && gen_run->step != Run::Step::Failed && gen_run->step != Run::Step::Cancelled;
    gen_go->Enable(problems.Trim().IsEmpty() && !running_now && server() != nullptr);
    gen_preview->set(gen, gen_run ? gen_run->batch : 0);
    if (auto *form = dynamic_cast<wxScrolledWindow *>(gen_flat->GetParent())) {
        form->Layout();
        form->FitInside();
    }
}

void ServersPanel::Ui::generate()
{
    ServerRecord *sv = server();
    if (!sv || (gen_run && gen_run->process && gen_run->process->running())) return;
    read_generator();
    if (!generator_problems(gen).empty() || !gen_eula->GetValue()) return;
    const ServerFolder f = inspect_folder(sv->folder);
    const std::string  jar = f.jar.empty() ? sv->launch.jar : f.jar;
    if (jar.empty()) {
        MessageDialog(self, _L("This server has no server file to make the world with."), _L("World generator"), wxOK | wxICON_WARNING).ShowModal();
        return;
    }
    ServerVersion v;
    v.id           = gen.version;
    v.data_version = f.info.data_version;
    const std::string java = pick_java(Live::get().javas(), java_needed(v, f.info.java_version));
    if (java.empty()) {
        MessageDialog(self, wxString::Format(_L("Java %d is needed to make a world with this version, and none was found."), java_needed(v, f.info.java_version)),
                      _L("World generator"), wxOK | wxICON_WARNING)
            .ShowModal();
        return;
    }
    const int port = free_port_near(25600);
    auto      run  = std::make_unique<Run>();
    run->server    = sv->name;
    run->zip       = gen_write->GetSelection() == 1;
    const std::string base_name = safe_file_name(sv->name + " world " + now_stamp());
    const fs::path    out_dir   = fs::u8path(into_u8(gen_output->GetTextCtrl()->GetValue()));
    std::vector<std::string> taken;
    std::error_code          ec;
    for (fs::directory_iterator it(out_dir, ec), end; !ec && it != end; it.increment(ec)) taken.push_back(it->path().filename().u8string());
    const std::string out_name = unique_name(run->zip ? base_name + ".zip" : base_name, taken);
    run->output = (out_dir / fs::u8path(out_name)).u8string();
    run->temp   = (fs::u8path(data_dir()) / "minecraft-world-runs" / fs::u8path(now_stamp())).u8string();
    fs::create_directories(fs::u8path(run->temp), ec);
    fs::create_directories(out_dir, ec);
    // This run's own folder: the EULA the person just accepted, and settings
    // that keep everyone else out.
    PropertiesFile props;
    for (const auto &[k, val] : generator_properties(gen, port)) props.set(k, val);
    std::string error;
    if (!write_file_atomic((fs::u8path(run->temp) / "eula.txt").u8string(), eula_text(true, into_u8(wxDateTime::Now().FormatISOCombined())), &error) ||
        !write_file_atomic((fs::u8path(run->temp) / "server.properties").u8string(), write_properties(props), &error)) {
        MessageDialog(self, u8(error), _L("World generator"), wxOK | wxICON_WARNING).ShowModal();
        return;
    }
    LaunchSettings launch = sv->launch;
    launch.java           = java;
    launch.jar            = (fs::u8path(sv->folder) / fs::u8path(jar)).u8string();
    launch.jvm_args.clear();
    run->batches  = plan_batches(gen);
    run->process  = std::make_unique<ServerProcess>();
    std::weak_ptr<bool> guard = alive;
    const bool started = run->process->start(
        launch_arguments(launch), run->temp,
        [this, guard](const std::string &line) {
            wxTheApp->CallAfter([this, guard, line]() {
                if (!guard.expired()) generation_line(line);
            });
        },
        [this, guard](int code) {
            wxTheApp->CallAfter([this, guard, code]() {
                if (!guard.expired()) generation_exit(code);
            });
        },
        &error);
    if (!started) {
        fs::remove_all(fs::u8path(run->temp), ec);
        MessageDialog(self, u8(error), _L("World generator"), wxOK | wxICON_WARNING).ShowModal();
        return;
    }
    gen_run = std::move(run);
    gen_progress->SetLabel(_L("Starting a server for this world..."));
    gen_go->Enable(false);
    gen_stop->Enable(true);
    gen_timer.Start(1000);
    Live::get().record(sv->name, "World generator", out_name, "Started");
}

void ServersPanel::Ui::generation_line(const std::string &line)
{
    if (!gen_run) return;
    Run &r = *gen_run;
    r.tail.push_back(line);
    while (r.tail.size() > 12) r.tail.pop_front();
    const LogEvent e = parse_log_line(line);
    const auto next_batch = [this, &r]() {
        if (r.batch >= r.batches.size()) {
            r.step = Run::Step::Saving;
            gen_progress->SetLabel(_L("Saving the world..."));
            gen_run->process->send("save-all flush");
            return;
        }
        r.probes        = r.batches[r.batch].probe_commands();
        r.probe         = 0;
        r.batch_started = std::time(nullptr);
        r.process->send(r.batches[r.batch].add_command());
        r.process->send(r.probes[0]);
        gen_progress->SetLabel(wxString::Format(_L("Batch %d of %d, %d chunks ready"), int(r.batch + 1), int(r.batches.size()), r.ready));
    };
    switch (e.kind) {
    case LogEvent::Kind::Started:
        if (r.step != Run::Step::Starting) break;
        for (const auto &[name, val] : gen.game_rules) r.process->send("gamerule " + name + " " + val);
        r.step = Run::Step::Batches;
        next_batch();
        break;
    case LogEvent::Kind::TestPassed:
        if (r.step != Run::Step::Batches) break;
        if (++r.probe < r.probes.size()) {
            r.process->send(r.probes[r.probe]);
            break;
        }
        r.ready += r.batches[r.batch].chunks();
        r.process->send("forceload remove all");
        ++r.batch;
        gen_preview->set(gen, r.batch);
        next_batch();
        break;
    case LogEvent::Kind::TestFailed:
        if (r.step == Run::Step::Batches) r.probe_failed_at = std::time(nullptr); // asked again by the timer
        break;
    case LogEvent::Kind::Saved:
        if (r.step == Run::Step::Saving) {
            r.step = Run::Step::Stopping;
            gen_progress->SetLabel(_L("Stopping the generation server..."));
            r.process->send("stop");
        }
        break;
    case LogEvent::Kind::PortInUse:
    case LogEvent::Kind::EulaNeeded:
        r.process->send("stop");
        break;
    default: break;
    }
}

void ServersPanel::Ui::generation_tick()
{
    if (!gen_run || !gen_run->process) return;
    Run &r = *gen_run;
    const std::time_t now = std::time(nullptr);
    if (r.step == Run::Step::Batches) {
        if (r.probe_failed_at && now - r.probe_failed_at >= 1) {
            r.probe_failed_at = 0;
            r.process->send(r.probes[r.probe]);
        }
        if (now - r.batch_started > 240) {
            r.step = Run::Step::Failed;
            gen_progress->SetLabel(_L("A batch took more than four minutes; stopping."));
            r.process->send("stop");
        }
    }
    if (r.step == Run::Step::Cancelled && r.process->running() && r.cancel_at && now - r.cancel_at > 30) r.process->kill();
}

void ServersPanel::Ui::generation_exit(int code)
{
    if (!gen_run) return;
    gen_timer.Stop();
    Run &r = *gen_run;
    if (r.step != Run::Step::Stopping) {
        wxString why;
        if (r.step == Run::Step::Cancelled) why = _L("Cancelled; nothing was written.");
        else {
            why = wxString::Format(_L("The generation server stopped early (exit code %d)."), code);
            if (!r.tail.empty()) why << "\n" << u8(r.tail.back());
        }
        generation_finish(false, why);
        return;
    }
    r.step = Run::Step::Packaging;
    gen_progress->SetLabel(r.zip ? _L("Writing the ZIP file and checking it...") : _L("Copying the world..."));
    const std::string temp = r.temp, output = r.output;
    const bool        zip = r.zip;
    std::weak_ptr<bool> guard = alive;
    std::thread([this, guard, temp, output, zip]() {
        std::string error;
        bool        ok = false;
        if (zip) {
            const ZipOutcome z = write_zip(temp, {"world"}, output);
            ok    = z.ok;
            error = z.error;
        } else {
            ok = copy_tree((fs::u8path(temp) / "world").u8string(), output, &error);
        }
        wxTheApp->CallAfter([this, guard, ok, error]() {
            if (!guard.expired()) generation_finish(ok, ok ? wxString() : u8(error));
        });
    }).detach();
}

void ServersPanel::Ui::generation_finish(bool ok, const wxString &why)
{
    if (!gen_run) return;
    std::error_code ec;
    fs::remove_all(fs::u8path(gen_run->temp), ec);
    gen_run->step = ok ? Run::Step::Done : (gen_run->step == Run::Step::Cancelled ? Run::Step::Cancelled : Run::Step::Failed);
    gen_progress->SetLabel(ok ? wxString::Format(_L("Done: %s"), u8(gen_run->output)) : why);
    Live::get().record(gen_run->server, "World generator", leaf(gen_run->output) + (ok ? "" : ": " + into_u8(why)),
                       ok ? "Done" : (gen_run->step == Run::Step::Cancelled ? "Cancelled" : "Failed"));
    gen_stop->Enable(false);
    read_generator();
}

// ---- History -----------------------------------------------------------------

void ServersPanel::Ui::build_history(wxWindow *page)
{
    auto *s = new wxBoxSizer(wxVERTICAL);
    history = new MD3DataViewListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, page->FromDIP(320)), wxDV_ROW_LINES | wxDV_SINGLE);
    history->AppendTextColumn(_L("When"), wxDATAVIEW_CELL_INERT, page->FromDIP(150));
    history->AppendTextColumn(_L("What"), wxDATAVIEW_CELL_INERT, page->FromDIP(160));
    history->AppendTextColumn(_L("Details"), wxDATAVIEW_CELL_INERT, page->FromDIP(320));
    history->AppendTextColumn(_L("How it ended"), wxDATAVIEW_CELL_INERT, page->FromDIP(110));
    s->Add(history, 1, wxEXPAND);
    s->Add(text(page, Label::Body_12, _L("Every start, stop, setting, player change, backup, restore, version switch and world made here. The last 300 are kept."), true), 0,
           wxTOP, page->FromDIP(6));
    page->SetSizer(s);
}

void ServersPanel::Ui::refresh_history()
{
    ServerRecord *sv = server();
    history->DeleteAllItems();
    if (!sv) return;
    const auto &h = Live::get().library.history;
    for (auto it = h.rbegin(); it != h.rend(); ++it) {
        if (it->server != sv->name) continue;
        wxVector<wxVariant> cells;
        cells.push_back(wxVariant(when_text(it->when)));
        cells.push_back(wxVariant(u8(it->action)));
        cells.push_back(wxVariant(u8(it->detail)));
        cells.push_back(wxVariant(it->outcome == "Done" ? _L("Done") : it->outcome == "Failed" ? _L("Failed") : it->outcome == "Cancelled" ? _L("Cancelled") : u8(it->outcome)));
        history->AppendItem(cells);
    }
}

// ---- Actions -------------------------------------------------------------------

void ServersPanel::Ui::start()
{
    ServerRecord *sv = server();
    if (!sv) return;
    std::string error;
    LaunchCheck check;
    if (!Live::get().start(sv->id, error, &check)) {
        MessageDialog(self, u8(error), _L("This server cannot start yet"), wxOK | wxICON_WARNING).ShowModal();
        return;
    }
    if (!check.notes.empty()) {
        wxString notes;
        for (const Message &m : check.notes) notes << say(m) << "\n";
        status->SetToolTip(notes);
    }
    refresh_list();
    refresh_detail();
    show_tab(1);
}

void ServersPanel::Ui::stop(bool restart)
{
    if (ServerRecord *sv = server()) Live::get().stop(sv->id, restart);
}

void ServersPanel::Ui::force_stop()
{
    ServerRecord *sv = server();
    if (!sv || !Live::get().running(sv->id)) return;
    SuperConfirmGate::Spec spec;
    spec.action      = _L("Force stop the server");
    spec.consequence = _L("The server process ends at once without saving. Whatever changed since its last save is lost, and a world can be damaged.");
    spec.affected    = {u8(sv->name)};
    if (SuperConfirmGate::Run(b_kill, spec)) Live::get().kill(sv->id);
}

void ServersPanel::Ui::remove_server()
{
    ServerRecord *sv = server();
    if (!sv || Live::get().running(sv->id)) return;
    if (MessageDialog(self, wxString::Format(_L("Remove %s from this list? Its folder, worlds and backups stay where they are."), u8(sv->name)), _L("Remove from list"),
                      wxYES_NO | wxICON_QUESTION)
            .ShowModal() != wxID_YES)
        return;
    Live &live = Live::get();
    live.record(sv->name, "Remove from list", sv->folder, "Done");
    const std::string id = sv->id;
    live.library.servers.erase(std::remove_if(live.library.servers.begin(), live.library.servers.end(), [&](const ServerRecord &s) { return s.id == id; }),
                               live.library.servers.end());
    live.runs.erase(id);
    live.save();
    selected.clear();
    refresh_list();
    refresh_detail();
}

void ServersPanel::Ui::adopt()
{
    wxDirDialog d(self, _L("A folder with Minecraft servers in it (three levels are searched)"));
    if (d.ShowModal() != wxID_OK) return;
    const std::string root  = into_u8(d.GetPath());
    std::vector<std::string> found = find_server_folders(root);
    AdoptDialog dlg(self, found, Live::get().library);
    if (dlg.ShowModal() != wxID_OK) return;
    Live &live = Live::get();
    const std::uint64_t phys = physical_memory_mb();
    for (const std::string &folder : dlg.chosen()) {
        const ServerFolder f = inspect_folder(folder);
        ServerRecord       s;
        s.name                 = leaf(folder);
        s.id                   = live.library.unique_id(s.name);
        s.folder               = folder;
        s.launch.jar           = f.jar;
        s.launch.memory_min_mb = 1024;
        s.launch.memory_max_mb = int(std::clamp<std::uint64_t>(phys / 4 / 512 * 512, 2048, 8192));
        s.version              = f.info.version;
        s.flavour              = flavour_name(f.info.flavour);
        live.library.servers.push_back(s);
        live.record(s.name, "Adopt", folder, "Done");
        selected = s.id;
    }
    live.save();
    refresh_list();
    refresh_detail();
}

void ServersPanel::Ui::create_server()
{
    if (busy) return;
    Live &live = Live::get();
    if (live.manifest.versions.empty()) {
        MessageDialog(self, _L("Read the Mojang version list first (Versions, Read the Mojang list), so there is a version to choose."), _L("Create server"),
                      wxOK | wxICON_INFORMATION)
            .ShowModal();
        read_manifest();
        return;
    }
    CreateDialog dlg(self, live.manifest);
    if (dlg.ShowModal() != wxID_OK) return;
    const std::string name = dlg.name(), version = dlg.version(), folder = dlg.folder();
    const int         memory = dlg.memory(), port = dlg.port();
    const ManifestVersion *mv = nullptr;
    for (const ManifestVersion &v : live.manifest.versions)
        if (v.id == version) mv = &v;
    if (!mv) return;
    std::error_code ec;
    fs::create_directories(fs::u8path(folder), ec);
    if (ec) {
        MessageDialog(self, wxString::Format(_L("The folder could not be made: %s"), u8(ec.message())), _L("Create server"), wxOK | wxICON_WARNING).ShowModal();
        return;
    }
    set_busy(true, wxString::Format(_L("Creating %s"), u8(name)));
    busy_step->SetLabel(_L("Reading the version record"));
    std::weak_ptr<bool> guard = alive;
    auto                stop  = cancel;
    fetch_server_jar(
        mv->url, (fs::u8path(folder) / "server.jar").u8string(), stop,
        [this, guard](std::uint64_t a, std::uint64_t t) {
            wxTheApp->CallAfter([this, guard, a, t]() {
                if (!guard.expired()) busy_step->SetLabel(wxString::Format(_L("Downloading: %s of %s"), size_text(a), size_text(t)));
            });
        },
        [this, guard, name, version, folder, memory, port](bool ok, std::string error, VersionRecord) {
            wxTheApp->CallAfter([this, guard, name, version, folder, memory, port, ok, error]() {
                if (guard.expired()) return;
                set_busy(false);
                Live &live = Live::get();
                if (!ok) {
                    if (!error.empty()) MessageDialog(self, u8(error), _L("Create server"), wxOK | wxICON_WARNING).ShowModal();
                    live.record(name, "Create", version + ": " + error, error.empty() ? "Cancelled" : "Failed");
                    return;
                }
                // The EULA was accepted in the dialog; the first settings are the port and the name.
                PropertiesFile props;
                props.set("server-port", std::to_string(port));
                props.set("motd", name);
                write_file_atomic((fs::u8path(folder) / "eula.txt").u8string(), eula_text(true, into_u8(wxDateTime::Now().FormatISOCombined())));
                write_file_atomic((fs::u8path(folder) / "server.properties").u8string(), write_properties(props));
                ServerRecord s;
                s.name                 = name;
                s.id                   = live.library.unique_id(name);
                s.folder               = folder;
                s.launch.jar           = "server.jar";
                s.launch.memory_min_mb = std::min(1024, memory);
                s.launch.memory_max_mb = memory;
                s.version              = version;
                s.flavour              = "Vanilla";
                live.library.servers.push_back(s);
                live.record(name, "Create", version, "Done");
                live.save();
                selected = s.id;
                refresh_list();
                refresh_detail();
            });
        });
}

// ---------------------------------------------------------------------------

ServersPanel::ServersPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY), m_ui(std::make_unique<Ui>())
{
    SetName("minecraft-servers");
    SetBackgroundColour(role(MD3::Role::Surface));
    Live::get().load();
    m_ui->self = this;
    m_ui->build();
    Ui *ui = m_ui.get();
    m_ui->listener = Live::get().add_listener([ui](const std::string &id, bool console_only) {
        if (id == ui->selected && ui->book->GetSelection() == 1) ui->refresh_console(false);
        if (console_only) return;
        ui->refresh_list();
        if (id == ui->selected) {
            ui->refresh_detail();
        }
    });
    m_ui->refresh_list();
    m_ui->show_tab(0);
    m_ui->refresh_detail();
}

ServersPanel::~ServersPanel()
{
    Live::get().remove_listener(m_ui->listener);
    m_ui->ov_timer.Stop();
    m_ui->gen_timer.Stop();
    *m_ui->cancel = true;
    // A world being made belongs to this page: it stops with it.
    if (m_ui->gen_run && m_ui->gen_run->process && m_ui->gen_run->process->running()) {
        m_ui->gen_run->process->send("stop");
        const std::string temp = m_ui->gen_run->temp;
        std::thread([temp]() {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            std::error_code ec;
            fs::remove_all(fs::u8path(temp), ec);
        }).detach();
    }
    m_ui->alive.reset();
    // The controls go before the state their handlers use.
    DestroyChildren();
}

void ServersPanel::open()
{
    if (MainFrame *frame = wxGetApp().mainframe)
        frame->open_service("minecraft-servers", _L("Minecraft servers"), [](wxWindow *parent) { return new ServersPanel(parent); });
}

void shutdown() { Live::get().shutdown(); }

}}} // namespace Slic3r::GUI::GameServers
