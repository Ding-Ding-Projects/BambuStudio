#include "Changelog.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace Slic3r::Changelog {

// ---------------------------------------------------------------------------
// CivilDate
// ---------------------------------------------------------------------------

bool CivilDate::is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int CivilDate::days_in_month(int year, int month)
{
    static constexpr int lengths[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12)
        return 0;
    if (month == 2 && is_leap_year(year))
        return 29;
    return lengths[month - 1];
}

bool CivilDate::is_valid() const
{
    return year >= 1 && year <= 9999 && month >= 1 && month <= 12 && day >= 1 && day <= days_in_month(year, month);
}

namespace {

// Splits "a<sep>b<sep>c" into exactly three all-digit fields. Returns false on
// anything else: a missing field is partial input, a letter is not a date.
bool split_three_numeric_fields(std::string_view text, std::string fields[3])
{
    // Trim surrounding whitespace.
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
        text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.remove_suffix(1);
    if (text.empty())
        return false;

    int index = 0;
    for (char c : text) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            if (fields[index].size() >= 4)
                return false;
            fields[index].push_back(c);
        } else if (c == '/' || c == '-' || c == '.' || c == ' ') {
            if (fields[index].empty() || index == 2)
                return false; // empty field or too many separators
            ++index;
        } else {
            return false;
        }
    }
    return index == 2 && !fields[0].empty() && !fields[1].empty() && !fields[2].empty();
}

std::optional<CivilDate> make_date(int year, int month, int day)
{
    CivilDate date{year, month, day};
    if (!date.is_valid())
        return std::nullopt;
    return date;
}

} // namespace

std::optional<CivilDate> CivilDate::from_iso(std::string_view text)
{
    std::string fields[3];
    if (!split_three_numeric_fields(text, fields))
        return std::nullopt;
    // ISO is year first with a four-digit year; the other fields are 1 or 2 digits.
    if (fields[0].size() != 4 || fields[1].size() > 2 || fields[2].size() > 2)
        return std::nullopt;
    return make_date(std::stoi(fields[0]), std::stoi(fields[1]), std::stoi(fields[2]));
}

std::string CivilDate::to_iso() const
{
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
    return buffer;
}

// Days-from-civil / civil-from-days (Howard Hinnant's algorithms), so no
// dependency on the C library's time zone state.
long CivilDate::to_days() const
{
    long y = year - (month <= 2 ? 1 : 0);
    const long era = (y >= 0 ? y : y - 399) / 400;
    const long yoe = y - era * 400;
    const long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

CivilDate CivilDate::from_days(long z)
{
    z += 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const long doe = z - era * 146097;
    const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long y   = yoe + era * 400;
    const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long mp  = (5 * doy + 2) / 153;
    const long d   = doy - (153 * mp + 2) / 5 + 1;
    const long m   = mp + (mp < 10 ? 3 : -9);
    return CivilDate{static_cast<int>(y + (m <= 2 ? 1 : 0)), static_cast<int>(m), static_cast<int>(d)};
}

bool operator==(const CivilDate &a, const CivilDate &b) { return a.year == b.year && a.month == b.month && a.day == b.day; }
bool operator!=(const CivilDate &a, const CivilDate &b) { return !(a == b); }
bool operator<(const CivilDate &a, const CivilDate &b) { return a.to_days() < b.to_days(); }
bool operator<=(const CivilDate &a, const CivilDate &b) { return !(b < a); }
bool operator>(const CivilDate &a, const CivilDate &b) { return b < a; }
bool operator>=(const CivilDate &a, const CivilDate &b) { return !(a < b); }

std::optional<CivilDate> parse_typed_date(std::string_view text, DateOrder order)
{
    // ISO always wins: it is unambiguous regardless of locale.
    if (auto iso = CivilDate::from_iso(text))
        return iso;

    std::string fields[3];
    if (!split_three_numeric_fields(text, fields))
        return std::nullopt;

    int year_index;
    int month_index;
    int day_index;
    switch (order) {
    case DateOrder::YearMonthDay: year_index = 0; month_index = 1; day_index = 2; break;
    case DateOrder::DayMonthYear: year_index = 2; month_index = 1; day_index = 0; break;
    case DateOrder::MonthDayYear: year_index = 2; month_index = 0; day_index = 1; break;
    default: return std::nullopt;
    }
    // A two-digit year is ambiguous ("26" could be 1926 or 2026); refuse it.
    if (fields[year_index].size() != 4 || fields[month_index].size() > 2 || fields[day_index].size() > 2)
        return std::nullopt;
    return make_date(std::stoi(fields[year_index]), std::stoi(fields[month_index]), std::stoi(fields[day_index]));
}

// ---------------------------------------------------------------------------
// Document
// ---------------------------------------------------------------------------

namespace {

using json = nlohmann::json;

std::string require_string(const json &object, const char *key, const std::string &context)
{
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string())
        throw std::runtime_error(context + ": missing or non-string field \"" + key + "\"");
    return it->get<std::string>();
}

std::string optional_string(const json &object, const char *key)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_string() ? it->get<std::string>() : std::string{};
}

bool optional_bool(const json &object, const char *key)
{
    const auto it = object.find(key);
    return it != object.end() && it->is_boolean() && it->get<bool>();
}

bool is_full_sha(const std::string &sha)
{
    return sha.size() == 40 && std::all_of(sha.begin(), sha.end(), [](unsigned char c) {
               return std::isxdigit(c) != 0;
           });
}

} // namespace

Document parse_document(const std::string &json_text)
{
    json root;
    try {
        root = json::parse(json_text);
    } catch (const std::exception &e) {
        throw std::runtime_error(std::string("changelog.json is not valid JSON: ") + e.what());
    }
    if (!root.is_object())
        throw std::runtime_error("changelog.json: top level is not an object");

    Document document;
    const auto schema = root.find("schema");
    if (schema == root.end() || !schema->is_number_integer())
        throw std::runtime_error("changelog.json: missing integer field \"schema\"");
    document.schema = schema->get<int>();
    if (document.schema != 1)
        throw std::runtime_error("changelog.json: unsupported schema version " + std::to_string(document.schema));

    document.repository          = require_string(root, "repository", "changelog.json");
    document.commit_url_template = require_string(root, "commitUrlTemplate", "changelog.json");
    if (document.commit_url_template.find("{sha}") == std::string::npos)
        throw std::runtime_error("changelog.json: commitUrlTemplate carries no {sha} placeholder");
    document.generated           = optional_string(root, "generated");
    document.category_derivation = optional_string(root, "categoryDerivation");

    const auto releases = root.find("releases");
    if (releases == root.end() || !releases->is_array())
        throw std::runtime_error("changelog.json: missing array field \"releases\"");

    for (const json &item : *releases) {
        if (!item.is_object())
            throw std::runtime_error("changelog.json: a release is not an object");
        Release release;
        release.tag     = require_string(item, "tag", "changelog.json release");
        const std::string context = "changelog.json release " + release.tag;
        release.version = require_string(item, "version", context);
        const auto ordinal = item.find("ordinal");
        release.ordinal = ordinal != item.end() && ordinal->is_number_integer() ? ordinal->get<int>() : 0;

        const std::string date_text = require_string(item, "date", context);
        const auto date = CivilDate::from_iso(date_text);
        if (!date)
            throw std::runtime_error(context + ": date \"" + date_text + "\" is not an ISO calendar date");
        release.date      = *date;
        release.published = optional_string(item, "published");

        const auto code_name = item.find("codeName");
        if (code_name != item.end() && code_name->is_object()) {
            release.code_name_en  = optional_string(*code_name, "en");
            release.code_name_yue = optional_string(*code_name, "yue");
        }
        release.qualifier   = optional_string(item, "qualifier");
        release.url         = optional_string(item, "url");
        release.commit      = optional_string(item, "commit");
        release.build       = optional_string(item, "build");
        release.prerelease  = optional_bool(item, "prerelease");
        release.baseline    = optional_bool(item, "baseline");
        release.same_commit = optional_bool(item, "sameCommit");

        const auto entries = item.find("entries");
        if (entries == item.end() || !entries->is_array())
            throw std::runtime_error(context + ": missing array field \"entries\"");
        for (const json &entry_json : *entries) {
            if (!entry_json.is_object())
                throw std::runtime_error(context + ": an entry is not an object");
            Entry entry;
            entry.sha = require_string(entry_json, "sha", context + " entry");
            if (!is_full_sha(entry.sha))
                throw std::runtime_error(context + ": entry sha \"" + entry.sha + "\" is not a full 40-hex commit id");
            entry.short_sha = optional_string(entry_json, "short");
            if (entry.short_sha.empty())
                entry.short_sha = entry.sha.substr(0, 9);
            entry.text     = require_string(entry_json, "text", context + " entry " + entry.short_sha);
            entry.category = optional_string(entry_json, "category");
            if (entry.category.empty())
                entry.category = "changed";
            release.entries.push_back(std::move(entry));
        }
        document.releases.push_back(std::move(release));
    }
    return document;
}

Document load_document(const std::string &path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream)
        throw std::runtime_error("changelog.json could not be opened: " + path);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return parse_document(buffer.str());
}

std::string commit_url(const Document &document, const std::string &sha)
{
    std::string url = document.commit_url_template;
    const auto pos = url.find("{sha}");
    if (pos != std::string::npos)
        url.replace(pos, 5, sha);
    return url;
}

// ---------------------------------------------------------------------------
// DateRange
// ---------------------------------------------------------------------------

bool DateRange::contains(const CivilDate &date) const
{
    if (from && date < *from)
        return false;
    if (to && date > *to)
        return false;
    return true;
}

std::string DateRange::describe() const
{
    if (from && to)
        return from->to_iso() + " to " + to->to_iso();
    if (from)
        return "from " + from->to_iso();
    if (to)
        return "until " + to->to_iso();
    return "all versions";
}

DateRange DateRange::all() { return DateRange{}; }

DateRange DateRange::last_days(const CivilDate &today, int days)
{
    DateRange range;
    range.from = today.plus_days(-static_cast<long>(std::max(days, 1)) + 1);
    range.to   = today;
    return range;
}

DateRange DateRange::this_year(const CivilDate &today)
{
    DateRange range;
    range.from = CivilDate{today.year, 1, 1};
    range.to   = today;
    return range;
}

// ---------------------------------------------------------------------------
// Filtering and export
// ---------------------------------------------------------------------------

std::vector<FilteredRelease> filter_releases(const Document &document, const DateRange &range, const TextMatcher &matcher)
{
    std::vector<FilteredRelease> result;
    for (const Release &release : document.releases) {
        if (!range.contains(release.date))
            continue;
        FilteredRelease filtered;
        filtered.release = &release;
        if (!matcher) {
            for (const Entry &entry : release.entries)
                filtered.entries.push_back(&entry);
            result.push_back(std::move(filtered));
            continue;
        }
        const bool header_matches = matcher(release.version) || matcher(release.tag) ||
                                    (!release.code_name_en.empty() && matcher(release.code_name_en)) ||
                                    (!release.code_name_yue.empty() && matcher(release.code_name_yue));
        for (const Entry &entry : release.entries) {
            if (header_matches || matcher(entry.text) || matcher(entry.short_sha))
                filtered.entries.push_back(&entry);
        }
        if (!filtered.entries.empty() || (header_matches && release.entries.empty()))
            result.push_back(std::move(filtered));
    }
    return result;
}

std::size_t count_entries(const std::vector<FilteredRelease> &releases)
{
    std::size_t count = 0;
    for (const FilteredRelease &release : releases)
        count += release.entries.size();
    return count;
}

namespace {

std::string category_title(const std::string &category)
{
    if (category.empty())
        return "Changed";
    std::string title = category;
    title[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(title[0])));
    return title;
}

} // namespace

std::string export_text(const Document &document, const std::vector<FilteredRelease> &releases, const DateRange &range,
                        const std::string &search_description, ExportFormat format)
{
    const bool markdown = format == ExportFormat::Markdown;
    std::ostringstream out;

    if (markdown)
        out << "# Changelog — " << document.repository << "\n\n";
    else
        out << "Changelog — " << document.repository << "\n" << std::string(60, '=') << "\n\n";

    out << (markdown ? "- " : "") << "Exported range: " << range.describe() << "\n";
    out << (markdown ? "- " : "") << "Search: " << (search_description.empty() ? "none" : search_description) << "\n";
    out << (markdown ? "- " : "") << "Versions: " << releases.size() << ", changes: " << count_entries(releases) << "\n";
    if (!document.generated.empty())
        out << (markdown ? "- " : "") << "Data generated: " << document.generated << "\n";
    out << "\n";

    if (releases.empty()) {
        out << "No versions match the exported range and search.\n";
        return out.str();
    }

    for (const FilteredRelease &filtered : releases) {
        const Release &release = *filtered.release;
        std::string heading = release.version;
        if (!release.code_name_en.empty()) {
            heading += " — " + release.code_name_en;
            if (!release.code_name_yue.empty())
                heading += " " + release.code_name_yue;
        }
        heading += " (" + release.date.to_iso() + ")";
        if (markdown) {
            out << "## " << heading << "\n\n";
            if (!release.url.empty())
                out << "Release: <" << release.url << ">\n\n";
        } else {
            out << heading << "\n" << std::string(heading.size(), '-') << "\n";
            if (!release.url.empty())
                out << "Release: " << release.url << "\n";
        }

        if (filtered.entries.empty()) {
            if (release.baseline)
                out << (markdown ? "_" : "") << "Oldest release: no earlier release to compare against."
                    << (markdown ? "_" : "") << "\n";
            else if (release.same_commit)
                out << (markdown ? "_" : "") << "Tagged the same commit as the previous release."
                    << (markdown ? "_" : "") << "\n";
            else
                out << (markdown ? "_" : "") << "No commits recorded for this release."
                    << (markdown ? "_" : "") << "\n";
        }
        for (const Entry *entry : filtered.entries) {
            if (markdown)
                out << "- **" << category_title(entry->category) << "**: " << entry->text << " ([`" << entry->short_sha
                    << "`](" << commit_url(document, entry->sha) << ") " << entry->sha << ")\n";
            else
                out << "- [" << category_title(entry->category) << "] " << entry->text << " (" << entry->sha << ")\n";
        }
        out << "\n";
    }
    return out.str();
}

} // namespace Slic3r::Changelog
