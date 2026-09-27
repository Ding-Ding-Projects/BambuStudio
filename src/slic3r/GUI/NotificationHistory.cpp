#include "NotificationHistory.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace Slic3r { namespace GUI {

using json = nlohmann::json;

namespace {

std::string lower_ascii(std::string s)
{
    for (char &c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool substring_match(const std::string &query, const std::string &haystack)
{
    if (query.empty())
        return true;
    return lower_ascii(haystack).find(lower_ascii(query)) != std::string::npos;
}

std::string csv_escape(const std::string &field)
{
    const bool needs_quotes = field.find_first_of(",\"\r\n") != std::string::npos;
    if (!needs_quotes)
        return field;
    std::string out = "\"";
    for (char c : field) {
        if (c == '"')
            out += '"';
        out += c;
    }
    out += '"';
    return out;
}

std::string md_escape(const std::string &field)
{
    std::string out;
    out.reserve(field.size());
    for (char c : field) {
        if (c == '|')
            out += "\\|";
        else if (c == '\n' || c == '\r')
            out += ' ';
        else
            out += c;
    }
    return out;
}

std::string single_line(const std::string &text)
{
    std::string out = text;
    std::replace(out.begin(), out.end(), '\n', ' ');
    std::replace(out.begin(), out.end(), '\r', ' ');
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
// Helpers

std::int64_t NotificationHistory::now_ms()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string NotificationHistory::format_iso8601(std::int64_t timestamp_ms)
{
    const std::time_t secs = static_cast<std::time_t>(timestamp_ms / 1000);
    const int         ms   = static_cast<int>(timestamp_ms % 1000);
    std::tm           tm{};
#ifdef _WIN32
    gmtime_s(&tm, &secs);
#else
    gmtime_r(&secs, &tm);
#endif
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tm.tm_year + 1900, tm.tm_mon + 1,
                  tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, ms < 0 ? 0 : ms);
    return buf;
}

std::string NotificationHistory::first_line(const std::string &text)
{
    const std::size_t nl = text.find_first_of("\r\n");
    return nl == std::string::npos ? text : text.substr(0, nl);
}

std::string NotificationHistory::level_name(int level)
{
    // Mirrors NotificationManager::NotificationLevel (asserted in NotificationManager.cpp).
    switch (level) {
    case 1: return "progress";
    case 2: return "hint";
    case 3: return "regular";
    case 4: return "print_info";
    case 5: return "print_info_short";
    case 6: return "important";
    case 7: return "warning";
    case 8: return "serious_warning";
    case 9: return "error";
    default: return "level_" + std::to_string(level);
    }
}

std::string NotificationHistory::level_display(int level)
{
    switch (level) {
    case 1: return "Progress";
    case 2: return "Hint";
    case 3: return "Info";
    case 4: return "Print info";
    case 5: return "Print info";
    case 6: return "Important";
    case 7: return "Warning";
    case 8: return "Serious warning";
    case 9: return "Error";
    default: return "Level " + std::to_string(level);
    }
}

// ---------------------------------------------------------------------------
// Recording

NotificationHistory::NotificationHistory(std::size_t max_entries) : m_max_entries(std::max<std::size_t>(1, max_entries)) {}

void NotificationHistory::notify()
{
    ++m_revision;
    if (m_on_change)
        m_on_change();
}

void NotificationHistory::trim()
{
    if (m_entries.size() > m_max_entries)
        m_entries.erase(m_entries.begin(), m_entries.begin() + static_cast<std::ptrdiff_t>(m_entries.size() - m_max_entries));
}

std::uint64_t NotificationHistory::append(int level, const std::string &type_name, const std::string &text,
                                          std::int64_t timestamp_ms)
{
    NotificationHistoryEntry entry;
    entry.id           = m_next_id++;
    entry.timestamp_ms = timestamp_ms == 0 ? now_ms() : timestamp_ms;
    entry.level        = level;
    entry.level_name   = level_name(level);
    entry.type_name    = type_name;
    entry.title        = first_line(text);
    entry.text         = text;
    m_entries.push_back(std::move(entry));
    trim();
    notify();
    return m_entries.back().id;
}

bool NotificationHistory::mark_dismissed(std::uint64_t id, std::int64_t timestamp_ms)
{
    for (auto &entry : m_entries) {
        if (entry.id == id) {
            if (entry.dismissed)
                return true;
            entry.dismissed    = true;
            entry.dismissed_ms = timestamp_ms == 0 ? now_ms() : timestamp_ms;
            notify();
            return true;
        }
    }
    return false;
}

bool NotificationHistory::record_action(std::uint64_t id, const std::string &action)
{
    for (auto &entry : m_entries) {
        if (entry.id == id) {
            entry.action = action;
            notify();
            return true;
        }
    }
    return false;
}

bool NotificationHistory::mark_all_seen()
{
    bool changed = false;
    for (auto &entry : m_entries) {
        if (!entry.seen) {
            entry.seen = true;
            changed    = true;
        }
    }
    if (changed)
        notify();
    return changed;
}

std::size_t NotificationHistory::erase(const std::set<std::uint64_t> &ids)
{
    if (ids.empty())
        return 0;
    const std::size_t before = m_entries.size();
    m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
                                   [&ids](const NotificationHistoryEntry &e) { return ids.count(e.id) != 0; }),
                    m_entries.end());
    const std::size_t removed = before - m_entries.size();
    if (removed > 0)
        notify();
    return removed;
}

// ---------------------------------------------------------------------------
// Read

const NotificationHistoryEntry *NotificationHistory::find(std::uint64_t id) const
{
    for (const auto &entry : m_entries)
        if (entry.id == id)
            return &entry;
    return nullptr;
}

std::size_t NotificationHistory::unread_count() const
{
    return static_cast<std::size_t>(std::count_if(m_entries.begin(), m_entries.end(),
                                                  [](const NotificationHistoryEntry &e) { return !e.seen; }));
}

std::size_t NotificationHistory::active_count() const
{
    return static_cast<std::size_t>(std::count_if(m_entries.begin(), m_entries.end(),
                                                  [](const NotificationHistoryEntry &e) { return !e.dismissed; }));
}

// ---------------------------------------------------------------------------
// Filtering

std::string NotificationHistory::Filter::describe() const
{
    std::string out;
    out += query.empty() ? "query: (none)" : "query: \"" + query + "\"";
    out += "; levels: ";
    if (levels.empty()) {
        out += "all";
    } else {
        bool first = true;
        for (int level : levels) {
            if (!first)
                out += ", ";
            out += level_name(level);
            first = false;
        }
    }
    out += "; status: ";
    if (include_active && include_dismissed)
        out += "active and dismissed";
    else if (include_active)
        out += "active only";
    else if (include_dismissed)
        out += "dismissed only";
    else
        out += "none";
    return out;
}

std::string NotificationHistory::haystack(const NotificationHistoryEntry &entry)
{
    return entry.title + "\n" + entry.text + "\n" + entry.type_name + "\n" + entry.level_name + "\n" +
           level_display(entry.level) + "\n" + entry.action;
}

std::vector<std::uint64_t> NotificationHistory::filtered_ids(const Filter &filter) const
{
    std::vector<std::uint64_t> out;
    for (auto it = m_entries.rbegin(); it != m_entries.rend(); ++it) {
        const auto &entry = *it;
        if (!filter.levels.empty() && filter.levels.count(entry.level) == 0)
            continue;
        if (entry.dismissed && !filter.include_dismissed)
            continue;
        if (!entry.dismissed && !filter.include_active)
            continue;
        if (!filter.query.empty()) {
            const std::string hay = haystack(entry);
            const bool        hit = filter.matcher ? filter.matcher(filter.query, hay) : substring_match(filter.query, hay);
            if (!hit)
                continue;
        }
        out.push_back(entry.id);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Selection

void NotificationHistory::Selection::toggle(std::uint64_t id)
{
    if (!m_ids.erase(id))
        m_ids.insert(id);
}

void NotificationHistory::Selection::set(std::uint64_t id, bool on)
{
    if (on)
        m_ids.insert(id);
    else
        m_ids.erase(id);
}

void NotificationHistory::Selection::select_range(const std::vector<std::uint64_t> &order, std::uint64_t anchor,
                                                  std::uint64_t id)
{
    const auto a = std::find(order.begin(), order.end(), anchor);
    const auto b = std::find(order.begin(), order.end(), id);
    if (a == order.end() || b == order.end()) {
        if (b != order.end())
            m_ids.insert(id);
        return;
    }
    const auto lo = std::min(a, b);
    const auto hi = std::max(a, b);
    for (auto it = lo; it <= hi; ++it)
        m_ids.insert(*it);
}

void NotificationHistory::Selection::select_page(const std::vector<std::uint64_t> &page_ids)
{
    m_ids.insert(page_ids.begin(), page_ids.end());
}

void NotificationHistory::Selection::select_all_matches(const std::vector<std::uint64_t> &match_ids)
{
    m_ids.insert(match_ids.begin(), match_ids.end());
}

void NotificationHistory::Selection::invert(const std::vector<std::uint64_t> &universe)
{
    for (std::uint64_t id : universe)
        toggle(id);
}

void NotificationHistory::Selection::retain(const std::set<std::uint64_t> &existing)
{
    for (auto it = m_ids.begin(); it != m_ids.end();) {
        if (existing.count(*it) == 0)
            it = m_ids.erase(it);
        else
            ++it;
    }
}

std::size_t NotificationHistory::Selection::count_within(const std::vector<std::uint64_t> &universe) const
{
    return static_cast<std::size_t>(
        std::count_if(universe.begin(), universe.end(), [this](std::uint64_t id) { return m_ids.count(id) != 0; }));
}

// ---------------------------------------------------------------------------
// Export

const char *NotificationHistory::export_extension(ExportFormat format)
{
    switch (format) {
    case ExportFormat::Json: return "json";
    case ExportFormat::Csv: return "csv";
    case ExportFormat::Markdown: return "md";
    case ExportFormat::PlainText: return "txt";
    }
    return "txt";
}

const char *NotificationHistory::export_format_name(ExportFormat format)
{
    switch (format) {
    case ExportFormat::Json: return "JSON";
    case ExportFormat::Csv: return "CSV";
    case ExportFormat::Markdown: return "Markdown";
    case ExportFormat::PlainText: return "Plain text";
    }
    return "Plain text";
}

std::string NotificationHistory::export_entries(const std::vector<std::uint64_t> &ids, ExportFormat format,
                                                const Filter &filter) const
{
    std::vector<const NotificationHistoryEntry *> rows;
    rows.reserve(ids.size());
    for (std::uint64_t id : ids)
        if (const auto *e = find(id))
            rows.push_back(e);

    std::int64_t newest = 0, oldest = 0;
    for (const auto *e : rows) {
        if (newest == 0 || e->timestamp_ms > newest)
            newest = e->timestamp_ms;
        if (oldest == 0 || e->timestamp_ms < oldest)
            oldest = e->timestamp_ms;
    }
    const std::string span = rows.empty() ? std::string("(no entries)")
                                          : format_iso8601(oldest) + " to " + format_iso8601(newest);
    const std::string range = "Exported " + std::to_string(rows.size()) + " of " + std::to_string(m_entries.size()) +
                              " recorded notifications; " + filter.describe() + "; time span " + span +
                              "; exported at " + format_iso8601(now_ms()) + "; encoding UTF-8; schema v" +
                              std::to_string(SCHEMA_VERSION);

    std::ostringstream out;
    switch (format) {
    case ExportFormat::Json: {
        json doc;
        doc["schema"]       = "bambustudio.notification_history.export";
        doc["version"]      = SCHEMA_VERSION;
        doc["range"]        = range;
        doc["exported"]     = rows.size();
        doc["total"]        = m_entries.size();
        doc["filter"]       = filter.describe();
        doc["entries"]      = json::array();
        for (const auto *e : rows) {
            json j;
            j["id"]           = e->id;
            j["timestamp"]    = format_iso8601(e->timestamp_ms);
            j["timestamp_ms"] = e->timestamp_ms;
            j["level"]        = e->level;
            j["level_name"]   = e->level_name;
            j["type"]         = e->type_name;
            j["title"]        = e->title;
            j["text"]         = e->text;
            j["dismissed"]    = e->dismissed;
            j["dismissed_at"] = e->dismissed ? format_iso8601(e->dismissed_ms) : "";
            j["action"]       = e->action;
            doc["entries"].push_back(std::move(j));
        }
        return doc.dump(2);
    }
    case ExportFormat::Csv: {
        out << "# " << range << "\n";
        out << "id,timestamp,level,level_name,type,title,text,dismissed,dismissed_at,action\n";
        for (const auto *e : rows) {
            out << e->id << ',' << format_iso8601(e->timestamp_ms) << ',' << e->level << ',' << csv_escape(e->level_name)
                << ',' << csv_escape(e->type_name) << ',' << csv_escape(e->title) << ',' << csv_escape(e->text) << ','
                << (e->dismissed ? "true" : "false") << ',' << (e->dismissed ? format_iso8601(e->dismissed_ms) : "")
                << ',' << csv_escape(e->action) << "\n";
        }
        return out.str();
    }
    case ExportFormat::Markdown: {
        out << "# Notification history\n\n" << range << "\n\n";
        out << "| Time (UTC) | Level | Type | Title | Text | Status | Action |\n";
        out << "| --- | --- | --- | --- | --- | --- | --- |\n";
        for (const auto *e : rows) {
            out << "| " << format_iso8601(e->timestamp_ms) << " | " << md_escape(level_display(e->level)) << " | "
                << md_escape(e->type_name) << " | " << md_escape(e->title) << " | " << md_escape(e->text) << " | "
                << (e->dismissed ? "dismissed" : "active") << " | " << md_escape(e->action) << " |\n";
        }
        return out.str();
    }
    case ExportFormat::PlainText: {
        out << range << "\n\n";
        for (const auto *e : rows) {
            out << format_iso8601(e->timestamp_ms) << "  [" << level_display(e->level) << "]  " << e->type_name << "  "
                << (e->dismissed ? "(dismissed)" : "(active)") << "\n";
            out << "    " << single_line(e->text) << "\n";
            if (!e->action.empty())
                out << "    action: " << e->action << "\n";
        }
        return out.str();
    }
    }
    return out.str();
}

// ---------------------------------------------------------------------------
// Persistence

std::string NotificationHistory::to_json() const
{
    json doc;
    doc["schema"]      = "bambustudio.notification_history";
    doc["version"]     = SCHEMA_VERSION;
    doc["max_entries"] = m_max_entries;
    doc["next_id"]     = m_next_id;
    doc["entries"]     = json::array();
    for (const auto &e : m_entries) {
        json j;
        j["id"]           = e.id;
        j["timestamp_ms"] = e.timestamp_ms;
        j["level"]        = e.level;
        j["level_name"]   = e.level_name;
        j["type"]         = e.type_name;
        j["title"]        = e.title;
        j["text"]         = e.text;
        j["dismissed"]    = e.dismissed;
        j["dismissed_ms"] = e.dismissed_ms;
        j["seen"]         = e.seen;
        j["action"]       = e.action;
        doc["entries"].push_back(std::move(j));
    }
    return doc.dump(2);
}

bool NotificationHistory::from_json(const std::string &text, std::string *error)
{
    json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        if (error)
            *error = "notification history is not a JSON object";
        return false;
    }
    if (doc.value("schema", std::string()) != "bambustudio.notification_history") {
        if (error)
            *error = "unexpected schema";
        return false;
    }
    if (doc.value("version", 0) > SCHEMA_VERSION) {
        if (error)
            *error = "notification history was written by a newer version";
        return false;
    }
    if (!doc.contains("entries") || !doc["entries"].is_array()) {
        if (error)
            *error = "entries array missing";
        return false;
    }
    std::vector<NotificationHistoryEntry> loaded;
    std::uint64_t                         max_id = 0;
    for (const auto &j : doc["entries"]) {
        if (!j.is_object())
            continue;
        NotificationHistoryEntry e;
        e.id           = j.value("id", std::uint64_t(0));
        e.timestamp_ms = j.value("timestamp_ms", std::int64_t(0));
        e.level        = j.value("level", 0);
        e.level_name   = j.value("level_name", level_name(e.level));
        e.type_name    = j.value("type", std::string());
        e.text         = j.value("text", std::string());
        e.title        = j.value("title", first_line(e.text));
        e.dismissed    = j.value("dismissed", false);
        e.dismissed_ms = j.value("dismissed_ms", std::int64_t(0));
        e.seen         = j.value("seen", false);
        e.action       = j.value("action", std::string());
        if (e.id == 0)
            continue;
        max_id = std::max(max_id, e.id);
        loaded.push_back(std::move(e));
    }
    std::sort(loaded.begin(), loaded.end(),
              [](const NotificationHistoryEntry &a, const NotificationHistoryEntry &b) { return a.id < b.id; });
    m_entries = std::move(loaded);
    m_next_id = std::max(max_id + 1, doc.value("next_id", std::uint64_t(1)));
    trim();
    notify();
    return true;
}

bool NotificationHistory::save(const std::filesystem::path &file, std::string *error) const
{
    std::error_code ec;
    if (!file.parent_path().empty())
        std::filesystem::create_directories(file.parent_path(), ec);
    const std::filesystem::path tmp = file.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) {
            if (error)
                *error = "cannot open " + tmp.string() + " for writing";
            return false;
        }
        out << to_json();
        if (!out) {
            if (error)
                *error = "write to " + tmp.string() + " failed";
            return false;
        }
    }
    std::filesystem::rename(tmp, file, ec);
    if (ec) {
        // Fall back to copy+remove for filesystems that refuse the rename.
        std::filesystem::copy_file(tmp, file, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmp);
        if (ec) {
            if (error)
                *error = "cannot replace " + file.string() + ": " + ec.message();
            return false;
        }
    }
    return true;
}

bool NotificationHistory::load(const std::filesystem::path &file, std::string *error)
{
    std::error_code ec;
    if (!std::filesystem::exists(file, ec))
        return true;
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        if (error)
            *error = "cannot open " + file.string();
        return false;
    }
    std::stringstream buf;
    buf << in.rdbuf();
    return from_json(buf.str(), error);
}

} } // namespace Slic3r::GUI
