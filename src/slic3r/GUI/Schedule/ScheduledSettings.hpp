#ifndef slic3r_GUI_Schedule_ScheduledSettings_hpp_
#define slic3r_GUI_Schedule_ScheduledSettings_hpp_

// Scheduled settings: the runtime half. Owns the rule document (stored as one
// JSON value in AppConfig under `scheduled_settings`, so every edit rides the
// ordinary AppConfig::save() and lands in the preferences history like any
// other settings change), the 60 s evaluation timer, the external-source
// cache and the base-settings snapshot that is restored when a rule ends.
//
// The evaluator never blocks the UI: Home Assistant reads go through the
// HomeAssistant client's owned worker and API reads through Http::perform()
// on a background thread; each answer is marshalled to the UI thread and
// checked against a per-rule generation counter so a slow, stale reply can
// never overwrite a newer one. A source that has not answered (or whose last
// answer failed) contributes nothing, so the base settings stay in force.

#include "ScheduledSettingsModel.hpp"

#include <wx/timer.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace Schedule {

inline constexpr const char *kDocumentConfigKey  = "scheduled_settings";
inline constexpr const char *kOverrideConfigKey  = "scheduled_settings_override";
inline constexpr int         kEvaluateIntervalSeconds = 60;

// What the panel shows beside each rule.
struct RuleStatus
{
    bool        calendar_matches = false; // enabled + weekday + window + dates
    bool        active           = false; // contributed values in the last evaluation
    std::string source_note;              // "Home Assistant: on", "API: HTTP 404", ...
    std::string owned_keys;               // keys this rule currently wins, comma separated
};

class Scheduler final : private wxEvtHandler
{
public:
    static Scheduler &instance();

    // Load the document from AppConfig, evaluate once and start the timer.
    // Call on the main thread after the config is loaded. Idempotent.
    void install();
    // Stop the timer and cancel outstanding source requests. Base settings are
    // deliberately left as they are on disk: the override bookkeeping is
    // persisted, so the next launch restores them when the rule has ended.
    void shutdown();

    const Document &document() const { return m_document; }
    // Replace the document, persist it (one AppConfig::save(), which is what
    // the preferences history records) and evaluate immediately. Returns
    // false with `error` filled when the document does not validate.
    bool save_document(const Document &document, std::string &error);

    // Evaluate now (activation, a user edit, the toast's Retry action).
    void evaluate_now();

    RuleStatus                     status(const std::string &rule_id) const;
    const std::vector<std::string> &active_rule_ids() const { return m_last_resolution.active_rule_ids; }
    // Keys currently under schedule control (for the Preferences rows and the
    // provenance line beside each affected setting).
    bool        is_overridden(const std::string &key) const { return m_override.applied.count(key) != 0; }
    std::string owning_rule_label(const std::string &key) const;
    std::string last_evaluated_text() const; // "Last checked 21:04:05" or "Never"

    // The panel subscribes to redraw after every evaluation.
    void set_change_listener(std::function<void()> listener) { m_listener = std::move(listener); }

    // Local wall-clock now, as the evaluator sees it. Exposed for the panel's
    // timezone note.
    static LocalInstant now();
    static std::string  timezone_name();

private:
    Scheduler();
    ~Scheduler() override;

    struct SourceEntry
    {
        GenerationGuard                       generation;
        SourceState                           state;
        std::string                           note;
        std::chrono::steady_clock::time_point fetched_at{};
        bool                                  in_flight = false;
        bool                                  failed    = false; // last answer was a failure (for toast edge)
    };

    void load_from_config();
    void persist_override();
    void on_timer(wxTimerEvent &);
    void refresh_sources(const LocalInstant &now);
    void request_home_assistant(const Rule &rule, SourceEntry &entry);
    void request_api(const Rule &rule, SourceEntry &entry);
    void on_source_answer(const std::string &rule_id, std::uint64_t generation, SourceState state, std::string note, bool failed);
    void resolve_and_apply(const LocalInstant &now);
    void apply_key(const std::string &key, const std::string &value);
    void notify_failure(const Rule &rule, const std::string &note);
    std::map<std::string, std::string> live_values() const;

    Document                           m_document;
    OverrideState                      m_override;
    Resolution                         m_last_resolution;
    std::map<std::string, SourceEntry> m_sources; // rule id -> cache
    wxTimer                            m_timer;
    std::function<void()>              m_listener;
    std::string                        m_last_evaluated;
    bool                               m_installed  = false;
    bool                               m_shut_down  = false;
    bool                               m_applying   = false;
};

} } } // namespace Slic3r::GUI::Schedule

#endif // slic3r_GUI_Schedule_ScheduledSettings_hpp_
