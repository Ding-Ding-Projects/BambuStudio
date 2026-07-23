#include "PrivacyUpdateDialog.hpp"
#include "GUI_App.hpp"
#include "BitmapCache.hpp"
#include <wx/dcgraph.h>
#include <slic3r/GUI/I18N.hpp>


namespace Slic3r { namespace GUI {

wxDEFINE_EVENT(EVT_PRIVACY_UPDATE_CONFIRM, wxCommandEvent);
wxDEFINE_EVENT(EVT_PRIVACY_UPDATE_CANCEL, wxCommandEvent);

static std::string url_encode(const std::string& value) {
	std::ostringstream escaped;
	escaped.fill('0');
	escaped << std::hex;
	for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
		std::string::value_type c = (*i);

		// Keep alphanumeric and other accepted characters intact
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			escaped << c;
			continue;
		}

		// Any other characters are percent-encoded
		escaped << std::uppercase;
		escaped << '%' << std::setw(2) << int((unsigned char)c);
		escaped << std::nouppercase;
	}
	return escaped.str();
}

PrivacyUpdateDialog::PrivacyUpdateDialog(wxWindow* parent, wxWindowID id, const wxString& title, enum ButtonStyle btn_style, const wxPoint& pos, const wxSize& size, long style)
    : MD3Dialog(parent, title, wxEmptyString, MaterialIcon::Lock)
{
    //webview — hosted in the kit body (pad 0/24)
    m_vebview_release_note = CreateTipView(this);
    if (m_vebview_release_note == nullptr) {
        wxLogError("Could not init m_browser");
        return;
    }
    m_vebview_release_note->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    m_vebview_release_note->SetSize(wxSize(FromDIP(540), FromDIP(340)));
    m_vebview_release_note->SetMinSize(wxSize(FromDIP(540), FromDIP(340)));

    fs::path ph(resources_dir());
    ph /= "tooltip/privacyupdate.html";
    m_host_url = ph.string();
    std::replace(m_host_url.begin(), m_host_url.end(), '\\', '/');
    m_host_url = "file:///" + m_host_url;
    m_vebview_release_note->LoadURL(from_u8(m_host_url));
    GetContentSizer()->Add(m_vebview_release_note, 1, wxEXPAND);

#ifndef __WINDOWS__
    m_vebview_release_note->Bind(wxEVT_WEBVIEW_LOADED, [this](auto& e) {
#else
    m_vebview_release_note->Bind(wxEVT_WEBVIEW_NAVIGATED, [this](auto& e) {
#endif
         if (!m_mkdown_text.empty()) {
             ShowReleaseNote(m_mkdown_text);
         }
         e.Skip();
    });

    //m_vebview_release_note->Bind(wxEVT_WEBVIEW_NAVIGATING , &PrivacyUpdateDialog::OnNavigating, this);

    // Footer: text "Log Out" (secondary) + filled pill "Accept" (primary,
    // OnPrimary text). Kept on wxEVT_LEFT_DOWN so the existing custom
    // confirm/cancel events + on_hide() flow is unchanged.
    m_button_cancel = new Button(this, _L("Log Out"));
    m_button_cancel->SetVariant(Button::Variant::Text);
    m_button_cancel->SetButtonSize(Button::Size::Medium);
    m_button_cancel->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        wxCommandEvent evt(EVT_PRIVACY_UPDATE_CANCEL);
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
        this->on_hide();
        });

    m_button_ok = new Button(this, _L("Accept"));
    m_button_ok->SetVariant(Button::Variant::Filled);
    m_button_ok->SetButtonSize(Button::Size::Medium);
    m_button_ok->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) {
        wxCommandEvent evt(EVT_PRIVACY_UPDATE_CONFIRM, GetId());
        e.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
        this->on_hide();
    });

    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {e.Veto(); });

    // Keyboard a11y: the window-close is vetoed to force an explicit choice, which
    // also swallows the usual Escape-to-cancel. Route Escape through the same
    // guarded decline path as the header [x] (OnHeaderClose): it declines + hides
    // only when a "Log Out" affordance is shown, otherwise stays inert.
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
        if (e.GetKeyCode() == WXK_ESCAPE)
            OnHeaderClose();
        else
            e.Skip();
    });

    if (btn_style != CONFIRM_AND_CANCEL)
        m_button_cancel->Hide();
    else
        m_button_cancel->Show();

    AddFooterButton(m_button_cancel);
    AddFooterButton(m_button_ok);

    SetMinSize(wxSize(FromDIP(540), -1));
    Layout();
    Fit();
    CenterOnParent();
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
}

void PrivacyUpdateDialog::OnHeaderClose()
{
    // Preserve the original forced-choice: the header [x] only acts as the
    // "Log Out"/decline affordance when that button is available; otherwise it
    // is inert (the window-close was vetoed in the legacy dialog).
    if (m_button_cancel && m_button_cancel->IsShown()) {
        wxCommandEvent evt(EVT_PRIVACY_UPDATE_CANCEL);
        evt.SetEventObject(this);
        GetEventHandler()->ProcessEvent(evt);
        this->on_hide();
    }
}

wxWebView* PrivacyUpdateDialog::CreateTipView(wxWindow* parent)
{
	wxWebView* tipView = WebView::CreateWebView(parent, "");
	return tipView;
}

void PrivacyUpdateDialog::OnNavigating(wxWebViewEvent& event)
{
    wxString jump_url = event.GetURL();
    if (jump_url != m_host_url) {
        event.Veto();
        wxLaunchDefaultBrowser(jump_url);
    }
    else {
        event.Skip();
    }
}

bool PrivacyUpdateDialog::ShowReleaseNote(std::string content)
{
	auto script = "window.showMarkdown('" + url_encode(content) + "', true);";
    RunScript(script);
    return true;
}

void PrivacyUpdateDialog::RunScript(std::string script)
{
    WebView::RunScript(m_vebview_release_note, script);
    std::string switch_dark_mode_script = "SwitchDarkMode(";
    switch_dark_mode_script += wxGetApp().app_config->get("dark_color_mode") == "1" ? "true" : "false";
    switch_dark_mode_script += ");";
    WebView::RunScript(m_vebview_release_note, switch_dark_mode_script);
    script.clear();
}

void PrivacyUpdateDialog::on_show()
{
    wxGetApp().UpdateDlgDarkUI(this);
    this->ShowModal();
}

void PrivacyUpdateDialog::on_hide()
{
    EndModal(wxID_OK);
}

void PrivacyUpdateDialog::update_btn_label(wxString ok_btn_text, wxString cancel_btn_text)
{
    m_button_ok->SetLabel(ok_btn_text);
    m_button_cancel->SetLabel(cancel_btn_text);
    rescale();
}

PrivacyUpdateDialog::~PrivacyUpdateDialog()
{

}

void PrivacyUpdateDialog::on_dpi_changed(const wxRect& suggested_rect)
{
    MD3Dialog::on_dpi_changed(suggested_rect); // reshape the rounded frame
    rescale();
}

void PrivacyUpdateDialog::rescale()
{
    m_button_ok->Rescale();
    m_button_cancel->Rescale();
}

}} // namespace Slic3r::GUI