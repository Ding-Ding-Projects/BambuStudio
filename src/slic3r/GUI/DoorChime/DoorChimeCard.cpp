#include "DoorChimeCard.hpp"

#include "DoorChimeService.hpp"
#include "DoorMonitorSetup.hpp"

#include "../DeviceManager.hpp"
#include "../DeviceCore/DevManager.h"
#include "../GUI_App.hpp"
#include "libslic3r/AppConfig.hpp"

#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/SwitchButton.hpp"
#include "../Widgets/TextInput.hpp"

#include <wx/clipbrd.h>
#include <wx/sizer.h>
#include <wx/utils.h>
#include <wx/weakref.h>

#include <algorithm>
#include <thread>

#include <boost/log/trivial.hpp>

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

AppConfig *app_config() { return wxGetApp().app_config; }

std::string config_value(const char *key) { return app_config() != nullptr ? app_config()->get(key) : std::string(); }

void set_config_value(const char *key, const std::string &value)
{
    if (app_config() != nullptr) app_config()->set(key, value);
}

// Opens the monitor in the default browser. Automated checks that run Bambu
// Studio off screen set BAMBU_STUDIO_NO_BROWSER so the page is logged instead
// of opening on someone's desktop.
void open_page(const std::string &url)
{
    if (wxGetEnv("BAMBU_STUDIO_NO_BROWSER", nullptr)) {
        BOOST_LOG_TRIVIAL(info) << "Door monitor page (browser disabled): " << url;
        return;
    }
    wxLaunchDefaultBrowser(wxString::FromUTF8(url));
}

wxString phase_text(DoorMonitor::Phase phase)
{
    using DoorMonitor::Phase;
    switch (phase) {
    case Phase::Check: return _L("Checking Docker…");
    case Phase::Pull: return _L("Getting the door monitor image…");
    case Phase::Start: return _L("Starting the door monitor…");
    case Phase::Health: return _L("Waiting for the door monitor to be ready…");
    case Phase::Configure: return _L("Giving it the printer and the chimes…");
    case Phase::Reach: return _L("Checking its page from this computer…");
    }
    return {};
}

wxString error_text(DoorMonitor::Error error)
{
    using DoorMonitor::Error;
    switch (error) {
    case Error::None: return {};
    case Error::BadTarget: return _L("Write the SSH host as user@host, for example docker@my-docker-host.");
    case Error::DockerMissing: return _L("Docker is not installed on this computer. Install Docker Desktop, then try again. It is also needed to reach a host over SSH.");
    case Error::ComposeMissing: return _L("Docker Compose is not available. Update Docker Desktop, then try again.");
    case Error::EngineStopped: return _L("Docker is installed but not running. Start Docker Desktop, then try again.");
    case Error::SshEngine: return _L("Docker on that host cannot be reached over SSH. Check that SSH to it works without a password prompt.");
    case Error::Pull: return _L("The door monitor image could not be downloaded. Check the network, then try again.");
    case Error::Start: return _L("The door monitor did not start. Port 8790 may already be in use on that computer.");
    case Error::Health: return _L("The door monitor started but did not get ready within 90 seconds.");
    case Error::Configure: return _L("The door monitor did not accept the printer settings.");
    case Error::Reach: return _L("The door monitor is running, but its page does not open from this computer. Check the firewall on that computer.");
    }
    return {};
}

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
    m_switch->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &event) {
        DoorChime::set_enabled(m_switch->GetValue());
        refresh_state();
        // The switch redraws itself in its own handler for this event, which runs after this one.
        event.Skip();
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
    sizer->Add(build_monitor_section(), 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    SetSizer(sizer);

    // The printer can connect, start printing or finish while the card is on
    // screen, so the note and the Preview buttons follow it once a second.
    m_refresh.Bind(wxEVT_TIMER, [this](wxTimerEvent &) { if (IsShownOnScreen()) refresh_state(); });
    m_refresh.Start(1000);
    refresh_state();
    refresh_monitor_buttons();
}

DoorChimeCard::~DoorChimeCard() { *m_monitor_cancel = true; }

wxSizer *DoorChimeCard::build_monitor_section()
{
    auto *section = new wxBoxSizer(wxVERTICAL);

    auto *divider = new wxWindow(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(1)));
    divider->SetBackgroundColour(divider_color());
    section->Add(divider, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

    auto *heading = new Label(this, _L("Door monitor"));
    heading->SetFont(::Label::Head_14);
    heading->SetForegroundColour(text_color());
    section->Add(heading, 0, wxBOTTOM, FromDIP(4));

    auto *intro = new Label(this, _L("Keeps a log of the doors and rings these chimes from a computer with Docker, even while Bambu Studio is closed. One click sets it up for the selected printer."));
    intro->SetFont(::Label::Body_12);
    intro->SetForegroundColour(muted_color());
    intro->Wrap(FromDIP(420));
    section->Add(intro, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    auto *where = new wxFlexGridSizer(2, FromDIP(8), FromDIP(12));
    where->AddGrowableCol(1, 1);
    auto *where_label = new Label(this, _L("Run it on"));
    where_label->SetFont(::Label::Body_13);
    where_label->SetForegroundColour(text_color());
    where->Add(where_label, 0, wxALIGN_CENTER_VERTICAL);
    m_monitor_target = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(200), -1), 0, nullptr, wxCB_READONLY);
    m_monitor_target->Append(_L("This computer"));
    m_monitor_target->Append(_L("A Docker host over SSH"));
    m_monitor_target->SetSelection(config_value(DoorMonitor::TARGET_KEY) == "ssh" ? 1 : 0);
    m_monitor_target->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { on_monitor_target(); });
    where->Add(m_monitor_target, 1, wxEXPAND);
    auto *ssh_label = new Label(this, _L("SSH host"));
    ssh_label->SetFont(::Label::Body_13);
    ssh_label->SetForegroundColour(text_color());
    where->Add(ssh_label, 0, wxALIGN_CENTER_VERTICAL);
    m_monitor_ssh = new TextInput(this, wxString::FromUTF8(config_value(DoorMonitor::SSH_KEY)), "", "", wxDefaultPosition, wxSize(FromDIP(200), -1));
    m_monitor_ssh->GetTextCtrl()->SetHint("user@host");
    m_monitor_ssh->SetToolTip(_L("Uses your existing SSH key. Docker Desktop on this computer provides the docker command."));
    where->Add(m_monitor_ssh, 1, wxEXPAND);
    section->Add(where, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    auto *buttons = new wxBoxSizer(wxHORIZONTAL);
    m_monitor_setup = new Button(this, _L("Set up door monitor"));
    m_monitor_setup->SetVariant(Button::Variant::Filled);
    m_monitor_setup->SetButtonSize(Button::Size::Small);
    m_monitor_setup->SetToolTip(_L("Runs the door monitor with Docker, gives it this printer and these chimes, and opens it signed in"));
    m_monitor_setup->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_monitor_setup(); });
    buttons->Add(m_monitor_setup, 0, wxRIGHT, FromDIP(8));
    m_monitor_open = new Button(this, _L("Open"));
    m_monitor_open->SetVariant(Button::Variant::Tonal);
    m_monitor_open->SetButtonSize(Button::Size::Small);
    m_monitor_open->SetToolTip(_L("Open the door monitor in the browser, signed in"));
    m_monitor_open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_monitor_open(); });
    buttons->Add(m_monitor_open, 0, wxRIGHT, FromDIP(8));
    m_monitor_password = new Button(this, _L("Copy password"));
    m_monitor_password->SetVariant(Button::Variant::Text);
    m_monitor_password->SetButtonSize(Button::Size::Small);
    m_monitor_password->SetToolTip(_L("Copy the door monitor password Bambu Studio chose, for signing in on another device"));
    m_monitor_password->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_monitor_copy_password(); });
    buttons->Add(m_monitor_password, 0);
    section->Add(buttons, 0, wxBOTTOM, FromDIP(8));

    m_monitor_status = new Label(this, wxEmptyString);
    m_monitor_status->SetFont(::Label::Body_12);
    m_monitor_status->SetForegroundColour(muted_color());
    section->Add(m_monitor_status, 0, wxEXPAND);

    const std::string url = config_value(DoorMonitor::URL_KEY);
    if (!url.empty()) m_monitor_status->SetLabel(wxString::Format(_L("Set up at %s."), wxString::FromUTF8(url)));
    return section;
}

void DoorChimeCard::on_monitor_target()
{
    const bool ssh = m_monitor_target->GetSelection() == 1;
    m_monitor_ssh->Enable(ssh && !m_monitor_busy);
}

void DoorChimeCard::refresh_monitor_buttons()
{
    m_monitor_setup->Enable(!m_monitor_busy);
    m_monitor_target->Enable(!m_monitor_busy);
    m_monitor_open->Enable(!m_monitor_busy && !config_value(DoorMonitor::URL_KEY).empty());
    m_monitor_password->Enable(!config_value(DoorMonitor::PASSWORD_KEY).empty());
    on_monitor_target();
}

void DoorChimeCard::set_monitor_status(const wxString &status)
{
    if (m_monitor_status->GetLabel() == status) return;
    m_monitor_status->SetLabel(status);
    m_monitor_status->Wrap(std::max(FromDIP(200), GetClientSize().x - FromDIP(24)));
    Layout();
    if (GetParent() != nullptr) GetParent()->Layout();
}

void DoorChimeCard::on_monitor_setup()
{
    if (m_monitor_busy) return;
    DoorMonitor::Request request;
    if (m_monitor_target->GetSelection() == 1) {
        request.ssh = m_monitor_ssh->GetTextCtrl()->GetValue().Trim().Trim(false).ToStdString();
        if (!DoorMonitor::valid_ssh_target(request.ssh)) {
            set_monitor_status(error_text(DoorMonitor::Error::BadTarget));
            return;
        }
    }

    // The selected printer is filled in when Bambu Studio knows its LAN address and
    // access code; otherwise the monitor still starts and asks for it on its page.
    DeviceManager *manager = wxGetApp().getDeviceManager();
    MachineObject *printer = manager != nullptr ? manager->get_selected_machine() : nullptr;
    if (printer != nullptr && !printer->get_dev_ip().empty() && !printer->get_access_code().empty()) {
        request.printer_host = printer->get_dev_ip();
        request.serial       = printer->get_dev_id();
        request.access_code  = printer->get_access_code();
    }
    request.open_chime  = DoorChime::chosen_id(true);
    request.close_chime = DoorChime::chosen_id(false);
    // The same password is offered every time; the monitor takes it only when it has none.
    std::string password = config_value(DoorMonitor::PASSWORD_KEY);
    if (password.empty()) password = DoorMonitor::generate_password();
    request.password = password;

    set_config_value(DoorMonitor::TARGET_KEY, request.ssh.empty() ? "local" : "ssh");
    if (!request.ssh.empty()) set_config_value(DoorMonitor::SSH_KEY, request.ssh);

    m_monitor_busy = true;
    refresh_monitor_buttons();
    set_monitor_status(phase_text(DoorMonitor::Phase::Check));

    wxWeakRef<DoorChimeCard> self(this);
    auto cancel = m_monitor_cancel;
    std::thread([self, cancel, request, password]() {
        const auto on_phase = [self](DoorMonitor::Phase phase) {
            wxGetApp().CallAfter([self, phase]() { if (self) self->set_monitor_status(phase_text(phase)); });
        };
        const DoorMonitor::Result result = DoorMonitor::setup(request, on_phase, *cancel);
        const std::string ssh         = request.ssh;
        const bool        has_printer = !request.printer_host.empty();
        wxGetApp().CallAfter([self, result, password, ssh, has_printer]() {
            if (result.error == DoorMonitor::Error::None) {
                set_config_value(DoorMonitor::URL_KEY, result.url);
                if (result.password_set) set_config_value(DoorMonitor::PASSWORD_KEY, password);
                // The monitor rings the chimes now; Bambu Studio ringing them too would play each one twice.
                DoorChime::set_enabled(false);
                open_page(result.login_url.empty() ? result.url : result.login_url);
            }
            if (!self) return;
            self->m_monitor_busy = false;
            if (result.error == DoorMonitor::Error::None) {
                self->m_switch->SetValue(false);
                self->refresh_state();
                wxString status = wxString::Format(
                    _L("Ready at %s on %s. The door monitor rings the chimes now, so Bambu Studio stopped ringing them itself."),
                    wxString::FromUTF8(result.url), ssh.empty() ? _L("this computer") : wxString::FromUTF8(ssh));
                if (!has_printer)
                    status += "\n" + _L("No printer was filled in: connect to the printer over the network (LAN) and press Set up door monitor again, or enter it on the monitor's page.");
                self->set_monitor_status(status);
            } else {
                self->set_monitor_status(error_text(result.error));
            }
            self->refresh_monitor_buttons();
        });
    }).detach();
}

void DoorChimeCard::on_monitor_open()
{
    const std::string url = config_value(DoorMonitor::URL_KEY);
    if (url.empty() || m_monitor_busy) return;
    const std::string ssh = config_value(DoorMonitor::TARGET_KEY) == "ssh" ? config_value(DoorMonitor::SSH_KEY) : std::string();
    m_monitor_busy = true;
    refresh_monitor_buttons();
    wxWeakRef<DoorChimeCard> self(this);
    std::thread([self, ssh, url]() {
        const DoorMonitor::Result result = DoorMonitor::sign_in_link(ssh, url);
        wxGetApp().CallAfter([self, result, url]() {
            // Without a sign-in link (the monitor was moved or stopped) the plain page still opens.
            open_page(result.login_url.empty() ? url : result.login_url);
            if (!self) return;
            self->m_monitor_busy = false;
            self->refresh_monitor_buttons();
        });
    }).detach();
}

void DoorChimeCard::on_monitor_copy_password()
{
    const std::string password = config_value(DoorMonitor::PASSWORD_KEY);
    if (password.empty() || !wxTheClipboard->Open()) return;
    wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(password)));
    wxTheClipboard->Close();
    set_monitor_status(_L("Password copied."));
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
