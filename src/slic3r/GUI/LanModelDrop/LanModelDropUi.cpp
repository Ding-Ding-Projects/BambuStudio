#include "LanModelDropUi.hpp"
#include "LanModelDropStation.hpp"

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../format.hpp"
#include "../FeatureServices/SurfaceRegistry.hpp"
#include "../LocalSecurity/PairingQr.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3Dialog.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/TextInput.hpp"
#include "libslic3r/AppConfig.hpp"

#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace LanModelDrop {

namespace {

constexpr int kRowLeft  = 24; // Preferences ITEM_LEFT_PADDING
constexpr int kRowRight = 16; // Preferences ITEM_RIGHT_PADDING
constexpr int kWrap     = 520;

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

Label *body_label(wxWindow *parent, const wxString &text, MD3::Role colour = MD3::Role::OnSurface, const wxFont &font = ::Label::Body_13)
{
    auto *label = new Label(parent, text);
    label->SetFont(font);
    label->SetForegroundColour(role(colour));
    label->Wrap(parent->FromDIP(kWrap));
    return label;
}

// Returns true when the text changed.
bool set_wrapped(Label *label, const wxString &text)
{
    if (label == nullptr || label->GetUnwrappedLabel() == text) return false;
    label->SetLabel(text);
    label->Wrap(label->GetParent()->FromDIP(kWrap));
    return true;
}

bool show(wxWindow *window, bool shown)
{
    if (window == nullptr || window->IsShown() == shown) return false;
    window->Show(shown);
    return true;
}

// Lays out `self` and its ancestors up to the top-level window, and lets a scrolled page recompute
// its virtual size. A dialog that hosts the row is fitted to its new content.
void relayout(wxWindow *self, bool fit_top_level)
{
    for (wxWindow *w = self; w != nullptr; w = w->GetParent()) {
        w->Layout();
        if (auto *scrolled = dynamic_cast<wxScrolledWindow *>(w)) scrolled->FitInside();
        if (w->IsTopLevel()) {
            if (fit_top_level) w->Fit();
            break;
        }
    }
}

Button *kit_button(wxWindow *parent, const wxString &text, Button::Variant variant)
{
    auto *button = new Button(parent, text);
    button->SetVariant(variant);
    button->SetButtonSize(Button::Size::Small);
    return button;
}

// A row panel on the Preferences page surface that follows the station view while it lives.
class ViewRow : public wxPanel
{
public:
    explicit ViewRow(wxWindow *parent) : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(parent->GetBackgroundColour());
        m_listener = add_listener([this] { update_view(); });
    }
    ~ViewRow() override { remove_listener(m_listener); }

protected:
    virtual void update_view() = 0;

private:
    int m_listener = 0;
};

// The large QR code, for a phone held at a distance.
class LargerQrDialog : public MD3Dialog
{
public:
    LargerQrDialog(wxWindow *parent, const std::string &link)
        : MD3Dialog(parent, _L("Invite link QR code"), wxString::FromUTF8(link), MaterialIcon::Glyph::OpenInFull)
    {
        auto *qr = new QrView(this, 360);
        qr->SetText(link);
        GetContentSizer()->Add(qr, 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, FromDIP(8));
        auto *close = AddFooterButton(kit_button(this, _L("Close"), Button::Variant::Filled));
        close->SetId(wxID_CANCEL);
        close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
        SetEscapeId(wxID_CANCEL);
        Fit();
        CentreOnParent();
    }
    void on_dpi_changed(const wxRect &) override { Fit(); }
};

class InviteDialog : public MD3Dialog
{
public:
    explicit InviteDialog(wxWindow *parent)
        : MD3Dialog(parent, _L("Invite someone to send a model"),
                    _L("They open the link on a phone or computer on your local network and send a 3D model to Bambu Studio."),
                    MaterialIcon::Glyph::Lan)
    {
        InvitePanel::Options options;
        options.show_heading  = false;
        options.offer_turn_on = true;
        GetContentSizer()->Add(new InvitePanel(this, options), 1, wxEXPAND);
        auto *settings = AddFooterButton(kit_button(this, _L("LAN model drop settings"), Button::Variant::Outlined));
        auto *close    = AddFooterButton(kit_button(this, _L("Close"), Button::Variant::Filled));
        close->SetId(wxID_CANCEL);
        settings->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            EndModal(wxID_OK);
            // Preferences opens after this dialog is gone.
            wxGetApp().CallAfter([] { open_preferences_section(); });
        });
        close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
        SetEscapeId(wxID_CANCEL);
        SetMinSize(wxSize(FromDIP(560), -1));
        Fit();
        CentreOnParent();
    }
    void on_dpi_changed(const wxRect &) override { Fit(); }
};

} // namespace

// ---------------------------------------------------------------------------------------------
// QrView
// ---------------------------------------------------------------------------------------------

QrView::QrView(wxWindow *parent, int edge_dip) : wxPanel(parent, wxID_ANY)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    const wxSize edge(FromDIP(edge_dip), FromDIP(edge_dip));
    SetMinSize(edge);
    SetMaxSize(edge);
    SetName(_L("QR code of the invite link"));
    Bind(wxEVT_PAINT, [this](wxPaintEvent &) { render(); });
    Hide();
}

bool QrView::SetText(const std::string &text)
{
    if (text == m_text && (m_modules > 0 || text.empty())) return m_modules > 0;
    m_text = text;
    m_modules = 0;
    m_dark.clear();
    if (!text.empty()) {
        try {
            const auto qr = LocalSecurityUI::PairingQr::encode_text(text);
            m_modules = qr.modules;
            m_dark.assign(qr.dark.begin(), qr.dark.end());
        } catch (...) {
            m_modules = 0;
            m_dark.clear();
        }
    }
    Show(m_modules > 0);
    SetToolTip(m_modules > 0 ? wxString::FromUTF8(text) : wxString());
    Refresh();
    return m_modules > 0;
}

void QrView::render()
{
    wxAutoBufferedPaintDC dc(this);
    const wxSize size = GetClientSize();
    dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));
    dc.Clear();
    if (m_modules == 0 || m_dark.size() != std::size_t(m_modules) * m_modules) return;
    // Black on white in both themes, with the quiet zone the scanners need.
    const int count = static_cast<int>(m_modules + 2 * LocalSecurityUI::PairingQr::quiet_zone);
    const int scale = std::min(size.x, size.y) / count;
    if (scale < 1) return;
    const int ox = (size.x - count * scale) / 2;
    const int oy = (size.y - count * scale) / 2;
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(*wxWHITE_BRUSH);
    dc.DrawRectangle(ox, oy, count * scale, count * scale);
    dc.SetBrush(*wxBLACK_BRUSH);
    const int qx = ox + static_cast<int>(LocalSecurityUI::PairingQr::quiet_zone) * scale;
    const int qy = oy + static_cast<int>(LocalSecurityUI::PairingQr::quiet_zone) * scale;
    for (unsigned y = 0; y < m_modules; ++y)
        for (unsigned x = 0; x < m_modules; ++x)
            if (m_dark[std::size_t(y) * m_modules + x] != 0)
                dc.DrawRectangle(qx + static_cast<int>(x) * scale, qy + static_cast<int>(y) * scale, scale, scale);
}

// ---------------------------------------------------------------------------------------------
// InvitePanel
// ---------------------------------------------------------------------------------------------

InvitePanel::InvitePanel(wxWindow *parent, const Options &options) : wxPanel(parent, wxID_ANY), m_options(options)
{
    SetBackgroundColour(parent->GetBackgroundColour());
    auto *outer = new wxBoxSizer(wxVERTICAL);
    const int gap = FromDIP(8);

    if (m_options.show_heading) {
        outer->Add(body_label(this, _L("Invite someone to send a model"), MD3::Role::OnSurface, ::Label::Head_14), 0, wxBOTTOM, FromDIP(4));
        outer->Add(body_label(this, _L("Send them the link, or let them scan the QR code. It opens the drop site with the code filled in."),
                              MD3::Role::OnSurfaceVariant, ::Label::Body_12),
                   0, wxBOTTOM, gap);
    }

    m_off_note = body_label(this, _L("LAN model drop is off. Switch it on to get a link to share."), MD3::Role::OnSurfaceVariant);
    outer->Add(m_off_note, 0, wxBOTTOM, gap);
    if (m_options.offer_turn_on) {
        m_turn_on = kit_button(this, _L("Turn on LAN model drop"), Button::Variant::Filled);
        outer->Add(m_turn_on, 0, wxBOTTOM, gap);
        m_turn_on->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            AppConfig *cfg = wxGetApp().app_config;
            if (cfg == nullptr) return;
            // The button disappears: move focus on before it does.
            m_turn_on->Navigate();
            m_focus_copy_when_ready = true;
            cfg->set_bool(kEnabledConfigKey, true);
            cfg->save();
            apply_settings();
        });
    }
    m_pending_note = body_label(this, wxEmptyString, MD3::Role::OnSurfaceVariant);
    outer->Add(m_pending_note, 0, wxBOTTOM, gap);

    // Link, actions and the QR code of exactly that link.
    m_link_area = new wxPanel(this, wxID_ANY);
    m_link_area->SetBackgroundColour(GetBackgroundColour());
    auto *link_row = new wxBoxSizer(wxHORIZONTAL);
    m_qr = new QrView(m_link_area, 168);
    m_qr->SetCursor(wxCursor(wxCURSOR_HAND));
    link_row->Add(m_qr, 0, wxRIGHT, FromDIP(16));
    auto *column = new wxBoxSizer(wxVERTICAL);
    auto *link_caption = body_label(m_link_area, _L("Invite link"), MD3::Role::OnSurfaceVariant, ::Label::Body_12);
    column->Add(link_caption, 0, wxBOTTOM, FromDIP(4));
    m_link = new TextInput(m_link_area, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition,
                           wxSize(FromDIP(320), -1), wxTE_READONLY);
    m_link->GetTextCtrl()->SetName(_L("Invite link"));
    m_link->GetTextCtrl()->SetFont(::Label::Body_13);
    column->Add(m_link, 0, wxEXPAND | wxBOTTOM, gap);
    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    m_copy     = kit_button(m_link_area, _L("Copy link"), Button::Variant::Filled);
    m_new_link = kit_button(m_link_area, _L("New link"), Button::Variant::Outlined);
    m_larger   = kit_button(m_link_area, _L("Show larger QR code"), Button::Variant::Text);
    m_copy->SetGlyph(MaterialIcon::ContentCopy);
    m_new_link->SetGlyph(MaterialIcon::Refresh);
    m_larger->SetGlyph(MaterialIcon::OpenInFull);
    actions->Add(m_copy, 0, wxRIGHT, gap);
    actions->Add(m_new_link, 0, wxRIGHT, gap);
    actions->Add(m_larger, 0);
    column->Add(actions, 0, wxBOTTOM, gap);
    m_fixed_note = body_label(m_link_area, wxEmptyString, MD3::Role::OnSurfaceVariant, ::Label::Body_12);
    column->Add(m_fixed_note, 0, wxBOTTOM, FromDIP(4));
    auto *lan_row = new wxBoxSizer(wxHORIZONTAL);
    m_lan_label = body_label(m_link_area, _L("Address of this computer"), MD3::Role::OnSurface, ::Label::Body_12);
    m_lan_choice = new ComboBox(m_link_area, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(180), -1), 0, nullptr,
                                wxCB_READONLY);
    m_lan_choice->SetName(_L("Address of this computer"));
    lan_row->Add(m_lan_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    lan_row->Add(m_lan_choice, 0, wxALIGN_CENTER_VERTICAL);
    column->Add(lan_row, 0, wxBOTTOM, FromDIP(4));
    m_source_note = body_label(m_link_area, wxEmptyString, MD3::Role::OnSurfaceVariant, ::Label::Body_12);
    column->Add(m_source_note, 0);
    link_row->Add(column, 1, wxEXPAND);
    m_link_area->SetSizer(link_row);
    outer->Add(m_link_area, 0, wxEXPAND | wxBOTTOM, gap);

    // Polite announcements (copied, renewed) for screen readers and for everyone else.
    m_live = body_label(this, wxEmptyString, MD3::Role::Primary, ::Label::Body_12);
    outer->Add(m_live, 0, wxBOTTOM, FromDIP(4));

    auto *waiting_row = new wxBoxSizer(wxHORIZONTAL);
    m_waiting_label = body_label(this, wxEmptyString, MD3::Role::OnSurface);
    m_show_waiting  = kit_button(this, _L("Show them"), Button::Variant::Text);
    waiting_row->Add(m_waiting_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    waiting_row->Add(m_show_waiting, 0, wxALIGN_CENTER_VERTICAL);
    outer->Add(waiting_row, 0);

    SetSizer(outer);

    m_copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { copy_link(); });
    m_new_link->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        m_waiting_for_code = true;
        m_code_before      = view().drop_code;
        request_new_code();
    });
    m_larger->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { show_larger(); });
    m_qr->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) { show_larger(); });
    m_lan_choice->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) {
        const auto &choices = view().invite.choices;
        const int   index   = m_lan_choice->GetSelection();
        if (index >= 0 && index < int(choices.size())) choose_invite_ipv4(choices[std::size_t(index)]);
    });
    m_show_waiting->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { show_waiting_again(); });

    m_listener = add_listener([this] { refresh(); });
    refresh_lan_addresses(); // also refreshes this panel through the listener
    refresh();
}

InvitePanel::~InvitePanel() { remove_listener(m_listener); }

void InvitePanel::announce(const wxString &text)
{
    m_live->SetLabel(text);
    m_live->SetName(text);
    m_live->Wrap(FromDIP(kWrap));
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, m_live, wxOBJID_CLIENT, wxACC_SELF);
#endif
    relayout(this, m_options.offer_turn_on);
}

void InvitePanel::copy_link()
{
    const std::string &link = view().link;
    if (link.empty()) return;
    bool copied = false;
    if (wxTheClipboard->Open()) {
        copied = wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(link)));
        wxTheClipboard->Close();
    }
    announce(copied ? _L("Link copied to the clipboard.") : _L("The clipboard is not available. Select the link and copy it."));
}

void InvitePanel::show_larger()
{
    const std::string link = view().link;
    if (link.empty()) return;
    LargerQrDialog dialog(wxGetTopLevelParent(this), link);
    dialog.ShowModal();
}

void InvitePanel::refresh()
{
    const View &v       = view();
    bool        changed = false;

    changed |= show(m_off_note, !v.enabled);
    if (m_turn_on) changed |= show(m_turn_on, !v.enabled);

    const bool has_link = v.enabled && !v.link.empty();
    wxString   pending;
    if (v.enabled && !has_link) {
        if (v.invite.source == InviteSource::None)
            pending = _L("This computer has no private local network address to put in a link. Set the drop site address to an address the others can open.");
        else if (v.state == LinkState::NeedsStationKey || v.state == LinkState::KeyStorageUnavailable ||
                 v.state == LinkState::InvalidAddress || v.state == LinkState::WrongStationKey)
            pending = format_wxstr(_L("The link appears once Bambu Studio is connected to the drop site. %1%"), status_text(v));
        else
            pending = format_wxstr(_L("Waiting for the drop site to send its code. %1%"), status_text(v));
    }
    changed |= set_wrapped(m_pending_note, pending);
    changed |= show(m_pending_note, !pending.empty());

    changed |= show(m_link_area, has_link);
    const wxString link = has_link ? wxString::FromUTF8(v.link) : wxString();
    if (m_link->GetTextCtrl()->GetValue() != link) {
        m_link->GetTextCtrl()->ChangeValue(link);
        changed = true;
    }
    if (m_qr->GetText() != (has_link ? v.link : std::string())) {
        m_qr->SetText(has_link ? v.link : std::string());
        changed = true;
    }
    m_new_link->Enable(has_link && !v.fixed_code && !v.busy_code);
    wxString fixed;
    if (v.fixed_code)
        fixed = _L("The drop site's code is fixed by DROP_CODE, so New link cannot change it. Change DROP_CODE on the drop site to stop older links.");
    else if (v.code_failed)
        fixed = _L("The drop site did not make a new code. Older links still work.");
    changed |= set_wrapped(m_fixed_note, fixed);
    changed |= show(m_fixed_note, !fixed.empty());

    const auto &choices = v.invite.choices;
    const bool  several = v.invite.source == InviteSource::LanAddress && choices.size() > 1;
    changed |= show(m_lan_label, several);
    changed |= show(m_lan_choice, several);
    if (several) {
        bool same = m_lan_choice->GetCount() == choices.size();
        for (std::size_t i = 0; same && i < choices.size(); ++i)
            same = m_lan_choice->GetString(static_cast<unsigned int>(i)) == wxString::FromUTF8(choices[i]);
        if (!same) {
            m_lan_choice->Clear();
            for (const std::string &c : choices) m_lan_choice->Append(wxString::FromUTF8(c));
            changed = true;
        }
        const auto it    = std::find(choices.begin(), choices.end(), v.invite.chosen);
        const int  index = it == choices.end() ? 0 : static_cast<int>(it - choices.begin());
        if (m_lan_choice->GetSelection() != index) m_lan_choice->SetSelection(index);
    }
    changed |= set_wrapped(m_source_note, has_link ? invite_source_text(v) : wxString());

    if (m_waiting_for_code && !v.busy_code) {
        m_waiting_for_code = false;
        if (!v.drop_code.empty() && v.drop_code != m_code_before) announce(_L("New link ready. Older links no longer work."));
    }

    changed |= set_wrapped(m_waiting_label, v.waiting > 0 ? format_wxstr(_L("%1% received files waiting for Open or Discard."), v.waiting) : wxString());
    changed |= show(m_waiting_label, v.waiting > 0);
    changed |= show(m_show_waiting, v.waiting > 0);

    if (changed) relayout(this, m_options.offer_turn_on);
    // Turned on from the dialog: the invite is the first thing shown, and focus lands on Copy link
    // as soon as the link exists.
    if (m_focus_copy_when_ready && has_link) {
        m_focus_copy_when_ready = false;
        m_copy->SetFocus();
    }
}

// ---------------------------------------------------------------------------------------------
// Preferences rows
// ---------------------------------------------------------------------------------------------

namespace {

class StatusRow : public ViewRow
{
public:
    explicit StatusRow(wxWindow *parent) : ViewRow(parent)
    {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        auto *texts = new wxBoxSizer(wxVERTICAL);
        m_status = body_label(this, wxEmptyString);
        m_status->SetName(_L("LAN model drop status"));
        m_test_result = body_label(this, wxEmptyString, MD3::Role::OnSurfaceVariant, ::Label::Body_12);
        texts->Add(m_status, 0);
        texts->Add(m_test_result, 0, wxTOP, FromDIP(2));
        m_test = kit_button(this, _L("Test connection"), Button::Variant::Outlined);
        row->AddSpacer(FromDIP(kRowLeft));
        row->Add(texts, 1, wxALIGN_CENTER_VERTICAL);
        row->Add(m_test, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(kRowRight));
        SetSizer(row);
        m_test->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { test_connection(); });
        update_view();
    }

protected:
    void update_view() override
    {
        const View &v       = view();
        bool        changed = set_wrapped(m_status, status_text(v));
        wxString    result;
        if (v.busy_test) result = _L("Testing the connection...");
        else if (v.tested) result = format_wxstr(_L("Test result: %1%"), state_text(v.test_state));
        const bool result_changed = set_wrapped(m_test_result, result);
        changed |= result_changed;
        changed |= show(m_test_result, !result.empty());
        m_test->Enable(!v.busy_test);
#if wxUSE_ACCESSIBILITY
        if (result_changed) wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, m_test_result, wxOBJID_CLIENT, wxACC_SELF);
#endif
        if (changed) relayout(this, false);
    }

private:
    Label  *m_status      = nullptr;
    Label  *m_test_result = nullptr;
    Button *m_test        = nullptr;
};

class AddressRow : public ViewRow
{
public:
    explicit AddressRow(wxWindow *parent) : ViewRow(parent)
    {
        auto *outer = new wxBoxSizer(wxVERTICAL);
        auto *row   = new wxBoxSizer(wxHORIZONTAL);
        auto *title = body_label(this, _L("Drop site address"));
        m_input = new TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(260), -1),
                                wxTE_PROCESS_ENTER);
        m_input->GetTextCtrl()->SetName(_L("Drop site address"));
        m_input->GetTextCtrl()->SetFont(::Label::Body_13);
        AppConfig  *cfg  = wxGetApp().app_config;
        std::string text = cfg ? cfg->get(kAddressConfigKey) : std::string();
        m_input->GetTextCtrl()->ChangeValue(wxString::FromUTF8(text.empty() ? std::string(kDefaultAddress) : text));
        row->AddSpacer(FromDIP(kRowLeft));
        row->Add(title, 1, wxALIGN_CENTER_VERTICAL);
        row->Add(m_input, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(kRowRight));
        outer->Add(row, 0, wxEXPAND);
        m_hint = body_label(this, _L("The address Bambu Studio uses to reach the drop site, for example http://localhost:8833 when it runs on this computer."),
                            MD3::Role::OnSurfaceVariant, ::Label::Body_12);
        outer->Add(m_hint, 0, wxLEFT | wxTOP, FromDIP(kRowLeft));
        SetSizer(outer);
        m_input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &e) {
            save();
            e.Skip();
        });
        m_input->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) {
            save();
            e.Skip();
        });
    }

protected:
    void update_view() override {}

private:
    void save()
    {
        AppConfig *cfg = wxGetApp().app_config;
        if (cfg == nullptr) return;
        const std::string text   = m_input->GetTextCtrl()->GetValue().ToUTF8().data();
        const auto        parsed = parse_base_address(text);
        if (!parsed) {
            set_wrapped(m_hint, _L("This address is not valid. Use http:// or https://, a host name or IPv4 address, and an optional port, "
                                   "for example http://localhost:8833."));
            m_hint->SetForegroundColour(role(MD3::Role::Error));
            relayout(this, false);
            return;
        }
        if (set_wrapped(m_hint, _L("The address Bambu Studio uses to reach the drop site, for example http://localhost:8833 when it runs on this computer."))) {
            m_hint->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
            relayout(this, false);
        }
        const wxString normalized = wxString::FromUTF8(parsed->base);
        if (m_input->GetTextCtrl()->GetValue() != normalized) m_input->GetTextCtrl()->ChangeValue(normalized);
        if (cfg->get(kAddressConfigKey) == parsed->base) return;
        cfg->set(kAddressConfigKey, parsed->base);
        cfg->save();
        apply_settings();
    }

    TextInput *m_input = nullptr;
    Label     *m_hint  = nullptr;
};

class KeyRow : public ViewRow
{
public:
    explicit KeyRow(wxWindow *parent) : ViewRow(parent)
    {
        auto &registry = FeatureServices::SurfaceRegistry::instance();
        registry.register_sensitive(this);
        auto *outer = new wxBoxSizer(wxVERTICAL);
        auto *row   = new wxBoxSizer(wxHORIZONTAL);
        auto *title = body_label(this, _L("Station key"));
        // wxTE_PASSWORD cannot be switched on an existing Windows edit control, so a masked and a
        // plain field share the value and only one of them is shown.
        m_masked = new TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(220), -1),
                                 wxTE_PASSWORD | wxTE_PROCESS_ENTER);
        m_plain  = new TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(220), -1),
                                 wxTE_PROCESS_ENTER);
        for (TextInput *input : {m_masked, m_plain}) {
            input->GetTextCtrl()->SetName(_L("Station key"));
            input->GetTextCtrl()->SetMaxLength(512);
            input->GetTextCtrl()->SetFont(::Label::Mono_13);
            registry.register_sensitive(input);
            registry.register_sensitive(input->GetTextCtrl());
        }
        m_plain->Hide();
        m_toggle = new Button(this, wxEmptyString);
        m_toggle->SetIconButton(Button::IconShape::Circle);
        m_toggle->SetGlyph(MaterialIcon::Visibility);
        m_toggle->SetName(_L("Show station key"));
        m_toggle->SetToolTip(_L("Show station key"));
        m_save = kit_button(this, _L("Save key"), Button::Variant::Outlined);
        row->AddSpacer(FromDIP(kRowLeft));
        row->Add(title, 1, wxALIGN_CENTER_VERTICAL);
        row->Add(m_masked, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(m_plain, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(m_toggle, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
        row->Add(m_save, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(kRowRight) / 2);
        row->AddSpacer(FromDIP(kRowRight) / 2);
        outer->Add(row, 0, wxEXPAND);
        m_note = body_label(this, wxEmptyString, MD3::Role::OnSurfaceVariant, ::Label::Body_12);
        outer->Add(m_note, 0, wxLEFT | wxTOP, FromDIP(kRowLeft));
        // The command is the drop site's own; it is not translated.
        outer->Add(body_label(this,
                              format_wxstr(_L("Read the key on the computer that runs the drop site with: %1%"),
                                           "docker compose exec lan-model-drop node server/station-key.mjs"),
                              MD3::Role::OnSurfaceVariant, ::Label::Body_12),
                   0, wxLEFT | wxTOP, FromDIP(kRowLeft));
        SetSizer(outer);

        std::string stored = stored_station_key();
        m_masked->GetTextCtrl()->ChangeValue(wxString::FromUTF8(stored));
        wipe(stored);

        m_toggle->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { toggle(); });
        m_save->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { save(); });
        for (TextInput *input : {m_masked, m_plain})
            input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { save(); });
        update_view();
    }

protected:
    void update_view() override
    {
        const View &v = view();
        wxString note;
        if (v.state == LinkState::KeyStorageUnavailable)
            note = _L("Protected key storage is not available on this system, so the key cannot be saved.");
        else if (v.has_key)
            note = _L("Saved for this Windows account, protected with Windows data protection. It is never shown in logs or exports.");
        else
            note = _L("No station key is saved.");
        if (!m_message.empty()) note = m_message + "\n" + note;
        if (set_wrapped(m_note, note)) relayout(this, false);
    }

private:
    TextInput *shown() const { return m_showing ? m_plain : m_masked; }

    void toggle()
    {
        TextInput *from = shown();
        m_showing       = !m_showing;
        TextInput *to   = shown();
        wxString value  = from->GetTextCtrl()->GetValue();
        to->GetTextCtrl()->ChangeValue(value);
        from->GetTextCtrl()->ChangeValue(wxEmptyString);
        value.Clear();
        from->Hide();
        to->Show();
        m_toggle->SetGlyph(m_showing ? MaterialIcon::VisibilityOff : MaterialIcon::Visibility);
        const wxString label = m_showing ? _L("Hide station key") : _L("Show station key");
        m_toggle->SetName(label);
        m_toggle->SetToolTip(label);
        relayout(this, false);
        to->GetTextCtrl()->SetFocus();
    }

    void save()
    {
        std::string key = shown()->GetTextCtrl()->GetValue().ToUTF8().data();
        // Trim surrounding spaces a copy and paste may bring along.
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t' || key.back() == '\r' || key.back() == '\n')) key.pop_back();
        std::size_t lead = 0;
        while (lead < key.size() && (key[lead] == ' ' || key[lead] == '\t')) ++lead;
        key.erase(0, lead);
        if (!key.empty() && !is_station_key(key)) {
            wipe(key);
            m_message = _L("The station key must be 16 to 512 visible characters without spaces. Nothing was saved.");
            update_view();
            return;
        }
        const KeyStoreResult result = store_station_key(key);
        wipe(key);
        if (result == KeyStoreResult::Ok) m_message = _L("Station key saved.");
        else if (result == KeyStoreResult::Missing) m_message = _L("Station key removed.");
        else if (result == KeyStoreResult::Unavailable) m_message.clear();
        else m_message = _L("The station key could not be saved.");
        update_view();
    }

    TextInput *m_masked  = nullptr;
    TextInput *m_plain   = nullptr;
    Button    *m_toggle  = nullptr;
    Button    *m_save    = nullptr;
    Label     *m_note    = nullptr;
    bool       m_showing = false;
    wxString   m_message;
};

class CodeRow : public ViewRow
{
public:
    explicit CodeRow(wxWindow *parent) : ViewRow(parent)
    {
        auto *row   = new wxBoxSizer(wxHORIZONTAL);
        auto *title = body_label(this, _L("Drop code"));
        m_code      = body_label(this, wxEmptyString, MD3::Role::OnSurface, ::Label::Mono_14);
        m_code->SetName(_L("Drop code"));
        m_new = kit_button(this, _L("New code"), Button::Variant::Outlined);
        row->AddSpacer(FromDIP(kRowLeft));
        row->Add(title, 1, wxALIGN_CENTER_VERTICAL);
        row->Add(m_code, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
        row->Add(m_new, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(kRowRight));
        SetSizer(row);
        m_new->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { request_new_code(); });
        update_view();
    }

protected:
    void update_view() override
    {
        const View &v = view();
        const bool  changed = set_wrapped(m_code, v.drop_code.empty() ? _L("Not known yet") : wxString::FromUTF8(v.drop_code));
        m_new->Enable(v.enabled && !v.drop_code.empty() && !v.fixed_code && !v.busy_code);
        m_new->SetToolTip(v.fixed_code ? _L("The code is fixed by DROP_CODE on the drop site.") : _L("Make a new code; links with the old one stop working."));
        if (changed) relayout(this, false);
    }

private:
    Label  *m_code = nullptr;
    Button *m_new  = nullptr;
};

} // namespace

wxWindow *create_intro_row(wxWindow *parent)
{
    auto *panel = new wxPanel(parent, wxID_ANY);
    panel->SetBackgroundColour(parent->GetBackgroundColour());
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(body_label(panel,
                          _L("People on your local network can send 3D models to this computer through the LAN drop site that runs in "
                             "Docker. Each model waits for you: nothing is opened, sliced or printed until you choose Open."),
                          MD3::Role::OnSurfaceVariant, ::Label::Body_12),
               0, wxLEFT | wxRIGHT, panel->FromDIP(kRowLeft));
    panel->SetSizer(sizer);
    return panel;
}

wxWindow *create_status_row(wxWindow *parent) { return new StatusRow(parent); }
wxWindow *create_address_row(wxWindow *parent) { return new AddressRow(parent); }
wxWindow *create_key_row(wxWindow *parent) { return new KeyRow(parent); }
wxWindow *create_code_row(wxWindow *parent) { return new CodeRow(parent); }

void open_invite_dialog(wxWindow *parent)
{
    if (parent == nullptr) parent = wxGetApp().mainframe;
    InviteDialog dialog(parent);
    dialog.ShowModal();
}

}}} // namespace Slic3r::GUI::LanModelDrop
