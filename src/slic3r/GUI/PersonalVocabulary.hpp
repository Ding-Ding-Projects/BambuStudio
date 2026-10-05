#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <wx/string.h>

class wxWindow;

namespace Slic3r::GUI::PersonalVocabulary {
constexpr std::size_t max_bytes = 1024 * 1024;
constexpr std::size_t max_entries = 2048;
constexpr std::size_t max_text_bytes = 512;
using Entries = std::vector<std::pair<std::wstring, std::wstring>>;

// Pure parser: never includes input, keys, values or filenames in diagnostics.
bool parse(std::string_view bytes, Entries &result);
bool load(const std::filesystem::path &file);
bool clear();
bool loaded();
void initialize();
void refresh();
void observe(wxWindow *window, std::function<void()> refresh_display);
bool is_cache_path(const std::filesystem::path &path);

// Register public, translated UI copy without changing it. Only the final
// native display adapter uses replacements; exports and translation callers
// continue receiving their original text.
wxString remember(const wxString &source);
wxString display(const wxString &source);
// Stateless display transform for deterministic validation with synthetic data.
wxString apply(const wxString &source, const Entries &mapping);
}
