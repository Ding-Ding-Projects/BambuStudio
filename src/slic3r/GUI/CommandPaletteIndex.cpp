#include "CommandPaletteIndex.hpp"

// Only the L() marker macro is used below (a compile-time no-op, see
// I18N.hpp), never a real translate call: tests/command_palette links this
// file with wx core+base only, not I18N.cpp/LanguageMode.cpp.
#include "I18N.hpp"

#include <algorithm>
#include <cstring>

namespace Slic3r::GUI::PaletteIndex {

// ---------------------------------------------------------------------------
// Accelerators
// ---------------------------------------------------------------------------

std::vector<wxAcceleratorEntry> main_frame_accelerators()
{
    std::vector<wxAcceleratorEntry> entries;
    entries.reserve(kNumpadTabCount + 2);
    for (int n = 1; n <= kNumpadTabCount; ++n)
        entries.emplace_back(wxACCEL_CTRL, WXK_NUMPAD0 + n, kNumpadTabBaseId + n - 1);
    entries.emplace_back(wxACCEL_CTRL | wxACCEL_SHIFT, 'F', kPaletteCommandId);
    entries.emplace_back(wxACCEL_CTRL | wxACCEL_SHIFT, 'E', kAppearanceEditorCommandId);
    return entries;
}

bool is_palette_accelerator(const wxAcceleratorEntry &entry)
{
    return entry.GetFlags() == (wxACCEL_CTRL | wxACCEL_SHIFT) && entry.GetKeyCode() == 'F' &&
           entry.GetCommand() == kPaletteCommandId;
}

bool is_numpad_tab_accelerator(const wxAcceleratorEntry &entry, int n)
{
    return n >= 1 && n <= kNumpadTabCount && entry.GetFlags() == wxACCEL_CTRL &&
           entry.GetKeyCode() == WXK_NUMPAD0 + n && entry.GetCommand() == kNumpadTabBaseId + n - 1;
}

const char *palette_shortcut_label() { return "Ctrl+Shift+F"; }

// ---------------------------------------------------------------------------
// Persisted size choice
// ---------------------------------------------------------------------------

PaletteSize parse_palette_size(const std::string &stored)
{
    return stored == "full" ? PaletteSize::FullWindow : PaletteSize::Card;
}

std::string palette_size_value(PaletteSize size)
{
    return size == PaletteSize::FullWindow ? "full" : "card";
}

PaletteSize load_palette_size(const std::function<std::string(const std::string &)> &get)
{
    return parse_palette_size(get ? get(kPaletteSizeKey) : std::string());
}

void store_palette_size(PaletteSize size,
                        const std::function<void(const std::string &, const std::string &)> &set)
{
    if (set)
        set(kPaletteSizeKey, palette_size_value(size));
}

// ---------------------------------------------------------------------------
// Preferences settings index
// ---------------------------------------------------------------------------

const std::vector<const char *> &preference_page_names()
{
    static const std::vector<const char *> names = {
        L("Appearance"), L("General"), L("User"), L("3D"), L("Other"), L("Developer Tools"),
    };
    return names;
}

const std::vector<PreferenceEntry> &preference_entries()
{
    // Order follows the rows top-to-bottom inside each Preferences page.
    static const std::vector<PreferenceEntry> entries = {
        // Appearance ---------------------------------------------------------
        {"dark_color_mode", L("Theme"), L("Light or dark appearance"), PageAppearance},
        {"ui_density", L("Density"), L("Comfortable or compact control spacing"), PageAppearance},
        {"ui_accent_seed", L("Accent color"), L("Seed color the interface is tinted with"), PageAppearance},
        {"ui_font_family", L("Font"), L("Interface typeface"), PageAppearance},
        {"ui_font_scale", L("Text size"), L("Interface text scale"), PageAppearance},
        // General ------------------------------------------------------------
        {"language", L("Language"), L("Interface language mode"), PageGeneral},
        {"region", L("Login Region"), L("Cloud login region"), PageGeneral},
        {"use_inches", L("Units"), L("Metric or imperial units"), PageGeneral},
        {"auto_calculate_flush", L("Auto Flush"), L("Auto calculate flush volumes"), PageGeneral},
        {"prepare_sidebar_dock", L("Prepare panel position"), L("Dock the Prepare panel on the left, right, top, or bottom"), PageGeneral},
        {"single_instance", L("Keep only one Bambu Studio instance"), "", PageGeneral},
        {"studio_enable_fila_manager", L("Filament Manager"), L("Take effect after restarting Studio"), PageGeneral},
        {"enable_multi_machine", L("Multi-device Management"), L("Take effect after restarting Studio"), PageGeneral},
        {"enable_beta_version_update", L("Support beta version update"), L("Receive beta version updates"), PageGeneral},
        {"privacyuse", L("Join the User Experience Improvement Program"), "", PageGeneral},
        {"download_path", L("Downloads"), L("Folder downloaded files are saved to"), PageGeneral},
        {"external_editor", L("External editor"), L("Editor used by File > Open in External Editor"), PageGeneral},
        {"external_editor_path", L("External editor path"), L("Executable used by the Custom external editor"), PageGeneral},
        // User ---------------------------------------------------------------
        {"user_bed_type", L("Auto plate type"), L("Remember the build plate selected last time per printer model"), PageUser},
        {"use_12h_time_format", L("time format for print progress"), L("12-hour or 24-hour clock"), PageUser},
        {"auto_stop_liveview", L("Keep liveview when printing"), "", PageUser},
        {"auto_transfer_when_switch_preset", L("Automatically transfer modified value when switching process and filament presets"), "", PageUser},
        {"enable_high_low_temp_mixed_printing", L("Remove the restriction on mixed printing of high and low temperature filaments"), "", PageUser},
        {"sync_user_preset", L("Auto sync user presets(Printer/Filament/Process)"), "", PageUser},
        {"sync_system_preset", L("Auto check for system presets updates"), "", PageUser},
        {"webview_auto_fill", L("Auto-fill previously logged-in accounts"), "", PageUser},
        // 3D -----------------------------------------------------------------
        {"zoom_to_mouse", L("Zoom to mouse position"), "", Page3D},
        {"enable_assemble_view_preview", L("Display overview"), "", Page3D},
        {"grabber_size_factor", L("Grabber scale"), L("Grabber size for move, rotate and scale tools"), Page3D},
        {"3d_middle_tooltip_offset_x", L("Tooltip offset"), L("Horizontal tooltip offset"), Page3D},
        {"3d_middle_tooltip_offset_y", L("Tooltip offset"), L("Vertical tooltip offset"), Page3D},
        {"toolbar_style", L("Toolbar Style"), L("Collapsible or uncollapsible toolbar"), Page3D},
        {"show_shells_in_preview", L("Always show shells in preview"), "", Page3D},
        {"enable_step_mesh_setting", L("Show the step mesh parameter setting dialog"), "", Page3D},
        {"import_single_svg_and_split", L("Import a single SVG and split it"), "", Page3D},
        {"gamma_correct_in_import_obj", L("Enable gamma correction for the imported obj file"), "", Page3D},
        {"enable_record_gcodeviewer_option_item", L("Remember last used color scheme"), "", Page3D},
        {"enable_lod", L("Improve rendering performance by lod"), "", Page3D},
        {"enable_advanced_gcode_viewer_", L("Enable advanced gcode viewer"), "", Page3D},
        {"show_assembly_bvh_bounds", L("Show assembly BVH primary bounds"), "", Page3D},
        {"camera_fullscreen_active_monitor_only", L("Open full screen camera view on active monitor only"), "", Page3D},
        // Other --------------------------------------------------------------
        {"max_recent_count", L("Maximum recent projects"), "", PageOther},
        {"no_warn_when_modified_gcodes", L("No warnings when loading 3MF with modified G-codes"), "", PageOther},
        {"backup_interval", L("Auto-Backup"), L("Backup period in seconds"), PageOther},
        {"staff_pick_switch", L("Show online staff-picked models on the home page"), "", PageOther},
        {"show_print_history", L("Show history on the home page"), "", PageOther},
        {"printer_watch_enabled", L("Watch the live view and notify about progress and failures"), L("AI printer watch"), PageOther},
        {"printer_watch_model", L("Local model tag"), L("Vision-capable Ollama tag"), PageOther},
        {"printer_watch_interval", L("Check interval (minutes)"), L("Minutes between live-view checks"), PageOther},
        {"developer_mode", L("Develop mode"), "", PageOther},
        {"skip_ams_blacklist_check", L("Skip AMS blacklist check"), "", PageOther},
        {"associate_3mf", L("Associate .3mf files to Bambu Studio"), "", PageOther},
        {"associate_stl", L("Associate .stl files to Bambu Studio"), "", PageOther},
        {"associate_step", L("Associate .step/.stp files to Bambu Studio"), "", PageOther},
        {"severity_level", L("Log Level"), "", PageOther},
        // Developer Tools (non-public builds) ------------------------------------
        {"internal_developer_mode", L("Internal developer mode"), "", PageDeveloper},
        {"enable_ssl_for_mqtt", L("Enable SSL(MQTT)"), "", PageDeveloper},
        {"enable_ssl_for_ftp", L("Enable SSL(FTP)"), "", PageDeveloper},
        {"dev_host", L("DEV host"), "api-dev.bambu-lab.com/v1", PageDeveloper},
        {"qa_host", L("QA host"), "api-qa.bambu-lab.com/v1", PageDeveloper},
        {"pre_host", L("PRE host"), "api-pre.bambu-lab.com/v1", PageDeveloper},
        {"product_host", L("Product host"), "", PageDeveloper},
    };
    return entries;
}

const PreferenceEntry *find_preference(const std::string &key)
{
    const auto &entries = preference_entries();
    const auto  it      = std::find_if(entries.begin(), entries.end(),
                                       [&key](const PreferenceEntry &e) { return key == e.key; });
    return it == entries.end() ? nullptr : &*it;
}

// ---------------------------------------------------------------------------
// Workspace tabs
// ---------------------------------------------------------------------------

const std::vector<WorkspaceTab> &workspace_tabs()
{
    // Positions are MainFrame::TabPosition values; the palette checks the
    // page exists before offering a row (Multi-device / Filament are gated).
    static const std::vector<WorkspaceTab> tabs = {
        {L("Go to Home"),         L("Start page with recent projects"),           0}, // tpHome
        {L("Go to Prepare"),      L("Arrange models and set up the print"),       1}, // tp3DEditor
        {L("Go to Preview"),      L("Sliced result, layers and toolpaths"),       2}, // tpPreview
        {L("Go to Device"),       L("Printer status and control"),                3}, // tpMonitor
        {L("Go to Multi-device"), L("Manage several printers at once"),           4}, // tpMultiDevice
        {L("Go to Project"),      L("Project files and metadata"),                5}, // tpProject
        {L("Go to Calibration"),  L("Flow, pressure advance and temperature"),    6}, // tpCalibration
        {L("Go to Filament"),     L("Filament manager"),                          9}, // tpFilamentManager
    };
    return tabs;
}

// ---------------------------------------------------------------------------
// Documentation articles
// ---------------------------------------------------------------------------

const std::vector<Article> &documentation_articles()
{
    // Kept in step with docs/features/**/*.md by the directory-scan guard in
    // tests/command_palette: an article on disk that is missing here, or whose
    // H1 changed, fails that test.
    static const std::vector<Article> articles = {
        {"docs/features/api/home-assistant-printer-discovery.md", L("Home Assistant printer-discovery API")},
        {"docs/features/design-system/clipping-inventory.md", L("Layout clipping inventory")},
        {"docs/features/design-system/generated-visual-showcase.md", L("Generated visual showcase")},
        {"docs/features/design-system/gizmo-rail-svg-icons-completion.md", L("Gizmo rail composite SVG completion")},
        {"docs/features/design-system/kit-widgets-2026-09.md", L("Kit widgets added in the every-element sweep (2026-09-05)")},
        {"docs/features/design-system/layout-probe.md", L("Runtime layout probe")},
        {"docs/features/design-system/md3-design-system.md", L("Vendored Material Design 3 design system")},
        {"docs/features/design-system/md3-parity-register.md", L("MD3 parity register")},
        {"docs/features/design-system/themed-surface-colors.md", L("Themed surface colors on StaticBox cards")},
        {"docs/features/gcode-preview/toolpath-legend.md", L("Toolpath color-scheme legend")},
        {"docs/features/model-preview/makerworld-opengl-preview.md", L("MakerWorld OpenGL preview")},
        {"docs/features/pages/changelog-viewer.md", L("Changelog viewer")},
        {"docs/features/pages/deployment-and-layout-gate.md", L("Deployment and the layout gate")},
        {"docs/features/pages/dim-sum-surprise.md", L("Dim sum surprise")},
        {"docs/features/pages/language-and-funny-levels.md", L("Language modes and funny levels")},
        {"docs/features/pages/notifications.md", L("Notifications")},
        {"docs/features/pages/regex-builder.md", L("Regex builder")},
        {"docs/features/pages/settings-and-appearance.md", L("Settings and appearance")},
        {"docs/features/pages/tabbed-navigation.md", L("Tabbed navigation")},
        {"docs/features/prepare/dockable-sidebar.md", L("Dockable Prepare sidebar")},
        {"docs/features/prepare/process-settings-full-tree.md", L("Every process setting shown, no Simple/Advanced filter")},
        {"docs/features/prepare/process-settings-sidebar.md", L("Process settings in the Prepare sidebar")},
        {"docs/features/releases/release-codenames.md", L("Release codenames")},
        {"docs/features/releases/windows-build-from-source.md", L("Build from source (Windows installer)")},
        {"docs/features/releases/windows-native-installer.md", L("Native Windows installer")},
        {"docs/features/releases/windows-one-click-build.md", L("One-click Windows build and installer")},
        {"docs/features/releases/windows-release-supply-chain.md", L("Windows CI and release supply chain")},
        {"docs/features/windows/ai-filament-scanner.md", L("AI filament scanner")},
        {"docs/features/windows/ai-printer-watch.md", L("AI printer watch (local models)")},
        {"docs/features/windows/app-updates.md", L("App updates from this fork's releases")},
        {"docs/features/windows/appearance-customization.md", L("Appearance customization")},
        {"docs/features/windows/bulk-filament-actions.md", L("Bulk filament actions")},
        {"docs/features/windows/cloud-web-recovery.md", L("Cloud web-page failure recovery")},
        {"docs/features/windows/command-palette.md", L("Command palette (Ctrl+Shift+F)")},
        {"docs/features/windows/gui-accessibility.md", L("Keyboard, assistive, and responsive GUI accessibility")},
        {"docs/features/windows/ink-terminology.md", L("Ink terminology (filament \xE2\x86\x92 ink, AMS \xE2\x86\x92 Ink Dispenser)")},
        {"docs/features/windows/language-modes.md", L("English, Hong Kong Cantonese, and bilingual modes")},
        {"docs/features/windows/md3-color-picker.md", L("Material color picker & color translator")},
        {"docs/features/windows/md3-native-ui.md", L("Native Material Design 3 UI on Windows")},
        {"docs/features/windows/native-visual-smoke.md", L("Native Windows visual smoke test")},
        {"docs/features/windows/print-simulation.md", L("Print simulation playback (feedrate-true)")},
        {"docs/features/windows/regex-builder.md", L("Regex builder")},
        {"docs/features/windows/release-splash-art.md", L("Release splash art (fresh dim sum per release)")},
        {"docs/features/windows/sidebar-search.md", L("Prepare sidebar search")},
        {"docs/features/windows/smart-home.md", L("Smart home: printer handover, TTS narrator, and alert lights")},
        {"docs/features/windows/software-gl-fallback.md", L("Software OpenGL fallback (Mesa llvmpipe)")},
        {"docs/features/windows/stop-print-interlock.md", L("Stop-print safety interlock")},
        {"docs/features/windows/windows-only-platform.md", L("Windows-only platform policy")},
        {"docs/features/workspace/config-profiles-backup.md", L("Config profiles & full-data backup")},
        {"docs/features/workspace/external-editor.md", L("External editor")},
        {"docs/features/workspace/non-blocking-notifications.md", L("Non-blocking notifications")},
        {"docs/features/workspace/preferences-history.md", L("Preferences auto-history")},
        {"docs/features/workspace/project-tabs.md", L("Browser-like project tabs")},
        {"docs/features/workspace/project-version-history.md", L("Project version history")},
    };
    return articles;
}

std::string article_url(const Article &article, bool cantonese)
{
    std::string path = article.path;
    static const std::string extension = ".md";
    if (cantonese && path.size() > extension.size() &&
        path.compare(path.size() - extension.size(), extension.size(), extension) == 0)
        path.insert(path.size() - extension.size(), ".yue_HK");
    return std::string("https://github.com/Ding-Ding-Projects/BambuStudio/blob/main/") + path;
}

} // namespace Slic3r::GUI::PaletteIndex
