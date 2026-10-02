#include "ConfigProfileArchive.hpp"
#include <algorithm>
#include <cwctype>
#include <memory>
#include <vector>
#include <wx/file.h>
#include <wx/log.h>
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

namespace Slic3r::GUI::ConfigProfileArchive {
namespace {
struct CreatedPaths {
    bool complete = false;
    std::vector<std::filesystem::path> files;
    std::vector<std::filesystem::path> directories;
    ~CreatedPaths()
    {
        if (complete) return;
        std::error_code ignored;
        for (auto it = files.rbegin(); it != files.rend(); ++it) std::filesystem::remove(*it, ignored);
        // remove(), never remove_all(): an unrecorded external addition is kept.
        for (auto it = directories.rbegin(); it != directories.rend(); ++it) std::filesystem::remove(*it, ignored);
    }
};

bool safe_relative(const std::filesystem::path &path)
{
    if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory()) return false;
    for (const auto &part : path) {
        const auto text = part.wstring();
        if (text.empty() || text == L"." || text == L".." || text.find(L':') != std::wstring::npos ||
            text.find(L'\0') != std::wstring::npos || text.back() == L'.' || text.back() == L' ') return false;
        auto base = text.substr(0, text.find(L'.'));
        std::transform(base.begin(), base.end(), base.begin(), [](wchar_t ch) { return std::towupper(ch); });
        if (base == L"CON" || base == L"PRN" || base == L"AUX" || base == L"NUL" ||
            (base.size() == 4 && (base.substr(0, 3) == L"COM" || base.substr(0, 3) == L"LPT") &&
             base[3] >= L'1' && base[3] <= L'9')) return false;
    }
    return true;
}
}

ImportError import_archive(const std::filesystem::path &archive, const std::filesystem::path &destination)
{
    // Return generic enum errors, never archive-provided names in diagnostics.
    wxLogNull quiet;
    wxFFileInputStream input(wxString(archive.wstring()));
    if (!input.IsOk()) return ImportError::Open;
    std::error_code error;
    CreatedPaths created;
    // create_directory is the reservation: a pre-existing destination is not ours.
    if (!std::filesystem::create_directory(destination, error) || error) return ImportError::Destination;
    created.directories.push_back(destination);
    const auto root = std::filesystem::weakly_canonical(destination, error);
    if (error) return ImportError::Destination;
    wxZipInputStream zip(input);
    while (auto *raw = zip.GetNextEntry()) {
        std::unique_ptr<wxZipEntry> entry(raw);
        if (entry->IsDir()) continue;
        const auto relative = std::filesystem::path(entry->GetName().ToStdWstring());
        if (!safe_relative(relative)) return ImportError::UnsafePath;
        const auto output = destination / relative;
        const auto resolved = std::filesystem::weakly_canonical(output, error);
        const auto inside = resolved.lexically_relative(root);
        if (error || inside.empty() || inside == "." || inside.is_absolute() || *inside.begin() == "..")
            return ImportError::UnsafePath;
        auto directory = destination;
        for (const auto &component : relative.parent_path()) {
            directory /= component;
            if (std::filesystem::create_directory(directory, error)) created.directories.push_back(directory);
            else if (error || std::filesystem::is_symlink(directory, error) || error ||
                     !std::filesystem::is_directory(directory, error) || error) return ImportError::UnsafePath;
        }
        wxFile file;
        if (!file.Create(wxString(output.wstring()), false)) return ImportError::Write;
        created.files.push_back(output);
        wxFileOutputStream stream(file);
        stream.Write(zip);
        stream.Sync();
        if (!stream.IsOk() || !file.Close()) return ImportError::Write;
        // Reading through entry EOF validates its size and CRC in wxZipInputStream.
        // Check before GetNextEntry(), which can overwrite the previous status.
        if (zip.GetLastError() != wxSTREAM_EOF || !zip.CloseEntry()) return ImportError::Corrupt;
    }
    if (zip.GetLastError() != wxSTREAM_EOF) return ImportError::Corrupt;
    created.complete = true;
    return ImportError::None;
}
}
