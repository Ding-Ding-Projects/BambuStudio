#include <catch_main.hpp>

#include "slic3r/GUI/LanguageMode.hpp"

using namespace Slic3r::GUI::I18N;

#ifndef LANGUAGE_MODE_TEST_I18N_DIR
#error "LANGUAGE_MODE_TEST_I18N_DIR must identify the checked-in resources/i18n directory"
#endif

TEST_CASE("Language modes normalize canonical IDs and compatibility aliases", "[LanguageMode]")
{
    REQUIRE(normalize_language_mode_id(" en ") == LANGUAGE_MODE_ENGLISH);
    REQUIRE(normalize_language_mode_id("yue-HK") == LANGUAGE_MODE_CANTONESE_HONG_KONG);
    REQUIRE(normalize_language_mode_id("BILINGUAL-EN-YUE-HK") == LANGUAGE_MODE_ENGLISH_CANTONESE_HK);
    REQUIRE(normalize_language_mode_id("pt-br") == "pt_BR");
}

TEST_CASE("Custom language modes keep local routes separate from services", "[LanguageMode]")
{
    const LanguageModeProfile english = resolve_language_mode(LANGUAGE_MODE_ENGLISH);
    REQUIRE(english.kind == LanguageModeKind::English);
    REQUIRE(english.canonical_id == LANGUAGE_MODE_ENGLISH);
    REQUIRE(english.local_web_language == LANGUAGE_MODE_ENGLISH);

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

    const LanguageModeProfile traditional_chinese = resolve_language_mode("zh_TW");
    REQUIRE(traditional_chinese.kind == LanguageModeKind::Standard);
    REQUIRE(traditional_chinese.local_web_language == "zh_TW");
    REQUIRE(traditional_chinese.local_web_language != "zh_CN");
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

TEST_CASE("Checked-in Cantonese catalog loads and supplies bilingual fallback", "[LanguageMode][catalog]")
{
    LanguageModeService service;
    const wxString catalog_root = wxString::FromUTF8(LANGUAGE_MODE_TEST_I18N_DIR);

    REQUIRE(service.configure(LANGUAGE_MODE_CANTONESE_HONG_KONG, catalog_root));
    REQUIRE(service.cantonese_catalog_loaded());
    const LocalizedText cantonese = service.translate(wxString::FromUTF8("Language"));
    REQUIRE(cantonese.primary == wxString::FromUTF8("語言"));
    REQUIRE_FALSE(cantonese.has_secondary());

    // New model-preview and Prepare-dock surfaces resolve fully in standalone yue mode.
    const LocalizedText cantonese_open = service.translate(wxString::FromUTF8("Open in Prepare"));
    REQUIRE(cantonese_open.primary == wxString::FromUTF8("喺準備頁開啟"));
    REQUIRE_FALSE(cantonese_open.has_secondary());
    const LocalizedText cantonese_left = service.translate(wxString::FromUTF8("Left"));
    REQUIRE(cantonese_left.primary == wxString::FromUTF8("左"));
    REQUIRE_FALSE(cantonese_left.has_secondary());

    REQUIRE(service.configure(LANGUAGE_MODE_ENGLISH_CANTONESE_HK, catalog_root));
    const LocalizedText bilingual = service.translate(wxString::FromUTF8("Language"));
    REQUIRE(bilingual.primary == wxString::FromUTF8("Language"));
    REQUIRE(bilingual.secondary == wxString::FromUTF8("語言"));

    // Bilingual mode carries English primary with the Cantonese secondary for the same new keys.
    const LocalizedText bilingual_open = service.translate(wxString::FromUTF8("Open in Prepare"));
    REQUIRE(bilingual_open.primary == wxString::FromUTF8("Open in Prepare"));
    REQUIRE(bilingual_open.secondary == wxString::FromUTF8("喺準備頁開啟"));
    const LocalizedText bilingual_left = service.translate(wxString::FromUTF8("Left"));
    REQUIRE(bilingual_left.primary == wxString::FromUTF8("Left"));
    REQUIRE(bilingual_left.secondary == wxString::FromUTF8("左"));

    const LocalizedText fallback = service.translate(wxString::FromUTF8("__missing_catalog_message__"));
    REQUIRE(fallback.primary == wxString::FromUTF8("__missing_catalog_message__"));
    REQUIRE_FALSE(fallback.has_secondary());
}
