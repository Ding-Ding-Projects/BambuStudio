#include "ExternalEditor.hpp"

#include <algorithm>

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/system/error_code.hpp>

#include <wx/string.h>
#include <wx/tokenzr.h>
#include <wx/utils.h>

#ifdef _WIN32
#include <wx/msw/registry.h>
#endif

namespace Slic3r {
namespace GUI {

namespace {

// Convert a UTF-8 path into a boost path without losing non-ASCII characters on Windows.
boost::filesystem::path u8_to_path(const std::string &path_u8)
{
#ifdef _WIN32
    return boost::filesystem::path(wxString::FromUTF8(path_u8.c_str()).wc_str());
#else
    return boost::filesystem::path(path_u8);
#endif
}

// Convert a boost path back into a UTF-8 string.
std::string path_to_u8(const boost::filesystem::path &path)
{
#ifdef _WIN32
    return std::string(wxString(path.wstring()).ToUTF8().data());
#else
    return path.string();
#endif
}

// Check file existence without throwing.
bool file_exists(const boost::filesystem::path &path)
{
    boost::system::error_code ec;
    return boost::filesystem::exists(path, ec) && !ec;
}

// A shim executable to look for in the directories listed in the PATH environment variable.
struct PathShim
{
    // File name to look for in each PATH directory.
    const char *shim;
    // Optional path relative to the shim's directory pointing to the real executable
    // (e.g. VS Code puts "bin\\code.cmd" on the PATH while the launchable binary is
    // "..\\Code.exe"), nullptr to launch the found shim itself.
    const char *resolve;
};

// Split the PATH environment variable into a list of directories (UTF-8).
std::vector<std::string> path_environment_directories()
{
    std::vector<std::string> dirs;
    wxString path_env;
    if (!wxGetEnv("PATH", &path_env))
        return dirs;
#ifdef _WIN32
    static const wxString separators = ";";
#else
    static const wxString separators = ":";
#endif
    wxStringTokenizer tokenizer(path_env, separators, wxTOKEN_STRTOK);
    while (tokenizer.HasMoreTokens()) {
        wxString dir = tokenizer.GetNextToken().Trim(true).Trim(false);
        if (!dir.IsEmpty())
            dirs.emplace_back(dir.ToUTF8().data());
    }
    return dirs;
}

// Scan the PATH directories for one of the given shims, resolving it to the launchable
// executable. Returns an empty string when nothing was found.
std::string find_editor_on_path(const std::vector<std::string> &path_dirs, const std::vector<PathShim> &shims)
{
    for (const std::string &dir : path_dirs) {
        for (const PathShim &shim : shims) {
            boost::filesystem::path candidate = u8_to_path(dir) / shim.shim;
            if (!file_exists(candidate))
                continue;
            if (shim.resolve == nullptr)
                return path_to_u8(candidate);
            boost::filesystem::path resolved = (candidate.parent_path() / shim.resolve).lexically_normal();
            if (file_exists(resolved))
                return path_to_u8(resolved);
            // The shim exists but does not lead to a launchable executable, keep looking.
        }
    }
    return std::string();
}

#ifdef _WIN32

// Which registry value of the uninstall key holds the executable location.
enum class InstallLocationKey {
    InstallLocation, // "InstallLocation" is a directory, joined with the executable shim sub-paths
    DisplayIcon      // "DisplayIcon" contains the executable path itself (possibly ",<icon index>" suffixed)
};

// A single uninstall registry key candidate of an editor.
struct RegistryProbe
{
    bool        current_user; // HKEY_CURRENT_USER when true, HKEY_LOCAL_MACHINE otherwise
    bool        wow64;        // probe the WOW6432Node (32-bit) registry view
    const char *subkey;       // subkey below ...\CurrentVersion\Uninstall
};

struct WindowsEditor
{
    const char                *name;
    std::vector<RegistryProbe> registry_probes;
    InstallLocationKey         location_key;
    // Executable shim paths relative to InstallLocation (InstallLocationKey::InstallLocation only).
    std::vector<const char *>  shim_paths;
    // Fallback shims to scan the PATH for when the registry lookup comes up empty.
    std::vector<PathShim>      path_shims;
};

// Well-known Windows editors, transcribed from GitHub Desktop's app/src/lib/editors/win32.ts
// (registry GUIDs of the uninstall keys and the executable shim names).
const std::vector<WindowsEditor>& known_windows_editors()
{
    static const std::vector<WindowsEditor> editors = {
        { "Visual Studio Code",
          { { true,  false, "{771FD6B0-FA20-440A-A002-3B3BAC16DC50}_is1" },   // 64-bit (user)
            { true,  false, "{D628A17A-9713-46BF-8D57-E671B46A741E}_is1" },   // 32-bit (user)
            { true,  false, "{D9E514E7-1A56-452D-9337-2990C0DC4310}_is1" },   // ARM64 (user)
            { false, false, "{EA457B21-F73E-494C-ACAB-524FDE069978}_is1" },   // 64-bit (system)
            { false, true,  "{F8A2A208-72B3-4D61-95FC-8A65D340689B}_is1" },   // 32-bit (system)
            { false, false, "{A5270FC5-65AD-483E-AC30-2C276B63D0AC}_is1" } }, // ARM64 (system)
          InstallLocationKey::InstallLocation,
          { "Code.exe" },
          { { "Code.exe", nullptr }, { "code.cmd", "..\\Code.exe" } } },
        { "Visual Studio Code (Insiders)",
          { { true,  false, "{217B4C08-948D-4276-BFBB-BEE930AE5A2C}_is1" },   // 64-bit (user)
            { true,  false, "{26F4A15E-E392-4887-8C09-7BC55712FD5B}_is1" },   // 32-bit (user)
            { true,  false, "{69BD8F7B-65EB-4C6F-A14E-44CFA83712C0}_is1" },   // ARM64 (user)
            { false, false, "{1287CAD5-7C8D-410D-88B9-0D1EE4A83FF2}_is1" },   // 64-bit (system)
            { false, true,  "{C26E74D1-022E-4238-8B9D-1E7564A36CC9}_is1" },   // 32-bit (system)
            { false, false, "{0AEDB616-9614-463B-97D7-119DD86CCA64}_is1" } }, // ARM64 (system)
          InstallLocationKey::InstallLocation,
          { "Code - Insiders.exe" },
          { { "Code - Insiders.exe", nullptr }, { "code-insiders.cmd", "..\\Code - Insiders.exe" } } },
        { "VSCodium",
          { { true,  false, "{2E1F05D1-C245-4562-81EE-28188DB6FD17}_is1" },   // 64-bit (user)
            { true,  false, "{0FD05EB4-651E-4E78-A062-515204B47A3A}_is1" },   // 32-bit (user), new key
            { true,  false, "{57FD70A5-1B8D-4875-9F40-C5553F094828}_is1" },   // ARM64 (user), new key
            { false, false, "{88DA3577-054F-4CA1-8122-7D820494CFFB}_is1" },   // 64-bit (system), new key
            { false, true,  "{763CBF88-25C6-4B10-952F-326AE657F16B}_is1" },   // 32-bit (system), new key
            { false, false, "{67DEE444-3D04-4258-B92A-BC1F0FF2CAE4}_is1" },   // ARM64 (system), new key
            { true,  false, "{C6065F05-9603-4FC4-8101-B9781A25D88E}}_is1" },  // 32-bit (user), old key
            { true,  false, "{3AEBF0C8-F733-4AD4-BADE-FDB816D53D7B}_is1" },   // ARM64 (user), old key
            { false, false, "{D77B7E06-80BA-4137-BCF4-654B95CCEBC5}_is1" },   // 64-bit (system), old key
            { false, true,  "{E34003BB-9E10-4501-8C11-BE3FAA83F23F}_is1" },   // 32-bit (system), old key
            { false, false, "{D1ACE434-89C5-48D1-88D3-E2991DF85475}_is1" } }, // ARM64 (system), old key
          InstallLocationKey::InstallLocation,
          { "VSCodium.exe" },
          { { "VSCodium.exe", nullptr }, { "codium.cmd", "..\\VSCodium.exe" } } },
        { "Sublime Text",
          { { false, false, "Sublime Text_is1" },     // Sublime Text 4 (and newer?)
            { false, false, "Sublime Text 3_is1" } }, // Sublime Text 3
          InstallLocationKey::InstallLocation,
          { "subl.exe" },
          { { "subl.exe", nullptr }, { "sublime_text.exe", nullptr } } },
        { "Notepad++",
          { { false, false, "Notepad++" },   // 64-bit
            { false, true,  "Notepad++" } }, // 32-bit
          InstallLocationKey::DisplayIcon,
          {},
          { { "notepad++.exe", nullptr } } },
        { "Cursor",
          { { true,  false, "62625861-8486-5be9-9e46-1da50df5f8ff" },
            { true,  false, "{DADADADA-ADAD-ADAD-ADAD-ADADADADADAD}}_is1" },
            { true,  false, "{DBDBDBDB-BDBD-BDBD-BDBD-BDBDBDBDBDBD}}_is1" } }, // ARM64
          InstallLocationKey::DisplayIcon,
          {},
          { { "Cursor.exe", nullptr }, { "cursor.cmd", "..\\Cursor.exe" } } },
        { "Windsurf",
          { { true,  false, "{5A8B7D94-9B5F-4D1F-93FC-5609F7159349}_is1" } },
          InstallLocationKey::DisplayIcon,
          {},
          { { "Windsurf.exe", nullptr }, { "windsurf.cmd", "..\\Windsurf.exe" } } },
        { "Zed",
          { { true,  false, "{2DB0DA96-CA55-49BB-AF4F-64AF36A86712}_is1" } },
          InstallLocationKey::DisplayIcon,
          {},
          { { "zed.exe", nullptr } } },
    };
    return editors;
}

// Read a REG_SZ value from an open registry key, empty string when missing.
std::string read_registry_string(const wxRegKey &key, const wxString &value_name)
{
    if (!key.HasValue(value_name))
        return std::string();
    wxString value;
    if (!key.QueryValue(value_name, value))
        return std::string();
    return std::string(value.ToUTF8().data());
}

// A DisplayIcon value may carry an icon index after a comma ("C:\Path\app.exe,0")
// and surrounding quotes; reduce it to the bare executable path.
std::string clean_display_icon_path(std::string value)
{
    size_t comma = value.find(',');
    if (comma != std::string::npos)
        value.erase(comma);
    value.erase(std::remove(value.begin(), value.end(), '"'), value.end());
    return value;
}

// Probe the uninstall registry keys of a single editor.
// Returns the executable path (UTF-8) or an empty string.
std::string find_editor_in_registry(const WindowsEditor &editor)
{
    for (const RegistryProbe &probe : editor.registry_probes) {
        wxString subkey = probe.wow64 ? "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\"
                                      : "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\";
        subkey += wxString::FromUTF8(probe.subkey);
        wxRegKey key(probe.current_user ? wxRegKey::HKCU : wxRegKey::HKLM, subkey);
        if (!key.Exists())
            continue;
        if (editor.location_key == InstallLocationKey::DisplayIcon) {
            std::string exe = clean_display_icon_path(read_registry_string(key, "DisplayIcon"));
            if (!exe.empty() && file_exists(u8_to_path(exe)))
                return exe;
        } else {
            std::string install_location = read_registry_string(key, "InstallLocation");
            if (install_location.empty())
                continue;
            for (const char *shim : editor.shim_paths) {
                boost::filesystem::path exe = u8_to_path(install_location) / shim;
                if (file_exists(exe))
                    return path_to_u8(exe);
            }
        }
    }
    return std::string();
}

std::vector<FoundEditor> detect_installed_editors()
{
    std::vector<FoundEditor> found;
    const std::vector<std::string> path_dirs = path_environment_directories();
    for (const WindowsEditor &editor : known_windows_editors()) {
        std::string exe = find_editor_in_registry(editor);
        if (exe.empty())
            exe = find_editor_on_path(path_dirs, editor.path_shims);
        if (!exe.empty()) {
            BOOST_LOG_TRIVIAL(debug) << "Found external editor \"" << editor.name << "\" at \"" << exe << "\"";
            found.push_back({ editor.name, std::move(exe) });
        }
    }
    return found;
}

#else // ! _WIN32

struct UnixEditor
{
    const char               *name;
    // Absolute well-known install locations, checked in order.
    std::vector<const char *> absolute_paths;
    // Fallback shims to scan the PATH for.
    std::vector<PathShim>     path_shims;
};

#ifdef __APPLE__

// Well-known macOS editors: the CLI shim inside the application bundle is preferred,
// as it opens the target in the running instance instead of a new one.
const std::vector<UnixEditor>& known_unix_editors()
{
    static const std::vector<UnixEditor> editors = {
        { "Visual Studio Code",
          { "/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code",
            "/usr/local/bin/code" },
          { { "code", nullptr } } },
        { "Visual Studio Code (Insiders)",
          { "/Applications/Visual Studio Code - Insiders.app/Contents/Resources/app/bin/code",
            "/usr/local/bin/code-insiders" },
          { { "code-insiders", nullptr } } },
        { "VSCodium",
          { "/Applications/VSCodium.app/Contents/Resources/app/bin/codium",
            "/usr/local/bin/codium" },
          { { "codium", nullptr } } },
        { "Sublime Text",
          { "/Applications/Sublime Text.app/Contents/SharedSupport/bin/subl",
            "/usr/local/bin/subl" },
          { { "subl", nullptr } } },
        { "Cursor",
          { "/Applications/Cursor.app/Contents/Resources/app/bin/cursor",
            "/usr/local/bin/cursor" },
          { { "cursor", nullptr } } },
        { "Windsurf",
          { "/Applications/Windsurf.app/Contents/Resources/app/bin/windsurf",
            "/usr/local/bin/windsurf" },
          { { "windsurf", nullptr } } },
        { "Zed",
          { "/Applications/Zed.app/Contents/MacOS/cli",
            "/usr/local/bin/zed" },
          { { "zed", nullptr } } },
    };
    return editors;
}

#else // Linux or Unix

// Well-known Linux editors, transcribed from GitHub Desktop's app/src/lib/editors/linux.ts.
const std::vector<UnixEditor>& known_unix_editors()
{
    static const std::vector<UnixEditor> editors = {
        { "Visual Studio Code",
          { "/usr/share/code/bin/code", "/snap/bin/code", "/usr/bin/code" },
          { { "code", nullptr } } },
        { "Visual Studio Code (Insiders)",
          { "/snap/bin/code-insiders", "/usr/bin/code-insiders" },
          { { "code-insiders", nullptr } } },
        { "VSCodium",
          { "/usr/bin/codium", "/usr/share/vscodium-bin/bin/codium", "/snap/bin/codium" },
          { { "codium", nullptr } } },
        { "Sublime Text",
          { "/usr/bin/subl" },
          { { "subl", nullptr } } },
        { "Cursor",
          { "/usr/bin/cursor" },
          { { "cursor", nullptr } } },
        { "Windsurf",
          { "/usr/bin/windsurf" },
          { { "windsurf", nullptr } } },
        { "Zed",
          { "/usr/bin/zeditor", "/usr/bin/zed-editor", "/usr/bin/zed" },
          { { "zeditor", nullptr }, { "zed", nullptr } } },
        { "Kate",
          { "/usr/bin/kate" },
          { { "kate", nullptr } } },
        { "GNOME Text Editor",
          { "/usr/bin/gnome-text-editor" },
          { { "gnome-text-editor", nullptr } } },
        { "GEdit",
          { "/usr/bin/gedit" },
          { { "gedit", nullptr } } },
    };
    return editors;
}

#endif // __APPLE__

std::vector<FoundEditor> detect_installed_editors()
{
    std::vector<FoundEditor> found;
    const std::vector<std::string> path_dirs = path_environment_directories();
    for (const UnixEditor &editor : known_unix_editors()) {
        std::string exe;
        for (const char *path : editor.absolute_paths)
            if (file_exists(boost::filesystem::path(path))) {
                exe = path;
                break;
            }
        if (exe.empty())
            exe = find_editor_on_path(path_dirs, editor.path_shims);
        if (!exe.empty()) {
            BOOST_LOG_TRIVIAL(debug) << "Found external editor \"" << editor.name << "\" at \"" << exe << "\"";
            found.push_back({ editor.name, std::move(exe) });
        }
    }
    return found;
}

#endif // _WIN32

} // anonymous namespace

std::vector<FoundEditor> get_available_editors()
{
    static const std::vector<FoundEditor> s_editors = []() {
        std::vector<FoundEditor> editors;
        try {
            editors = detect_installed_editors();
        } catch (...) {
            // Never let editor detection break the caller; an empty list just means "nothing found".
            BOOST_LOG_TRIVIAL(error) << "External editor detection failed";
        }
        return editors;
    }();
    return s_editors;
}

FoundEditor find_editor_or_default(const std::string &name)
{
    const std::vector<FoundEditor> editors = get_available_editors();
    if (!name.empty())
        for (const FoundEditor &editor : editors)
            if (editor.name == name)
                return editor;
    if (!editors.empty())
        return editors.front();
    return FoundEditor{};
}

} // namespace GUI
} // namespace Slic3r
