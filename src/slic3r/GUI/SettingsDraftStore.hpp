#ifndef slic3r_SettingsDraftStore_hpp_
#define slic3r_SettingsDraftStore_hpp_

#include "libslic3r/Preset.hpp"
#include <map>
#include <string>
#include <vector>

namespace Slic3r { namespace GUI {

// Detached snapshots only. Callers own persistence and all live configuration changes.
class SettingsDraftStore
{
public:
    struct Target {
        std::string project_id;
        std::string target_id;
        std::string revision;
        bool operator==(const Target& rhs) const;
    };
    struct Draft {
        std::string id;
        Preset::Type type = Preset::TYPE_INVALID;
        std::string source_preset_name;
        DynamicPrintConfig baseline;
        DynamicPrintConfig config;
        Target target;
        std::string baseline_fingerprint;
        std::string search_query;
        int scroll_y = 0;
    };
    struct ApplyResult {
        bool ready = false;
        std::string conflict;
        std::vector<std::string> changed_keys;
        std::vector<std::string> removed_keys;
        DynamicPrintConfig delta;
    };

    std::string create(Preset::Type type, const std::string& name,
                       const DynamicPrintConfig& config, const Target& target);
    std::string duplicate(const std::string& id);
    Draft* find(const std::string& id);
    const Draft* find(const std::string& id) const;
    bool erase(const std::string& id);
    bool dirty(const std::string& id) const;
    std::vector<std::string> ids() const;
    ApplyResult prepare_apply(const std::string& id, const DynamicPrintConfig& live,
                              const Target& target) const;
    // Versioned JSON suitable for an AppConfig string. Restore is transactional.
    std::string serialize() const;
    bool restore(const std::string& json, std::string* reason = nullptr);
    static std::string fingerprint(const DynamicPrintConfig& config);
    static bool permitted_key(const std::string& key);

private:
    std::map<std::string, Draft> m_drafts;
};

}} // namespace Slic3r::GUI
#endif
