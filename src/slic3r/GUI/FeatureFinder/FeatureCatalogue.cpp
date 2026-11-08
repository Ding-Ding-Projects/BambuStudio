#include "FeatureCatalogue.hpp"

#include <algorithm>
#include <cctype>
#include <set>

// Only the gettext marker is needed here; the page translates with _() at
// display time. Defined locally so the catalogue stays free of wxWidgets.
#ifndef L
#define L(s) s
#endif

namespace Slic3r::GUI::FeatureFinder {

// Generated from the design catalogue (design/local-tools-2026-11). Keep the
// order: it is the Group order of the Every feature page.
const std::vector<Category> &categories()
{
    static const std::vector<Category> list = {
        {"language-voice", L("Language and voice")},
        {"look", L("Look")},
        {"find", L("Find and navigate")},
        {"records", L("Records")},
        {"tools", L("Tools")},
        {"safety", L("Safety")},
    };
    return list;
}

const std::vector<Feature> &features()
{
    static const std::vector<Feature> list = {
        // Language and voice
        {"language-mode", "language-voice", 0xE8E2 /* translate */, L("Language and Cantonese modes"), L("Preferences, Language"), "", "English Cantonese bilingual \xE5\xBB\xA3\xE6\x9D\xB1\xE8\xA9\xB1 \xE7\xB2\xB5\xE8\xAA\x9E translation", Badge::None, State::Ready, "prefs:language"},
        {"funny-levels", "language-voice", 0xE815 /* sentiment_very_satisfied */, L("Funny levels and emoji"), L("Preferences, Funny level"), "", "humour humor jokes tone emoji playful serious", Badge::None, State::Ready, "prefs:funny_level_en"},
        {"personal-vocabulary", "language-voice", 0xE8CE /* spellcheck */, L("Personal wording file"), L("Preferences, Personal vocabulary, or drop a .json on the window"), "", "wording words json drop file vocabulary dictionary", Badge::Fixed, State::Ready, "prefs:personal_vocabulary"},
        {"school-mode", "language-voice", 0xE80C /* school */, L("School mode"), L("Preferences, Presentation mode"), "", "presentation classroom passcode lock", Badge::None, State::Ready, "prefs:school_mode"},
        {"narrator", "language-voice", 0xE91F /* record_voice_over */, L("Narrator and voices"), L("Preferences, Narrator"), "", "speech text to speech voice read aloud TTS", Badge::None, State::Ready, "prefs:narrator_enabled"},
        {"action-sounds", "language-voice", 0xE050 /* volume_up */, L("Action sounds"), L("Preferences, Sound effects"), "", "sound effects audio clicks", Badge::None, State::Ready, "prefs:action_sounds_enabled"},
        {"scheduled-settings", "language-voice", 0xEB7B /* event_repeat */, L("Scheduled settings"), L("File, Scheduled settings"), "", "schedule time of day automatic", Badge::None, State::Ready, "command:scheduled-settings"},
        {"app-name", "language-voice", 0xEA67 /* badge */, L("Rename the app"), L("Preferences, App name"), "", "rename title display name", Badge::None, State::Ready, "prefs:app_display_name"},
        {"printer-wording", "language-voice", 0xE555 /* print */, L("Your wording on the printer"), L("Preferences, Use my wording in job names sent to printers"), "", "printer job name vocabulary send print wording", Badge::New, State::Ready, "prefs:vocabulary_job_names"},
        {"attention-modes", "language-voice", 0xEA78 /* self_improvement */, L("Attention modes"), L("Planned for Preferences"), "", "focus ADHD low stimulation time awareness", Badge::None, State::NotBuilt, ""},
        {"dim-sum", "language-voice", 0xEA64 /* ramen_dining */, L("Dim sum surprise"), L("Now and then at start-up"), "", "surprise startup food photo", Badge::None, State::Ready, "docs:windows/dim-sum-surprise.md"},
        // Look
        {"appearance-editor", "look", 0xE243 /* format_paint */, L("Edit appearance of any element"), L("Focus an element, then the shortcut"), "Ctrl+Shift+E", "style colours fonts per element customise customize", Badge::None, State::Ready, "docs:windows/appearance-editor.md"},
        {"colour-picker", "look", 0xE3B8 /* colorize */, L("Colour picker"), L("Inside Edit appearance"), "", "color hex RGB HSL contrast eyedropper", Badge::None, State::Ready, "docs:windows/color-picker-translator.md"},
        {"theme", "look", 0xE3B7 /* palette */, L("Theme, density and accent"), L("Preferences, Appearance"), "", "dark light density compact accent colour color", Badge::None, State::Ready, "prefs:dark_color_mode"},
        {"app-logo", "look", 0xE251 /* image */, L("App logo"), L("Preferences, App logo"), "", "icon image picture brand", Badge::None, State::Ready, "prefs:app_logo"},
        {"motion", "look", 0xE71C /* animation */, L("Motion and reduced motion"), L("Preferences, Interface motion"), "", "animation reduced motion transitions", Badge::None, State::Ready, "prefs:motion_preference"},
        {"live-charts", "look", 0xF190 /* monitoring */, L("Live charts"), L("Device page, temperatures"), "", "graph temperature chart realtime", Badge::New, State::NotBuilt, ""},
        // Find and navigate
        {"command-palette", "find", 0xEAE7 /* keyboard_command_key */, L("Command palette"), L("Anywhere"), "Ctrl+Shift+F", "search commands run anything", Badge::None, State::Ready, "palette"},
        {"regex-builder", "find", 0xF750 /* regular_expression */, L("Regex builder"), L("The .* button in any search"), "", "regular expression pattern search", Badge::None, State::Ready, "docs:windows/regex-builder.md"},
        {"keyboard-shortcuts", "find", 0xE312 /* keyboard */, L("Keyboard shortcuts"), L("Help, Keyboard Shortcuts"), "Shift+?", "hotkeys keys shortcuts", Badge::None, State::Ready, "command:keyboard-shortcuts"},
        {"printer-setup", "find", 0xE555 /* print */, L("Printer and filament setup"), L("Help, Setup Wizard"), "", "wizard printers filaments first run", Badge::None, State::Ready, "command:setup-wizard"},
        {"context-menus", "find", 0xF706 /* right_click */, L("Context menus with live controls"), L("Right-click anything"), "", "right click menu", Badge::New, State::NotBuilt, ""},
        {"info-icons", "find", 0xE88E /* info */, L("Info icons and explanations"), L("Beside every Preferences setting"), "", "help explanation info button", Badge::New, State::Ready, "command:preferences"},
        {"onboarding", "find", 0xEB9B /* rocket_launch */, L("Practical setup wizard"), L("Help, Set up Bambu Studio"), "", "wizard first run welcome onboarding resume", Badge::New, State::Ready, "command:practical-setup"},
        // Records
        {"version-history", "records", 0xE28E /* history */, L("Version history"), L("File, Version history"), "", "git snapshots restore project versions", Badge::None, State::Ready, "command:version-history"},
        {"changelog", "records", 0xEF49 /* campaign */, L("What is new"), L("Help, What's new / Changelog"), "", "release notes what is new", Badge::None, State::Ready, "command:changelog"},
        {"config-backup", "records", 0xE864 /* backup */, L("Config profiles and backup"), L("File, Config profiles & backup"), "", "export import profiles backup restore", Badge::None, State::Ready, "command:config-profiles"},
        {"notifications", "records", 0xE7F4 /* notifications */, L("Notification centre"), L("Bell in the title bar"), "", "bell alerts messages history", Badge::None, State::Ready, "command:notification-centre"},
        {"offline-docs", "records", 0xEA19 /* menu_book */, L("Offline documentation"), L("Help, Offline documentation"), "F1", "help guide manual documentation", Badge::None, State::Ready, "command:offline-docs"},
        {"external-editor", "records", 0xE745 /* edit_note */, L("Open in your editor"), L("File, Open in External Editor"), "", "VS Code editor folder open", Badge::None, State::Ready, "command:external-editor"},
        {"status-hub", "records", 0xE9F4 /* hub */, L("Status Hub"), L("Help, Status Hub"), "", "status delivery enrollment", Badge::None, State::Ready, "command:status-hub"},
        // Tools
        {"base64", "tools", 0xEAD3 /* data_object */, L("Base64 toolkit"), L("File, Base64 toolkit"), "", "encode decode data URL sha256 b64", Badge::New, State::Ready, "command:base64"},
        {"extensions", "tools", 0xE87B /* extension */, L("Extensions"), L("File, Extensions"), "", "plugins add-ons scripts hooks post-processing", Badge::New, State::Ready, "command:extensions"},
        {"converter", "tools", 0xE428 /* transform */, L("Local file converter"), L("File, Local file converter"), "", "convert file formats", Badge::None, State::Ready, "command:converter"},
        {"model-suite", "tools", 0xE322 /* memory */, L("Local model suite"), L("File, Local model suite"), "", "Ollama local AI models", Badge::None, State::Ready, "command:model-suite"},
        {"virtual-disks", "tools", 0xF80E /* hard_drive */, L("Virtual disks"), L("File, Virtual disks"), "", "VHD VHDX ISO disk Windows", Badge::New, State::NotBuilt, ""},
        {"minecraft", "tools", 0xE875 /* dns */, L("Minecraft servers and worlds"), L("File, Minecraft servers"), "", "server world game", Badge::New, State::NotBuilt, ""},
        {"drawing", "tools", 0xE746 /* draw */, L("Drawing board"), L("File, Drawing board"), "", "draw pen annotate sketch", Badge::New, State::NotBuilt, ""},
        {"melody", "tools", 0xE3A1 /* music_note */, L("Melody creator"), L("File, Melody creator"), "", "music tune song M1006 beep", Badge::None, State::Ready, "command:melody"},
        {"lan-drop", "tools", 0xE1E2 /* wifi_tethering */, L("LAN model drop"), L("File, LAN model drop"), "", "network share receive models", Badge::None, State::Ready, "command:lan-drop"},
        {"smart-home", "tools", 0xE283 /* home_iot_device */, L("Smart home"), L("File, Smart home"), "", "Home Assistant speakers lights", Badge::None, State::Ready, "command:smart-home"},
        {"filament-scanner", "tools", 0xE5FA /* document_scanner */, L("AI filament scanner"), L("File, AI filament scanner"), "", "spool photo AI identify", Badge::None, State::Ready, "command:filament-scanner"},
        // Safety
        {"support-tickets", "safety", 0xF0E2 /* support_agent */, L("Local support tickets"), L("File, Local support tickets"), "", "help support local", Badge::None, State::Ready, "command:local-support"},
        {"authenticator", "safety", 0xF045 /* pin */, L("Authenticator and QR pairing"), L("File, Authenticator"), "", "TOTP two factor codes QR", Badge::None, State::Ready, "command:authenticator"},
        {"identity-history", "safety", 0xE90D /* fingerprint */, L("Identity history"), L("File, Identity history"), "", "history password encrypted", Badge::None, State::Ready, "command:identity-history"},
        {"super-confirmation", "safety", 0xF014 /* gpp_maybe */, L("Two-key super confirmation"), L("Every destructive action"), "", "two keys slider delete destructive", Badge::None, State::Ready, "docs:windows/super-confirmation.md"},
        {"element-locks", "safety", 0xE88D /* lock */, L("Element locks"), L("Right-click a control, Lock this element"), "", "lock protect control", Badge::None, State::NotBuilt, ""},
        {"unlock-ladder", "safety", 0xE898 /* lock_open */, L("Unlock ladder"), L("Offered after wrong codes on a lock"), "", "unlock wrong code lockout", Badge::None, State::NotBuilt, ""},
    };
    return list;
}

const Feature *find(std::string_view id)
{
    for (const Feature &f : features())
        if (id == f.id) return &f;
    return nullptr;
}

const Category *find_category(std::string_view id)
{
    for (const Category &c : categories())
        if (id == c.id) return &c;
    return nullptr;
}

std::size_t count_in(std::string_view category_id)
{
    return std::size_t(std::count_if(features().begin(), features().end(),
                                     [category_id](const Feature &f) { return category_id == f.category; }));
}

std::size_t count_state(State state)
{
    return std::size_t(std::count_if(features().begin(), features().end(),
                                     [state](const Feature &f) { return f.state == state; }));
}

Action parse_action(std::string_view action)
{
    Action result;
    const auto take = [&](std::string_view prefix, ActionKind kind) {
        if (action.size() <= prefix.size() || action.substr(0, prefix.size()) != prefix) return false;
        result.kind   = kind;
        result.target = std::string(action.substr(prefix.size()));
        return true;
    };
    if (action == "palette") {
        result.kind = ActionKind::Palette;
        return result;
    }
    if (take("prefs:", ActionKind::Preference) || take("command:", ActionKind::Command) ||
        take("docs:", ActionKind::Documentation))
        return result;
    return Action{};
}

bool passes_filter(const Feature &feature, std::string_view filter)
{
    if (filter.empty()) return true;
    if (filter == kNotBuiltFilter) return feature.state == State::NotBuilt;
    return filter == feature.category;
}

namespace {
char fold(char c) { return char(std::tolower(static_cast<unsigned char>(c))); }
} // namespace

SortOrder parse_sort(std::string_view stored)
{
    if (stored == "a-z") return SortOrder::AToZ;
    if (stored == "recent") return SortOrder::Recent;
    return SortOrder::Group;
}

const char *sort_value(SortOrder order)
{
    switch (order) {
    case SortOrder::AToZ: return "a-z";
    case SortOrder::Recent: return "recent";
    case SortOrder::Group: break;
    }
    return "group";
}

std::vector<std::size_t> order(SortOrder sort, const std::vector<std::string> &recent,
                               const std::function<std::string(const Feature &)> &title_of)
{
    const std::vector<Feature> &all = features();
    std::vector<std::size_t>    result;
    result.reserve(all.size());

    if (sort == SortOrder::Group) {
        // Categories in their order, features in theirs, even if the table is
        // ever edited out of category order.
        for (const Category &c : categories())
            for (std::size_t i = 0; i < all.size(); ++i)
                if (std::string_view(c.id) == all[i].category) result.push_back(i);
        return result;
    }

    for (std::size_t i = 0; i < all.size(); ++i) result.push_back(i);

    if (sort == SortOrder::AToZ) {
        std::vector<std::string> keys(all.size());
        for (std::size_t i = 0; i < all.size(); ++i) {
            std::string key = title_of ? title_of(all[i]) : std::string(all[i].title);
            for (char &c : key) c = fold(c);
            keys[i] = std::move(key);
        }
        std::stable_sort(result.begin(), result.end(), [&](std::size_t a, std::size_t b) {
            if (keys[a] != keys[b]) return keys[a] < keys[b];
            return std::string_view(all[a].id) < std::string_view(all[b].id);
        });
        return result;
    }

    // Recent: recently opened first, most recent first, then catalogue order.
    std::vector<std::size_t> front;
    std::set<std::size_t>    taken;
    for (const std::string &id : recent)
        for (std::size_t i = 0; i < all.size(); ++i)
            if (id == all[i].id && taken.insert(i).second) front.push_back(i);
    for (std::size_t i : order(SortOrder::Group, recent, title_of))
        if (!taken.count(i)) front.push_back(i);
    return front;
}

std::vector<std::string> parse_recent(std::string_view stored)
{
    std::vector<std::string> ids;
    std::size_t              start = 0;
    while (start <= stored.size() && ids.size() < kRecentLimit) {
        std::size_t comma = stored.find(',', start);
        if (comma == std::string_view::npos) comma = stored.size();
        std::string_view id = stored.substr(start, comma - start);
        const Feature   *f  = find(id);
        if (f != nullptr && f->state == State::Ready &&
            std::find(ids.begin(), ids.end(), id) == ids.end())
            ids.emplace_back(id);
        if (comma == stored.size()) break;
        start = comma + 1;
    }
    return ids;
}

std::string serialize_recent(const std::vector<std::string> &ids)
{
    std::string out;
    for (const std::string &id : ids) {
        if (!out.empty()) out += ',';
        out += id;
    }
    return out;
}

std::vector<std::string> touch_recent(std::vector<std::string> recent, std::string_view id)
{
    const Feature *f = find(id);
    if (f == nullptr || f->state != State::Ready) return recent;
    recent.erase(std::remove(recent.begin(), recent.end(), id), recent.end());
#ifdef SLIC3R_FEATURE_CATALOGUE_NEGATIVE_FIXTURE
    // Test-only mutation (tests/feature_catalogue.test.mjs): the suite must fail.
    recent.emplace_back(id);
#else
    recent.insert(recent.begin(), std::string(id));
#endif
    if (recent.size() > kRecentLimit) recent.resize(kRecentLimit);
    return recent;
}

} // namespace Slic3r::GUI::FeatureFinder
