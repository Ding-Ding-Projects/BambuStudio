#include "CollapsibleFilterBar.hpp"

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../PersonalVocabulary.hpp"
#include "Label.hpp"
#include "MD3Tokens.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"
#include "StaticBox.hpp"

#include "libslic3r/AppConfig.hpp"

#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

namespace CF = Slic3r::GUI::CollapsibleFilters;

namespace {

constexpr int kChevronPx = 18;

CF::Section::Read config_reader()
{
    return [](const std::string &section, const std::string &key) {
        Slic3r::AppConfig *config = Slic3r::GUI::wxGetApp().app_config;
        return config != nullptr ? config->get(section, key) : std::string();
    };
}

CF::Section::Write config_writer()
{
    return [](const std::string &section, const std::string &key, const std::string &value) {
        if (Slic3r::AppConfig *config = Slic3r::GUI::wxGetApp().app_config)
            config->set(section, key, value);
    };
}

bool focus_is_inside(const wxWindow *container)
{
    for (const wxWindow *w = wxWindow::FindFocus(); w != nullptr; w = w->GetParent()) {
        if (w == container)
            return true;
        if (w->IsTopLevel())
            break;
    }
    return false;
}

#if wxUSE_ACCESSIBILITY
// The header is a disclosure button: push-button role with the expanded /
// collapsed pair the MSAA bridge maps, a name that carries the active-filter
// count and a description that repeats the disclosure line.
class CollapsibleFilterBarAccessible final : public wxWindowAccessible
{
public:
    explicit CollapsibleFilterBarAccessible(CollapsibleFilterBar *bar) : wxWindowAccessible(bar), m_bar(bar) {}

    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (child_id != wxACC_SELF || name == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        *name = Slic3r::GUI::PersonalVocabulary::display(m_bar->AccessibleName());
        return wxACC_OK;
    }

    wxAccStatus GetDescription(int child_id, wxString *description) override
    {
        if (child_id != wxACC_SELF || description == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        *description = Slic3r::GUI::PersonalVocabulary::display(m_bar->AccessibleDescription());
        return wxACC_OK;
    }

    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (child_id != wxACC_SELF || role == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        *role = wxROLE_SYSTEM_PUSHBUTTON;
        return wxACC_OK;
    }

    wxAccStatus GetState(int child_id, long *state) override
    {
        if (child_id != wxACC_SELF || state == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        *state = 0;
        if (m_bar->IsKeyboardFocusable())
            *state |= wxACC_STATE_SYSTEM_FOCUSABLE;
        if (m_bar->HasFocus())
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        *state |= CF::accessible_state(m_bar->IsExpanded()) == CF::AccessibleState::Expanded
                      ? wxACC_STATE_SYSTEM_EXPANDED
                      : wxACC_STATE_SYSTEM_COLLAPSED;
        if (!m_bar->IsEnabled())
            *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        if (!m_bar->IsShown())
            *state |= wxACC_STATE_SYSTEM_INVISIBLE;
        return wxACC_OK;
    }

    wxAccStatus GetDefaultAction(int child_id, wxString *action_name) override
    {
        if (child_id != wxACC_SELF || action_name == nullptr)
            return wxACC_NOT_IMPLEMENTED;
        // TRN: Screen reader action of a collapsible filter or statistics header.
        *action_name = m_bar->IsExpanded() ? _L("Collapse") : _L("Expand");
        return wxACC_OK;
    }

    wxAccStatus DoDefaultAction(int child_id) override
    {
        if (child_id != wxACC_SELF)
            return wxACC_NOT_IMPLEMENTED;
        if (m_bar->IsEnabled() && m_bar->IsShown())
            m_bar->Toggle();
        return wxACC_OK;
    }

private:
    CollapsibleFilterBar *m_bar;
};
#endif

} // namespace

CollapsibleFilterBar::CollapsibleFilterBar(wxWindow *parent, const std::string &surface_id, const wxString &title,
                                           Purpose purpose, Layout layout)
    : Button(parent, title)
    , m_title(title)
    , m_state(surface_id, purpose)
{
    SetVariant(Button::Variant::Text);
    SetButtonSize(Button::Size::Small);
#if wxUSE_ACCESSIBILITY
    // Replaces the generic push-button bridge Button::Create installed.
    SetAccessible(new CollapsibleFilterBarAccessible(this));
#endif
    // TRN: Tooltip of the header that collapses a search bar, filter row or statistics panel.
    SetToolTip(_L("Show or hide these controls. Your choice is remembered."));

    const wxColour background = StaticBox::GetParentBackgroundColor(parent);

    m_summary = new Label(parent, Label::Body_13, wxEmptyString, LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    m_summary->SetMinSize(wxSize(0, -1));
    m_summary->SetCursor(wxCursor(wxCURSOR_HAND));
    m_summary->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &event) {
        SetExpanded(true);
        event.Skip();
    });
    m_summary->Hide();

    // The body carries the host background explicitly: custom-drawn children
    // read their parent's colour to paint around rounded corners.
    m_body = new wxPanel(parent, wxID_ANY);
    m_body->SetBackgroundColour(background);
    m_body->SetName(title);
    m_body->SetSizer(new wxBoxSizer(wxVERTICAL));

    if (layout == Layout::Inline) {
        // Toolbar form: header, then either the disclosure line (collapsed)
        // or the controls (expanded) on the same row.
        auto *section = new wxBoxSizer(wxHORIZONTAL);
        section->Add(this, 0, wxALIGN_CENTER_VERTICAL);
        section->Add(m_summary, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        section->Add(m_body, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        m_section = section;
    } else {
        auto *section = new wxBoxSizer(wxVERTICAL);
        section->Add(this, 0, wxALIGN_LEFT);
        section->Add(m_summary, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(8));
        section->Add(m_body, 0, wxEXPAND | wxTOP, FromDIP(4));
        m_section = section;
    }

    // Handled here and not propagated: a host binding wxEVT_BUTTON on itself
    // must not mistake the disclosure header for one of its own actions.
    Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Toggle(); });
    // Follow the host background when the host re-themes and repaints.
    Bind(wxEVT_PAINT, [this](wxPaintEvent &event) {
        SyncTheme();
        event.Skip();
    });

    SyncTheme();
    m_state.restore(config_reader());
    applyState(/*user_change=*/false);
}

wxSizer *CollapsibleFilterBar::GetBodySizer() const { return m_body->GetSizer(); }

void CollapsibleFilterBar::SetActiveFilters(const std::vector<wxString> &labels)
{
    if (labels == m_active)
        return;
    m_active = labels;
    updateDisclosure(/*notify=*/true);
    relayout();
}

void CollapsibleFilterBar::SetExpanded(bool expanded, bool remember)
{
    if (!m_state.set_expanded(expanded, remember ? config_writer() : CF::Section::Write()))
        return;
    applyState(/*user_change=*/true);
    if (m_on_toggled)
        m_on_toggled(expanded);
}

void CollapsibleFilterBar::ShowSection(bool show)
{
    if (show == m_section_shown)
        return;
    m_section_shown = show;
    if (!show && focus_is_inside(m_body))
        GetParent()->SetFocus();
    applyVisibility();
}

void CollapsibleFilterBar::applyVisibility()
{
    const bool expanded = m_state.expanded();
    Show(m_section_shown);
    m_body->Show(m_section_shown && expanded);
    m_summary->Show(m_section_shown && !m_disclosure_text.IsEmpty());
}

void CollapsibleFilterBar::applyState(bool user_change)
{
    const bool expanded = m_state.expanded();
    SetGlyph(expanded ? MaterialIcon::ExpandLess : MaterialIcon::ExpandMore, kChevronPx);
    if (!expanded && focus_is_inside(m_body))
        SetFocus();
    updateDisclosure(user_change);
    applyVisibility();
#if wxUSE_ACCESSIBILITY
    if (user_change)
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
    if (user_change)
        relayout();
}

void CollapsibleFilterBar::updateDisclosure(bool notify)
{
    std::vector<std::string> labels;
    labels.reserve(m_active.size());
    for (const wxString &label : m_active)
        labels.emplace_back(label.ToUTF8().data());
    const CF::Disclosure disclosure = CF::disclose(labels, m_state.expanded());

    wxString text;
    if (disclosure.visible) {
        if (disclosure.named.empty()) {
            // TRN: Shown under a collapsed filter row; %d is how many filters still narrow the list.
            text = wxString::Format(_L("Active filters (%d)"), static_cast<int>(disclosure.active));
        } else {
            wxString list;
            for (const std::string &name : disclosure.named) {
                if (!list.IsEmpty())
                    list += wxString::FromUTF8(" \xC2\xB7 ");
                list += wxString::FromUTF8(name.c_str());
            }
            if (disclosure.more > 0)
                // TRN: Appended to the active filter list of a collapsed filter row; %d filters are not named.
                list += wxString::FromUTF8(" \xC2\xB7 ") + wxString::Format(_L("+%d more"), static_cast<int>(disclosure.more));
            // TRN: Shown under a collapsed filter row. %d is the number of active filters, %s lists them.
            text = wxString::Format(_L("Active filters (%d): %s"), static_cast<int>(disclosure.active), list);
        }
    }

    // TRN: Collapsible filter header. %s is the section title, %d the number of active filters.
    const wxString header = disclosure.active > 0
        ? wxString::Format(_L("%s (%d active)"), m_title, static_cast<int>(disclosure.active))
        : m_title;
    const bool header_changed = header != GetLabel();
    const bool text_changed   = text != m_disclosure_text;
    if (header_changed)
        SetLabel(header); // Button::SetLabel fires the name-change event itself.
    m_disclosure_text = text;
    if (text_changed)
        m_summary->SetLabel(text);
    m_summary->Show(m_section_shown && disclosure.visible);
#if wxUSE_ACCESSIBILITY
    if (notify && text_changed)
        wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_DESCRIPTIONCHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#else
    (void) notify;
#endif
}

wxString CollapsibleFilterBar::FilterLabel(const wxString &name, const wxString &value)
{
    constexpr size_t kMaxValue = 32;
    wxString shown = value;
    shown.Trim(true).Trim(false);
    if (shown.length() > kMaxValue)
        shown = shown.Left(kMaxValue - 1) + wxString::FromUTF8("\xE2\x80\xA6");
    // TRN: One active filter under a collapsed filter row: %1$s names the filter, %2$s is its value.
    return wxString::Format(_L("%s: %s"), name, shown);
}

wxString CollapsibleFilterBar::SearchFilterLabel(const wxString &query)
{
    wxString trimmed = query;
    trimmed.Trim(true).Trim(false);
    if (trimmed.IsEmpty())
        return wxString();
    return FilterLabel(_L("Search"), trimmed);
}

wxString CollapsibleFilterBar::ExcludedFilterLabel(const wxString &what)
{
    // TRN: One active filter under a collapsed filter row: a category that is switched off.
    return wxString::Format(_L("Hiding %s"), what);
}

wxString CollapsibleFilterBar::AccessibleName() const
{
    // The visible header already carries the active count.
    return GetLabel();
}

void CollapsibleFilterBar::SyncTheme()
{
    wxWindow *parent = GetParent();
    if (parent == nullptr || m_body == nullptr || m_summary == nullptr)
        return;
    const wxColour background = StaticBox::GetParentBackgroundColor(parent);
    const wxColour foreground = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    if (background == m_synced_background && m_summary->GetForegroundColour() == foreground)
        return;
    m_synced_background = background;
    m_body->SetBackgroundColour(background);
    m_summary->SetBackgroundColour(background);
    m_summary->SetForegroundColour(foreground);
    m_body->Refresh();
    m_summary->Refresh();
}

void CollapsibleFilterBar::Rescale()
{
    Button::Rescale();
    SetGlyph(IsExpanded() ? MaterialIcon::ExpandLess : MaterialIcon::ExpandMore, kChevronPx);
    relayout();
}

void CollapsibleFilterBar::relayout()
{
    wxWindow *parent = GetParent();
    if (parent == nullptr)
        return;
    parent->Layout();
    // Inside a scrolled host the virtual height changes with the body.
    if (dynamic_cast<wxScrollHelper *>(parent) != nullptr)
        parent->FitInside();
    parent->Refresh();
}
