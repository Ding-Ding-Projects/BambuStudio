#include "OneCoreVoices.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace Slic3r::GUI::Narration {

#ifdef _WIN32
namespace {
constexpr const wchar_t* kTokens = L"SOFTWARE\\Microsoft\\Speech_OneCore\\Voices\\Tokens";

std::wstring read_string(HKEY key, const wchar_t* value)
{
    DWORD type = 0, size = 0;
    if (::RegQueryValueExW(key, value, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ || size == 0 || size > 4096) return {};
    std::wstring text(size / sizeof(wchar_t), L'\0');
    if (::RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<LPBYTE>(text.data()), &size) != ERROR_SUCCESS) return {};
    while (!text.empty() && text.back() == L'\0') text.pop_back();
    return text;
}
} // namespace

std::vector<CatalogVoice> enumerate_onecore_voices()
{
    std::vector<CatalogVoice> voices;
    HKEY root = nullptr;
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, kTokens, 0, KEY_READ | KEY_WOW64_64KEY, &root) != ERROR_SUCCESS) return voices;
    for (DWORD i = 0; i < 512; ++i) {
        wchar_t name[256];
        DWORD length = 256;
        if (::RegEnumKeyExW(root, i, name, &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        HKEY token = nullptr;
        if (::RegOpenKeyExW(root, name, 0, KEY_READ | KEY_WOW64_64KEY, &token) != ERROR_SUCCESS) continue;
        CatalogVoice voice;
        voice.id = std::wstring(L"HKEY_LOCAL_MACHINE\\") + kTokens + L"\\" + name;
        voice.name = read_string(token, nullptr);
        HKEY attributes = nullptr;
        if (::RegOpenKeyExW(token, L"Attributes", 0, KEY_READ | KEY_WOW64_64KEY, &attributes) == ERROR_SUCCESS) {
            if (voice.name.empty()) voice.name = read_string(attributes, L"Name");
            const auto flags = classify_language(read_string(attributes, L"Language"));
            voice.english = flags.english;
            voice.cantonese = flags.cantonese;
            ::RegCloseKey(attributes);
        }
        ::RegCloseKey(token);
        if (voice.name.empty()) voice.name = name;
        if (voice.english || voice.cantonese) voices.push_back(std::move(voice));
    }
    ::RegCloseKey(root);
    return voices;
}
#else
std::vector<CatalogVoice> enumerate_onecore_voices() { return {}; }
#endif

} // namespace Slic3r::GUI::Narration
