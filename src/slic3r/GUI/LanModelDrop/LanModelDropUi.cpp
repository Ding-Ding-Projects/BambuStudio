#include "LanModelDropUi.hpp"
#include "LanModelDropStation.hpp"
#include "LocalHost.hpp"

#include "../BilingualDecorator.hpp"
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
#include "libslic3r/Utils.hpp"

#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#if wxUSE_ACCESSIBILITY
#include <wx/access.h>
#endif

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace LanModelDrop {

namespace {

constexpr int kRowLeft  = 24; // Preferences ITEM_LEFT_PADDING
constexpr int kRowRight = 16; // Preferences ITEM_RIGHT_PADDING
constexpr int kWrap     = 520;
// GUI-thread revision: automatic pairing invalidates untouched key editor text.
std::uint64_t local_pairing_revision = 0;

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

Label *body_label(wxWindow *parent, const wxString &text, MD3::Role colour = MD3::Role::OnSurface, const wxFont &font = ::Label::Body_13)
{
    auto *label = new Label(parent, text);
    label->SetFont(font);
    label->SetForegroundColour(role(colour));
    label->Wrap(parent->FromDIP(kWrap));
    return label;
}

bool show(wxWindow *window, bool shown)
{
    if (window == nullptr || window->IsShown() == shown) return false;
    window->Show(shown);
    return true;
}

// True while `window` or one of its ancestors is being destroyed: a focus change during the
// teardown of Preferences must not save or lay anything out.
bool tearing_down(const wxWindow *window)
{
    for (const wxWindow *w = window; w != nullptr; w = w->GetParent())
        if (w->IsBeingDeleted()) return true;
    return false;
}

// Lays out `self` and its ancestors up to the top-level window, and lets a scrolled page recompute
// its virtual size. A dialog that hosts the row is fitted to its new content.
void relayout(wxWindow *self, bool fit_top_level)
{
    if (tearing_down(self)) return;
    for (wxWindow *w = self; w != nullptr; w = w->GetParent()) {
        w->InvalidateBestSize();
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
// LiveText
// ---------------------------------------------------------------------------------------------

// Read once, at creation, before the decorator has seen the label.
LiveText::LiveText(Label *label) : m_label(label)
{
    if (m_label != nullptr) m_english = m_label->GetUnwrappedLabel();
}

bool LiveText::set(const wxString &english)
{
    if (m_label == nullptr || english == m_english) return false;
    m_english = english;
    m_label->SetLabel(english);
    m_label->Wrap(m_label->GetParent()->FromDIP(kWrap));
    I18N::refresh_bilingual_decoration(m_label);
    return true;
}

// ---------------------------------------------------------------------------------------------
// QrView
// ---------------------------------------------------------------------------------------------

QrView::QrView(wxWindow *parent, int edge_dip) : wxPanel(parent, wxID_ANY)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    const wxSize edge(FromDIP(edge_dip), FromDIP(edge_dip));
    SetMinSize(edge);
    SetMaxSize(edge);
    // The accessible name: the window text on Windows, and the name the layout probe reports.
    SetLabel(_L("QR code of the invite link"));
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

InvitePanel::InvitePanel(wxWindow *parent, const Options &options) : wxPanel(parent, wxID_ANY), m_options(options), m_local_timer(this)
{
    SetBackgroundColour(parent->GetBackgroundColour());
    auto *outer = new wxBoxSizer(wxVERTICAL);
    const int gap = FromDIP(8);

    // Hosting is available before reception is enabled; turning on polling alone
    // cannot start the separate local service.
    auto *host_actions = new wxWrapSizer(wxHORIZONTAL);
    m_start_local = kit_button(this, _L("Start local drop server"), Button::Variant::Filled);
    m_open_page = kit_button(this, _L("Open drop page"), Button::Variant::Outlined);
    host_actions->Add(m_start_local, 0, wxRIGHT | wxBOTTOM, gap);
    host_actions->Add(m_open_page, 0, wxBOTTOM, gap);
    outer->Add(host_actions, 0, wxEXPAND);
    m_local_status = LiveText(body_label(this, _L("Start the local server to set up reception on this computer. Docker must be installed and running."),
        MD3::Role::OnSurfaceVariant, ::Label::Body_12));
    outer->Add(m_local_status.get(), 0, wxEXPAND | wxBOTTOM, gap);
    m_start_local->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { start_local(); });
    m_open_page->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        const auto &v = view();
        const std::string url = v.state == LinkState::Connected ? v.link : m_local_lan_url;
        if (!url.empty()) wxGetApp().open_browser_with_warning_dialog(wxString::FromUTF8(url));
    });
    Bind(wxEVT_TIMER, &InvitePanel::on_local_timer, this, m_local_timer.GetId());

    if (m_options.show_heading) {
        outer->Add(body_label(this, _L("Invite someone to send a model"), MD3::Role::OnSurface, ::Label::Head_14), 0, wxBOTTOM, FromDIP(4));
        outer->Add(body_label(this, _L("Send them the link, or let them scan the QR code. It opens the drop page, ready to choose and send models."),
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
    m_pending_note = LiveText(body_label(this, wxEmptyString, MD3::Role::OnSurfaceVariant));
    outer->Add(m_pending_note.get(), 0, wxBOTTOM, gap);

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
    m_larger   = kit_button(m_link_area, _L("Show larger QR code"), Button::Variant::Text);
    m_copy->SetGlyph(MaterialIcon::ContentCopy);
    m_larger->SetGlyph(MaterialIcon::OpenInFull);
    actions->Add(m_copy, 0, wxRIGHT, gap);
    actions->Add(m_larger, 0);
    column->Add(actions, 0, wxBOTTOM, gap);
    auto *lan_row = new wxBoxSizer(wxHORIZONTAL);
    m_lan_label = body_label(m_link_area, _L("Address of this computer"), MD3::Role::OnSurface, ::Label::Body_12);
    m_lan_choice = new ComboBox(m_link_area, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(180), -1), 0, nullptr,
                                wxCB_READONLY);
    m_lan_choice->SetName(_L("Address of this computer"));
    lan_row->Add(m_lan_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    lan_row->Add(m_lan_choice, 0, wxALIGN_CENTER_VERTICAL);
    column->Add(lan_row, 0, wxBOTTOM, FromDIP(4));
    m_manual = LiveText(body_label(m_link_area, wxEmptyString, MD3::Role::OnSurface, ::Label::Body_12));
    column->Add(m_manual.get(), 0, wxBOTTOM, FromDIP(4));
    m_source_note = LiveText(body_label(m_link_area, wxEmptyString, MD3::Role::OnSurfaceVariant, ::Label::Body_12));
    column->Add(m_source_note.get(), 0);
    link_row->Add(column, 1, wxEXPAND);
    m_link_area->SetSizer(link_row);
    outer->Add(m_link_area, 0, wxEXPAND | wxBOTTOM, gap);

    // Polite announcements (link copied) for screen readers and for everyone else.
    m_live = LiveText(body_label(this, wxEmptyString, MD3::Role::Primary, ::Label::Body_12));
    outer->Add(m_live.get(), 0, wxBOTTOM, FromDIP(4));

    auto *waiting_row = new wxBoxSizer(wxHORIZONTAL);
    m_waiting_label = LiveText(body_label(this, wxEmptyString, MD3::Role::OnSurface));
    m_show_waiting  = kit_button(this, _L("Show them"), Button::Variant::Text);
    waiting_row->Add(m_waiting_label.get(), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, gap);
    waiting_row->Add(m_show_waiting, 0, wxALIGN_CENTER_VERTICAL);
    outer->Add(waiting_row, 0);

    SetSizer(outer);

    m_copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { copy_link(); });
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

InvitePanel::~InvitePanel()
{
    m_local_timer.Stop();
    Unbind(wxEVT_TIMER, &InvitePanel::on_local_timer, this, m_local_timer.GetId());
    m_local_host.reset();
    wipe(m_local_key_snapshot);
    remove_listener(m_listener);
}

bool InvitePanel::may_pair_local() const
{
    const auto *cfg = wxGetApp().app_config;
    if (!cfg || !key_store_available()) return false;
    const auto configured = cfg->get(kAddressConfigKey);
    const auto address = parse_base_address(configured.empty() ? kDefaultAddress : configured);
    // An existing remote station is never silently replaced by local setup.
    return address && (address->host == "localhost" || address->host == "127.0.0.1" || address->host == "[::1]");
}

void InvitePanel::start_local()
{
    if (!may_pair_local()) {
        m_local_status.set(_L("An existing remote station is configured. Its address and protected key have not been changed."));
        relayout(this, false);
        return;
    }
    if (m_local_host && m_local_host->running()) return;
    auto *cfg = wxGetApp().app_config;
    m_local_address_snapshot = cfg->get(kAddressConfigKey);
    m_local_enabled_snapshot = cfg->get(kEnabledConfigKey);
    wipe(m_local_key_snapshot);
    m_local_key_snapshot = stored_station_key();
    m_local_host = std::make_unique<LocalHost>();
    if (!m_local_host->start()) {
        wipe(m_local_key_snapshot);
        m_local_status.set(_L("Local server setup is already running. Wait for it to finish before trying again."));
    } else {
        m_local_status.set(_L("Starting the local drop server and checking its network address. This can take several minutes."));
        m_start_local->Enable(false);
        m_local_timer.Start(200);
    }
    relayout(this, false);
}

void InvitePanel::on_local_timer(wxTimerEvent &)
{
    if (!m_local_host) return;
    auto result = m_local_host->take_result();
    if (!result) return;
    m_local_timer.Stop();
    auto *cfg = wxGetApp().app_config;
    std::string current_key = stored_station_key();
    const bool unchanged = cfg && may_pair_local() && cfg->get(kAddressConfigKey) == m_local_address_snapshot &&
        cfg->get(kEnabledConfigKey) == m_local_enabled_snapshot && current_key == m_local_key_snapshot;
    wipe(current_key);
    wxString outcome;
    if (!result->ok) {
        if (result->code == "payload_missing") outcome = _L("The local hosting files are missing. Repair or update this installation and try again.");
        else if (result->code == "timeout") outcome = _L("Local server setup timed out. Check Docker and the network connection, then try again.");
        else outcome = _L("The local server could not be started. Check that Docker is installed and running, then try again.");
    } else if (!unchanged) {
        outcome = _L("The server started, but station settings changed during setup. Your current station was preserved.");
    } else {
        const auto key_file = station_key_file(std::filesystem::u8path(data_dir()));
        if (save_station_key(key_file, result->station_key) != KeyStoreResult::Ok) {
            outcome = _L("The server started, but its protected pairing could not be saved. Station settings were not changed.");
        } else {
            try {
                cfg->set(kAddressConfigKey, result->address);
                cfg->set_bool(kEnabledConfigKey, true);
                cfg->save();
                if (!persisted_station_matches(std::filesystem::u8path(cfg->config_path()), result->address, "true"))
                    throw std::runtime_error("Pairing persistence was not verified");
                m_local_lan_url = result->lan_url;
                m_local_pair_address = result->address;
                ++local_pairing_revision;
                apply_settings();
                outcome = _L("Local server ready. Open the drop page, or share the invite link when the connection is ready.");
            } catch (...) {
                // Do not poll with a new key against an old address. Restore both
                // halves before resuming the original station.
                cfg->set(kAddressConfigKey, m_local_address_snapshot);
                cfg->set(kEnabledConfigKey, m_local_enabled_snapshot);
                const auto rollback = m_local_key_snapshot.empty() ? forget_station_key(key_file) : save_station_key(key_file, m_local_key_snapshot);
                try { cfg->save(); } catch (...) {}
                const bool restored = (rollback == KeyStoreResult::Ok || rollback == KeyStoreResult::Missing) &&
                    persisted_station_matches(std::filesystem::u8path(cfg->config_path()), m_local_address_snapshot, m_local_enabled_snapshot);
                if (!restored) cfg->set_bool(kEnabledConfigKey, false);
                apply_settings();
                outcome = restored
                    ? _L("The server started, but pairing settings could not be saved. The previous station settings were restored.")
                    : _L("Pairing settings could not be saved or fully restored. Reception is paused for this session; review settings before restarting.");
            }
        }
    }
    wipe(m_local_key_snapshot);
    m_local_status.set(outcome);
    announce(outcome);
    refresh();
    relayout(this, false);
}

void InvitePanel::announce(const wxString &text)
{
    const bool changed = m_live.set(text);
    m_live->SetName(text);
    // Announced again even when the words are the same (a second Copy link).
#if wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, m_live.get(), wxOBJID_CLIENT, wxACC_SELF);
#endif
    if (changed) relayout(this, m_options.offer_turn_on);
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

    if (!m_local_lan_url.empty() && (!wxGetApp().app_config || wxGetApp().app_config->get(kAddressConfigKey) != m_local_pair_address))
        m_local_lan_url.clear();
    if (!m_local_host && !may_pair_local())
        changed |= m_local_status.set(key_store_available()
            ? _L("An existing remote station is configured. Its address and protected key have not been changed.")
            : _L("Protected key storage is unavailable, so automatic local pairing cannot be used."));
    m_start_local->Enable(may_pair_local() && !(m_local_host && m_local_host->running()));
    m_open_page->Enable((v.state == LinkState::Connected && !v.link.empty()) || !m_local_lan_url.empty());

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
    changed |= m_pending_note.set(pending);
    changed |= show(m_pending_note.get(), !pending.empty());

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
    // The same direct address is available without scanning.
    changed |= m_manual.set(has_link ? format_wxstr(_L("Or open %1% to choose and send models directly."), v.invite.base) : wxString());
    changed |= m_source_note.set(has_link ? invite_source_text(v) : wxString());

    changed |= m_waiting_label.set(v.waiting > 0 ? format_wxstr(_L("%1% received files waiting for Open or Discard."), v.waiting) : wxString());
    changed |= show(m_waiting_label.get(), v.waiting > 0);
    changed |= show(m_show_waiting, v.waiting > 0);

    if (changed) {
        // The QR code, the picker and the notes live in the inner panel: size it afresh first.
        m_link_area->InvalidateBestSize();
        m_link_area->Layout();
        relayout(this, m_options.offer_turn_on);
    }
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
        m_status = LiveText(body_label(this, wxEmptyString));
        m_status->SetName(_L("LAN model drop status"));
        m_test_result = LiveText(body_label(this, wxEmptyString, MD3::Role::OnSurfaceVariant, ::Label::Body_12));
        texts->Add(m_status.get(), 0);
        texts->Add(m_test_result.get(), 0, wxTOP, FromDIP(2));
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
        bool        changed = m_status.set(status_text(v));
        wxString    result;
        if (v.busy_test) result = _L("Testing the connection...");
        else if (v.tested) result = format_wxstr(_L("Test result: %1%"), state_text(v.test_state));
        const bool result_changed = m_test_result.set(result);
        changed |= result_changed;
        changed |= show(m_test_result.get(), !result.empty());
        m_test->Enable(!v.busy_test);
#if wxUSE_ACCESSIBILITY
        if (result_changed) wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_NAMECHANGE, m_test_result.get(), wxOBJID_CLIENT, wxACC_SELF);
#endif
        if (changed) relayout(this, false);
    }

private:
    LiveText m_status;
    LiveText m_test_result;
    Button  *m_test = nullptr;
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
        m_loaded_address = text;
        m_input->GetTextCtrl()->ChangeValue(wxString::FromUTF8(text.empty() ? std::string(kDefaultAddress) : text));
        row->AddSpacer(FromDIP(kRowLeft));
        row->Add(title, 1, wxALIGN_CENTER_VERTICAL);
        row->Add(m_input, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(kRowRight));
        outer->Add(row, 0, wxEXPAND);
        m_hint = LiveText(body_label(this, address_hint(), MD3::Role::OnSurfaceVariant, ::Label::Body_12));
        outer->Add(m_hint.get(), 0, wxLEFT | wxTOP, FromDIP(kRowLeft));
        SetSizer(outer);
        m_input->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &e) { m_dirty = true; e.Skip(); });
        m_input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &e) {
            save(true);
            e.Skip();
        });
        m_input->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) {
            save();
            e.Skip();
        });
    }

protected:
    void update_view() override
    {
        if (!m_input || m_dirty) return;
        const auto *cfg = wxGetApp().app_config;
        if (!cfg) return;
        m_loaded_address = cfg->get(kAddressConfigKey);
        const auto value = wxString::FromUTF8(m_loaded_address.empty() ? kDefaultAddress : m_loaded_address);
        if (m_input->GetTextCtrl()->GetValue() != value) m_input->GetTextCtrl()->ChangeValue(value);
    }

private:
    void save(bool explicit_request = false)
    {
        AppConfig *cfg = wxGetApp().app_config;
        if (cfg == nullptr || tearing_down(this)) return;
        if (!m_dirty) { update_view(); return; }
        if (!explicit_request && cfg->get(kAddressConfigKey) != m_loaded_address) {
            m_hint.set(_L("The saved station changed while you were editing. Review this address and press Enter to replace it."));
            relayout(this, false);
            return;
        }
        const std::string text   = m_input->GetTextCtrl()->GetValue().ToUTF8().data();
        const auto        parsed = parse_base_address(text);
        if (!parsed) {
            if (m_hint.set(_L("This address is not valid. Use http:// or https://, a host name or IPv4 address, and an optional port, "
                              "for example http://localhost:8833."))) {
                m_hint->SetForegroundColour(role(MD3::Role::Error));
                relayout(this, false);
            }
            return;
        }
        if (m_hint.set(address_hint())) {
            m_hint->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
            relayout(this, false);
        }
        const wxString normalized = wxString::FromUTF8(parsed->base);
        if (m_input->GetTextCtrl()->GetValue() != normalized) m_input->GetTextCtrl()->ChangeValue(normalized);
        m_loaded_address = parsed->base;
        m_dirty = false;
        if (cfg->get(kAddressConfigKey) == parsed->base) return;
        cfg->set(kAddressConfigKey, parsed->base);
        cfg->save();
        apply_settings();
    }

    static wxString address_hint()
    {
        return _L("The address Bambu Studio uses to reach the drop site, for example http://localhost:8833 when it runs on this computer.");
    }

    TextInput *m_input = nullptr;
    LiveText   m_hint;
    std::string m_loaded_address;
    bool m_dirty = false;
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
        m_save->Enable(false);
        row->AddSpacer(FromDIP(kRowLeft));
        row->Add(title, 1, wxALIGN_CENTER_VERTICAL);
        row->Add(m_masked, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(m_plain, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(m_toggle, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
        row->Add(m_save, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(kRowRight) / 2);
        row->AddSpacer(FromDIP(kRowRight) / 2);
        outer->Add(row, 0, wxEXPAND);
        // The outcome of Save key and the note under it are separate labels: each holds one catalogue
        // message, so bilingual mode can show the Cantonese of both.
        m_outcome = LiveText(body_label(this, wxEmptyString, MD3::Role::OnSurfaceVariant, ::Label::Body_12));
        outer->Add(m_outcome.get(), 0, wxLEFT | wxTOP, FromDIP(kRowLeft));
        m_note = LiveText(body_label(this, wxEmptyString, MD3::Role::OnSurfaceVariant, ::Label::Body_12));
        outer->Add(m_note.get(), 0, wxLEFT | wxTOP, FromDIP(kRowLeft));
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
        for (TextInput *input : {m_masked, m_plain})
            input->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &event) {
                m_dirty = true;
                m_save->Enable(true);
                event.Skip();
            });
        m_pairing_revision = local_pairing_revision;
        update_view();
    }

protected:
    void update_view() override
    {
        if (m_pairing_revision != local_pairing_revision) {
            m_pairing_revision = local_pairing_revision;
            if (!m_dirty) {
                m_masked->GetTextCtrl()->ChangeValue(wxEmptyString);
                m_plain->GetTextCtrl()->ChangeValue(wxEmptyString);
                m_save->Enable(false);
                m_message = _L("Local pairing updated. The previous key text was cleared.");
            } else {
                m_draft_conflict = true;
                m_message = _L("Local pairing changed while you were editing. Your key draft was preserved; review it before saving.");
            }
        }
        const View &v = view();
        wxString note;
        if (v.state == LinkState::KeyStorageUnavailable)
            note = _L("Protected key storage is not available on this system, so the key cannot be saved.");
        else if (v.has_key)
            note = _L("Saved for this Windows account, protected with Windows data protection. It is never shown in logs or exports.");
        else
            note = _L("No station key is saved.");
        bool changed = m_outcome.set(m_message);
        changed |= show(m_outcome.get(), !m_message.empty());
        changed |= m_note.set(note);
        if (changed) relayout(this, false);
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
        if (tearing_down(this) || !m_dirty) return;
        if (m_draft_conflict) {
            m_draft_conflict = false;
            m_message = _L("Saving this draft will replace the new local pairing. Press Save key again only if that is intended.");
            update_view();
            return;
        }
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
        if (result == KeyStoreResult::Ok || result == KeyStoreResult::Missing) {
            m_dirty = false;
            m_save->Enable(false);
        }
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
    LiveText   m_outcome; // what the last Save key did
    LiveText   m_note;
    bool       m_showing = false;
    wxString   m_message;
    std::uint64_t m_pairing_revision = 0;
    bool m_dirty = false;
    bool m_draft_conflict = false;
};

} // namespace

wxWindow *create_intro_row(wxWindow *parent)
{
    auto *panel = new wxPanel(parent, wxID_ANY);
    panel->SetBackgroundColour(parent->GetBackgroundColour());
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(body_label(panel,
                          _L("People on your local network can send 3D models to this computer through the LAN drop site that runs in "
                             "Docker. Use Start local drop server below to set it up on this computer, or keep an existing remote station. "
                             "Each model waits for you: nothing is opened, sliced or printed until you choose Open."),
                          MD3::Role::OnSurfaceVariant, ::Label::Body_12),
               0, wxLEFT | wxRIGHT, panel->FromDIP(kRowLeft));
    panel->SetSizer(sizer);
    return panel;
}

wxWindow *create_status_row(wxWindow *parent) { return new StatusRow(parent); }
wxWindow *create_address_row(wxWindow *parent) { return new AddressRow(parent); }
wxWindow *create_key_row(wxWindow *parent) { return new KeyRow(parent); }

void open_invite_dialog(wxWindow *parent)
{
    if (parent == nullptr) parent = wxGetApp().mainframe;
    InviteDialog dialog(parent);
    dialog.ShowModal();
}

}}} // namespace Slic3r::GUI::LanModelDrop
