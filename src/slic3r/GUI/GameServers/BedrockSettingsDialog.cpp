#include "BedrockSettingsDialog.hpp"
#include "BedrockSettings.hpp"
#include "ServerSystem.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/LabeledCheckBox.hpp"
#include "../Widgets/MD3DataView.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/TextInput.hpp"
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <filesystem>

namespace Slic3r { namespace GUI { namespace GameServers {
namespace {
class BedrockSettingsDialog final : public wxDialog
{
public:
    BedrockSettingsDialog(wxWindow *parent, std::string dir)
        : wxDialog(parent, wxID_ANY, _L("Bedrock server settings"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
        , root(std::filesystem::u8path(dir))
        , file(root / "server.properties")
    {
        auto *body = new wxBoxSizer(wxVERTICAL);
        search     = new SearchField(this, _L("Find a Bedrock setting"));
        body->Add(search, 0, wxEXPAND | wxALL, FromDIP(8));
        list = new MD3DataViewListCtrl(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(560, 230)), wxDV_SINGLE | wxDV_ROW_LINES);
        list->AppendTextColumn(_L("Installed setting"), wxDATAVIEW_CELL_INERT, FromDIP(320));
        list->AppendTextColumn(_L("Configured value"), wxDATAVIEW_CELL_INERT, FromDIP(190));
        body->Add(list, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(8));
        note = new Label(this, Label::Body_13, wxEmptyString);
        body->Add(note, 0, wxEXPAND | wxALL, FromDIP(8));
        input   = new TextInput(this, wxEmptyString);
        boolean = new LabeledCheckBox(this, _L("Enabled"));
        number  = new SpinInput(this, {}, {}, wxDefaultPosition, wxDefaultSize, 0, 0, 2147483647, 0);
        choices = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
        for (wxWindow *w : std::vector<wxWindow *>{input, boolean, number, choices}) body->Add(w, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(8));
        auto *buttons = new wxBoxSizer(wxHORIZONTAL);
        auto *reload  = new Button(this, _L("Reload file"));
        apply         = new Button(this, _L("Apply selected change"));
        auto *close   = new Button(this, _L("Close"));
        reload->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { load(); });
        apply->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { save(); });
        close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CLOSE); });
        buttons->Add(reload, 0, wxRIGHT, FromDIP(5));
        buttons->Add(apply, 0, wxRIGHT, FromDIP(5));
        buttons->AddStretchSpacer();
        buttons->Add(close);
        body->Add(buttons, 0, wxEXPAND | wxALL, FromDIP(8));
        list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &) { select(); });
        search->SetOnQuery([this](const wxString &) { filter(); });
        search->SetOnRegexToggle([this](bool) { filter(); });
        SetSizer(body);
        load();
        Fit();
        MD3DialogCaption::Adopt(this);
        CentreOnParent();
    }

private:
    void load()
    {
        std::error_code ec;
        std::string     fresh;
        if (!root.is_absolute() || !std::filesystem::is_regular_file(root / "bedrock_server.exe", ec) || std::filesystem::is_symlink(file, ec) ||
            !read_file(file.u8string(), fresh, 1024 * 1024)) {
            rows.clear();
            filter();
            note->SetLabel(_L("Select a local Bedrock installation with a readable server.properties file."));
            return;
        }
        raw  = std::move(fresh);
        rows = read_bedrock_settings(raw);
        filter();
    }
    void filter()
    {
        visible.clear();
        list->DeleteAllItems();
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto &r   = rows[i];
            const auto  key = wxString::FromUTF8(r.key);
            if (!SearchField::textMatches(search->GetValue(), key, search->IsRegexEnabled(), search->IsCaseSensitive(), search->IsWholeWord(), search->IsMultiline())) continue;
            wxVector<wxVariant> values;
            values.push_back(wxVariant(key));
            const auto kind = bedrock_field(r.key).kind;
            values.push_back(wxVariant(r.duplicate                     ? _L("Duplicate key") :
                                       kind == BedrockValue::Unknown   ? _L("Unrecognized") :
                                       kind == BedrockValue::Protected ? _L("Protected") :
                                                                         wxString::FromUTF8(r.value)));
            visible.push_back(i);
            list->AppendItem(values);
        }
        select();
    }
    const BedrockSetting *selected() const
    {
        const int n = list->GetSelectedRow();
        return n < 0 || std::size_t(n) >= visible.size() ? nullptr : &rows[visible[std::size_t(n)]];
    }
    void select()
    {
        auto       r    = selected();
        const auto f    = r ? bedrock_field(r->key) : BedrockField{};
        const bool edit = r && !r->duplicate && f.kind != BedrockValue::Protected && f.kind != BedrockValue::Unknown;
        boolean->Show(edit && f.kind == BedrockValue::Bool);
        choices->Show(edit && f.kind == BedrockValue::Choice);
        number->Show(edit && f.kind == BedrockValue::Integer);
        input->Show(edit && (f.kind == BedrockValue::Text || f.kind == BedrockValue::Decimal));
        apply->Enable(edit);
        if (!r)
            note->SetLabel(_L("Select a setting from this installation."));
        else if (!edit)
            note->SetLabel(_L("This entry is preserved and cannot be edited here."));
        else {
            note->SetLabel(wxString::Format(_L("Edit %s. The server reads changes at startup."), wxString::FromUTF8(r->key)));
            if (f.kind == BedrockValue::Bool) boolean->SetValue(r->value == "true");
            if (f.kind == BedrockValue::Choice) {
                choices->Clear();
                for (const auto &v : f.choices) choices->Append(wxString::FromUTF8(v));
                choices->SetStringSelection(wxString::FromUTF8(r->value));
            }
            if (f.kind == BedrockValue::Integer) {
                number->SetRange(f.low, f.high);
                int n = f.low;
                std::from_chars(r->value.data(), r->value.data() + r->value.size(), n);
                number->SetValue(n);
            }
            if (f.kind == BedrockValue::Text || f.kind == BedrockValue::Decimal) input->GetTextCtrl()->ChangeValue(wxString::FromUTF8(r->value));
        }
        Layout();
    }
    void save()
    {
        auto r = selected();
        if (!r) return;
        const auto  f = bedrock_field(r->key);
        std::string value;
        if (f.kind == BedrockValue::Bool)
            value = boolean->GetValue() ? "true" : "false";
        else if (f.kind == BedrockValue::Choice)
            value = choices->GetStringSelection().ToStdString(wxConvUTF8);
        else if (f.kind == BedrockValue::Integer)
            value = std::to_string(number->GetValue());
        else
            value = input->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8);
        std::string current;
        if (!read_file(file.u8string(), current, 1024 * 1024) || current != raw) {
            note->SetLabel(_L("The settings file changed. Reload it before editing."));
            return;
        }
        if (!change_bedrock_setting(current, r->key, value)) {
            note->SetLabel(_L("That value is invalid for this setting."));
            return;
        }
        std::string error;
        if (!write_file_atomic(file.u8string(), current, &error)) {
            note->SetLabel(wxString::FromUTF8(error));
            return;
        }
        load();
        note->SetLabel(_L("Saved. The server will read the new value at startup."));
    }
    std::filesystem::path       root, file;
    std::string                 raw;
    std::vector<BedrockSetting> rows;
    std::vector<std::size_t>    visible;
    SearchField                *search;
    MD3DataViewListCtrl        *list;
    Label                      *note;
    TextInput                  *input;
    LabeledCheckBox            *boolean;
    SpinInput                  *number;
    ComboBox                   *choices;
    Button                     *apply;
};
} // namespace
void show_bedrock_settings(wxWindow *parent, const std::string &folder)
{
    BedrockSettingsDialog dlg(parent, folder);
    dlg.ShowModal();
}
}}} // namespace Slic3r::GUI::GameServers
