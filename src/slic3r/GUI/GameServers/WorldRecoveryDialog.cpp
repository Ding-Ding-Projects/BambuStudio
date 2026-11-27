#include "WorldRecoveryDialog.hpp"
#include "ContainerSetup.hpp"
#include "NativeServerTarget.hpp"
#include "WorldRecoveryScript.hpp"
#include "ServerSystem.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/TextArea.hpp"
#include "../Widgets/TextInput.hpp"
#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/filefn.h>
#include <wx/sizer.h>
#include <wx/utils.h>
#include <wx/wrapsizer.h>
#include <nlohmann/json.hpp>

namespace Slic3r { namespace GUI { namespace GameServers {
namespace {
class RecoveryDialog final : public wxDialog
{
public:
    RecoveryDialog(wxWindow *parent, WorldRecoveryTarget selected)
        : wxDialog(parent, wxID_ANY, _L("World backup and restore"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), target(std::move(selected))
    {
        auto *body = new wxBoxSizer(wxVERTICAL);
        auto *help = new Label(this, Label::Body_13,
                               target.container ? _L("Back up a stopped container's world into its private backup volume. Restores go to a separate recovery volume. The helper "
                                                     "uses Python 3.13 Alpine with its network disabled.") :
                                                  _L("Back up an owned SSH server while stopped. Restore into a new absolute directory on the same host."));
        help->Wrap(FromDIP(510));
        body->Add(help, 0, wxEXPAND | wxALL, FromDIP(8));
        archives = new ComboBox(this, wxID_ANY, {}, wxDefaultPosition, FromDIP(wxSize(500, -1)), 0, nullptr, wxCB_READONLY);
        body->Add(archives, 0, wxEXPAND | wxALL, FromDIP(8));
        dest = new TextInput(this, {});
        dest->GetTextCtrl()->SetHint(target.container ? _L("New directory name in the recovery volume") : _L("New absolute restore directory on SSH host"));
        body->Add(dest, 0, wxEXPAND | wxALL, FromDIP(8));
        auto *actions = new wxWrapSizer(wxHORIZONTAL);
        for (auto a : {std::make_pair(_L("Refresh backups"), std::string("list")), std::make_pair(_L("Create backup"), std::string("snapshot")),
                       std::make_pair(_L("Restore separately"), std::string("restore"))}) {
            auto *b = new Button(this, a.first);
            b->Bind(wxEVT_BUTTON, [this, mode = a.second](wxCommandEvent &) { request(mode); });
            actions->Add(b, 0, wxALL, FromDIP(3));
        }
        body->Add(actions, 0, wxEXPAND | wxALL, FromDIP(5));
        status = new TextArea(this, {}, FromDIP(wxSize(500, 92)), wxTE_READONLY);
        body->Add(status, 0, wxEXPAND | wxALL, FromDIP(8));
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &e) {
            if (active && e.CanVeto())
                e.Veto();
            else
                e.Skip();
        });
        SetEscapeId(wxID_NONE);
        SetSizerAndFit(body);
        MD3DialogCaption::Adopt(this);
        CentreOnParent();
    }

private:
    using Callback = std::function<void(int, const std::string &)>;
    void run(std::vector<std::string> argv, Callback done)
    {
        auto                output = std::make_shared<std::string>();
        std::weak_ptr<bool> weak   = life;
        std::string         error;
        proc   = std::make_unique<ServerProcess>();
        active = proc->start(
            argv, wxGetCwd().ToStdString(wxConvUTF8),
            [output](const std::string &line) {
                if (output->size() + line.size() < 32768) *output += line + "\n";
            },
            [this, weak, done, output](int code) {
                wxTheApp->CallAfter([this, weak, done, output, code] {
                    if (weak.expired()) return;
                    active = false;
                    done(code, *output);
                });
            },
            &error);
        status->SetValue(active ? _L("Waiting for the selected target...") : wxString::FromUTF8(error));
    }
    void request(const std::string &mode)
    {
        if (active) return;
        const auto archive = archives->GetStringSelection().ToStdString(wxConvUTF8), name = dest->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8);
        if (mode == "restore" && (archive.empty() || (target.container ? !container_field(name) : shell_path(name).empty()))) {
            status->SetValue(_L("Select a backup and enter a new restore destination."));
            return;
        }
        wxString env, exe;
        wxGetEnv("PATH", &env);
        if (!wxFindFileInPath(&exe, env, target.container ? "docker.exe" : "ssh.exe")) {
            status->SetValue(_L("The required target client is unavailable."));
            return;
        }
        if (target.container) {
            if (!container_id(target.owner) || !container_field(target.profile)) {
                status->SetValue(_L("Invalid container identity."));
                return;
            }
            const auto program = exe.ToStdString(wxConvUTF8);
            run({program, "--context", target.profile, "inspect", "--format", "{{.State.Running}}", target.owner},
                [this, mode, name, archive, program](int code, const std::string &out) {
                    if (code || (mode == "snapshot" && out != "false\n")) {
                        status->SetValue(_L("The selected container is unavailable or still running."));
                        return;
                    }
                    const auto store  = "bambu-world-backups-" + target.owner;
                    auto       launch = [this, program, store, mode, name, archive] {
                        std::vector<std::string> args = {program,          "--context",
                                                         target.profile,   "run",
                                                         "--rm",           "--network",
                                                         "none",           "--read-only",
                                                         "--cap-drop",     "ALL",
                                                         "--security-opt", "no-new-privileges",
                                                         "--volumes-from", target.owner + ":ro",
                                                         "--mount",        "type=volume,src=" + store + ",dst=/backups"};
                        if (mode == "restore") args.insert(args.end(), {"--mount", "type=volume,src=bambu-world-restores-" + target.owner + ",dst=/restore"});
                        args.insert(args.end(), {"python:3.13-alpine", "python3", "-c", world_recovery_script, mode, "container", "/data", target.bedrock ? "bedrock" : "java",
                                                 target.owner, archive, "/restore/" + name});
                        go(std::move(args), mode);
                    };
                    if (mode == "snapshot") {
                        launch();
                        return;
                    }
                    run({program, "--context", target.profile, "volume", "ls", "--filter", "name=" + store, "--format", "{{.Name}}"},
                        [this, store, launch](int code, const std::string &out) {
                            if (code) {
                                status->SetValue(_L("Could not list backup volumes."));
                                return;
                            }
                            if (out != store + "\n") {
                                archives->Clear();
                                status->SetValue(_L("No owned backup volume is available."));
                                return;
                            }
                            launch();
                        });
                });
        } else {
            if (!ownership_id(target.owner) || shell_path(target.server_folder).empty()) {
                status->SetValue(_L("Provision an owned SSH target first."));
                return;
            }
            std::string cmd = "exec python3 -c " + shell_literal(world_recovery_script) + " " + mode + " ssh " + shell_path(target.server_folder) +
                              (target.bedrock ? " bedrock " : " java ") + target.owner + " " + shell_literal(archive) + " " + shell_literal(name);
            go(ssh_invocation(exe.ToStdString(wxConvUTF8), target.profile, cmd), mode);
        }
    }
    void go(std::vector<std::string> args, const std::string &mode)
    {
        run(std::move(args), [this, mode](int code, const std::string &out) {
            if (code) {
                status->SetValue(wxString::Format(_L("Recovery action exited with code %d: %s"), code, wxString::FromUTF8(out)));
                return;
            }
            // Docker can print pull progress before the final one-line result.
            auto end = out.find_last_not_of("\r\n");
            if (end == std::string::npos) {
                status->SetValue(_L("No recovery receipt was returned."));
                return;
            }
            auto       start   = out.rfind('\n', end);
            const auto payload = out.substr(start == std::string::npos ? 0 : start + 1);
            auto       json    = nlohmann::json::parse(payload, nullptr, false);
            if (!json.is_object()) {
                status->SetValue(_L("The recovery report is unreadable."));
                return;
            }
            if (mode == "list" && json.contains("archives") && json["archives"].is_array()) {
                archives->Clear();
                for (const auto &a : json["archives"])
                    if (a.is_string()) archives->Append(wxString::FromUTF8(a.get<std::string>()));
                if (archives->GetCount()) archives->SetSelection(0);
                status->SetValue(wxString::Format(_L("Backups available: %d"), int(archives->GetCount())));
            } else if (mode == "snapshot" && json.contains("archive") && json["archive"].is_string()) {
                auto a = wxString::FromUTF8(json["archive"].get<std::string>());
                archives->Insert(a, 0);
                archives->SetSelection(0);
                status->SetValue(wxString::FromUTF8(json.dump(2)));
            } else if (mode == "restore" && json.contains("restored")) {
                auto details = wxString::FromUTF8(json.dump(2));
                if (target.container) details += "\n" + wxString::FromUTF8("bambu-world-restores-" + target.owner);
                status->SetValue(details);
            } else
                status->SetValue(_L("The target returned an incomplete recovery report."));
        });
    }
    WorldRecoveryTarget            target;
    ComboBox                      *archives;
    TextInput                     *dest;
    TextArea                      *status;
    bool                           active = false;
    std::unique_ptr<ServerProcess> proc;
    std::shared_ptr<bool>          life = std::make_shared<bool>(true);
};
} // namespace
void show_world_recovery(wxWindow *parent, WorldRecoveryTarget target)
{
    RecoveryDialog dlg(parent, std::move(target));
    dlg.ShowModal();
}
}}} // namespace Slic3r::GUI::GameServers
