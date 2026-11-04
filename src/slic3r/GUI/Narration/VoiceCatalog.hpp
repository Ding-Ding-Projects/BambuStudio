#pragma once
// Pure voice-catalogue logic: language classification of platform voice
// attributes, a conservative network-capability guess, and merging the SAPI
// inventory with the OneCore/natural inventory.
#include <algorithm>
#include <cwchar>
#include <cwctype>
#include <string>
#include <vector>

namespace Slic3r::GUI::Narration {

enum class VoiceSource { Sapi, OneCore };
enum class NetworkCapability { Unknown, NetworkBacked };

struct CatalogVoice {
    std::wstring id, name;
    VoiceSource source = VoiceSource::Sapi;
    bool english = false;
    bool cantonese = false;
    NetworkCapability network = NetworkCapability::Unknown;
    bool speakable = false; // true only when this build has a playback path for the voice
};

struct LanguageFlags { bool english = false; bool cantonese = false; };

// Accepts the SAPI/OneCore "Language" attribute: semicolon-separated hexadecimal
// LCIDs ("409;9", "C04"), or BCP-47 tags ("en-US", "zh-HK", "yue-HK").
// Hong Kong Cantonese is LCID 0x0C04, which Windows names zh-HK.
inline LanguageFlags classify_language(const std::wstring& attribute)
{
    LanguageFlags flags;
    std::size_t start = 0;
    while (start <= attribute.size()) {
        const auto end = attribute.find(L';', start);
        std::wstring part = attribute.substr(start, end == std::wstring::npos ? end : end - start);
        std::transform(part.begin(), part.end(), part.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
        if (!part.empty()) {
            if (part.rfind(L"en", 0) == 0 && (part.size() == 2 || part[2] == L'-' || part[2] == L'_')) flags.english = true;
            else if (part == L"yue-hk" || part == L"zh-hk" || part == L"yue_hk" || part == L"zh_hk" || part == L"yue") flags.cantonese = true;
            else {
                wchar_t* parsed_end = nullptr;
                const auto id = std::wcstoul(part.c_str(), &parsed_end, 16);
                if (parsed_end != part.c_str() && *parsed_end == 0) {
                    if ((id & 0x3ff) == 9) flags.english = true;
                    if (id == 0x0c04) flags.cantonese = true;
                }
            }
        }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return flags;
}

// The platform exposes no network attribute for SAPI tokens. Natural voices are
// published as "... Online (Natural)", which is the one reliable signal; anything
// else stays Unknown. This never claims a voice is offline.
inline NetworkCapability classify_network(const std::wstring& name)
{
    auto lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return lower.find(L"online") != std::wstring::npos ? NetworkCapability::NetworkBacked : NetworkCapability::Unknown;
}

// SAPI voices are speakable. A OneCore voice is listed, but is only speakable if
// the same id is also a SAPI voice; otherwise it is reported as installed-but-unusable.
inline std::vector<CatalogVoice> merge(const std::vector<CatalogVoice>& sapi, const std::vector<CatalogVoice>& onecore)
{
    std::vector<CatalogVoice> out;
    for (auto v : sapi) { v.source = VoiceSource::Sapi; v.speakable = true; v.network = classify_network(v.name); out.push_back(std::move(v)); }
    for (auto v : onecore) {
        const bool duplicate = std::any_of(out.begin(), out.end(), [&](const CatalogVoice& o) { return o.id == v.id; });
        if (duplicate || out.size() >= 1024) continue;
        v.source = VoiceSource::OneCore; v.speakable = false; v.network = classify_network(v.name);
        out.push_back(std::move(v));
    }
    return out;
}

// True when a Hong Kong Cantonese voice exists on the machine but none of those is speakable.
inline bool cantonese_installed_but_unusable(const std::vector<CatalogVoice>& all)
{
    bool installed = false;
    for (const auto& v : all) if (v.cantonese) { if (v.speakable) return false; installed = true; }
    return installed;
}

} // namespace Slic3r::GUI::Narration
