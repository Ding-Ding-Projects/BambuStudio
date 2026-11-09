// Local status API, the rows of its Preferences section. See StatusApiUi.hpp.

#include "StatusApiUi.hpp"
#include "StatusApiCore.hpp"
#include "StatusApiServer.hpp"

#include "../FeatureServices/SurfaceRegistry.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../format.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/TextInput.hpp"
#include "libslic3r/AppConfig.hpp"

#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/timer.h>

namespace Slic3r { namespace GUI { namespace StatusApi {

namespace {

constexpr int kRowLeft  = 24; // Preferences ITEM_LEFT_PADDING
constexpr int kRowRight = 16; // Preferences ITEM_RIGHT_PADDING
constexpr int kWrap     = 520;

Label *body_label(wxWindow *parent, const wxString &text, MD3::Role colour = MD3::Role::OnSurface, const wxFont &font = ::Label::Body_13)
{
    auto *label = new Label(parent, text);
    label->SetFont(font);
    label->SetForegroundColour(StateColor::semantic(colour));
    label->Wrap(parent->FromDIP(kWrap));
    return label;
}

Button *kit_button(wxWindow *parent, const wxString &text, Button::Variant variant)
{
    auto *button = new Button(parent, text);
    button->SetVariant(variant);
    button->SetButtonSize(Button::Size::Small);
    return button;
}

wxString status_line()
{
    const State state = current_state();
    if (state == State::Listening)
        return format_wxstr(_L("%1% at http://127.0.0.1:%2%/api/v1/"), state_text(state), std::to_string(current_port()));
    return state_text(state);
}

// Shows the listener state and re-reads it every second while the Preferences page lives, so the
// row follows the switch and the port without a listener plumbing of its own.
class StatusRow : public wxPanel
{
public:
    explicit StatusRow(wxWindow *parent) : wxPanel(parent, wxID_ANY), m_timer(this)
    {
        SetBackgroundColour(parent->GetBackgroundColour());
        auto *row   = new wxBoxSizer(wxHORIZONTAL);
        auto *title = body_label(this, _L("Status API state"));
        m_value     = body_label(this, status_line(), MD3::Role::OnSurfaceVariant, ::Label::Body_12);
        m_value->SetName(_L("Status API state"));
        row->AddSpacer(FromDIP(kRowLeft));
        row->Add(title, 1, wxALIGN_CENTER_VERTICAL);
        row->Add(m_value, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(kRowRight));
        SetSizer(row);
        Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
            const wxString text = status_line();
            if (text != m_shown) {
                m_shown = text;
                m_value->SetLabel(text);
                Layout();
            }
        });
        m_shown = status_line();
        m_timer.Start(1000);
    }

private:
    Label   *m_value = nullptr;
    wxString m_shown;
    wxTimer  m_timer;
};

class TokenRow : public wxPanel
{
public:
    explicit TokenRow(wxWindow *parent) : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(parent->GetBackgroundColour());
        auto &registry = FeatureServices::SurfaceRegistry::instance();
        registry.register_sensitive(this);
        auto *outer = new wxBoxSizer(wxVERTICAL);
        auto *row   = new wxBoxSizer(wxHORIZONTAL);
        auto *title = body_label(this, _L("Access token"));
        // wxTE_PASSWORD cannot be switched on an existing Windows edit control, so a masked and a
        // plain field share the value and only one of them is shown.
        m_masked = new TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(220), -1),
                                 wxTE_PASSWORD | wxTE_READONLY);
        m_plain  = new TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(220), -1),
                                 wxTE_READONLY);
        for (TextInput *input : {m_masked, m_plain}) {
            input->GetTextCtrl()->SetName(_L("Access token"));
            input->GetTextCtrl()->SetFont(::Label::Mono_13);
            registry.register_sensitive(input);
            registry.register_sensitive(input->GetTextCtrl());
        }
        m_plain->Hide();
        m_toggle = new Button(this, wxEmptyString);
        m_toggle->SetIconButton(Button::IconShape::Square);
        m_toggle->SetGlyph(MaterialIcon::Visibility);
        m_toggle->SetName(_L("Show token"));
        m_toggle->SetToolTip(_L("Show token"));
        m_copy  = kit_button(this, _L("Copy token"), Button::Variant::Outlined);
        m_regen = kit_button(this, _L("Regenerate token"), Button::Variant::Outlined);
        row->AddSpacer(FromDIP(kRowLeft));
        row->Add(title, 1, wxALIGN_CENTER_VERTICAL);
        row->Add(m_masked, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(m_plain, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(m_toggle, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
        row->Add(m_copy, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
        row->Add(m_regen, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(kRowRight) / 2);
        outer->Add(row, 0, wxEXPAND);
        m_note = body_label(this,
                            _L("Send it as Authorization: Bearer <token>. It is kept in the application settings and is never written to a log. "
                               "Regenerating it stops the old one at once."),
                            MD3::Role::OnSurfaceVariant, ::Label::Body_12);
        outer->Add(m_note, 0, wxLEFT | wxTOP, FromDIP(kRowLeft));
        m_outcome = body_label(this, wxEmptyString, MD3::Role::OnSurfaceVariant, ::Label::Body_12);
        m_outcome->Hide();
        outer->Add(m_outcome, 0, wxLEFT | wxTOP, FromDIP(kRowLeft));
        SetSizer(outer);

        load();

        m_toggle->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { toggle(); });
        m_copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { copy(); });
        m_regen->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { regenerate(); });
    }

private:
    TextInput *shown() const { return m_showing ? m_plain : m_masked; }

    void say(const wxString &text)
    {
        m_outcome->SetLabel(text);
        m_outcome->Wrap(FromDIP(kWrap));
        m_outcome->Show(!text.empty());
        Layout();
        if (wxWindow *top = GetParent()) top->Layout();
    }

    void load()
    {
        AppConfig  *cfg   = wxGetApp().app_config;
        std::string token = cfg ? cfg->get(kTokenConfigKey) : std::string();
        // Before the first switch-on there is no token yet; the Copy and Show buttons make one on demand.
        m_masked->GetTextCtrl()->ChangeValue(wxString::FromUTF8(token));
        m_plain->GetTextCtrl()->ChangeValue(m_showing ? wxString::FromUTF8(token) : wxString());
        token.assign(token.size(), '\0');
    }

    void toggle()
    {
        const std::string token = ensure_token();
        m_showing               = !m_showing;
        m_masked->GetTextCtrl()->ChangeValue(m_showing ? wxString() : wxString::FromUTF8(token));
        m_plain->GetTextCtrl()->ChangeValue(m_showing ? wxString::FromUTF8(token) : wxString());
        m_masked->Show(!m_showing);
        m_plain->Show(m_showing);
        m_toggle->SetGlyph(m_showing ? MaterialIcon::VisibilityOff : MaterialIcon::Visibility);
        const wxString label = m_showing ? _L("Hide token") : _L("Show token");
        m_toggle->SetName(label);
        m_toggle->SetToolTip(label);
        Layout();
    }

    void copy()
    {
        const std::string token  = ensure_token();
        bool              copied = false;
        if (!token.empty() && wxTheClipboard->Open()) {
            copied = wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(token)));
            wxTheClipboard->Close();
        }
        load();
        say(copied ? _L("Token copied to the clipboard.") : _L("The clipboard is not available. Show the token and copy it."));
    }

    void regenerate()
    {
        const std::string token = regenerate_token();
        load();
        say(token.empty() ? _L("A new token could not be made.") : _L("A new token was made. The old one no longer works."));
    }

    TextInput *m_masked  = nullptr;
    TextInput *m_plain   = nullptr;
    Button    *m_toggle  = nullptr;
    Button    *m_copy    = nullptr;
    Button    *m_regen   = nullptr;
    Label     *m_note    = nullptr;
    Label     *m_outcome = nullptr;
    bool       m_showing = false;
};

} // namespace

wxWindow *create_intro_row(wxWindow *parent)
{
    auto *panel = new wxPanel(parent, wxID_ANY);
    panel->SetBackgroundColour(parent->GetBackgroundColour());
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(body_label(panel,
                          _L("Other programs on this computer can read the door state and the printer list over a local, read-only web "
                             "interface. It listens on 127.0.0.1 only, needs the access token, and answers GET requests only."),
                          MD3::Role::OnSurfaceVariant, ::Label::Body_12),
               0, wxLEFT | wxRIGHT, panel->FromDIP(kRowLeft));
    panel->SetSizer(sizer);
    return panel;
}

wxWindow *create_status_row(wxWindow *parent) { return new StatusRow(parent); }
wxWindow *create_token_row(wxWindow *parent) { return new TokenRow(parent); }

}}} // namespace Slic3r::GUI::StatusApi
