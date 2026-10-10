#include "LabeledCheckBox.hpp"

#include "Label.hpp"
#include "StateColor.hpp"
#include "../I18N.hpp"
#include "../PersonalVocabulary.hpp"

#include <wx/checkbox.h>
#include <wx/control.h>
#include <wx/sizer.h>
#include <wx/tglbtn.h>
#include <wx/tooltip.h>

#if wxUSE_ACCESSIBILITY
#include <wx/access.h>

// The glyph is the native BUTTON that takes the keyboard focus, and it has no
// window text, so without this peer Windows read it as an unnamed button. It
// answers as the stock wxCheckBox did: named by the row, the check box role,
// the checked state, and a default action that toggles like a click.
class LabeledCheckBox::Accessible final : public wxWindowAccessible
{
public:
    Accessible(CheckBox *glyph, LabeledCheckBox *row) : wxWindowAccessible(glyph), m_row(row) {}

    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (!name || child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        *name = m_row->accessibleName();
        return name->IsEmpty() ? wxACC_NOT_IMPLEMENTED : wxACC_OK;
    }

    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (!role || child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        *role = wxROLE_SYSTEM_CHECKBUTTON;
        return wxACC_OK;
    }

    wxAccStatus GetState(int child_id, long *state) override
    {
        if (!state || child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        const CheckBox *glyph = m_row->m_check;
        *state = 0;
        if (m_row->IsEnabled() && glyph->IsShownOnScreen()) *state |= wxACC_STATE_SYSTEM_FOCUSABLE;
        if (glyph->HasFocus()) *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (glyph->GetValue()) *state |= wxACC_STATE_SYSTEM_CHECKED;
        if (!m_row->IsEnabled()) *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        if (!glyph->IsShownOnScreen()) *state |= wxACC_STATE_SYSTEM_INVISIBLE;
        return wxACC_OK;
    }

    wxAccStatus GetDefaultAction(int child_id, wxString *action_name) override
    {
        if (!action_name || child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        *action_name = m_row->GetValue() ? _L("Uncheck") : _L("Check");
        return wxACC_OK;
    }

    wxAccStatus DoDefaultAction(int child_id) override
    {
        if (child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        if (!m_row->IsEnabled() || !m_row->m_check->IsShownOnScreen()) return wxACC_FAIL;
        m_row->toggleByUser();
        return wxACC_OK;
    }

private:
    LabeledCheckBox *m_row;
};
#endif

LabeledCheckBox::LabeledCheckBox(wxWindow *parent, const wxString &label, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL | wxBORDER_NONE)
{
    SetBackgroundColour(parent->GetBackgroundColour());
    m_check = new CheckBox(this);
    m_label = new Label(this, label);
    m_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));

    auto *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_check, 0, wxALIGN_CENTER_VERTICAL);
    if (!label.IsEmpty())
        sizer->Add(m_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    else
        m_label->Hide();
    SetSizer(sizer);
    Layout();
    Fit();

    // The glyph toggles itself; the label toggles the glyph. Both surface as
    // one wxEVT_CHECKBOX from this panel, the event the callers already bind.
    m_check->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) { emitChange(); e.Skip(); });
    m_label->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) { toggleByUser(); });

#if wxUSE_ACCESSIBILITY
    m_check->SetAccessible(new Accessible(m_check, this));
#endif
}

void LabeledCheckBox::toggleByUser()
{
    if (!IsEnabled()) return;
    m_check->SetValue(!m_check->GetValue());
    emitChange();
}

wxString LabeledCheckBox::accessibleName() const
{
    wxString name = m_check->GetName();
    if (name == wxASCII_STR(wxCheckBoxNameStr)) name.clear();
    if (name.IsEmpty()) {
        name = GetName();
        if (name == wxASCII_STR(wxPanelNameStr)) name.clear();
    }
    // The Label paints the personal-vocabulary form of its text, so the name
    // says the same words the row shows.
    if (name.IsEmpty())
        name = wxControl::RemoveMnemonics(Slic3r::GUI::PersonalVocabulary::display(m_label->GetUnwrappedLabel()));
    if (name.IsEmpty()) name = m_check->GetToolTipText();
    return name;
}

void LabeledCheckBox::emitChange()
{
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, m_check, wxOBJID_CLIENT, wxACC_SELF);
#endif
    wxCommandEvent event(wxEVT_CHECKBOX, GetId());
    event.SetEventObject(this);
    event.SetInt(m_check->GetValue() ? 1 : 0);
    GetEventHandler()->ProcessEvent(event);
}

bool LabeledCheckBox::GetValue() const { return m_check->GetValue(); }

void LabeledCheckBox::SetValue(bool value)
{
    const bool changed = m_check->GetValue() != value;
    m_check->SetValue(value);
#if wxUSE_ACCESSIBILITY
    if (changed) wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, m_check, wxOBJID_CLIENT, wxACC_SELF);
#else
    wxUnusedVar(changed);
#endif
}

void LabeledCheckBox::SetLabel(const wxString &label)
{
    wxPanel::SetLabel(label);
    m_label->SetLabel(label);
    m_label->Show(!label.IsEmpty());
    Layout();
    Fit();
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, m_check, wxOBJID_CLIENT, wxACC_SELF);
#endif
}

bool LabeledCheckBox::SetFont(const wxFont &font)
{
    const bool ok = wxPanel::SetFont(font);
    m_label->SetFont(font);
    Layout();
    Fit();
    return ok;
}

bool LabeledCheckBox::SetForegroundColour(const wxColour &colour)
{
    m_label->SetForegroundColour(colour);
    return wxPanel::SetForegroundColour(colour);
}

bool LabeledCheckBox::SetBackgroundColour(const wxColour &colour)
{
    const bool ok = wxPanel::SetBackgroundColour(colour);
    if (m_label) m_label->SetBackgroundColour(colour);
    if (m_check) m_check->SetBackgroundColour(colour);
    return ok;
}

void LabeledCheckBox::SetToolTip(const wxString &tip)
{
    wxPanel::SetToolTip(tip);
    m_check->SetToolTip(tip);
    m_label->SetToolTip(tip);
}

bool LabeledCheckBox::Enable(bool enable)
{
    const bool ok = wxPanel::Enable(enable);
    m_check->Enable(enable);
    m_label->Enable(enable);
    return ok;
}

void LabeledCheckBox::SetColorScheme(MD3::ColorScheme scheme) { m_check->SetColorScheme(scheme); }

void LabeledCheckBox::Rescale()
{
    m_check->Rescale();
    Layout();
    Fit();
}
