#include "LabeledRadioButton.hpp"

#include "Label.hpp"
#include "StateColor.hpp"
#include "../I18N.hpp"

#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#include <wx/tglbtn.h>
#include <wx/tooltip.h>

#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

#include <algorithm>

namespace Slic3r { namespace GUI {

#if wxUSE_ACCESSIBILITY
class LabeledRadioButton::Accessible final : public wxWindowAccessible
{
public:
    explicit Accessible(LabeledRadioButton *row) : wxWindowAccessible(row), m_row(row) {}

    wxAccStatus GetName(int child_id, wxString *name) override
    {
        if (!name || child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        *name = m_row->GetLabel();
        if (name->IsEmpty()) *name = m_row->GetName();
        if (*name == wxASCII_STR(wxPanelNameStr)) name->clear();
        return name->IsEmpty() ? wxACC_NOT_IMPLEMENTED : wxACC_OK;
    }

    wxAccStatus GetRole(int child_id, wxAccRole *role) override
    {
        if (!role || child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        *role = wxROLE_SYSTEM_RADIOBUTTON;
        return wxACC_OK;
    }

    wxAccStatus GetState(int child_id, long *state) override
    {
        if (!state || child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        *state = 0;
        if (m_row->AcceptsFocusFromKeyboard()) *state |= wxACC_STATE_SYSTEM_FOCUSABLE;
        if (m_row->HasFocus()) *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (m_row->GetValue()) *state |= wxACC_STATE_SYSTEM_CHECKED;
        if (!m_row->IsEnabled()) *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        if (!m_row->IsShown()) *state |= wxACC_STATE_SYSTEM_INVISIBLE;
        return wxACC_OK;
    }

    wxAccStatus GetDefaultAction(int child_id, wxString *action_name) override
    {
        if (!action_name || child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        *action_name = _L("Select");
        return wxACC_OK;
    }

    wxAccStatus DoDefaultAction(int child_id) override
    {
        if (child_id != wxACC_SELF) return wxACC_NOT_IMPLEMENTED;
        if (!m_row->IsEnabled() || !m_row->IsShown()) return wxACC_FAIL;
        m_row->Activate();
        return wxACC_OK;
    }

private:
    LabeledRadioButton *m_row;
};
#endif

LabeledRadioButton::LabeledRadioButton(wxWindow *parent, const wxString &label, wxWindowID id)
    : wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(parent->GetBackgroundColour());
    wxPanel::SetLabel(label);

    m_radio = new RadioBox(this);
    m_label = new Label(this, label);
    m_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));

    // The glyph is a native BUTTON that would steal the focus the row owns; the
    // row is the single focusable, accessible object.
    m_radio->SetCanFocus(false);

    auto *sizer = new wxBoxSizer(wxHORIZONTAL);
    // 2 px inset leaves room for the focus ring around the glyph.
    sizer->Add(m_radio, 0, wxALIGN_CENTER_VERTICAL | wxALL, FromDIP(2));
    if (!label.IsEmpty())
        sizer->Add(m_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    else
        m_label->Hide();
    SetSizer(sizer);
    Layout();
    Fit();

    // Every user path funnels into Activate(): glyph click, label click, row
    // click, Space/Enter on the focused row.
    m_radio->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &) { Activate(); });
    m_label->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) { Activate(); });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) { Activate(); });
    Bind(wxEVT_KEY_DOWN, &LabeledRadioButton::onKey, this);
    Bind(wxEVT_PAINT, &LabeledRadioButton::onPaint, this);
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) { Refresh(); e.Skip(); });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) { Refresh(); e.Skip(); });
    Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &) { SetCursor(wxCURSOR_HAND); });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &) { SetCursor(wxCURSOR_ARROW); });

#if wxUSE_ACCESSIBILITY
    SetAccessible(new Accessible(this));
#endif
}

void LabeledRadioButton::Activate()
{
    if (!IsEnabled()) return;
    if (!HasFocus() && AcceptsFocusFromKeyboard()) SetFocus();
    // A radio can be selected by the user, never deselected by the user.
    m_radio->SetValue(true);
    emitSelected();
}

void LabeledRadioButton::emitSelected()
{
    wxCommandEvent event(wxEVT_RADIOBUTTON, GetId());
    event.SetEventObject(this);
    event.SetInt(1);
    GetEventHandler()->ProcessEvent(event);
    Refresh();
}

void LabeledRadioButton::onKey(wxKeyEvent &evt)
{
    const int code = evt.GetKeyCode();
    if (code == WXK_SPACE || code == WXK_RETURN || code == WXK_NUMPAD_ENTER) {
        Activate();
        return;
    }
    evt.Skip(); // arrows reach the RadioGroup through this row's handler chain
}

void LabeledRadioButton::onPaint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    if (!HasFocus()) return;
    // MD3 focus indicator: a 2 px Primary ring around the glyph target.
    const wxRect glyph = m_radio->GetRect();
    wxRect ring = glyph;
    ring.Inflate(FromDIP(2));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Primary), FromDIP(2)));
    dc.DrawRoundedRectangle(ring, ring.GetHeight() / 2.0);
}

bool LabeledRadioButton::GetValue() const { return m_radio->GetValue(); }

void LabeledRadioButton::SetValue(bool value)
{
    m_radio->SetValue(value);
    Refresh();
}

void LabeledRadioButton::SetLabel(const wxString &label)
{
    wxPanel::SetLabel(label);
    m_label->SetLabel(label);
    const bool show = !label.IsEmpty();
    if (show != m_label->IsShown()) {
        if (show && GetSizer()->GetItem(m_label) == nullptr)
            GetSizer()->Add(m_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
        m_label->Show(show);
    }
    Layout();
    Fit();
}

bool LabeledRadioButton::SetFont(const wxFont &font)
{
    const bool ok = wxPanel::SetFont(font);
    m_label->SetFont(font);
    Layout();
    Fit();
    return ok;
}

bool LabeledRadioButton::SetForegroundColour(const wxColour &colour)
{
    m_label->SetForegroundColour(colour);
    return wxPanel::SetForegroundColour(colour);
}

bool LabeledRadioButton::SetBackgroundColour(const wxColour &colour)
{
    const bool ok = wxPanel::SetBackgroundColour(colour);
    if (m_label) m_label->SetBackgroundColour(colour);
    if (m_radio) m_radio->SetBackgroundColour(colour);
    return ok;
}

void LabeledRadioButton::SetToolTip(const wxString &tip)
{
    wxPanel::SetToolTip(tip);
    m_radio->SetToolTip(tip);
    m_label->SetToolTip(tip);
}

bool LabeledRadioButton::Enable(bool enable)
{
    const bool ok = wxPanel::Enable(enable);
    if (enable) m_radio->Enable(); else m_radio->Disable();
    m_label->Enable(enable);
    Refresh();
    return ok;
}

void LabeledRadioButton::SetColorScheme(MD3::ColorScheme scheme) { m_radio->SetColorScheme(scheme); }

void LabeledRadioButton::Rescale()
{
    m_radio->Rescale();
    Layout();
    Fit();
}

// ---------------------------------------------------------------------------

RadioGroup::~RadioGroup()
{
    for (auto *b : m_buttons) {
        b->Unbind(wxEVT_RADIOBUTTON, &RadioGroup::onMemberSelected, this);
        b->Unbind(wxEVT_KEY_DOWN, &RadioGroup::onMemberKey, this);
    }
}

void RadioGroup::Add(LabeledRadioButton *button)
{
    if (!button || IndexOf(button) >= 0) return;
    m_buttons.push_back(button);
    button->Bind(wxEVT_RADIOBUTTON, &RadioGroup::onMemberSelected, this);
    button->Bind(wxEVT_KEY_DOWN, &RadioGroup::onMemberKey, this);
    button->Bind(wxEVT_DESTROY, [this, button](wxWindowDestroyEvent &e) {
        m_buttons.erase(std::remove(m_buttons.begin(), m_buttons.end(), button), m_buttons.end());
        e.Skip();
    });
}

int RadioGroup::IndexOf(const LabeledRadioButton *button) const
{
    for (int i = 0; i < GetCount(); ++i)
        if (m_buttons[i] == button) return i;
    return -1;
}

int RadioGroup::GetSelection() const
{
    for (int i = 0; i < GetCount(); ++i)
        if (m_buttons[i]->GetValue()) return i;
    return -1;
}

void RadioGroup::SetSelection(int index)
{
    for (int i = 0; i < GetCount(); ++i)
        m_buttons[i]->SetValue(i == index);
}

void RadioGroup::onMemberSelected(wxCommandEvent &evt)
{
    auto *selected = dynamic_cast<LabeledRadioButton *>(evt.GetEventObject());
    for (auto *b : m_buttons)
        if (b != selected && b->GetValue()) b->SetValue(false);
    evt.Skip(); // the dialog's own handler still runs
}

void RadioGroup::moveTo(int index)
{
    if (!GetCount()) return;
    // Skip members that cannot take the selection (hidden or disabled).
    for (int tries = 0; tries < GetCount(); ++tries) {
        const int i = ((index % GetCount()) + GetCount()) % GetCount();
        auto *b = m_buttons[i];
        if (b->IsShown() && b->IsEnabled()) { b->Activate(); return; }
        index = index < 0 ? i - 1 : i + 1;
    }
}

void RadioGroup::onMemberKey(wxKeyEvent &evt)
{
    auto *from = dynamic_cast<LabeledRadioButton *>(evt.GetEventObject());
    const int here = IndexOf(from);
    switch (evt.GetKeyCode()) {
    case WXK_UP: case WXK_LEFT:   moveTo(here - 1); return;
    case WXK_DOWN: case WXK_RIGHT: moveTo(here + 1); return;
    case WXK_HOME: moveTo(0); return;
    case WXK_END:  moveTo(GetCount() - 1); return;
    default: evt.Skip();
    }
}

}} // namespace Slic3r::GUI
