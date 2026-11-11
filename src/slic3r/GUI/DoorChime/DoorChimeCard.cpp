#include "DoorChimeCard.hpp"

#include "DoorChimeService.hpp"

#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/SwitchButton.hpp"

#include <wx/sizer.h>

#include <algorithm>

namespace Slic3r { namespace GUI {

namespace {

// The same surface, outline and text roles as the other Device page cards.
wxColour card_color() { return StateColor::semantic(MD3::Role::SurfaceContainerLowest); }
wxColour divider_color() { return StateColor::semantic(MD3::Role::OutlineVariant); }
wxColour text_color() { return StateColor::semantic(MD3::Role::OnSurface); }
wxColour muted_color() { return StateColor::semantic(MD3::Role::OnSurfaceVariant); }

const std::vector<DoorChime::Chime> &chimes_for(bool open)
{
    return open ? DoorChime::open_chimes() : DoorChime::close_chimes();
}

wxString chime_name(const DoorChime::Chime &chime) { return _(wxString::FromUTF8(chime.name)); }

} // namespace

DoorChimeCard::DoorChimeCard(wxWindow *parent) : StaticBox(parent)
{
    SetBackgroundColor(StateColor(card_color()));
    SetBorderColor(StateColor(divider_color()));
    SetBackgroundColour(card_color());
    SetCornerRadius(FromDIP(MD3::Metrics::active().radius));

    auto *title = new SectionHeader(this, _L("Door chime"), MaterialIcon::available() ? MaterialIcon::NotificationsActive : 0);

    auto *switch_row = new wxBoxSizer(wxHORIZONTAL);
    auto *switch_label = new Label(this, _L("Play a chime when the door opens or closes"));
    switch_label->SetFont(::Label::Body_13);
    switch_label->SetForegroundColour(text_color());
    switch_row->Add(switch_label, 1, wxALIGN_CENTER_VERTICAL);
    m_switch = new SwitchButton(this);
    m_switch->SetColorScheme(MD3::ColorScheme::Device);
    m_switch->SetMinSize(wxSize(FromDIP(44), FromDIP(24)));
    m_switch->SetValue(DoorChime::enabled());
    m_switch->SetToolTip(_L("Beta: the door reading comes from a status bit that the printer maker does not document."));
    m_switch->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &) {
        DoorChime::set_enabled(m_switch->GetValue());
        refresh_state();
    });
    switch_row->Add(m_switch, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));

    auto *rows = new wxFlexGridSizer(3, FromDIP(8), FromDIP(12));
    rows->AddGrowableCol(1, 1);
    m_open  = add_row(rows, true, _L("When the door opens"));
    m_close = add_row(rows, false, _L("When the door closes"));

    m_note = new Label(this, wxEmptyString);
    m_note->SetFont(::Label::Body_12);
    m_note->SetForegroundColour(muted_color());

    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    sizer->Add(switch_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    sizer->Add(rows, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));
    sizer->Add(m_note, 0, wxEXPAND | wxALL, FromDIP(12));
    SetSizer(sizer);

    // The printer can connect, start printing or finish while the card is on
    // screen, so the note and the Preview buttons follow it once a second.
    m_refresh.Bind(wxEVT_TIMER, [this](wxTimerEvent &) { if (IsShownOnScreen()) refresh_state(); });
    m_refresh.Start(1000);
    refresh_state();
}

DoorChimeCard::Row DoorChimeCard::add_row(wxSizer *sizer, bool open, const wxString &label)
{
    auto *text = new Label(this, label);
    text->SetFont(::Label::Body_13);
    text->SetForegroundColour(text_color());
    sizer->Add(text, 0, wxALIGN_CENTER_VERTICAL);

    Row row;
    row.choice = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(200), -1), 0, nullptr, wxCB_READONLY);
    const std::string chosen = DoorChime::chosen_id(open);
    const auto       &chimes = chimes_for(open);
    for (size_t i = 0; i < chimes.size(); ++i) {
        row.choice->Append(chime_name(chimes[i]));
        if (chosen == chimes[i].id) row.choice->SetSelection(int(i));
    }
    row.choice->Bind(wxEVT_COMBOBOX, [this, open](wxCommandEvent &) { on_choice(open); });
    sizer->Add(row.choice, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

    row.preview = new Button(this, _L("Preview"));
    row.preview->SetVariant(Button::Variant::Tonal);
    row.preview->SetButtonSize(Button::Size::Small);
    row.preview->SetGlyph(MaterialIcon::PlayArrow);
    row.preview->SetToolTip(open ? _L("Play the door-open chime on the printer now")
                                 : _L("Play the door-close chime on the printer now"));
    row.preview->Bind(wxEVT_BUTTON, [this, open](wxCommandEvent &) { on_preview(open); });
    sizer->Add(row.preview, 0, wxALIGN_CENTER_VERTICAL);
    return row;
}

void DoorChimeCard::on_choice(bool open)
{
    const Row &row = open ? m_open : m_close;
    const int  pick = row.choice->GetSelection();
    const auto &chimes = chimes_for(open);
    if (pick < 0 || pick >= int(chimes.size())) return;
    DoorChime::set_chosen_id(open, chimes[pick].id);
    refresh_state();
}

void DoorChimeCard::on_preview(bool open)
{
    const Row &row = open ? m_open : m_close;
    const int  pick = row.choice->GetSelection();
    const auto &chimes = chimes_for(open);
    const std::string id = pick >= 0 && pick < int(chimes.size()) ? chimes[pick].id : DoorChime::chosen_id(open);
    DoorChime::preview(open, id);
    refresh_state();
}

void DoorChimeCard::refresh_state()
{
    using DoorChime::Readiness;
    const Readiness ready = DoorChime::readiness();
    // A preview only needs the buzzer, so it works on a printer without a door sensor.
    const bool can_preview = ready == Readiness::Ready || ready == Readiness::NoDoorSensor;
    for (const Row *row : {&m_open, &m_close}) {
        const auto &chimes = chimes_for(row == &m_open);
        const int   pick   = row->choice->GetSelection();
        const bool  silent = pick >= 0 && pick < int(chimes.size()) && DoorChime::is_silent(chimes[pick]);
        row->preview->Enable(can_preview && !silent);
    }

    wxString note;
    switch (ready) {
    case Readiness::NoPrinter: note = _L("Connect a printer to hear the chimes."); break;
    case Readiness::NoDoorSensor:
        note = _L("This printer does not report its door, so only Preview plays. The chimes need a printer with a door sensor.");
        break;
    case Readiness::Printing: note = _L("Chimes wait while the printer is printing, so the print is never disturbed."); break;
    case Readiness::Ready:
        note = DoorChime::enabled() ? _L("The printer plays the chime when its door opens or closes. Each chime releases the motors when it ends.")
                                    : _L("Turn the switch on to hear the chimes when the door moves.");
        break;
    }
    if (m_note->GetLabel() != note) {
        m_note->SetLabel(note);
        m_note->Wrap(std::max(FromDIP(200), GetClientSize().x - FromDIP(24)));
        Layout();
    }
}

}} // namespace Slic3r::GUI
