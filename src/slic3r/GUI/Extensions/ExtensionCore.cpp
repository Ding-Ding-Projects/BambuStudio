#include "ExtensionCore.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <set>
#include <system_error>

#ifndef L
#define L(s) s
#endif

namespace Slic3r { namespace GUI { namespace Extensions {

namespace {

using nlohmann::json;

std::size_t code_points(std::string_view s)
{
    std::size_t n = 0;
    for (const char c : s)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    return n;
}

bool has_control(std::string_view s)
{
    return std::any_of(s.begin(), s.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; });
}

std::string lower(std::string_view s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return out;
}

bool valid_id(std::string_view id)
{
    if (id.size() < 3 || id.size() > 100 || id.find('.') == std::string_view::npos) return false;
    char previous = '.';
    for (const char c : id) {
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        const bool sep   = c == '.' || c == '-';
        if (!alnum && !sep) return false;
        if (sep && (previous == '.' || previous == '-')) return false;
        previous = c;
    }
    return previous != '.' && previous != '-';
}

bool parse_version(std::string_view v, int (&parts)[3])
{
    int         index = 0;
    std::size_t digits = 0;
    parts[0] = parts[1] = parts[2] = 0;
    for (const char c : v) {
        if (c == '.') {
            if (digits == 0 || ++index > 2) return false;
            digits = 0;
        } else if (c >= '0' && c <= '9') {
            if (++digits > 5) return false;
            parts[index] = parts[index] * 10 + (c - '0');
        } else {
            return false;
        }
    }
    return index == 2 && digits > 0;
}

bool valid_action_id(std::string_view id)
{
    if (id.empty() || id.size() > 40) return false;
    return std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}

ManifestError fail(const char *code, const char *field, std::string detail = {})
{
    return ManifestError{code, field ? field : "", std::move(detail)};
}

std::vector<Permission> sorted_unique(std::vector<Permission> list)
{
    std::sort(list.begin(), list.end());
    list.erase(std::unique(list.begin(), list.end()), list.end());
    return list;
}

json scrub(const json &value)
{
    if (value.is_object()) {
        json out = json::object();
        for (auto it = value.begin(); it != value.end(); ++it)
            if (!looks_sensitive(it.key())) out[it.key()] = scrub(it.value());
        return out;
    }
    if (value.is_array()) {
        json out = json::array();
        for (const json &item : value) out.push_back(scrub(item));
        return out;
    }
    return value;
}

bool string_list(const json &value, std::size_t max_items, std::size_t max_chars, std::vector<std::string> &out, std::string &problem,
                 const char *field)
{
    if (!value.is_array()) {
        problem = std::string("wrong-type:") + field;
        return false;
    }
    if (value.size() > max_items) {
        problem = std::string("too-many:") + field;
        return false;
    }
    for (const json &item : value) {
        if (!item.is_string()) {
            problem = std::string("wrong-type:") + field;
            return false;
        }
        const std::string text = item.get<std::string>();
        if (code_points(text) > max_chars) {
            problem = std::string("too-long:") + field;
            return false;
        }
        out.push_back(text);
    }
    return true;
}

bool needs(const Consent &consent, Permission permission, std::string &problem)
{
    if (allowed(consent, permission)) return true;
    problem = std::string("not-allowed:") + info(permission).id;
    return false;
}

bool safe_file_name(std::string_view name)
{
    if (name.empty() || code_points(name) > 80) return false;
    if (name.find_first_of("\\/:*?\"<>|") != std::string_view::npos || has_control(name)) return false;
    if (name.front() == '.' || name.back() == '.' || name.back() == ' ') return false;
    const std::string l = lower(name);
    for (const char *ext : {".txt", ".md", ".json", ".csv"}) {
        const std::size_t n = std::char_traits<char>::length(ext);
        if (l.size() > n && l.compare(l.size() - n, n, ext) == 0) return true;
    }
    return false;
}

} // namespace

// ---------------------------------------------------------------- hooks

const std::vector<HookInfo> &hooks()
{
    static const std::vector<HookInfo> all = {
        {Hook::ModelImported, "model.imported", L("After a model is imported"), L("Name, triangles, size and units"), L("Notes"), true},
        {Hook::PlateSliced, "plate.sliced", L("After slicing"), L("Time, filament per slot, layers and warnings"), L("Notes"), true},
        {Hook::PrintBeforeSend, "print.before_send", L("Before sending a print"), L("Plate, printer model, filaments and options"),
         L("Checklist rows, a block with its reason"), true},
        {Hook::GcodeExported, "gcode.exported", L("After G-code export"), L("The file name and the slice statistics"),
         L("Comment lines to add at the start or end"), true},
        {Hook::PrintFinished, "print.finished", L("After a print finishes"), L("Result, duration and filament used"), L("Notes"), false},
        {Hook::Command, "command", L("Its own commands"), L("A summary of the selection"), L("Notes, a text file to save"), true},
    };
    return all;
}

const HookInfo *find_hook(std::string_view id)
{
    for (const HookInfo &h : hooks())
        if (id == h.id) return &h;
    return nullptr;
}

const HookInfo &info(Hook hook)
{
    for (const HookInfo &h : hooks())
        if (h.hook == hook) return h;
    return hooks().front();
}

// ---------------------------------------------------------------- permissions

const std::vector<PermissionInfo> &permissions()
{
    static const std::vector<PermissionInfo> all = {
        {Permission::SliceRead, "slice.read", L("Read slice statistics (time, grams, metres per plate)")},
        {Permission::ModelRead, "model.read", L("Read model names, sizes and triangle counts")},
        {Permission::PrinterRead, "printer.read", L("Read the printer preset, model, nozzle sizes and filament presets")},
        {Permission::NotesShow, "notes.show", L("Show notes in Bambu Studio")},
        {Permission::SendWarn, "send.warn", L("Add checklist rows before a print is sent")},
        {Permission::SendBlock, "send.block", L("Stop a print from being sent, with a reason")},
        {Permission::GcodeComment, "gcode.comment", L("Add comment lines to exported G-code")},
        {Permission::FileSave, "file.save", L("Offer a text file for you to save")},
    };
    return all;
}

const PermissionInfo *find_permission(std::string_view id)
{
    for (const PermissionInfo &p : permissions())
        if (id == p.id) return &p;
    return nullptr;
}

const PermissionInfo &info(Permission permission)
{
    for (const PermissionInfo &p : permissions())
        if (p.permission == permission) return p;
    return permissions().front();
}

// ---------------------------------------------------------------- manifest

std::vector<std::string> split_command_line(std::string_view command)
{
    std::vector<std::string> args;
    std::string              current;
    bool                     in_quotes = false, have = false;
    std::size_t              i = 0;
    while (i < command.size()) {
        const char c = command[i];
        if (c == '\\') {
            std::size_t slashes = 0;
            while (i < command.size() && command[i] == '\\') ++slashes, ++i;
            if (i < command.size() && command[i] == '"') {
                current.append(slashes / 2, '\\');
                if (slashes % 2 == 1) {
                    current += '"';
                    ++i;
                }
            } else {
                current.append(slashes, '\\');
            }
            have = true;
            continue;
        }
        if (c == '"') {
            if (in_quotes && i + 1 < command.size() && command[i + 1] == '"') { // "" inside quotes is a literal quote
                current += '"';
                i += 2;
                continue;
            }
            in_quotes = !in_quotes;
            have      = true;
            ++i;
            continue;
        }
        if (!in_quotes && (c == ' ' || c == '\t')) {
            if (have) args.push_back(current);
            current.clear();
            have = false;
            ++i;
            continue;
        }
        current += c;
        have = true;
        ++i;
    }
    if (have) args.push_back(current);
    return args;
}

int compare_versions(std::string_view a, std::string_view b)
{
    int pa[3], pb[3];
    if (!parse_version(a, pa) || !parse_version(b, pb)) return a < b ? -1 : a > b ? 1 : 0;
    for (int i = 0; i < 3; ++i)
        if (pa[i] != pb[i]) return pa[i] < pb[i] ? -1 : 1;
    return 0;
}

bool parse_manifest(std::string_view text, Manifest &out, ManifestError &error)
{
    error = {};
    if (text.size() > kManifestLimit) {
        error = fail("too-large", nullptr);
        return false;
    }
    const json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded()) {
        error = fail("not-json", nullptr);
        return false;
    }
    if (!j.is_object()) {
        error = fail("wrong-type", nullptr);
        return false;
    }
    static const std::set<std::string> known = {"id", "name", "version", "apiVersion", "description", "author", "run",
                                                "hooks", "actions", "permissions"};
    for (auto it = j.begin(); it != j.end(); ++it)
        if (!known.count(it.key())) {
            error = fail("unknown-field", nullptr, it.key().substr(0, 40));
            return false;
        }

    Manifest m;
    const auto string_field = [&](const char *field, bool required, std::size_t max, std::string &target) {
        if (!j.contains(field)) {
            if (required) error = fail("missing", field);
            return !required;
        }
        if (!j[field].is_string()) {
            error = fail("wrong-type", field);
            return false;
        }
        target = j[field].get<std::string>();
        if (code_points(target) > max) {
            error = fail("too-long", field);
            return false;
        }
        if (has_control(target)) {
            error = fail("wrong-type", field);
            return false;
        }
        return true;
    };
    std::string author;
    if (!string_field("id", true, 100, m.id) || !string_field("name", true, 80, m.name) || !string_field("version", true, 17, m.version) ||
        !string_field("description", false, 500, m.description) || !string_field("author", false, 80, author))
        return false;
    if (!valid_id(m.id)) {
        error = fail("bad-id", "id");
        return false;
    }
    if (m.name.empty()) {
        error = fail("missing", "name");
        return false;
    }
    int parts[3];
    if (!parse_version(m.version, parts)) {
        error = fail("bad-version", "version");
        return false;
    }

    if (!j.contains("apiVersion")) {
        error = fail("missing", "apiVersion");
        return false;
    }
    if (!j["apiVersion"].is_number_integer()) {
        error = fail("wrong-type", "apiVersion");
        return false;
    }
    m.api_version = j["apiVersion"].get<int>();
    if (m.api_version > kApiVersion) {
        error = fail("api-newer", "apiVersion", std::to_string(m.api_version));
        return false;
    }
    if (m.api_version < 1) {
        error = fail("api-older", "apiVersion", std::to_string(m.api_version));
        return false;
    }

    if (!j.contains("run")) {
        error = fail("missing", "run");
        return false;
    }
    if (!j["run"].is_object()) {
        error = fail("wrong-type", "run");
        return false;
    }
    for (auto it = j["run"].begin(); it != j["run"].end(); ++it)
        if (!it.value().is_string()) {
            error = fail("wrong-type", "run");
            return false;
        }
    if (!j["run"].contains("windows")) {
        error = fail("missing", "run.windows");
        return false;
    }
    m.run = j["run"]["windows"].get<std::string>();
    if (code_points(m.run) > 1000) {
        error = fail("too-long", "run.windows");
        return false;
    }
    m.argv = split_command_line(m.run);
    if (m.argv.empty() || m.argv.front().empty()) {
        error = fail("empty-run", "run.windows");
        return false;
    }

    if (j.contains("hooks")) {
        if (!j["hooks"].is_array()) {
            error = fail("wrong-type", "hooks");
            return false;
        }
        for (const json &h : j["hooks"]) {
            if (!h.is_string()) {
                error = fail("wrong-type", "hooks");
                return false;
            }
            const HookInfo *found = find_hook(h.get<std::string>());
            if (!found) {
                error = fail("unknown-hook", "hooks", h.get<std::string>().substr(0, 40));
                return false;
            }
            if (std::find(m.hooks.begin(), m.hooks.end(), found->hook) != m.hooks.end()) {
                error = fail("duplicate", "hooks", found->id);
                return false;
            }
            m.hooks.push_back(found->hook);
        }
    }
    if (j.contains("actions")) {
        if (!j["actions"].is_array() || j["actions"].size() > 10) {
            error = fail(j["actions"].is_array() ? "too-long" : "wrong-type", "actions");
            return false;
        }
        for (const json &a : j["actions"]) {
            if (!a.is_object() || !a.contains("id") || !a.contains("title") || !a["id"].is_string() || !a["title"].is_string() ||
                a.size() != 2) {
                error = fail("wrong-type", "actions");
                return false;
            }
            Action action{a["id"].get<std::string>(), a["title"].get<std::string>()};
            if (!valid_action_id(action.id) || action.title.empty() || code_points(action.title) > 60 || has_control(action.title)) {
                error = fail("wrong-type", "actions", action.id.substr(0, 40));
                return false;
            }
            for (const Action &other : m.actions)
                if (other.id == action.id) {
                    error = fail("duplicate", "actions", action.id);
                    return false;
                }
            m.actions.push_back(action);
        }
        if (!m.actions.empty() && std::find(m.hooks.begin(), m.hooks.end(), Hook::Command) == m.hooks.end())
            m.hooks.push_back(Hook::Command);
    }
    if (m.hooks.empty()) {
        error = fail("missing", "hooks");
        return false;
    }
    if (j.contains("permissions")) {
        if (!j["permissions"].is_array()) {
            error = fail("wrong-type", "permissions");
            return false;
        }
        for (const json &p : j["permissions"]) {
            if (!p.is_string()) {
                error = fail("wrong-type", "permissions");
                return false;
            }
            const PermissionInfo *found = find_permission(p.get<std::string>());
            if (!found) {
                error = fail("unknown-permission", "permissions", p.get<std::string>().substr(0, 40));
                return false;
            }
            if (std::find(m.permissions.begin(), m.permissions.end(), found->permission) != m.permissions.end()) {
                error = fail("duplicate", "permissions", found->id);
                return false;
            }
            m.permissions.push_back(found->permission);
        }
    }
    m.permissions = sorted_unique(m.permissions);
    out           = std::move(m);
    return true;
}

// ---------------------------------------------------------------- consent

bool allowed(const Consent &consent, Permission permission)
{
    return std::find(consent.allowed.begin(), consent.allowed.end(), permission) != consent.allowed.end();
}

std::vector<Permission> added_permissions(const Manifest &manifest, const Consent &consent)
{
    std::vector<Permission> added;
    for (Permission p : manifest.permissions)
        if (!allowed(consent, p)) added.push_back(p);
    return added;
}

State state_for(const ManifestError &manifest_error, const Manifest &manifest, const Consent *consent, bool master_on)
{
    if (!manifest_error.ok()) return State::ManifestError;
    if (!master_on) return State::AllOff;
    if (consent == nullptr) return State::WaitingNew;
    if (consent->off_by_user && consent->version == manifest.version) return State::OffByUser;
    if (consent->version != manifest.version || sorted_unique(consent->allowed) != manifest.permissions)
        return consent->version.empty() ? State::WaitingNew : State::WaitingChanged;
    if (consent->off_after_failures) return State::OffAfterFailures;
    if (!consent->enabled) return State::OffByUser;
    return State::On;
}

Consent answer_consent(const Manifest &manifest, const Consent *previous, bool allow)
{
    Consent c;
    c.version = manifest.version;
    if (allow) {
        c.allowed = manifest.permissions;
        c.enabled = true;
    } else {
        c.off_by_user = true;
        if (previous) c.allowed = previous->allowed;
    }
    return c;
}

std::string serialize_consents(const std::map<std::string, Consent> &consents)
{
    json out = json::object();
    for (const auto &[id, c] : consents) {
        json allowed_ids = json::array();
        for (Permission p : c.allowed) allowed_ids.push_back(info(p).id);
        out[id] = json{{"version", c.version},
                       {"allowed", allowed_ids},
                       {"enabled", c.enabled},
                       {"offByUser", c.off_by_user},
                       {"failuresInRow", c.failures_in_row},
                       {"offAfterFailures", c.off_after_failures}};
    }
    return out.dump(2);
}

bool parse_consents(std::string_view text, std::map<std::string, Consent> &out)
{
    const json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) return false;
    std::map<std::string, Consent> parsed;
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (!valid_id(it.key()) || !it.value().is_object()) continue;
        const json &v = it.value();
        Consent     c;
        if (v.contains("version") && v["version"].is_string()) c.version = v["version"].get<std::string>();
        if (v.contains("allowed") && v["allowed"].is_array())
            for (const json &p : v["allowed"])
                if (p.is_string())
                    if (const PermissionInfo *found = find_permission(p.get<std::string>())) c.allowed.push_back(found->permission);
        c.allowed = sorted_unique(c.allowed);
        if (v.contains("enabled") && v["enabled"].is_boolean()) c.enabled = v["enabled"].get<bool>();
        if (v.contains("offByUser") && v["offByUser"].is_boolean()) c.off_by_user = v["offByUser"].get<bool>();
        if (v.contains("failuresInRow") && v["failuresInRow"].is_number_integer()) c.failures_in_row = std::max(0, v["failuresInRow"].get<int>());
        if (v.contains("offAfterFailures") && v["offAfterFailures"].is_boolean()) c.off_after_failures = v["offAfterFailures"].get<bool>();
        parsed[it.key()] = c;
    }
    out = std::move(parsed);
    return true;
}

Limits clamp_limits(Limits limits)
{
    limits.timeout_ms         = std::clamp(limits.timeout_ms, 1000, 60000);
    limits.max_answer         = std::clamp<std::size_t>(limits.max_answer, 1024, 1024 * 1024);
    limits.off_after_failures = std::clamp(limits.off_after_failures, 0, 20);
    return limits;
}

// ---------------------------------------------------------------- request

bool looks_sensitive(std::string_view key)
{
    const std::string k = lower(key);
    static const char *needles[] = {"password", "passwd", "secret", "token", "access_code", "accesscode", "credential",
                                    "api_key", "apikey", "cookie", "private_key", "authorization", "session_id", "pin_code"};
    for (const char *n : needles)
        if (k.find(n) != std::string::npos) return true;
    return false;
}

json build_request(Hook hook, const std::string &action, const json &context, const Manifest &manifest, const Consent &consent)
{
    json filtered = json::object();
    if (context.is_object()) {
        const std::pair<const char *, Permission> sections[] = {{"slice", Permission::SliceRead},
                                                                {"model", Permission::ModelRead},
                                                                {"printer", Permission::PrinterRead},
                                                                {"send", Permission::PrinterRead}};
        for (const auto &[section, permission] : sections)
            if (context.contains(section) && allowed(consent, permission)) filtered[section] = scrub(context[section]);
    }
    return json{{"apiVersion", kApiVersion},
                {"hook", info(hook).id},
                {"action", action},
                {"extension", json{{"id", manifest.id}, {"version", manifest.version}}},
                {"context", filtered}};
}

// ---------------------------------------------------------------- answer

bool parse_answer(std::string_view text, Hook hook, const Manifest &manifest, const Consent &consent, Answer &out, std::string &problem)
{
    (void) manifest;
    problem.clear();
    const json j = json::parse(text.begin(), text.end(), nullptr, false);
    if (j.is_discarded()) {
        problem = "not-json";
        return false;
    }
    if (!j.is_object()) {
        problem = "not-object";
        return false;
    }
    Answer a;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const std::string &key = it.key();
        const json        &v   = it.value();
        if (key == "apiVersion") {
            if (!v.is_number_integer() || v.get<int>() != kApiVersion) {
                problem = "wrong-api";
                return false;
            }
        } else if (key == "notes") {
            if (!needs(consent, Permission::NotesShow, problem) || !string_list(v, 10, 300, a.notes, problem, "notes")) return false;
        } else if (key == "checklist") {
            if (hook != Hook::PrintBeforeSend) {
                problem = "wrong-hook:checklist";
                return false;
            }
            if (!needs(consent, Permission::SendWarn, problem)) return false;
            if (!v.is_array() || v.size() > 20) {
                problem = v.is_array() ? "too-many:checklist" : "wrong-type:checklist";
                return false;
            }
            for (const json &row : v) {
                if (!row.is_object() || !row.contains("state") || !row.contains("text") || !row["state"].is_string() ||
                    !row["text"].is_string() || row.size() != 2) {
                    problem = "wrong-type:checklist";
                    return false;
                }
                ChecklistRow r{row["state"].get<std::string>(), row["text"].get<std::string>()};
                if (r.state != "pass" && r.state != "warn" && r.state != "fail") {
                    problem = "bad-state";
                    return false;
                }
                if (code_points(r.text) > 200 || r.text.empty()) {
                    problem = "too-long:checklist";
                    return false;
                }
                a.checklist.push_back(r);
            }
        } else if (key == "block") {
            if (hook != Hook::PrintBeforeSend) {
                problem = "wrong-hook:block";
                return false;
            }
            if (!v.is_boolean()) {
                problem = "wrong-type:block";
                return false;
            }
            a.block = v.get<bool>();
            if (a.block && !needs(consent, Permission::SendBlock, problem)) return false;
        } else if (key == "blockReason") {
            if (!v.is_string() || code_points(v.get<std::string>()) > 300) {
                problem = "too-long:blockReason";
                return false;
            }
            a.block_reason = v.get<std::string>();
        } else if (key == "gcode") {
            if (hook != Hook::GcodeExported) {
                problem = "wrong-hook:gcode";
                return false;
            }
            if (!needs(consent, Permission::GcodeComment, problem)) return false;
            if (!v.is_object()) {
                problem = "wrong-type:gcode";
                return false;
            }
            for (auto g = v.begin(); g != v.end(); ++g) {
                std::vector<std::string> *target = g.key() == "prepend" ? &a.gcode_prepend : g.key() == "append" ? &a.gcode_append : nullptr;
                if (!target) {
                    problem = "unknown-field";
                    return false;
                }
                if (!string_list(g.value(), 20, 200, *target, problem, "gcode")) return false;
                for (const std::string &line : *target)
                    if (line.empty() || line.front() != ';' || line.find_first_of("\r\n") != std::string::npos) {
                        problem = "not-comment";
                        return false;
                    }
            }
        } else if (key == "file") {
            if (hook != Hook::Command) {
                problem = "wrong-hook:file";
                return false;
            }
            if (!needs(consent, Permission::FileSave, problem)) return false;
            if (!v.is_object() || !v.contains("name") || !v.contains("text") || !v["name"].is_string() || !v["text"].is_string() ||
                v.size() != 2) {
                problem = "wrong-type:file";
                return false;
            }
            a.file_name = v["name"].get<std::string>();
            a.file_text = v["text"].get<std::string>();
            if (!safe_file_name(a.file_name)) {
                problem = "bad-file-name";
                return false;
            }
            if (a.file_text.size() > 64 * 1024) {
                problem = "too-long:file";
                return false;
            }
        } else {
            problem = "unknown-field";
            return false;
        }
    }
    if (!a.block_reason.empty() && !a.block) a.block_reason.clear();
    out = std::move(a);
    return true;
}

// ---------------------------------------------------------------- runs

const char *outcome_id(Outcome outcome)
{
    switch (outcome) {
    case Outcome::Answered: return "answered";
    case Outcome::TimedOut: return "timed-out";
    case Outcome::TooLarge: return "too-large";
    case Outcome::NotJson: return "not-json";
    case Outcome::Invalid: return "invalid";
    case Outcome::Failed: return "failed";
    case Outcome::NotStarted: return "not-started";
    case Outcome::Stopped: return "stopped";
    }
    return "failed";
}

std::string to_line(const RunRecord &r)
{
    return json{{"t", r.time}, {"x", r.extension}, {"h", r.hook}, {"s", r.subject}, {"d", r.seconds}, {"b", r.answer_bytes},
                {"o", outcome_id(r.outcome)}, {"p", r.problem}}
        .dump(-1, ' ', false, json::error_handler_t::replace);
}

bool from_line(std::string_view line, RunRecord &record)
{
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.remove_suffix(1);
    const json j = json::parse(line.begin(), line.end(), nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("t") || !j["t"].is_number_integer() || !j.contains("x") || !j["x"].is_string())
        return false;
    RunRecord r;
    r.time      = j["t"].get<std::int64_t>();
    r.extension = j["x"].get<std::string>();
    if (j.contains("h") && j["h"].is_string()) r.hook = j["h"].get<std::string>();
    if (j.contains("s") && j["s"].is_string()) r.subject = j["s"].get<std::string>();
    if (j.contains("d") && j["d"].is_number()) r.seconds = j["d"].get<double>();
    if (j.contains("b") && j["b"].is_number_unsigned()) r.answer_bytes = j["b"].get<std::size_t>();
    if (j.contains("p") && j["p"].is_string()) r.problem = j["p"].get<std::string>();
    const std::string o = j.contains("o") && j["o"].is_string() ? j["o"].get<std::string>() : "";
    const Outcome all[] = {Outcome::Answered, Outcome::TimedOut, Outcome::TooLarge, Outcome::NotJson, Outcome::Invalid, Outcome::Failed, Outcome::NotStarted, Outcome::Stopped};
    bool known = false;
    for (Outcome candidate : all)
        if (o == outcome_id(candidate)) {
            r.outcome = candidate;
            known     = true;
        }
    if (!known) return false;
    record = std::move(r);
    return true;
}

void RunLog::load()
{
    m_records.clear();
    if (m_file.empty()) return;
    std::ifstream in(m_file, std::ios::binary);
    std::string   line;
    while (std::getline(in, line)) {
        RunRecord r;
        if (from_line(line, r)) m_records.push_back(std::move(r));
    }
    if (m_records.size() > kLimit) m_records.erase(m_records.begin(), m_records.begin() + std::ptrdiff_t(m_records.size() - kLimit));
}

void RunLog::append(const RunRecord &record)
{
    m_records.push_back(record);
    if (m_records.size() > kLimit) m_records.erase(m_records.begin(), m_records.begin() + std::ptrdiff_t(m_records.size() - kLimit));
    save();
}

std::vector<RunRecord> RunLog::for_extension(const std::string &id) const
{
    std::vector<RunRecord> out;
    for (const RunRecord &r : m_records)
        if (r.extension == id) out.push_back(r);
    return out;
}

std::string RunLog::export_json() const
{
    json list = json::array();
    for (const RunRecord &r : m_records) list.push_back(json::parse(to_line(r)));
    return json{{"tool", "Bambu Studio extension runs"}, {"runs", list}}.dump(2);
}

void RunLog::save() const
{
    if (m_file.empty()) return;
    std::error_code ec;
    if (m_file.has_parent_path()) std::filesystem::create_directories(m_file.parent_path(), ec);
    const std::filesystem::path partial = m_file.parent_path() / std::filesystem::u8path(m_file.filename().u8string() + ".partial");
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        if (!out) return;
        for (const RunRecord &r : m_records) out << to_line(r) << '\n';
        if (!out) {
            out.close();
            std::filesystem::remove(partial, ec);
            return;
        }
    }
    std::filesystem::rename(partial, m_file, ec);
    if (ec) std::filesystem::remove(partial, ec);
}

Consent after_run(Consent consent, Outcome outcome, const Limits &limits)
{
    if (outcome == Outcome::NotStarted || outcome == Outcome::Stopped) return consent;
    if (outcome == Outcome::Answered) {
        consent.failures_in_row = 0;
        return consent;
    }
    ++consent.failures_in_row;
#ifndef SLIC3R_EXTENSION_CORE_NEGATIVE_FIXTURE
    if (limits.off_after_failures > 0 && consent.failures_in_row >= limits.off_after_failures) {
        consent.off_after_failures = true;
        consent.enabled            = false;
    }
#else
    (void) limits;
#endif
    return consent;
}

// ---------------------------------------------------------------- before a print is sent

SendSummary summarise_send_check(const std::vector<SendCheckRun> &runs)
{
    SendSummary s;
    bool        look = false;
    for (std::size_t i = 0; i < runs.size(); ++i) {
        const SendCheckRun &r = runs[i];
        if (r.outcome == Outcome::Stopped) {
            ++s.stopped;
            continue;
        }
        if (r.outcome != Outcome::Answered) {
            ++s.not_answered;
            look = true;
            continue;
        }
        for (const ChecklistRow &row : r.answer.checklist) {
            if (row.state == "pass") ++s.passed;
            else if (row.state == "warn") ++s.warned;
            else ++s.failed;
        }
        s.notes += int(r.answer.notes.size());
        if (s.warned + s.failed + s.notes > 0) look = true;
#ifndef SLIC3R_EXTENSION_SEND_NEGATIVE_FIXTURE
        if (r.answer.block) s.blockers.push_back(i);
#endif
    }
    s.verdict = !s.blockers.empty() ? SendVerdict::Stop : look ? SendVerdict::Look : SendVerdict::Continue;
    return s;
}

}}} // namespace Slic3r::GUI::Extensions
