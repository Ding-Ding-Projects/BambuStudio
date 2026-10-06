#include "Service.hpp"
#include <ctime>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincred.h>
#include <curl/curl.h>
#pragma comment(lib,"Advapi32.lib")
#pragma comment(lib,"Ws2_32.lib")
namespace Slic3r::ScheduledSettings {
namespace {
std::string utf8(const wchar_t* text) {int n=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);if(n<=1)return {};std::string out(size_t(n),0);WideCharToMultiByte(CP_UTF8,0,text,-1,out.data(),n,nullptr,nullptr);out.pop_back();return out;}
std::wstring wide(const std::string& text) {int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),nullptr,0);if(n<=0)return {};std::wstring out(size_t(n),0);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),int(text.size()),out.data(),n);return out;}
std::wstring account(const std::string& id) {return L"BambuStudio/ScheduledSettings/HomeAssistant/"+wide(id);}
struct Request { const Source* source;const std::atomic_bool* cancel;std::string body;bool too_large=false; };
size_t receive(char* ptr,size_t size,size_t nmemb,void* userdata) {
    auto& r=*static_cast<Request*>(userdata);if(r.cancel->load()||size!=0&&nmemb>max_payload/size)return 0;
    const size_t n=size*nmemb;if(n>max_payload-r.body.size()){r.too_large=true;return 0;}r.body.append(ptr,n);return n;
}
int progress(void* data,curl_off_t,curl_off_t,curl_off_t,curl_off_t) {return static_cast<Request*>(data)->cancel->load()?1:0;}
curl_socket_t open_socket(void* data,curlsocktype purpose,struct curl_sockaddr* addr) {
    auto& r=*static_cast<Request*>(data);if(r.cancel->load()||purpose!=CURLSOCKTYPE_IPCXN)return CURL_SOCKET_BAD;
    char ip[INET6_ADDRSTRLEN]={};
    if(addr->family==AF_INET)inet_ntop(AF_INET,&reinterpret_cast<sockaddr_in*>(&addr->addr)->sin_addr,ip,sizeof(ip));
    else if(addr->family==AF_INET6)inet_ntop(AF_INET6,&reinterpret_cast<sockaddr_in6*>(&addr->addr)->sin6_addr,ip,sizeof(ip));
    else return CURL_SOCKET_BAD;
    if(!allowed_address(ip,*r.source))return CURL_SOCKET_BAD;
    return ::socket(addr->family,addr->socktype,addr->protocol);
}
bool zone(const std::string& id,DYNAMIC_TIME_ZONE_INFORMATION& out) {
    if(id=="system")return GetDynamicTimeZoneInformation(&out)!=TIME_ZONE_ID_INVALID;
    for(DWORD i=0;;++i){DYNAMIC_TIME_ZONE_INFORMATION z={};const auto status=EnumDynamicTimeZoneInformation(i,&z);if(status!=ERROR_SUCCESS)return false;if(utf8(z.TimeZoneKeyName)==id){out=z;return true;}}
}
}
FetchResult native_fetch(const Source& source,const std::string& id,const std::atomic_bool& cancel) {
    Endpoint ep;std::string error;if(!source.consent||!endpoint(source,ep,error)||cancel.load())return {State::Unauthorized,{}};
    const auto* version=curl_version_info(CURLVERSION_NOW);
    if(!(version->features&CURL_VERSION_ASYNCHDNS))return {State::Unavailable,{}};
    std::string credential;
    if(source.kind==SourceKind::HomeAssistant) {
        PCREDENTIALW entry=nullptr;const auto key=account(id);
        if(!CredReadW(key.c_str(),CRED_TYPE_GENERIC,0,&entry))return {State::Unauthorized,{}};
        if(entry->CredentialBlobSize>0&&entry->CredentialBlobSize<=8192)credential.assign(reinterpret_cast<const char*>(entry->CredentialBlob),entry->CredentialBlobSize);
        SecureZeroMemory(entry->CredentialBlob,entry->CredentialBlobSize);CredFree(entry);
        if(credential.empty()||credential.find_first_of("\r\n")!=std::string::npos){if(!credential.empty())SecureZeroMemory(credential.data(),credential.size());return {State::Unauthorized,{}};}
    }
    CURL* curl=curl_easy_init();if(!curl){if(!credential.empty())SecureZeroMemory(credential.data(),credential.size());return {State::Unavailable,{}};}
    Request request{&source,&cancel};
    std::string url=source.url;if(source.kind==SourceKind::HomeAssistant){while(!url.empty()&&url.back()=='/')url.pop_back();url+="/api/states/"+source.entity;}
    curl_slist* headers=nullptr;headers=curl_slist_append(headers,"Accept: application/json");
    if(!credential.empty()){std::string auth="Authorization: Bearer "+credential;headers=curl_slist_append(headers,auth.c_str());SecureZeroMemory(auth.data(),auth.size());SecureZeroMemory(credential.data(),credential.size());}
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());
    curl_easy_setopt(curl,CURLOPT_PROXY,""); // Do not inherit ambient proxy or application account headers.
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(curl,CURLOPT_MAXREDIRS,0L);
    curl_easy_setopt(curl,CURLOPT_PROTOCOLS,long(CURLPROTO_HTTPS|(source.loopback_development?CURLPROTO_HTTP:0)));
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
    curl_easy_setopt(curl,CURLOPT_VERBOSE,0L);curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,5000L);curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,10000L);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&request);
    curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,progress);curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&request);
    curl_easy_setopt(curl,CURLOPT_OPENSOCKETFUNCTION,open_socket);curl_easy_setopt(curl,CURLOPT_OPENSOCKETDATA,&request);
    curl_easy_setopt(curl,CURLOPT_FRESH_CONNECT,1L);curl_easy_setopt(curl,CURLOPT_FORBID_REUSE,1L);
    const auto result=curl_easy_perform(curl);long code=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
    curl_easy_cleanup(curl);
    for(auto* h=headers;h;h=h->next)if(h->data)SecureZeroMemory(h->data,strlen(h->data));curl_slist_free_all(headers);
    if(result!=CURLE_OK)return {request.too_large?State::Invalid:State::Offline,{}};
    if(code==401||code==403)return {State::Unauthorized,{}};
    if(code==429)return {State::RateLimited,{}};
    if(code!=200)return {State::Invalid,{}};
    return {State::Active,std::move(request.body)};
}
bool store_home_credential(const std::string& id,const std::wstring& secret) {
    if(id.empty()||id.size()>80||secret.empty()||secret.size()>2048||secret.find_first_of(L"\r\n")!=std::wstring::npos)return false;
    auto bytes=utf8(secret.c_str());if(bytes.empty()||bytes.size()>CRED_MAX_CREDENTIAL_BLOB_SIZE)return false;
    const auto key=account(id);CREDENTIALW c={};c.Type=CRED_TYPE_GENERIC;c.TargetName=const_cast<wchar_t*>(key.c_str());c.Persist=CRED_PERSIST_LOCAL_MACHINE;c.CredentialBlobSize=DWORD(bytes.size());c.CredentialBlob=reinterpret_cast<LPBYTE>(bytes.data());
    const bool ok=CredWriteW(&c,0)!=0;SecureZeroMemory(bytes.data(),bytes.size());return ok;
}
bool remove_home_credential(const std::string& id) {const auto key=account(id);return CredDeleteW(key.c_str(),CRED_TYPE_GENERIC,0)!=0||GetLastError()==ERROR_NOT_FOUND;}
std::vector<Zone> timezones() {
    std::vector<Zone> result{{"system","System local time"}};
    for(DWORD i=0;;++i){DYNAMIC_TIME_ZONE_INFORMATION z={};if(EnumDynamicTimeZoneInformation(i,&z)!=ERROR_SUCCESS)break;result.push_back({utf8(z.TimeZoneKeyName),utf8(z.StandardName)});}return result;
}
bool local_time(const std::string& id,LocalTime& out) {
    DYNAMIC_TIME_ZONE_INFORMATION z={};if(!zone(id,z))return false;SYSTEMTIME utc={},local={};GetSystemTime(&utc);
    if(!SystemTimeToTzSpecificLocalTimeEx(&z,&utc,&local))return false;out={{int(local.wYear),int(local.wMonth),int(local.wDay)},int(local.wHour)*60+local.wMinute};return true;
}
}
#else
namespace Slic3r::ScheduledSettings {
FetchResult native_fetch(const Source&,const std::string&,const std::atomic_bool&){return {State::Unavailable,{}};}
bool store_home_credential(const std::string&,const std::wstring&){return false;}
bool remove_home_credential(const std::string&){return false;}
std::vector<Zone> timezones(){return {{"system","System local time"}};}
bool local_time(const std::string& id,LocalTime& out){if(id!="system")return false;const auto now=std::time(nullptr);std::tm t={};if(!localtime_r(&now,&t))return false;out={{t.tm_year+1900,t.tm_mon+1,t.tm_mday},t.tm_hour*60+t.tm_min};return true;}
}
#endif
