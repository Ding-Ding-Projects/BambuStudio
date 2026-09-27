#include <catch2/catch.hpp>

#include "slic3r/GUI/LanguageMode.hpp"

#include <wx/defs.h>

using namespace Slic3r::GUI::I18N;

// The Cantonese catalog is only needed for the fallback checks; the funny-level
// table itself carries its own Cantonese text.
static const wxString funny_test_catalog_root = wxString::FromUTF8(LANGUAGE_MODE_TEST_I18N_DIR);

TEST_CASE("Funny levels clamp to the 1..5 range", "[FunnyLevel]")
{
    REQUIRE(clamp_funny_level(-7) == FUNNY_LEVEL_MIN);
    REQUIRE(clamp_funny_level(0) == FUNNY_LEVEL_MIN);
    REQUIRE(clamp_funny_level(1) == 1);
    REQUIRE(clamp_funny_level(3) == 3);
    REQUIRE(clamp_funny_level(5) == 5);
    REQUIRE(clamp_funny_level(6) == FUNNY_LEVEL_MAX);
    REQUIRE(clamp_funny_level(999) == FUNNY_LEVEL_MAX);

    LanguageModeService service;
    REQUIRE(service.funny_level(FunnyLanguage::English) == FUNNY_LEVEL_DEFAULT);
    REQUIRE(service.funny_level(FunnyLanguage::Cantonese) == FUNNY_LEVEL_DEFAULT);
    service.set_funny_level(FunnyLanguage::English, 42);
    service.set_funny_level(FunnyLanguage::Cantonese, -3);
    REQUIRE(service.funny_level(FunnyLanguage::English) == FUNNY_LEVEL_MAX);
    REQUIRE(service.funny_level(FunnyLanguage::Cantonese) == FUNNY_LEVEL_MIN);
    // The two ladders are independent.
    service.set_funny_level(FunnyLanguage::English, 1);
    REQUIRE(service.funny_level(FunnyLanguage::Cantonese) == FUNNY_LEVEL_MIN);
    service.set_funny_level(FunnyLanguage::Cantonese, 4);
    REQUIRE(service.funny_level(FunnyLanguage::English) == 1);
    REQUIRE(service.funny_level(FunnyLanguage::Cantonese) == 4);
}

TEST_CASE("Persisted funny levels parse with a compiled default and stable keys", "[FunnyLevel]")
{
    REQUIRE(std::string(FUNNY_LEVEL_ENGLISH_KEY) == "funny_level_en");
    REQUIRE(std::string(FUNNY_LEVEL_CANTONESE_KEY) == "funny_level_yue");
    REQUIRE(std::string(DIALOG_EMOJIS_KEY) == "dialog_emojis");
    REQUIRE(std::string(FUNNY_LEVEL_DISCLOSED_KEY) == "funny_level_disclosed");
    REQUIRE(FUNNY_LEVEL_DEFAULT == 2);

    // Absent or malformed values fall back to the compiled default.
    REQUIRE(parse_funny_level("") == FUNNY_LEVEL_DEFAULT);
    REQUIRE(parse_funny_level("   ") == FUNNY_LEVEL_DEFAULT);
    REQUIRE(parse_funny_level("loud") == FUNNY_LEVEL_DEFAULT);
    REQUIRE(parse_funny_level("3x") == FUNNY_LEVEL_DEFAULT);
    REQUIRE(parse_funny_level("", 4) == 4);
    // Numbers round-trip and are clamped rather than rejected.
    REQUIRE(parse_funny_level("1") == 1);
    REQUIRE(parse_funny_level(" 5 ") == 5);
    REQUIRE(parse_funny_level("0") == FUNNY_LEVEL_MIN);
    REQUIRE(parse_funny_level("12") == FUNNY_LEVEL_MAX);
    REQUIRE(parse_funny_level("-2") == FUNNY_LEVEL_MIN);

    REQUIRE(parse_dialog_emojis("true"));
    REQUIRE(parse_dialog_emojis("1"));
    REQUIRE_FALSE(parse_dialog_emojis(""));
    REQUIRE_FALSE(parse_dialog_emojis("false"));
    REQUIRE_FALSE(parse_dialog_emojis("0"));
}

TEST_CASE("Copy variants follow the level ladder per language", "[FunnyLevel]")
{
    const wxString source = wxString::FromUTF8("Slicing complete");

    // Five-entry ladder: one variant per level; levels 1 and 2 stay serious.
    REQUIRE(*funny_copy_variant(source, FunnyLanguage::English, 1) == source);
    REQUIRE(*funny_copy_variant(source, FunnyLanguage::English, 2) == source);
    REQUIRE(*funny_copy_variant(source, FunnyLanguage::English, 3) == wxString::FromUTF8("Slicing complete. Ready to print."));
    REQUIRE(*funny_copy_variant(source, FunnyLanguage::English, 5) ==
            wxString::FromUTF8("Slicing done. Every layer counted and nothing left behind."));
    REQUIRE(*funny_copy_variant(source, FunnyLanguage::Cantonese, 1) == wxString::FromUTF8("切片完成"));
    REQUIRE(*funny_copy_variant(source, FunnyLanguage::Cantonese, 5) == wxString::FromUTF8("切片搞掂晒，一層都冇走漏。"));

    // Out-of-range levels are clamped, not rejected.
    REQUIRE(*funny_copy_variant(source, FunnyLanguage::English, 0) == *funny_copy_variant(source, FunnyLanguage::English, 1));
    REQUIRE(*funny_copy_variant(source, FunnyLanguage::English, 9) == *funny_copy_variant(source, FunnyLanguage::English, 5));

    // One-entry ladders (control labels) never vary with the level.
    const wxString label = wxString::FromUTF8("Funny level (English)");
    for (int level = FUNNY_LEVEL_MIN; level <= FUNNY_LEVEL_MAX; ++level) {
        REQUIRE(*funny_copy_variant(label, FunnyLanguage::English, level) == label);
        REQUIRE(*funny_copy_variant(label, FunnyLanguage::Cantonese, level) == wxString::FromUTF8("搞笑程度（英文）"));
    }

    // Unknown strings have no ladder.
    REQUIRE(funny_copy_variant(wxString::FromUTF8("__no_ladder__"), FunnyLanguage::English, 3) == nullptr);
    REQUIRE(funny_copy_variant(wxString::FromUTF8("__no_ladder__"), FunnyLanguage::Cantonese, 3) == nullptr);
}

TEST_CASE("Level variants keep their format placeholders so facts stay exact", "[FunnyLevel]")
{
    const wxString source = wxString::FromUTF8("Setting saved: %s");
    for (int level = FUNNY_LEVEL_MIN; level <= FUNNY_LEVEL_MAX; ++level) {
        REQUIRE(funny_copy_variant(source, FunnyLanguage::English, level)->Contains(wxString::FromUTF8("%s")));
        REQUIRE(funny_copy_variant(source, FunnyLanguage::Cantonese, level)->Contains(wxString::FromUTF8("%s")));
    }

    LanguageModeService service;
    REQUIRE(service.configure(LANGUAGE_MODE_ENGLISH_CANTONESE_HK, funny_test_catalog_root));
    service.set_funny_level(FunnyLanguage::English, 5);
    service.set_funny_level(FunnyLanguage::Cantonese, 5);
    const FormattedLocalizedText formatted = service.translate(source).format_each([](wxString pattern) {
        return wxString::Format(pattern, wxString::FromUTF8("Units"));
    });
    REQUIRE(formatted.primary() == wxString::FromUTF8("Noted and filed: Units"));
    REQUIRE(formatted.secondary() == wxString::FromUTF8("記低咗，仲入咗檔：Units"));
}

TEST_CASE("translate() picks the level variant and otherwise the base string", "[FunnyLevel]")
{
    const wxString source  = wxString::FromUTF8("Export successfully.");
    const wxString unknown = wxString::FromUTF8("Open in Prepare");

    LanguageModeService service;
    REQUIRE(service.configure(LANGUAGE_MODE_ENGLISH, funny_test_catalog_root));

    // English mode, default level (2): serious copy, identical to the source.
    REQUIRE(service.translate(source).primary == source);
    service.set_funny_level(FunnyLanguage::English, 4);
    REQUIRE(service.translate(source).primary == wxString::FromUTF8("Exported. Go and find it where you put it."));
    // A string without a ladder is untouched at any level.
    REQUIRE(service.translate(unknown).primary == unknown);
    REQUIRE_FALSE(service.translate(unknown).has_secondary());

    // Cantonese mode: the Cantonese ladder is selected by the Cantonese level,
    // independent of the English level.
    REQUIRE(service.configure(LANGUAGE_MODE_CANTONESE_HONG_KONG, funny_test_catalog_root));
    service.set_funny_level(FunnyLanguage::English, 5);
    service.set_funny_level(FunnyLanguage::Cantonese, 1);
    REQUIRE(service.translate(source).primary == wxString::FromUTF8("匯出成功。"));
    service.set_funny_level(FunnyLanguage::Cantonese, 5);
    REQUIRE(service.translate(source).primary == wxString::FromUTF8("匯出咗仲送到埗，就喺你揀嗰度等你。"));
    // Strings without a ladder still come from the catalog, then English.
    REQUIRE(service.translate(unknown).primary == wxString::FromUTF8("喺準備頁開啟"));
    REQUIRE(service.translate(wxString::FromUTF8("__missing_catalog_message__")).primary ==
            wxString::FromUTF8("__missing_catalog_message__"));

    // Bilingual mode: English primary from the English level, Cantonese
    // secondary from the Cantonese level.
    REQUIRE(service.configure(LANGUAGE_MODE_ENGLISH_CANTONESE_HK, funny_test_catalog_root));
    service.set_funny_level(FunnyLanguage::English, 1);
    service.set_funny_level(FunnyLanguage::Cantonese, 3);
    const LocalizedText bilingual = service.translate(source);
    REQUIRE(bilingual.primary == source);
    REQUIRE(bilingual.secondary == wxString::FromUTF8("匯出咗喇，檔案喺你揀嘅位置。"));
    // Settings labels render bilingually without the catalog.
    const LocalizedText label = service.translate(wxString::FromUTF8("Funny level (Cantonese)"));
    REQUIRE(label.primary == wxString::FromUTF8("Funny level (Cantonese)"));
    REQUIRE(label.secondary == wxString::FromUTF8("搞笑程度（廣東話）"));
}

TEST_CASE("Dialog emojis decorate headlines only and never action labels", "[FunnyLevel][DialogEmoji]")
{
    const wxString headline = wxString::FromUTF8("Unsaved Changes");

    // Off by default: text passes through untouched.
    LanguageModeService service;
    REQUIRE_FALSE(service.dialog_emojis());
    REQUIRE(decorate_dialog_text(headline, DialogEmojiKind::Warning, service.dialog_emojis()) == headline);

    service.set_dialog_emojis(true);
    const wxString decorated = decorate_dialog_text(headline, DialogEmojiKind::Warning, service.dialog_emojis());
    REQUIRE(decorated != headline);
    REQUIRE(decorated.EndsWith(headline));
    REQUIRE(decorated.StartsWith(dialog_emoji(DialogEmojiKind::Warning)));
    REQUIRE(has_dialog_emoji(decorated));
    REQUIRE_FALSE(has_dialog_emoji(headline));
    // Decorating twice never stacks glyphs; empty text stays empty.
    REQUIRE(decorate_dialog_text(decorated, DialogEmojiKind::Warning, true) == decorated);
    REQUIRE(decorate_dialog_text(wxString(), DialogEmojiKind::Error, true).empty());

    // Style mapping matches the header glyph mapping in MsgDialog.
    REQUIRE(dialog_emoji_kind_for_style(wxOK | wxICON_ERROR) == DialogEmojiKind::Error);
    REQUIRE(dialog_emoji_kind_for_style(wxOK | wxICON_WARNING) == DialogEmojiKind::Warning);
    REQUIRE(dialog_emoji_kind_for_style(wxYES_NO | wxICON_QUESTION) == DialogEmojiKind::Question);
    REQUIRE(dialog_emoji_kind_for_style(wxOK | wxICON_INFORMATION) == DialogEmojiKind::Info);
    REQUIRE(dialog_emoji_kind_for_style(wxOK | wxAPPLY) == DialogEmojiKind::Success);
    REQUIRE(dialog_emoji_kind_for_style(wxOK) == DialogEmojiKind::Info);

    // The action-label guard used by MsgDialog::add_button() / SetButtonLabel():
    // every glyph is stripped, plain labels are untouched.
    for (DialogEmojiKind kind : {DialogEmojiKind::Info, DialogEmojiKind::Warning, DialogEmojiKind::Error,
                                 DialogEmojiKind::Question, DialogEmojiKind::Success}) {
        const wxString button = wxString::FromUTF8("Save");
        const wxString leaked = decorate_dialog_text(button, kind, true);
        REQUIRE(has_dialog_emoji(leaked));
        REQUIRE(strip_dialog_emoji(leaked) == button);
        REQUIRE_FALSE(has_dialog_emoji(strip_dialog_emoji(leaked)));
    }
    REQUIRE(strip_dialog_emoji(wxString::FromUTF8("Cancel")) == wxString::FromUTF8("Cancel"));
    REQUIRE(strip_dialog_emoji(wxString()).empty());

    // The level never changes the emoji rule: a level-5 headline is decorated
    // exactly once and its button label stays plain.
    service.set_funny_level(FunnyLanguage::English, 5);
    const wxString loud = service.translate(headline).primary;
    REQUIRE(loud == wxString::FromUTF8("Unsaved changes are waiting for a decision"));
    REQUIRE(strip_dialog_emoji(decorate_dialog_text(loud, DialogEmojiKind::Warning, true)) == loud);
}
