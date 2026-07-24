#include "StopPrintGate.hpp"

#include "I18N.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/SlideToConfirm.hpp"
#include "Widgets/StateColor.hpp"

#include <wx/dcbuffer.h>
#include <wx/panel.h>
#include <wx/sizer.h>

namespace Slic3r::GUI {

StopPrintGateDialog::StopPrintGateDialog(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, _L("Stop print - safety interlock"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE)
{
    const wxColour surface = StateColor::semantic(MD3::Role::Surface);
    SetBackgroundColour(surface);
    auto *root = new wxBoxSizer(wxVERTICAL);

    auto *title = new Label(this, Label::Head_16, _L("Stopping discards the print in progress."));
    title->SetBackgroundColour(surface);
    title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    root->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));
    m_stage_label = new Label(this, Label::Body_13, wxEmptyString);
    m_stage_label->SetBackgroundColour(surface);
    m_stage_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    root->Add(m_stage_label, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    // --- Stage 1: two key switches -----------------------------------------
    auto *keys_row = new wxBoxSizer(wxHORIZONTAL);
    for (int k = 0; k < 2; ++k) {
        auto *sw = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(64), FromDIP(64)));
        sw->SetBackgroundStyle(wxBG_STYLE_PAINT);
        m_key_switches[k] = sw;
        sw->Bind(wxEVT_PAINT, [this, k, sw](wxPaintEvent &) {
            wxAutoBufferedPaintDC dc(sw);
            dc.SetBackground(wxBrush(GetBackgroundColour()));
            dc.Clear();
            const wxSize sz = sw->GetClientSize();
            const int cx = sz.x / 2, cy = sz.y / 2, r = std::min(cx, cy) - FromDIP(4);
            dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Outline), 2));
            dc.SetBrush(wxBrush(StateColor::semantic(m_keys[k] ? MD3::Role::PrimaryContainer
                                                               : MD3::Role::SurfaceContainerHighest)));
            dc.DrawCircle(cx, cy, r);
            // the "key": a thick slot, vertical when off, horizontal when on
            dc.SetPen(wxPen(StateColor::semantic(m_keys[k] ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant),
                            FromDIP(6)));
            if (m_keys[k])
                dc.DrawLine(cx - r + FromDIP(8), cy, cx + r - FromDIP(8), cy);
            else
                dc.DrawLine(cx, cy - r + FromDIP(8), cx, cy + r - FromDIP(8));
        });
        sw->Bind(wxEVT_LEFT_DOWN, [this, k](wxMouseEvent &) {
            m_keys[k] = !m_keys[k];
            m_key_switches[k]->Refresh();
            update_stage();
        });
        keys_row->Add(sw, 0, wxRIGHT, FromDIP(14));
    }
    root->Add(keys_row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    // --- Stage 2: three arming buttons, two presses each --------------------
    auto *arm_row = new wxBoxSizer(wxHORIZONTAL);
    for (int b = 0; b < 3; ++b) {
        auto *btn = new Button(this, wxString::Format(_L("Arm %d (2)"), b + 1));
        btn->SetMinSize(wxSize(FromDIP(110), FromDIP(40)));
        m_arm_buttons[b] = btn;
        btn->Bind(wxEVT_BUTTON, [this, b](wxCommandEvent &) {
            if (m_presses[b] >= 2)
                return;
            ++m_presses[b];
            if (m_presses[b] >= 2)
                // TRN: %d is the arming button's number once fully pressed.
                m_arm_buttons[b]->SetLabel(wxString::Format(_L("Armed %d"), b + 1));
            else
                m_arm_buttons[b]->SetLabel(wxString::Format(_L("Arm %d (1)"), b + 1));
            update_stage();
        });
        arm_row->Add(btn, 0, wxRIGHT, FromDIP(10));
    }
    root->Add(arm_row, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    // --- Stage 3: slide to confirm ------------------------------------------
    m_slider = new SlideToConfirm(this, _L("Slide to open the safety cover"), _L("Cover unlocked"));
    m_slider->Enable(false);
    m_slider->SetOnConfirm([this]() {
        m_slid = true;
        update_stage();
    });
    root->Add(m_slider, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    // --- Stage 4: the cover and, under it, THE button ------------------------
    m_cover = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(200), FromDIP(56)));
    m_cover->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_cover->Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(m_cover);
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        const wxSize sz = m_cover->GetClientSize();
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Error), 2));
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::ErrorContainer)));
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, FromDIP(10));
        // hazard stripes
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Error), FromDIP(4)));
        for (int x = -sz.y; x < sz.x; x += FromDIP(18))
            dc.DrawLine(x, sz.y, x + sz.y, 0);
        dc.SetTextForeground(StateColor::semantic(MD3::Role::OnErrorContainer));
        dc.SetFont(Label::Head_12);
        const wxString cap = _L("Lift cover");
        const wxSize te = dc.GetTextExtent(cap);
        dc.DrawText(cap, (sz.x - te.x) / 2, (sz.y - te.y) / 2);
    });
    m_cover->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) {
        if (!m_slid)
            return;
        m_cover_open = true;
        m_cover->Hide();
        m_stop->Show();
        Layout();
        update_stage();
    });
    root->Add(m_cover, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    m_stop = new Button(this, _L("STOP PRINT"));
    m_stop->SetVariant(Button::Variant::Danger);
    m_stop->SetMinSize(wxSize(FromDIP(200), FromDIP(56)));
    m_stop->Hide();
    m_stop->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        EndModal(wxID_OK);
        if (m_on_stop)
            m_on_stop();
    });
    root->Add(m_stop, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    auto *keep = new Button(this, _L("Keep printing"), "", 0, 0, wxID_CANCEL);
    keep->SetMinSize(wxSize(FromDIP(140), FromDIP(40)));
    keep->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    actions->AddStretchSpacer();
    actions->Add(keep, 0);
    root->Add(actions, 0, wxEXPAND | wxALL, FromDIP(20));

    SetSizerAndFit(root);
    update_stage();
    CenterOnParent();
}

void StopPrintGateDialog::update_stage()
{
    const bool keys_ok  = m_keys[0] && m_keys[1];
    const bool armed_ok = m_presses[0] >= 2 && m_presses[1] >= 2 && m_presses[2] >= 2;
    if (m_slider)
        m_slider->Enable(keys_ok && armed_ok);
    wxString stage;
    if (!keys_ok)
        stage = _L("Step 1 of 4: turn both key switches.");
    else if (!armed_ok)
        stage = _L("Step 2 of 4: press each arming button twice.");
    else if (!m_slid)
        stage = _L("Step 3 of 4: slide to unlock the safety cover.");
    else if (!m_cover_open)
        stage = _L("Step 4 of 4: lift the cover and press STOP PRINT.");
    else
        stage = _L("Interlock complete. STOP PRINT is live.");
    m_stage_label->SetLabel(stage);
    Layout();
}

} // namespace Slic3r::GUI
