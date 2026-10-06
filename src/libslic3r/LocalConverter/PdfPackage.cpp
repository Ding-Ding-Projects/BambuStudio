#include "PdfPackage.hpp"
#include "PdfAdapter.hpp"
#include <array>
#include <fstream>
#include <set>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#endif

namespace Slic3r::LocalConverter {
namespace {
namespace fs=std::filesystem;
// Trusted release pins from qpdf-12.4.2-msvc64.zip, archive SHA-256
// db87077e683630c1217e0e8f9a20a9749d952ab676e881c3689187763a5de25d.
// The local manifest never grants trust to itself.
struct Pin {const char *name;const char *sha256;};
constexpr Pin pins[]{
 {"concrt140.dll","54716f0738af891f283d213b5c8d11b25896bb8ee3097d301eae718560cf974e"},
 {"msvcp140.dll","7c26614e1d733892c2deac7e245ce115504b1d80592dd0a01b08e3e5a55f89ca"},
 {"msvcp140_1.dll","206c931bf90fdad8816de3b5e2ef80b2bcaa9406c89ecc05fe6fddffe251e982"},
 {"msvcp140_2.dll","d50d7883f20d1dc6191768d3746f52dd9cac89c346ffaed5be1f110c2f34a838"},
 {"msvcp140_atomic_wait.dll","3d0cbfaa1bf3eecf5a3f4491d2960ee803cb994f30292c6adc4a07c498f60e2b"},
 {"msvcp140_codecvt_ids.dll","8a65c7596ef2e6938731f5a1058e7e40145b6d97967cc649231a076b9a608d78"},
 {"qpdf30.dll","8812952678de93e0750308a113aa650e9161af466f9770679ea9673adeb61041"},
 {"vcruntime140.dll","d1f4225df2cd877dbf130d5668a021dce3f94118455ff5ec952061c30afc9ce7"},
 {"vcruntime140_1.dll","a7146c08f89fe5b04541ab507cdb59ff7b44534d4ba3c668a426c6450a03434e"}
};
#ifdef _WIN32
bool digest(const fs::path &path,const std::string &expected)
{
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return false;
    if(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0){BCryptCloseAlgorithmProvider(algorithm,0);return false;}
    std::ifstream in(path,std::ios::binary);std::array<unsigned char,65536> block{};bool ok=bool(in);
    while(in){in.read(reinterpret_cast<char*>(block.data()),block.size());if(in.gcount()&&BCryptHashData(hash,block.data(),static_cast<ULONG>(in.gcount()),0)<0){ok=false;break;}}
    std::array<unsigned char,32> result{};ok=ok&&in.eof()&&BCryptFinishHash(hash,result.data(),static_cast<ULONG>(result.size()),0)>=0;
    BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);std::string actual;constexpr char digits[]="0123456789abcdef";
    for(unsigned c:result){actual+=digits[c>>4];actual+=digits[c&15];}return ok&&actual==expected;
}
#endif
}
bool verify_pdf_package(const fs::path &installed,std::string &reason)
{
    reason="PDF requires the complete pinned qpdf 12.4.2 runtime inside tools/pdf.";
#ifdef _WIN32
    try {
        // GetModuleFileName supplies the worker's installed directory. Avoid
        // enumerating its parent inside AppContainer: only tools/pdf is granted
        // read access. Every loaded byte still has an immutable compiled pin.
        const auto root=fs::absolute(installed).lexically_normal();const auto directory=root/"tools"/"pdf";
        if(!fs::is_directory(fs::symlink_status(directory)))return false;
        std::set<std::string> allowed;
        for(const auto &pin:pins){allowed.insert(pin.name);const auto file=directory/pin.name;
            if(!fs::is_regular_file(fs::symlink_status(file))||fs::file_size(file)>16*1024*1024||!digest(file,pin.sha256))return false;}
        std::size_t count=0;
        for(const auto &entry:fs::directory_iterator(directory)){
            if(++count>1000)return false;
            auto extension=entry.path().extension().string();for(auto &c:extension)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if(extension==".dll"&&!allowed.count(entry.path().filename().string()))return false;
        }
        reason.clear();return true;
    }catch(...){return false;}
#else
    return false;
#endif
}
bool load_pdf_engine(std::string &code)
{
#ifdef _WIN32
    HANDLE token=nullptr;DWORD container=0,length=0;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token)){code="pdf_sandbox_unavailable";return false;}
    const bool isolated=GetTokenInformation(token,TokenIsAppContainer,&container,sizeof(container),&length)&&container;CloseHandle(token);
    if(!isolated){code="pdf_sandbox_required";return false;}
    std::array<wchar_t,32768> executable{};const auto n=GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    if(!n||n>=executable.size()){code="pdf_package_unavailable";return false;}
    const auto installed=fs::path(executable.data()).parent_path();const auto directory=installed/"tools"/"pdf";
    // Hold read-only handles across verification and DLL loading. Writers and
    // replacement are excluded until the isolated worker exits.
    static std::vector<HANDLE> held;
    for(const auto &pin:pins){HANDLE h=CreateFileW((directory/pin.name).c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(h==INVALID_HANDLE_VALUE){for(auto old:held)CloseHandle(old);held.clear();code="pdf_package_unreadable";return false;}held.push_back(h);}
    std::string reason;if(!verify_pdf_package(installed,reason)){code="pdf_package_digest_mismatch";return false;}
    // qpdf's official Windows distribution imports USER32. Confirm the host
    // can initialize that system component in this sandbox before decoding.
    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY win32k{};
    if(GetProcessMitigationPolicy(GetCurrentProcess(),ProcessSystemCallDisablePolicy,&win32k,sizeof(win32k))&&win32k.DisallowWin32kSystemCalls){code="pdf_win32k_disabled";return false;}
    if(!LoadLibraryExW(L"user32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)){code="pdf_system_ui_load_"+std::to_string(GetLastError());return false;}
    HMODULE module=LoadLibraryExW((directory/"qpdf30.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!module){code="pdf_engine_load_"+std::to_string(GetLastError());return false;}
    if(!pdf_install_engine(module)){code="pdf_engine_api_mismatch";return false;}
    code="ok";return true;
#else
    code="pdf_sandbox_unavailable";return false;
#endif
}
}
