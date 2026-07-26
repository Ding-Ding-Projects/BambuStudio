#include "ThermalPreconditioningDialog.hpp"
#include "I18N.hpp"
#include "GUI.hpp"
#include "GUI_App.hpp"
#include "wxExtensions.hpp"
#include "DeviceManager.hpp"
#include "DeviceCore/DevManager.h"
#include "Widgets/Button.hpp"

namespace Slic3r { namespace GUI {

BEGIN_EVENT_TABLE(ThermalPreconditioningDialog, MD3Dialog)
EVT_BUTTON(wxID_OK, ThermalPreconditioningDialog::on_ok_clicked)
END_EVENT_TABLE()

ThermalPreconditioningDialog::ThermalPreconditioningDialog(wxWindow *parent, std::string dev_id, bool is_show_remain_time)
    : MD3Dialog(parent, _L("Thermal Preconditioning for first layer optimization"), wxEmptyString, MaterialIcon::ModeHeat)
    , m_dev_id(dev_id)
{
    // Device (teal) contextual scheme — this is a live device-monitoring flow.
    SetColorScheme(MD3::ColorScheme::Device);
    create_ui();
    m_refresh_timer = new wxTimer(this);
    this->Bind(wxEVT_TIMER, &ThermalPreconditioningDialog::on_timer, this);
    m_refresh_timer->Start(1000);

    if (is_show_remain_time)
    {
        m_remaining_time_label->SetLabelText(_L("Remaining time: Calculating..."));
    }
    else
    {
        m_remaining_time_label->Hide();
    }

    Layout();
    GetSizer()->SetSizeHints(this);
    SetMinSize(wxSize(FromDIP(500), -1));
    SetSize(wxSize(FromDIP(500), -1));
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
    CentreOnScreen();
}

ThermalPreconditioningDialog::~ThermalPreconditioningDialog()
{
    if (m_refresh_timer && m_refresh_timer->IsRunning()) {
        m_refresh_timer->Stop();
        delete m_refresh_timer;
        m_refresh_timer = nullptr;
    }
}

void ThermalPreconditioningDialog::create_ui()
{
    wxBoxSizer *content = GetContentSizer();

    // Remaining time label
    m_remaining_time_label = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    wxFont time_font       = m_remaining_time_label->GetFont();
    time_font.SetPointSize(14);
    time_font.SetWeight(wxFONTWEIGHT_BOLD);
    m_remaining_time_label->SetFont(time_font);
    m_remaining_time_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));

    // Explanation text
    m_explanation_label =
        new wxStaticText(this, wxID_ANY,
                         _L("The heated bed's thermal preconditioning helps optimize the first layer print quality. Printing will start once preconditioning is complete."),
                         wxDefaultPosition, wxDefaultSize, wxALIGN_LEFT);
    m_explanation_label->Wrap(FromDIP(450));
    m_explanation_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));

    content->Add(m_remaining_time_label, 0, wxEXPAND);
    content->AddSpacer(FromDIP(10));
    content->Add(m_explanation_label, 0, wxEXPAND);

    // Footer: kit tonal pill OK button (Device scheme), preserving wxID_OK so the
    // EVT_BUTTON(wxID_OK, ...) binding continues to route to on_ok_clicked.
    m_ok_button = new Button(this, _L("OK"), "", 0, 0, wxID_OK);
    m_ok_button->SetVariant(Button::Variant::Tonal);
    m_ok_button->SetButtonSize(Button::Size::Medium);
    m_ok_button->SetColorScheme(MD3::ColorScheme::Device);
    AddFooterButton(m_ok_button);
}

void ThermalPreconditioningDialog::on_ok_clicked(wxCommandEvent &event) { EndModal(wxID_OK); }

void ThermalPreconditioningDialog::update_thermal_remaining_time()
{
    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;
    MachineObject *m_obj = dev->get_my_machine(m_dev_id);

    int      remaining_seconds = m_obj->get_stage_remaining_seconds();
    wxString remaining_time;
    if (remaining_seconds >= 0) {
        int minutes    = remaining_seconds / 60;
        int seconds    = remaining_seconds % 60;
        remaining_time = wxString::Format(_L("Remaining time: %dmin%ds"), minutes, seconds);
    }

    if (m_remaining_time_label) m_remaining_time_label->SetLabelText(remaining_time);

     Layout();
}

void ThermalPreconditioningDialog::on_timer(wxTimerEvent &event) {
    DeviceManager *dev = Slic3r::GUI::wxGetApp().getDeviceManager();
    if (!dev) return;
    MachineObject *m_obj = dev->get_my_machine(m_dev_id);

    if (IsShown() && m_obj && m_obj->stage_curr == 58) {
        update_thermal_remaining_time();
    } else {
        EndModal(wxID_OK);
        m_refresh_timer->Stop();
    }
}

}} // namespace Slic3r