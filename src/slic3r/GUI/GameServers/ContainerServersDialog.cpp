#include "ContainerServersDialog.hpp"
#include "ContainerSetup.hpp"
#include "ServerSystem.hpp"
#include "ServerModel.hpp"
#include "libslic3r/Utils.hpp"
#include <filesystem>
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/LabeledCheckBox.hpp"
#include "../Widgets/MD3DataView.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/TextArea.hpp"
#include "../Widgets/TextInput.hpp"

#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/filefn.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>
#include <wx/wrapsizer.h>
#include <memory>

namespace Slic3r { namespace GUI { namespace GameServers {
namespace {

class ContainerServersDialog final : public wxDialog {
public:
    explicit ContainerServersDialog(wxWindow *parent)
        : wxDialog(parent, wxID_ANY, _L("Container servers"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        auto *body = new wxBoxSizer(wxVERTICAL);
        auto *grid = new wxFlexGridSizer(2, FromDIP(6), FromDIP(12));
        grid->AddGrowableCol(1);
        auto field = [&](const wxString &label, wxWindow *control) {
            grid->Add(new Label(this, Label::Body_13, label), 0, wxALIGN_CENTER_VERTICAL);
            grid->Add(control, 1, wxEXPAND);
        };
        context = new TextInput(this, "default");
        field(_L("Docker context (local or SSH)"), context);
        name = new TextInput(this, "my-server");
        field(_L("New container name"), name);
        edition = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
        edition->Append("Java"); edition->Append("Bedrock"); edition->SetSelection(0);
        field(_L("Edition"), edition);
        flavour = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
        for (auto type : {"VANILLA", "PAPER", "PURPUR", "SPIGOT", "FABRIC", "FORGE", "NEOFORGE", "QUILT"}) flavour->Append(type);
        flavour->SetSelection(0);
        field(_L("Java distribution"), flavour);
        version = new TextInput(this, "LATEST");
        field(_L("Server version"), version);
        tag = new TextInput(this, "latest");
        field(_L("Container image tag"), tag);
        bind_address = new TextInput(this, "127.0.0.1");
        field(_L("Target host IPv4 address"), bind_address);
        lan_allowed = new LabeledCheckBox(this, _L("Allow connections from the target host's private LAN"));
        field(_L("LAN access"), lan_allowed);
        port = new SpinInput(this, wxEmptyString, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, 1, 65535, 25565);
        field(_L("Port on target host"), port);
        edition->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) {
            const bool bedrock = edition->GetSelection() == 1;
            flavour->Enable(!bedrock);
            port->SetValue(bedrock ? 19132 : 25565);
        });
        body->Add(grid, 0, wxEXPAND | wxALL, FromDIP(12));
        eula = new LabeledCheckBox(this, _L("I accept the Minecraft EULA for the new server"));
        body->Add(eula, 0, wxLEFT | wxRIGHT, FromDIP(12));
        list = new MD3DataViewListCtrl(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(520, 120)), wxDV_SINGLE | wxDV_ROW_LINES);
        list->AppendTextColumn(_L("Container and state"), wxDATAVIEW_CELL_INERT, FromDIP(480));
        body->Add(list, 1, wxEXPAND | wxALL, FromDIP(12));
        auto *actions = new wxWrapSizer(wxHORIZONTAL);
        auto action = [&](const wxString &label, std::function<void()> fn) {
            auto *btn = new Button(this, label);
            btn->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
            actions->Add(btn, 0, wxALL, FromDIP(3));
        };
        action(_L("Read EULA"), [] { wxLaunchDefaultBrowser("https://aka.ms/MinecraftEULA"); });
        action(_L("Create"), [this] { create(); });
        action(_L("Refresh servers"), [this] { refresh(); });
        action(_L("Start selected"), [this] { control("start"); });
        action(_L("Stop selected"), [this] { control("stop"); });
        body->Add(actions, 0, wxEXPAND | wxLEFT, FromDIP(9));
        status = new TextArea(this, _L("Refresh to list managed containers in this context."), FromDIP(wxSize(520, 85)), wxTE_READONLY);
        body->Add(status, 0, wxEXPAND | wxALL, FromDIP(12));
        context->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { ids.clear(); list->DeleteAllItems(); });
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &e) {
            if (active && e.CanVeto()) { e.Veto(); return; }
            e.Skip();
        });
        SetEscapeId(wxID_NONE);
        SetSizerAndFit(body);
        MD3DialogCaption::Adopt(this);
        CentreOnParent();
    }

private:
    using Lines = std::vector<std::string>;
    std::string target() const { return context->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8); }

    void run(std::vector<std::string> args, std::function<void(const Lines &)> done)
    {
        if (active) return;
        if (!container_field(target())) { status->SetValue(_L("Enter a configured Docker context name.")); return; }
        wxString path, exe;
        wxGetEnv("PATH", &path);
        if (!wxFindFileInPath(&exe, path, "docker.exe")) { status->SetValue(_L("Docker CLI was not found on PATH.")); return; }
        const std::string original = target();
        args.insert(args.begin(), {exe.ToStdString(wxConvUTF8), "--context", original});
        auto out = std::make_shared<Lines>();
        std::weak_ptr<bool> weak = lifetime;
        process = std::make_unique<ServerProcess>();
        std::string error;
        bool ok = process->start(args, wxGetCwd().ToStdString(wxConvUTF8),
            [out](const std::string &line) { if (out->size() < 512) out->push_back(line.substr(0, 1000)); },
            [this, weak, original, out, done](int code) {
                wxTheApp->CallAfter([this, weak, original, out, done, code]() {
                    if (weak.expired()) return;
                    active = false;
                    context->Enable();
                    if (original != target()) return;
                    if (code != 0) {
                        wxString msg = wxString::Format(_L("Docker exited with code %d."), code);
                        for (const auto &line : *out) msg += "\n" + wxString::FromUTF8(line);
                        status->SetValue(msg);
                    } else done(*out);
                });
            }, &error);
        active = ok;
        context->Enable(!ok);
        status->SetValue(ok ? _L("Waiting for Docker...") : wxString::FromUTF8(error));
    }

    std::filesystem::path receipt(const std::string &id) const
    {
        return std::filesystem::u8path(Slic3r::data_dir()) / "minecraft-container-ids" / sha1_hex(target()) / id;
    }

    bool owned(const std::string &id) const
    {
        std::string content;
        return container_id(id) && read_file(receipt(id).u8string(), content, 256) && content == id;
    }

    void refresh()
    {
        run({"ps", "--all", "--no-trunc", "--filter", "label=org.bambustudio.minecraft=1", "--format", "{{.ID}}\t{{.Names}}\t{{.Status}}"},
            [this](const Lines &out) {
                list->DeleteAllItems(); ids.clear();
                for (const std::string &line : out) {
                    const auto split = line.find('\t');
                    if (split == std::string::npos || !owned(line.substr(0, split))) continue;
                    ids.push_back(line.substr(0, split)); wxVector<wxVariant> row;
                    row.push_back(wxVariant(wxString::FromUTF8(line.substr(split + 1)))); list->AppendItem(row);
                }
                status->SetValue(wxString::Format(_L("Managed containers listed: %d"), int(ids.size())));
            });
    }

    void create()
    {
        auto args = container_create({name->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8), version->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8),
            flavour->GetStringSelection().ToStdString(wxConvUTF8), tag->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8),
            port->GetValue(), edition->GetSelection() == 1, eula->GetValue(), bind_address->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8), lan_allowed->GetValue()});
        if (args.empty()) { status->SetValue(_L("Check the name, version, tag, IPv4 address, port, LAN permission and EULA acceptance.")); return; }
        run(std::move(args), [this](const Lines &out) {
            for (auto it = out.rbegin(); it != out.rend(); ++it) {
                if (!container_id(*it)) continue;
                const auto path = receipt(*it);
                std::error_code ec;
                std::filesystem::create_directories(path.parent_path(), ec);
                if (ec || !write_file_atomic(path.u8string(), *it) || !owned(*it)) {
                    status->SetValue(_L("Container created, but its local ownership record could not be saved. Container ID: ") + wxString::FromUTF8(*it));
                    return;
                }
                refresh();
                return;
            }
            status->SetValue(_L("Docker returned without a full container ID. Refresh before retrying."));
        });
    }

    void control(const std::string &verb)
    {
        const int n = list->GetSelectedRow();
        if (n < 0 || std::size_t(n) >= ids.size()) { status->SetValue(_L("Select a managed container first.")); return; }
        if (!owned(ids[std::size_t(n)])) { status->SetValue(_L("The local ownership record is missing.")); return; }
        run({verb, ids[std::size_t(n)]}, [this](const Lines &) { refresh(); });
    }

    TextInput *context, *name, *version, *tag;
    TextArea *status;
    bool active = false;
    TextInput *bind_address;
    LabeledCheckBox *lan_allowed;
    ComboBox *edition, *flavour;
    SpinInput *port;
    LabeledCheckBox *eula;
    MD3DataViewListCtrl *list;
    Lines ids;
    std::unique_ptr<ServerProcess> process;
    std::shared_ptr<bool> lifetime = std::make_shared<bool>(true);
};
}
void show_container_servers(wxWindow *parent) { ContainerServersDialog dlg(parent); dlg.ShowModal(); }
}}}
