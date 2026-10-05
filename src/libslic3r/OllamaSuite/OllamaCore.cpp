#include "OllamaCore.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

namespace Slic3r::OllamaSuite {
namespace {
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
bool identifier(const std::string &s) {
    return !s.empty() && s.size() <= 80 && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    });
}
std::optional<std::uint64_t> number(const Json &j, const char *key) {
    auto i = j.find(key); if (i == j.end() || !i->is_number_unsigned()) return {};
    return i->get<std::uint64_t>();
}
std::string text(const Json &j, const char *key, std::size_t limit = 512) {
    auto i = j.find(key); if (i == j.end()) return {};
    require(i->is_string(), "Invalid text field in local API response");
    auto s = i->get<std::string>(); require(s.size() <= limit, "Oversized text field in local API response"); return s;
}
std::uint64_t plus(std::uint64_t a, std::uint64_t b) {
    require(b <= (std::numeric_limits<std::uint64_t>::max)() - a, "Resource estimate overflow"); return a + b;
}
std::vector<std::filesystem::path> file_page(const std::filesystem::path &root, std::size_t offset, std::size_t count) {
    require(count > 0 && count <= 100, "Page size must be between 1 and 100");
    std::vector<std::filesystem::path> out; std::size_t seen = 0;
    for (const auto &e : std::filesystem::directory_iterator(root)) {
        if (!e.is_regular_file() || e.path().extension() != ".json" || !identifier(e.path().stem().string())) continue;
        if (seen++ < offset) continue;
        out.push_back(e.path()); if (out.size() == count) break;
    }
    return out;
}
Json item_json(const PullItem &i) {
    return {{"schema", 1}, {"id", i.id}, {"model", i.model}, {"state", static_cast<int>(i.state)},
            {"message", i.message}, {"completed", i.completed}, {"total", i.total}};
}
PullItem item_read(const Json &j) {
    require(j.value("schema", 0) == 1, "Unsupported pull state schema");
    PullItem i; i.id = text(j, "id"); i.model = text(j, "model");
    int state = j.at("state").get<int>(); require(state >= 0 && state <= 6, "Invalid pull state");
    require(identifier(i.id) && valid_model(i.model), "Invalid persisted pull identity");
    i.state = static_cast<PullState>(state); i.message = text(j, "message");
    i.completed = j.at("completed").get<std::uint64_t>(); i.total = j.at("total").get<std::uint64_t>();
    require(i.total == 0 || i.completed <= i.total, "Invalid persisted pull progress"); return i;
}
}
bool valid_model(const std::string &s) {
    if (s.empty() || s.size() > 200 || s.front() == '/' || s.find("..") != std::string::npos) return false;
    return std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-' || c == '.' || c == ':' || c == '/';
    }) && std::count(s.begin(), s.end(), ':') <= 1 && s.back() != ':' && s.find("//") == std::string::npos;
}
Request request(Operation op, const Json &p) {
    require(p.is_object(), "Request payload must be an object"); Request r;
    switch (op) {
    case Operation::Version: r = {"GET", "/api/version", {}, false, 5}; break;
    case Operation::Installed: r = {"GET", "/api/tags", {}, false, 15}; break;
    case Operation::Running: r = {"GET", "/api/ps", {}, false, 15}; break;
    case Operation::Show: r = {"POST", "/api/show", {}, false, 30}; break;
    case Operation::Pull: r = {"POST", "/api/pull", {}, true, 3600}; break;
    case Operation::Delete: r = {"DELETE", "/api/delete", {}, false, 30}; break;
    case Operation::Copy: r = {"POST", "/api/copy", {}, false, 30}; break;
    case Operation::Chat: r = {"POST", "/api/chat", {}, true, 600}; break;
    case Operation::Generate: r = {"POST", "/api/generate", {}, true, 600}; break;
    }
    if (r.method == "GET") { require(p.empty(), "GET payload is not permitted"); return r; }
    std::set<std::string> allowed = op == Operation::Copy ? std::set<std::string>{"source", "destination"} : std::set<std::string>{"model"};
    if (op == Operation::Chat) { allowed.insert("messages"); allowed.insert("options"); }
    if (op == Operation::Generate) { allowed.insert("prompt"); allowed.insert("options"); }
    for (auto it = p.begin(); it != p.end(); ++it) require(allowed.count(it.key()) != 0, "Unsupported request field");
    Json body;
    if (op == Operation::Copy) {
        require(valid_model(text(p, "source")) && valid_model(text(p, "destination")), "Choose valid source and destination models");
        body = {{"source", p.at("source")}, {"destination", p.at("destination")}};
    } else {
        require(valid_model(text(p, "model")), "Choose a valid model");
        body = {{"model", p.at("model")}};
        if (op == Operation::Chat || op == Operation::Generate) {
            // These requests must be produced by chat_payload or its validated generation equivalent.
            require(p.contains("options") && p.at("options").is_object(), "Validated generation parameters are required");
            const auto &o = p.at("options");
            require(o.size() == 3 && o.contains("temperature") && o.contains("num_ctx") && o.contains("num_predict"), "Unsupported generation parameter");
            double t = o.at("temperature").get<double>(); auto c = o.at("num_ctx").get<std::uint64_t>(); auto n = o.at("num_predict").get<std::uint64_t>();
            require(std::isfinite(t) && t >= 0 && t <= 2 && c >= 512 && c <= 32768 && n >= 1 && n <= 4096, "Generation parameters exceed bounds");
            if (op == Operation::Chat) {
                require(p.contains("messages") && p.at("messages").is_array() && !p.at("messages").empty() && p.at("messages").size() <= 64, "Chat history exceeds bounds");
                body["messages"] = p.at("messages");
            } else { body["prompt"] = text(p, "prompt", max_chat_bytes); }
            body["options"] = o; body["keep_alive"] = "5m";
        }
        if (r.streaming) body["stream"] = true;
    }
    r.body = body.dump(); require(r.body.size() <= max_json_bytes, "Request body exceeds bounds"); return r;
}
Json parse_json(const std::string &s, std::size_t limit) {
    require(!s.empty() && s.size() <= limit, "Response is empty or oversized");
    // Reject deeply nested data before the parser can recurse on untrusted input.
    std::size_t depth = 0; bool quoted = false, escaped = false;
    for (char c : s) {
        if (quoted) { if (escaped) escaped = false; else if (c == '\\') escaped = true; else if (c == '"') quoted = false; }
        else if (c == '"') quoted = true;
        else if (c == '[' || c == '{') require(++depth <= 32, "Response nesting exceeds bounds");
        else if (c == ']' || c == '}') { require(depth > 0, "Invalid response nesting"); --depth; }
    }
    auto j = Json::parse(s); require(j.is_object(), "Response must be an object"); return j;
}
Ndjson::Ndjson(std::function<bool(const Json &)> c) : m_consume(std::move(c)) {}
bool Ndjson::line() {
    if (m_buffer.empty() || m_buffer == "\r") { m_buffer.clear(); return true; }
    try { auto j = parse_json(m_buffer, max_stream_line); m_buffer.clear();
        if (j.contains("error")) { m_error = "Ollama returned a stream error; inspect the local runtime and retry"; return false; }
        if (!m_consume(j)) { m_error = "Stream cancelled by consumer"; return false; }
    } catch (...) { m_error = "Malformed or oversized streaming response"; return false; } return true;
}
bool Ndjson::feed(const char *s, std::size_t n) {
    if (!m_error.empty()) return false;
    if (n > 64 * 1024 * 1024 - m_total) { m_error = "Stream transfer limit exceeded"; return false; }
    m_total += n;
    for (std::size_t i = 0; i < n; ++i) {
        if (s[i] == '\n') { if (!line()) return false; }
        else { if (m_buffer.size() == max_stream_line) { m_error = "Stream line limit exceeded"; return false; } m_buffer += s[i]; }
    } return true;
}
bool Ndjson::finish() { return m_error.empty() && line(); }
std::vector<Model> installed_models(const Json &j) {
    require(j.contains("models") && j.at("models").is_array() && j.at("models").size() <= 10000, "Invalid installed-model response");
    std::vector<Model> out; std::set<std::string> names;
    for (const auto &v : j.at("models")) {
        Model m; m.name = text(v, "name"); require(valid_model(m.name) && names.insert(m.name).second, "Invalid or duplicate model name");
        m.digest = text(v, "digest"); m.bytes = number(v, "size"); m.installed = true;
        if (v.contains("details")) { const auto &d = v.at("details"); m.family = text(d, "family"); m.parameters = text(d, "parameter_size"); m.quantization = text(d, "quantization_level"); }
        out.push_back(std::move(m));
    } return out;
}
void apply_details(Model &m, const Json &j) {
    m.capabilities.clear(); m.capabilities_verified = false;
    m.local = !j.contains("remote_model") && !j.contains("remote_host");
    if (j.contains("capabilities")) {
        require(j.at("capabilities").is_array() && j.at("capabilities").size() <= 32, "Invalid capabilities");
        for (const auto &c : j.at("capabilities")) { require(c.is_string() && c.get<std::string>().size() <= 80, "Invalid capability"); m.capabilities.insert(c.get<std::string>()); }
        m.capabilities_verified = true;
    }
    if (j.contains("model_info")) {
        const auto &info = j.at("model_info"); require(info.is_object() && info.size() <= 10000, "Invalid model metadata");
        m.parameter_count = number(info, "general.parameter_count");
        const auto architecture = text(info, "general.architecture");
        if (!architecture.empty()) m.context_length = number(info, (architecture + ".context_length").c_str());
    }
}
std::vector<Model> reconcile(const std::vector<Model> &catalog, const std::vector<Model> &installed, const std::set<std::string> &running) {
    std::map<std::string, Model> all;
    for (auto m : catalog) { m.installed = false; m.running = false; all[m.name] = std::move(m); }
    for (const auto &m : installed) all[m.name] = m;
    std::vector<Model> out; for (auto &[name, m] : all) { m.running = running.count(name) != 0; out.push_back(std::move(m)); } return out;
}
std::string fit_label(Fit f) { switch (f) { case Fit::RunsWell:return "Runs well"; case Fit::WithLimits:return "Runs with limits"; case Fit::Unlikely:return "Unlikely"; default:return "Unknown"; } }
FitResult fit(const Model &m, const Hardware &h, std::uint64_t context, std::optional<std::uint64_t> overhead) {
    FitResult r; r.evidence.push_back("Estimates are conservative and do not guarantee execution.");
    if (!m.bytes || !*m.bytes || !h.free_disk || h.measured_at.empty()) { r.evidence.push_back("Exact model size, destination space, or measurement timestamp is unavailable."); return r; }
    try { r.disk_required = m.installed ? 0 : plus(*m.bytes, *m.bytes / 10); }
    catch (...) { r.evidence.push_back("Storage estimate overflow."); return r; }
    if (*h.free_disk < *r.disk_required) { r.verdict = Fit::Unlikely; r.evidence.push_back("Destination has less than model size plus ten percent safety allowance."); return r; }
    if (!h.available_ram || !h.usable_vram || !h.backend_verified || h.architecture.empty() || !m.parameter_count || !m.context_length || m.quantization.empty() || !overhead) {
        r.evidence.push_back("RAM, usable VRAM, backend, architecture, parameters, quantization, context, and context-memory evidence are all required."); return r;
    }
    if (context == 0 || context > *m.context_length) { r.verdict = Fit::Unlikely; r.evidence.push_back("Requested context exceeds the verified model context."); return r; }
    try { r.memory_required = plus(plus(*m.bytes, *m.bytes / 5), *overhead); }
    catch (...) { r.evidence.push_back("Memory estimate overflow."); return r; }
    r.evidence.push_back("Memory estimate: blob bytes + twenty percent runtime allowance + explicitly supplied context-memory estimate.");
    if (*h.usable_vram >= *r.memory_required && *h.available_ram >= *m.bytes / 5) r.verdict = Fit::RunsWell;
    else if (*h.available_ram >= *r.memory_required) { r.verdict = Fit::WithLimits; r.evidence.push_back("CPU or partial GPU offload may be necessary; throughput is not predicted."); }
    else { r.verdict = Fit::Unlikely; r.evidence.push_back("Neither measured RAM nor measured VRAM accommodates this estimate."); }
    return r;
}
Json chat_payload(const Model &m, const Json &messages, double temperature, std::uint64_t context, std::uint64_t output) {
    require(m.installed && m.local && m.capabilities_verified && m.capabilities.count("completion"), "Select a verified installed local completion model");
    require(m.context_length && context <= *m.context_length, "Selected context lacks verified model support");
    require(messages.is_array() && !messages.empty() && messages.size() <= 64, "Chat history must contain 1 to 64 messages");
    std::size_t bytes = 0;
    for (const auto &v : messages) {
        auto role = text(v, "role"); require(role == "system" || role == "user" || role == "assistant", "Unsupported chat role");
        bytes += text(v, "content", max_chat_bytes).size(); require(bytes <= max_chat_bytes, "Prompt and history exceed bounds");
        for (auto it = v.begin(); it != v.end(); ++it) require(it.key() == "role" || it.key() == "content" || it.key() == "images", "Unsupported message field");
        if (v.contains("images")) {
            require(m.capabilities.count("vision") && role == "user", "Image attachments require verified vision capability");
            const auto &images = v.at("images"); require(images.is_array() && images.size() <= 4, "At most four images per message");
            for (const auto &image : images) {
                require(image.is_string(), "Invalid image attachment"); auto s = image.get<std::string>();
                require(!s.empty() && s.size() <= 1024 * 1024 && s.size() % 4 == 0, "Image attachment exceeds base64 bounds");
                require(std::all_of(s.begin(), s.end(), [](unsigned char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '='; }), "Image attachment must contain base64 bytes only");
            }
        }
    }
    Json p = {{"model", m.name}, {"messages", messages}, {"options", {{"temperature",temperature}, {"num_ctx",context}, {"num_predict",output}}}};
    request(Operation::Chat, p); return p;
}
bool official_catalog_path(const std::string &s) {
    if (s.size() > 300 || s.compare(0, 8, "/library") != 0) return false;
    const auto q = s.find('?'); const auto base = s.substr(0, q);
    if (base != "/library") {
        if (base.size() <= 14 || base.substr(base.size() - 5) != "/tags") return false;
        auto family = base.substr(9, base.size() - 14); if (!valid_model(family) || family.find_first_of("/:") != std::string::npos) return false;
    }
    if (q == std::string::npos) return true;
    const auto query = s.substr(q + 1); return query.compare(0, 5, "page=") == 0 && query.size() > 5 && query.size() < 12 &&
        std::all_of(query.begin() + 5, query.end(), [](char c) { return c >= '0' && c <= '9'; });
}
CatalogPage parse_catalog_html(const std::string &path, const std::string &html, const std::string &identity, const std::string &at) {
    require(official_catalog_path(path) && html.size() <= max_json_bytes, "Invalid catalog page");
    CatalogPage p; p.path = path; p.response_identity = identity; p.fetched_at = at;
    std::set<std::string> names, next;
    const bool families = path.substr(0, path.find('?')) == "/library";
    for (std::size_t pos = 0; (pos = html.find("href=\"", pos)) != std::string::npos;) {
        pos += 6; auto end = html.find('"', pos); require(end != std::string::npos, "Malformed catalog link");
        const auto href = html.substr(pos, end - pos); pos = end + 1;
        if (official_catalog_path(href) && href.find('?') != std::string::npos) {
            require(href.substr(0, href.find('?')) == path.substr(0, path.find('?')), "Catalog pagination changed family"); next.insert(href); continue;
        }
        if (href.compare(0, 9, "/library/") != 0) continue;
        auto name = href.substr(9); if (!valid_model(name) || name.find('/') != std::string::npos) continue;
        if (families && name.find(':') == std::string::npos) names.insert(name);
        else if (!families && name.find(':') != std::string::npos) {
            const auto family = path.substr(9, path.find("/tags") - 9);
            if (name.substr(0, name.find(':')) == family) names.insert(name);
        }
    }
    p.names.assign(names.begin(), names.end()); p.next_pages.assign(next.begin(), next.end());
    // HTML has no versioned completeness contract. Never infer total coverage from link absence.
    return p;
}
CatalogSnapshot refresh_catalog(const CatalogFetcher &fetch, const std::atomic_bool &cancel) {
    CatalogSnapshot s; s.refreshed_at = utc_now(); std::set<std::string> visited, pending{"/library"}, families, variants;
    bool counts_verified = true;
    try {
        while (!pending.empty()) {
            require(!cancel.load(), "Catalog refresh cancelled"); require(visited.size() < 10000, "Catalog page safety bound reached");
            const auto path = *pending.begin(); pending.erase(pending.begin()); if (!visited.insert(path).second) continue;
            auto p = fetch(path); require(p.path == path && !p.response_identity.empty() && !p.fetched_at.empty(), "Catalog page lacks source receipt");
            require(!p.names.empty(), "Catalog page contains no recognized entries");
            const bool index = path.substr(0, path.find('?')) == "/library";
            counts_verified = counts_verified && p.advertised_count.has_value();
            for (const auto &n : p.names) {
                require(valid_model(n), "Invalid catalog model");
                if (index) { require(n.find_first_of("/:") == std::string::npos, "Invalid family entry"); families.insert(n); pending.insert("/library/" + n + "/tags"); }
                else { require(n.find(':') != std::string::npos, "Variant lacks an exact tag"); variants.insert(n); }
            }
            for (const auto &next : p.next_pages) { require(official_catalog_path(next), "Unsafe catalog pagination"); if (!visited.count(next)) pending.insert(next); }
            s.pages.push_back(std::move(p));
            require(variants.size() <= 200000, "Catalog variant safety bound reached");
        }
        for (const auto &n : variants) { Model m; m.name = n; s.models.push_back(std::move(m)); }
        // Require an explicit count for each source collection and exact reconciliation across its pages.
        std::map<std::string, std::set<std::string>> actual; std::map<std::string, std::size_t> expected;
        for (const auto &p : s.pages) {
            auto base = p.path.substr(0,p.path.find('?')); actual[base].insert(p.names.begin(), p.names.end());
            if (p.advertised_count) { auto [i, inserted] = expected.emplace(base,*p.advertised_count); require(inserted || i->second == *p.advertised_count, "Catalog changed during pagination"); }
        }
        for (const auto &[base, names] : actual) counts_verified = counts_verified && expected.count(base) && expected[base] == names.size();
        s.complete = counts_verified && !families.empty() && !variants.empty();
        s.reason = s.complete ? "Every advertised collection count reconciled across all pages." : "Official HTML has no verified total-count contract. Entries are discovered, not certified exhaustive.";
        if (s.complete) s.last_successful_refresh = s.refreshed_at;
    } catch (const std::exception &e) { s.reason = e.what(); s.complete = false; s.offline = true; }
    return s;
}
Json catalog_json(const CatalogSnapshot &s) {
    Json j = {{"schema",1},{"complete",s.complete},{"offline",s.offline},{"refreshed_at",s.refreshed_at},{"last_successful_refresh",s.last_successful_refresh},{"reason",s.reason},{"models",Json::array()},{"pages",Json::array()}};
    for (const auto &m : s.models) j["models"].push_back({{"name",m.name}});
    for (const auto &p : s.pages) j["pages"].push_back({{"path",p.path},{"response_identity",p.response_identity},{"fetched_at",p.fetched_at},{"names",p.names},{"next_pages",p.next_pages},{"advertised_count",p.advertised_count ? Json(*p.advertised_count) : Json(nullptr)}});
    return j;
}
CatalogSnapshot load_catalog(const Json &j) {
    require(j.value("schema",0) == 1 && j.at("pages").is_array() && j.at("pages").size() <= 10000, "Invalid catalog cache");
    std::map<std::string,CatalogPage> pages;
    for (const auto &p : j.at("pages")) { CatalogPage x; x.path=text(p,"path"); x.response_identity=text(p,"response_identity"); x.fetched_at=text(p,"fetched_at"); x.names=p.at("names").get<std::vector<std::string>>(); x.next_pages=p.at("next_pages").get<std::vector<std::string>>(); if (!p.at("advertised_count").is_null()) x.advertised_count=p.at("advertised_count").get<std::size_t>(); require(pages.emplace(x.path,x).second,"Duplicate cached catalog page"); }
    std::atomic_bool cancel{false}; auto s=refresh_catalog([&](const std::string &path) { require(pages.count(path),"Cached catalog page missing"); return pages.at(path); },cancel);
    s.refreshed_at=text(j,"refreshed_at"); s.offline=true;
    if (s.complete) s.last_successful_refresh=text(j,"last_successful_refresh");
    return s;
}
std::string utc_now() {
    auto time=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm,&time);
#else
    gmtime_r(&time,&tm);
#endif
    std::ostringstream s; s << std::put_time(&tm,"%Y-%m-%dT%H:%M:%SZ"); return s.str();
}
std::string unique_id() { std::random_device rd; std::ostringstream s; s << std::hex << std::setfill('0'); for (int i=0;i<4;++i) s << std::setw(8) << rd(); return s.str(); }
void atomic_json(const std::filesystem::path &path,const Json &j) {
    auto bytes=j.dump(); require(bytes.size() <= max_json_bytes,"Persisted state exceeds bounds");
    std::filesystem::create_directories(path.parent_path()); auto temp=path; temp += "."+unique_id()+".tmp";
    { std::ofstream out(temp,std::ios::binary|std::ios::trunc); require(bool(out),"Cannot create local state"); out.write(bytes.data(),bytes.size()); out.flush(); if (!out) { out.close(); std::filesystem::remove(temp); throw std::runtime_error("Cannot persist local state"); } }
#ifdef _WIN32
    if (!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { std::filesystem::remove(temp); throw std::runtime_error("Cannot atomically replace local state"); }
#else
    std::error_code ec; std::filesystem::rename(temp,path,ec); if(ec) { std::filesystem::remove(temp); throw std::runtime_error("Cannot atomically replace local state"); }
#endif
}
Json read_json(const std::filesystem::path &path,std::size_t limit) {
    require(std::filesystem::is_regular_file(path) && std::filesystem::file_size(path)<=limit,"Local state missing or oversized");
    std::ifstream in(path,std::ios::binary); std::string bytes; char chunk[4096];
    while(in) { in.read(chunk,sizeof(chunk)); bytes.append(chunk,static_cast<std::size_t>(in.gcount())); require(bytes.size()<=limit,"Local state grew beyond bounds"); }
    return parse_json(bytes,limit);
}
PullQueue::PullQueue(std::filesystem::path p):m_root(std::move(p)) { std::filesystem::create_directories(m_root); }
PullItem PullQueue::add(const std::string &name) { require(valid_model(name),"Invalid queued model"); PullItem i; i.id=unique_id(); i.model=name; save(i); return i; }
void PullQueue::save(const PullItem &i) { require(identifier(i.id)&&valid_model(i.model)&&i.message.size()<=512,"Invalid queue item"); atomic_json(m_root/(i.id+".json"),item_json(i)); }
PullItem PullQueue::read(const std::string &id) const { require(identifier(id),"Invalid queue identity"); auto i=item_read(read_json(m_root/(id+".json"),4096)); require(i.id==id,"Queue identity mismatch"); return i; }
std::vector<PullItem> PullQueue::page(std::size_t offset,std::size_t count) const { std::vector<PullItem> out; for (const auto &p:file_page(m_root,offset,count)) out.push_back(read(p.stem().string())); return out; }
std::optional<PullItem> PullQueue::next() const { for (const auto &e:std::filesystem::directory_iterator(m_root)) { if(e.path().extension()!=".json") continue; auto i=read(e.path().stem().string()); if(i.state==PullState::Queued) return i; } return {}; }
void PullQueue::recover() { for (const auto &e:std::filesystem::directory_iterator(m_root)) { if(e.path().extension()!=".json") continue; auto i=read(e.path().stem().string()); if(i.state==PullState::Pulling) { i.state=PullState::Interrupted; i.message="Interrupted. Refresh installed models, then retry this exact tag."; save(i); } } }
void PullQueue::retry(const std::string &id) { auto i=read(id); require(i.state==PullState::Failed||i.state==PullState::Cancelled||i.state==PullState::Interrupted,"Only unfinished pulls can retry"); i.state=PullState::Queued; i.message.clear(); i.completed=i.total=0; save(i); }
ChatStore::ChatStore(std::filesystem::path p):m_root(std::move(p)) { std::filesystem::create_directories(m_root); }
void ChatStore::save(const ChatSession &s) {
    require(identifier(s.id)&&valid_model(s.model)&&s.title.size()<=200&&s.messages.is_array()&&s.messages.size()<=64,"Invalid chat session");
    require(s.messages.dump().size()<=max_chat_bytes,"Chat history exceeds local bound");
    // Attachment bytes are transient and never enter local history.
    Json messages=s.messages; for(auto &m:messages) { m.erase("images"); m.erase("thinking"); m.erase("tool_calls"); }
    atomic_json(m_root/(s.id+".json"),{{"schema",1},{"id",s.id},{"title",s.title},{"model",s.model},{"messages",messages}});
}
ChatSession ChatStore::read(const std::string &id) const {
    require(identifier(id),"Invalid session identity"); auto j=read_json(m_root/(id+".json"),max_chat_bytes+4096); require(j.value("schema",0)==1&&text(j,"id")==id,"Invalid chat schema");
    ChatSession s{id,text(j,"title",200),text(j,"model"),j.at("messages")}; require(valid_model(s.model)&&s.messages.is_array()&&s.messages.size()<=64,"Invalid stored chat"); return s;
}
std::vector<ChatSession> ChatStore::page(std::size_t offset,std::size_t count) const { std::vector<ChatSession> out; for(const auto &p:file_page(m_root,offset,count)) out.push_back(read(p.stem().string())); return out; }
void ChatStore::remove(const std::string &id,bool confirmed) { require(confirmed&&identifier(id),"Explicit deletion confirmation is required"); std::filesystem::remove(m_root/(id+".json")); }
Json ChatStore::redacted_export(const ChatSession &s) const {
    // Arbitrary user/model text cannot be proven secret-free. Omit it in ordinary exports.
    Json j={{"schema",1},{"messages",Json::array()},{"omissions","All titles, model identifiers, message content, paths and attachments are omitted from this ordinary export."}};
    for(const auto &m:s.messages) { auto role=text(m,"role"); if(role!="system"&&role!="user"&&role!="assistant") role="unknown"; j["messages"].push_back({{"role",role},{"content","[omitted]"}}); } return j;
}
}
