//**********************************************************/
/* File: uiAmsHumidityPopup.cpp
*  Description: The popup with DevAms Humidity
*
* \n class uiAmsHumidityPopup
//**********************************************************/

#include "uiAmsHumidityPopup.h"

#include "slic3r/Utils/WxFontUtils.hpp"

#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include "slic3r/GUI/Widgets/MD3DialogChrome.hpp"


#include <wx/dcgraph.h>
#include <wx/grid.h>

namespace Slic3r { namespace GUI {

uiAmsPercentHumidityDryPopup::uiAmsPercentHumidityDryPopup(wxWindow *parent)
    : wxDialog(parent, wxID_ANY, "")
{
    Create();
}

void uiAmsPercentHumidityDryPopup::Create()
{
    // create images
    idle_img = ScalableBitmap(this, "ams_drying", 16);
    drying_img = ScalableBitmap(this, "dev_ams_dry_ctr_heating_icon", 16);

    // background: the kit surface and text roles follow the light and dark themes. The title
    // is drawn by the kit caption strip that Adopt() adds at the end of this function.
    const wxColour surface = StateColor::semantic(MD3::Role::SurfaceContainerLowest);
    SetBackgroundColour(surface);

    // create humidity image
    m_humidity_img = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap);

    // create dry state sizer
    wxGridSizer* dry_state_sizer = new wxGridSizer(2, FromDIP(5), FromDIP(5));
    m_dry_state_img = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap);
    m_dry_state_img->SetMinSize(wxSize(FromDIP(16), FromDIP(16)));
    m_dry_state_img->SetMaxSize(wxSize(FromDIP(16), FromDIP(16)));
    m_dry_state = new Label(this);
    m_dry_state->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_dry_state->SetBackgroundColour(surface);
    m_dry_state->SetFont(Label::Body_14);
    dry_state_sizer->Add(m_dry_state_img, 1, wxALIGN_RIGHT);
    dry_state_sizer->Add(m_dry_state, 1, wxALIGN_LEFT);

    // create table grid sizer
    wxGridSizer* grid_sizer = new wxGridSizer(2, 3, FromDIP(10), FromDIP(10));
    m_humidity_header = new Label(this, _L("Humidity"));
    m_temperature_header = new Label(this, _L("Temperature"));
    left_dry_time_header = new Label(this, _L("Left Time"));
    m_humidity_label = new Label(this);
    m_temperature_label = new Label(this);
    left_dry_time_label = new Label(this);

    grid_sizer->Add(m_humidity_header, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    grid_sizer->Add(m_temperature_header, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    grid_sizer->Add(left_dry_time_header, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    grid_sizer->Add(m_humidity_label, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    grid_sizer->Add(m_temperature_label, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    grid_sizer->Add(left_dry_time_label, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);

    // complete main sizer
    m_sizer = new wxBoxSizer(wxVERTICAL);
    m_sizer->AddSpacer(FromDIP(10));
    m_sizer->Add(m_humidity_img, 1, wxALIGN_CENTER_HORIZONTAL, 0);
    m_sizer->AddSpacer(FromDIP(10));
    m_sizer->Add(dry_state_sizer, 1 ,wxEXPAND | wxHORIZONTAL);
    m_sizer->Add(grid_sizer, 1, wxEXPAND | wxHORIZONTAL, FromDIP(15));
    m_sizer->AddSpacer(FromDIP(10));
    SetSizer(m_sizer);

    // The popup keeps the 400 x 270 footprint it had with the native frame: the kit caption
    // strip takes its height out of that and the body gets the rest. The floor sits on the body
    // sizer rather than on the frame, so the strip that Adopt() adds cannot push the last row
    // out of a fixed-height window, and Fit() in UpdateContents() keeps the same size.
    m_sizer->SetMinSize(FromDIP(400), FromDIP(270) - MD3DialogCaption::Height(this));

    Fit();
    Layout();
    // Last layout act: swap the native title bar for the kit caption strip. The window text is
    // set to the same title first, because Alt-Tab and assistive technology read it, not the strip.
    SetTitle(_L("Current AMS humidity"));
    MD3DialogCaption::Adopt(this, _L("Current AMS humidity"));
    Refresh();
}

void uiAmsPercentHumidityDryPopup::Update(int humidiy_level, int humidity_percent, int left_dry_time, float current_temperature)
{
    if (m_humidity_level != humidiy_level || m_humidity_percent != humidity_percent ||
        m_left_dry_time != left_dry_time || m_current_temperature != current_temperature)
    {
        m_humidity_level   = humidiy_level;
        m_humidity_percent = humidity_percent;
        m_left_dry_time    = left_dry_time;
        m_current_temperature = current_temperature;

        UpdateContents();
    }
}

void uiAmsPercentHumidityDryPopup::UpdateContents()
{
    // humitidy image
    if (0 < m_humidity_level && m_humidity_level < 6)
    {
        ScalableBitmap humitidy_image;
        if (wxGetApp().dark_mode())
        {
            humitidy_image = ScalableBitmap(this, "hum_level" + std::to_string(m_humidity_level) + "_no_num_light", 64);
        }
        else
        {
            humitidy_image = ScalableBitmap(this, "hum_level" + std::to_string(m_humidity_level) + "_no_num_light", 64);
        }

        m_humidity_img->SetBitmap(humitidy_image.bmp());
    }

    // dry state
    if (m_left_dry_time > 0)
    {
        m_dry_state_img->SetBitmap(drying_img.bmp());
        m_dry_state->SetLabel(_L("Drying"));
        m_dry_state->Fit();
    }
    else
    {
        m_dry_state_img->SetBitmap(idle_img.bmp());
        m_dry_state->SetLabel(_L("Idle"));
        m_dry_state->Fit();
    }

    // table grid
    const wxString& humidity_str = wxString::Format("%d%%", m_humidity_percent);
    m_humidity_label->SetLabel(humidity_str);
    const wxString& temp_str = wxString::Format(_L("%d \u2103"), (int)std::round(m_current_temperature));
    m_temperature_label->SetLabel(temp_str);

    if (m_left_dry_time > 0)
    {
        wxString display_hour_str;
        int left_hours = m_left_dry_time / 60;
        if (left_hours < 10) {
            display_hour_str = wxString::Format("0%d", left_hours);
        } else {
            display_hour_str = wxString::Format("%d", left_hours);
        }

        wxString display_min_str;
        int left_minutes = m_left_dry_time % 60;
        if (left_minutes < 10) {
            display_min_str = wxString::Format("0%d", left_minutes);
        } else {
            display_min_str = wxString::Format("%d", left_minutes);
        }

        const wxString& time_str = wxString::Format("%s : %s", display_hour_str, display_min_str);
        left_dry_time_label->SetLabel(time_str);
    }
    else
    {
        left_dry_time_label->SetLabel(_L("Idle"));
    }

    Fit();
    Layout();
    Refresh();
}

void uiAmsPercentHumidityDryPopup::msw_rescale()
{
    idle_img.msw_rescale();
    drying_img.msw_rescale();
    UpdateContents();
}

} // namespace GUI

} // namespace Slic3r