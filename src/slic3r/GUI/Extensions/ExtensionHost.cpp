#include "ExtensionHost.hpp"

#include "../Base64Toolkit/Base64Codec.hpp"

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace Slic3r { namespace GUI { namespace Extensions {

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

constexpr const char *kMarker         = ".builtin.json";
constexpr std::size_t kMaxFolderBytes = std::size_t(50) << 20;
constexpr std::size_t kMaxFolderFiles = 500;

bool read_file(const fs::path &path, std::size_t limit, std::string &out)
{
    std::error_code ec;
    const auto      size = fs::file_size(path, ec);
    if (ec || size > limit) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream text;
    text << in.rdbuf();
    out = text.str();
    return true;
}

bool write_file(const fs::path &path, const std::string &text)
{
    std::error_code ec;
    if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
    const fs::path partial = path.parent_path() / fs::u8path(path.filename().u8string() + ".partial");
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << text;
        if (!out) return false;
    }
    fs::rename(partial, path, ec);
    if (ec) {
        fs::remove(partial, ec);
        return false;
    }
    return true;
}

std::string file_sha256(const fs::path &path)
{
    std::ifstream          in(path, std::ios::binary);
    Base64Toolkit::Sha256  hash;
    char                   buffer[65536];
    while (in) {
        in.read(buffer, sizeof(buffer));
        const auto got = std::size_t(in.gcount());
        if (got == 0) break;
        hash.update(reinterpret_cast<const std::uint8_t *>(buffer), got);
    }
    return Base64Toolkit::hex(hash.finish());
}

// Copies a folder without following links; refuses folders that are too big.
bool copy_folder(const fs::path &from, const fs::path &to, std::string &error)
{
    std::error_code ec;
    std::size_t     bytes = 0, files = 0;
    for (auto it = fs::recursive_directory_iterator(from, fs::directory_options::none, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (it->is_symlink(ec)) {
            error = "the folder contains a link";
            return false;
        }
        if (it->is_regular_file(ec)) {
            bytes += std::size_t(it->file_size(ec));
            if (++files > kMaxFolderFiles || bytes > kMaxFolderBytes) {
                error = "the folder is larger than 50 MB or 500 files";
                return false;
            }
        }
    }
    if (ec) {
        error = "the folder could not be read";
        return false;
    }
    // File by file: fs::copy's handling of an existing target directory
    // differs between standard libraries.
    fs::create_directories(to, ec);
    for (auto it = fs::recursive_directory_iterator(from, fs::directory_options::none, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        const fs::path target = to / fs::relative(it->path(), from, ec);
        if (ec) break;
        if (it->is_directory(ec)) fs::create_directories(target, ec);
        else if (it->is_regular_file(ec)) fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, ec);
        if (ec) break;
    }
    if (ec) {
        error = "the folder could not be copied";
        return false;
    }
    return true;
}

bool load_manifest(const fs::path &folder, Manifest &manifest, ManifestError &error)
{
    std::string text;
    if (!read_file(folder / "manifest.json", kManifestLimit + 1, text)) {
        error = ManifestError{"missing", "manifest.json", {}};
        return false;
    }
    return parse_manifest(text, manifest, error);
}

json marker_json(const Manifest &manifest, const std::map<std::string, std::string> &hashes)
{
    json files = json::object();
    for (const auto &[name, sha] : hashes) files[name] = sha;
    return json{{"id", manifest.id}, {"version", manifest.version}, {"files", files}};
}

bool read_marker(const fs::path &folder, std::string &version, std::map<std::string, std::string> &files)
{
    std::string text;
    if (!read_file(folder / kMarker, 1 << 20, text)) return false;
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object() || !j.contains("version") || !j["version"].is_string() || !j.contains("files") ||
        !j["files"].is_object())
        return false;
    version = j["version"].get<std::string>();
    files.clear();
    for (auto it = j["files"].begin(); it != j["files"].end(); ++it)
        if (it.value().is_string()) files[it.key()] = it.value().get<std::string>();
    return true;
}

} // namespace

std::map<std::string, std::string> folder_hashes(const fs::path &folder)
{
    std::map<std::string, std::string> out;
    std::error_code                    ec;
    for (auto it = fs::recursive_directory_iterator(folder, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        const std::string rel = fs::relative(it->path(), folder, ec).generic_u8string();
        if (rel == kMarker) continue;
        out[rel] = file_sha256(it->path());
    }
    return out;
}

RunResult execute(const PlannedRun &plan, const json &context, const std::string &subject, std::int64_t now, const std::atomic<bool> *cancel)
{
    RunResult result;
    result.plan = plan;
    const json request = build_request(plan.hook, plan.action, context, plan.manifest, plan.consent, plan.settings);

    ProcessRequest process;
    process.folder     = plan.folder;
    process.argv       = plan.manifest.argv;
    process.input      = request.dump(-1, ' ', false, json::error_handler_t::replace);
    process.timeout_ms = plan.limits.timeout_ms;
    process.max_output = plan.limits.max_answer;
    process.cancel     = cancel;
    const ProcessResult ran = run_process(process);

    result.record.time         = now;
    result.record.extension    = plan.id;
    result.record.hook         = plan.hook == Hook::Command ? "command:" + plan.action : info(plan.hook).id;
    result.record.subject      = subject.substr(0, 120);
    result.record.seconds      = ran.seconds;
    result.record.answer_bytes = ran.output.size();
    switch (ran.status) {
    case ProcessResult::Status::StartFailed:
        result.outcome = Outcome::Failed;
        result.problem = ran.start_error.empty() ? "the program could not be started" : ran.start_error;
        break;
    case ProcessResult::Status::Cancelled:
        result.outcome = Outcome::Stopped;
        result.problem = "stopped";
        break;
    case ProcessResult::Status::TimedOut: result.outcome = Outcome::TimedOut; break;
    case ProcessResult::Status::TooLarge: result.outcome = Outcome::TooLarge; break;
    case ProcessResult::Status::Exited:
        if (ran.exit_code != 0) {
            result.outcome = Outcome::Failed;
            result.problem = "exit code " + std::to_string(ran.exit_code);
        } else if (!parse_answer(ran.output, plan.hook, plan.manifest, plan.consent, result.answer, result.problem)) {
            result.outcome = result.problem == "not-json" || result.problem == "not-object" ? Outcome::NotJson : Outcome::Invalid;
        } else {
            result.outcome = Outcome::Answered;
        }
        break;
    }
    result.record.outcome = result.outcome;
    result.record.problem = result.problem;
    return result;
}

Host::Host(fs::path root, fs::path state, fs::path builtin)
    : m_root(std::move(root)), m_state(std::move(state)), m_builtin(std::move(builtin)), m_runs(m_state / "runs.jsonl")
{}

void Host::load_settings()
{
    std::string text;
    m_master_on = true;
    m_limits    = Limits{};
    m_removed_builtins.clear();
    if (!read_file(m_state / "settings.json", 1 << 20, text)) return;
    const json j = json::parse(text, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return;
    if (j.contains("masterOn") && j["masterOn"].is_boolean()) m_master_on = j["masterOn"].get<bool>();
    if (j.contains("timeoutMs") && j["timeoutMs"].is_number_integer()) m_limits.timeout_ms = j["timeoutMs"].get<int>();
    if (j.contains("maxAnswer") && j["maxAnswer"].is_number_unsigned()) m_limits.max_answer = j["maxAnswer"].get<std::size_t>();
    if (j.contains("offAfterFailures") && j["offAfterFailures"].is_number_integer()) m_limits.off_after_failures = j["offAfterFailures"].get<int>();
    m_limits = clamp_limits(m_limits);
    if (j.contains("removedBuiltins") && j["removedBuiltins"].is_array())
        for (const json &id : j["removedBuiltins"])
            if (id.is_string()) m_removed_builtins.push_back(id.get<std::string>());
}

void Host::save_settings() const
{
    json j{{"masterOn", m_master_on},
           {"timeoutMs", m_limits.timeout_ms},
           {"maxAnswer", m_limits.max_answer},
           {"offAfterFailures", m_limits.off_after_failures},
           {"removedBuiltins", m_removed_builtins}};
    write_file(m_state / "settings.json", j.dump(2));
}

void Host::save_consents() const { write_file(m_state / "consent.json", serialize_consents(m_consents)); }

void Host::install_builtins()
{
    std::error_code ec;
    if (m_builtin.empty() || !fs::is_directory(m_builtin, ec)) return;
    for (const auto &entry : fs::directory_iterator(m_builtin, ec)) {
        if (!entry.is_directory(ec)) continue;
        Manifest      manifest;
        ManifestError error;
        if (!load_manifest(entry.path(), manifest, error)) continue;
        if (std::find(m_removed_builtins.begin(), m_removed_builtins.end(), manifest.id) != m_removed_builtins.end()) continue;
        const fs::path target = m_root / fs::u8path(manifest.id);
        std::string    copy_error;
        if (!fs::exists(target, ec)) {
            if (copy_folder(entry.path(), target, copy_error))
                write_file(target / kMarker, marker_json(manifest, folder_hashes(target)).dump(2));
            continue;
        }
        std::string                        installed_version;
        std::map<std::string, std::string> recorded;
        if (!read_marker(target, installed_version, recorded)) continue; // an extension of yours with the same id: left alone
#ifndef SLIC3R_EXTENSION_HOST_NEGATIVE_FIXTURE
        if (folder_hashes(target) != recorded) continue;                 // you changed it: updates are paused
#endif
        if (compare_versions(manifest.version, installed_version) <= 0) continue;
        const fs::path incoming = m_root / fs::u8path(manifest.id + ".incoming");
        fs::remove_all(incoming, ec);
        if (!copy_folder(entry.path(), incoming, copy_error)) {
            fs::remove_all(incoming, ec);
            continue;
        }
        write_file(incoming / kMarker, marker_json(manifest, folder_hashes(incoming)).dump(2));
        fs::remove_all(target, ec);
        fs::rename(incoming, target, ec);
    }
}

void Host::load()
{
    std::error_code ec;
    fs::create_directories(m_root, ec);
    fs::create_directories(m_state, ec);
    load_settings();
    std::string text;
    m_consents.clear();
    if (read_file(m_state / "consent.json", 1 << 20, text)) parse_consents(text, m_consents);
    m_values.clear();
    if (read_file(m_state / "extension-settings.json", 1 << 20, text)) parse_setting_values(text, m_values);
    install_builtins();
    m_runs.load();

    m_extensions.clear();
    std::set<std::string> seen;
    std::vector<fs::path> folders;
    for (const auto &entry : fs::directory_iterator(m_root, ec))
        if (entry.is_directory(ec) && entry.path().extension() != ".incoming") folders.push_back(entry.path());
    std::sort(folders.begin(), folders.end());
    for (const fs::path &folder : folders) {
        Installed x;
        x.folder = folder;
        load_manifest(folder, x.manifest, x.error);
        if (x.error.ok() && !seen.insert(x.manifest.id).second) x.error = ManifestError{"duplicate", "id", x.manifest.id};
        std::string                        version;
        std::map<std::string, std::string> recorded;
        if (read_marker(folder, version, recorded)) {
            x.builtin  = true;
            x.modified = folder_hashes(folder) != recorded;
        }
        if (x.manifest.id.empty()) x.manifest.name = folder.filename().u8string();
        m_extensions.push_back(std::move(x));
    }
}

const Installed *Host::find(const std::string &id) const
{
    for (const Installed &x : m_extensions)
        if (x.error.ok() && x.manifest.id == id) return &x;
    return nullptr;
}

const Consent *Host::consent(const std::string &id) const
{
    const auto it = m_consents.find(id);
    return it == m_consents.end() ? nullptr : &it->second;
}

State Host::state(const Installed &x) const { return state_for(x.error, x.manifest, consent(x.manifest.id), m_master_on); }

void Host::allow(const std::string &id)
{
    const Installed *x = find(id);
    if (!x) return;
    m_consents[id] = answer_consent(x->manifest, consent(id), true);
    save_consents();
}

void Host::keep_off(const std::string &id)
{
    const Installed *x = find(id);
    if (!x) return;
    m_consents[id] = answer_consent(x->manifest, consent(id), false);
    save_consents();
}

bool Host::set_enabled(const std::string &id, bool on)
{
    const Installed *x = find(id);
    if (!x) return false;
    auto it = m_consents.find(id);
    if (!on) {
        if (it == m_consents.end()) m_consents[id] = answer_consent(x->manifest, nullptr, false);
        else {
            it->second.enabled     = false;
            it->second.off_by_user = true;
        }
        save_consents();
        return true;
    }
    if (it == m_consents.end() || it->second.version != x->manifest.version || it->second.allowed != x->manifest.permissions)
        return false; // the prompt must ask first
    it->second.enabled            = true;
    it->second.off_by_user        = false;
    it->second.off_after_failures = false;
    it->second.failures_in_row    = 0;
    save_consents();
    return true;
}

void Host::set_master(bool on)
{
    m_master_on = on;
    save_settings();
}

void Host::set_limits(Limits limits)
{
    m_limits = clamp_limits(limits);
    save_settings();
}

bool Host::remove(const std::string &id, std::string &error)
{
    const Installed *x = find(id);
    if (!x) {
        error = "not installed";
        return false;
    }
    const bool      builtin = x->builtin;
    std::error_code ec;
    fs::remove_all(x->folder, ec);
    if (ec) {
        error = "the folder could not be removed";
        return false;
    }
    if (builtin && std::find(m_removed_builtins.begin(), m_removed_builtins.end(), id) == m_removed_builtins.end())
        m_removed_builtins.push_back(id);
    m_consents.erase(id);
    m_values.erase(id);
    save_consents();
    save_settings();
    save_values();
    load();
    return true;
}

Host::Inspection Host::inspect(const fs::path &from, Manifest &incoming, ManifestError &error) const
{
    if (!load_manifest(from, incoming, error)) return Inspection::Invalid;
    const Installed *existing = find(incoming.id);
    if (!existing) return Inspection::New;
    const int order = compare_versions(incoming.version, existing->manifest.version);
    return order > 0 ? Inspection::NewerVersion : order < 0 ? Inspection::OlderVersion : Inspection::SameVersion;
}

bool Host::install(const fs::path &from, std::string &error)
{
    Manifest      manifest;
    ManifestError manifest_error;
    if (!load_manifest(from, manifest, manifest_error)) {
        error = "the manifest is not valid: " + manifest_error.code;
        return false;
    }
    std::error_code ec;
    const fs::path  target   = m_root / fs::u8path(manifest.id);
    const fs::path  incoming = m_root / fs::u8path(manifest.id + ".incoming");
    if (fs::equivalent(from, target, ec)) {
        error = "that folder is already the installed one";
        return false;
    }
    fs::remove_all(incoming, ec);
    if (!copy_folder(from, incoming, error)) {
        fs::remove_all(incoming, ec);
        return false;
    }
    fs::remove(incoming / kMarker, ec); // a copied folder is yours, not a built-in
    fs::remove_all(target, ec);
    fs::rename(incoming, target, ec);
    if (ec) {
        error = "the folder could not be put in place";
        return false;
    }
    m_removed_builtins.erase(std::remove(m_removed_builtins.begin(), m_removed_builtins.end(), manifest.id), m_removed_builtins.end());
    save_settings();
    load();
    return true;
}

std::vector<PlannedRun> Host::plan(Hook hook) const
{
    std::vector<PlannedRun> out;
    if (!m_master_on || !info(hook).built) return out;
    for (const Installed &x : m_extensions) {
        if (state(x) != State::On) continue;
        if (std::find(x.manifest.hooks.begin(), x.manifest.hooks.end(), hook) == x.manifest.hooks.end()) continue;
        out.push_back(PlannedRun{x.manifest.id, x.folder, x.manifest, *consent(x.manifest.id), m_limits, hook, {}, stored_values(x.manifest.id)});
    }
    return out;
}

std::optional<PlannedRun> Host::plan_action(const std::string &id, const std::string &action) const
{
    const Installed *x = find(id);
    if (!x || state(*x) != State::On) return std::nullopt;
    const bool known = std::any_of(x->manifest.actions.begin(), x->manifest.actions.end(), [&](const Action &a) { return a.id == action; });
    if (!known) return std::nullopt;
    return PlannedRun{x->manifest.id, x->folder, x->manifest, *consent(id), m_limits, Hook::Command, action, stored_values(id)};
}

std::optional<PlannedRun> Host::plan_sample(const std::string &id, Hook hook, const std::string &action) const
{
    const Installed *x = find(id);
    if (!x || state(*x) != State::On) return std::nullopt;
    if (hook == Hook::Command) return plan_action(id, action);
    if (std::find(x->manifest.hooks.begin(), x->manifest.hooks.end(), hook) == x->manifest.hooks.end()) return std::nullopt;
    return PlannedRun{x->manifest.id, x->folder, x->manifest, *consent(id), m_limits, hook, {}, stored_values(id)};
}

json Host::stored_values(const std::string &id) const
{
    const auto it = m_values.find(id);
    return it == m_values.end() ? json::object() : it->second;
}

json Host::setting_values(const std::string &id) const
{
    const Installed *x = find(id);
    return x ? settings_for_request(x->manifest, stored_values(id)) : json::object();
}

bool Host::set_setting(const std::string &id, const std::string &setting, const json &value, std::string &problem)
{
    const Installed *x = find(id);
    if (!x || !x->error.ok()) {
        problem = "not-installed";
        return false;
    }
    const auto spec = std::find_if(x->manifest.settings.begin(), x->manifest.settings.end(), [&](const SettingSpec &s) { return s.id == setting; });
    if (spec == x->manifest.settings.end()) {
        problem = "unknown-setting";
        return false;
    }
    if (!valid_setting_value(*spec, value, problem)) return false;
    json &stored = m_values[id];
    if (!stored.is_object()) stored = json::object();
    stored[setting] = value;
    save_values();
    return true;
}

void Host::reset_settings(const std::string &id)
{
    if (m_values.erase(id) > 0) save_values();
}

void Host::save_values() const { write_file(m_state / "extension-settings.json", serialize_setting_values(m_values)); }

void Host::record(const RunResult &result)
{
    auto it = m_consents.find(result.plan.id);
    if (it != m_consents.end()) {
        it->second = after_run(it->second, result.outcome, m_limits);
        save_consents();
    }
    m_runs.append(result.record);
}

}}} // namespace Slic3r::GUI::Extensions
