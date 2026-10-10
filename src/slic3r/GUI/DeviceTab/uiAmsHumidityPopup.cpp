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
#include "slic3r/GUI/Widgets/MD3ScrolledWindow.hpp"


#include <wx/dcgraph.h>
#include <wx/grid.h>
#include <wx/display.h>
#include <wx/settings.h>
#include <algorithm>

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
    m_body = new MD3ScrolledWindow(this, wxID_ANY);
    m_body->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    m_body->SetScrollRate(0, FromDIP(10));

    // create humidity image
    m_humidity_img = new wxStaticBitmap(m_body, wxID_ANY, wxNullBitmap);

    // create dry state sizer
    auto *dry_state_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_dry_state_img = new wxStaticBitmap(m_body, wxID_ANY, wxNullBitmap);
    m_dry_state_img->SetMinSize(wxSize(FromDIP(16), FromDIP(16)));
    m_dry_state_img->SetMaxSize(wxSize(FromDIP(16), FromDIP(16)));
    m_dry_state = new Label(m_body);
    m_dry_state->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_dry_state->SetBackgroundColour(m_body->GetBackgroundColour());
    m_dry_state->SetFont(Label::Body_14);
    dry_state_sizer->Add(m_dry_state_img, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    dry_state_sizer->Add(m_dry_state, 0, wxALIGN_CENTER_VERTICAL);

    // create table grid sizer
    auto *grid_sizer = new wxFlexGridSizer(0, 2, FromDIP(12), FromDIP(16));
    m_readouts = grid_sizer;
    grid_sizer->AddGrowableCol(1);
    m_humidity_header = new Label(m_body, _L("Humidity"));
    m_temperature_header = new Label(m_body, _L("Temperature"));
    left_dry_time_header = new Label(m_body, _L("Left Time"));
    m_humidity_label = new Label(m_body);
    m_temperature_label = new Label(m_body);
    left_dry_time_label = new Label(m_body);

    grid_sizer->Add(m_humidity_header, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    grid_sizer->Add(m_humidity_label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    grid_sizer->Add(m_temperature_header, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    grid_sizer->Add(m_temperature_label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);
    grid_sizer->Add(left_dry_time_header, 0, wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);
    grid_sizer->Add(left_dry_time_label, 0, wxALIGN_RIGHT | wxALIGN_CENTER_VERTICAL);

    // complete main sizer
    m_sizer = new wxBoxSizer(wxVERTICAL);
    m_sizer->AddSpacer(FromDIP(10));
    m_sizer->Add(m_humidity_img, 1, wxALIGN_CENTER_HORIZONTAL, 0);
    m_sizer->AddSpacer(FromDIP(10));
    m_sizer->Add(dry_state_sizer, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, FromDIP(16));
    m_sizer->Add(grid_sizer, 0, wxEXPAND | wxALL, FromDIP(16));
    m_sizer->AddSpacer(FromDIP(10));
    m_body->SetSizer(m_sizer);
    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(m_body, 1, wxEXPAND);
    SetSizer(root);

    // Seed the existing nominal body before caption adoption: the 400 x 270 footprint the
    // popup had with the native frame, less the kit caption strip that Adopt() adds, taken
    // from the strip itself rather than a number that assumes its height. LayoutReadouts then
    // measures actual content, adds the strip, and bounds the scroll viewport to the display.
    m_body->SetMinSize(wxSize(FromDIP(400), FromDIP(270) - MD3DialogCaption::Height(this)));

    Fit();
    Layout();
    // Last layout act: swap the native title bar for the kit caption strip. The window text is
    // set to the same title first, because Alt-Tab and assistive technology read it, not the strip.
    SetTitle(_L("Current AMS humidity"));
    MD3DialogCaption::Adopt(this, _L("Current AMS humidity"));
    LayoutReadouts();
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

    LayoutReadouts();
    Refresh();
}

void uiAmsPercentHumidityDryPopup::LayoutReadouts()
{
    const int display = wxDisplay::GetFromWindow(GetParent() ? GetParent() : this);
    const wxSize work = display == wxNOT_FOUND ? wxSize(FromDIP(800), FromDIP(600)) : wxDisplay(display).GetClientArea().GetSize();
    const int padding = FromDIP(MD3::Metrics::active().padding);
    const int width = std::max(1, std::min(FromDIP(440), work.x - FromDIP(32)));
    const int label_width = std::max(1, (width - 4 * padding - wxSystemSettings::GetMetric(wxSYS_VSCROLL_X, this)) / 2);
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_body->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    m_dry_state->SetFont(::Label::Body_14);
    m_dry_state->SetBackgroundColour(m_body->GetBackgroundColour());
    m_dry_state_img->SetMinSize(wxSize(FromDIP(16), FromDIP(16)));
    m_dry_state_img->SetMaxSize(wxSize(FromDIP(16), FromDIP(16)));
    for (Label *label : {m_humidity_header, m_temperature_header, left_dry_time_header}) {
        label->SetFont(::Label::Body_14);
        label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        label->SetBackgroundColour(m_body->GetBackgroundColour());
        label->SetMinSize(wxDefaultSize);
        label->Wrap(label_width);
        label->InvalidateBestSize();
    }
    for (Label *value : {m_humidity_label, m_temperature_label, left_dry_time_label}) {
        value->SetFont(::Label::Head_16);
        value->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
        value->SetBackgroundColour(m_body->GetBackgroundColour());
        value->InvalidateBestSize();
    }
    m_readouts->SetVGap(FromDIP(MD3::Metrics::active().gap));
    m_readouts->SetHGap(padding);
    m_sizer->GetItem(m_readouts)->SetBorder(padding);
    m_body->SetMinSize(wxSize(-1, 1));
    SetMinSize(wxDefaultSize);
    const int height = std::max(1, std::min(m_sizer->CalcMin().y + MD3DialogCaption::Height(this), work.y - FromDIP(32)));
    SetClientSize(width, height);
    Layout();
    m_body->FitInside();
}

void uiAmsPercentHumidityDryPopup::msw_rescale()
{
    idle_img.msw_rescale();
    drying_img.msw_rescale();
    UpdateContents();
}

} // namespace GUI

} // namespace Slic3r
