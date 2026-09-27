#include "ModelMall.hpp"
#include "GUI_App.hpp"

#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include "wx/evtloop.h"

#include "libslic3r/Model.hpp"
#include "MainFrame.hpp"
#include "GUI_App.hpp"
#include "Plater.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/StateColor.hpp"

namespace Slic3r {
namespace GUI {
    ModelMallDialog::ModelMallDialog(Plater* plater /*= nullptr*/)
        :DPIFrame(nullptr, wxID_ANY, _L("3D Models"), wxDefaultPosition, wxDefaultSize, wxCLOSE_BOX|wxDEFAULT_DIALOG_STYLE|wxMAXIMIZE_BOX|wxMINIMIZE_BOX|wxRESIZE_BORDER)
    {
        // icon
        std::string icon_path = (boost::format("%1%/images/BambuStudioTitle.ico") % resources_dir()).str();
        SetIcon(wxIcon(encode_path(icon_path.c_str()), wxBITMAP_TYPE_ICO));

        SetSize(MODEL_MALL_PAGE_SIZE);
        SetMinSize(wxSize(MODEL_MALL_PAGE_SIZE.x / 4, MODEL_MALL_PAGE_SIZE.y / 4));

        wxBoxSizer* m_sizer_main = new wxBoxSizer(wxVERTICAL);

        auto m_line_top = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1), wxTAB_TRAVERSAL);
        // MD3 OutlineVariant divider (the legacy wxColour(166,169,170) had no
        // entry in the dark-remap table, so it stayed light in dark mode).
        m_line_top->SetBackgroundColour(StateColor::semantic(MD3::Role::OutlineVariant));
        m_sizer_main->Add(m_line_top, 0, wxEXPAND, 0);

        m_web_control_panel = new wxPanel(this, wxID_ANY, wxDefaultPosition, MODEL_MALL_PAGE_CONTROL_SIZE, wxTAB_TRAVERSAL);
        // MD3 SurfaceContainerLowest toolbar chrome (was a raw *wxWHITE fill).
        m_web_control_panel->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
        m_web_control_panel->SetSize(MODEL_MALL_PAGE_CONTROL_SIZE);


        wxBoxSizer* m_sizer_web_control = new wxBoxSizer(wxHORIZONTAL);

        // MD3 store-toolbar nav actions: Material Symbols glyphs (arrow_back /
        // arrow_forward / refresh) tinted OnSurfaceVariant on the
        // SurfaceContainerLowest panel. Capability-gated: when the icon font is
        // unavailable, fall back to the legacy mall_control_* raster assets so a
        // missing TTF degrades to the old look rather than rendering tofu.
        const wxColour ctrl_panel_bg  = StateColor::semantic(MD3::Role::SurfaceContainerLowest);
        const wxColour ctrl_icon_col  = StateColor::semantic(MD3::Role::OnSurfaceVariant);
        const int      ctrl_icon_px   = 16;
        const bool     use_ctrl_glyph = MaterialIcon::available();

        auto make_control_button = [&](const std::string &raster_name, uint32_t glyph) -> ScalableButton * {
            // Empty icon name on the glyph path: ScalableButton::msw_rescale()
            // (reached via UpdateDarkUI) reloads m_current_icon_name over the
            // bitmap, so leaving it empty keeps the glyph from being clobbered.
            ScalableButton *btn = new ScalableButton(
                m_web_control_panel, wxID_ANY,
                use_ctrl_glyph ? std::string() : raster_name,
                wxEmptyString, wxDefaultSize, wxDefaultPosition,
                wxBU_EXACTFIT | wxNO_BORDER, true);
            if (use_ctrl_glyph)
                btn->SetBitmap(MaterialIcon::bitmap(this, glyph, ctrl_icon_px, ctrl_icon_col));
            btn->SetBackgroundColour(ctrl_panel_bg);
            btn->SetSize(wxSize(FromDIP(25), FromDIP(30)));
            btn->SetMinSize(wxSize(FromDIP(25), FromDIP(30)));
            btn->SetMaxSize(wxSize(FromDIP(25), FromDIP(30)));
            btn->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) { SetCursor(wxCursor(wxCURSOR_HAND)); });
            btn->Bind(wxEVT_LEAVE_WINDOW, [this](auto &e) { SetCursor(wxCursor(wxCURSOR_ARROW)); });
            return btn;
        };

        auto m_control_back = make_control_button("mall_control_back", MaterialIcon::ArrowBack);
        m_control_back->Bind(wxEVT_LEFT_DOWN, &ModelMallDialog::on_back, this);

        auto m_control_forward = make_control_button("mall_control_forward", MaterialIcon::ArrowForward);
        m_control_forward->Bind(wxEVT_LEFT_DOWN, &ModelMallDialog::on_forward, this);

        auto m_control_refresh = make_control_button("mall_control_refresh", MaterialIcon::Refresh);
        m_control_refresh->Bind(wxEVT_LEFT_DOWN, &ModelMallDialog::on_refresh, this);

#ifdef __APPLE__
        m_control_back->SetToolTip(_L("Click to return (Command + Left Arrow)"));
        m_control_forward->SetToolTip(_L("Click to continue (Command + Right Arrow)"));
#else
        m_control_back->SetToolTip(_L("Click to return (Alt + Left Arrow)"));
        m_control_forward->SetToolTip(_L("Click to continue (Alt + Right Arrow)"));
#endif

        m_control_refresh->SetToolTip(_L("Refresh"));
        /* auto m_textCtrl1 = new wxTextCtrl(m_web_control_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(600, 30), 0);
         auto m_button1 = new wxButton(m_web_control_panel, wxID_ANY, wxT("GO"), wxDefaultPosition, wxDefaultSize, 0);
         m_button1->Bind(wxEVT_BUTTON, [this,m_textCtrl1](auto& e) {
             go_to_url(m_textCtrl1->GetValue());
         });*/

        m_sizer_web_control->Add( m_control_back, 0, wxALIGN_CENTER | wxLEFT, FromDIP(26) );
        m_sizer_web_control->Add(m_control_forward, 0, wxALIGN_CENTER | wxLEFT, FromDIP(26));
        m_sizer_web_control->Add(m_control_refresh, 0, wxALIGN_CENTER | wxLEFT, FromDIP(26));
        //m_sizer_web_control->Add(m_button1, 0, wxALIGN_CENTER|wxLEFT, 5);
        //m_sizer_web_control->Add(m_textCtrl1, 0, wxALIGN_CENTER|wxLEFT, 5);

        m_web_control_panel->SetSizer(m_sizer_web_control);
        m_web_control_panel->Layout();
        m_sizer_web_control->Fit(m_web_control_panel);

        m_browser = WebView::CreateWebView(this, wxEmptyString);
        if (m_browser == nullptr) {
            wxLogError("Could not init m_browser");
            return;
        }

        m_browser->SetSize(MODEL_MALL_PAGE_WEB_SIZE);
        m_browser->SetMinSize(MODEL_MALL_PAGE_WEB_SIZE);
        m_browser->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED, &ModelMallDialog::OnScriptMessage, this, m_browser->GetId());

        m_sizer_main->Add(m_web_control_panel, 0, wxEXPAND, 0);
        m_sizer_main->Add(m_browser, 1, wxEXPAND, 0);
        SetSizer(m_sizer_main);
        Layout();
        Fit();

        Centre(wxBOTH);
        Bind(wxEVT_SHOW, &ModelMallDialog::on_show, this);

        Bind(wxEVT_CLOSE_WINDOW, [this](auto& e) {
            this->Hide();
        });
    }


    ModelMallDialog::~ModelMallDialog()
    {
    }

    void ModelMallDialog::OnScriptMessage(wxWebViewEvent& evt)
    {
        try {
            wxString strInput = evt.GetString();
            json     j = json::parse(strInput);

            wxString strCmd = j["command"];

            if(strCmd == "request_close_publish_window") {
                this->Hide();
            }

        }
        catch (std::exception& /*e*/) {
            // wxMessageBox(e.what(), "json Exception", MB_OK);
        }
    }

    void ModelMallDialog::on_dpi_changed(const wxRect& suggested_rect)
    {
    }

    void ModelMallDialog::on_show(wxShowEvent& event)
    {
        wxGetApp().UpdateFrameDarkUI(this);
        if (event.IsShown()) {
            Centre(wxBOTH);
        }
        /*else {
            go_to_url(m_url);
        }*/
        event.Skip();
    }

    void ModelMallDialog::on_refresh(wxMouseEvent& evt)
    {
        if (!m_browser->GetCurrentURL().empty()) {
            m_browser->Reload();
        }
    }

    void ModelMallDialog::on_back(wxMouseEvent& evt)
    {
        if (m_browser->CanGoBack()) {
            m_browser->GoBack();
        }
    }

    void ModelMallDialog::on_forward(wxMouseEvent& evt)
    {
        if (m_browser->CanGoForward()) {
            m_browser->GoForward();
        }
    }

    void ModelMallDialog::go_to_url(wxString url)
    {
        //m_browser->LoadURL(url);
        WebView::LoadUrl(m_browser, url);
    }

    void ModelMallDialog::show_control(bool show)
    {
        m_web_control_panel->Show(show);
        Layout();
        Fit();
    }

    void ModelMallDialog::go_to_mall(wxString url)
    {
        /*if (!url.empty() && m_homepage_url.empty()) {
            m_homepage_url = url;
        }*/
        if(url.empty())return;
        m_url = url;
        go_to_url(url);
    }

    void ModelMallDialog::go_to_publish(wxString url)
    {
        /*if (!url.empty() && m_publish_url.empty()) {
            m_publish_url = url;
        }*/
        if(url.empty())return;
        m_url = url;
        go_to_url(url);
    }

}
} // namespace Slic3r::GUI
