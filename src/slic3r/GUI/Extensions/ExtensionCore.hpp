#ifndef slic3r_GUI_Extensions_ExtensionCore_hpp_
#define slic3r_GUI_Extensions_ExtensionCore_hpp_

// Extensions for Bambu Studio, without wxWidgets: the manifest an extension
// folder carries, the hooks and permissions it can declare, the user's consent,
// the one JSON request an extension receives and the one JSON answer it may
// give back, and the record of runs. Tested on its own by
// tests/extension_core_test.cpp; the process runner is ExtensionProcess.

#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r { namespace GUI { namespace Extensions {

constexpr int         kApiVersion    = 1;
constexpr std::size_t kManifestLimit = 64 * 1024;

// ---------------------------------------------------------------- hooks
enum class Hook { ModelImported, PlateSliced, PrintBeforeSend, GcodeExported, PrintFinished, Command };

struct HookInfo
{
    Hook        hook;
    const char *id;       // manifest spelling
    const char *title;    // untranslated
    const char *receives; // untranslated
    const char *answers;  // untranslated
    bool        built;    // false: declared in the protocol but not run by this build yet
};
const std::vector<HookInfo> &hooks();
const HookInfo              *find_hook(std::string_view id);
const HookInfo              &info(Hook hook);

// ---------------------------------------------------------------- permissions
enum class Permission { SliceRead, ModelRead, PrinterRead, NotesShow, SendWarn, SendBlock, GcodeComment, FileSave };

struct PermissionInfo
{
    Permission  permission;
    const char *id;    // manifest spelling
    const char *title; // untranslated, says what it may receive or do
};
const std::vector<PermissionInfo> &permissions();
const PermissionInfo              *find_permission(std::string_view id);
const PermissionInfo              &info(Permission permission);

// ---------------------------------------------------------------- manifest
struct Action
{
    std::string id;
    std::string title;
};

struct Manifest
{
    std::string              id;          // reverse-DNS style: lower-case letters, digits, dots and dashes
    std::string              name;
    std::string              version;     // MAJOR.MINOR.PATCH
    int                      api_version = 0;
    std::string              description;
    std::string              run;         // the Windows command line, relative to the extension folder
    std::vector<std::string> argv;        // run, split with Windows quoting rules
    std::vector<Hook>        hooks;
    std::vector<Action>      actions;
    std::vector<Permission>  permissions; // sorted, unique
};

struct ManifestError
{
    std::string code;   // stable ASCII: not-json, too-large, missing, wrong-type, bad-id, bad-version, api-newer, api-older, unknown-hook, unknown-permission, unknown-field, too-long, empty-run, duplicate
    std::string field;  // the manifest field, when there is one
    std::string detail; // a value the person can act on, never a secret: the API version found, the unknown name
    bool        ok() const { return code.empty(); }
};

bool parse_manifest(std::string_view json, Manifest &out, ManifestError &error);
// Windows command-line splitting (CommandLineToArgvW rules for quotes and backslashes).
std::vector<std::string> split_command_line(std::string_view command);
// -1, 0 or 1, comparing MAJOR.MINOR.PATCH numerically.
int compare_versions(std::string_view a, std::string_view b);

// ---------------------------------------------------------------- consent and state
struct Consent
{
    std::string             version;          // the version the person answered for
    std::vector<Permission> allowed;          // exactly what they allowed, sorted
    bool                    enabled     = false;
    bool                    off_by_user = false;
    int                     failures_in_row = 0;
    bool                    off_after_failures = false;
};

enum class State {
    On,
    OffByUser,
    WaitingNew,             // never answered
    WaitingChanged,         // a new version or a changed permission list
    OffAfterFailures,
    ManifestError,
    AllOff,                 // the master switch is off
};

State                   state_for(const ManifestError &manifest_error, const Manifest &manifest, const Consent *consent, bool master_on);
std::vector<Permission> added_permissions(const Manifest &manifest, const Consent &consent);
// Records an answer to the consent prompt: allow exactly what the manifest asks for, or keep it off.
Consent                 answer_consent(const Manifest &manifest, const Consent *previous, bool allow);
bool                    allowed(const Consent &consent, Permission permission);

std::string serialize_consents(const std::map<std::string, Consent> &consents);
bool        parse_consents(std::string_view json, std::map<std::string, Consent> &out);

// ---------------------------------------------------------------- limits
struct Limits
{
    int         timeout_ms         = 10000;
    std::size_t max_answer         = 64 * 1024;
    int         off_after_failures = 3;
};
Limits clamp_limits(Limits limits);

// ---------------------------------------------------------------- request
// The app's context is a JSON object whose top-level sections need a
// permission: "slice" needs slice.read, "model" needs model.read, "printer"
// and "send" (where a print is going, with which options and filaments) need
// printer.read. Anything else is dropped. Values whose key looks like a secret
// are removed at every depth, whatever the permissions.
nlohmann::json build_request(Hook hook, const std::string &action, const nlohmann::json &context, const Manifest &manifest,
                             const Consent &consent);
bool           looks_sensitive(std::string_view key);

// ---------------------------------------------------------------- answer
struct ChecklistRow
{
    std::string state; // pass, warn or fail
    std::string text;
};

struct Answer
{
    std::vector<std::string>  notes;
    std::vector<ChecklistRow> checklist;
    bool                      block = false;
    std::string               block_reason;
    std::vector<std::string>  gcode_prepend;
    std::vector<std::string>  gcode_append;
    std::string               file_name;
    std::string               file_text;
};

// problem is stable ASCII naming the first rule broken: not-json, not-object,
// wrong-api, unknown-field, not-allowed:<permission>, wrong-hook:<field>,
// too-many:<field>, too-long:<field>, not-comment, bad-state, bad-file-name.
bool parse_answer(std::string_view json, Hook hook, const Manifest &manifest, const Consent &consent, Answer &out, std::string &problem);

// ---------------------------------------------------------------- runs
// Stopped: the person stopped the run, or Bambu Studio closed; it never counts as a failure.
enum class Outcome { Answered, TimedOut, TooLarge, NotJson, Invalid, Failed, NotStarted, Stopped };
const char *outcome_id(Outcome outcome);

struct RunRecord
{
    std::int64_t time = 0; // seconds since 1970-01-01 UTC
    std::string  extension;
    std::string  hook;     // hook id, or "command:<action>"
    std::string  subject;  // a plate or model name; never file contents
    double       seconds   = 0;
    std::size_t  answer_bytes = 0;
    Outcome      outcome   = Outcome::Answered;
    std::string  problem;
};

std::string to_line(const RunRecord &record);
bool        from_line(std::string_view line, RunRecord &record);

class RunLog
{
public:
    static constexpr std::size_t kLimit = 500;
    explicit RunLog(std::filesystem::path file = {}) : m_file(std::move(file)) {}
    void                          load();
    void                          append(const RunRecord &record);
    const std::vector<RunRecord> &records() const { return m_records; } // oldest first
    std::vector<RunRecord>        for_extension(const std::string &id) const;
    std::string                   export_json() const;

private:
    void                   save() const;
    std::filesystem::path  m_file;
    std::vector<RunRecord> m_records;
};

// Applies a run's outcome to the consent: failures in a row turn an extension off.
Consent after_run(Consent consent, Outcome outcome, const Limits &limits);

// ---------------------------------------------------------------- before a print is sent
// One extension's part in the check before a print is sent.
struct SendCheckRun
{
    std::string id;
    Outcome     outcome = Outcome::NotStarted;
    std::string problem;
    Answer      answer; // when outcome is Answered
};

// Continue: nothing to look at, sending carries on. Look: rows that warn or
// fail, notes, or a run without a valid answer, for the person to read before
// sending. Stop: a valid answer blocked the print.
enum class SendVerdict { Continue, Look, Stop };

struct SendSummary
{
    int                      passed = 0, warned = 0, failed = 0, notes = 0;
    int                      not_answered = 0; // ran but gave no valid answer
    int                      stopped      = 0; // stopped by the person: counts for nothing
    std::vector<std::size_t> blockers;          // runs whose answer blocked the print, in order
    SendVerdict              verdict = SendVerdict::Continue;
};

// Only a valid answer can block: one that broke a rule (a block without
// send.block among them) or never came is shown, and never holds a print back.
SendSummary summarise_send_check(const std::vector<SendCheckRun> &runs);

}}} // namespace Slic3r::GUI::Extensions

#endif // slic3r_GUI_Extensions_ExtensionCore_hpp_
