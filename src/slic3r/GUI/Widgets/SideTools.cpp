#include "SideTools.hpp"
#include "bambu_networking.hpp"
#include <wx/dcmemory.h>
#include <wx/dcgraph.h>
#include "Label.hpp"
#include "StateColor.hpp"
#include "MaterialIcon.hpp"
#include "../GUI_App.hpp"
#include "../wxExtensions.hpp"
#include "../I18N.hpp"
#include "../GUI.hpp"

namespace Slic3r { namespace GUI {

// Build a monochrome icon as a Material Symbols glyph rendered at a logical px
// in colour, degrading to the legacy raster (fallback_name) when the icon face
// is unavailable. Mirrors the same capability-gated pattern already applied to
// this file's wifi/wired signal icons (see init_signal_bitmaps()).
static ScalableBitmap side_tools_glyph_bitmap(wxWindow *ref, uint32_t glyph, int px, const wxColour &colour, const std::string &fallback_name)
{
    if (MaterialIcon::available()) {
        ScalableBitmap sb;
        sb.bmp() = MaterialIcon::bitmap(ref, glyph, px, colour);
        return sb;
    }
    return ScalableBitmap(ref, fallback_name, px);
}

	SideToolsPanel::SideToolsPanel(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
{
    wxPanel::Create(parent, id, pos, size);

    SetMinSize(wxSize(-1, FromDIP(50)));
    SetMaxSize(wxSize(-1, FromDIP(50)));

    Bind(wxEVT_PAINT, &SideToolsPanel::OnPaint, this);
    // Device-page surface (Device.jsx SurfaceDim), replacing the legacy
    // ThemeColor::White literal.
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceDim));

    m_printing_img = side_tools_glyph_bitmap(this, MaterialIcon::Print, 16, StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Device), "printer");
    m_arrow_img    = side_tools_glyph_bitmap(this, MaterialIcon::ChevronRight, 14, StateColor::semantic(MD3::Role::OnSurfaceVariant), "monitor_arrow");

    // Drawn on the SIDE_TOOLS_BRAND (Device Primary) fill in doRender(), so the
    // glyph colour is OnPrimary/Device -- correct in both light AND dark theme,
    // unlike a hardcoded white that would lose contrast once the fill itself
    // flips to a light teal in dark mode.
    const wxColour none_state_fg = StateColor::semantic(MD3::Role::OnPrimary, MD3::ColorScheme::Device);
    m_none_printing_img = side_tools_glyph_bitmap(this, MaterialIcon::Print, 24, none_state_fg, "tab_monitor_active");
    m_none_arrow_img    = side_tools_glyph_bitmap(this, MaterialIcon::ChevronRight, 14, none_state_fg, "monitor_none_arrow");
    m_none_add_img      = side_tools_glyph_bitmap(this, MaterialIcon::Add, 14, none_state_fg, "monitor_none_add");

    init_signal_bitmaps();

    m_intetval_timer = new wxTimer();
    m_intetval_timer->SetOwner(this);

    this->Bind(wxEVT_TIMER, &SideToolsPanel::stop_interval, this);
    this->Bind(wxEVT_ENTER_WINDOW, &SideToolsPanel::on_mouse_enter, this);
    this->Bind(wxEVT_LEAVE_WINDOW, &SideToolsPanel::on_mouse_leave, this);
    this->Bind(wxEVT_LEFT_DOWN, &SideToolsPanel::on_mouse_left_down, this);
    this->Bind(wxEVT_LEFT_UP, &SideToolsPanel::on_mouse_left_up, this);
    this->Bind(wxEVT_SET_FOCUS, &SideToolsPanel::on_set_focus, this);
    this->Bind(wxEVT_KILL_FOCUS, &SideToolsPanel::on_kill_focus, this);
    this->Bind(wxEVT_KEY_DOWN, &SideToolsPanel::on_key_down, this);

    // a11y-label: give the icon-only switcher strip an accessible name/tooltip so
    // assistive tech announces its purpose (set_current_printer_name() keeps the
    // name in sync with the live device).
    SetName(_L("Switch printer"));
    SetToolTip(_L("Switch printer"));
}

SideToolsPanel::~SideToolsPanel() { delete m_intetval_timer; }

void SideToolsPanel::set_none_printer_mode()
{
    m_none_printer = true;
    Refresh();
}

void SideToolsPanel::on_timer(wxTimerEvent &event)
{
}

void SideToolsPanel::set_current_printer_name(std::string dev_name)
{
     if (m_dev_name == from_u8(dev_name) && !m_none_printer) return;

     m_none_printer = false;
     m_dev_name     = from_u8(dev_name);
     // Keep the accessible name in step with the visible device label.
     if (!m_dev_name.IsEmpty())
         SetName(m_dev_name);
     Refresh();
}

void SideToolsPanel::set_current_printer_signal(WifiSignal sign)
{
     if (last_printer_signal == sign && !m_none_printer) return;

     last_printer_signal = sign;
     m_none_printer = false;
     m_wifi_type    = sign;
     Refresh();
}

void SideToolsPanel::start_interval()
{
    m_intetval_timer->Start(SIDE_TOOL_CLICK_INTERVAL);
    m_is_in_interval = true;
}

void SideToolsPanel::stop_interval(wxTimerEvent& event)
{
    m_is_in_interval = false;
    m_intetval_timer->Stop();
}


bool SideToolsPanel::is_in_interval()
{
    return m_is_in_interval;
}

void SideToolsPanel::init_signal_bitmaps()
{
    // MD3: render the Wi-Fi / wired connectivity indicators from the Material
    // Symbols icon font instead of the legacy monitor_signal_*/monitor_network_wired
    // rasters. The Wi-Fi signal level is carried by the glyph shape (4-bar /
    // 3-bar / 2-bar / null); state is expressed through colour, never the FILL
    // axis. Fall back to the bundled bitmaps when the icon face is unavailable so
    // a missing TTF degrades to the legacy look instead of tofu.
    if (MaterialIcon::available()) {
        const wxColour signal_on  = StateColor::semantic(MD3::Role::Primary);
        const wxColour signal_off = StateColor::semantic(MD3::Role::OnSurfaceVariant);
        m_wifi_strong_img   = MaterialIcon::bitmap(this, MaterialIcon::SignalWifi4Bar,          18, signal_on);
        m_wifi_middle_img   = MaterialIcon::bitmap(this, MaterialIcon::NetworkWifi3Bar,         18, signal_on);
        m_wifi_weak_img     = MaterialIcon::bitmap(this, MaterialIcon::NetworkWifi2Bar,         18, signal_off);
        m_wifi_none_img     = MaterialIcon::bitmap(this, MaterialIcon::SignalWifiStatusbarNull, 18, signal_off);
        m_network_wired_img = MaterialIcon::bitmap(this, MaterialIcon::Lan,                     18, signal_on);
    } else {
        m_wifi_strong_img   = create_scaled_bitmap("monitor_signal_strong", this, 18);
        m_wifi_middle_img   = create_scaled_bitmap("monitor_signal_middle", this, 18);
        m_wifi_weak_img     = create_scaled_bitmap("monitor_signal_weak", this, 18);
        m_wifi_none_img     = create_scaled_bitmap("monitor_signal_no", this, 18);
        m_network_wired_img = create_scaled_bitmap("monitor_network_wired", this, 18);
    }
}

void SideToolsPanel::msw_rescale()
{
    // Rebuilt fresh (not .msw_rescale()'d): a glyph-backed ScalableBitmap has no
    // bundled name to reload from, so it is re-rendered from the Material Symbol
    // at the new DPI, same as init_signal_bitmaps() below.
    m_printing_img = side_tools_glyph_bitmap(this, MaterialIcon::Print, 16, StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Device), "printer");
    m_arrow_img    = side_tools_glyph_bitmap(this, MaterialIcon::ChevronRight, 14, StateColor::semantic(MD3::Role::OnSurfaceVariant), "monitor_arrow");

    const wxColour none_state_fg = StateColor::semantic(MD3::Role::OnPrimary, MD3::ColorScheme::Device);
    m_none_printing_img = side_tools_glyph_bitmap(this, MaterialIcon::Print, 24, none_state_fg, "tab_monitor_active");
    m_none_arrow_img    = side_tools_glyph_bitmap(this, MaterialIcon::ChevronRight, 14, none_state_fg, "monitor_none_arrow");
    m_none_add_img      = side_tools_glyph_bitmap(this, MaterialIcon::Add, 14, none_state_fg, "monitor_none_add");

    init_signal_bitmaps();

    Refresh();
}

void SideToolsPanel::OnPaint(wxPaintEvent &event)
{
    wxPaintDC dc(this);
    doRender(dc);
}

void SideToolsPanel::render(wxDC &dc)
{
#ifdef __WXMSW__
    wxSize     size = GetSize();
    wxMemoryDC memdc;
    wxBitmap   bmp(size.x, size.y);
    memdc.SelectObject(bmp);
    memdc.Blit({0, 0}, size, &dc, {0, 0});

    {
        wxGCDC dc2(memdc);
        doRender(dc2);
    }

    memdc.SelectObject(wxNullBitmap);
    dc.DrawBitmap(bmp, 0, 0);
#else
    doRender(dc);
#endif
}

void SideToolsPanel::doRender(wxDC &dc)
{
    auto   left = FromDIP(15);
    wxSize size = GetSize();

    //if (m_none_printer) {
    //    dc.SetPen(SIDE_TOOLS_LIGHT_GREEN);
    //    dc.SetBrush(SIDE_TOOLS_LIGHT_GREEN);
    //    dc.DrawRectangle(0, 0, size.x, size.y);
    //}

    if (m_none_printer) {
        dc.SetPen(SIDE_TOOLS_BRAND);
        dc.SetBrush(SIDE_TOOLS_BRAND);
        dc.DrawRectangle(0, 0, size.x, size.y);

        dc.DrawBitmap(m_none_printing_img.bmp(), left, (size.y - m_none_printing_img.GetBmpSize().y) / 2);

        left += (m_none_printing_img.GetBmpSize().x + FromDIP(15));
        dc.DrawBitmap(m_none_arrow_img.bmp(), left, (size.y - m_none_arrow_img.GetBmpSize().y) / 2);

        left += (m_none_arrow_img.GetBmpSize().x + FromDIP(6));
        dc.SetFont(::Label::Body_14);
        dc.SetBackgroundMode(wxTRANSPARENT);
        dc.SetTextForeground(StateColor::semantic(MD3::Role::OnPrimary, MD3::ColorScheme::Device));

        wxString no_printer_str = _L("No printer");
        auto sizet = dc.GetTextExtent(no_printer_str);
        auto left_add_bitmap = size.x - FromDIP(30) - ScalableBitmap::GetBmpSize(m_wifi_none_img).x - m_none_add_img.GetBmpSize().x;
        auto size_width = left_add_bitmap - left;

        if (sizet.x > size_width) {
            wxString temp_str = wxEmptyString;
            for (auto i = 0; i < no_printer_str.Len(); i++) {
                if (dc.GetTextExtent(L("...") + temp_str).x < size_width) {
                    temp_str += no_printer_str[i];
                }
                else {
                    break;
                }
            }

            no_printer_str = temp_str + L("...");
        }

        dc.DrawText(no_printer_str, wxPoint(left, (size.y - sizet.y) / 2));

        left = size.x - FromDIP(30) - ScalableBitmap::GetBmpSize(m_wifi_none_img).x;
        dc.DrawBitmap(m_none_add_img.bmp(), left, (size.y - m_none_add_img.GetBmpSize().y) / 2);
    } else {
        dc.DrawBitmap(m_printing_img.bmp(), left, (size.y - m_printing_img.GetBmpSize().y) / 2);

        left += (m_printing_img.GetBmpSize().x + FromDIP(5));
        dc.DrawBitmap(m_arrow_img.bmp(), left, (size.y - m_arrow_img.GetBmpSize().y) / 2);

        left += (m_arrow_img.GetBmpSize().x + FromDIP(6));
        dc.SetFont(::Label::Body_14);
        dc.SetBackgroundMode(wxTRANSPARENT);
        dc.SetTextForeground(SIDE_TOOLS_GREY900);

        auto sizet = dc.GetTextExtent(m_dev_name);
        auto text_end = size.x - ScalableBitmap::GetBmpSize(m_wifi_none_img).x - 20;

        wxString finally_name = m_dev_name;
        if (sizet.x > (text_end - left)) {
            auto limit_width = text_end - left - dc.GetTextExtent("...").x - 20;
            for (auto i = 0; i < m_dev_name.length(); i++) {
                auto curr_width = dc.GetTextExtent(m_dev_name.substr(0, i));
                if (curr_width.x >= limit_width) {
                    finally_name = m_dev_name.substr(0, i) + "...";
                    break;
                }
            }
        }


        dc.DrawText(finally_name, wxPoint(left, (size.y - sizet.y) / 2));

        left = size.x - FromDIP(18) - ScalableBitmap::GetBmpSize(m_wifi_none_img).x;
        if (m_wifi_type == WifiSignal::NONE) dc.DrawBitmap(m_wifi_none_img, left, (size.y - ScalableBitmap::GetBmpSize(m_wifi_none_img).y) / 2);
        if (m_wifi_type == WifiSignal::WEAK) dc.DrawBitmap(m_wifi_weak_img, left, (size.y - ScalableBitmap::GetBmpSize(m_wifi_weak_img).y) / 2);
        if (m_wifi_type == WifiSignal::MIDDLE) dc.DrawBitmap(m_wifi_middle_img, left, (size.y - ScalableBitmap::GetBmpSize(m_wifi_middle_img).y) / 2);
        if (m_wifi_type == WifiSignal::STRONG) dc.DrawBitmap(m_wifi_strong_img, left, (size.y - ScalableBitmap::GetBmpSize(m_wifi_strong_img).y) / 2);
        if (m_wifi_type == WifiSignal::WIRED)  dc.DrawBitmap(m_network_wired_img, left, (size.y - ScalableBitmap::GetBmpSize(m_network_wired_img).y) / 2);
    }

    if (m_hover) {
        dc.SetPen(SIDE_TOOLS_BRAND);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(0, 0, size.x, size.y);
    }

    // a11y-focus: keyboard focus indicator -- a distinct inset 2px Primary ring so
    // the focused strip is visually separable from the 1px hover outline.
    if (m_focused) {
        dc.SetPen(wxPen(StateColor::semantic(MD3::Role::Primary, MD3::ColorScheme::Device), FromDIP(2)));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        const int inset = FromDIP(2);
        dc.DrawRectangle(inset, inset, size.x - 2 * inset, size.y - 2 * inset);
    }
}

void SideToolsPanel::on_mouse_left_down(wxMouseEvent &evt)
{
    m_click = true;
    // Move keyboard focus here on click so the focus ring and Enter/Space
    // activation follow a pointer interaction too.
    if (!HasFocus())
        SetFocus();
    Refresh();
}

void SideToolsPanel::on_set_focus(wxFocusEvent &evt)
{
    m_focused = true;
    Refresh();
    evt.Skip();
}

void SideToolsPanel::on_kill_focus(wxFocusEvent &evt)
{
    m_focused = false;
    Refresh();
    evt.Skip();
}

void SideToolsPanel::on_key_down(wxKeyEvent &evt)
{
    const int key = evt.GetKeyCode();
    if (key == WXK_RETURN || key == WXK_NUMPAD_ENTER || key == WXK_SPACE) {
        trigger_primary_action();
    } else {
        evt.Skip();
    }
}

void SideToolsPanel::trigger_primary_action()
{
    // The host (MonitorPanel/CalibrationPanel) connects the printer-switch popup
    // to this panel's wxEVT_LEFT_DOWN; on_printer_clicked ignores the pointer
    // coordinate, so a synthetic left-down replays the exact same action for the
    // keyboard.
    wxMouseEvent e(wxEVT_LEFT_DOWN);
    e.SetEventObject(this);
    GetEventHandler()->ProcessEvent(e);
}

void SideToolsPanel::on_mouse_left_up(wxMouseEvent &evt)
{
     m_click = false;
     Refresh();
}

void SideToolsPanel::on_mouse_enter(wxMouseEvent &evt)
{
    m_hover = true;
    Refresh();
}

void SideToolsPanel::on_mouse_leave(wxMouseEvent &evt)
{
    m_hover = false;
    Refresh();
}

SideTools::SideTools(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size)
{
    wxPanel::Create(parent, id, pos, size);
    // Device-page surface (Device.jsx SurfaceDim), replacing the legacy
    // ThemeColor::White literal.
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceDim));

    m_side_tools = new SideToolsPanel(this, wxID_ANY);

    m_connection_info = new Button(this, wxEmptyString);
    m_connection_info->SetBackgroundColor(ThemeColor::Warning);
    m_connection_info->SetBorderColor(ThemeColor::Warning);
    m_connection_info->SetTextColor(ThemeColor::White);
    m_connection_info->SetFont(::Label::Body_13);
    m_connection_info->SetCornerRadius(0);
    m_connection_info->SetSize(wxSize(FromDIP(-1), FromDIP(25)));
    m_connection_info->SetMinSize(wxSize(FromDIP(-1), FromDIP(25)));
    m_connection_info->SetMaxSize(wxSize(FromDIP(-1), FromDIP(25)));


    wxBoxSizer* connection_sizer_V = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer* connection_sizer_H = new wxBoxSizer(wxHORIZONTAL);


    bool is_zh  = wxGetApp().app_config->get("language") == "zh_CN";
    wxString hyperlink_url = is_zh ? wxT("https://wiki.bambulab.com/zh/software/bambu-studio/failed-to-connect-printer") :
                             wxT("https://wiki.bambulab.com/en/software/bambu-studio/failed-to-connect-printer");


    m_hyperlink = new wxHyperlinkCtrl(m_connection_info, wxID_ANY, _L("Failed to connect to the server"), hyperlink_url, wxDefaultPosition, wxDefaultSize, wxHL_DEFAULT_STYLE);
    m_hyperlink->SetBackgroundColour(ThemeColor::Warning);

    // MD3: expand/collapse chevron glyph on the warning-fill banner, falling
    // back to the legacy monitir_err_open/close rasters when the icon face is
    // unavailable (mirrors the wifi/printer/arrow icons in this file).
    m_more_err_open  = side_tools_glyph_bitmap(this, MaterialIcon::ExpandMore, 16, ThemeColor::White, "monitir_err_open");
    m_more_err_close = side_tools_glyph_bitmap(this, MaterialIcon::ExpandLess, 16, ThemeColor::White, "monitir_err_close");
    m_more_button = new ScalableButton(m_connection_info, wxID_ANY, "monitir_err_open");
    m_more_button->SetBitmap(m_more_err_open.bmp());
    m_more_button->Bind(wxEVT_ENTER_WINDOW, [this](auto& e) {SetCursor(wxCURSOR_HAND); });
    m_more_button->Bind(wxEVT_LEAVE_WINDOW, [this](auto& e) {SetCursor(wxCURSOR_ARROW); });
    m_more_button->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {
        if (!m_more_err_state) {
            m_more_button->SetBitmap(m_more_err_close.bmp());
            Freeze();
            m_side_error_panel->Show();
            m_more_err_state = true;
            m_tabpanel->Refresh();
            m_tabpanel->Layout();
            Thaw();
        }
        else {
            m_more_button->SetBitmap(m_more_err_open.bmp());
            Freeze();
            m_side_error_panel->Hide();
            m_more_err_state = false;
            m_tabpanel->Refresh();
            m_tabpanel->Layout();
            Thaw();
        }

        });

    connection_sizer_H->Add(m_hyperlink, 0, wxALIGN_CENTER | wxALL, 5);
    connection_sizer_H->Add(m_more_button, 0, wxALIGN_CENTER | wxALL, 3);
    connection_sizer_V->Add(connection_sizer_H, 0, wxALIGN_CENTER, 0);

    m_connection_info->SetSizer(connection_sizer_V);
    m_connection_info->Layout();
    connection_sizer_V->Fit(m_connection_info);

    m_side_error_panel = new wxWindow(this, wxID_ANY);
    m_side_error_panel->SetBackgroundColour(StateColor::semantic(MD3::Role::ErrorContainer));
    m_side_error_panel->SetMinSize(wxSize(-1, -1));
    m_side_error_panel->SetMaxSize(wxSize(-1, -1));

    m_side_error_panel->Hide();
    m_more_button->Hide();
    m_connection_info->Hide();

    wxBoxSizer* sizer_print_failed_info = new wxBoxSizer(wxVERTICAL);
    m_side_error_panel->SetSizer(sizer_print_failed_info);

    wxBoxSizer* sizer_error_code = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_error_desc = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* sizer_extra_info = new wxBoxSizer(wxHORIZONTAL);

    m_link_network_state = new wxHyperlinkCtrl(m_side_error_panel, wxID_ANY,_L("Check the status of current system services"),"",wxDefaultPosition,wxDefaultSize, wxHL_ALIGN_CENTRE |wxST_ELLIPSIZE_END);
    m_link_network_state->SetMinSize(wxSize(FromDIP(220), -1));
    m_link_network_state->SetMaxSize(wxSize(FromDIP(220), -1));
    m_link_network_state->SetFont(::Label::Body_12);
    m_link_network_state->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {wxGetApp().link_to_network_check(); });
    m_link_network_state->Bind(wxEVT_ENTER_WINDOW, [this](auto& e) {m_link_network_state->SetCursor(wxCURSOR_HAND); });
    m_link_network_state->Bind(wxEVT_LEAVE_WINDOW, [this](auto& e) {m_link_network_state->SetCursor(wxCURSOR_ARROW); });

    auto st_title_error_code = new wxStaticText(m_side_error_panel, wxID_ANY, _L("code"), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    auto st_title_error_code_doc = new wxStaticText(m_side_error_panel, wxID_ANY, ": ");
    m_st_txt_error_code = new Label(m_side_error_panel, wxEmptyString, LB_AUTO_WRAP);
    // a11y-contrast: this text sits on the ErrorContainer fill (see
    // m_side_error_panel above), so it must use the OnErrorContainer role to meet
    // WCAG AA -- the flat legacy TextDisabled grey failed contrast on that surface.
    st_title_error_code->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));
    st_title_error_code_doc->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));
    m_st_txt_error_code->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));
    st_title_error_code->SetFont(::Label::Body_12);
    st_title_error_code_doc->SetFont(::Label::Body_12);
    m_st_txt_error_code->SetFont(::Label::Body_12);
    st_title_error_code->SetMinSize(wxSize(FromDIP(32), -1));
    st_title_error_code->SetMaxSize(wxSize(FromDIP(32), -1));
    m_st_txt_error_code->SetMinSize(wxSize(FromDIP(175), -1));
    m_st_txt_error_code->SetMaxSize(wxSize(FromDIP(175), -1));
    sizer_error_code->Add(st_title_error_code, 0, wxALL, 0);
    sizer_error_code->Add(st_title_error_code_doc, 0, wxALL, 0);
    sizer_error_code->Add(m_st_txt_error_code, 0, wxALL, 0);


    auto st_title_error_desc = new wxStaticText(m_side_error_panel, wxID_ANY, wxT("desc"), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    auto st_title_error_desc_doc = new wxStaticText(m_side_error_panel, wxID_ANY, ": ");
    m_st_txt_error_desc = new Label(m_side_error_panel, wxEmptyString, LB_AUTO_WRAP);
    st_title_error_desc->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));
    st_title_error_desc_doc->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));
    m_st_txt_error_desc->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));
    st_title_error_desc->SetFont(::Label::Body_12);
    st_title_error_desc_doc->SetFont(::Label::Body_12);
    m_st_txt_error_desc->SetFont(::Label::Body_12);
    st_title_error_desc->SetMinSize(wxSize(FromDIP(32), -1));
    st_title_error_desc->SetMaxSize(wxSize(FromDIP(32), -1));
    m_st_txt_error_desc->SetMinSize(wxSize(FromDIP(175), -1));
    m_st_txt_error_desc->SetMaxSize(wxSize(FromDIP(175), -1));
    sizer_error_desc->Add(st_title_error_desc, 0, wxALL, 0);
    sizer_error_desc->Add(st_title_error_desc_doc, 0, wxALL, 0);
    sizer_error_desc->Add(m_st_txt_error_desc, 0, wxALL, 0);

    auto st_title_extra_info = new wxStaticText(m_side_error_panel, wxID_ANY, wxT("info"), wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
    auto st_title_extra_info_doc = new wxStaticText(m_side_error_panel, wxID_ANY, ": ");
    m_st_txt_extra_info = new Label(m_side_error_panel, wxEmptyString, LB_AUTO_WRAP);
    st_title_extra_info->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));
    st_title_extra_info_doc->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));
    m_st_txt_extra_info->SetForegroundColour(StateColor::semantic(MD3::Role::OnErrorContainer));
    st_title_extra_info->SetFont(::Label::Body_12);
    st_title_extra_info_doc->SetFont(::Label::Body_12);
    m_st_txt_extra_info->SetFont(::Label::Body_12);
    st_title_extra_info->SetMinSize(wxSize(FromDIP(32), -1));
    st_title_extra_info->SetMaxSize(wxSize(FromDIP(32), -1));
    m_st_txt_extra_info->SetMinSize(wxSize(FromDIP(175), -1));
    m_st_txt_extra_info->SetMaxSize(wxSize(FromDIP(175), -1));
    sizer_extra_info->Add(st_title_extra_info, 0, wxALL, 0);
    sizer_extra_info->Add(st_title_extra_info_doc, 0, wxALL, 0);
    sizer_extra_info->Add(m_st_txt_extra_info, 0, wxALL, 0);

    sizer_print_failed_info->Add(m_link_network_state, 0, wxALIGN_CENTER, 3);
    sizer_print_failed_info->Add(sizer_error_code, 0, wxLEFT, 5);
    sizer_print_failed_info->Add(0, 0, 0, wxTOP, FromDIP(3));
    sizer_print_failed_info->Add(sizer_error_desc, 0, wxLEFT, 5);
    sizer_print_failed_info->Add(0, 0, 0, wxTOP, FromDIP(3));
    sizer_print_failed_info->Add(sizer_extra_info, 0, wxLEFT, 5);

    m_st_txt_error_desc->SetLabel("");

    wxBoxSizer* m_main_sizer = new wxBoxSizer(wxVERTICAL);
    m_main_sizer->Add(m_connection_info, 0, wxEXPAND, 0);
    m_main_sizer->Add(m_side_error_panel, 0, wxEXPAND, 0);
    m_main_sizer->Add(m_side_tools, 1, wxEXPAND, 0);
    SetSizer(m_main_sizer);
    Layout();
    Fit();
}

void SideTools::msw_rescale()
{
    m_side_tools->msw_rescale();
    m_connection_info->SetCornerRadius(0);
    m_connection_info->SetSize(wxSize(FromDIP(220), FromDIP(25)));
    m_connection_info->SetMinSize(wxSize(FromDIP(220), FromDIP(25)));
    m_connection_info->SetMaxSize(wxSize(FromDIP(220), FromDIP(25)));
}

bool SideTools::is_in_interval()
{
    return m_side_tools->is_in_interval();
}

void SideTools::set_current_printer_name(std::string dev_name)
{
    m_side_tools->set_current_printer_name(dev_name);
}

void SideTools::set_current_printer_signal(WifiSignal sign)
{
    m_side_tools->set_current_printer_signal(sign);
}

void SideTools::set_none_printer_mode()
{
    m_side_tools->set_none_printer_mode();
}

void SideTools::start_interval()
{
    m_side_tools->start_interval();
}

void SideTools::update_connect_err_info(int code, wxString desc, wxString info)
{
    m_st_txt_error_code->SetLabelText(wxString::Format("%d", code));
    m_st_txt_error_desc->SetLabelText(desc);
    m_st_txt_extra_info->SetLabelText(info);

    if (code == BAMBU_NETWORK_ERR_CONNECTION_TO_PRINTER_FAILED) {
        m_link_network_state->Hide();
    }
    else if (code == BAMBU_NETWORK_ERR_CONNECTION_TO_SERVER_FAILED) {
        m_link_network_state->Show();
    }
}

void SideTools::update_status(MachineObject* obj)
{
    if (!obj) return;

    /* Update Device Info */
    m_side_tools->set_current_printer_name(obj->get_dev_name());

    // update wifi signal image
    int wifi_signal_val = 0;
    if (!obj->is_connected() || obj->is_connecting()) {
        m_side_tools->set_current_printer_signal(WifiSignal::NONE);
    }
    else if (!obj->is_lan_mode_printer() && !obj->is_online()) {
        m_side_tools->set_current_printer_signal(WifiSignal::NONE);/*STUDIO-10185*/
    }
    else {
        if (obj->network_wired) {
            m_side_tools->set_current_printer_signal(WifiSignal::WIRED);
        }
        else if (!obj->wifi_signal.empty() && boost::ends_with(obj->wifi_signal, "dBm")) {
            try {
                wifi_signal_val = std::stoi(obj->wifi_signal.substr(0, obj->wifi_signal.size() - 3));
            }
            catch (...) {
                ;
            }
            if (wifi_signal_val > -45) {
                m_side_tools->set_current_printer_signal(WifiSignal::STRONG);
            }
            else if (wifi_signal_val <= -45 && wifi_signal_val >= -60) {
                m_side_tools->set_current_printer_signal(WifiSignal::MIDDLE);
            }
            else {
                m_side_tools->set_current_printer_signal(WifiSignal::WEAK);
            }
        }
        else {
            m_side_tools->set_current_printer_signal(WifiSignal::MIDDLE);
        }
    }
}

void SideTools::show_status(int status)
{
    if (((status & (int)MonitorStatus::MONITOR_DISCONNECTED) != 0) || ((status & (int)MonitorStatus::MONITOR_DISCONNECTED_SERVER) != 0)) {
        if ((status & (int)MonitorStatus::MONITOR_DISCONNECTED_SERVER)) {
            m_hyperlink->SetLabel(_L("Failed to connect to the server"));
            update_connect_err_info(BAMBU_NETWORK_ERR_CONNECTION_TO_SERVER_FAILED,
                _L("Failed to connect to cloud service"),
                _L("Please click on the hyperlink above to view the cloud service status"));
        }
        else {
            m_hyperlink->SetLabel(_L("Failed to connect to the printer"));
            update_connect_err_info(BAMBU_NETWORK_ERR_CONNECTION_TO_PRINTER_FAILED,
                _L("Connection to printer failed"),
                _L("Please check the network connection of the printer and Studio."));
        }

        m_hyperlink->Show();
        m_connection_info->SetLabel(wxEmptyString);
        m_connection_info->SetBackgroundColor(ThemeColor::Warning);
        m_connection_info->SetBorderColor(ThemeColor::Warning);
        m_connection_info->Show();
        m_more_button->Show();

    }
    else if ((status & (int)MonitorStatus::MONITOR_NORMAL) != 0) {
        m_connection_info->Hide();
        m_more_button->Hide();
        m_side_error_panel->Hide();
    }
    else if ((status & (int)MonitorStatus::MONITOR_CONNECTING) != 0) {
        m_hyperlink->Hide();
        m_connection_info->SetLabel(_L("Connecting..."));
        m_connection_info->SetBackgroundColor(ThemeColor::BrandGreen);
        m_connection_info->SetBorderColor(ThemeColor::BrandGreen);
        m_connection_info->Show();
        m_more_button->Hide();
        m_side_error_panel->Hide();
    }

    if ((status & (int)MonitorStatus::MONITOR_NO_PRINTER) != 0) {
        m_side_tools->set_none_printer_mode();
        m_connection_info->Hide();
        m_side_error_panel->Hide();
        m_more_button->Hide();
    }
    else if (((status & (int)MonitorStatus::MONITOR_NORMAL) != 0)
        || ((status & (int)MonitorStatus::MONITOR_DISCONNECTED) != 0)
        || ((status & (int)MonitorStatus::MONITOR_DISCONNECTED_SERVER) != 0)
        || ((status & (int)MonitorStatus::MONITOR_CONNECTING) != 0)
        ) {
        if (((status & (int)MonitorStatus::MONITOR_DISCONNECTED) != 0)
            || ((status & (int)MonitorStatus::MONITOR_DISCONNECTED_SERVER) != 0)
            || ((status & (int)MonitorStatus::MONITOR_CONNECTING) != 0)) {
            m_side_tools->set_current_printer_signal(WifiSignal::NONE);
        }
    }
    Layout();
    Fit();
}

SideTools::~SideTools()
{
}

}}
