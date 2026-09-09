#include "HomeAssistantSettingsSync.hpp"
#include "HomeAssistantSettingsSyncModel.hpp"

#include "GUI_App.hpp"
#include "HomeAssistant.hpp"
#include "I18N.hpp"
#include "NotificationManager.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r_version.h"

#include <boost/log/trivial.hpp>

#include <wx/datetime.h>
#include <wx/event.h>
#include <wx/timer.h>

#include <cstdio>
#include <ctime>
#include <memory>
#include <utility>

namespace Slic3r { namespace GUI { namespace HomeAssistant { namespace SettingsSync {

namespace {

std::string local_iso_now()
{
    const std::time_t t = std::time(nullptr);
    std::tm           local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char buf[40];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%S", &local);
    std::string text = buf;
    // strftime %z prints "+0400"; the dialog and HA both read "+04:00".
    char zone[8];
    std::strftime(zone, sizeof zone, "%z", &local);
    std::string z = zone;
    if (z.size() == 5)
        text += z.substr(0, 3) + ":" + z.substr(3);
    return text;
}

std::string error_note(EntityFetchErrorCode code, unsigned status)
{
    switch (code) {
    case EntityFetchErrorCode::None: return {};
    case EntityFetchErrorCode::ShuttingDown: return _u8L("shutting down");
    case EntityFetchErrorCode::NotConfigured: return _u8L("Home Assistant is not connected (enter the URL and token above)");
    case EntityFetchErrorCode::InsecureTransport: return _u8L("Home Assistant URL must be HTTPS or localhost");
    case EntityFetchErrorCode::InvalidFilter: return _u8L("entity id was rejected");
    case EntityFetchErrorCode::WorkerUnavailable:
    case EntityFetchErrorCode::QueueFull:
    case EntityFetchErrorCode::RequestSetupFailed: return _u8L("could not start the request");
    case EntityFetchErrorCode::TransportError: return _u8L("could not reach Home Assistant");
    case EntityFetchErrorCode::HttpStatus:
        return status == 401 || status == 403 ? _u8L("Home Assistant refused the token (HTTP ") + std::to_string(status) + ")"
                                                : "HTTP " + std::to_string(status);
    case EntityFetchErrorCode::ResponseTooLarge: return _u8L("response too large");
    case EntityFetchErrorCode::InvalidResponse: return _u8L("unexpected response");
    }
    return _u8L("unknown error");
}

class SyncService final : private wxEvtHandler
{
public:
    SyncService()
    {
        m_debounce.SetOwner(this, wxID_ANY);
        m_interval.SetOwner(this, wxID_ANY);
        Bind(wxEVT_TIMER, [this](wxTimerEvent &e) {
            if (e.GetTimer().GetId() == m_debounce.GetId() || e.GetTimer().GetId() == m_interval.GetId())
                sync_now();
        });
    }

    void install()
    {
        if (m_installed)
            return;
        m_installed = true;
        // Chain, never replace: PreferencesHistory installed its observer first.
        std::function<void()> previous = AppConfig::save_observer();
        AppConfig::set_save_observer([previous]() {
            if (previous)
                previous();
            service().on_config_saved();
        });
        apply_enabled_state(enabled());
    }

    void shutdown()
    {
        m_shut_down = true;
        m_debounce.Stop();
        m_interval.Stop();
        ++m_generation; // in-flight completions are ignored
    }

    bool enabled() const
    {
        AppConfig *cfg = wxGetApp().app_config;
        return cfg != nullptr && cfg->get(kEnabledKey) == "true";
    }

    void set_enabled(bool on)
    {
        AppConfig *cfg = wxGetApp().app_config;
        if (cfg == nullptr)
            return;
        cfg->set(kEnabledKey, on ? "true" : "false");
        cfg->save(); // fires the observer; debounce is harmless because we sync now
        apply_enabled_state(on);
        if (on)
            sync_now();
        else {
            m_status = Status{};
            notify_listener();
        }
    }

    void on_config_saved()
    {
        if (m_shut_down || !enabled())
            return;
        m_debounce.StartOnce(kDebounceSeconds * 1000);
    }

    void sync_now()
    {
        if (m_shut_down || !enabled() || m_in_flight)
            return;
        AppConfig *cfg = wxGetApp().app_config;
        if (cfg == nullptr)
            return;
        m_debounce.Stop();
        const auto settings = publishable_settings(cfg->get_section("app"));
        const std::string synced_at = local_iso_now();
        auto updates = std::make_shared<std::vector<StateUpdate>>(build_state_updates(settings, synced_at, SLIC3R_VERSION));
        m_in_flight   = true;
        m_status.state = State::Syncing;
        m_status.entities_total = int(updates->size());
        m_status.entities_ok    = 0;
        notify_listener();

        const std::uint64_t generation = ++m_generation;
        auto pending = std::make_shared<int>(int(updates->size()));
        auto failed  = std::make_shared<std::string>();
        for (const StateUpdate &update : *updates) {
            // Belt and braces: the transport refuses anything the model let through.
            if (!payload_is_clean(update.body)) {
                BOOST_LOG_TRIVIAL(warning) << "HomeAssistant settings sync: payload for " << update.entity_id << " failed the deny-list and was not sent";
                *failed = _u8L("a payload failed the deny-list and was not sent");
                if (--*pending == 0)
                    finish(generation, synced_at, *failed);
                continue;
            }
            set_entity_state(update.entity_id, update.body, [this, generation, synced_at, pending, failed](StateWriteResult result) {
                if (generation != m_generation)
                    return; // a newer push or shutdown superseded this one
                if (result)
                    ++m_status.entities_ok;
                else if (failed->empty())
                    *failed = error_note(result.error_code, result.http_status);
                if (--*pending == 0)
                    finish(generation, synced_at, *failed);
            });
        }
    }

    Status status() const { return m_status; }
    void   set_status_listener(std::function<void()> listener) { m_listener = std::move(listener); }

    static SyncService &service();

private:

    void apply_enabled_state(bool on)
    {
        if (on) {
            if (!m_interval.IsRunning())
                m_interval.Start(kIntervalMinutes * 60 * 1000);
            if (m_status.state == State::Off)
                m_status.state = State::Idle;
        } else {
            m_interval.Stop();
            m_debounce.Stop();
            m_status.state = State::Off;
        }
    }

    void finish(std::uint64_t generation, const std::string &synced_at, const std::string &error)
    {
        if (generation != m_generation)
            return;
        m_in_flight = false;
        const bool was_failed = m_status.state == State::Failed;
        if (error.empty()) {
            m_status.state          = State::Synced;
            m_status.last_synced_at = synced_at;
            m_status.last_error.clear();
        } else {
            m_status.state      = State::Failed;
            m_status.last_error = error;
            if (!was_failed)
                notify_failure(error);
        }
        notify_listener();
    }

    void notify_failure(const std::string &error)
    {
        NotificationManager *notifications = wxGetApp().notification_manager();
        if (notifications == nullptr)
            return;
        notifications->push_notification(
            NotificationType::CustomNotification, NotificationManager::NotificationLevel::WarningNotificationLevel,
            _u8L("Settings were not synced to Home Assistant:") + " " + error, _u8L("Retry"),
            [](wxEvtHandler *) {
                SettingsSync::sync_now();
                return true;
            });
    }

    void notify_listener()
    {
        if (m_listener)
            m_listener();
    }

    wxTimer               m_debounce;
    wxTimer               m_interval;
    Status                m_status;
    std::function<void()> m_listener;
    std::uint64_t         m_generation = 0;
    bool                  m_installed  = false;
    bool                  m_shut_down  = false;
    bool                  m_in_flight  = false;
};

SyncService &SyncService::service()
{
    static SyncService *s_service = new SyncService(); // app-lifetime
    return *s_service;
}

SyncService &service() { return SyncService::service(); }

} // namespace

void install() { service().install(); }
void shutdown() { service().shutdown(); }
bool enabled() { return service().enabled(); }
void set_enabled(bool on) { service().set_enabled(on); }
void sync_now() { service().sync_now(); }
Status status() { return service().status(); }
void set_status_listener(std::function<void()> listener) { service().set_status_listener(std::move(listener)); }

} } } } // namespace Slic3r::GUI::HomeAssistant::SettingsSync
