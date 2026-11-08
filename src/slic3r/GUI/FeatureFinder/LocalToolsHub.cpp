#include "LocalToolsHub.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

// Only the gettext marker is needed here; the page translates with _() at
// display time. Defined locally so the hub stays free of wxWidgets.
#ifndef L
#define L(s) s
#endif

namespace Slic3r::GUI::LocalTools {

const std::vector<Group> &groups()
{
    static const std::vector<Group> list = {
        {"files", L("Files and data")},
        {"printer", L("Printer")},
        {"windows", L("Windows")},
        {"drawing", L("Drawing")},
        {"games", L("Games")},
        {"extensions", L("Extensions")},
        {"setup", L("Setup")},
        {"safety", L("Safety")},
    };
    return list;
}

// The summaries are the tools' own menu help, so the page and the menu say the
// same thing in every language.
const std::vector<Tool> &tools()
{
    static const std::vector<Tool> list = {
        {"base64", "files", L("Encode and decode text, files beside the originals and data: URLs on this computer")},
        {"converter", "files", L("Convert local files with verified bundled adapters")},
        {"model-suite", "files", L("Inspect local Ollama models, catalog availability and guided tools")},
        {"lan-drop", "files", L("Receive 3D models that people on your local network send through the drop site")},
        {"live-charts", "printer", L("Nozzle, bed and chamber temperatures of the selected printer over time")},
        {"melody", "printer", L("Compose a tune the printer plays, preview it, send it to the printer or add it to the start or end G-code")},
        {"filament-scanner", "printer", L("Snap a spool photo on your phone; a local model identifies it and loads an AMS slot")},
        {"smart-home", "printer", L("Home Assistant speakers, media controls, TTS narrator and alert lights")},
        {"virtual-disks", "windows", L("Every VHD, VHDX and ISO you add: attach, create, grow, compact, chains, merge and Quick pack")},
        {"drawing", "drawing", L("Draw on a capture of the plate with seven pens, versions and export, saved on this computer")},
        {"minecraft", "games", L("Create, adopt and run Java servers: console, every setting, players, versions, backups, schedules and a world generator")},
        {"extensions", "extensions", L("Steps after slicing, comment lines in exported G-code and commands of your own, each asking first")},
        {"onboarding", "setup", L("Language, look, printers, inks and backups in one guided pass that saves as it goes")},
        {"scheduled-settings", "setup", L("Schedule presentation without replacing stored preferences")},
        {"authenticator", "safety", L("Manage local time-based authentication codes")},
        {"identity-history", "safety", L("Enroll an independent history password and inspect encrypted identity changes")},
        {"support-tickets", "safety", L("Manage local-only support records; nothing is sent anywhere")},
        {"element-locks", "safety", L("Every element you locked, with Unlock beside each")},
    };
    return list;
}

const Tool *find_tool(std::string_view feature)
{
    for (const Tool &t : tools())
        if (feature == t.feature) return &t;
    return nullptr;
}

const Group *find_group(std::string_view id)
{
    for (const Group &g : groups())
        if (id == g.id) return &g;
    return nullptr;
}

std::size_t count_in(std::string_view group)
{
    return std::size_t(std::count_if(tools().begin(), tools().end(), [group](const Tool &t) { return group == t.group; }));
}

// ---------------------------------------------------------------------------
// Pins
// ---------------------------------------------------------------------------

std::vector<std::string> default_pins()
{
    return {"base64", "extensions", "onboarding", "virtual-disks", "minecraft", "drawing", "converter", "model-suite", "authenticator"};
}

std::vector<std::string> parse_pins(std::string_view stored, bool present)
{
    if (!present) return default_pins();
    std::vector<std::string> pins;
    std::size_t              start = 0;
    while (start <= stored.size() && pins.size() < kPinLimit) {
        std::size_t comma = stored.find(',', start);
        if (comma == std::string_view::npos) comma = stored.size();
        const std::string_view id = stored.substr(start, comma - start);
        if (find_tool(id) != nullptr && std::find(pins.begin(), pins.end(), id) == pins.end()) pins.emplace_back(id);
        if (comma == stored.size()) break;
        start = comma + 1;
    }
    return pins;
}

std::string serialize_pins(const std::vector<std::string> &pins)
{
    std::string out;
    for (const std::string &id : pins) {
        if (!out.empty()) out += ',';
        out += id;
    }
    return out;
}

bool is_pinned(const std::vector<std::string> &pins, std::string_view id)
{
    return std::find(pins.begin(), pins.end(), id) != pins.end();
}

std::vector<std::string> toggle_pin(std::vector<std::string> pins, std::string_view id)
{
    if (find_tool(id) == nullptr) return pins;
    const auto found = std::find(pins.begin(), pins.end(), id);
    if (found != pins.end()) {
        pins.erase(found);
        return pins;
    }
    if (pins.size() >= kPinLimit) return pins;
    pins.emplace_back(id);
    return pins;
}

std::vector<std::string> move_pin(std::vector<std::string> pins, std::string_view id, int delta)
{
    const auto found = std::find(pins.begin(), pins.end(), id);
    if (found == pins.end() || delta == 0) return pins;
    const long from = long(found - pins.begin());
    const long to   = std::clamp(from + long(delta), 0L, long(pins.size()) - 1);
    std::string moved = std::move(pins[std::size_t(from)]);
    pins.erase(pins.begin() + from);
    pins.insert(pins.begin() + to, std::move(moved));
    return pins;
}

std::string parse_filter(std::string_view stored)
{
    if (stored == kPinnedFilter || find_group(stored) != nullptr) return std::string(stored);
    return {};
}

std::vector<std::size_t> visible(std::string_view filter, const std::vector<std::string> &pins)
{
    const std::vector<Tool> &all = tools();
    std::vector<std::size_t> result;
    if (filter == kPinnedFilter) {
        for (const std::string &id : pins)
            for (std::size_t i = 0; i < all.size(); ++i)
                if (id == all[i].feature) result.push_back(i);
        return result;
    }
    for (std::size_t i = 0; i < all.size(); ++i)
        if (filter.empty() || filter == all[i].group) result.push_back(i);
    return result;
}

// ---------------------------------------------------------------------------
// Activity
// ---------------------------------------------------------------------------

std::string clean_text(std::string_view text, std::size_t limit)
{
    std::string out;
    out.reserve(std::min(text.size(), limit + 3));
    bool space = false;
    for (char c : text) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20 || u == 0x7F || c == ' ') {
            space = !out.empty();
            continue;
        }
        if (space) out += ' ';
        space = false;
        out += c;
    }
    if (out.size() <= limit) return out;
    std::size_t cut = limit >= 3 ? limit - 3 : 0;
    // Back to the first byte of a character, so no character is cut in half.
    while (cut > 0 && (static_cast<unsigned char>(out[cut]) & 0xC0) == 0x80) --cut;
    out.resize(cut);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out + "...";
}

std::string clean_result(std::string_view result)
{
    for (const char *known : {"Done", "Failed", "Cancelled", "Incomplete", "Started"})
        if (result == known) return known;
    return "Done";
}

std::string to_line(const Activity &activity)
{
    const nlohmann::json j = {{"time", activity.time},
                              {"tool", activity.tool},
                              {"action", activity.action},
                              {"subject", activity.subject},
                              {"result", activity.result}};
    // A name read from disk may not be valid UTF-8; replace rather than throw.
    return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

bool from_line(std::string_view line, Activity &activity)
{
    const nlohmann::json j = nlohmann::json::parse(line.begin(), line.end(), nullptr, false);
    if (!j.is_object()) return false;
    Activity a;
    try {
        a.time    = j.value("time", std::int64_t(0));
        a.tool    = j.value("tool", std::string());
        a.action  = clean_text(j.value("action", std::string()));
        a.subject = clean_text(j.value("subject", std::string()));
        a.result  = clean_result(j.value("result", std::string()));
    } catch (const nlohmann::json::exception &) {
        return false;
    }
    if (find_tool(a.tool) == nullptr || a.action.empty()) return false;
    activity = std::move(a);
    return true;
}

void ActivityLog::load()
{
    m_entries.clear();
    if (m_file.empty()) return;
    std::ifstream in(m_file, std::ios::binary);
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        Activity a;
        if (from_line(line, a)) m_entries.push_back(std::move(a));
    }
    if (m_entries.size() > kActivityLimit)
        m_entries.erase(m_entries.begin(), m_entries.end() - std::ptrdiff_t(kActivityLimit));
}

bool ActivityLog::append(Activity activity)
{
    activity.action  = clean_text(activity.action);
    activity.subject = clean_text(activity.subject);
    activity.result  = clean_result(activity.result);
    if (find_tool(activity.tool) == nullptr || activity.action.empty()) return false;
    m_entries.push_back(std::move(activity));
    if (m_entries.size() > kActivityLimit) {
#ifdef SLIC3R_LOCAL_TOOLS_HUB_NEGATIVE_FIXTURE
        // Test-only mutation (tests/local_tools_hub.test.mjs): the suite must fail.
        m_entries.pop_back();
#else
        m_entries.erase(m_entries.begin());
#endif
    }
    save();
    return true;
}

void ActivityLog::clear()
{
    m_entries.clear();
    if (m_file.empty()) return;
    std::error_code ec;
    std::filesystem::remove(m_file, ec);
}

std::vector<Activity> ActivityLog::newest(std::size_t limit, std::string_view tool) const
{
    std::vector<Activity> out;
    for (auto it = m_entries.rbegin(); it != m_entries.rend() && out.size() < limit; ++it)
        if (tool.empty() || tool == it->tool) out.push_back(*it);
    return out;
}

void ActivityLog::save() const
{
    if (m_file.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(m_file.parent_path(), ec);
    const std::filesystem::path partial = std::filesystem::path(m_file).concat(".partial");
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        for (const Activity &a : m_entries) out << to_line(a) << '\n';
        if (!out) return;
    }
    std::filesystem::rename(partial, m_file, ec);
}

} // namespace Slic3r::GUI::LocalTools
