#include "ManagedSchedules.hpp"
#include "ManagedContainerLifecycle.hpp"
#include "ManagedSshSession.hpp"
#include "NativeServerTarget.hpp"
#include "ContainerSetup.hpp"
#include "TargetSettings.hpp"
#include "ServerSystem.hpp"
#include "libslic3r/Utils.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/LabeledCheckBox.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/SpinInput.hpp"
#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/filefn.h>
#include <wx/sizer.h>
#include <wx/utils.h>
#include <filesystem>
#include <set>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs = std::filesystem;
namespace {
std::string key(const WorldRecoveryTarget &t) { return sha1_hex((t.container ? "container\n" : "ssh\n") + t.profile + "\n" + t.server_folder + "\n" + t.owner); }
bool        owns(const WorldRecoveryTarget &t)
{
    if (t.container ? (!container_id(t.owner) || !container_field(t.profile)) : (!ownership_id(t.owner) || !ssh_profile_name(t.profile) || shell_path(t.server_folder).empty()))
        return false;
    auto        path = fs::u8path(data_dir()) / (t.container ? "minecraft-container-ids" : "minecraft-native-profiles") /
                       sha1_hex(t.container ? t.profile : t.profile + "\n" + t.server_folder) / (t.container ? t.owner : "id");
    std::string owner;
    return read_file(path.u8string(), owner, 128) && owner == t.owner;
}
fs::path directory() { return fs::u8path(data_dir()) / "minecraft-target-schedules"; }
class ScheduleLease
{
public:
    explicit ScheduleLease(const std::string &id)
    {
        std::error_code ec;
        fs::create_directories(directory(), ec);
        if (ec) return;
        auto path = directory() / (id + ".lock");
#ifdef _WIN32
        handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
#else
        fd = open(path.c_str(), O_CREAT | O_RDWR, 0600);
        if (fd >= 0 && flock(fd, LOCK_EX | LOCK_NB) != 0) {
            close(fd);
            fd = -1;
        }
#endif
    }
    explicit operator bool() const
    {
#ifdef _WIN32
        return handle != INVALID_HANDLE_VALUE;
#else
        return fd >= 0;
#endif
    }
    ~ScheduleLease()
    {
#ifdef _WIN32
        if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
#else
        if (fd >= 0) close(fd);
#endif
    }

private:
#ifdef _WIN32
    HANDLE handle = INVALID_HANDLE_VALUE;
#else
    int fd = -1;
#endif
};
bool write(const ManagedSchedule &s)
{
    std::error_code ec;
    fs::create_directories(directory(), ec);
    if (ec) return false;
    const auto    &t = s.target;
    const auto    &v = s.schedule;
    nlohmann::json j = {{"schema", 1},      {"container", t.container}, {"bedrock", t.bedrock},   {"profile", t.profile},       {"folder", t.server_folder},
                        {"owner", t.owner}, {"enabled", v.restart_on},  {"hour", v.restart_hour}, {"minute", v.restart_minute}, {"warning", v.warn_minutes},
                        {"next", s.next},   {"warned", s.warned_for},   {"last", s.last}};
    return write_file_atomic((directory() / (key(t) + ".json")).u8string(), j.dump());
}
bool read(const fs::path &p, ManagedSchedule &s)
{
    std::string raw;
    if (!read_file(p.u8string(), raw, 16384)) return false;
    auto j = nlohmann::json::parse(raw, nullptr, false);
    if (!j.is_object()) return false;
    try {
        if (j.at("schema") != 1) return false;
        s.target                  = {j.at("container").get<bool>(), j.at("bedrock").get<bool>(), j.at("profile").get<std::string>(), j.at("folder").get<std::string>(),
                                     j.at("owner").get<std::string>()};
        s.schedule.restart_on     = j.at("enabled");
        s.schedule.restart_hour   = j.at("hour");
        s.schedule.restart_minute = j.at("minute");
        s.schedule.warn_minutes   = j.at("warning");
        s.next                    = j.at("next");
        s.warned_for              = j.at("warned");
        s.last                    = j.value("last", std::string());
        return p.filename() == key(s.target) + ".json" && s.schedule.restart_hour >= 0 && s.schedule.restart_hour < 24 && s.schedule.restart_minute >= 0 &&
               s.schedule.restart_minute < 60 && s.schedule.warn_minutes >= 0 && s.schedule.warn_minutes <= 30;
    } catch (const nlohmann::json::exception &) {
        return false;
    }
}
std::string dispatch(const ManagedSchedule &s, const std::string &exe, ScheduleIntent event)
{
    const auto &t = s.target;
    if (!owns(t)) return "Skipped: the local ownership receipt changed.";
    const bool warning = event == ScheduleIntent::Warn;
    if (!t.container) {
        const std::string action = warning ? "warn " + std::to_string(s.schedule.warn_minutes) : "restart";
        auto              argv   = ssh_invocation(exe, t.profile,
                                                  "exec python3 -c " + shell_literal(managed_ssh_session) + " " + shell_path(t.server_folder) + " " + t.owner +
                                                      (t.bedrock ? " bedrock " : " java ") + action);
        auto              r      = run_target_cli(argv, {}, 90);
        return r.first == 0 ? (warning ? "Warning sent to the verified SSH console." : "Verified replacement SSH process. Game readiness is unverified.") :
                              "SSH action incomplete: " + r.second.substr(0, 600);
    }
    auto docker = [&](std::vector<std::string> args, int timeout = 12) {
        args.insert(args.begin(), {exe, "--context", t.profile});
        return run_target_cli(args, {}, timeout);
    };
    return managed_container_restart(
        t.owner, t.bedrock, warning, s.schedule.warn_minutes, docker, [] { return std::chrono::steady_clock::now(); },
        [] { std::this_thread::sleep_for(std::chrono::seconds(1)); });
}
std::set<std::string> active;
std::mutex            active_mutex;
bool                  in_flight(const std::string &id)
{
    std::lock_guard<std::mutex> lock(active_mutex);
    return active.count(id) != 0;
}
} // namespace
void poll_managed_schedules()
{
    static std::time_t last = 0;
    const auto         now  = std::time(nullptr);
    // The existing server manager polls every thirty seconds. A new app session
    // or a significant suspend/resume gap never replays overdue restarts.
    bool resumed = !last || now < last || now - last > 90;
    last         = now;
    std::error_code ec;
    for (fs::directory_iterator it(directory(), ec), end; !ec && it != end; it.increment(ec)) {
        if (it->path().extension() != ".json" || it->is_symlink(ec)) continue;
        ScheduleLease lease(it->path().stem().u8string());
        if (!lease) continue;
        ManagedSchedule s;
        if (!read(it->path(), s) || !s.schedule.restart_on) continue;
        const auto id = key(s.target);
        if (in_flight(id)) continue;
        const auto prev = s.next;
        // A newly opened second window must not steal a due event from an already-running manager.
        static std::map<std::string, std::time_t> skipped_on_resume;
        if (resumed && s.next && s.next <= now) skipped_on_resume[id] = s.next;
        auto skipped = skipped_on_resume.find(id);
        if (skipped != skipped_on_resume.end()) {
            if (skipped->second == s.next && now >= s.next && now - s.next <= 90) continue;
            skipped_on_resume.erase(skipped);
        }
        auto event = managed_schedule_due(s, now, resumed);
        if (event == ScheduleIntent::None) {
            if (prev != s.next) write(s);
            continue;
        }
        if (event == ScheduleIntent::Missed) {
            s.last = "Missed while the manager was inactive.";
            write(s);
            continue;
        }
        wxString path, exe;
        wxGetEnv("PATH", &path);
        if (!owns(s.target) || !wxFindFileInPath(&exe, path, s.target.container ? "docker.exe" : "ssh.exe")) {
            s.last = "Ownership or target client unavailable.";
            write(s);
            continue;
        }
        s.last = "Action dispatched; terminal result pending.";
        if (!write(s)) continue; // Advance the durable checkpoint before a network action.
        {
            std::lock_guard<std::mutex> guard(active_mutex);
            active.insert(id);
        }
        auto program = exe.ToStdString(wxConvUTF8);
        std::thread([s, id, program, event] {
            auto verdict = dispatch(s, program, event);
            {
                ScheduleLease   lease(id);
                ManagedSchedule current;
                if (lease && read(directory() / (id + ".json"), current)) {
                    current.last = verdict;
                    write(current);
                }
            }
            std::lock_guard<std::mutex> guard(active_mutex);
            active.erase(id);
        }).detach();
    }
}
void show_managed_schedule(wxWindow *parent, WorldRecoveryTarget target)
{
    wxDialog        dlg(parent, wxID_ANY, _L("Owned server restart schedule"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    auto           *body = new wxBoxSizer(wxVERTICAL);
    ManagedSchedule s;
    s.target = std::move(target);
    ManagedSchedule stored;
    if (read(directory() / (key(s.target) + ".json"), stored)) s = std::move(stored);
    auto *enabled = new LabeledCheckBox(&dlg, _L("Restart this running server daily"));
    enabled->SetValue(s.schedule.restart_on);
    body->Add(enabled, 0, wxALL, dlg.FromDIP(8));
    auto *grid = new wxFlexGridSizer(2, dlg.FromDIP(6), dlg.FromDIP(12));
    auto  num  = [&](const wxString &label, int hi, int value) {
        grid->Add(new Label(&dlg, Label::Body_13, label), 0, wxALIGN_CENTER_VERTICAL);
        auto *n = new SpinInput(&dlg, {}, {}, wxDefaultPosition, wxDefaultSize, 0, 0, hi, value);
        grid->Add(n, 1, wxEXPAND);
        return n;
    };
    auto *hour = num(_L("Local hour"), 23, s.schedule.restart_hour), *minute = num(_L("Minute"), 59, s.schedule.restart_minute),
         *warn = num(_L("Warning before restart (minutes)"), 30, s.schedule.warn_minutes);
    body->Add(grid, 0, wxEXPAND | wxALL, dlg.FromDIP(8));
    auto *note = new Label(&dlg, Label::Body_13, s.last.empty() ? _L("Runs while the server manager is active. Missed restarts are skipped.") : wxString::FromUTF8(s.last));
    note->Wrap(dlg.FromDIP(480));
    body->Add(note, 0, wxEXPAND | wxALL, dlg.FromDIP(8));
    auto *save = new Button(&dlg, _L("Save schedule"));
    save->Bind(wxEVT_BUTTON, [&](wxCommandEvent &) {
        ScheduleLease lease(key(s.target));
        if (!lease) {
            note->SetLabel(_L("A schedule update is already in progress."));
            return;
        }
        if (!owns(s.target)) {
            note->SetLabel(_L("The server has no matching local ownership receipt."));
            return;
        }
        s.schedule.restart_on     = enabled->GetValue();
        s.schedule.restart_hour   = hour->GetValue();
        s.schedule.restart_minute = minute->GetValue();
        s.schedule.warn_minutes   = warn->GetValue();
        s.next                    = s.schedule.restart_on ? next_restart(s.schedule, std::time(nullptr)) : 0;
        s.warned_for              = 0;
        s.last                    = s.schedule.restart_on ? "Scheduled." : "Disabled.";
        if (write(s))
            dlg.EndModal(wxID_OK);
        else
            note->SetLabel(_L("Could not save the schedule."));
    });
    body->Add(save, 0, wxALL, dlg.FromDIP(8));
    dlg.SetSizerAndFit(body);
    MD3DialogCaption::Adopt(&dlg);
    dlg.CentreOnParent();
    dlg.ShowModal();
}
}}} // namespace Slic3r::GUI::GameServers
