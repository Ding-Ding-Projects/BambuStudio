#include "ScheduledSettings.hpp"

#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/HomeAssistant.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "slic3r/GUI/NotificationManager.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/MD3Tokens.hpp"
#include "slic3r/Utils/Http.hpp"

#include "libslic3r/AppConfig.hpp"

#include <boost/log/trivial.hpp>

#include <wx/app.h>
#include <wx/colour.h>
#include <wx/datetime.h>
#include <wx/event.h>

#include <ctime>
#include <random>
#include <utility>

namespace Slic3r { namespace GUI { namespace Schedule {

namespace {

// External sources are re-read at most once per evaluation tick.
constexpr std::chrono::seconds kSourceMinimumAge{kEvaluateIntervalSeconds - 5};

std::string ha_error_note(HomeAssistant::EntityFetchErrorCode code, unsigned status)
{
    using HomeAssistant::EntityFetchErrorCode;
    switch (code) {
    case EntityFetchErrorCode::None: return {};
    case EntityFetchErrorCode::ShuttingDown: return _u8L("shutting down");
    case EntityFetchErrorCode::NotConfigured: return _u8L("Home Assistant is not connected (no URL or token in Smart home)");
    case EntityFetchErrorCode::InsecureTransport: return _u8L("Home Assistant URL must be HTTPS or localhost");
    case EntityFetchErrorCode::InvalidFilter: return _u8L("entity id must look like input_boolean.night_mode");
    case EntityFetchErrorCode::WorkerUnavailable:
    case EntityFetchErrorCode::QueueFull:
    case EntityFetchErrorCode::RequestSetupFailed: return _u8L("could not start the request");
    case EntityFetchErrorCode::TransportError: return _u8L("could not reach Home Assistant");
    case EntityFetchErrorCode::HttpStatus:
        return status == 404 ? _u8L("entity not found (HTTP 404)") : "HTTP " + std::to_string(status);
    case EntityFetchErrorCode::ResponseTooLarge: return _u8L("response too large");
    case EntityFetchErrorCode::InvalidResponse: return _u8L("response was not a Home Assistant state");
    }
    return _u8L("unknown error");
}

} // namespace

Scheduler &Scheduler::instance()
{
    static Scheduler *s_instance = new Scheduler(); // app-lifetime
    return *s_instance;
}

Scheduler::Scheduler()
{
    m_timer.SetOwner(this);
    Bind(wxEVT_TIMER, &Scheduler::on_timer, this);
}

Scheduler::~Scheduler() { shutdown(); }

LocalInstant Scheduler::now()
{
    const std::time_t t = std::time(nullptr);
    std::tm           local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    return LocalInstant::from_tm(local);
}

std::string Scheduler::timezone_name()
{
    // wx reports the offset in the user's zone, DST included, for the current
    // moment; the panel pairs it with the name from the C runtime.
    wxDateTime now = wxDateTime::Now();
    const long offset_minutes = wxDateTime::TimeZone(wxDateTime::Local).GetOffset() / 60 + (now.IsDST() ? 60 : 0);
    char buf[16];
    std::snprintf(buf, sizeof buf, "UTC%c%02ld:%02ld", offset_minutes < 0 ? '-' : '+', std::labs(offset_minutes) / 60, std::labs(offset_minutes) % 60);
    std::string name = buf;
    if (now.IsDST())
        name += " (" + _u8L("daylight saving time") + ")";
    return name;
}

void Scheduler::load_from_config()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr)
        return;
    ParseResult parsed = parse_document(cfg->get(kDocumentConfigKey));
    if (parsed.ok) {
        m_document = std::move(parsed.document);
    } else {
        // Keep the stored text untouched so nothing is lost; run with no rules
        // and tell the user in the panel.
        BOOST_LOG_TRIVIAL(warning) << "ScheduledSettings: stored schedule ignored: " << parsed.error;
        m_document = Document{};
    }
    m_override = override_state_from_json(cfg->get(kOverrideConfigKey));
}

void Scheduler::persist_override()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr)
        return;
    if (m_override.applied.empty() && m_override.base.empty())
        cfg->set(kOverrideConfigKey, "");
    else
        cfg->set(kOverrideConfigKey, override_state_to_json(m_override).dump());
}

void Scheduler::install()
{
    if (m_installed || m_shut_down)
        return;
    m_installed = true;
    load_from_config();
    evaluate_now();
    m_timer.Start(kEvaluateIntervalSeconds * 1000);
}

void Scheduler::shutdown()
{
    m_shut_down = true;
    m_timer.Stop();
    // In-flight answers are dropped by their generation check: bump every
    // rule's counter so nothing that lands during teardown is applied.
    for (auto &[id, entry] : m_sources)
        entry.generation.issue();
}

bool Scheduler::save_document(const Document &document, std::string &error)
{
    if (document.rules.size() > kMaxRules) {
        error = _u8L("Too many rules.");
        return false;
    }
    for (const Rule &rule : document.rules) {
        std::vector<std::string> problems = validate_rule(rule);
        if (rule.source == SourceKind::Api) {
            const std::string url_problem = validate_api_url(rule.source_url, rule.allow_loopback_http);
            if (!url_problem.empty())
                problems.push_back(url_problem);
        }
        if (!problems.empty()) {
            error = rule.label + ": " + problems.front();
            return false;
        }
    }
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr) {
        error = _u8L("Settings are not available yet.");
        return false;
    }
    // Rules that vanished or changed source drop their cached answers.
    for (auto it = m_sources.begin(); it != m_sources.end();) {
        const Rule *rule = document.find(it->first);
        const Rule *old  = m_document.find(it->first);
        const bool  keep = rule && old && rule->source == old->source && rule->source_url == old->source_url &&
                          rule->source_entity_id == old->source_entity_id && rule->allow_loopback_http == old->allow_loopback_http;
        if (keep) {
            ++it;
        } else {
            it->second.generation.issue();
            it = m_sources.erase(it);
        }
    }
    m_document = document;
    cfg->set(kDocumentConfigKey, serialize_document(m_document));
    // evaluate_now() saves once at the end (it may also write overrides), and
    // that single save is what PreferencesHistory snapshots.
    evaluate_now();
    return true;
}

void Scheduler::on_timer(wxTimerEvent &)
{
    if (m_shut_down)
        return;
    evaluate_now();
}

void Scheduler::evaluate_now()
{
    if (m_shut_down || wxGetApp().app_config == nullptr)
        return;
    const LocalInstant current = now();
    refresh_sources(current);
    resolve_and_apply(current);
}

void Scheduler::refresh_sources(const LocalInstant &current)
{
    const auto steady_now = std::chrono::steady_clock::now();
    for (const Rule &rule : m_document.rules) {
        if (rule.source == SourceKind::Local)
            continue;
        if (!rule_matches_calendar(rule, current)) {
            // Outside the window nothing is read; a later answer for an older
            // request is dropped by the generation bump.
            auto it = m_sources.find(rule.id);
            if (it != m_sources.end() && it->second.in_flight) {
                it->second.generation.issue();
                it->second.in_flight = false;
            }
            continue;
        }
        SourceEntry &entry = m_sources[rule.id];
        if (entry.in_flight)
            continue;
        if (entry.fetched_at != std::chrono::steady_clock::time_point{} && steady_now - entry.fetched_at < kSourceMinimumAge)
            continue;
        if (rule.source == SourceKind::HomeAssistant)
            request_home_assistant(rule, entry);
        else
            request_api(rule, entry);
    }
}

void Scheduler::request_home_assistant(const Rule &rule, SourceEntry &entry)
{
    const std::uint64_t generation = entry.generation.issue();
    entry.in_flight                = true;
    const std::string rule_id      = rule.id;
    HomeAssistant::fetch_entity_state(rule.source_entity_id, [this, rule_id, generation](HomeAssistant::EntityStateResult result) {
        SourceState state;
        std::string note;
        bool        failed = false;
        if (result) {
            const std::string &s = result.entity.state;
            if (s == "on") {
                state.kind = SourceState::Kind::Active;
                note       = _u8L("Home Assistant: on");
            } else if (s == "off") {
                state.kind = SourceState::Kind::Inactive;
                note       = _u8L("Home Assistant: off");
            } else {
                state.kind = SourceState::Kind::Unknown;
                note       = _u8L("Home Assistant: entity state is") + " \"" + s + "\"";
            }
        } else {
            state.kind = SourceState::Kind::Unknown;
            note       = _u8L("Home Assistant: ") + ha_error_note(result.error_code, result.http_status);
            failed     = result.error_code != HomeAssistant::EntityFetchErrorCode::ShuttingDown;
        }
        on_source_answer(rule_id, generation, std::move(state), std::move(note), failed);
    });
}

void Scheduler::request_api(const Rule &rule, SourceEntry &entry)
{
    const std::string url_problem = validate_api_url(rule.source_url, rule.allow_loopback_http);
    if (!url_problem.empty()) {
        entry.state.kind = SourceState::Kind::Unknown;
        entry.note       = _u8L("API: ") + url_problem;
        entry.fetched_at = std::chrono::steady_clock::now();
        return;
    }
    const std::uint64_t generation = entry.generation.issue();
    entry.in_flight                = true;
    const std::string rule_id      = rule.id;

    // Http::perform() runs on its own thread and calls back there; every
    // outcome is marshalled to the UI thread through CallAfter and then
    // checked against the generation.
    auto deliver = [this, rule_id, generation](SourceState state, std::string note, bool failed) {
        if (wxTheApp == nullptr)
            return;
        wxTheApp->CallAfter([this, rule_id, generation, state = std::move(state), note = std::move(note), failed]() mutable {
            on_source_answer(rule_id, generation, std::move(state), std::move(note), failed);
        });
    };
    try {
        auto http = Http::get(rule.source_url);
        http.follow_redirects(false)
            .verbose(false)
            .header("Accept", "application/json")
            .header("Accept-Encoding", "identity")
            .timeout_max(kApiTimeoutSeconds)
            .size_limit(kMaxApiResponseBytes)
            .on_complete([deliver](std::string body, unsigned status) {
                if (status < 200 || status >= 300) {
                    // follow_redirects(false) surfaces a 3xx here: refused on purpose.
                    deliver(SourceState{}, _u8L("API: ") + (status >= 300 && status < 400 ? _u8L("redirect refused") : "HTTP " + std::to_string(status)), true);
                    return;
                }
                ApiResponse parsed = parse_api_response(body);
                if (!parsed.ok) {
                    deliver(SourceState{}, _u8L("API: ") + parsed.error, true);
                    return;
                }
                SourceState state;
                state.kind   = SourceState::Kind::Active;
                state.values = std::move(parsed.values);
                deliver(std::move(state), _u8L("API: ok"), false);
            })
            .on_error([deliver](std::string, std::string error, unsigned status) {
                std::string note = _u8L("API: ");
                if (status != 0)
                    note += "HTTP " + std::to_string(status);
                else if (error.rfind("HTTP body data size exceeded limit", 0) == 0)
                    note += _u8L("response too large");
                else
                    note += _u8L("could not reach the server");
                deliver(SourceState{}, std::move(note), true);
            })
            .perform();
    } catch (...) {
        entry.in_flight = false;
        entry.state     = SourceState{};
        entry.note      = _u8L("API: could not start the request");
        entry.fetched_at = std::chrono::steady_clock::now();
    }
}

void Scheduler::on_source_answer(const std::string &rule_id, std::uint64_t generation, SourceState state, std::string note, bool failed)
{
    if (m_shut_down)
        return;
    auto it = m_sources.find(rule_id);
    if (it == m_sources.end() || !it->second.generation.is_current(generation))
        return; // stale: a newer request was issued, or the rule went away
    SourceEntry &entry = it->second;
    entry.in_flight    = false;
    entry.fetched_at   = std::chrono::steady_clock::now();
    entry.state        = std::move(state);
    entry.note         = std::move(note);
    const bool was_failed = entry.failed;
    entry.failed          = failed;
    if (failed && !was_failed) {
        if (const Rule *rule = m_document.find(rule_id))
            notify_failure(*rule, entry.note);
    }
    resolve_and_apply(now());
}

void Scheduler::notify_failure(const Rule &rule, const std::string &note)
{
    NotificationManager *notifications = wxGetApp().notification_manager();
    if (notifications == nullptr)
        return;
    const std::string text = _u8L("Scheduled settings: rule") + " \"" + rule.label + "\" " + _u8L("could not read its source.") + "\n" + note + "\n" +
                             _u8L("Your own settings stay in force until it answers.");
    notifications->push_notification(
        NotificationType::CustomNotification, NotificationManager::NotificationLevel::WarningNotificationLevel, text, _u8L("Retry"),
        [](wxEvtHandler *) {
            Scheduler::instance().evaluate_now();
            return true;
        });
}

std::map<std::string, std::string> Scheduler::live_values() const
{
    std::map<std::string, std::string> live;
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr)
        return live;
    for (const std::string &key : allowed_keys())
        live[key] = cfg->get(key);
    return live;
}

void Scheduler::resolve_and_apply(const LocalInstant &current)
{
    if (m_applying)
        return;
    m_applying = true;
    AppConfig *cfg = wxGetApp().app_config;
    m_last_resolution = resolve(m_document, current, [this](const Rule &rule) {
        auto it = m_sources.find(rule.id);
        return it == m_sources.end() ? SourceState{} : it->second.state;
    });
    Transition transition = plan_transition(m_override, m_last_resolution, live_values());
    m_override            = transition.next;
    for (const auto &[key, value] : transition.writes)
        apply_key(key, value);
    persist_override();
    if (cfg != nullptr && (cfg->dirty() || !transition.writes.empty()))
        cfg->save();
    {
        char buf[16];
        std::snprintf(buf, sizeof buf, "%02d:%02d", current.time.hour, current.time.minute);
        m_last_evaluated = buf;
    }
    m_applying = false;
    if (m_listener)
        m_listener();
}

void Scheduler::apply_key(const std::string &key, const std::string &value)
{
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr)
        return;
    if (key == kKeyDisplayName) {
        // set_app_display_name sanitizes, persists and broadcasts; "" resets.
        wxGetApp().set_app_display_name(value);
        return;
    }
    cfg->set(key, value);
    if (key == kKeyTheme) {
        wxGetApp().Update_dark_mode_flag();
#ifdef _MSW_DARK_MODE
        wxGetApp().force_colors_update();
        wxGetApp().update_ui_from_settings();
#endif
    } else if (key == kKeyDensity) {
        MD3::Metrics::setDensity(value == "compact" ? MD3::Metrics::Density::Compact : MD3::Metrics::Density::Comfortable);
    } else if (key == kKeyAccentSeed) {
        wxColour seed(wxString::FromUTF8(value.empty() ? "#146c2e" : value));
        if (!seed.IsOk())
            seed = wxColour(wxString::FromUTF8("#146c2e"));
        MD3::setAccentSeed(seed);
#ifdef _MSW_DARK_MODE
        wxGetApp().force_colors_update();
        wxGetApp().update_ui_from_settings();
#endif
    } else if (key == kKeyFontFamily || key == kKeyFontScale) {
        ::Label::rebuild_fonts(I18N::language_mode_profile().font_language);
    } else if (key == kKeyFunnyLevelEnglish) {
        I18N::language_mode_service().set_funny_level(I18N::FunnyLanguage::English, I18N::parse_funny_level(value));
    } else if (key == kKeyFunnyLevelCantonese) {
        I18N::language_mode_service().set_funny_level(I18N::FunnyLanguage::Cantonese, I18N::parse_funny_level(value));
    } else if (key == kKeyLanguageMode) {
        // Catalogs reload for windows built from now on; the running frame
        // keeps its current strings until the next launch (the same
        // restart-scoped behaviour the language picker documents).
        wxGetApp().load_language(wxString::FromUTF8(value), false);
    }
    if (wxGetApp().plater()) {
        SimpleEvent evt(EVT_GLCANVAS_COLOR_MODE_CHANGED);
        wxPostEvent(wxGetApp().plater(), evt);
    }
    if (wxGetApp().mainframe)
        wxGetApp().mainframe->Refresh();
}

RuleStatus Scheduler::status(const std::string &rule_id) const
{
    RuleStatus s;
    const Rule *rule = m_document.find(rule_id);
    if (rule == nullptr)
        return s;
    s.calendar_matches = rule_matches_calendar(*rule, now());
    for (const std::string &id : m_last_resolution.active_rule_ids)
        if (id == rule_id) s.active = true;
    if (rule->source != SourceKind::Local) {
        auto it = m_sources.find(rule_id);
        if (it != m_sources.end() && !it->second.note.empty())
            s.source_note = it->second.note;
        else if (!s.calendar_matches)
            s.source_note = _u8L("source is read only inside the time window");
        else
            s.source_note = _u8L("waiting for the source to answer");
    }
    for (const auto &[key, owner] : m_last_resolution.winning_rule) {
        if (owner != rule_id) continue;
        if (!s.owned_keys.empty()) s.owned_keys += ", ";
        s.owned_keys += key;
    }
    return s;
}

std::string Scheduler::owning_rule_label(const std::string &key) const
{
    auto it = m_last_resolution.winning_rule.find(key);
    if (it == m_last_resolution.winning_rule.end())
        return {};
    const Rule *rule = m_document.find(it->second);
    return rule ? rule->label : it->second;
}

std::string Scheduler::last_evaluated_text() const
{
    return m_last_evaluated.empty() ? _u8L("Not checked yet") : _u8L("Last checked at") + " " + m_last_evaluated;
}

} } } // namespace Slic3r::GUI::Schedule
