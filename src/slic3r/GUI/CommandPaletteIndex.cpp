#include "CommandPaletteIndex.hpp"

#include <algorithm>
#include <cstring>

namespace Slic3r::GUI::PaletteIndex {

// ---------------------------------------------------------------------------
// Accelerators
// ---------------------------------------------------------------------------

std::vector<wxAcceleratorEntry> main_frame_accelerators()
{
    std::vector<wxAcceleratorEntry> entries;
    entries.reserve(kNumpadTabCount + 1);
    for (int n = 1; n <= kNumpadTabCount; ++n)
        entries.emplace_back(wxACCEL_CTRL, WXK_NUMPAD0 + n, kNumpadTabBaseId + n - 1);
    entries.emplace_back(wxACCEL_CTRL | wxACCEL_SHIFT, 'F', kPaletteCommandId);
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
        "Appearance", "General", "User", "3D", "Other", "Developer Tools",
    };
    return names;
}

const std::vector<PreferenceEntry> &preference_entries()
{
    // Order follows the rows top-to-bottom inside each Preferences page.
    static const std::vector<PreferenceEntry> entries = {
        // Appearance ---------------------------------------------------------
        {"dark_color_mode", "Theme", "Light or dark appearance", PageAppearance},
        {"ui_density", "Density", "Comfortable or compact control spacing", PageAppearance},
        {"ui_accent_seed", "Accent color", "Seed color the interface is tinted with", PageAppearance},
        {"ui_font_family", "Font", "Interface typeface", PageAppearance},
        {"ui_font_scale", "Text size", "Interface text scale", PageAppearance},
        // General ------------------------------------------------------------
        {"language", "Language", "Interface language mode", PageGeneral},
        {"region", "Login Region", "Cloud login region", PageGeneral},
        {"use_inches", "Units", "Metric or imperial units", PageGeneral},
        {"auto_calculate_flush", "Auto Flush", "Auto calculate flush volumes", PageGeneral},
        {"prepare_sidebar_dock", "Prepare panel position", "Dock the Prepare panel on the left, right, top, or bottom", PageGeneral},
        {"single_instance", "Keep only one Bambu Studio instance", "", PageGeneral},
        {"studio_enable_fila_manager", "Filament Manager", "Take effect after restarting Studio", PageGeneral},
        {"enable_multi_machine", "Multi-device Management", "Take effect after restarting Studio", PageGeneral},
        {"enable_beta_version_update", "Support beta version update", "Receive beta version updates", PageGeneral},
        {"privacyuse", "Join the User Experience Improvement Program", "", PageGeneral},
        {"download_path", "Downloads", "Folder downloaded files are saved to", PageGeneral},
        {"external_editor", "External editor", "Editor used by File > Open in External Editor", PageGeneral},
        {"external_editor_path", "External editor path", "Executable used by the Custom external editor", PageGeneral},
        // User ---------------------------------------------------------------
        {"user_bed_type", "Auto plate type", "Remember the build plate selected last time per printer model", PageUser},
        {"use_12h_time_format", "time format for print progress", "12-hour or 24-hour clock", PageUser},
        {"auto_stop_liveview", "Keep liveview when printing", "", PageUser},
        {"auto_transfer_when_switch_preset", "Automatically transfer modified value when switching process and filament presets", "", PageUser},
        {"enable_high_low_temp_mixed_printing", "Remove the restriction on mixed printing of high and low temperature filaments", "", PageUser},
        {"sync_user_preset", "Auto sync user presets(Printer/Filament/Process)", "", PageUser},
        {"sync_system_preset", "Auto check for system presets updates", "", PageUser},
        {"webview_auto_fill", "Auto-fill previously logged-in accounts", "", PageUser},
        // 3D -----------------------------------------------------------------
        {"zoom_to_mouse", "Zoom to mouse position", "", Page3D},
        {"enable_assemble_view_preview", "Display overview", "", Page3D},
        {"grabber_size_factor", "Grabber scale", "Grabber size for move, rotate and scale tools", Page3D},
        {"3d_middle_tooltip_offset_x", "Tooltip offset", "Horizontal tooltip offset", Page3D},
        {"3d_middle_tooltip_offset_y", "Tooltip offset", "Vertical tooltip offset", Page3D},
        {"toolbar_style", "Toolbar Style", "Collapsible or uncollapsible toolbar", Page3D},
        {"show_shells_in_preview", "Always show shells in preview", "", Page3D},
        {"enable_step_mesh_setting", "Show the step mesh parameter setting dialog", "", Page3D},
        {"import_single_svg_and_split", "Import a single SVG and split it", "", Page3D},
        {"gamma_correct_in_import_obj", "Enable gamma correction for the imported obj file", "", Page3D},
        {"enable_record_gcodeviewer_option_item", "Remember last used color scheme", "", Page3D},
        {"enable_lod", "Improve rendering performance by lod", "", Page3D},
        {"enable_advanced_gcode_viewer_", "Enable advanced gcode viewer", "", Page3D},
        {"show_assembly_bvh_bounds", "Show assembly BVH primary bounds", "", Page3D},
        {"camera_fullscreen_active_monitor_only", "Open full screen camera view on active monitor only", "", Page3D},
        // Other --------------------------------------------------------------
        {"max_recent_count", "Maximum recent projects", "", PageOther},
        {"no_warn_when_modified_gcodes", "No warnings when loading 3MF with modified G-codes", "", PageOther},
        {"backup_interval", "Auto-Backup", "Backup period in seconds", PageOther},
        {"staff_pick_switch", "Show online staff-picked models on the home page", "", PageOther},
        {"show_print_history", "Show history on the home page", "", PageOther},
        {"printer_watch_enabled", "Watch the live view and notify about progress and failures", "AI printer watch", PageOther},
        {"printer_watch_model", "Local model tag", "Vision-capable Ollama tag", PageOther},
        {"printer_watch_interval", "Check interval (minutes)", "Minutes between live-view checks", PageOther},
        {"developer_mode", "Develop mode", "", PageOther},
        {"skip_ams_blacklist_check", "Skip AMS blacklist check", "", PageOther},
        {"associate_3mf", "Associate .3mf files to Bambu Studio", "", PageOther},
        {"associate_stl", "Associate .stl files to Bambu Studio", "", PageOther},
        {"associate_step", "Associate .step/.stp files to Bambu Studio", "", PageOther},
        {"severity_level", "Log Level", "", PageOther},
        // Developer Tools (non-public builds) ------------------------------------
        {"internal_developer_mode", "Internal developer mode", "", PageDeveloper},
        {"enable_ssl_for_mqtt", "Enable SSL(MQTT)", "", PageDeveloper},
        {"enable_ssl_for_ftp", "Enable SSL(FTP)", "", PageDeveloper},
        {"dev_host", "DEV host", "api-dev.bambu-lab.com/v1", PageDeveloper},
        {"qa_host", "QA host", "api-qa.bambu-lab.com/v1", PageDeveloper},
        {"pre_host", "PRE host", "api-pre.bambu-lab.com/v1", PageDeveloper},
        {"product_host", "Product host", "", PageDeveloper},
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
        {"Go to Home",         "Start page with recent projects",           0}, // tpHome
        {"Go to Prepare",      "Arrange models and set up the print",       1}, // tp3DEditor
        {"Go to Preview",      "Sliced result, layers and toolpaths",       2}, // tpPreview
        {"Go to Device",       "Printer status and control",                3}, // tpMonitor
        {"Go to Multi-device", "Manage several printers at once",           4}, // tpMultiDevice
        {"Go to Project",      "Project files and metadata",                5}, // tpProject
        {"Go to Calibration",  "Flow, pressure advance and temperature",    6}, // tpCalibration
        {"Go to Filament",     "Filament manager",                          9}, // tpFilamentManager
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
        {"docs/features/api/home-assistant-printer-discovery.md", "Home Assistant printer-discovery API"},
        {"docs/features/design-system/cheap-jor-inventory.md", "Layout clipping inventory"},
        {"docs/features/design-system/generated-visual-showcase.md", "Generated visual showcase"},
        {"docs/features/design-system/gizmo-rail-svg-icons-completion.md", "Gizmo rail composite SVG completion"},
        {"docs/features/design-system/kit-widgets-2026-09.md", "Kit widgets added in the every-element sweep (2026-09-05)"},
        {"docs/features/design-system/layout-probe.md", "Runtime layout probe"},
        {"docs/features/design-system/md3-design-system.md", "Vendored Material Design 3 design system"},
        {"docs/features/design-system/md3-parity-register.md", "MD3 parity register"},
        {"docs/features/design-system/themed-surface-colors.md", "Themed surface colors on StaticBox cards"},
        {"docs/features/gcode-preview/toolpath-legend.md", "Toolpath color-scheme legend"},
        {"docs/features/model-preview/makerworld-opengl-preview.md", "MakerWorld OpenGL preview"},
        {"docs/features/pages/changelog-viewer.md", "Changelog viewer"},
        {"docs/features/pages/deployment-and-layout-gate.md", "Deployment and the layout gate"},
        {"docs/features/pages/dim-sum-surprise.md", "Dim sum surprise"},
        {"docs/features/pages/language-and-funny-levels.md", "Language modes and funny levels"},
        {"docs/features/pages/notifications.md", "Notifications"},
        {"docs/features/pages/regex-builder.md", "Regex builder"},
        {"docs/features/pages/settings-and-appearance.md", "Settings and appearance"},
        {"docs/features/pages/tabbed-navigation.md", "Tabbed navigation"},
        {"docs/features/prepare/dockable-sidebar.md", "Dockable Prepare sidebar"},
        {"docs/features/prepare/process-settings-full-tree.md", "Every process setting shown, no Simple/Advanced filter"},
        {"docs/features/prepare/process-settings-sidebar.md", "Process settings in the Prepare sidebar"},
        {"docs/features/releases/release-codenames.md", "Release codenames"},
        {"docs/features/releases/windows-build-from-source.md", "Build from source (Windows installer)"},
        {"docs/features/releases/windows-native-installer.md", "Native Windows installer"},
        {"docs/features/releases/windows-one-click-build.md", "One-click Windows build and installer"},
        {"docs/features/releases/windows-release-supply-chain.md", "Windows CI and release supply chain"},
        {"docs/features/windows/ai-filament-scanner.md", "AI filament scanner"},
        {"docs/features/windows/ai-printer-watch.md", "AI printer watch (local models)"},
        {"docs/features/windows/app-updates.md", "App updates from this fork's releases"},
        {"docs/features/windows/appearance-customization.md", "Appearance customization"},
        {"docs/features/windows/bulk-filament-actions.md", "Bulk filament actions"},
        {"docs/features/windows/cloud-web-recovery.md", "Cloud web-page failure recovery"},
        {"docs/features/windows/command-palette.md", "Command palette (Ctrl+Shift+F)"},
        {"docs/features/windows/gui-accessibility.md", "Keyboard, assistive, and responsive GUI accessibility"},
        {"docs/features/windows/ink-terminology.md", "Ink terminology (filament \xE2\x86\x92 ink, AMS \xE2\x86\x92 Ink Dispenser)"},
        {"docs/features/windows/language-modes.md", "English, Hong Kong Cantonese, and bilingual modes"},
        {"docs/features/windows/md3-color-picker.md", "Material color picker & color translator"},
        {"docs/features/windows/md3-native-ui.md", "Native Material Design 3 UI on Windows"},
        {"docs/features/windows/native-visual-smoke.md", "Native Windows visual smoke test"},
        {"docs/features/windows/print-simulation.md", "Print simulation playback (feedrate-true)"},
        {"docs/features/windows/regex-builder.md", "Regex builder"},
        {"docs/features/windows/release-splash-art.md", "Release splash art (fresh dim sum per release)"},
        {"docs/features/windows/sidebar-search.md", "Prepare sidebar search"},
        {"docs/features/windows/smart-home.md", "Smart home: printer handover, TTS narrator, and alert lights"},
        {"docs/features/windows/software-gl-fallback.md", "Software OpenGL fallback (Mesa llvmpipe)"},
        {"docs/features/windows/stop-print-interlock.md", "Stop-print safety interlock"},
        {"docs/features/windows/windows-only-platform.md", "Windows-only platform policy"},
        {"docs/features/workspace/config-profiles-backup.md", "Config profiles & full-data backup"},
        {"docs/features/workspace/external-editor.md", "External editor"},
        {"docs/features/workspace/non-blocking-notifications.md", "Non-blocking notifications"},
        {"docs/features/workspace/preferences-history.md", "Preferences auto-history"},
        {"docs/features/workspace/project-tabs.md", "Browser-like project tabs"},
        {"docs/features/workspace/project-version-history.md", "Project version history"},
    };
    return articles;
}

std::string article_url(const Article &article)
{
    return std::string("https://github.com/Ding-Ding-Projects/BambuStudio/blob/main/") + article.path;
}

} // namespace Slic3r::GUI::PaletteIndex
