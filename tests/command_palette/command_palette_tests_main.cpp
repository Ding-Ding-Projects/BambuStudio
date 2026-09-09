#include <catch_main.hpp>

#include "slic3r/GUI/CommandPaletteIndex.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace Slic3r::GUI::PaletteIndex;

#ifndef COMMAND_PALETTE_TEST_SOURCE_DIR
#error "COMMAND_PALETTE_TEST_SOURCE_DIR must identify the repository root"
#endif

namespace {

const std::filesystem::path kSourceRoot{COMMAND_PALETTE_TEST_SOURCE_DIR};

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

bool looks_like_config_key(const std::string &literal)
{
    if (literal.size() < 3) return false;
    bool has_letter = false;
    for (const char c : literal) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            if (!std::islower(static_cast<unsigned char>(c))) return false;
            has_letter = true;
        } else if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '_')) {
            return false;
        }
    }
    return has_letter;
}

// Bare string literals inside `text` (a call's argument list) that are NOT
// the argument of a translation macro (_L( / L( / _CTX( ).
std::vector<std::string> bare_literals(const std::string &text)
{
    std::vector<std::string> out;
    size_t                   i = 0;
    while (i < text.size()) {
        if (text[i] != '"') { ++i; continue; }
        // Look back past whitespace for an opening translation macro.
        size_t j = i;
        while (j > 0 && std::isspace(static_cast<unsigned char>(text[j - 1]))) --j;
        const bool translated = (j >= 3 && text.compare(j - 3, 3, "_L(") == 0) ||
                                (j >= 2 && text.compare(j - 2, 2, "L(") == 0) ||
                                (j >= 5 && text.compare(j - 5, 5, "_CTX(") == 0);
        std::string literal;
        ++i;
        while (i < text.size() && text[i] != '"') {
            if (text[i] == '\\' && i + 1 < text.size()) { literal += text[i + 1]; i += 2; continue; }
            literal += text[i++];
        }
        ++i; // closing quote
        if (!translated) out.push_back(literal);
    }
    return out;
}

// Argument text of the call whose opening parenthesis is at `open`.
std::string call_arguments(const std::string &src, size_t open)
{
    int    depth = 0;
    bool   in_string = false;
    size_t i = open;
    for (; i < src.size(); ++i) {
        const char c = src[i];
        if (in_string) {
            if (c == '\\') { ++i; continue; }
            if (c == '"') in_string = false;
            continue;
        }
        if (c == '"') { in_string = true; continue; }
        if (c == '(') ++depth;
        else if (c == ')' && --depth == 0) break;
    }
    return src.substr(open + 1, i > open ? i - open - 1 : 0);
}

// Value of `constexpr const char *<ident> = "...";` from any header under
// src/slic3r/GUI; empty when the constant is not found.
std::string resolve_config_key_constant(const std::string &ident)
{
    const std::filesystem::path gui = kSourceRoot / "src" / "slic3r" / "GUI";
    for (const auto &entry : std::filesystem::recursive_directory_iterator(gui)) {
        if (!entry.is_regular_file()) continue;
        const std::string ext = entry.path().extension().string();
        if (ext != ".h" && ext != ".hpp") continue;
        const std::string src = read_file(entry.path());
        size_t            pos = src.find(ident);
        while (pos != std::string::npos) {
            size_t eq = src.find('=', pos + ident.size());
            if (eq != std::string::npos) {
                size_t open = src.find('"', eq);
                size_t nl   = src.find('\n', eq);
                if (open != std::string::npos && (nl == std::string::npos || open < nl)) {
                    const size_t close = src.find('"', open + 1);
                    if (close != std::string::npos) return src.substr(open + 1, close - open - 1);
                }
            }
            pos = src.find(ident, pos + ident.size());
        }
    }
    return {};
}

// Every AppConfig key Preferences.cpp binds a row to, derived from the source
// the way a reader would: the key argument of each create_item_* call, plus
// the explicit register_option_row("key", ...) registrations (Appearance rows,
// region, log level).
std::set<std::string> preference_keys_from_source()
{
    const std::string src = read_file(kSourceRoot / "src/slic3r/GUI/Preferences.cpp");
    std::set<std::string> keys;

    static const std::vector<std::string> row_builders = {
        "create_item_checkbox", "create_item_combobox", "create_item_input", "create_item_switch",
        "create_item_radiobox", "create_item_multiple_combobox", "create_item_range_input",
        "create_item_range_two_input", "create_item_language_mode_combobox",
        "create_item_language_combobox", "create_item_downloads", "create_item_external_editor",
    };
    for (const std::string &name : row_builders) {
        size_t pos = 0;
        while ((pos = src.find(name + "(", pos)) != std::string::npos) {
            const size_t open = pos + name.size();
            pos = open;
            // Skip the definition (`PreferencesDialog::create_item_x(`) and any
            // longer identifier that merely ends with this name.
            if (pos >= name.size() + 2 && src.compare(pos - name.size() - 2, 2, "::") == 0) continue;
            if (pos > name.size()) {
                const char before = src[pos - name.size() - 1];
                if (std::isalnum(static_cast<unsigned char>(before)) || before == '_') continue;
            }
            const std::string              args     = call_arguments(src, open);
            const std::vector<std::string> literals = bare_literals(args);
            size_t                         wanted   = name == "create_item_range_two_input" ? 2 : 1;
            for (const std::string &lit : literals) {
                if (!looks_like_config_key(lit)) continue;
                keys.insert(lit);
                if (--wanted == 0) break;
            }
            // A row may pass a named constant instead of a literal
            // (e.g. FilaManagerEnabledConfigKey); resolve it from the GUI headers.
            if (wanted > 0) {
                const std::string suffix = "ConfigKey";
                size_t            at     = 0;
                while ((at = args.find(suffix, at)) != std::string::npos) {
                    size_t start = at;
                    while (start > 0 && (std::isalnum(static_cast<unsigned char>(args[start - 1])) || args[start - 1] == '_')) --start;
                    const std::string ident = args.substr(start, at + suffix.size() - start);
                    at += suffix.size();
                    const std::string value = resolve_config_key_constant(ident);
                    INFO("unresolved config-key constant in Preferences.cpp: " << ident);
                    REQUIRE_FALSE(value.empty());
                    keys.insert(value);
                }
            }
        }
    }

    const std::string marker = "register_option_row(\"";
    size_t            pos    = 0;
    while ((pos = src.find(marker, pos)) != std::string::npos) {
        pos += marker.size();
        const size_t end = src.find('"', pos);
        REQUIRE(end != std::string::npos);
        keys.insert(src.substr(pos, end - pos));
    }
    return keys;
}

std::set<std::string> catalog_keys()
{
    std::set<std::string> keys;
    for (const PreferenceEntry &e : preference_entries()) keys.insert(e.key);
    return keys;
}

std::string first_heading(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::string   line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("# ", 0) == 0) return line.substr(2);
    }
    return {};
}

} // namespace

// ---------------------------------------------------------------------------
// Accelerators: one table, both families present.
// ---------------------------------------------------------------------------

TEST_CASE("Main frame accelerator table carries the numpad chords and Ctrl+Shift+F together", "[CommandPalette][accelerators]")
{
    const std::vector<wxAcceleratorEntry> entries = main_frame_accelerators();
    REQUIRE(entries.size() == size_t(kNumpadTabCount) + 2);

    for (int n = 1; n <= kNumpadTabCount; ++n) {
        const bool present = std::any_of(entries.begin(), entries.end(),
                                         [n](const wxAcceleratorEntry &e) { return is_numpad_tab_accelerator(e, n); });
        INFO("Ctrl+Numpad" << n);
        REQUIRE(present);
    }
    REQUIRE(std::count_if(entries.begin(), entries.end(), is_palette_accelerator) == 1);
    // F1 -> in-app documentation browser lives in the same single table.
    REQUIRE(std::count_if(entries.begin(), entries.end(), is_docs_accelerator) == 1);
    REQUIRE(std::string(docs_shortcut_label()) == "F1");

    // Ctrl+F (without Shift) is no longer a palette chord anywhere in the table.
    const bool ctrl_f = std::any_of(entries.begin(), entries.end(), [](const wxAcceleratorEntry &e) {
        return e.GetFlags() == wxACCEL_CTRL && e.GetKeyCode() == 'F';
    });
    REQUIRE_FALSE(ctrl_f);

    // No two entries share an id: a duplicate would make one chord unreachable.
    std::set<int> ids;
    for (const wxAcceleratorEntry &e : entries) ids.insert(e.GetCommand());
    REQUIRE(ids.size() == entries.size());

    REQUIRE(std::string(palette_shortcut_label()) == "Ctrl+Shift+F");
}

// ---------------------------------------------------------------------------
// Size choice persistence.
// ---------------------------------------------------------------------------

TEST_CASE("Palette size choice defaults to the card and round-trips through a store", "[CommandPalette][size]")
{
    std::map<std::string, std::string> store;
    auto get = [&store](const std::string &key) {
        const auto it = store.find(key);
        return it == store.end() ? std::string() : it->second;
    };
    auto set = [&store](const std::string &key, const std::string &value) { store[key] = value; };

    REQUIRE(load_palette_size(get) == PaletteSize::Card);          // unset -> default
    REQUIRE(parse_palette_size("garbage") == PaletteSize::Card);   // unknown -> default

    store_palette_size(PaletteSize::FullWindow, set);
    REQUIRE(store.count(kPaletteSizeKey) == 1);
    REQUIRE(store[kPaletteSizeKey] == "full");
    REQUIRE(load_palette_size(get) == PaletteSize::FullWindow);

    store_palette_size(PaletteSize::Card, set);
    REQUIRE(store[kPaletteSizeKey] == "card");
    REQUIRE(load_palette_size(get) == PaletteSize::Card);

    REQUIRE(parse_palette_size(palette_size_value(PaletteSize::FullWindow)) == PaletteSize::FullWindow);
    REQUIRE(parse_palette_size(palette_size_value(PaletteSize::Card)) == PaletteSize::Card);
}

// ---------------------------------------------------------------------------
// Teleport target resolution.
// ---------------------------------------------------------------------------

TEST_CASE("Teleport targets resolve a known setting key to its Preferences page", "[CommandPalette][teleport]")
{
    const PreferenceEntry *units = find_preference("use_inches");
    REQUIRE(units != nullptr);
    REQUIRE(units->page == PageGeneral);
    REQUIRE(std::string(units->title) == "Units");
    REQUIRE(std::string(preference_page_names()[units->page]) == "General");

    const PreferenceEntry *theme = find_preference("dark_color_mode");
    REQUIRE(theme != nullptr);
    REQUIRE(theme->page == PageAppearance);

    const PreferenceEntry *backup = find_preference("backup_interval");
    REQUIRE(backup != nullptr);
    REQUIRE(backup->page == PageOther);

    REQUIRE(find_preference("not_a_setting") == nullptr);
    REQUIRE(find_preference("") == nullptr);

    // Every entry names a page the dialog actually has.
    for (const PreferenceEntry &e : preference_entries()) {
        INFO(e.key);
        REQUIRE(e.page >= 0);
        REQUIRE(size_t(e.page) < preference_page_names().size());
        REQUIRE_FALSE(std::string(e.title).empty());
    }
}

// ---------------------------------------------------------------------------
// Completeness guards. The hand-written lists are the point: a rule-only test
// ("every entry present is well-formed") passes on an empty index.
// ---------------------------------------------------------------------------

TEST_CASE("Hand-written list of surfaces that MUST be reachable from the palette", "[CommandPalette][completeness]")
{
    // Preferences settings a user reaches for by name.
    static const std::vector<std::string> must_have_settings = {
        "dark_color_mode", "ui_density", "ui_accent_seed", "ui_font_family", "ui_font_scale",
        "language", "region", "use_inches", "prepare_sidebar_dock", "download_path",
        "external_editor", "external_editor_path", "single_instance",
        "use_12h_time_format", "sync_user_preset",
        "zoom_to_mouse", "toolbar_style", "enable_lod",
        "max_recent_count", "backup_interval", "developer_mode",
        "printer_watch_enabled", "printer_watch_model", "printer_watch_interval",
        "associate_3mf", "severity_level",
    };
    const std::set<std::string> keys = catalog_keys();
    for (const std::string &key : must_have_settings) {
        INFO("setting missing from the palette index: " << key);
        REQUIRE(keys.count(key) == 1);
    }

    // Every Preferences page is represented by at least one row.
    for (size_t page = 0; page < preference_page_names().size(); ++page) {
        INFO("no palette rows for Preferences page: " << preference_page_names()[page]);
        REQUIRE(std::any_of(preference_entries().begin(), preference_entries().end(),
                            [page](const PreferenceEntry &e) { return size_t(e.page) == page; }));
    }

    // Workspace tab destinations.
    static const std::vector<std::pair<std::string, int>> must_have_tabs = {
        {"Go to Home", 0}, {"Go to Prepare", 1}, {"Go to Preview", 2}, {"Go to Device", 3},
        {"Go to Multi-device", 4}, {"Go to Project", 5}, {"Go to Calibration", 6}, {"Go to Filament", 9},
    };
    for (const auto &[title, position] : must_have_tabs) {
        const auto it = std::find_if(workspace_tabs().begin(), workspace_tabs().end(),
                                     [&title](const WorkspaceTab &t) { return title == t.title; });
        INFO("workspace tab missing from the palette index: " << title);
        REQUIRE(it != workspace_tabs().end());
        REQUIRE(it->position == position);
    }

    // Documentation articles.
    static const std::vector<std::string> must_have_articles = {
        "docs/features/windows/command-palette.md",
        "docs/features/windows/regex-builder.md",
        "docs/features/windows/appearance-customization.md",
        "docs/features/windows/language-modes.md",
        "docs/features/windows/gui-accessibility.md",
        "docs/features/workspace/external-editor.md",
        "docs/features/workspace/project-version-history.md",
        "docs/features/workspace/non-blocking-notifications.md",
        "docs/features/prepare/dockable-sidebar.md",
        "docs/features/releases/windows-native-installer.md",
    };
    for (const std::string &path : must_have_articles) {
        const auto it = std::find_if(documentation_articles().begin(), documentation_articles().end(),
                                     [&path](const Article &a) { return path == a.path; });
        INFO("article missing from the palette index: " << path);
        REQUIRE(it != documentation_articles().end());
        REQUIRE(article_url(*it).rfind("https://", 0) == 0);
        REQUIRE(article_url(*it).find(path) != std::string::npos);
    }
}

TEST_CASE("Palette settings index matches every row Preferences.cpp builds", "[CommandPalette][completeness]")
{
    const std::set<std::string> from_source  = preference_keys_from_source();
    const std::set<std::string> from_catalog = catalog_keys();
    REQUIRE(from_source.size() >= 40); // the scan found the rows, not an empty file

    std::vector<std::string> missing_in_catalog, missing_in_source;
    std::set_difference(from_source.begin(), from_source.end(), from_catalog.begin(), from_catalog.end(),
                        std::back_inserter(missing_in_catalog));
    std::set_difference(from_catalog.begin(), from_catalog.end(), from_source.begin(), from_source.end(),
                        std::back_inserter(missing_in_source));

    for (const std::string &key : missing_in_catalog)
        WARN("Preferences.cpp binds a row to '" << key << "' but CommandPaletteIndex.cpp does not list it");
    for (const std::string &key : missing_in_source)
        WARN("CommandPaletteIndex.cpp lists '" << key << "' but no Preferences.cpp row binds it");
    REQUIRE(missing_in_catalog.empty());
    REQUIRE(missing_in_source.empty());

    // Keys are unique: two rows with one key would make the teleport ambiguous.
    REQUIRE(from_catalog.size() == preference_entries().size());
}

TEST_CASE("Palette article index matches docs/features on disk", "[CommandPalette][completeness]")
{
    std::map<std::string, std::string> on_disk; // repo-relative path -> H1
    const std::filesystem::path        docs = kSourceRoot / "docs" / "features";
    REQUIRE(std::filesystem::is_directory(docs));
    for (const auto &entry : std::filesystem::recursive_directory_iterator(docs)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".md") continue;
        if (entry.path().filename() == "README.md") continue; // category indexes, not articles
        std::string rel = std::filesystem::relative(entry.path(), kSourceRoot).generic_string();
        on_disk[rel]    = first_heading(entry.path());
    }
    REQUIRE(on_disk.size() >= 40);

    std::set<std::string> indexed;
    for (const Article &a : documentation_articles()) {
        indexed.insert(a.path);
        const auto it = on_disk.find(a.path);
        INFO("indexed article not on disk: " << a.path);
        REQUIRE(it != on_disk.end());
        INFO("H1 drifted for " << a.path << ": disk='" << it->second << "' index='" << a.title << "'");
        REQUIRE(it->second == a.title);
    }
    for (const auto &[path, title] : on_disk) {
        INFO("article on disk missing from the palette index: " << path << " (" << title << ")");
        REQUIRE(indexed.count(path) == 1);
    }
    REQUIRE(indexed.size() == documentation_articles().size()); // no duplicate paths
}
