#pragma once

#include <algorithm>
#include <cwchar>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <oleauto.h>
#endif

namespace Slic3r::GUI::PersonalModes {
struct VoiceInfo {
    std::wstring id, name;
    bool english = false;
    bool cantonese = false;
    // SAPI does not provide a portable offline/network capability attribute.
    bool network_capability_known = false;
};
struct VoiceStatus {
    bool available = false;
    bool selected_missing = false;
    std::wstring effective_id, effective_name;
};
inline std::wstring speech_xml(const std::wstring& text, int pitch) {
    std::wstring escaped;
    for (const auto ch : text) {
        if (ch == L'&') escaped += L"&amp;";
        else if (ch == L'<') escaped += L"&lt;";
        else if (ch == L'>') escaped += L"&gt;";
        else if (ch == L'\"') escaped += L"&quot;";
        else if (ch == L'\'') escaped += L"&apos;";
        else if (ch == L'\t' || ch == L'\n' || ch == L'\r' || ch >= 32) escaped += ch;
    }
    return L"<pitch absmiddle=\"" + std::to_wstring(std::clamp(pitch, -10, 10)) + L"\">" + escaped + L"</pitch>";
}

// Own on the GUI thread. Every COM result is released on that same thread.
class SapiVoice {
public:
    SapiVoice() {
#ifdef _WIN32
        auto hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        m_initialized = SUCCEEDED(hr);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return;
        CLSID clsid;
        if (SUCCEEDED(::CLSIDFromProgID(L"SAPI.SpVoice", &clsid)))
            ::CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_IDispatch, reinterpret_cast<void**>(&m_voice));
#endif
    }
    ~SapiVoice() {
#ifdef _WIN32
        if (m_voice) m_voice->Release();
        if (m_initialized) ::CoUninitialize();
#endif
    }
    SapiVoice(const SapiVoice&) = delete;
    SapiVoice& operator=(const SapiVoice&) = delete;
    bool available() const {
#ifdef _WIN32
        return m_voice != nullptr;
#else
        return false;
#endif
    }
    std::vector<VoiceInfo> enumerate() {
        std::vector<VoiceInfo> result;
#ifdef _WIN32
        Variant collection;
        if (!invoke(m_voice, L"GetVoices", DISPATCH_METHOD, {}, collection) || collection.vt != VT_DISPATCH) return result;
        Variant count;
        if (!invoke(collection.pdispVal, L"Count", DISPATCH_PROPERTYGET, {}, count) || count.vt != VT_I4) return result;
        for (long i = 0; i < std::min(count.lVal, 512L); ++i) {
            Variant index; index.vt = VT_I4; index.lVal = i;
            Variant item;
            if (!invoke(collection.pdispVal, L"Item", DISPATCH_METHOD, {index}, item) || item.vt != VT_DISPATCH) continue;
            VoiceInfo info;
            info.id = text(item.pdispVal, L"Id", DISPATCH_PROPERTYGET);
            info.name = text(item.pdispVal, L"GetDescription", DISPATCH_METHOD);
            Variant language; language.vt = VT_BSTR; language.bstrVal = ::SysAllocString(L"Language");
            Variant attributes;
            if (invoke(item.pdispVal, L"GetAttribute", DISPATCH_METHOD, {language}, attributes) && attributes.vt == VT_BSTR) {
                std::wstring ids(attributes.bstrVal ? attributes.bstrVal : L"");
                std::size_t start = 0;
                while (start < ids.size()) {
                    const auto end = ids.find(L';', start);
                    const auto part = ids.substr(start, end == std::wstring::npos ? end : end - start);
                    wchar_t* parsed_end = nullptr;
                    const auto id = std::wcstoul(part.c_str(), &parsed_end, 16);
                    if (parsed_end != part.c_str() && *parsed_end == 0) {
                        info.english = info.english || (id & 0x3ff) == 9;
                        info.cantonese = info.cantonese || id == 0x0c04;
                    }
                    if (end == std::wstring::npos) break;
                    start = end + 1;
                }
            }
            if (!info.id.empty() && (info.english || info.cantonese)) result.push_back(std::move(info));
        }
#endif
        return result;
    }
    VoiceStatus resolve(const std::vector<VoiceInfo>& voices, bool cantonese, const std::wstring& selected) const {
        const VoiceInfo* fallback = nullptr;
        for (const auto& v : voices) {
            if (!(cantonese ? v.cantonese : v.english)) continue;
            if (!fallback) fallback = &v;
            if (!selected.empty() && v.id == selected) return {true, false, v.id, v.name};
        }
        if (fallback) return {true, !selected.empty(), fallback->id, fallback->name};
        return {false, !selected.empty(), {}, {}};
    }
    bool complete() {
#ifdef _WIN32
        Variant timeout; timeout.vt = VT_I4; timeout.lVal = 0;
        Variant done;
        if (!invoke(m_voice, L"WaitUntilDone", DISPATCH_METHOD, {timeout}, done) || done.vt != VT_BOOL) return false;
        return done.boolVal != VARIANT_FALSE;
#else
        return true;
#endif
    }
    bool speak(const std::wstring& line, const std::wstring& voice_id, int rate, int pitch) {
#ifdef _WIN32
        if (!m_voice || !select(voice_id)) return false;
        Variant speed; speed.vt = VT_I4; speed.lVal = std::clamp(rate, -10, 10);
        Variant ignored;
        if (!invoke(m_voice, L"Rate", DISPATCH_PROPERTYPUT, {speed}, ignored)) return false;
        Variant flags; flags.vt = VT_I4; flags.lVal = 1 | 8; // asynchronous, XML; never purge another line
        Variant content; content.vt = VT_BSTR; content.bstrVal = ::SysAllocString(speech_xml(line, pitch).c_str());
        return invoke(m_voice, L"Speak", DISPATCH_METHOD, {flags, content}, ignored);
#else
        return false;
#endif
    }
    void stop() {
#ifdef _WIN32
        Variant flags; flags.vt = VT_I4; flags.lVal = 1 | 2;
        Variant content; content.vt = VT_BSTR; content.bstrVal = ::SysAllocString(L"");
        Variant ignored;
        invoke(m_voice, L"Speak", DISPATCH_METHOD, {flags, content}, ignored);
#endif
    }
private:
#ifdef _WIN32
    struct Variant : VARIANT {
        Variant() { ::VariantInit(this); }
        Variant(const Variant& other) { ::VariantInit(this); ::VariantCopy(this, const_cast<Variant*>(&other)); }
        ~Variant() { ::VariantClear(this); }
    };
    static bool invoke(IDispatch* object, const wchar_t* name, WORD mode, std::initializer_list<Variant> arguments, Variant& result) {
        if (!object) return false;
        DISPID id;
        auto mutable_name = const_cast<OLECHAR*>(name);
        if (FAILED(object->GetIDsOfNames(IID_NULL, &mutable_name, 1, LOCALE_USER_DEFAULT, &id))) return false;
        std::vector<VARIANT> args;
        for (const auto& arg : arguments) args.push_back(arg);
        DISPID put = DISPID_PROPERTYPUT;
        const bool writing = (mode & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) != 0;
        DISPPARAMS params{args.data(), writing ? &put : nullptr, static_cast<UINT>(args.size()), writing ? 1U : 0U};
        return SUCCEEDED(object->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, mode, &params, &result, nullptr, nullptr));
    }
    static std::wstring text(IDispatch* object, const wchar_t* name, WORD mode) {
        Variant value;
        if (!invoke(object, name, mode, {}, value) || value.vt != VT_BSTR || !value.bstrVal) return {};
        return value.bstrVal;
    }
    bool select(const std::wstring& id) {
        Variant voices;
        if (!invoke(m_voice, L"GetVoices", DISPATCH_METHOD, {}, voices) || voices.vt != VT_DISPATCH) return false;
        Variant count;
        if (!invoke(voices.pdispVal, L"Count", DISPATCH_PROPERTYGET, {}, count) || count.vt != VT_I4) return false;
        for (long i = 0; i < std::min(count.lVal, 512L); ++i) {
            Variant index; index.vt = VT_I4; index.lVal = i;
            Variant item, ignored;
            if (invoke(voices.pdispVal, L"Item", DISPATCH_METHOD, {index}, item) && item.vt == VT_DISPATCH &&
                text(item.pdispVal, L"Id", DISPATCH_PROPERTYGET) == id)
                return invoke(m_voice, L"Voice", DISPATCH_PROPERTYPUTREF, {item}, ignored);
        }
        return false;
    }
    IDispatch* m_voice = nullptr;
    bool m_initialized = false;
#endif
};
}
