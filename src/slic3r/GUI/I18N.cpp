#include "I18N.hpp"

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

} // namespace I18N

wxString L_str(const std::string &str)
{
	//! Explicitly specify that the source string is already in UTF-8 encoding
	return I18N::translate(wxString(str.c_str(), wxConvUTF8));
}

} }
