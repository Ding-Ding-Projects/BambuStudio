#include "OllamaClient.hpp"
#include <curl/curl.h>
#include <openssl/evp.h>
#include <array>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cwchar>
#ifdef _WIN32
#include <windows.h>
#include <dxgi1_4.h>
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
#ifdef _WIN32
template<class T> struct ComRef {
    T *p = nullptr;
    ComRef() = default;
    ComRef(const ComRef &) = delete;
    ComRef &operator=(const ComRef &) = delete;
    ~ComRef() { if (p) p->Release(); }
    T *operator->() const { return p; }
};
std::string narrow(const wchar_t *text) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &out[0], n, nullptr, nullptr);
    out.resize(static_cast<std::size_t>(n - 1));
    return out;
}
std::optional<std::wstring> process_setting(const wchar_t *name) {
    const DWORD n = GetEnvironmentVariableW(name, nullptr, 0);
    if (n <= 1 || n > 32768) return {};
    std::wstring value(n, L'\0');
    const DWORD written = GetEnvironmentVariableW(name, &value[0], n);
    if (written == 0 || written >= n) return {};
    value.resize(written);
    return value;
}
// RRF_RT_REG_SZ also accepts REG_EXPAND_SZ and expands it, as the environment block the runtime inherits does.
std::optional<std::wstring> registry_setting(HKEY root, const wchar_t *key, const wchar_t *name) {
    DWORD bytes = 0;
    if (RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) != ERROR_SUCCESS) return {};
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (bytes < 2 * sizeof(wchar_t) || bytes > 65536) return {};
        std::wstring value(bytes / sizeof(wchar_t) + 1, L'\0');
        DWORD size = static_cast<DWORD>(value.size() * sizeof(wchar_t));
        const LSTATUS status = RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, &value[0], &size);
        if (status == ERROR_MORE_DATA) { bytes = size; continue; } // expansion needed more room
        if (status != ERROR_SUCCESS) return {};
        value.resize(wcsnlen(value.c_str(), value.size()));
        if (value.empty()) return {};
        return value;
    }
    return {};
}
std::vector<GpuAdapter> graphics_adapters() {
    std::vector<GpuAdapter> out;
    HMODULE library = LoadLibraryExW(L"dxgi.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!library) return out;
    {
        using CreateFactory = HRESULT(WINAPI *)(REFIID, void **);
        const auto create = reinterpret_cast<CreateFactory>(reinterpret_cast<void *>(GetProcAddress(library, "CreateDXGIFactory1")));
        ComRef<IDXGIFactory1> factory;
        if (create && SUCCEEDED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory.p))) && factory.p) {
            for (UINT i = 0; i < 16; ++i) {
                ComRef<IDXGIAdapter1> adapter;
                if (factory->EnumAdapters1(i, &adapter.p) == DXGI_ERROR_NOT_FOUND) break;
                if (!adapter.p) continue;
                DXGI_ADAPTER_DESC1 description{};
                if (FAILED(adapter->GetDesc1(&description)) || (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) continue;
                GpuAdapter gpu;
                gpu.name = narrow(description.Description);
                gpu.dedicated_bytes = static_cast<std::uint64_t>(description.DedicatedVideoMemory);
                LARGE_INTEGER umd{};
                if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &umd))) {
                    std::ostringstream version;
                    version << HIWORD(umd.HighPart) << '.' << LOWORD(umd.HighPart) << '.' << HIWORD(umd.LowPart) << '.' << LOWORD(umd.LowPart);
                    gpu.driver_version = version.str();
                }
                ComRef<IDXGIAdapter3> adapter3;
                if (SUCCEEDED(adapter->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void **>(&adapter3.p))) && adapter3.p) {
                    DXGI_QUERY_VIDEO_MEMORY_INFO memory{};
                    // Budget less this process's own use: what another process can expect right now.
                    if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory)))
                        gpu.budget_bytes = memory.Budget > memory.CurrentUsage ? memory.Budget - memory.CurrentUsage : 0;
                }
                out.push_back(std::move(gpu));
            }
        }
    } // every interface is released before the library reference is dropped
    FreeLibrary(library);
    return out;
}
#endif
}
std::string content_identity(const std::string &bytes) { return identity(bytes); }
std::vector<DestinationCandidate> destination_candidates() {
    std::vector<DestinationCandidate> out;
    auto add = [&](std::filesystem::path path, DestinationSource source) {
        if (path.empty() || !path.is_absolute()) return;
        for (const auto &c : out) if (c.path.lexically_normal() == path.lexically_normal()) return;
        out.push_back({std::move(path), source});
    };
#ifdef _WIN32
    std::filesystem::path fallback;
    if (auto home = process_setting(L"USERPROFILE")) fallback = std::filesystem::path(*home) / L".ollama" / L"models";
    // A user setting overrides the computer setting in the environment the runtime starts with.
    if (auto user = registry_setting(HKEY_CURRENT_USER, L"Environment", L"OLLAMA_MODELS")) add(*user, DestinationSource::UserSetting);
    else if (auto machine = registry_setting(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", L"OLLAMA_MODELS")) add(*machine, DestinationSource::MachineSetting);
    else add(fallback, DestinationSource::Default);
    if (auto process = process_setting(L"OLLAMA_MODELS")) add(*process, DestinationSource::ProcessSetting);
    else add(fallback, DestinationSource::Default);
#else
    const char *process = std::getenv("OLLAMA_MODELS");
    const char *home = std::getenv("HOME");
    if (process && *process) add(process, DestinationSource::ProcessSetting);
    else if (home && *home) add(std::filesystem::path(home) / ".ollama" / "models", DestinationSource::Default);
#endif
    return out;
}
ApiResult LocalClient::execute(Operation op,const Json &payload,const std::atomic_bool &cancel,const std::function<bool(const Json &)> &on_chunk) const {
    ApiResult result;
    try {
        if(op==Operation::Chat||op==Operation::Generate) {
            // Enforce local capability at the transport boundary as well as in the UI.
            auto tags=execute(Operation::Installed,Json::object(),cancel);
            if(tags.state!=RuntimeState::Healthy) return tags;
            const auto name=payload.at("model").get<std::string>(); auto installed=installed_models(tags.value);
            auto found=std::find_if(installed.begin(),installed.end(),[&](const Model &m){return m.name==name;});
            if(found==installed.end()) throw std::runtime_error("Model is not installed");
            auto shown=execute(Operation::Show,{{"model",name}},cancel); if(shown.state!=RuntimeState::Healthy) return shown;
            apply_details(*found,shown.value); const auto &o=payload.at("options");
            auto messages=op==Operation::Chat?payload.at("messages"):Json::array({{{"role","user"},{"content",payload.at("prompt")}}});
            chat_payload(*found,messages,o.at("temperature").get<double>(),o.at("num_ctx").get<std::uint64_t>(),o.at("num_predict").get<std::uint64_t>());
        }
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
        } else if(!t.bytes.empty()) {
            result.value=parse_json(t.bytes);
            if(op==Operation::Version) {
                if(!result.value.contains("version")||!result.value.at("version").is_string()||result.value.at("version").get<std::string>().empty()||result.value.at("version").get<std::string>().size()>80) throw std::runtime_error("Invalid runtime version response");
            }
            if(op==Operation::Installed||op==Operation::Running) installed_models(result.value);
        }
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
Model LocalClient::registry_metadata(const std::string &tag,const std::atomic_bool &cancel) const {
    if(!valid_model(tag)||tag.find('/')!=std::string::npos||tag.find(':')==std::string::npos) throw std::runtime_error("Choose an exact official library tag");
    auto colon=tag.find(':'); auto path="/v2/library/"+tag.substr(0,colon)+"/manifests/"+tag.substr(colon+1);
    Transfer t; t.cancel=&cancel; auto c=connection("https://registry.ollama.ai"+path,t,30,false);
    std::unique_ptr<curl_slist,HeaderDelete> headers(curl_slist_append(nullptr,"Accept: application/vnd.docker.distribution.manifest.v2+json"));
    curl_easy_setopt(c.get(),CURLOPT_HTTPHEADER,headers.get());
    auto code=curl_easy_perform(c.get()); long status=0; curl_easy_getinfo(c.get(),CURLINFO_RESPONSE_CODE,&status);
    if(code!=CURLE_OK||status!=200) throw std::runtime_error("Official registry metadata unavailable");
    Model m; m.name=tag; apply_manifest(m,parse_json(t.bytes),identity(t.bytes)); return m;
}
Hardware detect_hardware(const std::vector<Model> &installed) {
    Hardware h; h.measured_at=utc_now();
    std::vector<DestinationProbe> probes;
    for(const auto &candidate:destination_candidates()) probes.push_back(probe_destination(candidate,installed));
    h.destination=choose_destination(probes); h.free_disk=h.destination.free_bytes;
#ifdef _WIN32
    MEMORYSTATUSEX memory{}; memory.dwLength=sizeof(memory); if(GlobalMemoryStatusEx(&memory)) { h.available_ram=memory.ullAvailPhys; h.total_ram=memory.ullTotalPhys; }
    SYSTEM_INFO info{}; GetNativeSystemInfo(&info);
    if(info.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64) h.architecture="x86_64";
    else if(info.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_ARM64) h.architecture="arm64";
    h.gpus=graphics_adapters();
#endif
    // Adapter memory is measured here; whether the runtime can use it is decided only by apply_backend.
    h.usable_vram=usable_vram(h.gpus);
    std::uint64_t best=0;
    for(const auto &gpu:h.gpus) { const auto usable=usable_vram({gpu}); if(usable&&*usable>=best) { best=*usable; h.gpu=gpu.name; } }
    return h;
}
}
