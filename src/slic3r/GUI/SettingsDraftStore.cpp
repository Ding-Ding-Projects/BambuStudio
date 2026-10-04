#include "SettingsDraftStore.hpp"

#include <nlohmann/json.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace Slic3r { namespace GUI {
namespace {
using Json = nlohmann::json;
constexpr size_t max_drafts = 64;
constexpr size_t max_document_bytes = 16 * 1024 * 1024;

bool supported(Preset::Type type)
{
    return type == Preset::TYPE_PRINT || type == Preset::TYPE_FILAMENT || type == Preset::TYPE_PRINTER;
}

Json config_json(const DynamicPrintConfig& config)
{
    Json values = Json::object();
    for (const auto& key : config.keys())
        if (SettingsDraftStore::permitted_key(key))
            values[key] = config.opt_serialize(key);
    return values;
}

DynamicPrintConfig read_config(const Json& values)
{
    if (!values.is_object() || values.size() > 4096)
        throw std::runtime_error("Invalid draft configuration");
    DynamicPrintConfig config;
    for (auto it = values.begin(); it != values.end(); ++it) {
        if (!SettingsDraftStore::permitted_key(it.key()) || !it.value().is_string())
            throw std::runtime_error("Invalid draft option");
        const auto value = it.value().get<std::string>();
        if (value.size() > 1024 * 1024)
            throw std::runtime_error("Draft option exceeds storage limit");
        config.set_deserialize_strict(it.key(), value);
        // Legacy aliases must not silently change a persisted snapshot.
        if (config.option(it.key()) == nullptr)
            throw std::runtime_error("Unsupported draft option");
    }
    return config;
}

std::vector<std::string> differences(const DynamicPrintConfig& a, const DynamicPrintConfig& b)
{
    const auto av = config_json(a), bv = config_json(b);
    std::set<std::string> keys;
    for (auto it = av.begin(); it != av.end(); ++it) keys.insert(it.key());
    for (auto it = bv.begin(); it != bv.end(); ++it) keys.insert(it.key());
    std::vector<std::string> result;
    for (const auto& key : keys)
        if (!av.contains(key) || !bv.contains(key) || av.at(key) != bv.at(key))
            result.push_back(key);
    return result;
}

DynamicPrintConfig sanitized(const DynamicPrintConfig& input)
{
    DynamicPrintConfig output(input);
    for (const auto& key : input.keys())
        if (!SettingsDraftStore::permitted_key(key)) output.erase(key);
    return output;
}

std::string new_id()
{
    return "draft:" + boost::uuids::to_string(boost::uuids::random_generator()());
}
}

bool SettingsDraftStore::Target::operator==(const Target& rhs) const
{
    return project_id == rhs.project_id && target_id == rhs.target_id && revision == rhs.revision;
}

bool SettingsDraftStore::permitted_key(const std::string& key)
{
    std::string lower(key);
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    for (const char* sensitive : {"password", "passwd", "token", "secret", "credential", "access_code", "apikey", "api_key", "private_key"})
        if (lower.find(sensitive) != std::string::npos) return false;
    return !key.empty();
}

std::string SettingsDraftStore::fingerprint(const DynamicPrintConfig& config)
{
    // Deterministic revision comparison, not an authentication or cryptographic digest.
    const auto bytes = config_json(config).dump();
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : bytes) { hash ^= c; hash *= 1099511628211ULL; }
    std::ostringstream output;
    output << std::hex << std::setw(16) << std::setfill('0') << hash;
    return output.str();
}

std::string SettingsDraftStore::create(Preset::Type type, const std::string& name,
                                     const DynamicPrintConfig& config, const Target& target)
{
    if (!supported(type) || m_drafts.size() >= max_drafts || name.size() > 4096 ||
        target.project_id.size() > 4096 || target.target_id.size() > 4096 || target.revision.size() > 4096)
        return {};
    Draft draft;
    draft.id = new_id();
    draft.type = type;
    draft.source_preset_name = name;
    draft.baseline = sanitized(config);
    draft.config = draft.baseline;
    draft.target = target;
    draft.baseline_fingerprint = fingerprint(draft.baseline);
    const auto id = draft.id;
    m_drafts.emplace(id, std::move(draft));
    return id;
}

std::string SettingsDraftStore::duplicate(const std::string& id)
{
    const auto* source = find(id);
    if (!source || m_drafts.size() >= max_drafts) return {};
    Draft copy(*source);
    copy.id = new_id();
    const auto copy_id = copy.id;
    m_drafts.emplace(copy_id, std::move(copy));
    return copy_id;
}

SettingsDraftStore::Draft* SettingsDraftStore::find(const std::string& id)
{
    auto it = m_drafts.find(id);
    return it == m_drafts.end() ? nullptr : &it->second;
}
const SettingsDraftStore::Draft* SettingsDraftStore::find(const std::string& id) const
{
    auto it = m_drafts.find(id);
    return it == m_drafts.end() ? nullptr : &it->second;
}
bool SettingsDraftStore::erase(const std::string& id) { return m_drafts.erase(id) != 0; }
bool SettingsDraftStore::dirty(const std::string& id) const
{
    const auto* draft = find(id);
    return draft && !differences(draft->baseline, draft->config).empty();
}
std::vector<std::string> SettingsDraftStore::ids() const
{
    std::vector<std::string> result;
    for (const auto& item : m_drafts) result.push_back(item.first);
    return result;
}

SettingsDraftStore::ApplyResult SettingsDraftStore::prepare_apply(
    const std::string& id, const DynamicPrintConfig& live, const Target& target) const
{
    ApplyResult result;
    const auto* draft = find(id);
    if (!draft) { result.conflict = "Draft no longer exists"; return result; }
    if (!(draft->target == target)) { result.conflict = "Draft target or revision changed"; return result; }
    if (!supported(draft->type) || !differences(draft->baseline, live).empty()) {
        result.conflict = "Source settings changed since this draft was created";
        return result;
    }
    result.changed_keys = differences(draft->baseline, draft->config);
    for (const auto& key : result.changed_keys) {
        const auto* option = draft->config.option(key);
        if (option) result.delta.set_key_value(key, option->clone());
        else result.removed_keys.push_back(key);
    }
    result.ready = true;
    return result;
}

std::string SettingsDraftStore::serialize() const
{
    Json root = {{"version", 1}, {"drafts", Json::array()}};
    for (const auto& item : m_drafts) {
        const auto& draft = item.second;
        root["drafts"].push_back({{"id", draft.id}, {"type", int(draft.type)},
            {"source_preset_name", draft.source_preset_name},
            {"target", {{"project_id", draft.target.project_id}, {"target_id", draft.target.target_id}, {"revision", draft.target.revision}}},
            {"baseline", config_json(draft.baseline)}, {"config", config_json(draft.config)},
            {"baseline_fingerprint", fingerprint(draft.baseline)},
            {"view", {{"search", draft.search_query}, {"scroll_y", draft.scroll_y}}}});
    }
    return root.dump();
}

bool SettingsDraftStore::restore(const std::string& text, std::string* reason)
{
    try {
        if (text.size() > max_document_bytes) throw std::runtime_error("Draft storage exceeds limit");
        const auto root = Json::parse(text);
        if (!root.is_object() || root.at("version") != 1 || !root.at("drafts").is_array() || root.at("drafts").size() > max_drafts)
            throw std::runtime_error("Unsupported draft storage format");
        std::map<std::string, Draft> restored;
        for (const auto& value : root.at("drafts")) {
            Draft draft;
            draft.id = value.at("id").get<std::string>();
            draft.type = static_cast<Preset::Type>(value.at("type").get<int>());
            draft.source_preset_name = value.at("source_preset_name").get<std::string>();
            const auto& target = value.at("target");
            draft.target = {target.at("project_id").get<std::string>(), target.at("target_id").get<std::string>(), target.at("revision").get<std::string>()};
            if (draft.id.find("draft:") != 0 || draft.id.size() > 128 || draft.source_preset_name.size() > 4096 ||
                draft.target.project_id.size() > 4096 || draft.target.target_id.size() > 4096 || draft.target.revision.size() > 4096 || !supported(draft.type))
                throw std::runtime_error("Invalid draft identity");
            if (value.contains("view")) {
                draft.search_query = value.at("view").value("search", std::string());
                draft.scroll_y = value.at("view").value("scroll_y", 0);
                if (draft.search_query.size() > 4096 || draft.scroll_y < 0 || draft.scroll_y > 1000000)
                    throw std::runtime_error("Invalid draft view state");
            }
            draft.baseline = read_config(value.at("baseline"));
            draft.config = read_config(value.at("config"));
            draft.baseline_fingerprint = fingerprint(draft.baseline);
            if (draft.baseline_fingerprint != value.at("baseline_fingerprint").get<std::string>())
                throw std::runtime_error("Draft baseline revision mismatch");
            const auto id = draft.id;
            if (!restored.emplace(id, std::move(draft)).second) throw std::runtime_error("Duplicate draft identity");
        }
        m_drafts.swap(restored);
        if (reason) reason->clear();
        return true;
    } catch (const std::exception&) {
        // Never forward parser diagnostics, which can include persisted settings values.
        if (reason) *reason = "Stored drafts are invalid or incompatible; existing drafts were retained";
        return false;
    }
}

}} // namespace Slic3r::GUI
