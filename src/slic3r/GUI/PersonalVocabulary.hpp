#pragma once

#include "PersonalVocabularySchema.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <utility>
#include <wx/string.h>

class wxWindow;

namespace Slic3r::GUI::PersonalVocabulary {
using Entries = std::vector<std::pair<std::wstring, std::wstring>>;

// Pure parser over the rules in PersonalVocabularySchema.hpp: never includes
// input, keys, values or filenames in diagnostics. On rejection result is
// unchanged and reason, when given, names the first rule that failed.
bool parse(std::string_view bytes, Entries &result, Rejection *reason = nullptr);
// Validates the selected file, then saves it as the private copy and applies
// it. A rejected file changes nothing.
bool load(const std::filesystem::path &file, Rejection *reason = nullptr);
bool clear();
bool loaded();
// Why the private copy saved on this device was not used at startup, or
// Rejection::None when there was no copy or it passed validation.
Rejection cache_rejection();
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
