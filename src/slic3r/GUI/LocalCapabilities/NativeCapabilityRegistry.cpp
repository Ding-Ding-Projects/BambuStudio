#include "NativeCapabilityRegistry.hpp"
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#endif
#endif
namespace Slic3r { namespace GUI { namespace LocalCapabilities {
namespace {
std::string random_secret()
{
#ifdef _WIN32
    unsigned char bytes[32];
    if(BCryptGenRandom(nullptr,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG)!=0)
        throw Rejected("random_unavailable");
    constexpr char hex[]="0123456789ABCDEF";
    std::string result; result.reserve(64);
    for(auto byte:bytes) {result+=hex[byte>>4];result+=hex[byte&15];}
    SecureZeroMemory(bytes,sizeof(bytes)); return result;
#else
    throw Rejected("platform_unavailable");
#endif
}
}
Registry& registry() {static Registry value(random_secret);return value;}
}}}
