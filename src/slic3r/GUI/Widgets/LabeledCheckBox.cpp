#include "LabeledCheckBox.hpp"

#include "Label.hpp"
#include "StateColor.hpp"

#include <wx/sizer.h>
#include <wx/tglbtn.h>
#include <wx/tooltip.h>

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
    m_label->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) {
        if (!IsEnabled()) return;
        m_check->SetValue(!m_check->GetValue());
        emitChange();
    });
}

void LabeledCheckBox::emitChange()
{
    wxCommandEvent event(wxEVT_CHECKBOX, GetId());
    event.SetEventObject(this);
    event.SetInt(m_check->GetValue() ? 1 : 0);
    GetEventHandler()->ProcessEvent(event);
}

bool LabeledCheckBox::GetValue() const { return m_check->GetValue(); }

void LabeledCheckBox::SetValue(bool value) { m_check->SetValue(value); }

void LabeledCheckBox::SetLabel(const wxString &label)
{
    wxPanel::SetLabel(label);
    m_label->SetLabel(label);
    m_label->Show(!label.IsEmpty());
    Layout();
    Fit();
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
