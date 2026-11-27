#include "TargetContentDialog.hpp"
#include "TargetContentScript.hpp"
#include "TargetSettings.hpp"
#include "ContainerSetup.hpp"
#include "NativeServerTarget.hpp"
#include "ServerSystem.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/LabeledCheckBox.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/TextArea.hpp"
#include "../Widgets/SuperConfirmGate.hpp"
#include <openssl/evp.h>
#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/filedlg.h>
#include <wx/filefn.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#include <wx/utils.h>
#include <condition_variable>
#include <filesystem>
#include <fstream>
namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs = std::filesystem;
namespace {
std::string file_digest(const fs::path &path)
{
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx(EVP_MD_CTX_new(), EVP_MD_CTX_free);
    std::ifstream                                           f(path, std::ios::binary);
    if (!ctx || !f || EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) != 1) return {};
    char buf[65536];
    while (f) {
        f.read(buf, sizeof(buf));
        if (EVP_DigestUpdate(ctx.get(), buf, std::size_t(f.gcount())) != 1) return {};
    }
    unsigned char sum[EVP_MAX_MD_SIZE];
    unsigned int  n = 0;
    if (!f.eof() || EVP_DigestFinal_ex(ctx.get(), sum, &n) != 1 || n != 32) return {};
    std::string result;
    for (unsigned i = 0; i < n; ++i) {
        result += "0123456789abcdef"[sum[i] >> 4];
        result += "0123456789abcdef"[sum[i] & 15];
    }
    return result;
}
struct TransferState
{
    std::mutex              mutex;
    std::condition_variable signal;
    int                     code = -1;
    bool                    done = false;
    std::string             output;
};

TargetSettingsResult transfer(
    const WorldRecoveryTarget &target, const std::string &exe, const fs::path &pkg, const std::string &flavour, bool activate, const nlohmann::json &selected)
{
    std::error_code ec;
    auto            size = fs::file_size(pkg, ec);
    if (ec || !size || size > 1024ULL * 1024 * 1024) return {"Choose a readable content package of at most 1 GiB.", {}};
    auto entries = zip_top_entries(pkg.u8string());
    auto has     = [&](const char *name) { return std::find(entries.begin(), entries.end(), name) != entries.end(); };
    if (target.bedrock ? !has("manifest.json") :
                         !(has("META-INF") || has("plugin.yml") || has("paper-plugin.yml") || has("fabric.mod.json") || has("quilt.mod.json") || has("pack.mcmeta")))
        return {"The selected file lacks a recognized content descriptor.", {}};
    auto sum = file_digest(pkg);
    if (sum.empty()) return {"The selected package could not be hashed.", {}};
    std::vector<std::string> args;
    if (target.container) {
        if (!container_id(target.owner) || !container_field(target.profile)) return {"Invalid container target.", {}};
        const auto identity = target_settings(target, exe, {{"mode", "write-identity"}, {"keys", nlohmann::json::array()}});
        if (!identity.error.empty() || !identity.result.contains("user") || !identity.result["user"].is_string())
            return {identity.error.empty() ? "Could not verify the stopped container and its file owner." : identity.error, {}};
        args = {exe,
                "--context",
                target.profile,
                "run",
                "--rm",
                "-i",
                "--network",
                "none",
                "--read-only",
                "--cap-drop",
                "ALL",
                "--security-opt",
                "no-new-privileges",
                "--user",
                identity.result["user"].get<std::string>(),
                "--volumes-from",
                target.owner + ":rw",
                "python:3.13-alpine",
                "python3",
                "-c",
                target_content_script,
                "/data",
                target.bedrock ? "bedrock" : "java",
                target.owner,
                "container"};
    } else {
        if (!ownership_id(target.owner) || shell_path(target.server_folder).empty()) return {"The SSH directory is not owned by this installation.", {}};
        args = ssh_invocation(exe, target.profile,
                              "exec python3 -c " + shell_literal(target_content_script) + " " + shell_path(target.server_folder) + (target.bedrock ? " bedrock " : " java ") +
                                  target.owner + " ssh");
    }
    if (args.empty()) return {"Invalid server target.", {}};
    auto          state = std::make_shared<TransferState>();
    ServerProcess process;
    std::string   error;
    if (!process.start(
            args, fs::current_path().u8string(),
            [state](const std::string &line) {
                if (state->output.size() + line.size() < 8192) state->output += line + "\n";
            },
            [state](int code) {
                std::lock_guard<std::mutex> guard(state->mutex);
                state->code = code;
                state->done = true;
                state->signal.notify_all();
            },
            &error))
        return {error, {}};
    std::thread    deadline([&process, state] {
        std::unique_lock<std::mutex> guard(state->mutex);
        if (!state->signal.wait_for(guard, std::chrono::minutes(5), [&] { return state->done; })) process.kill();
    });
    nlohmann::json header = {{"name", pkg.filename().u8string()}, {"size", size}, {"sha256", sum}, {"flavour", flavour}, {"activate", activate}};
    if (selected.is_object()) {
        header["action"] = "update";
        for (const auto *k : {"id", "state", "stamp"}) header[k] = selected.at(k);
    }
    std::ifstream              input(pkg, std::ios::binary);
    std::uintmax_t             sent = 0;
    bool                       ok   = process.send(header.dump());
    std::vector<unsigned char> data(12288), encoded(16385);
    while (ok && sent < size && input) {
        input.read(reinterpret_cast<char *>(data.data()), std::streamsize(std::min<std::uintmax_t>(data.size(), size - sent)));
        int len = int(input.gcount());
        if (len <= 0) {
            ok = false;
            break;
        }
        int n = EVP_EncodeBlock(encoded.data(), data.data(), len);
        ok    = n > 0 && process.send(std::string(reinterpret_cast<const char *>(encoded.data()), std::size_t(n)));
        sent += len;
    }
    if (!ok || sent != size) process.kill();
    deadline.join();
    std::unique_lock<std::mutex> lock(state->mutex);
    if (!state->done && !state->signal.wait_for(lock, std::chrono::seconds(5), [&] { return state->done; })) return {"The package transfer timed out.", {}};
    if (state->code != 0 || !ok) return {state->output.empty() ? "The target rejected the package transfer." : state->output, {}};
    auto end = state->output.find_last_not_of("\r\n");
    if (end == std::string::npos) return {"No package receipt was returned.", {}};
    auto pos    = state->output.rfind('\n', end);
    auto result = nlohmann::json::parse(state->output.substr(pos == std::string::npos ? 0 : pos + 1), nullptr, false);
    if (!result.is_object() || !result.value("installed", false) || result.value("sha256", std::string()) != sum)
        return {"The target did not confirm the requested package hash.", {}};
    return {{}, std::move(result)};
}
class PackageDialog final : public wxDialog
{
public:
    PackageDialog(wxWindow *p, WorldRecoveryTarget destination)
        : wxDialog(p, wxID_ANY, _L("Install target content"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), target(std::move(destination))
    {
        auto *body  = new wxBoxSizer(wxVERTICAL);
        auto *intro = new Label(this, Label::Body_13, wxString::Format(_L("Select a trusted content package to transfer to %s."), wxString::FromUTF8(target.profile)));
        body->Add(intro, 0, wxEXPAND | wxALL, FromDIP(8));
        auto *pick = new Button(this, _L("Choose package..."));
        body->Add(pick, 0, wxALL, FromDIP(8));
        selection = new Label(this, Label::Body_13, _L("No package selected."));
        body->Add(selection, 0, wxEXPAND | wxALL, FromDIP(8));
        pick->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            if (busy) return;
            wxFileDialog d(this, _L("Select trusted server content"), {}, {}, target.bedrock ? "Bedrock (*.mcpack;*.zip)|*.mcpack;*.zip" : "Java (*.jar;*.zip)|*.jar;*.zip",
                           wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (d.ShowModal() == wxID_OK) {
                file = fs::u8path(d.GetPath().ToStdString(wxConvUTF8));
                selection->SetLabel(d.GetFilename());
            }
        });
        flavour = new ComboBox(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
        for (const auto *f : {"Vanilla", "Paper", "Purpur", "Spigot", "Fabric", "Forge", "NeoForge", "Quilt"}) flavour->Append(f);
        flavour->SetSelection(0);
        if (!target.bedrock) {
            body->Add(new Label(this, Label::Body_13, _L("Installed Java distribution")), 0, wxLEFT, FromDIP(8));
            body->Add(flavour, 0, wxEXPAND | wxALL, FromDIP(8));
        } else
            flavour->Hide();
        enable = new LabeledCheckBox(this, _L("Activate the Bedrock pack in the configured world"));
        if (target.bedrock)
            body->Add(enable, 0, wxALL, FromDIP(8));
        else
            enable->Hide();
        auto *install = new Button(this, _L("Transfer and install"));
        install->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { apply(); });
        body->Add(install, 0, wxALL, FromDIP(8));
        inventory = new ComboBox(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
        body->Add(inventory, 0, wxEXPAND | wxALL, FromDIP(8));
        auto *maintenance = new wxWrapSizer(wxHORIZONTAL);
        for (auto a : {std::make_pair(_L("List installed content"), "list"), std::make_pair(_L("Enable"), "enable"), std::make_pair(_L("Disable"), "disable"),
                       std::make_pair(_L("Remove from server"), "remove"), std::make_pair(_L("Roll back"), "rollback"), std::make_pair(_L("Update selected"), "update")}) {
            auto *b = new Button(this, a.first);
            b->Bind(wxEVT_BUTTON, [this, op = std::string(a.second), label = a.first](wxCommandEvent &) { maintenance_action(op, label); });
            maintenance->Add(b, 0, wxALL, FromDIP(3));
        }
        body->Add(maintenance, 0, wxEXPAND | wxALL, FromDIP(6));
        status = new TextArea(this, _L("Plugins, mods and scripts execute with the server's permissions."), FromDIP(wxSize(500, 82)), wxTE_READONLY);
        body->Add(status, 0, wxEXPAND | wxALL, FromDIP(8));
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &e) {
            if (busy && e.CanVeto())
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
    void maintenance_action(const std::string &op, const wxString &label)
    {
        if (busy) return;
        nlohmann::json entry;
        if (op != "list") {
            int n = inventory->GetSelection();
            if (n < 0 || std::size_t(n) >= items.size()) return;
            entry = items[std::size_t(n)];
            SuperConfirmGate::Spec spec;
            spec.action          = label;
            spec.consequence     = wxString::Format(_L("Change the selected package %s on this target? Prior package versions are kept for rollback."),
                                                    wxString::FromUTF8(entry.value("name", std::string())));
            spec.completed_label = _L("Applying");
            if (!SuperConfirmGate::Run(nullptr, spec)) return;
        }
        if (op == "update") {
            apply(entry);
            return;
        }
        wxString path, exe;
        wxGetEnv("PATH", &path);
        if (!wxFindFileInPath(&exe, path, target.container ? "docker.exe" : "ssh.exe")) {
            status->SetValue(_L("The target client is unavailable."));
            return;
        }
        nlohmann::json request = {{"action", op}};
        if (entry.is_object())
            for (auto k : {"id", "state", "stamp"}) request[k] = entry.at(k);
        auto t                   = target;
        auto program             = exe.ToStdString(wxConvUTF8);
        busy                     = true;
        std::weak_ptr<bool> weak = life;
        std::thread([this, weak, t, program, request, op] {
            auto reply = target_content_action(t, program, request);
            wxTheApp->CallAfter([this, weak, reply = std::move(reply), op] {
                if (weak.expired()) return;
                busy = false;
                if (!reply.error.empty()) {
                    status->SetValue(wxString::FromUTF8(reply.error));
                    return;
                }
                if (op != "list") {
                    status->SetValue(wxString::FromUTF8(reply.result.dump(2)));
                    return;
                }
                items.clear();
                inventory->Clear();
                if (reply.result.contains("packages") && reply.result["packages"].is_array())
                    for (const auto &e : reply.result["packages"]) {
                        if (!e.is_object()) continue;
                        bool valid = true;
                        for (auto k : {"id", "state", "stamp", "group", "name"})
                            if (!e.contains(k) || !e[k].is_string()) valid = false;
                        if (!valid) continue;
                        items.push_back(e);
                        inventory->Append(wxString::FromUTF8(e["group"].get<std::string>() + "/" + e["name"].get<std::string>() + " (" + e["state"].get<std::string>() + ")"));
                    }
                if (!items.empty()) inventory->SetSelection(0);
                status->SetValue(wxString::Format(_L("Installed and recoverable packages: %d"), int(items.size())));
            });
        }).detach();
    }
    void apply(const nlohmann::json &selected = {})
    {
        if (busy || file.empty()) return;
        wxString env, exe;
        wxGetEnv("PATH", &env);
        if (!wxFindFileInPath(&exe, env, target.container ? "docker.exe" : "ssh.exe")) {
            status->SetValue(_L("The target client is unavailable."));
            return;
        }
        busy = true;
        status->SetValue(_L("Verifying and transferring the selected package..."));
        auto                pkg     = file;
        auto                t       = target;
        auto                program = exe.ToStdString(wxConvUTF8), type = flavour->GetStringSelection().ToStdString(wxConvUTF8);
        bool                activate = target.bedrock && enable->GetValue();
        std::weak_ptr<bool> weak     = life;
        std::thread([this, weak, t, program, pkg, type, activate, selected] {
            auto r = transfer(t, program, pkg, type, activate, selected);
            wxTheApp->CallAfter([this, weak, r = std::move(r)] {
                if (weak.expired()) return;
                busy = false;
                status->SetValue(wxString::FromUTF8(r.error.empty() ? r.result.dump(2) : r.error));
            });
        }).detach();
    }
    WorldRecoveryTarget         target;
    fs::path                    file;
    Label                      *selection;
    ComboBox                   *flavour, *inventory;
    std::vector<nlohmann::json> items;
    LabeledCheckBox            *enable;
    TextArea                   *status;
    bool                        busy = false;
    std::shared_ptr<bool>       life = std::make_shared<bool>(true);
};
} // namespace
void show_target_content(wxWindow *parent, WorldRecoveryTarget target)
{
    PackageDialog dlg(parent, std::move(target));
    dlg.ShowModal();
}
}}} // namespace Slic3r::GUI::GameServers
