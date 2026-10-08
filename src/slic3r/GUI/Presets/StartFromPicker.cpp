#include "StartFromPicker.hpp"

#include "libslic3r/PresentationDefaults.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/LanguageMode.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/ListBox.hpp"
#include "slic3r/GUI/Widgets/MD3Tokens.hpp"
#include "slic3r/GUI/Widgets/SearchField.hpp"
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include "slic3r/GUI/Widgets/TextArea.hpp"

#include <wx/sizer.h>

#include <utility>

// The shipped funny level lives in the wx-free defaults table so libslic3r
// and the presets can read it; it must stay the language service's default.
static_assert(Slic3r::PresentationDefaults::kFunnyLevel == Slic3r::GUI::I18N::FUNNY_LEVEL_DEFAULT,
              "PresentationDefaults::kFunnyLevel must equal I18N::FUNNY_LEVEL_DEFAULT");

namespace Slic3r { namespace GUI {

namespace {

using BlankEditorPresets::Assignment;
using BlankEditorPresets::Omission;
using BlankEditorPresets::Preset;

wxString tr(const std::string &english) { return english.empty() ? wxString() : _L(english.c_str()); }

wxString assignment_text(const Assignment &assignment)
{
    wxString label = tr(assignment.label);
    if (!assignment.scope.empty())
        label = wxString::Format(_L("%s (%s)"), label, tr(assignment.scope));
    const wxString shown = assignment.shown_translatable ? tr(assignment.shown) : wxString::FromUTF8(assignment.shown.c_str());
    wxString text = wxString::Format(_L("%s: %s"), label, shown);
    if (assignment.from_default)
        text = wxString::Format(_L("%s (shipped default, nothing saved)"), text);
    return text;
}

wxString omission_text(const Omission &omission)
{
    return wxString::Format(_L("%s: %s"), tr(omission.label), tr(omission.reason));
}

} // namespace

wxString describe_preset(const Preset &preset)
{
    wxString text = tr(preset.creates);
    if (preset.sets.empty()) {
        text << "\n\n" << _L("Sets nothing.");
    } else {
        text << "\n\n" << _L("Sets:");
        for (const Assignment &assignment : preset.sets)
            text << "\n  " << assignment_text(assignment);
    }
    if (!preset.left_out.empty()) {
        text << "\n\n" << _L("Leaves out:");
        for (const Omission &omission : preset.left_out)
            text << "\n  " << omission_text(omission);
    }
    return text;
}

wxString preset_settings_line(const Preset &preset)
{
    if (preset.sets.empty())
        return _L("Sets nothing.");
    wxString line;
    for (const Assignment &assignment : preset.sets) {
        if (!line.empty())
            line << "; ";
        line << assignment_text(assignment);
    }
    return line;
}

StartFromPicker::StartFromPicker(wxWindow *parent, const wxString &title, const wxString &subtitle, std::vector<Preset> presets)
    : MD3Dialog(parent, title, subtitle, MaterialIcon::Add, MD3Dialog::Options{true, false})
    , m_presets(std::move(presets))
{
    const wxColour bg = GetBackgroundColour();
    wxBoxSizer    *content = GetContentSizer();
    const int      gap     = FromDIP(MD3::Metrics::active().gap);

    auto *intro = new Label(this, Label::Body_13,
                            _L("Every preset comes from the values the app ships with, your own saved settings, or a template the app ships. Nothing is applied until you start from one."),
                            LB_AUTO_WRAP);
    intro->SetBackgroundColour(bg);
    intro->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    intro->SetMinSize(wxSize(0, -1));
    content->Add(intro, 0, wxEXPAND | wxBOTTOM, gap);

    m_search = new SearchField(this, _L("Search presets"));
    m_search->SetName(_L("Search presets"));
    m_search->SetOnQuery([this](const wxString &) { filter(); });
    m_search->SetOnRegexToggle([this](bool) { filter(); });
    content->Add(m_search, 0, wxEXPAND | wxBOTTOM, gap);

    m_list = new ListBox(this, wxID_ANY, wxSize(-1, FromDIP(140)));
    m_list->SetName(_L("Presets to start from"));
    m_list->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &) { show_detail(); });
    m_list->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent &) { accept(); });
    content->Add(m_list, 0, wxEXPAND | wxBOTTOM, gap);

    m_no_match = new Label(this, Label::Body_13, _L("No preset matches the search. Clear the search to see every preset."), LB_AUTO_WRAP);
    m_no_match->SetBackgroundColour(bg);
    m_no_match->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_no_match->SetMinSize(wxSize(0, -1));
    m_no_match->Hide();
    content->Add(m_no_match, 0, wxEXPAND | wxBOTTOM, gap);

    auto *detail_title = new Label(this, Label::Body_13, _L("What the highlighted preset creates and sets"));
    detail_title->SetBackgroundColour(bg);
    content->Add(detail_title, 0, wxEXPAND | wxBOTTOM, FromDIP(4));
    m_detail = new TextAreaEditor(this, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(420, 180)),
                                  wxTE_MULTILINE | wxTE_READONLY);
    m_detail->SetName(_L("What the highlighted preset creates and sets"));
    content->Add(m_detail, 1, wxEXPAND);

    auto *cancel = AddFooterButton(new Button(this, _L("Cancel")));
    cancel->SetVariant(Button::Variant::Text);
    cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    m_start = AddFooterButton(new Button(this, _L("Start from this preset")));
    m_start->SetVariant(Button::Variant::Filled);
    m_start->SetName(_L("Start from this preset"));
    m_start->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { accept(); });

    // Enter starts from the highlighted preset unless the search box wants it;
    // Escape always cancels and returns focus to the editor that opened us.
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &event) {
        const int key = event.GetKeyCode();
        if (key == WXK_ESCAPE) {
            EndModal(wxID_CANCEL);
            return;
        }
        if ((key == WXK_RETURN || key == WXK_NUMPAD_ENTER) && FindFocus() == m_list) {
            accept();
            return;
        }
        event.Skip();
    });

    filter();
    SetMinSize(wxSize(FromDIP(480), FromDIP(520)));
    SetSize(wxSize(FromDIP(560), FromDIP(620)));
    Layout();
    CenterOnParent();
    m_list->SetFocus();
}

const Preset *StartFromPicker::chosen() const
{
    return m_chosen >= 0 && m_chosen < int(m_presets.size()) ? &m_presets[size_t(m_chosen)] : nullptr;
}

int StartFromPicker::highlighted() const
{
    const int row = m_list->GetSelection();
    return row >= 0 && row < int(m_visible.size()) ? int(m_visible[size_t(row)]) : -1;
}

void StartFromPicker::filter()
{
    const int previous = highlighted();
    SearchField::MatchPass pass(m_search->GetValue(), m_search->IsRegexEnabled(), m_search->IsCaseSensitive(),
                                m_search->IsWholeWord(), m_search->IsMultiline());
    m_visible.clear();
    std::vector<wxString> rows;
    for (size_t i = 0; i < m_presets.size(); ++i) {
        const wxString title = tr(m_presets[i].title);
        // The statement is searchable too, so "theme" finds every preset that sets it.
        if (!pass.matches(title + "\n" + describe_preset(m_presets[i])))
            continue;
        m_visible.push_back(i);
        rows.push_back(title);
    }
    m_list->Set(rows);
    int row = 0;
    for (size_t r = 0; r < m_visible.size(); ++r)
        if (int(m_visible[r]) == previous)
            row = int(r);
    if (!m_visible.empty())
        m_list->SetSelection(row);
    m_no_match->Show(m_visible.empty());
    show_detail();
    Layout();
}

void StartFromPicker::show_detail()
{
    const int index = highlighted();
    m_start->Enable(index >= 0);
    m_start->SetToolTip(index >= 0 ? _L("Start the new item from the highlighted preset") : _L("Highlight a preset first."));
    m_detail->ChangeValue(index >= 0 ? describe_preset(m_presets[size_t(index)]) : wxString());
}

void StartFromPicker::accept()
{
    const int index = highlighted();
    if (index < 0)
        return;
    m_chosen = index;
    EndModal(wxID_OK);
}

} } // namespace Slic3r::GUI
