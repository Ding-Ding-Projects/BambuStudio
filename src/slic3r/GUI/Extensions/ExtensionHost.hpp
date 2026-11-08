#ifndef slic3r_GUI_Extensions_ExtensionHost_hpp_
#define slic3r_GUI_Extensions_ExtensionHost_hpp_

// The installed extensions and everything kept about them, without wxWidgets:
// discovery of extension folders, the built-in extensions (installed, and
// updated only while you have not changed them), consent, the master switch,
// the limits, adding a folder, and running a hook. Running is split so a
// worker thread can do the slow part: plan() and record() run on the UI
// thread, execute() anywhere.

#include "ExtensionCore.hpp"
#include "ExtensionProcess.hpp"

#include <atomic>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI { namespace Extensions {

struct Installed
{
    std::filesystem::path folder;
    Manifest              manifest;
    ManifestError         error;
    bool                  builtin  = false; // came with Bambu Studio
    bool                  modified = false; // a built-in whose files you changed: updates are paused
};

struct PlannedRun
{
    std::string           id;
    std::filesystem::path folder;
    Manifest              manifest;
    Consent               consent;
    Limits                limits;
    Hook                  hook = Hook::Command;
    std::string           action;
    nlohmann::json        settings = nlohmann::json::object(); // the values stored for its settings
};

struct RunResult
{
    PlannedRun plan;
    Outcome    outcome = Outcome::Failed;
    Answer     answer;
    std::string problem;
    RunRecord  record;
};

// One run, start to finish: request, process, answer validation, record.
// cancel, when set during the run, stops it with Outcome::Stopped.
RunResult execute(const PlannedRun &plan, const nlohmann::json &context, const std::string &subject, std::int64_t now,
                  const std::atomic<bool> *cancel = nullptr);

// SHA-256 of every regular file under folder, by relative path (forward slashes).
std::map<std::string, std::string> folder_hashes(const std::filesystem::path &folder);

class Host
{
public:
    // root: the extension folders. state: consent, settings and runs.
    // builtin: the folders that come with Bambu Studio (may be empty).
    Host(std::filesystem::path root, std::filesystem::path state, std::filesystem::path builtin);

    // Installs or updates the built-ins, then reads every folder and the state.
    void load();
    const std::vector<Installed> &extensions() const { return m_extensions; }
    const Installed              *find(const std::string &id) const;
    State                         state(const Installed &extension) const;
    const Consent                *consent(const std::string &id) const;

    // The consent prompt's two answers.
    void allow(const std::string &id);
    void keep_off(const std::string &id);
    // On or off from the list. Turning on needs the prompt when the version or
    // the permission list changed; returns false then and changes nothing.
    bool set_enabled(const std::string &id, bool on);

    bool   master_on() const { return m_master_on; }
    void   set_master(bool on);
    Limits limits() const { return m_limits; }
    void   set_limits(Limits limits);

    // Removing: a built-in stays removed (it is not installed again).
    bool remove(const std::string &id, std::string &error);

    // Adding a folder: inspect first, then install (replacing only when asked).
    enum class Inspection { New, NewerVersion, OlderVersion, SameVersion, Invalid };
    Inspection inspect(const std::filesystem::path &from, Manifest &incoming, ManifestError &error) const;
    bool       install(const std::filesystem::path &from, std::string &error);

    std::vector<PlannedRun> plan(Hook hook) const;
    std::optional<PlannedRun> plan_action(const std::string &id, const std::string &action) const;
    // One extension, one hook it declares, whether or not this build runs that
    // hook yet: for "Try with a sample". Commands take the action to run.
    std::optional<PlannedRun> plan_sample(const std::string &id, Hook hook, const std::string &action = std::string()) const;
    // Applies a finished run: failures in a row, the run record.
    void record(const RunResult &result);

    // An extension's settings (File > Extensions, Settings): every declared
    // setting with its value, defaults where nothing valid is stored.
    nlohmann::json setting_values(const std::string &id) const;
    // Stores one value; false with the reason (valid_setting_value's, or
    // not-installed, unknown-setting) and nothing stored when it is not valid.
    bool set_setting(const std::string &id, const std::string &setting, const nlohmann::json &value, std::string &problem);
    // Back to the defaults the manifest declares.
    void reset_settings(const std::string &id);
    RunLog       &runs() { return m_runs; }
    const RunLog &runs() const { return m_runs; }

    const std::filesystem::path &root() const { return m_root; }

private:
    void install_builtins();
    void save_consents() const;
    void save_settings() const;
    void load_settings();
    void save_values() const;
    nlohmann::json stored_values(const std::string &id) const;

    std::filesystem::path          m_root, m_state, m_builtin;
    std::vector<Installed>         m_extensions;
    std::map<std::string, Consent> m_consents;
    std::map<std::string, nlohmann::json> m_values; // extension-settings.json
    std::vector<std::string>       m_removed_builtins;
    bool                           m_master_on = true;
    Limits                         m_limits;
    RunLog                         m_runs;
};

}}} // namespace Slic3r::GUI::Extensions

#endif // slic3r_GUI_Extensions_ExtensionHost_hpp_
