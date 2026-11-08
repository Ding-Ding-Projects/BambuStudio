#include "PreferencePlans.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <set>

namespace Slic3r::GUI::PreferencePlans {

namespace {

const Setting *find_setting(const std::vector<Setting> &settings, std::string_view key)
{
    for (const Setting &s : settings)
        if (s.key == key) return &s;
    return nullptr;
}

// The value in effect: what is written down, or else the built-in default.
std::optional<std::string> effective(const Setting &s) { return s.stored ? s.stored : s.default_value; }

} // namespace

bool by_hand_only(std::string_view key)
{
    // language and single_instance need a restart; enable_lod recreates the
    // window; region signs you out; privacyuse is a consent answer and lives
    // outside the app section; the developer modes switch the whole UI mode.
    static const std::set<std::string, std::less<>> keys = {"language",       "single_instance", "enable_lod",
                                                            "region",         "privacyuse",      "developer_mode",
                                                            "internal_developer_mode", "iot_environment"};
    return keys.count(key) > 0;
}

ResetPlan plan_section_reset(const std::vector<Setting> &section)
{
    ResetPlan             plan;
    std::set<std::string> seen;
    for (const Setting &s : section) {
        if (s.key.empty() || !seen.insert(s.key).second) continue;
        if (PreferenceHistory::is_sensitive_key(s.key)) {
            plan.private_keys.push_back(s.key);
            continue;
        }
        if (!PreferenceHistory::differs_from_default(s.stored, s.default_value)) {
            plan.unchanged.push_back(s.key);
            continue;
        }
#ifndef SLIC3R_PREFERENCE_PLANS_NEGATIVE_FIXTURE
        if (s.settable && !by_hand_only(s.key)) plan.reset.push_back(s.key);
#else
        // Test-only mutation (tests/preference_plans.test.mjs): settings that
        // restart the app or answer a consent question are reset with the rest.
        if (s.settable) plan.reset.push_back(s.key);
#endif
        else plan.by_hand.push_back(s.key);
    }
    return plan;
}

ImportPlan plan_import(std::string_view text, const std::vector<Setting> &settings)
{
    using json = nlohmann::json;
    ImportPlan plan;
    if (text.size() > kImportLimit) {
        plan.problem = "too-large";
        return plan;
    }
    const json root = json::parse(text.begin(), text.end(), nullptr, false);
    if (root.is_discarded()) {
        plan.problem = "not-json";
        return plan;
    }
    if (!root.is_object()) {
        plan.problem = "not-preferences";
        return plan;
    }
    const json *data = &root;
    if (root.contains("schema")) {
        if (!root["schema"].is_string() || root["schema"].get<std::string>() != "bambustudio.app-config" || !root.contains("data")) {
            plan.problem = "not-preferences";
            return plan;
        }
        if (root.contains("schemaVersion")) {
            if (!root["schemaVersion"].is_number_integer()) {
                plan.problem = "not-preferences";
                return plan;
            }
            if (root["schemaVersion"].get<long long>() > 1) {
                plan.problem = "newer-version";
                return plan;
            }
        }
        data = &root["data"];
    }
    if (!data->is_object() || !data->contains("app") || !(*data)["app"].is_object()) {
        plan.problem = "not-preferences";
        return plan;
    }
    const json &app = (*data)["app"];
    if (app.empty()) {
        plan.problem = "empty";
        return plan;
    }
    for (auto it = app.begin(); it != app.end(); ++it) {
        const std::string &key = it.key();
        if (PreferenceHistory::is_sensitive_key(key)) {
            plan.private_keys.push_back(key);
            continue;
        }
        const Setting *s = find_setting(settings, key);
        if (s == nullptr) {
            plan.not_here.push_back(key);
            continue;
        }
        const json &v = it.value();
        std::string value;
        if (v.is_string()) value = v.get<std::string>();
        else if (v.is_boolean()) value = v.get<bool>() ? "true" : "false";
        else if (v.is_number_integer()) value = std::to_string(v.get<long long>());
        else {
            plan.not_text.push_back(key);
            continue;
        }
        if (value.size() > 4096 || value.find('\0') != std::string::npos) {
            plan.not_text.push_back(key);
            continue;
        }
        if (effective(*s) == value) {
            plan.same.push_back(key);
            continue;
        }
        if (s->settable && !by_hand_only(key)) plan.apply.push_back({key, value});
        else plan.by_hand.push_back({key, value});
    }
    return plan;
}

UndoPlan plan_undo(const std::vector<PreferenceHistory::Change> &newest_first, const std::vector<Setting> &settings)
{
    UndoPlan plan;
    if (newest_first.empty()) {
        plan.reason = "nothing";
        return plan;
    }
    const PreferenceHistory::Change &c = newest_first.front();
    plan.key                           = c.key;
    if (c.redacted || PreferenceHistory::is_sensitive_key(c.key)) {
        plan.reason = "private";
        return plan;
    }
    const Setting *s = find_setting(settings, c.key);
    if (s == nullptr) {
        plan.reason = "not-shown";
        return plan;
    }
    if (by_hand_only(c.key)) {
        plan.reason = "by-hand";
        return plan;
    }
    if (!s->settable) {
        plan.reason = "not-settable";
        return plan;
    }
    // An empty earlier value means nothing was written down then: the built-in
    // default was in effect, so that is what goes back.
    plan.value    = c.from.empty() && s->default_value ? *s->default_value : c.from;
    plan.possible = true;
    return plan;
}

std::vector<std::string> related(const std::vector<Row> &rows, std::string_view key, std::size_t limit)
{
    std::size_t at = rows.size();
    for (std::size_t i = 0; i < rows.size() && at == rows.size(); ++i)
        if (std::find(rows[i].keys.begin(), rows[i].keys.end(), key) != rows[i].keys.end()) at = i;
    if (at == rows.size() || limit == 0) return {};
    const int page = rows[at].page;

    std::size_t begin = at;
    while (begin > 0 && rows[begin - 1].page == page && !rows[begin - 1].title) --begin;
    std::size_t end = at + 1;
    while (end < rows.size() && rows[end].page == page && !rows[end].title) ++end;

    // Nearest first, then the earlier of two at the same distance.
    auto by_distance = [at](std::vector<std::size_t> &indexes) {
        std::stable_sort(indexes.begin(), indexes.end(), [at](std::size_t a, std::size_t b) {
            const std::size_t da = a > at ? a - at : at - a, db = b > at ? b - at : at - b;
            return da < db;
        });
    };
    std::vector<std::string> out;
    std::set<std::string>    seen{std::string(key)};
    auto                     take = [&](const std::vector<std::size_t> &indexes) {
        for (std::size_t i : indexes)
            for (const std::string &k : rows[i].keys)
                if (out.size() < limit && seen.insert(k).second) out.push_back(k);
    };
    std::vector<std::size_t> group;
    for (std::size_t i = begin; i < end; ++i)
        if (i != at) group.push_back(i);
    by_distance(group);
    take(group);
    if (out.size() < 2) {
        std::vector<std::size_t> rest;
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].page == page && (i < begin || i >= end)) rest.push_back(i);
        by_distance(rest);
        take(rest);
    }
    return out;
}

std::map<int, int> settings_per_page(const std::vector<Row> &rows)
{
    std::map<int, std::set<std::string>> keys;
    for (const Row &row : rows)
        for (const std::string &k : row.keys)
            if (!k.empty()) keys[row.page].insert(k);
    std::map<int, int> out;
    for (const auto &[page, set] : keys) out[page] = int(set.size());
    return out;
}

} // namespace Slic3r::GUI::PreferencePlans
