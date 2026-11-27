#include "ContentLifecycleDialog.hpp"
#include "ContentLifecycle.hpp"
#include "BedrockLifecycle.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/MD3DataView.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/SuperConfirmGate.hpp"
#include "../Widgets/TextArea.hpp"
#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/filedlg.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#include <memory>
#include <thread>
namespace Slic3r { namespace GUI { namespace GameServers {
namespace {
class Packages final : public wxDialog
{
public:
    Packages(wxWindow *p, std::string dir, std::string type, bool is_bedrock = false)
        : wxDialog(p,
                   wxID_ANY,
                   is_bedrock ? _L("Installed Bedrock packs") : _L("Installed Java content"),
                   wxDefaultPosition,
                   wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        , folder(std::filesystem::u8path(dir))
        , flavour(std::move(type))
        , bedrock(is_bedrock)
    {
        auto *body = new wxBoxSizer(wxVERTICAL);
        list       = new MD3DataViewListCtrl(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(540, 200)), wxDV_SINGLE | wxDV_ROW_LINES);
        list->AppendTextColumn(_L("Package"), wxDATAVIEW_CELL_INERT, FromDIP(250));
        list->AppendTextColumn(_L("Directory"), wxDATAVIEW_CELL_INERT, FromDIP(160));
        list->AppendTextColumn(_L("State"), wxDATAVIEW_CELL_INERT, FromDIP(95));
        body->Add(list, 1, wxEXPAND | wxALL, FromDIP(8));
        auto *r = new wxWrapSizer(wxHORIZONTAL);
        for (auto a : {std::make_pair(_L("Enable or restore"), "enable"), std::make_pair(_L("Disable"), "disable"), std::make_pair(_L("Update from file..."), "update"),
                       std::make_pair(_L("Roll back version"), "rollback"), std::make_pair(_L("Remove from server"), "remove")}) {
            auto *b = new Button(this, a.first);
            b->Bind(wxEVT_BUTTON, [this, op = std::string(a.second), label = a.first](wxCommandEvent &) { act(op, label); });
            r->Add(b, 0, wxALL, FromDIP(3));
        }
        body->Add(r, 0, wxEXPAND | wxALL, FromDIP(5));
        status = new TextArea(this, _L("Removed packages are kept separately for recovery. Changes take effect at the next server start."), FromDIP(wxSize(525, 74)),
                              wxTE_READONLY);
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
        load();
    }

private:
    void load()
    {
        rows = bedrock ? list_local_bedrock_content(folder) : list_installed_java_content(folder);
        list->DeleteAllItems();
        for (const auto &e : rows) {
            wxVector<wxVariant> v;
            v.push_back(wxVariant(wxString::FromUTF8(e.name)));
            v.push_back(wxVariant(wxString::FromUTF8(e.subdir)));
            v.push_back(wxVariant(e.state == "enabled" ? _L("Enabled") : e.state == "disabled" ? _L("Disabled") : _L("Removed")));
            list->AppendItem(v);
        }
    }
    void act(const std::string &op, const wxString &label)
    {
        int i = list->GetSelectedRow();
        if (busy || i < 0 || std::size_t(i) >= rows.size()) return;
        auto                  entry = rows[std::size_t(i)];
        std::filesystem::path src;
        if (op == "update") {
            wxFileDialog d(this, _L("Select the compatible replacement for this package"), {}, {},
                           bedrock ? "Bedrock pack (*.mcpack;*.zip)|*.mcpack;*.zip" : "JAR / ZIP (*.jar;*.zip)|*.jar;*.zip", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (d.ShowModal() != wxID_OK) return;
            src = std::filesystem::u8path(d.GetPath().ToStdString(wxConvUTF8));
        }
        SuperConfirmGate::Spec spec;
        spec.action          = label;
        spec.consequence     = wxString::Format(_L("Change %s in %s? A verified recovery copy of its current bytes will be kept."), wxString::FromUTF8(entry.name),
                                                wxString::FromUTF8(entry.subdir));
        spec.completed_label = _L("Applying");
        if (!SuperConfirmGate::Run(nullptr, spec)) return;
        busy = true;
        status->SetValue(_L("Preserving the selected file and applying the change..."));
        auto                root = folder;
        auto                type = flavour;
        std::weak_ptr<bool> weak = life;
        std::thread([this, weak, entry, root, type, src, op, is_bedrock = bedrock] {
            auto error = is_bedrock ? change_local_bedrock_content(root, entry, op, src) : change_installed_java_content(root, type, entry, op, src);
            wxTheApp->CallAfter([this, weak, error] {
                if (weak.expired()) return;
                busy = false;
                load();
                status->SetValue(error.empty() ? _L("Package changed. Review the server log after starting.") : wxString::FromUTF8(error));
            });
        }).detach();
    }
    bool                              bedrock = false;
    std::filesystem::path             folder;
    std::string                       flavour;
    bool                              busy = false;
    MD3DataViewListCtrl              *list;
    TextArea                         *status;
    std::vector<InstalledJavaPackage> rows;
    std::shared_ptr<bool>             life = std::make_shared<bool>(true);
};
} // namespace
void show_bedrock_content_inventory(wxWindow *parent, const std::string &root)
{
    Packages d(parent, root, {}, true);
    d.ShowModal();
}
void show_java_content_inventory(wxWindow *parent, const std::string &root, const std::string &flavour)
{
    Packages d(parent, root, flavour);
    d.ShowModal();
}
}}} // namespace Slic3r::GUI::GameServers
