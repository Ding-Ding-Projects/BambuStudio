#ifndef slic3r_GUI_LanguageMode_hpp_
#define slic3r_GUI_LanguageMode_hpp_

#include <wx/intl.h>
#include <wx/string.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

class wxMsgCatalog;
class wxWindow;

namespace Slic3r { namespace GUI { namespace I18N {

inline constexpr const char *LANGUAGE_MODE_ENGLISH              = "en";
inline constexpr const char *LANGUAGE_MODE_ENGLISH_US           = "en_US";
inline constexpr const char *LANGUAGE_MODE_CANTONESE_HONG_KONG  = "yue_HK";
inline constexpr const char *LANGUAGE_MODE_ENGLISH_CANTONESE_HK = "bilingual_en_yue_HK";

enum class LanguageModeKind {
    Standard,
    English,
    CantoneseHongKong,
    BilingualEnglishCantoneseHongKong
};

// All routes are explicit so custom UI modes never leak unsupported locale
// identifiers into remote services or choose an unsuitable CJK font atlas.
struct LanguageModeProfile {
    LanguageModeKind kind { LanguageModeKind::English };
    std::string      requested_id;
    std::string      canonical_id { LANGUAGE_MODE_ENGLISH };
    wxLanguage      formatting_language { wxLANGUAGE_ENGLISH_US };
    std::string      primary_catalog_language { LANGUAGE_MODE_ENGLISH };
    std::string      auxiliary_catalog_language;
    std::string      service_language { LANGUAGE_MODE_ENGLISH_US };
    std::string      local_web_language { LANGUAGE_MODE_ENGLISH };
    std::string      local_web_fallback_language { LANGUAGE_MODE_ENGLISH };
    std::string      font_language { LANGUAGE_MODE_ENGLISH };
    bool             preview { false };
    bool             uses_auxiliary_cantonese_catalog { false };

    bool is_bilingual() const
    {
        return kind == LanguageModeKind::BilingualEnglishCantoneseHongKong;
    }

    bool is_cantonese() const
    {
        return kind == LanguageModeKind::CantoneseHongKong || is_bilingual();
    }
};

std::string         normalize_language_mode_id(std::string_view language_mode_id);
LanguageModeProfile resolve_language_mode(std::string_view language_mode_id);
bool                is_custom_language_mode(std::string_view language_mode_id);
bool                is_baseline_language_mode(std::string_view language_mode_id);

struct FormattedLocalizedText;

// These strings are translated format templates. Keep the two variants
// separate until format_each() has applied every placeholder argument.
struct LocalizedText {
    wxString primary;
    wxString secondary;

    bool has_secondary() const { return !secondary.empty(); }

    template <typename Formatter>
    FormattedLocalizedText format_each(Formatter &&formatter) const;

    // Explicit opt-in for strings which contain no runtime format arguments.
    FormattedLocalizedText finalize_without_arguments() const;
};

// Rendering helpers accept only this finalized type, which prevents an inline
// or stacked bilingual template from duplicating printf/boost placeholders
// before formatting.
class FormattedLocalizedText
{
public:
    const wxString &primary() const { return m_primary; }
    const wxString &secondary() const { return m_secondary; }
    bool has_secondary() const { return !m_secondary.empty(); }

private:
    friend struct LocalizedText;

    FormattedLocalizedText(wxString primary, wxString secondary)
        : m_primary(std::move(primary)), m_secondary(std::move(secondary))
    {
    }

    wxString m_primary;
    wxString m_secondary;
};

template <typename Formatter>
FormattedLocalizedText LocalizedText::format_each(Formatter &&formatter) const
{
    wxString formatted_primary = std::invoke(formatter, primary);
    wxString formatted_secondary;
    if (has_secondary())
        formatted_secondary = std::invoke(formatter, secondary);
    return FormattedLocalizedText(std::move(formatted_primary), std::move(formatted_secondary));
}

inline FormattedLocalizedText LocalizedText::finalize_without_arguments() const
{
    return FormattedLocalizedText(primary, secondary);
}

class LanguageModeService
{
public:
    LanguageModeService();
    ~LanguageModeService();

    LanguageModeService(const LanguageModeService &) = delete;
    LanguageModeService &operator=(const LanguageModeService &) = delete;

    // localization_root is the resources/i18n directory. Custom modes remain
    // usable with English fallback when the preview catalog is absent; false
    // reports that degraded state to the caller.
    bool configure(std::string_view language_mode_id, const wxString &localization_root);

    const LanguageModeProfile &profile() const { return m_profile; }
    bool cantonese_catalog_loaded() const { return m_cantonese_catalog != nullptr; }
    const wxString &cantonese_catalog_path() const { return m_cantonese_catalog_path; }

    LocalizedText translate(const wxString &message, const wxString &context = wxString()) const;
    LocalizedText translate_plural(const wxString &singular, const wxString &plural, unsigned int n,
                                   const wxString &context = wxString()) const;

private:
    const wxString *find_cantonese(const wxString &message, unsigned int n, const wxString &context) const;

    LanguageModeProfile          m_profile;
    std::unique_ptr<wxMsgCatalog> m_cantonese_catalog;
    wxString                     m_cantonese_catalog_path;
};

enum class LocalizedTextPresentation {
    Compact,
    Stacked,
    Progressive,
    Automatic
};

struct LocalizedTextRenderOptions {
    LocalizedTextPresentation presentation { LocalizedTextPresentation::Automatic };
    int                       max_width_px { -1 };
    wxString                  inline_separator { wxString::FromUTF8(" \xC2\xB7 ") };
    wxString                  stacked_separator { wxString::FromUTF8("\n") };
    wxString                  secondary_tooltip_prefix { wxString::FromUTF8("\xE5\xBB\xA3\xE6\x9D\xB1\xE8\xA9\xB1\xEF\xBC\x9A") };
    wxString                  base_tooltip;
};

struct LocalizedTextRenderResult {
    LocalizedTextPresentation presentation { LocalizedTextPresentation::Progressive };
    wxString                  label;
    wxString                  secondary_tooltip;
};

LocalizedTextRenderResult render_localized_text_compact(
    const FormattedLocalizedText &text, const wxString &separator = wxString::FromUTF8(" \xC2\xB7 "));
LocalizedTextRenderResult render_localized_text_stacked(
    const FormattedLocalizedText &text, const wxString &separator = wxString::FromUTF8("\n"));
LocalizedTextRenderResult render_localized_text_progressive(
    const FormattedLocalizedText &text,
    const wxString &secondary_tooltip_prefix = wxString::FromUTF8("\xE5\xBB\xA3\xE6\x9D\xB1\xE8\xA9\xB1\xEF\xBC\x9A"));
LocalizedTextRenderResult render_localized_text(const FormattedLocalizedText &text,
                                                const LocalizedTextRenderOptions &options);

// Automatic presentation uses the target's client width (or max_width_px)
// and selects compact only when it fits; otherwise it keeps Cantonese in the
// tooltip. base_tooltip is always applied from scratch, avoiding accumulation
// when the language mode changes repeatedly.
LocalizedTextRenderResult apply_localized_text(wxWindow &target, const FormattedLocalizedText &text,
                                               const LocalizedTextRenderOptions &options = {});

}}} // namespace Slic3r::GUI::I18N

#endif // slic3r_GUI_LanguageMode_hpp_
