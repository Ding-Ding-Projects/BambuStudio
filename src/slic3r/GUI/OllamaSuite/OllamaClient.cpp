#include "OllamaClient.hpp"
#include <curl/curl.h>
#include <openssl/evp.h>
#include <array>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Slic3r::OllamaSuite {
namespace {
struct CurlDelete { void operator()(CURL *c) const { curl_easy_cleanup(c); } };
struct HeaderDelete { void operator()(curl_slist *p) const { curl_slist_free_all(p); } };
struct Transfer {
    CURL *curl = nullptr; const std::atomic_bool *cancel = nullptr; std::string bytes;
    Ndjson *stream = nullptr; bool oversized = false, terminal = false;
};
size_t receive(char *data,size_t size,size_t n,void *context) noexcept {
    auto &t=*static_cast<Transfer *>(context);
    try {
        if(t.cancel->load() || size != 1) return 0;
        long status=0; curl_easy_getinfo(t.curl,CURLINFO_RESPONSE_CODE,&status);
        if(status<200||status>=300) return 0;
        if(t.stream) return t.stream->feed(data,n) ? n : 0;
        if(n>max_json_bytes-t.bytes.size()) { t.oversized=true; return 0; }
        t.bytes.append(data,n); return n;
    } catch(...) { return 0; }
}
int progress(void *context,curl_off_t,curl_off_t,curl_off_t,curl_off_t) noexcept {
    return static_cast<Transfer *>(context)->cancel->load() ? 1 : 0;
}
std::unique_ptr<CURL,CurlDelete> connection(const std::string &url, Transfer &t,long timeout,bool local) {
    // The application's curl initialization owns global lifetime.
    std::unique_ptr<CURL,CurlDelete> c(curl_easy_init()); if(!c) throw std::runtime_error("HTTP transport unavailable");
    t.curl=c.get();
    curl_easy_setopt(c.get(),CURLOPT_URL,url.c_str());
    curl_easy_setopt(c.get(),CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(c.get(),CURLOPT_MAXREDIRS,0L);
    curl_easy_setopt(c.get(),CURLOPT_PROTOCOLS,local?CURLPROTO_HTTP:CURLPROTO_HTTPS);
    curl_easy_setopt(c.get(),CURLOPT_PROXY,"");
    curl_easy_setopt(c.get(),CURLOPT_NOPROXY,"*");
    curl_easy_setopt(c.get(),CURLOPT_NETRC,CURL_NETRC_IGNORED);
    curl_easy_setopt(c.get(),CURLOPT_VERBOSE,0L);
    curl_easy_setopt(c.get(),CURLOPT_CONNECTTIMEOUT,5L);
    curl_easy_setopt(c.get(),CURLOPT_TIMEOUT,timeout);
    curl_easy_setopt(c.get(),CURLOPT_LOW_SPEED_LIMIT,1L);
    curl_easy_setopt(c.get(),CURLOPT_LOW_SPEED_TIME,60L);
    curl_easy_setopt(c.get(),CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(c.get(),CURLOPT_WRITEFUNCTION,receive);
    curl_easy_setopt(c.get(),CURLOPT_WRITEDATA,&t);
    curl_easy_setopt(c.get(),CURLOPT_XFERINFOFUNCTION,progress);
    curl_easy_setopt(c.get(),CURLOPT_XFERINFODATA,&t);
    curl_easy_setopt(c.get(),CURLOPT_NOPROGRESS,0L);
    return c;
}
std::string identity(const std::string &s) {
    std::array<unsigned char,32> digest{}; unsigned length=0;
    if(EVP_Digest(s.data(),s.size(),digest.data(),&length,EVP_sha256(),nullptr)!=1||length!=32) throw std::runtime_error("Catalog digest unavailable");
    std::ostringstream out; out<<std::hex<<std::setfill('0'); for(auto b:digest) out<<std::setw(2)<<int(b); return out.str();
}
}
ApiResult LocalClient::execute(Operation op,const Json &payload,const std::atomic_bool &cancel,const std::function<bool(const Json &)> &on_chunk) const {
    ApiResult result;
    try {
        auto r=request(op,payload); Transfer t; t.cancel=&cancel;
        Ndjson stream([&](const Json &j) {
            if(op==Operation::Pull && j.value("status",std::string())=="success") t.terminal=true;
            if((op==Operation::Chat||op==Operation::Generate)&&j.value("done",false)) t.terminal=true;
            return !on_chunk || on_chunk(j);
        });
        if(r.streaming) t.stream=&stream;
        auto c=connection("http://127.0.0.1:11434"+r.path,t,r.timeout_seconds,true);
        std::unique_ptr<curl_slist,HeaderDelete> headers(curl_slist_append(nullptr,"Content-Type: application/json"));
        curl_easy_setopt(c.get(),CURLOPT_HTTPHEADER,headers.get());
        if(r.method!="GET") { curl_easy_setopt(c.get(),CURLOPT_CUSTOMREQUEST,r.method.c_str()); curl_easy_setopt(c.get(),CURLOPT_POSTFIELDS,r.body.c_str()); curl_easy_setopt(c.get(),CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(r.body.size())); }
        auto code=curl_easy_perform(c.get()); curl_easy_getinfo(c.get(),CURLINFO_RESPONSE_CODE,&result.http_status);
        if(cancel.load()) { result.state=RuntimeState::Cancelled; result.diagnostic="Operation cancelled. Installed models are retained."; return result; }
        if(code==CURLE_COULDNT_CONNECT) { result.state=RuntimeState::Unreachable; result.diagnostic="No Ollama service answered on 127.0.0.1:11434. Open Troubleshooting and retry after starting the installed runtime."; return result; }
        if(code!=CURLE_OK||result.http_status<200||result.http_status>=300) { result.diagnostic=t.oversized?"Local API response exceeded the size bound.":"Local API request failed. Check runtime health, storage and compatibility in Troubleshooting."; return result; }
        if(r.streaming) {
            if(!stream.finish()||!t.terminal) { result.diagnostic="Stream ended without a successful terminal response. Partial output is retained as incomplete."; return result; }
        } else if(!t.bytes.empty()) result.value=parse_json(t.bytes);
        else if(op!=Operation::Delete && op!=Operation::Copy) { result.diagnostic="Local API returned an empty response."; return result; }
        result.state=RuntimeState::Healthy; result.terminal=true; result.diagnostic="Operation completed by the local API.";
    } catch(...) { result.diagnostic="Local request or response failed validation. No response content was logged."; }
    return result;
}
CatalogPage LocalClient::catalog_page(const std::string &path,const std::atomic_bool &cancel) const {
    if(!official_catalog_path(path)) throw std::runtime_error("Catalog path is not allowlisted");
    Transfer t; t.cancel=&cancel; auto c=connection("https://ollama.com"+path,t,30,false);
    curl_easy_setopt(c.get(),CURLOPT_SSL_VERIFYPEER,1L); curl_easy_setopt(c.get(),CURLOPT_SSL_VERIFYHOST,2L);
    auto code=curl_easy_perform(c.get()); long status=0; curl_easy_getinfo(c.get(),CURLINFO_RESPONSE_CODE,&status);
    if(code!=CURLE_OK||status!=200) throw std::runtime_error("Official catalog unavailable, cancelled, redirected or oversized");
    return parse_catalog_html(path,t.bytes,identity(t.bytes),utc_now());
}
Hardware detect_hardware(const std::filesystem::path &destination) {
    Hardware h; h.measured_at=utc_now(); std::error_code ec; auto s=std::filesystem::space(destination,ec); if(!ec) h.free_disk=s.available;
#ifdef _WIN32
    MEMORYSTATUSEX memory{}; memory.dwLength=sizeof(memory); if(GlobalMemoryStatusEx(&memory)) h.available_ram=memory.ullAvailPhys;
    SYSTEM_INFO info{}; GetNativeSystemInfo(&info);
    if(info.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64) h.architecture="x86_64";
    else if(info.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_ARM64) h.architecture="arm64";
#endif
    // Installed driver names alone do not prove usable backend memory. Missing GPU evidence stays unknown.
    return h;
}
}
