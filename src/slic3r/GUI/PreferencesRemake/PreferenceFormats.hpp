#ifndef slic3r_GUI_PreferencesRemake_PreferenceFormats_hpp_
#define slic3r_GUI_PreferencesRemake_PreferenceFormats_hpp_

// Import preferences... reads more than JSON: the YAML, TOML and XML that
// Export preferences... writes, with or without the schema header, turned
// into the JSON PreferencePlans::plan_import reads. Only what those writers
// produce (and the plain hand edits people make to it: comments, unquoted
// words, single quotes, an array over several lines) is read; anything else
// is refused with the format named, never guessed at. No wxWidgets:
// tests/preference_formats_test.cpp compiles this on its own.

#include <string>
#include <string_view>

namespace Slic3r::GUI::PreferenceFormats {

enum class Format { Json, Yaml, Toml, Xml, Unknown };

// From the file name's extension (.json, .yaml, .yml, .toml, .xml), else
// from the first characters of the text.
Format detect(std::string_view file_name, std::string_view text);
const char *name(Format format); // "JSON", "YAML", "TOML", "XML", ""

// The JSON the importer reads. JSON comes back unchanged. False, with problem
// "not-yaml", "not-toml", "not-xml" or "unknown-format", when the text is not
// what Export preferences... writes in that format.
bool to_json(Format format, std::string_view text, std::string &json, std::string &problem);

} // namespace Slic3r::GUI::PreferenceFormats

#endif // slic3r_GUI_PreferencesRemake_PreferenceFormats_hpp_
