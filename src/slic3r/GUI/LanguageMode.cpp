#include "LanguageMode.hpp"

#include <wx/dcclient.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/translation.h>
#include <wx/version.h>
#include <wx/window.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <utility>

namespace Slic3r { namespace GUI { namespace I18N {

namespace {

std::string ascii_lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string canonicalize_standard_locale(std::string locale)
{
    std::replace(locale.begin(), locale.end(), '-', '_');

    size_t part_begin = 0;
    size_t part_index = 0;
    while (part_begin < locale.size()) {
        const size_t part_end = locale.find('_', part_begin);
        const size_t length = (part_end == std::string::npos ? locale.size() : part_end) - part_begin;
        std::string part = locale.substr(part_begin, length);

        if (part_index == 0) {
            part = ascii_lower(std::move(part));
        } else if (part.size() == 2 || part.size() == 3) {
            std::transform(part.begin(), part.end(), part.begin(), [](unsigned char ch) {
                return static_cast<char>(std::toupper(ch));
            });
        } else if (part.size() == 4) {
            part = ascii_lower(std::move(part));
            part[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(part[0])));
        }

        locale.replace(part_begin, length, part);
        part_begin += part.size();
        if (part_end == std::string::npos)
            break;
        ++part_begin;
        ++part_index;
    }
    return locale;
}

std::string language_prefix(const std::string &language_id)
{
    return ascii_lower(language_id.substr(0, language_id.find('_')));
}

std::string service_language_for(const std::string &language_id)
{
    const std::string prefix = language_prefix(language_id);
    if (prefix == "cs" || prefix == "sk") return "cs_CZ";
    if (prefix == "de") return "de_DE";
    if (prefix == "nl") return "nl_NL";
    if (prefix == "sv") return "sv_SE";
    if (prefix == "es") return "es_ES";
    if (prefix == "fr") return "fr_FR";
    if (prefix == "it") return "it_IT";
    if (prefix == "ja") return "ja_JP";
    if (prefix == "ko") return "ko_KR";
    if (prefix == "pl") return "pl_PL";
    if (prefix == "uk") return "uk_UA";
    if (prefix == "zh") return "zh_CN";
    if (prefix == "ru") return "ru_RU";
    if (prefix == "tr") return "tr_TR";
    if (prefix == "pt") return "pt_BR";
    if (prefix == "hu") return "hu_HU";
    return LANGUAGE_MODE_ENGLISH_US;
}

std::string local_web_language_for(const std::string &language_id)
{
    // Embedded resources historically received the persisted Studio locale
    // verbatim and performed their own supported-language fallback. Keep that
    // contract for standard locales: routing zh_TW through the remote-service
    // map would silently select Simplified Chinese instead of the existing
    // English fallback. Only the two baseline English aliases are collapsed.
    if (language_id == LANGUAGE_MODE_ENGLISH || language_id == LANGUAGE_MODE_ENGLISH_US)
        return LANGUAGE_MODE_ENGLISH;
    return language_id;
}

std::string font_language_for(const std::string &language_id)
{
    const std::string lower = ascii_lower(language_id);
    if (language_prefix(language_id) != "zh")
        return language_id;

    if (lower.find("_tw") != std::string::npos || lower.find("_hk") != std::string::npos ||
        lower.find("_mo") != std::string::npos || lower.find("_hant") != std::string::npos)
        return "zh_TW";
    return "zh_CN";
}

wxLanguage formatting_language_for(const std::string &language_id)
{
    const wxLanguageInfo *info = wxLocale::FindLanguageInfo(wxString::FromUTF8(language_id.c_str()));
    return info == nullptr ? wxLANGUAGE_ENGLISH_US : static_cast<wxLanguage>(info->Language);
}

wxString untranslated_plural(const wxString &singular, const wxString &plural, unsigned int n)
{
    return n == 1 ? singular : plural;
}

wxString translate_standard(const wxString &message, const wxString &context)
{
#if wxCHECK_VERSION(3, 1, 1)
    return context.empty() ? wxGetTranslation(message) : wxGetTranslation(message, wxEmptyString, context);
#else
    (void) context;
    return wxGetTranslation(message);
#endif
}

wxString translate_standard_plural(const wxString &singular, const wxString &plural, unsigned int n,
                                   const wxString &context)
{
#if wxCHECK_VERSION(3, 1, 1)
    return context.empty() ? wxGetTranslation(singular, plural, n)
                           : wxGetTranslation(singular, plural, n, wxEmptyString, context);
#else
    (void) context;
    return wxGetTranslation(singular, plural, n);
#endif
}

} // namespace

std::string normalize_language_mode_id(std::string_view language_mode_id)
{
    size_t begin = 0;
    size_t end = language_mode_id.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(language_mode_id[begin])))
        ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(language_mode_id[end - 1])))
        --end;

    std::string normalized(language_mode_id.substr(begin, end - begin));
    if (normalized.empty())
        return LANGUAGE_MODE_ENGLISH;

    std::replace(normalized.begin(), normalized.end(), '-', '_');
    const std::string lower = ascii_lower(normalized);
    if (lower == "en")
        return LANGUAGE_MODE_ENGLISH;
    if (lower == "en_us")
        return LANGUAGE_MODE_ENGLISH_US;
    if (lower == "yue_hk")
        return LANGUAGE_MODE_CANTONESE_HONG_KONG;
    if (lower == "bilingual_en_yue_hk")
        return LANGUAGE_MODE_ENGLISH_CANTONESE_HK;
    return canonicalize_standard_locale(std::move(normalized));
}

LanguageModeProfile resolve_language_mode(std::string_view language_mode_id)
{
    LanguageModeProfile profile;
    profile.requested_id = std::string(language_mode_id);
    profile.canonical_id = normalize_language_mode_id(language_mode_id);

    if (profile.canonical_id == LANGUAGE_MODE_ENGLISH || profile.canonical_id == LANGUAGE_MODE_ENGLISH_US) {
        profile.kind = LanguageModeKind::English;
        profile.formatting_language = wxLANGUAGE_ENGLISH_US;
        profile.primary_catalog_language = profile.canonical_id;
        profile.service_language = LANGUAGE_MODE_ENGLISH_US;
        profile.local_web_language = LANGUAGE_MODE_ENGLISH;
        profile.font_language = LANGUAGE_MODE_ENGLISH;
        return profile;
    }

    if (profile.canonical_id == LANGUAGE_MODE_CANTONESE_HONG_KONG) {
        profile.kind = LanguageModeKind::CantoneseHongKong;
        profile.formatting_language = wxLANGUAGE_CHINESE_HONGKONG;
        profile.primary_catalog_language = LANGUAGE_MODE_CANTONESE_HONG_KONG;
        profile.auxiliary_catalog_language = LANGUAGE_MODE_CANTONESE_HONG_KONG;
        profile.service_language = LANGUAGE_MODE_ENGLISH_US;
        profile.local_web_language = LANGUAGE_MODE_CANTONESE_HONG_KONG;
        profile.font_language = "zh_TW";
        profile.preview = true;
        profile.uses_auxiliary_cantonese_catalog = true;
        return profile;
    }

    if (profile.canonical_id == LANGUAGE_MODE_ENGLISH_CANTONESE_HK) {
        profile.kind = LanguageModeKind::BilingualEnglishCantoneseHongKong;
        profile.formatting_language = wxLANGUAGE_ENGLISH_US;
        profile.primary_catalog_language = LANGUAGE_MODE_ENGLISH;
        profile.auxiliary_catalog_language = LANGUAGE_MODE_CANTONESE_HONG_KONG;
        profile.service_language = LANGUAGE_MODE_ENGLISH_US;
        profile.local_web_language = LANGUAGE_MODE_ENGLISH_CANTONESE_HK;
        profile.font_language = "zh_TW";
        profile.preview = true;
        profile.uses_auxiliary_cantonese_catalog = true;
        return profile;
    }

    profile.kind = LanguageModeKind::Standard;
    profile.formatting_language = formatting_language_for(profile.canonical_id);
    profile.primary_catalog_language = profile.canonical_id;
    profile.service_language = service_language_for(profile.canonical_id);
    profile.local_web_language = local_web_language_for(profile.canonical_id);
    profile.font_language = font_language_for(profile.canonical_id);
    return profile;
}

bool is_custom_language_mode(std::string_view language_mode_id)
{
    const std::string normalized = normalize_language_mode_id(language_mode_id);
    return normalized == LANGUAGE_MODE_CANTONESE_HONG_KONG ||
           normalized == LANGUAGE_MODE_ENGLISH_CANTONESE_HK;
}

bool is_baseline_language_mode(std::string_view language_mode_id)
{
    const std::string normalized = normalize_language_mode_id(language_mode_id);
    return normalized == LANGUAGE_MODE_ENGLISH || normalized == LANGUAGE_MODE_ENGLISH_US;
}

LanguageModeService::LanguageModeService()
    : m_profile(resolve_language_mode(LANGUAGE_MODE_ENGLISH))
{
}

LanguageModeService::~LanguageModeService() = default;

bool LanguageModeService::configure(std::string_view language_mode_id, const wxString &localization_root)
{
    LanguageModeProfile next_profile = resolve_language_mode(language_mode_id);
    std::unique_ptr<wxMsgCatalog> next_catalog;
    wxString next_catalog_path;

    if (next_profile.uses_auxiliary_cantonese_catalog && !localization_root.empty()) {
        wxFileName catalog_file = wxFileName::DirName(localization_root);
        catalog_file.AppendDir(wxString::FromUTF8(LANGUAGE_MODE_CANTONESE_HONG_KONG));
        catalog_file.SetFullName(wxString::FromUTF8("BambuStudio.mo"));
        next_catalog_path = catalog_file.GetFullPath();

        if (wxFileExists(next_catalog_path)) {
            next_catalog.reset(wxMsgCatalog::CreateFromFile(next_catalog_path,
                                                            wxString::FromUTF8("BambuStudio-yue_HK")));
        }
    }

    const bool catalog_ready = !next_profile.uses_auxiliary_cantonese_catalog || next_catalog != nullptr;
    m_profile = std::move(next_profile);
    m_cantonese_catalog = std::move(next_catalog);
    m_cantonese_catalog_path = std::move(next_catalog_path);
    return catalog_ready;
}

const wxString *LanguageModeService::find_cantonese(const wxString &message, unsigned int n,
                                                    const wxString &context) const
{
    if (m_cantonese_catalog == nullptr)
        return nullptr;
#if wxCHECK_VERSION(3, 1, 1)
    return m_cantonese_catalog->GetString(message, n, context);
#else
    (void) context;
    return m_cantonese_catalog->GetString(message, n);
#endif
}

LocalizedText LanguageModeService::translate(const wxString &message, const wxString &context) const
{
    if (m_profile.kind == LanguageModeKind::Standard)
        return { translate_standard(message, context), wxString() };

    if (m_profile.kind == LanguageModeKind::English)
        return { message, wxString() };

    const wxString *cantonese = find_cantonese(message, UINT_MAX, context);
    if (m_profile.kind == LanguageModeKind::CantoneseHongKong)
        return { cantonese == nullptr ? message : *cantonese, wxString() };

    LocalizedText result { message, wxString() };
    if (cantonese != nullptr && !cantonese->empty() && *cantonese != message)
        result.secondary = *cantonese;
    return result;
}

LocalizedText LanguageModeService::translate_plural(const wxString &singular, const wxString &plural,
                                                     unsigned int n, const wxString &context) const
{
    if (m_profile.kind == LanguageModeKind::Standard)
        return { translate_standard_plural(singular, plural, n, context), wxString() };

    const wxString source = untranslated_plural(singular, plural, n);
    if (m_profile.kind == LanguageModeKind::English)
        return { source, wxString() };

    const wxString *cantonese = find_cantonese(singular, n, context);
    if (m_profile.kind == LanguageModeKind::CantoneseHongKong)
        return { cantonese == nullptr ? source : *cantonese, wxString() };

    LocalizedText result { source, wxString() };
    if (cantonese != nullptr && !cantonese->empty() && *cantonese != source)
        result.secondary = *cantonese;
    return result;
}

LocalizedTextRenderResult render_localized_text_compact(const FormattedLocalizedText &text,
                                                        const wxString &separator)
{
    LocalizedTextRenderResult result;
    result.presentation = LocalizedTextPresentation::Compact;
    result.label = text.primary();
    if (text.has_secondary()) {
        if (!result.label.empty())
            result.label += separator;
        result.label += text.secondary();
    }
    return result;
}

LocalizedTextRenderResult render_localized_text_stacked(const FormattedLocalizedText &text,
                                                        const wxString &separator)
{
    LocalizedTextRenderResult result;
    result.presentation = LocalizedTextPresentation::Stacked;
    result.label = text.primary();
    if (text.has_secondary()) {
        if (!result.label.empty())
            result.label += separator;
        result.label += text.secondary();
    }
    return result;
}

LocalizedTextRenderResult render_localized_text_progressive(const FormattedLocalizedText &text,
                                                            const wxString &secondary_tooltip_prefix)
{
    LocalizedTextRenderResult result;
    result.presentation = LocalizedTextPresentation::Progressive;
    result.label = text.primary().empty() ? text.secondary() : text.primary();
    if (!text.primary().empty() && text.has_secondary())
        result.secondary_tooltip = secondary_tooltip_prefix + text.secondary();
    return result;
}

LocalizedTextRenderResult render_localized_text(const FormattedLocalizedText &text,
                                                const LocalizedTextRenderOptions &options)
{
    switch (options.presentation) {
    case LocalizedTextPresentation::Compact:
        return render_localized_text_compact(text, options.inline_separator);
    case LocalizedTextPresentation::Stacked:
        return render_localized_text_stacked(text, options.stacked_separator);
    case LocalizedTextPresentation::Progressive:
    case LocalizedTextPresentation::Automatic:
        return render_localized_text_progressive(text, options.secondary_tooltip_prefix);
    }
    return render_localized_text_progressive(text, options.secondary_tooltip_prefix);
}

LocalizedTextRenderResult apply_localized_text(wxWindow &target, const FormattedLocalizedText &text,
                                               const LocalizedTextRenderOptions &options)
{
    LocalizedTextRenderOptions resolved_options = options;
    if (resolved_options.presentation == LocalizedTextPresentation::Automatic && text.has_secondary()) {
        const LocalizedTextRenderResult compact = render_localized_text_compact(text, options.inline_separator);
        int available_width = options.max_width_px;
        if (available_width <= 0)
            available_width = target.GetClientSize().GetWidth();

        bool compact_fits = false;
        if (available_width > 0) {
            wxClientDC dc(&target);
            if (target.GetFont().IsOk())
                dc.SetFont(target.GetFont());
            wxCoord text_width = 0;
            wxCoord text_height = 0;
            dc.GetTextExtent(compact.label, &text_width, &text_height);
            compact_fits = text_width <= available_width;
        }
        resolved_options.presentation = compact_fits ? LocalizedTextPresentation::Compact
                                                     : LocalizedTextPresentation::Progressive;
    } else if (resolved_options.presentation == LocalizedTextPresentation::Automatic) {
        resolved_options.presentation = LocalizedTextPresentation::Progressive;
    }

    LocalizedTextRenderResult result = render_localized_text(text, resolved_options);
    target.SetLabel(result.label);

    wxString tooltip = options.base_tooltip;
    if (!result.secondary_tooltip.empty()) {
        if (!tooltip.empty())
            tooltip += wxString::FromUTF8("\n\n");
        tooltip += result.secondary_tooltip;
    }
    target.SetToolTip(tooltip);
    return result;
}

}}} // namespace Slic3r::GUI::I18N
