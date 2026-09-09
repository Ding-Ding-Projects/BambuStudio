#include "TabStripDialogs.hpp"

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "BoundedRegex.hpp"
#include "Button.hpp"
#include "Label.hpp"
#include "LabeledCheckBox.hpp"
#include "ListBox.hpp"
#include "MD3Tokens.hpp"
#include "SearchField.hpp"
#include "StateColor.hpp"
#include "TabStrip.hpp"
#include "TextInput.hpp"

#include <wx/display.h>
#include <wx/sizer.h>
#include <wx/vlbox.h>

namespace Slic3r { namespace GUI {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
void PlaceDialogBesideAnchor(wxDialog *dialog, wxWindow *anchor, const wxRect *anchor_rect_screen)
{
    if (!dialog)
        return;
    if (!anchor && !anchor_rect_screen) {
        dialog->CenterOnParent();
        return;
    }
    wxRect a = anchor_rect_screen ? *anchor_rect_screen : anchor->GetScreenRect();
    const int di = wxDisplay::GetFromPoint(wxPoint(a.x + a.width / 2, a.y + a.height / 2));
    const wxRect display = wxDisplay(di == wxNOT_FOUND ? 0u : unsigned(di)).GetClientArea();
    const wxSize sz      = dialog->GetSize();
    const int    gap     = dialog->FromDIP(8);
    wxPoint pos(a.x, a.GetBottom() + gap);
    if (pos.y + sz.y > display.GetBottom())
        pos.y = a.y - gap - sz.y;          // above
    if (pos.y < display.y)
        pos.y = display.y;                 // clamp
    if (pos.x + sz.x > display.GetRight())
        pos.x = display.GetRight() - sz.x; // slide left
    if (pos.x < display.x)
        pos.x = display.x;
    dialog->Move(pos);
}

MD3::Tabs::Matcher MatcherForField(const SearchField *field, bool *pattern_ok)
{
    const bool regex     = field && field->IsRegexEnabled();
    const bool case_sens = field && field->IsCaseSensitive();
    const bool whole     = field && field->IsWholeWord();
    const bool multiline = field && field->IsMultiline();
    if (pattern_ok) {
        *pattern_ok = true;
        if (regex && field) {
            wxString q = field->GetValue();
            q.Trim(true).Trim(false);
            if (!q.IsEmpty()) {
                BoundedRegex::Options opts;
                opts.case_sensitive = case_sens;
                opts.multiline      = multiline;
                const BoundedRegex::Result r = BoundedRegex::validate(q.ToStdWstring(), opts);
                *pattern_ok = r.status != BoundedRegex::Status::InvalidPattern;
            }
        }
    }
    return [regex, case_sens, whole, multiline](const wxString &query, const wxString &candidate) {
        return SearchField::textMatches(query, candidate, regex, case_sens, whole, multiline);
    };
}

namespace {

void bind_escape(wxDialog *dlg)
{
    dlg->Bind(wxEVT_CHAR_HOOK, [dlg](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_ESCAPE)
            dlg->EndModal(wxID_CANCEL);
        else
            e.Skip();
    });
}

Button *footer_button(MD3Dialog *dlg, const wxString &label, Button::Variant variant)
{
    auto *b = new Button(dlg, label);
    b->SetVariant(variant);
    b->SetButtonSize(Button::Size::Medium);
    dlg->AddFooterButton(b);
    return b;
}

// Forward list keyboard to the list from the search entry so Down / Up / Enter
// operate the results while typing.
void wire_search_to_list(SearchField *search, ListBox *list, const std::function<void()> &accept)
{
    if (!search || !list)
        return;
    auto *tc = search->GetTextCtrl();
    if (!tc)
        return;
    tc->Bind(wxEVT_KEY_DOWN, [list, accept](wxKeyEvent &e) {
        const int k = e.GetKeyCode();
        if (k == WXK_DOWN || k == WXK_UP) {
            const int n = int(list->GetCount());
            if (n > 0) {
                int sel = list->GetSelection();
                sel     = k == WXK_DOWN ? std::min(n - 1, sel + 1) : std::max(0, sel - 1);
                list->SetSelection(sel);
            }
            return;
        }
        if (k == WXK_RETURN || k == WXK_NUMPAD_ENTER) {
            accept();
            return;
        }
        e.Skip();
    });
    list->Bind(wxEVT_KEY_DOWN, [accept](wxKeyEvent &e) {
        const int k = e.GetKeyCode();
        if (k == WXK_RETURN || k == WXK_NUMPAD_ENTER) {
            accept();
            return;
        }
        e.Skip();
    });
    list->Bind(wxEVT_LISTBOX_DCLICK, [accept](wxCommandEvent &) { accept(); });
}

} // namespace

// ---------------------------------------------------------------------------
// GroupNameDialog
// ---------------------------------------------------------------------------
GroupNameDialog::GroupNameDialog(wxWindow *parent, const wxString &title, const wxString &value)
    : MD3Dialog(parent, title, wxEmptyString, MaterialIcon::Edit)
{
    const wxString prompt_text = _L("Group name") + ":";
    auto *prompt = new ::Label(this, ::Label::Body_14, prompt_text);
    prompt->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    GetContentSizer()->Add(prompt, 0, wxEXPAND);

    const int field_h = FromDIP(MD3::Metrics::active().row_height);
    m_input = new ::TextInput(this, value, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(320), field_h));
    m_input->SetMinSize(wxSize(FromDIP(320), field_h));
    m_input->SetName(prompt_text);
    if (auto *tc = m_input->GetTextCtrl())
        tc->SetName(prompt_text);
    GetContentSizer()->Add(m_input, 0, wxEXPAND | wxTOP, FromDIP(10));
    m_input->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { EndModal(wxID_OK); });

    footer_button(this, _L("Cancel"), Button::Variant::Text)->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    footer_button(this, _L("OK"), Button::Variant::Filled)->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_OK); });
    bind_escape(this);

    SetMinSize(wxSize(FromDIP(420), -1));
    Layout();
    Fit();
    CenterOnParent();
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
    if (auto *tc = m_input->GetTextCtrl()) {
        tc->SetFocus();
        tc->SetSelection(-1, -1);
    }
}

wxString GroupNameDialog::GetValue() const
{
    wxString v = m_input && m_input->GetTextCtrl() ? m_input->GetTextCtrl()->GetValue() : wxString();
    return v.Trim(true).Trim(false);
}

// ---------------------------------------------------------------------------
// MoveToGroupDialog
// ---------------------------------------------------------------------------
MoveToGroupDialog::MoveToGroupDialog(wxWindow *parent, wxWindow *anchor, const MD3::Tabs::Model &model, const std::string &tab_id)
    : MD3Dialog(parent, _L("Move into group"), wxEmptyString, MaterialIcon::AccountTree), m_model(model), m_tab_id(tab_id)
{
    if (const MD3::Tabs::Tab *t = m_model.find(tab_id))
        SetHeaderSubtitle(t->title);

    m_search = new SearchField(this, _L("Search groups"));
    m_search->SetName(_L("Search groups"));
    GetContentSizer()->Add(m_search, 0, wxEXPAND);

    m_list = new ListBox(this, wxID_ANY, wxSize(FromDIP(360), FromDIP(220)));
    m_list->SetName(_L("Groups"));
    GetContentSizer()->Add(m_list, 1, wxEXPAND | wxTOP, FromDIP(10));

    m_empty = new ::Label(this, ::Label::Body_13, _L("No groups yet. Create one to move this tab into it."));
    m_empty->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    GetContentSizer()->Add(m_empty, 0, wxTOP, FromDIP(6));

    auto *create = footer_button(this, _L("New group..."), Button::Variant::Outlined);
    create->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        m_create = true;
        EndModal(wxID_OK);
    });
    footer_button(this, _L("Cancel"), Button::Variant::Text)->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    auto *move = footer_button(this, _L("Move"), Button::Variant::Filled);
    move->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Accept(); });
    bind_escape(this);

    m_search->SetOnQuery([this](const wxString &) { Refilter(); });
    m_search->SetOnRegexToggle([this](bool) { Refilter(); });
    wire_search_to_list(m_search, m_list, [this]() { Accept(); });
    Refilter();

    SetMinSize(wxSize(FromDIP(420), -1));
    Layout();
    Fit();
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
    PlaceDialogBesideAnchor(this, anchor);
    if (auto *tc = m_search->GetTextCtrl())
        tc->SetFocus();
}

void MoveToGroupDialog::Refilter()
{
    m_visible_groups.clear();
    std::vector<wxString> rows;
    const wxString query   = m_search ? m_search->GetValue() : wxString();
    const auto     matcher = MatcherForField(m_search);
    const MD3::Tabs::Tab *self = m_model.find(m_tab_id);
    for (const MD3::Tabs::Group &g : m_model.groups()) {
        if (self && self->group_id == g.id)
            continue; // already there
        if (!matcher(query, g.name))
            continue;
        const int members = int(m_model.members(g.id).size());
        wxString  row     = g.name;
        row << wxString::FromUTF8("  \xC2\xB7  ") << SearchField::colorSearchText(g.color)
            << wxString::FromUTF8("  \xC2\xB7  ") << wxString::Format(_L("%d tabs"), members);
        if (g.collapsed)
            row << "  (" << _L("collapsed") << ")";
        rows.push_back(row);
        m_visible_groups.push_back(g.id);
    }
    m_list->Set(rows);
    if (!rows.empty())
        m_list->SetSelection(0);
    const bool none_at_all = m_model.groups().empty() || (self && m_model.groups().size() == 1 && self->group_id >= 0);
    m_empty->SetLabel(none_at_all ? _L("No groups yet. Create one to move this tab into it.")
                                  : (rows.empty() ? _L("No group matches this search.") : wxString()));
    m_empty->Show(rows.empty());
    Layout();
}

void MoveToGroupDialog::Accept()
{
    const int sel = m_list ? m_list->GetSelection() : wxNOT_FOUND;
    if (sel == wxNOT_FOUND || sel >= int(m_visible_groups.size()))
        return;
    m_selected_group = m_visible_groups[sel];
    EndModal(wxID_OK);
}

// ---------------------------------------------------------------------------
// BulkCloseDialog
// ---------------------------------------------------------------------------
BulkCloseDialog::BulkCloseDialog(wxWindow *parent, wxWindow *anchor, const MD3::Tabs::Model &model, bool not_containing,
                                 const wxString &close_verb)
    : MD3Dialog(parent, not_containing ? _L("Close tabs not containing text") : _L("Close tabs containing text"),
                wxEmptyString, MaterialIcon::FilterNone)
    , m_model(model), m_not_containing(not_containing), m_close_verb(close_verb)
{
    m_search = new SearchField(this, _L("Text to match in tab titles"));
    m_search->SetName(_L("Text to match in tab titles"));
    GetContentSizer()->Add(m_search, 0, wxEXPAND);

    m_invert = new LabeledCheckBox(this, _L("Close the tabs that do NOT contain the text"));
    m_invert->SetValue(not_containing);
    GetContentSizer()->Add(m_invert, 0, wxTOP, FromDIP(10));

    m_pinned = new LabeledCheckBox(this, _L("Include pinned tabs"));
    m_pinned->SetValue(false);
    GetContentSizer()->Add(m_pinned, 0, wxTOP, FromDIP(4));

    m_summary = new ::Label(this, ::Label::Body_13, wxEmptyString);
    m_summary->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    GetContentSizer()->Add(m_summary, 0, wxEXPAND | wxTOP, FromDIP(10));

    m_list = new ListBox(this, wxID_ANY, wxSize(FromDIP(380), FromDIP(180)));
    m_list->SetName(_L("Tabs that will close"));
    GetContentSizer()->Add(m_list, 1, wxEXPAND | wxTOP, FromDIP(6));

    footer_button(this, _L("Cancel"), Button::Variant::Text)->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    m_close = footer_button(this, close_verb, Button::Variant::Filled);
    m_close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (m_preview.valid && !m_preview.ids.empty())
            EndModal(wxID_OK);
    });
    bind_escape(this);

    m_search->SetOnQuery([this](const wxString &) { Refresh_(); });
    m_search->SetOnRegexToggle([this](bool) { Refresh_(); });
    m_invert->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { Refresh_(); });
    m_pinned->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { Refresh_(); });
    if (auto *tc = m_search->GetTextCtrl())
        tc->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) {
            if (m_preview.valid && !m_preview.ids.empty())
                EndModal(wxID_OK);
        });
    Refresh_();

    SetMinSize(wxSize(FromDIP(440), -1));
    Layout();
    Fit();
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
    PlaceDialogBesideAnchor(this, anchor);
    if (auto *tc = m_search->GetTextCtrl())
        tc->SetFocus();
}

void BulkCloseDialog::Refresh_()
{
    bool                     pattern_ok = true;
    const MD3::Tabs::Matcher matcher    = MatcherForField(m_search, &pattern_ok);
    MD3::Tabs::ClosePredicate p;
    p.text           = m_search->GetValue();
    p.regex          = m_search->IsRegexEnabled();
    p.invert         = m_invert->GetValue();
    p.include_pinned = m_pinned->GetValue();
    m_preview        = MD3::Tabs::preview_bulk_close(m_model, p, matcher, pattern_ok);

    std::vector<wxString> rows;
    if (m_preview.valid)
        rows = m_preview.titles;
    m_list->Set(rows);

    wxString summary;
    if (!m_preview.valid) {
        wxString q = p.text;
        q.Trim(true).Trim(false);
        summary = q.IsEmpty() ? _L("Enter text to match before closing tabs.")
                              : _L("The pattern is not a valid regular expression.");
    } else {
        summary = wxString::Format(_L("%d tabs will close"), int(m_preview.ids.size()));
        summary << wxString::FromUTF8("  \xC2\xB7  ") << (p.invert ? _L("Not containing text") : _L("Containing text"))
                << wxString::FromUTF8(" \xC2\xB7 ") << (p.regex ? _L("regex") : _L("plain text"));
        if (m_preview.protected_pinned > 0)
            summary << wxString::FromUTF8("  \xC2\xB7  ")
                    << wxString::Format(_L("%d pinned tabs kept"), m_preview.protected_pinned);
    }
    m_summary->SetLabel(summary);
    const bool can_close = m_preview.valid && !m_preview.ids.empty();
    m_close->Enable(can_close);
    m_close->SetLabel(can_close ? wxString::Format("%s (%d)", m_close_verb, int(m_preview.ids.size())) : m_close_verb);
    m_close->SetToolTip(can_close ? wxString() : (m_preview.valid ? _L("No tab matches this text.") : summary));
    Layout();
}

// ---------------------------------------------------------------------------
// TabSearchDialog
// ---------------------------------------------------------------------------
namespace {
wxString scope_title(TabSearchDialog::Scope s)
{
    switch (s) {
    case TabSearchDialog::Scope::Strip: return _L("Search tabs");
    case TabSearchDialog::Scope::Group: return _L("Search tabs in group");
    case TabSearchDialog::Scope::Groups: return _L("Search tab groups");
    case TabSearchDialog::Scope::Master: return _L("Search all tabs");
    }
    return _L("Search tabs");
}
} // namespace

TabSearchDialog::TabSearchDialog(wxWindow *parent, wxWindow *anchor, Scope scope, std::vector<TabStrip *> strips, int group_id)
    : MD3Dialog(parent, scope_title(scope), wxEmptyString, MaterialIcon::Search)
    , m_scope(scope), m_strips(std::move(strips)), m_group_id(group_id)
{
    if (scope == Scope::Group && !m_strips.empty())
        if (const MD3::Tabs::Group *g = m_strips.front()->GetModel().group(group_id))
            SetHeaderSubtitle(g->name);
    if (scope == Scope::Master)
        SetHeaderSubtitle(wxString::Format(_L("%d tab strips"), int(m_strips.size())));

    m_search = new SearchField(this, scope_title(scope));
    m_search->SetName(scope_title(scope));
    GetContentSizer()->Add(m_search, 0, wxEXPAND);

    m_list = new ListBox(this, wxID_ANY, wxSize(FromDIP(460), FromDIP(240)));
    m_list->SetName(_L("Results"));
    GetContentSizer()->Add(m_list, 1, wxEXPAND | wxTOP, FromDIP(10));

    m_empty = new ::Label(this, ::Label::Body_13, wxEmptyString);
    m_empty->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    GetContentSizer()->Add(m_empty, 0, wxTOP, FromDIP(6));

    footer_button(this, _L("Cancel"), Button::Variant::Text)->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    footer_button(this, _L("Go to tab"), Button::Variant::Filled)->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Accept(); });
    bind_escape(this);

    m_search->SetOnQuery([this](const wxString &) { Refilter(); });
    m_search->SetOnRegexToggle([this](bool) { Refilter(); });
    wire_search_to_list(m_search, m_list, [this]() { Accept(); });
    Refilter();

    SetMinSize(wxSize(FromDIP(520), -1));
    Layout();
    Fit();
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
    PlaceDialogBesideAnchor(this, anchor);
    if (auto *tc = m_search->GetTextCtrl())
        tc->SetFocus();
}

void TabSearchDialog::Refilter()
{
    m_rows.clear();
    const wxString query   = m_search->GetValue();
    const auto     matcher = MatcherForField(m_search);
    for (TabStrip *strip : m_strips) {
        if (!strip)
            continue;
        const auto &opt = strip->GetOptions();
        std::vector<MD3::Tabs::SearchHit> hits;
        if (m_scope == Scope::Groups)
            hits = MD3::Tabs::search_groups(strip->GetModel(), opt.surface_name, opt.strip_name, query, matcher);
        else
            hits = MD3::Tabs::search_tabs(strip->GetModel(), opt.surface_name, opt.strip_name, query, matcher,
                                          m_scope == Scope::Group ? m_group_id : -1);
        for (auto &h : hits)
            m_rows.push_back({strip, h});
    }
    std::vector<wxString> rows;
    for (const Row &r : m_rows)
        rows.push_back(r.hit.describe());
    m_list->Set(rows);
    if (!rows.empty())
        m_list->SetSelection(0);
    m_empty->SetLabel(rows.empty() ? (query.IsEmpty() ? _L("Nothing to list.") : _L("No tab matches this search."))
                                   : wxString::Format(_L("%d results"), int(rows.size())));
    Layout();
}

void TabSearchDialog::Accept()
{
    const int sel = m_list ? m_list->GetSelection() : wxNOT_FOUND;
    if (sel == wxNOT_FOUND || sel >= int(m_rows.size()))
        return;
    m_chosen_strip = m_rows[sel].strip;
    m_chosen_hit   = m_rows[sel].hit;
    EndModal(wxID_OK);
}

}} // namespace Slic3r::GUI
