#include "SmartHomeDialog.hpp"
#include "Widgets/Slider.hpp"

#include "DeviceCore/DevManager.h"
#include "DeviceManager.hpp"
#include "GUI_App.hpp"
#include "HomeAssistantSharingService.hpp"
#include "I18N.hpp"
#include "MsgDialog.hpp"
#include "NotificationManager.hpp"
#include "TtsNarrator.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/SearchField.hpp"
#include <wx/slider.h>
#include "Widgets/StateColor.hpp"
#include "Widgets/TextInput.hpp"

#include "libslic3r/AppConfig.hpp"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <map>
#include <utility>

#include <wx/arrstr.h>
#include <wx/display.h>
#include <wx/listbox.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/weakref.h>
#include <wx/wrapsizer.h>

#ifdef __WXMSW__
// Needed only for EM_SETPASSWORDCHAR on the token field (see mask_text_entry).
// NOMINMAX keeps windows.h from defining min/max as macros, which would break
// the std::min / std::max used throughout this file.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Slic3r { namespace GUI {

namespace {

constexpr int kDiscoverySharingLifetimeMs = 5 * 60 * 1000;
constexpr int kVolumeRequestDebounceMs     = 200;
constexpr std::size_t kMaxConfiguredHomeAssistantEntities = 32;
constexpr std::size_t kMaxVisibleHomeAssistantEntities    = 256;
constexpr std::size_t kMaxConfiguredHomeAssistantSegments = 256;
constexpr std::size_t kMaxConfiguredHomeAssistantScanBytes = 64 * 1024;
constexpr std::size_t kMaxConfiguredHomeAssistantValueBytes = 256;

std::vector<std::string> config_list_values(
    const char *key,
    bool *inspection_truncated = nullptr)
{
    if (inspection_truncated)
        *inspection_truncated = false;

    std::vector<std::string> values;
    const std::string raw = wxGetApp().app_config->get(key);
    std::size_t begin = 0;
    std::size_t inspected_segments = 0;
    bool consumed_all = false;
    bool rejected_oversized_value = false;
    const std::size_t scan_end =
        std::min(raw.size(), kMaxConfiguredHomeAssistantScanBytes);
    while (begin <= raw.size() &&
           begin <= scan_end &&
           inspected_segments < kMaxConfiguredHomeAssistantSegments) {
        const auto separator_it = std::find(
            raw.begin() + begin,
            raw.begin() + scan_end,
            ';');
        if (separator_it == raw.begin() + scan_end &&
            scan_end < raw.size())
            break;
        const std::size_t separator =
            separator_it == raw.begin() + scan_end
                ? std::string::npos
                : static_cast<std::size_t>(
                      separator_it - raw.begin());
        const std::size_t end =
            separator == std::string::npos ? raw.size() : separator;
        const std::size_t value_length = end - begin;
        ++inspected_segments;
        if (value_length > kMaxConfiguredHomeAssistantValueBytes) {
            rejected_oversized_value = true;
        } else if (value_length != 0) {
            const std::string value = raw.substr(begin, value_length);
            if (std::find(values.begin(), values.end(), value) == values.end())
                values.push_back(value);
        }
        // One sentinel entry beyond the active limit is enough to drive the
        // persistent warning. Keep duplicate checks and UI refresh work
        // bounded even if AppConfig was hand-edited with a very large list.
        if (values.size() > kMaxConfiguredHomeAssistantEntities)
            break;
        if (separator == std::string::npos) {
            consumed_all = true;
            break;
        }
        begin = separator + 1;
    }
    if (inspection_truncated)
        *inspection_truncated =
            rejected_oversized_value ||
            (!consumed_all &&
             values.size() <= kMaxConfiguredHomeAssistantEntities);
    return values;
}

enum class AppendConfigResult
{
    Added,
    AlreadyPresent,
    LimitReached,
};

AppendConfigResult append_to_config_list(const char *key, const std::string &value)
{
    AppConfig *cfg = wxGetApp().app_config;
    bool inspection_truncated = false;
    const std::vector<std::string> values =
        config_list_values(key, &inspection_truncated);
    if (std::find(values.begin(), values.end(), value) != values.end())
        return AppendConfigResult::AlreadyPresent;
    if (inspection_truncated ||
        values.size() >= kMaxConfiguredHomeAssistantEntities)
        return AppendConfigResult::LimitReached;
    // Preserve every pre-existing entry exactly. Adding one item is explicit
    // consent to append that item, not consent to normalize or truncate the
    // user's saved configuration.
    std::string updated = cfg->get(key);
    if (!updated.empty() && updated.back() != ';')
        updated += ';';
    updated += value;
    cfg->set(key, updated);
    cfg->save();
    return AppendConfigResult::Added;
}

void clear_config_list(const char *key)
{
    AppConfig *cfg = wxGetApp().app_config;
    cfg->set(key, "");
    cfg->save();
}

wxString config_list_pretty(const char *key, const wxString &empty_text)
{
    const std::vector<std::string> values = config_list_values(key);
    if (values.empty())
        return empty_text;

    // Render only the same bounded, de-duplicated prefix that the service can
    // activate. The raw value remains untouched in AppConfig, while an
    // accidentally huge hand-edited list cannot create an enormous label.
    std::string visible;
    for (std::size_t index = 0;
         index < values.size() && index < kMaxConfiguredHomeAssistantEntities;
         ++index) {
        if (!visible.empty())
            visible += ';';
        visible += values[index];
    }
    return wxString::FromUTF8(visible);
}

wxString localized_stacked(const char *source)
{
    return I18N::render_localized_text_stacked(
        I18N::translate_mode(source).finalize_without_arguments()).label;
}

wxString localized_stacked_count(const char *source, int count)
{
    const auto formatted = I18N::translate_mode(source).format_each([count](const wxString &format) {
        return format.Find("%d") == wxNOT_FOUND ? format : wxString::Format(format, count);
    });
    return I18N::render_localized_text_stacked(formatted).label;
}

wxString localized_stacked_counts(const char *source, int first, int second)
{
    const auto formatted = I18N::translate_mode(source).format_each([first, second](const wxString &format) {
        return wxString::Format(format, first, second);
    });
    return I18N::render_localized_text_stacked(formatted).label;
}

wxString localized_stacked_text(const char *source, const wxString &value)
{
    const auto formatted = I18N::translate_mode(source).format_each([&value](const wxString &format) {
        return wxString::Format(format, value);
    });
    return I18N::render_localized_text_stacked(formatted).label;
}

void make_responsive_action(Button &button)
{
    button.SetMinSize(button.FromDIP(wxSize(44, 44)));
    // Text actions must keep their measured label width. The surrounding
    // expand/wrap sizers already reflow them at narrow widths; allowing the
    // Button itself to shrink collapses labels such as "Play / Pause" and
    // "Close" into a 44-DIP square before the sizer gets a chance to wrap.
    button.SetAllowShrink(false);
    if (!button.GetLabel().empty())
        button.SetToolTip(button.GetLabel());
}

// Hide the long-lived Home Assistant token behind bullets.
//
// The masking style cannot travel through the MD3 field's constructor: TextInput
// strips wxALIGN_MASK from the style before forwarding it to its inner text
// control (Widgets/TextInput.cpp), and wxTE_PASSWORD (0x0800) is the same bit as
// wxALIGN_CENTER_VERTICAL, so it would be silently dropped and the credential
// would render in clear text. EM_SETPASSWORDCHAR is the documented way to turn
// masking on after the EDIT control exists -- adding ES_PASSWORD with
// SetWindowLong afterwards is explicitly not supported.
void mask_text_entry(wxTextCtrl *entry)
{
#ifdef __WXMSW__
    if (entry == nullptr || entry->GetHWND() == nullptr)
        return;
    // U+25CF BLACK CIRCLE, the glyph Windows itself uses for password edits.
    ::SendMessage(
        static_cast<HWND>(entry->GetHWND()),
        EM_SETPASSWORDCHAR,
        static_cast<WPARAM>(0x25CF),
        0);
    entry->Refresh();
#else
    (void) entry;
#endif
}

bool has_suffix(const std::string &value, const std::string &suffix)
{
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

wxString localized_printer_import_error(const std::string &error)
{
    static const std::string transport_suffix = ": transport error";
    static const std::string http_marker       = ": HTTP ";

    if (has_suffix(error, transport_suffix)) {
        const wxString serial = wxString::FromUTF8(
            error.substr(0, error.size() - transport_suffix.size()));
        return serial + ": " + localized_stacked("Home Assistant transport failed.");
    }

    const std::size_t http_pos = error.rfind(http_marker);
    if (http_pos != std::string::npos && http_pos + http_marker.size() < error.size()) {
        const wxString serial = wxString::FromUTF8(error.substr(0, http_pos));
        const wxString status = wxString::FromUTF8(error.substr(http_pos + http_marker.size()));
        return serial + ": " +
               localized_stacked_text("Home Assistant returned HTTP %s.", status);
    }

    if (error == "The Home Assistant printer import worker could not finish the request")
        return localized_stacked(
            "The Home Assistant printer import worker could not finish the request.");
    if (error == "The Home Assistant printer import worker could not start")
        return localized_stacked(
            "The Home Assistant printer import worker could not start.");
    if (error == "A Home Assistant printer import is already in progress")
        return localized_stacked(
            "A Home Assistant printer import is already in progress.");

    // The printer-import callback is still string-based. Keep unexpected
    // diagnostics bounded before placing them in a localized wrapper.
    constexpr std::size_t max_detail_length = 256;
    const wxString detail =
        wxString::FromUTF8(error.substr(0, max_detail_length));
    return localized_stacked_text("Home Assistant request failed: %s", detail);
}

wxString localized_entity_fetch_error(const HomeAssistant::EntityFetchResult &result)
{
    using Code = HomeAssistant::EntityFetchErrorCode;
    switch (result.error_code) {
    case Code::None:
        return {};
    case Code::ShuttingDown:
        return localized_stacked("Home Assistant is shutting down.");
    case Code::NotConfigured:
        return localized_stacked(
            "Home Assistant is not configured. Enter its URL and long-lived access token first.");
    case Code::InsecureTransport:
        return localized_stacked(
            "Home Assistant was not contacted. Use HTTPS for a remote URL. HTTP is allowed only for "
            "localhost or an IPv4 loopback address.");
    case Code::InvalidFilter:
        return localized_stacked(
            "Bambu Studio could not prepare the Home Assistant entity request.");
    case Code::WorkerUnavailable:
    case Code::RequestSetupFailed:
        return localized_stacked("Home Assistant request setup failed.");
    case Code::QueueFull:
        return localized_stacked(
            "Home Assistant is busy with another entity request. Try again in a moment.");
    case Code::TransportError:
        return localized_stacked(
            "Home Assistant could not be reached. Check the server and network connection.");
    case Code::HttpStatus:
        return localized_stacked_count(
            "Home Assistant returned HTTP %d while listing speakers and lights.",
            static_cast<int>(result.http_status));
    case Code::ResponseTooLarge:
        return localized_stacked(
            "Home Assistant returned more entity data than Bambu Studio can safely load.");
    case Code::InvalidResponse:
        return localized_stacked("Home Assistant returned an invalid states response.");
    }
    return localized_stacked("Home Assistant request failed.");
}

wxString localized_sharing_start_error(HomeAssistant::SharingStartErrorCode code)
{
    using Code = HomeAssistant::SharingStartErrorCode;
    switch (code) {
    case Code::None:
        return localized_stacked("Printer discovery sharing could not start.");
    case Code::AlreadyRunning:
    case Code::PreviousWorkerActive:
        return localized_stacked(
            "Printer discovery sharing is already active or still stopping. Turn it off and try again.");
    case Code::MissingSupplier:
    case Code::InvalidPairingToken:
    case Code::InvalidDisplayName:
    case Code::InvalidPort:
        return localized_stacked(
            "Bambu Studio could not prepare printer discovery sharing.");
    case Code::InvalidAdvertisedAddress:
    case Code::NoLanAddress:
        return localized_stacked(
            "Printer discovery sharing needs a usable local-network IPv4 address.");
    case Code::HttpEndpointUnavailable:
        return localized_stacked(
            "Printer discovery sharing could not open its local HTTP endpoint.");
    case Code::MdnsUnavailable:
        return localized_stacked(
            "Printer discovery sharing could not advertise itself with mDNS on this network.");
    case Code::WorkerUnavailable:
        return localized_stacked(
            "Printer discovery sharing could not start its background worker.");
    }
    return localized_stacked("Printer discovery sharing could not start.");
}

std::vector<HomeAssistant::PrinterHandover> accessible_printers()
{
    DeviceManager *device_manager = wxGetApp().getDeviceManager();
    std::map<std::string, HomeAssistant::PrinterHandover> unique;
    const auto collect = [&unique](const std::map<std::string, MachineObject *> &machines) {
        for (const auto &[serial, machine] : machines) {
            if (serial.empty() || machine == nullptr || !machine->has_access_right())
                continue;

            const std::string host         = machine->get_dev_ip();
            const std::string access_code = machine->get_access_code();
            if (host.empty() || access_code.empty() || unique.find(serial) != unique.end())
                continue;

            unique.emplace(serial, HomeAssistant::PrinterHandover{
                serial,
                host,
                access_code,
                machine->get_dev_name(),
            });
        }
    };
    if (device_manager != nullptr) {
        // Local discovery normally has the freshest LAN address, so it wins
        // when the same serial also appears in the signed-in user's map.
        collect(device_manager->get_local_machinelist());
        collect(device_manager->get_user_machinelist());
    }

    std::vector<HomeAssistant::PrinterHandover> printers;
    printers.reserve(unique.size());
    for (auto &entry : unique)
        printers.push_back(std::move(entry.second));
    return printers;
}

std::string printer_handover_payload(const std::vector<HomeAssistant::PrinterHandover> &printers)
{
    nlohmann::json payload;
    payload["printers"] = nlohmann::json::array();
    for (const auto &printer : printers) {
        nlohmann::json item = {
            {"serial", printer.serial},
            {"host", printer.host},
            {"access_code", printer.access_code},
        };
        if (!printer.name.empty())
            item["name"] = printer.name;
        payload["printers"].push_back(std::move(item));
    }
    return payload.dump();
}

bool push_home_assistant_notification(NotificationManager::NotificationLevel level,
                                      const wxString &message)
{
    NotificationManager *notifications = wxGetApp().notification_manager();
    if (notifications == nullptr)
        return false;
    notifications->push_notification(
        NotificationType::CustomNotification, level, std::string(message.ToUTF8()));
    return true;
}

void set_add_printers_button_copy(Button &button, bool busy)
{
    I18N::LocalizedTextRenderOptions options;
    options.max_width_px = button.FromDIP(300);
    options.base_tooltip = localized_stacked(
        "This copies each accessible printer's serial number, LAN address and access code to Home "
        "Assistant. Access codes are credentials.");
    I18N::apply_localized_text(
        button,
        I18N::translate_mode(
            busy ? "Adding printers..." : "Add my printers to Home Assistant").finalize_without_arguments(),
        options);
}

} // namespace

SmartHomeDialog::SmartHomeDialog(wxWindow *parent)
    : MD3Dialog(parent, _L("Smart home"),
                _L("Home Assistant speakers, media controls and alert lights."),
                MaterialIcon::Cast,
                MD3Dialog::Options{true, false})
{
    AppConfig *cfg = wxGetApp().app_config;
    const wxColour bg = GetBackgroundColour();
    const wxColour on = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour on_var = StateColor::semantic(MD3::Role::OnSurfaceVariant);

    // Keep the header and decision footer fixed while the potentially long,
    // bilingual Home Assistant content scrolls inside the available work area.
    m_scroll = new wxScrolledWindow(
        this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxVSCROLL | wxTAB_TRAVERSAL | wxBORDER_NONE);
    m_scroll->SetBackgroundColour(bg);
    m_scroll->SetScrollRate(0, FromDIP(12));
    auto *body = new wxBoxSizer(wxVERTICAL);
    m_scroll->SetSizer(body);
    GetContentSizer()->Add(m_scroll, 1, wxEXPAND);

    auto label = [&](const wxString &text, bool secondary = false, bool auto_wrap = false) {
        auto *l = new Label(
            m_scroll, secondary ? Label::Body_12 : Label::Body_13, text,
            auto_wrap ? LB_AUTO_WRAP : 0);
        l->SetBackgroundColour(bg);
        l->SetForegroundColour(secondary ? on_var : on);
        if (auto_wrap)
            l->SetMinSize(wxSize(0, -1));
        return l;
    };

    // --- connection ---------------------------------------------------------
    auto *conn = new wxBoxSizer(wxVERTICAL);
    conn->Add(label(_L("Home Assistant URL")), 0, wxBOTTOM, FromDIP(4));
    // The kit filled field (r10, SurfaceContainerHighest, Outline border that
    // promotes to Primary on hover) instead of a native sunken edit box, so the
    // credential row follows the theme like the rest of the dialog. The name is
    // set on the container AND on the inner entry: the container is what the
    // sizer and the layout tests address, the entry is what actually takes focus
    // and is announced by a screen reader.
    m_url = new ::TextInput(
        m_scroll, wxString::FromUTF8(cfg->get("ha_url")), wxEmptyString, wxEmptyString,
        wxDefaultPosition, wxDefaultSize);
    m_url->SetMinSize(wxSize(-1, FromDIP(44)));
    m_url->SetName(_L("Home Assistant URL"));
    m_url->GetTextCtrl()->SetName(_L("Home Assistant URL"));
    conn->Add(m_url, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    conn->Add(label(_L("Token")), 0, wxBOTTOM, FromDIP(4));
    m_token = new ::TextInput(
        m_scroll, wxString::FromUTF8(cfg->get("ha_token")), wxEmptyString, wxEmptyString,
        wxDefaultPosition, wxDefaultSize);
    m_token->SetMinSize(wxSize(-1, FromDIP(44)));
    m_token->SetName(_L("Token"));
    m_token->GetTextCtrl()->SetName(_L("Token"));
    mask_text_entry(m_token->GetTextCtrl());
    conn->Add(m_token, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    auto *connect = new Button(m_scroll, _L("Connect"));
    make_responsive_action(*connect);
    connect->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        AppConfig *c = wxGetApp().app_config;
        c->set("ha_url", std::string(m_url->GetTextCtrl()->GetValue().ToUTF8()));
        c->set("ha_token", std::string(m_token->GetTextCtrl()->GetValue().ToUTF8()));
        c->save();
        refresh_entities();
    });
    conn->Add(connect, 0, wxEXPAND);
    body->Add(conn, 0, wxEXPAND | wxALL, FromDIP(10));

    m_status = label(
        _L("Not connected yet. Enter the URL and a long-lived access token, then press Connect."),
        true,
        true);
    body->Add(m_status, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));

    // --- printer handover ----------------------------------------------------
    // The disclosure stays beside the action and is repeated in a decision
    // dialog at click time. Results use the non-blocking corner notification
    // system; only the credential-consent decision is modal.
    auto *handover = new wxBoxSizer(wxVERTICAL);
    handover->Add(label(localized_stacked("Printer handover")), 0, wxBOTTOM, FromDIP(4));
    auto *handover_disclosure = label(
        localized_stacked(
            "Printer access codes are credentials. Adding sends them to Home Assistant; discovery makes "
            "them available for five minutes to any device on this local network that sees the pairing "
            "value."),
        true, true);
    handover->Add(handover_disclosure, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    auto *handover_actions = new wxBoxSizer(wxVERTICAL);
    m_add_printers = new Button(m_scroll, wxEmptyString);
    make_responsive_action(*m_add_printers);
    set_add_printers_button_copy(*m_add_printers, false);
    m_add_printers->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        add_printers_to_home_assistant();
    });
    handover_actions->Add(m_add_printers, 0, wxEXPAND);

    auto *discovery_row = new wxBoxSizer(wxHORIZONTAL);
    m_discovery_toggle = new CheckBox(m_scroll);
    m_discovery_toggle->SetMinSize(FromDIP(wxSize(44, 44)));
    m_discovery_toggle->SetValue(false); // sharing never persists across windows or restarts
    const wxString discovery_toggle_copy =
        localized_stacked("Share for discovery for 5 minutes (no Home Assistant token)");
    const wxString discovery_tooltip = localized_stacked(
        "Discovery publishes a temporary pairing value on this local network. Any device on this "
        "network can use it to request printer serial numbers, LAN addresses and access codes for "
        "up to five minutes. Enable only while importing, then switch it off.");
    m_discovery_toggle->SetName(discovery_toggle_copy);
    m_discovery_toggle->SetToolTip(discovery_tooltip);
    discovery_row->Add(m_discovery_toggle, 0, wxALIGN_CENTER_VERTICAL);
    auto *discovery_label = label(discovery_toggle_copy, false, true);
    discovery_label->SetToolTip(discovery_tooltip);
    discovery_label->SetCursor(wxCursor(wxCURSOR_HAND));
    discovery_row->Add(discovery_label, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    m_discovery_toggle->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &event) {
        set_discovery_sharing(m_discovery_toggle->GetValue());
        event.Skip();
    });
    discovery_label->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) {
        const bool enabled = !m_discovery_toggle->GetValue();
        m_discovery_toggle->SetValue(enabled);
        set_discovery_sharing(enabled);
    });
    m_discovery_expiry_timer.Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
        if (m_discovery_toggle && m_discovery_toggle->GetValue()) {
            m_discovery_toggle->SetValue(false);
            set_discovery_sharing(false, true);
        }
    });
    handover_actions->Add(discovery_row, 0, wxEXPAND | wxTOP, FromDIP(8));
    handover->Add(handover_actions, 0, wxEXPAND);

    m_discovery_status =
        label(localized_stacked("Printer discovery sharing is off."), true, true);
    handover->Add(m_discovery_status, 0, wxEXPAND | wxTOP, FromDIP(6));
    body->Add(handover, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

    // --- entity browser: search + list --------------------------------------
    m_search = new SearchField(m_scroll, _L("Search speakers and lights"));
    m_search->SetOnQuery([this](const wxString &) { rebuild_list(); });
    m_search->SetOnRegexToggle([this](bool) { rebuild_list(); });
    body->Add(m_search, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    // wxBORDER_NONE drops the sunken Win32 client edge so the list reads as an
    // MD3 SurfaceContainer block against the dialog Surface rather than an OS
    // control. wxLB_HSCROLL stays: long friendly names must remain reachable
    // instead of being clipped.
    m_list = new wxListBox(
        m_scroll, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(150)),
        0, nullptr, wxBORDER_NONE | wxLB_SINGLE | wxLB_HSCROLL);
    m_list->SetName(_L("Search speakers and lights"));
    m_list->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    m_list->SetForegroundColour(on);
    m_list->SetFont(Label::Body_13);
    body->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    m_results_status = label(wxEmptyString, true, true);
    m_results_status->Hide();
    body->Add(
        m_results_status, 0,
        wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));

    // --- media controls ------------------------------------------------------
    auto *media = new wxWrapSizer(wxHORIZONTAL);
    const std::vector<std::pair<wxString, std::string>> controls = {
        {_L("Previous"), "media_previous_track"},
        {_L("Play / Pause"), "media_play_pause"},
        {_L("Next"), "media_next_track"},
    };
    for (const auto &[text, service] : controls) {
        auto *b = new Button(m_scroll, text);
        make_responsive_action(*b);
        b->Bind(wxEVT_BUTTON, [this, service = service](wxCommandEvent &) {
            if (const auto *entity = selected_entity())
                HomeAssistant::media_command(entity->entity_id, service);
        });
        media->Add(b, 0, wxRIGHT | wxBOTTOM, FromDIP(6));
    }
    body->Add(media, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

    auto *volume_group = new wxBoxSizer(wxVERTICAL);
    volume_group->Add(label(_L("Volume")), 0, wxBOTTOM, FromDIP(4));
    m_volume_debounce_timer.Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
        if (m_pending_volume_entity.empty())
            return;

        const std::string entity_id = std::move(m_pending_volume_entity);
        m_pending_volume_entity.clear();
        HomeAssistant::media_volume(entity_id, m_pending_volume_level);
    });
    // Kit Slider: the shared MD3 Slider now takes keyboard focus and exposes
    // ROLE_SYSTEM_SLIDER with its value through SliderAccessible, so the native
    // trackbar this once kept for accessibility is replaced without losing it.
    m_volume = new Slider(m_scroll, 50, 0, 100);
    m_volume->SetMinSize(FromDIP(wxSize(44, 44)));
    m_volume->SetName(_L("Volume"));
    m_volume->SetOnChange([this](int) {
        if (const auto *entity = selected_entity()) {
            m_pending_volume_entity = entity->entity_id;
            m_pending_volume_level  = m_volume->GetValue() / 100.0;
            m_volume_debounce_timer.StartOnce(kVolumeRequestDebounceMs);
        } else {
            m_volume_debounce_timer.Stop();
            m_pending_volume_entity.clear();
        }
    });
    volume_group->Add(m_volume, 0, wxEXPAND);
    body->Add(
        volume_group, 0,
        wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(4));

    // --- add-as buttons ------------------------------------------------------
    auto *adds = new wxBoxSizer(wxVERTICAL);
    auto *add_speaker = new Button(m_scroll, _L("Use as announcement speaker"));
    make_responsive_action(*add_speaker);
    add_speaker->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (const auto *entity = selected_entity();
            entity != nullptr && entity->entity_id.rfind("media_player.", 0) == 0) {
            const AppendConfigResult result =
                append_to_config_list("ha_speakers", entity->entity_id);
            if (result == AppendConfigResult::LimitReached) {
                const wxString message = localized_stacked(
                    "The Home Assistant list already has 32 entries. Clear the list before adding another.");
                if (!push_home_assistant_notification(
                        NotificationManager::NotificationLevel::WarningNotificationLevel,
                        message))
                    update_wrapped_label(m_status, message);
                return;
            }
            update_wrapped_label(
                m_speakers_label, config_list_pretty("ha_speakers", _L("none")));
            update_config_limit_notice();
        }
    });
    adds->Add(add_speaker, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    auto *add_light = new Button(m_scroll, _L("Use as alert light"));
    make_responsive_action(*add_light);
    add_light->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        if (const auto *entity = selected_entity();
            entity != nullptr && entity->entity_id.rfind("light.", 0) == 0) {
            const AppendConfigResult result =
                append_to_config_list("ha_lights", entity->entity_id);
            if (result == AppendConfigResult::LimitReached) {
                const wxString message = localized_stacked(
                    "The Home Assistant list already has 32 entries. Clear the list before adding another.");
                if (!push_home_assistant_notification(
                        NotificationManager::NotificationLevel::WarningNotificationLevel,
                        message))
                    update_wrapped_label(m_status, message);
                return;
            }
            update_wrapped_label(
                m_lights_label, config_list_pretty("ha_lights", _L("none")));
            update_config_limit_notice();
        }
    });
    adds->Add(add_light, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    body->Add(adds, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));
    body->Add(
        label(
            localized_stacked(
                "You can configure up to 32 announcement speakers and 32 alert lights."),
            true,
            true),
        0,
        wxEXPAND | wxLEFT | wxRIGHT,
        FromDIP(10));
    m_config_limit_status = label(wxEmptyString, true, true);
    m_config_limit_status->Hide();
    body->Add(
        m_config_limit_status, 0,
        wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));

    auto *speakers_row = new wxBoxSizer(wxVERTICAL);
    speakers_row->Add(
        label(_L("Announcement speakers:"), true), 0, wxBOTTOM, FromDIP(2));
    m_speakers_label =
        label(config_list_pretty("ha_speakers", _L("none")), true, true);
    m_speakers_label->SetName(_L("Announcement speakers:"));
    speakers_row->Add(m_speakers_label, 0, wxEXPAND);
    auto *clear_speakers = new Button(m_scroll, _L("Clear speakers"));
    make_responsive_action(*clear_speakers);
    clear_speakers->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        clear_config_list("ha_speakers");
        update_wrapped_label(m_speakers_label, _L("none"));
        update_config_limit_notice();
    });
    speakers_row->Add(clear_speakers, 0, wxEXPAND | wxTOP, FromDIP(4));
    body->Add(speakers_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
    auto *lights_row = new wxBoxSizer(wxVERTICAL);
    lights_row->Add(label(_L("Alert lights:"), true), 0, wxBOTTOM, FromDIP(2));
    m_lights_label = label(config_list_pretty("ha_lights", _L("none")), true, true);
    m_lights_label->SetName(_L("Alert lights:"));
    lights_row->Add(m_lights_label, 0, wxEXPAND);
    auto *clear_lights = new Button(m_scroll, _L("Clear lights"));
    make_responsive_action(*clear_lights);
    clear_lights->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        clear_config_list("ha_lights");
        update_wrapped_label(m_lights_label, _L("none"));
        update_config_limit_notice();
    });
    lights_row->Add(clear_lights, 0, wxEXPAND | wxTOP, FromDIP(4));
    body->Add(lights_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(2));

    // --- toggles -------------------------------------------------------------
    auto add_toggle = [&](CheckBox *&store, const char *key, const wxString &text, bool default_on) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        store = new CheckBox(m_scroll);
        store->SetMinSize(FromDIP(wxSize(44, 44)));
        store->SetName(text);
        store->SetToolTip(text);
        const std::string value = cfg->get(key);
        store->SetValue(value.empty() ? default_on : value == "true");
        store->Bind(wxEVT_TOGGLEBUTTON, [key, store](wxCommandEvent &e) {
            wxGetApp().app_config->set(key, store->GetValue() ? "true" : "false");
            wxGetApp().app_config->save();
            e.Skip();
        });
        row->Add(store, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(
            label(text, false, true), 1,
            wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        body->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(8));
    };
    // Narrator is OFF by default (narrator rules); light flashes default on
    // once lights are chosen, since choosing them is the explicit opt-in.
    add_toggle(m_narrator_toggle, "narrator_enabled",
               _L("Speak printer state changes and errors (TTS narrator)"), false);
    add_toggle(m_flash_error_toggle, "ha_flash_on_error",
               _L("Flash alert lights red on printer errors (auto-restores)"), true);
    add_toggle(m_flash_finish_toggle, "ha_flash_on_finish",
               _L("Pulse alert lights green when a print finishes (auto-restores)"), true);

    auto *close = new Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);
    make_responsive_action(*close);
    close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    GetFooterSizer()->AddStretchSpacer();
    GetFooterSizer()->Add(close, 0, wxALIGN_CENTER_VERTICAL);
    update_config_limit_notice();

    // A resize can add/remove the vertical scrollbar, which changes the usable
    // wrapping width. Re-run the content sizer after that width settles.
    m_scroll->Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
        event.Skip();
        if (m_scroll && m_scroll->GetSizer()) {
            m_scroll->GetSizer()->Layout();
            m_scroll->FitInside();
        }
    });

    constrain_to_work_area(true);
    relayout_content();
    CenterOnParent();
    constrain_to_work_area(false);
    if (HomeAssistant::configured())
        refresh_entities();
}

SmartHomeDialog::~SmartHomeDialog()
{
    m_volume_debounce_timer.Stop();
    m_discovery_expiry_timer.Stop();
    if (m_sharing_service)
        m_sharing_service->stop();
}

void SmartHomeDialog::update_wrapped_label(Label *label, const wxString &text)
{
    if (!label)
        return;
    label->SetLabel(text);
    relayout_content();
}

void SmartHomeDialog::update_config_limit_notice()
{
    if (!m_config_limit_status)
        return;

    bool speakers_truncated = false;
    bool lights_truncated = false;
    const std::vector<std::string> speakers =
        config_list_values("ha_speakers", &speakers_truncated);
    const std::vector<std::string> lights =
        config_list_values("ha_lights", &lights_truncated);
    const bool over_limit =
        speakers_truncated || lights_truncated ||
        speakers.size() > kMaxConfiguredHomeAssistantEntities ||
        lights.size() > kMaxConfiguredHomeAssistantEntities;
    if (over_limit) {
        m_config_limit_status->SetLabel(localized_stacked(
            "Saved Home Assistant speaker or light lists exceed the safe size limits. Existing "
            "entries were kept; only the first 32 valid entries are active."));
        m_config_limit_status->Show();
    } else {
        m_config_limit_status->Hide();
    }
    relayout_content();
}

void SmartHomeDialog::relayout_content()
{
    if (m_scroll) {
        if (wxSizer *sizer = m_scroll->GetSizer()) {
            // The first pass assigns the viewport width; auto-wrapping Labels
            // then update their best height, which the second pass consumes.
            sizer->Layout();
            sizer->Layout();
        }
        m_scroll->FitInside();
    }
    Layout();
}

void SmartHomeDialog::constrain_to_work_area(bool use_preferred_size)
{
    int display_index = wxDisplay::GetFromWindow(this);
    if (display_index == wxNOT_FOUND && GetParent())
        display_index = wxDisplay::GetFromWindow(GetParent());
    if (display_index == wxNOT_FOUND)
        display_index = 0;
    if (display_index < 0 || display_index >= static_cast<int>(wxDisplay::GetCount()))
        return;

    const wxRect work_area = wxDisplay(display_index).GetClientArea();
    if (work_area.IsEmpty())
        return;

    const int margin = FromDIP(16);
    const wxSize available(
        std::max(1, work_area.GetWidth() - 2 * margin),
        std::max(1, work_area.GetHeight() - 2 * margin));
    const wxSize desired_min = FromDIP(wxSize(520, 480));
    const wxSize minimum(
        std::min(desired_min.GetWidth(), available.GetWidth()),
        std::min(desired_min.GetHeight(), available.GetHeight()));
    SetMinSize(minimum);

    wxSize target = use_preferred_size ? FromDIP(wxSize(720, 760)) : GetSize();
    target.SetWidth(std::max(
        minimum.GetWidth(), std::min(target.GetWidth(), available.GetWidth())));
    target.SetHeight(std::max(
        minimum.GetHeight(), std::min(target.GetHeight(), available.GetHeight())));
    SetSize(target);

    if (!use_preferred_size) {
        const int max_x = work_area.GetRight() - margin - target.GetWidth() + 1;
        const int max_y = work_area.GetBottom() - margin - target.GetHeight() + 1;
        const wxPoint current = GetPosition();
        Move(
            std::max(work_area.GetLeft() + margin, std::min(current.x, max_x)),
            std::max(work_area.GetTop() + margin, std::min(current.y, max_y)));
    }
}

void SmartHomeDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    MD3Dialog::on_dpi_changed(suggested_rect);
    constrain_to_work_area(false);
    relayout_content();
}

void SmartHomeDialog::add_printers_to_home_assistant()
{
    // Use the values currently visible in the dialog. Clicking this action is
    // an explicit request to use them, just like Connect.
    AppConfig *config = wxGetApp().app_config;
    config->set("ha_url", std::string(m_url->GetTextCtrl()->GetValue().ToUTF8()));
    config->set("ha_token", std::string(m_token->GetTextCtrl()->GetValue().ToUTF8()));
    config->save();

    const auto notify_or_status = [this](NotificationManager::NotificationLevel level,
                                         const wxString &message) {
        if (!push_home_assistant_notification(level, message))
            update_wrapped_label(m_status, message);
    };

    switch (HomeAssistant::credential_transport_safety()) {
    case HomeAssistant::CredentialTransportSafety::NotConfigured:
        notify_or_status(
            NotificationManager::NotificationLevel::WarningNotificationLevel,
            localized_stacked(
                "Home Assistant is not configured. Enter its URL and long-lived access token first."));
        m_url->SetFocus();
        return;
    case HomeAssistant::CredentialTransportSafety::Insecure:
        notify_or_status(
            NotificationManager::NotificationLevel::ErrorNotificationLevel,
            localized_stacked(
                "Printer access codes were not sent. Use HTTPS for a remote Home Assistant URL. "
                "HTTP is allowed only for localhost or an IPv4 loopback address."));
        m_url->SetFocus();
        return;
    case HomeAssistant::CredentialTransportSafety::Safe:
        break;
    }

    std::vector<HomeAssistant::PrinterHandover> printers = accessible_printers();
    if (printers.empty()) {
        notify_or_status(
            NotificationManager::NotificationLevel::WarningNotificationLevel,
            localized_stacked(
                "No accessible printers were found. Connect a printer and make sure its LAN address "
                "and access code are available."));
        return;
    }

    const bool import_truncated =
        printers.size() > HomeAssistant::SharingService::max_printer_count();
    if (import_truncated)
        printers.resize(HomeAssistant::SharingService::max_printer_count());
    const int total = static_cast<int>(printers.size());
    const char *confirmation_source =
        total == 1
            ? "This will copy the serial number, LAN address and access code for one accessible "
              "printer to Home Assistant. Access codes are credentials. Continue?"
            : "This will copy the serial number, LAN address and access code for %d accessible "
              "printers to Home Assistant. Access codes are credentials. Continue?";
    wxString confirmation_message =
        localized_stacked_count(confirmation_source, total);
    if (import_truncated) {
        confirmation_message += "\n\n";
        confirmation_message += localized_stacked(
            "Printer handover is limited to the first 32 accessible printers; additional printers "
            "will not be sent.");
    }
    MessageDialog confirmation(
        this,
        confirmation_message,
        localized_stacked("Copy printer access codes to Home Assistant?"),
        wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    confirmation.SetButtonLabel(
        wxID_YES, localized_stacked("Copy access codes and add printers"));
    if (confirmation.ShowModal() != wxID_YES)
        return;

    m_add_printers->Enable(false);
    set_add_printers_button_copy(*m_add_printers, true);

    const wxString progress =
        total == 1
            ? localized_stacked("Adding one printer to Home Assistant...")
            : localized_stacked_count("Adding %d printers to Home Assistant...", total);
    notify_or_status(NotificationManager::NotificationLevel::RegularNotificationLevel, progress);

    wxWeakRef<Button>          add_button(m_add_printers);
    wxWeakRef<SmartHomeDialog> dialog(this);
    HomeAssistant::add_printers(
        printers,
        [add_button, dialog, total](int processed, std::vector<std::string> errors) {
            if (add_button) {
                add_button->Enable(true);
                set_add_printers_button_copy(*add_button, false);
            }

            wxString result = localized_stacked_counts(
                "Home Assistant processed %d of %d printer requests.", processed, total);
            if (!errors.empty()) {
                result += "\n";
                result += localized_stacked("Problems:");
                for (const std::string &error : errors)
                    result += "\n- " + localized_printer_import_error(error);
            }

            const NotificationManager::NotificationLevel level =
                errors.empty()
                    ? NotificationManager::NotificationLevel::RegularNotificationLevel
                    : (processed > 0
                           ? NotificationManager::NotificationLevel::WarningNotificationLevel
                           : NotificationManager::NotificationLevel::ErrorNotificationLevel);
            if (!push_home_assistant_notification(level, result) && dialog)
                dialog->update_wrapped_label(dialog->m_status, result);
        });
}

void SmartHomeDialog::set_discovery_sharing(bool enabled, bool expired)
{
    const auto set_status = [this](const wxString &message) {
        update_wrapped_label(m_discovery_status, message);
    };

    if (!enabled) {
        m_discovery_expiry_timer.Stop();
        const bool was_running = m_sharing_service && m_sharing_service->is_running();
        if (m_sharing_service)
            m_sharing_service->stop();
        m_sharing_service.reset();
        const wxString stopped_message = expired
            ? localized_stacked(
                  "Printer discovery sharing expired after five minutes. Access codes are no longer "
                  "offered.")
            : localized_stacked("Printer discovery sharing is off.");
        set_status(stopped_message);
        if (was_running) {
            push_home_assistant_notification(
                NotificationManager::NotificationLevel::RegularNotificationLevel,
                expired
                    ? stopped_message
                    : localized_stacked(
                          "Printer discovery sharing stopped. Access codes are no longer offered."));
        }
        return;
    }

    std::vector<HomeAssistant::PrinterHandover> printers = accessible_printers();
    if (printers.empty()) {
        m_discovery_toggle->SetValue(false);
        const wxString message = localized_stacked(
            "Printer discovery sharing did not start. No accessible printer has both a LAN address "
            "and an access code.");
        set_status(message);
        push_home_assistant_notification(
            NotificationManager::NotificationLevel::WarningNotificationLevel, message);
        return;
    }

    const bool offer_truncated =
        printers.size() > HomeAssistant::SharingService::max_printer_count();
    if (offer_truncated)
        printers.resize(HomeAssistant::SharingService::max_printer_count());
    const int count = static_cast<int>(printers.size());
    const std::string payload = printer_handover_payload(printers);

    m_sharing_service = std::make_unique<HomeAssistant::SharingService>();
    HomeAssistant::SharingService::Options options;
    options.pairing_token = HomeAssistant::SharingService::make_pairing_token();
    // Keep the mDNS display value generic: the random instance suffix
    // distinguishes concurrent offers without publishing the Windows hostname.
    options.display_name = "Bambu Studio";

    const auto result = m_sharing_service->start(
        std::move(options),
        [payload]() { return payload; });
    if (!result) {
        m_discovery_toggle->SetValue(false);
        m_sharing_service.reset();
        const wxString message = localized_sharing_start_error(result.error_code);
        set_status(message);
        push_home_assistant_notification(
            NotificationManager::NotificationLevel::ErrorNotificationLevel, message);
        return;
    }

    m_discovery_expiry_timer.StartOnce(kDiscoverySharingLifetimeMs);
    wxString message = count == 1
        ? localized_stacked(
              "Sharing one printer on the local network for up to five minutes. Turn sharing off "
              "after Home Assistant imports it.")
        : localized_stacked_count(
              "Sharing %d printers on the local network for up to five minutes. Turn sharing off "
              "after Home Assistant imports them.",
              count);
    if (offer_truncated) {
        message += "\n";
        message += localized_stacked(
            "Discovery sharing is limited to the first 32 accessible printers; additional printers "
            "were not included.");
    }
    set_status(message);
    push_home_assistant_notification(
        offer_truncated
            ? NotificationManager::NotificationLevel::WarningNotificationLevel
            : NotificationManager::NotificationLevel::RegularNotificationLevel,
        message);
}

void SmartHomeDialog::refresh_entities()
{
    if (m_entity_refresh_in_flight) {
        // Coalesce repeated Connect clicks while preserving the latest saved
        // URL/token. The active response is discarded and one fresh request
        // is replayed when it completes.
        m_entity_refresh_pending = true;
        return;
    }

    switch (HomeAssistant::credential_transport_safety()) {
    case HomeAssistant::CredentialTransportSafety::NotConfigured:
        update_wrapped_label(
            m_status,
            localized_stacked(
                "Home Assistant is not configured. Enter its URL and long-lived access token first."));
        return;
    case HomeAssistant::CredentialTransportSafety::Insecure:
        update_wrapped_label(
            m_status,
            localized_stacked(
                "Home Assistant was not contacted. Use HTTPS for a remote URL. HTTP is allowed only for "
                "localhost or an IPv4 loopback address."));
        return;
    case HomeAssistant::CredentialTransportSafety::Safe:
        break;
    }

    m_entity_refresh_in_flight = true;
    update_wrapped_label(m_status, _L("Connecting to Home Assistant..."));
    wxWeakRef<SmartHomeDialog> dialog(this);
    HomeAssistant::list_entities(
        HomeAssistant::EntityQuery{{"media_player", "light"}},
        [dialog](HomeAssistant::EntityFetchResult result) {
            if (!dialog)
                return;
            dialog->m_entity_refresh_in_flight = false;
            if (dialog->m_entity_refresh_pending) {
                dialog->m_entity_refresh_pending = false;
                dialog->refresh_entities();
                return;
            }
            if (!result) {
                dialog->update_wrapped_label(
                    dialog->m_status, localized_entity_fetch_error(result));
                return;
            }

            dialog->m_entities = std::move(result.entities);
            dialog->m_entity_fetch_truncated = result.truncated;
            dialog->rebuild_list();
            dialog->update_wrapped_label(
                dialog->m_status,
                wxString::Format(
                    _L("Connected. %d speakers and lights found."),
                    static_cast<int>(dialog->m_entities.size())));
        });
}

void SmartHomeDialog::rebuild_list()
{
    const wxString query = m_search->GetValue();
    const bool regex      = m_search->IsRegexEnabled();
    const bool case_sense = m_search->IsCaseSensitive();
    const bool whole_word = m_search->IsWholeWord();
    const bool multiline  = m_search->IsMultiline();

    wxArrayString rows;
    rows.Alloc(kMaxVisibleHomeAssistantEntities);
    m_visible.clear();
    m_visible.reserve(std::min(m_entities.size(), kMaxVisibleHomeAssistantEntities));
    std::size_t match_count = 0;
    SearchField::MatchPass match_pass(query, regex, case_sense, whole_word, multiline);
    for (int i = 0; i < (int) m_entities.size(); ++i) {
        const auto &entity = m_entities[i];
        const wxString hay = wxString::FromUTF8(entity.friendly_name + " " + entity.entity_id);
        if (!query.IsEmpty() && !match_pass.matches(hay))
            continue;
        ++match_count;
        if (rows.size() < kMaxVisibleHomeAssistantEntities) {
            rows.Add(wxString::Format("%s  (%s, %s)",
                                      wxString::FromUTF8(entity.friendly_name),
                                      wxString::FromUTF8(entity.entity_id),
                                      wxString::FromUTF8(entity.state)));
            m_visible.push_back(i);
        }
    }

    m_list->Freeze();
    m_list->Set(rows);
    m_list->Thaw();

    wxString summary;
    if (match_count == 0) {
        summary = localized_stacked("No speakers or lights match this search.");
    } else {
        summary = localized_stacked_counts(
            "Showing %d of %d matching speakers and lights.",
            static_cast<int>(rows.size()),
            static_cast<int>(match_count));
        if (match_count > rows.size()) {
            summary += "\n";
            summary += localized_stacked(
                "Refine the search to see the remaining matches.");
        }
    }
    if (m_entity_fetch_truncated) {
        if (!summary.empty())
            summary += "\n";
        summary += localized_stacked_count(
            "Home Assistant returned additional speakers and lights beyond the %d-entity safety "
            "limit. Those additional entities are not shown.",
            static_cast<int>(m_entities.size()));
    }
    m_results_status->Show();
    update_wrapped_label(m_results_status, summary);
}

const HomeAssistant::Entity *SmartHomeDialog::selected_entity() const
{
    const int sel = m_list->GetSelection();
    if (sel == wxNOT_FOUND || sel >= (int) m_visible.size())
        return nullptr;
    return &m_entities[m_visible[sel]];
}

} } // namespace Slic3r::GUI
