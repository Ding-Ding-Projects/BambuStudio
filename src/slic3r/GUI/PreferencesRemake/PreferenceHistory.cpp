#include "PreferenceHistory.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace Slic3r::GUI::PreferenceHistory {

bool is_sensitive_key(std::string_view key)
{
#ifdef SLIC3R_PREFERENCE_HISTORY_NEGATIVE_FIXTURE
    // Test-only mutation (tests/preference_history.test.mjs): nothing counts as
    // secret, so tokens would be written to the record. The suite must fail.
    (void) key;
    return false;
#endif
    std::string lower(key);
    for (char &c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
    for (const char *part : {"token", "password", "passwd", "secret", "station_key", "access_code", "_code", "credential", "api_key", "apikey"})
        if (lower.find(part) != std::string::npos) return true;
    return false;
}

std::vector<Change> diff(const std::map<std::string, std::string> &before,
                         const std::map<std::string, std::string> &after, std::int64_t time)
{
    std::vector<Change> out;
    for (const auto &[key, value] : after) {
        const auto        it  = before.find(key);
        const std::string old = it == before.end() ? std::string() : it->second;
        if (old == value) continue;
        Change change;
        change.time     = time;
        change.key      = key;
        change.redacted = is_sensitive_key(key);
        if (!change.redacted) {
            change.from = old;
            change.to   = value;
        }
        out.push_back(std::move(change));
    }
    return out;
}

std::string to_line(const Change &change)
{
    nlohmann::json j;
    j["t"] = change.time;
    j["k"] = change.key;
    if (change.redacted) {
        j["r"] = true;
    } else {
        j["f"] = change.from;
        j["v"] = change.to;
    }
    return j.dump();
}

std::optional<Change> from_line(std::string_view line)
{
    const nlohmann::json j = nlohmann::json::parse(line.begin(), line.end(), nullptr, /*allow_exceptions=*/false);
    if (!j.is_object()) return std::nullopt;
    const auto t = j.find("t");
    const auto k = j.find("k");
    if (t == j.end() || k == j.end() || !t->is_number_integer() || !k->is_string()) return std::nullopt;
    Change change;
    change.time = t->get<std::int64_t>();
    change.key  = k->get<std::string>();
    if (change.key.empty()) return std::nullopt;
    const auto r = j.find("r");
    change.redacted = (r != j.end() && r->is_boolean() && r->get<bool>()) || is_sensitive_key(change.key);
    if (!change.redacted) {
        const auto f = j.find("f");
        const auto v = j.find("v");
        if (f == j.end() || v == j.end() || !f->is_string() || !v->is_string()) return std::nullopt;
        change.from = f->get<std::string>();
        change.to   = v->get<std::string>();
    }
    return change;
}

void Store::load()
{
    m_changes.clear();
    std::ifstream in(m_file, std::ios::binary);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (auto change = from_line(line)) m_changes.push_back(std::move(*change));
    }
    if (m_changes.size() > kKeep) m_changes.erase(m_changes.begin(), m_changes.end() - std::ptrdiff_t(kKeep));
}

bool Store::rewrite() const
{
    std::error_code ec;
    if (!m_file.parent_path().empty()) std::filesystem::create_directories(m_file.parent_path(), ec);
    const std::filesystem::path temp = m_file.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        for (const Change &c : m_changes) out << to_line(c) << '\n';
        if (!out) return false;
    }
    std::filesystem::rename(temp, m_file, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        return false;
    }
    return true;
}

bool Store::append(const std::vector<Change> &changes)
{
    if (changes.empty()) return true;
    for (const Change &c : changes) {
        Change stored = c;
        if (is_sensitive_key(stored.key)) {
            stored.redacted = true;
            stored.from.clear();
            stored.to.clear();
        }
        m_changes.push_back(std::move(stored));
    }
    if (m_changes.size() > kCompactAt) {
        m_changes.erase(m_changes.begin(), m_changes.end() - std::ptrdiff_t(kKeep));
        return rewrite();
    }
    std::error_code ec;
    if (!m_file.parent_path().empty()) std::filesystem::create_directories(m_file.parent_path(), ec);
    std::ofstream out(m_file, std::ios::binary | std::ios::app);
    if (!out) return false;
    for (auto it = m_changes.end() - std::ptrdiff_t(changes.size()); it != m_changes.end(); ++it) out << to_line(*it) << '\n';
    return bool(out);
}

std::vector<Change> Store::newest(std::size_t limit) const
{
    std::vector<Change> out;
    for (auto it = m_changes.rbegin(); it != m_changes.rend() && out.size() < limit; ++it) out.push_back(*it);
    return out;
}

std::vector<Change> Store::for_key(std::string_view key, std::size_t limit) const
{
    std::vector<Change> out;
    for (auto it = m_changes.rbegin(); it != m_changes.rend() && out.size() < limit; ++it)
        if (it->key == key) out.push_back(*it);
    return out;
}

std::optional<Change> Store::last_for_key(std::string_view key) const
{
    for (auto it = m_changes.rbegin(); it != m_changes.rend(); ++it)
        if (it->key == key) return *it;
    return std::nullopt;
}

Provenance describe(const std::optional<std::string> &stored, const std::optional<std::string> &default_value,
                    const std::optional<std::string> &scheduled, const std::optional<Change> &last_change)
{
    Provenance p;
    p.default_value = default_value;
    if (scheduled) {
        p.origin = Origin::Scheduled;
        p.value  = *scheduled;
        return p;
    }
    if (!stored) {
        p.origin = default_value ? Origin::BuiltInDefault : Origin::NoDefault;
        p.value  = default_value.value_or(std::string());
        return p;
    }
    p.value = *stored;
    // The record wins when its last entry produced the value in effect (also
    // when you set a value back to the default yourself).
    if (last_change && (last_change->redacted || last_change->to == *stored)) {
        p.origin     = Origin::ChangedByYou;
        p.changed_at = last_change->time;
        return p;
    }
    if (default_value && *default_value == *stored) {
        p.origin = Origin::MatchesDefault;
        return p;
    }
    p.origin = Origin::Stored;
    return p;
}

bool differs_from_default(const std::optional<std::string> &stored, const std::optional<std::string> &default_value)
{
    return stored && default_value && *stored != *default_value;
}

} // namespace Slic3r::GUI::PreferenceHistory
