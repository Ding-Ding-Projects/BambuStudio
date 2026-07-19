#include <catch_main.hpp>

#include "slic3r/GUI/LanguageMode.hpp"

using namespace Slic3r::GUI::I18N;

TEST_CASE("Language modes normalize canonical IDs and compatibility aliases", "[LanguageMode]")
{
    REQUIRE(normalize_language_mode_id(" en ") == LANGUAGE_MODE_ENGLISH);
    REQUIRE(normalize_language_mode_id("yue-HK") == LANGUAGE_MODE_CANTONESE_HONG_KONG);
    REQUIRE(normalize_language_mode_id("BILINGUAL-EN-YUE-HK") == LANGUAGE_MODE_ENGLISH_CANTONESE_HK);
    REQUIRE(normalize_language_mode_id("pt-br") == "pt_BR");
}

TEST_CASE("Custom language modes keep local routes separate from services", "[LanguageMode]")
{
    const LanguageModeProfile cantonese = resolve_language_mode(LANGUAGE_MODE_CANTONESE_HONG_KONG);
    REQUIRE(cantonese.kind == LanguageModeKind::CantoneseHongKong);
    REQUIRE(cantonese.service_language == LANGUAGE_MODE_ENGLISH_US);
    REQUIRE(cantonese.local_web_language == LANGUAGE_MODE_CANTONESE_HONG_KONG);
    REQUIRE(cantonese.local_web_fallback_language == LANGUAGE_MODE_ENGLISH);
    REQUIRE(cantonese.font_language == "zh_TW");
    REQUIRE(cantonese.uses_auxiliary_cantonese_catalog);

    const LanguageModeProfile bilingual = resolve_language_mode(LANGUAGE_MODE_ENGLISH_CANTONESE_HK);
    REQUIRE(bilingual.kind == LanguageModeKind::BilingualEnglishCantoneseHongKong);
    REQUIRE(bilingual.service_language == LANGUAGE_MODE_ENGLISH_US);
    REQUIRE(bilingual.local_web_language == LANGUAGE_MODE_ENGLISH_CANTONESE_HK);
    REQUIRE(bilingual.font_language == "zh_TW");
    REQUIRE(bilingual.is_bilingual());
}

TEST_CASE("Bilingual format templates are formatted before presentation", "[LanguageMode]")
{
    const LocalizedText templates {
        wxString::FromUTF8("%d files"),
        wxString::FromUTF8("%d 個檔案"),
    };
    const FormattedLocalizedText formatted = templates.format_each([](wxString pattern) {
        pattern.Replace(wxString::FromUTF8("%d"), wxString::FromUTF8("3"));
        return pattern;
    });

    const LocalizedTextRenderResult compact = render_localized_text_compact(formatted);
    REQUIRE(compact.label.Contains(wxString::FromUTF8("3 files")));
    REQUIRE(compact.label.Contains(wxString::FromUTF8("3 個檔案")));
    REQUIRE_FALSE(compact.label.Contains(wxString::FromUTF8("%d")));

    const LocalizedTextRenderResult progressive = render_localized_text_progressive(formatted);
    REQUIRE(progressive.label == wxString::FromUTF8("3 files"));
    REQUIRE(progressive.secondary_tooltip.Contains(wxString::FromUTF8("3 個檔案")));
}
