#include "PrintCooldownCard.hpp"

#include "PrintCooldownService.hpp"

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/SwitchButton.hpp"

#include <wx/sizer.h>

namespace Slic3r { namespace GUI {

namespace {

// The same surface, outline and text roles as the other Device page cards.
wxColour card_color() { return StateColor::semantic(MD3::Role::SurfaceContainerLowest); }
wxColour divider_color() { return StateColor::semantic(MD3::Role::OutlineVariant); }
wxColour text_color() { return StateColor::semantic(MD3::Role::OnSurface); }
wxColour muted_color() { return StateColor::semantic(MD3::Role::OnSurfaceVariant); }

wxString degrees(float celsius) { return wxString::Format("%d", int(celsius + 0.5f)); }

} // namespace

PrintCooldownCard::PrintCooldownCard(wxWindow *parent) : StaticBox(parent)
{
    SetBackgroundColor(StateColor(card_color()));
    SetBorderColor(StateColor(divider_color()));
    SetBackgroundColour(card_color());
    SetCornerRadius(FromDIP(MD3::Metrics::active().radius));

    auto *title = new SectionHeader(this, _L("Cool down"), MaterialIcon::available() ? MaterialIcon::ModeFan : 0);

    auto *switch_row   = new wxBoxSizer(wxHORIZONTAL);
    auto *switch_label = new Label(this, _L("Cool down the printer after a print"));
    switch_label->SetFont(::Label::Body_13);
    switch_label->SetForegroundColour(text_color());
    switch_row->Add(switch_label, 1, wxALIGN_CENTER_VERTICAL);
    m_switch = new SwitchButton(this);
    m_switch->SetColorScheme(MD3::ColorScheme::Device);
    m_switch->SetMinSize(wxSize(FromDIP(44), FromDIP(24)));
    m_switch->SetValue(PrintCooldown::enabled());
    m_switch->SetToolTip(_L("When a print ends, all fans run at full speed until the bed and chamber are at or under 40 °C, for five minutes at most. Bambu Studio must stay open with this printer selected."));
    m_switch->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &event) {
        PrintCooldown::set_enabled(m_switch->GetValue());
        refresh_state();
        // The switch redraws itself in its own handler for this event, which runs after this one.
        event.Skip();
    });
    switch_row->Add(m_switch, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));

    m_note = new Label(this, wxEmptyString);
    m_note->SetFont(::Label::Body_12);
    m_note->SetForegroundColour(muted_color());

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    sizer->Add(switch_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    sizer->Add(m_note, 0, wxEXPAND | wxALL, FromDIP(12));
    SetSizer(sizer);

    // The print can end and the printer can cool while the card is on screen,
    // so the note follows it once a second.
    m_refresh.Bind(wxEVT_TIMER, [this](wxTimerEvent &) { if (IsShownOnScreen()) refresh_state(); });
    m_refresh.Start(1000);
    refresh_state();
}

void PrintCooldownCard::refresh_state()
{
    using PrintCooldown::Phase;
    const PrintCooldown::State state = PrintCooldown::state();

    wxString note;
    if (!PrintCooldown::enabled())
        note = _L("Off. Turn it on to run all fans after each print until the printer is cool.");
    else if (state.blocked)
        note = _L("Fan control is locked for this printer, so the fans are left alone.");
    else if (!state.connected)
        note = _L("Connect a printer. The cool down runs while Bambu Studio is open with the printer selected.");
    else {
        switch (state.phase) {
        case Phase::Idle:
            note = _L("Ready. When a print ends, all fans run until the bed and chamber are at or under 40 °C, for five minutes at most.");
            break;
        case Phase::Cooling: {
            const wxString left = wxString::Format("%d:%02d", state.remaining_s / 60, state.remaining_s % 60);
            if (state.chamber_c > 0.0f)
                note = wxString::Format(_L("Cooling: all fans on. Bed %s °C, chamber %s °C. Stops in %s at the latest."), degrees(state.bed_c),
                                        degrees(state.chamber_c), left);
            else
                note = wxString::Format(_L("Cooling: all fans on. Bed %s °C. Stops in %s at the latest."), degrees(state.bed_c), left);
            break;
        }
        case Phase::Cooled: note = _L("Cooled down. The fans are off and the printer is cool enough to open."); break;
        case Phase::TimedOut:
            note = wxString::Format(_L("Stopped after five minutes with the bed at %s °C. The fans are off; the printer may still be warm."),
                                    degrees(state.bed_c));
            break;
        }
    }
    if (note != m_note->GetLabel()) {
        m_note->SetLabel(note);
        m_note->Wrap(GetClientSize().GetWidth() - FromDIP(24));
        Layout();
    }
}

}} // namespace Slic3r::GUI
