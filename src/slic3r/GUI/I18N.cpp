#include "I18N.hpp"

#include <cctype>
#include <string>

namespace Slic3r { namespace GUI {

namespace I18N {

LanguageModeService &language_mode_service()
{
	static LanguageModeService service;
	return service;
}

bool configure_language_mode(std::string_view language_mode_id, const wxString &localization_root)
{
	return language_mode_service().configure(language_mode_id, localization_root);
}

const LanguageModeProfile &language_mode_profile()
{
	return language_mode_service().profile();
}

wxString finish(const wxString &message, const wxString &translated)
{
	return language_mode_service().finish(message, translated);
}

wxString finish(const wxString &message, const wxString &translated, const char *ctx)
{
	return language_mode_service().finish(message, translated, ctx == nullptr ? wxString() : wxString(ctx, wxConvUTF8));
}

wxString finish_plural(const wxString &singular, const wxString &plural, unsigned int n, const wxString &translated)
{
	return language_mode_service().finish_plural(singular, plural, n, translated);
}

LocalizedText translate_mode(const wxString &s)
{
	return language_mode_service().translate(s);
}

LocalizedText translate_mode(const wxString &s, const wxString &plural, unsigned int n)
{
	return language_mode_service().translate_plural(s, plural, n);
}

LocalizedText translate_mode(const wxString &s, const char *ctx)
{
	return language_mode_service().translate(s, ctx == nullptr ? wxString() : wxString(ctx, wxConvUTF8));
}

namespace {

// The value of the filament_type setting that names the dispenser, and the words of the preset
// names that carry it. Both stay as written in presets and files; see the declarations.
const char *const INK_DISPENSER_MATERIAL_TYPE = "TPU-AMS";
const char *const INK_DISPENSER_MATERIAL_NAME = "TPU for AMS";

bool name_word_char(char c)
{
	return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

} // namespace

bool is_ink_dispenser_material_type(const std::string &value)
{
	return value == INK_DISPENSER_MATERIAL_TYPE;
}

wxString display_material_type(const std::string &type, bool narrow)
{
	if (!is_ink_dispenser_material_type(type))
		return wxString::FromUTF8(type.c_str());
	if (!narrow)
		return _L("TPU-AMS");
	wxString shown = _CTX("TPU-AMS", "NarrowBlock");
	// A language that has no short wording reads the long one; the short form drops the word Dispenser.
	shown.Replace("Ink Dispenser", "Ink");
	return shown;
}

std::string display_material_type_utf8(const std::string &type, bool narrow)
{
	// Any other type is returned byte for byte, so this is cheap enough to call on every frame.
	if (!is_ink_dispenser_material_type(type))
		return type;
	return std::string(display_material_type(type, narrow).ToUTF8().data());
}

wxString display_material_name(const std::string &name)
{
	if (is_ink_dispenser_material_type(name))
		return display_material_type(name);
	const std::string phrase(INK_DISPENSER_MATERIAL_NAME);
	size_t            at = 0;
	while ((at = name.find(phrase, at)) != std::string::npos) {
		const size_t end    = at + phrase.size();
		const bool   starts = at == 0 || !name_word_char(name[at - 1]);
		const bool   ends   = end >= name.size() || !name_word_char(name[end]);
		if (starts && ends)
			return wxString::FromUTF8(name.substr(0, at).c_str()) + display_material_type(INK_DISPENSER_MATERIAL_TYPE) +
				   wxString::FromUTF8(name.substr(end).c_str());
		at = end;
	}
	return wxString::FromUTF8(name.c_str());
}

} // namespace I18N

wxString L_str(const std::string &str)
{
	//! Explicitly specify that the source string is already in UTF-8 encoding
	return I18N::translate(wxString(str.c_str(), wxConvUTF8));
}

} }
