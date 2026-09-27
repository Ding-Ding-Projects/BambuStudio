#include "LanguageMode.hpp"

#include <cwctype>

#include <wx/dcclient.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/translation.h>
#include <wx/version.h>
#include <wx/window.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

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

namespace {

bool vocabulary_word_char(wchar_t c)
{
    return std::iswalnum(static_cast<wint_t>(c)) || c == L'_';
}

// Whole-word, case-form-preserving substitution. Longer forms first so the
// plural is rewritten before its singular prefix could match.
const std::pair<const wchar_t *, const wchar_t *> VOCABULARY_RULES[] = {
    { L"Filaments", L"Inks" },
    { L"filaments", L"inks" },
    { L"FILAMENTS", L"INKS" },
    { L"Filament",  L"Ink" },
    { L"filament",  L"ink" },
    { L"FILAMENT",  L"INK" },
    { L"AMS",       L"Ink Dispenser" },
};

} // namespace

wxString vocabulary(const wxString &text)
{
    if (text.empty())
        return text;
    std::wstring value = text.ToStdWstring();
    if (value.find(L"://") != std::wstring::npos)
        return text;
    if (value.find(L'_') != std::wstring::npos && value.find(L' ') == std::wstring::npos)
        return text;

    bool changed = false;
    for (const auto &rule : VOCABULARY_RULES) {
        const std::wstring from(rule.first);
        const std::wstring to(rule.second);
        size_t pos = 0;
        while ((pos = value.find(from, pos)) != std::wstring::npos) {
            const bool starts = pos == 0 || !vocabulary_word_char(value[pos - 1]);
            const size_t end  = pos + from.size();
            const bool ends   = end >= value.size() || !vocabulary_word_char(value[end]);
            if (starts && ends) {
                value.replace(pos, from.size(), to);
                pos += to.size();
                changed = true;
            } else {
                pos += from.size();
            }
        }
    }
    return changed ? wxString(value) : text;
}

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

namespace {

// One ladder per language for a known English source string. Ladders may hold
// 1, 2, 3 or 5 entries and are expanded to the five levels exactly like
// ui-md3/site/copy.js:
//   5 entries -> levels 1..5 map one to one
//   3 entries -> levels 1-2 use [0], level 3 uses [1], levels 4-5 use [2]
//   2 entries -> levels 1-2 use [0], levels 3-5 use [1]
//   1 entry   -> the same text at every level
// Every entry keeps its printf placeholders, so the facts a caller formats in
// (file names, counts) are identical at every level.
struct FunnyCopyEntry {
    const char *              source;
    std::vector<const char *> english;
    std::vector<const char *> cantonese;
};

const std::vector<FunnyCopyEntry> &funny_copy_table()
{
    static const std::vector<FunnyCopyEntry> table = {
        {"Slicing complete",
         {"Slicing complete", "Slicing complete", "Slicing complete. Ready to print.",
          "Slicing done. Every layer accounted for.", "Slicing done. Every layer counted and nothing left behind."},
         {"切片完成", "切片完成", "切片完成，可以印喇。", "切片搞掂，每一層都數齊。", "切片搞掂晒，一層都冇走漏。"}},
        {"Export successfully.",
         {"Export successfully.", "Export successfully.", "Exported. The file is where you asked.",
          "Exported. Go and find it where you put it.", "Exported and delivered. It is exactly where you asked, waiting for you."},
         {"匯出成功。", "匯出成功。", "匯出咗喇，檔案喺你揀嘅位置。", "匯出咗，去你放嘅地方搵啦。", "匯出咗仲送到埗，就喺你揀嗰度等你。"}},
        {"Model file downloaded.",
         {"Model file downloaded.", "Model file downloaded.", "Model downloaded. It is on disk now.",
          "Model downloaded and safely on disk.", "Model downloaded, landed, and sitting comfortably on disk."},
         {"模型檔案已下載。", "模型檔案已下載。", "模型下載咗，已經喺硬碟。", "模型下載咗，穩穩陣陣喺硬碟。", "模型下載咗，落咗地，喺硬碟坐得好舒服。"}},
        {"Setting saved: %s",
         {"Setting saved: %s", "Setting saved: %s", "Saved: %s", "Noted: %s", "Noted and filed: %s"},
         {"已儲存設定：%s", "已儲存設定：%s", "已儲存：%s", "記低咗：%s", "記低咗，仲入咗檔：%s"}},
        {"Deleted: %s",
         {"Deleted: %s", "Deleted: %s", "Deleted: %s", "Gone: %s", "Gone, and it is not coming back on its own: %s"},
         {"已刪除：%s", "已刪除：%s", "刪除咗：%s", "冇咗喇：%s", "冇咗喇，自己唔會返嚟：%s"}},
        {"Error",
         {"Error", "Error", "Error", "Something went wrong", "Well, that did not go to plan"},
         {"錯誤", "錯誤", "錯誤", "出咗問題", "呢個唔喺計劃之內"}},
        {"Warning",
         {"Warning", "Warning", "Warning", "Heads up", "Heads up before this goes any further"},
         {"警告", "警告", "警告", "注意", "小心，行落去之前先睇睇"}},
        {"Unsaved Changes",
         {"Unsaved Changes", "Unsaved Changes", "Unsaved changes", "You have unsaved changes", "Unsaved changes are waiting for a decision"},
         {"未儲存嘅變更", "未儲存嘅變更", "有變更未儲存", "你有變更未儲存", "有變更未儲存，等你決定"}},
        {"Do you want to continue?",
         {"Do you want to continue?", "Do you want to continue?", "Continue?", "Continue anyway?", "Shall we carry on regardless?"},
         {"你想繼續嗎？", "你想繼續嗎？", "繼續？", "照樣繼續？", "咁都要繼續？"}},

        // Settings labels for the feature itself: one entry per language, so the
        // label never varies with the level but still renders bilingually.
        {"Funny level (English)", {"Funny level (English)"}, {"搞笑程度（英文）"}},
        {"Funny level (Cantonese)", {"Funny level (Cantonese)"}, {"搞笑程度（廣東話）"}},
        {"Level %d of 5", {"Level %d of 5"}, {"第 %d 級（共 5 級）"}},
        {"1 = fully serious, 5 = maximum playfulness", {"1 = fully serious, 5 = maximum playfulness"}, {"1 = 完全認真，5 = 玩到盡"}},
        {"Sets the tone of every message Bambu Studio shows in this language, including errors, warnings and destructive confirmations. It never changes what a message says has happened or what will be affected.",
         {"Sets the tone of every message Bambu Studio shows in this language, including errors, warnings and destructive confirmations. It never changes what a message says has happened or what will be affected."},
         {"控制 Bambu Studio 用呢種語言顯示嘅所有訊息語氣，包括錯誤、警告同破壞性確認。但永遠唔會改變訊息講嘅事實同影響範圍。"}},
        {"What does this change?", {"What does this change?"}, {"呢個會改變啲乜？"}},
        {"Hide details", {"Hide details"}, {"收起詳情"}},
        {"Stored in BambuStudio.conf as %d.", {"Stored in BambuStudio.conf as %d."}, {"已儲存喺 BambuStudio.conf，值係 %d。"}},
        {"Not stored yet; using the compiled default %d.", {"Not stored yet; using the compiled default %d."}, {"未儲存；用編譯預設值 %d。"}},
        {"Show emojis in dialogs and message boxes", {"Show emojis in dialogs and message boxes"}, {"喺對話框同訊息框顯示表情符號"}},
        {"Adds one decorative emoji to a dialog headline. Buttons, action labels and field labels never carry one.",
         {"Adds one decorative emoji to a dialog headline. Buttons, action labels and field labels never carry one."},
         {"喺對話框標題加一個裝飾表情符號。按鈕、動作標籤同欄位標籤永遠唔會加。"}},
        {"The funny level styles every message in this language, including errors and warnings. Facts never change. Adjust it in Preferences > General.",
         {"The funny level styles every message in this language, including errors and warnings. Facts never change. Adjust it in Preferences > General."},
         {"搞笑程度會影響呢種語言嘅所有訊息語氣，包括錯誤同警告。事實永遠唔變。可以喺「偏好設定 > 一般」更改。"}},
    };
    return table;
}

size_t funny_ladder_index(size_t ladder_size, int level)
{
    level = clamp_funny_level(level);
    switch (ladder_size) {
    case 5: return static_cast<size_t>(level - 1);
    case 3: return level <= 2 ? 0 : (level == 3 ? 1 : 2);
    case 2: return level <= 2 ? 0 : 1;
    default: return 0;
    }
}

struct DialogEmojiGlyph {
    DialogEmojiKind kind;
    const char *    utf8;
};

const std::vector<DialogEmojiGlyph> &dialog_emoji_glyphs()
{
    static const std::vector<DialogEmojiGlyph> glyphs = {
        {DialogEmojiKind::Info,     "\xE2\x84\xB9\xEF\xB8\x8F"},   // information source
        {DialogEmojiKind::Warning,  "\xE2\x9A\xA0\xEF\xB8\x8F"},   // warning sign
        {DialogEmojiKind::Error,    "\xE2\x9D\x8C"},               // cross mark
        {DialogEmojiKind::Question, "\xE2\x9D\x93"},               // question mark
        {DialogEmojiKind::Success,  "\xE2\x9C\x85"},               // check mark button
    };
    return glyphs;
}

} // namespace

int clamp_funny_level(int level)
{
    return std::max(FUNNY_LEVEL_MIN, std::min(FUNNY_LEVEL_MAX, level));
}

int parse_funny_level(std::string_view stored, int fallback)
{
    size_t begin = 0;
    while (begin < stored.size() && std::isspace(static_cast<unsigned char>(stored[begin])))
        ++begin;
    size_t end = stored.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(stored[end - 1])))
        --end;
    if (begin == end)
        return clamp_funny_level(fallback);

    const std::string digits(stored.substr(begin, end - begin));
    char *            parse_end = nullptr;
    const long        value     = std::strtol(digits.c_str(), &parse_end, 10);
    if (parse_end == nullptr || *parse_end != '\0')
        return clamp_funny_level(fallback);
    if (value < FUNNY_LEVEL_MIN) return FUNNY_LEVEL_MIN;
    if (value > FUNNY_LEVEL_MAX) return FUNNY_LEVEL_MAX;
    return static_cast<int>(value);
}

bool parse_dialog_emojis(std::string_view stored)
{
    return stored == "true" || stored == "1";
}

const wxString *funny_copy_variant(const wxString &source, FunnyLanguage language, int level)
{
    // Cache the wxString conversions once per (entry, language, ladder index).
    struct Resolved {
        wxString              source;
        std::vector<wxString> english;
        std::vector<wxString> cantonese;
    };
    static const std::vector<Resolved> resolved = [] {
        std::vector<Resolved> out;
        for (const FunnyCopyEntry &entry : funny_copy_table()) {
            Resolved item;
            item.source = wxString::FromUTF8(entry.source);
            for (const char *text : entry.english)   item.english.emplace_back(wxString::FromUTF8(text));
            for (const char *text : entry.cantonese) item.cantonese.emplace_back(wxString::FromUTF8(text));
            out.push_back(std::move(item));
        }
        return out;
    }();

    for (size_t i = 0; i < resolved.size(); ++i) {
        if (source != resolved[i].source)
            continue;
        const std::vector<wxString> &ladder =
            language == FunnyLanguage::English ? resolved[i].english : resolved[i].cantonese;
        if (ladder.empty())
            return nullptr;
        return &ladder[funny_ladder_index(ladder.size(), level)];
    }
    return nullptr;
}

DialogEmojiKind dialog_emoji_kind_for_style(long wx_message_style)
{
    if (wx_message_style & wxAPPLY)            return DialogEmojiKind::Success;
    if (wx_message_style & wxICON_ERROR)       return DialogEmojiKind::Error;
    if (wx_message_style & wxICON_WARNING)     return DialogEmojiKind::Warning;
    if (wx_message_style & wxICON_QUESTION)    return DialogEmojiKind::Question;
    return DialogEmojiKind::Info;
}

wxString dialog_emoji(DialogEmojiKind kind)
{
    for (const DialogEmojiGlyph &glyph : dialog_emoji_glyphs())
        if (glyph.kind == kind)
            return wxString::FromUTF8(glyph.utf8);
    return wxString();
}

bool has_dialog_emoji(const wxString &text)
{
    for (const DialogEmojiGlyph &glyph : dialog_emoji_glyphs())
        if (text.StartsWith(wxString::FromUTF8(glyph.utf8)))
            return true;
    return false;
}

wxString decorate_dialog_text(const wxString &text, DialogEmojiKind kind, bool enabled)
{
    if (!enabled || text.empty() || has_dialog_emoji(text))
        return text;
    return dialog_emoji(kind) + wxString::FromUTF8(" ") + text;
}

wxString strip_dialog_emoji(const wxString &text)
{
    for (const DialogEmojiGlyph &glyph : dialog_emoji_glyphs()) {
        const wxString prefix = wxString::FromUTF8(glyph.utf8);
        if (!text.StartsWith(prefix))
            continue;
        wxString rest = text.Mid(prefix.length());
        while (!rest.empty() && rest[0] == ' ')
            rest = rest.Mid(1);
        return rest;
    }
    return text;
}

void LanguageModeService::set_funny_level(FunnyLanguage language, int level)
{
    if (language == FunnyLanguage::English)
        m_funny_level_english = clamp_funny_level(level);
    else
        m_funny_level_cantonese = clamp_funny_level(level);
}

int LanguageModeService::funny_level(FunnyLanguage language) const
{
    return language == FunnyLanguage::English ? m_funny_level_english : m_funny_level_cantonese;
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
        return { vocabulary(translate_standard(message, context)), wxString() };

    // The funny level swaps in a voice variant when the source string has a
    // ladder; every other string falls through to the unchanged base copy.
    const wxString *english_variant = funny_copy_variant(message, FunnyLanguage::English, m_funny_level_english);
    const wxString  english         = vocabulary(english_variant == nullptr ? message : *english_variant);

    if (m_profile.kind == LanguageModeKind::English)
        return { english, wxString() };

    const wxString *cantonese = funny_copy_variant(message, FunnyLanguage::Cantonese, m_funny_level_cantonese);
    if (cantonese == nullptr)
        cantonese = find_cantonese(message, UINT_MAX, context);
    if (m_profile.kind == LanguageModeKind::CantoneseHongKong)
        return { cantonese == nullptr ? english : *cantonese, wxString() };

    LocalizedText result { english, wxString() };
    if (cantonese != nullptr && !cantonese->empty() && *cantonese != message)
        result.secondary = *cantonese;
    return result;
}

LocalizedText LanguageModeService::translate_plural(const wxString &singular, const wxString &plural,
                                                     unsigned int n, const wxString &context) const
{
    if (m_profile.kind == LanguageModeKind::Standard)
        return { vocabulary(translate_standard_plural(singular, plural, n, context)), wxString() };

    const wxString source = vocabulary(untranslated_plural(singular, plural, n));
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
