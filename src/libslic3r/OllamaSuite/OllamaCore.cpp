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
std::uint64_t times(std::uint64_t a, std::uint64_t b) {
    require(a == 0 || b <= (std::numeric_limits<std::uint64_t>::max)() / a, "Resource estimate overflow"); return a * b;
}
// A per-layer array (hybrid and sliding-window architectures) contributes its largest entry, which never underestimates.
std::optional<std::uint64_t> largest(const Json &j, const std::string &key) {
    auto i = j.find(key); if (i == j.end()) return {};
    if (i->is_number_unsigned()) return i->get<std::uint64_t>();
    if (!i->is_array() || i->empty() || i->size() > 4096) return {};
    std::uint64_t most = 0;
    for (const auto &v : *i) { if (!v.is_number_unsigned()) return {}; most = (std::max)(most, v.get<std::uint64_t>()); }
    return most;
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
    default: throw std::runtime_error("Unsupported local API operation");
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
    m.block_count.reset(); m.head_count.reset(); m.head_count_kv.reset(); m.key_length.reset(); m.value_length.reset(); m.embedding_length.reset();
    if (j.contains("model_info")) {
        const auto &info = j.at("model_info"); require(info.is_object() && info.size() <= 10000, "Invalid model metadata");
        m.parameter_count = number(info, "general.parameter_count");
        const auto architecture = text(info, "general.architecture");
        if (!architecture.empty()) {
            m.context_length = number(info, (architecture + ".context_length").c_str());
            m.block_count = number(info, (architecture + ".block_count").c_str());
            m.embedding_length = number(info, (architecture + ".embedding_length").c_str());
            m.head_count = largest(info, architecture + ".attention.head_count");
            m.head_count_kv = largest(info, architecture + ".attention.head_count_kv");
            m.key_length = number(info, (architecture + ".attention.key_length").c_str());
            m.value_length = number(info, (architecture + ".attention.value_length").c_str());
        }
    }
}
void apply_manifest(Model &m, const Json &j, const std::string &sha) {
    require(j.value("schemaVersion",0)==2 && j.value("mediaType",std::string())=="application/vnd.docker.distribution.manifest.v2+json", "Unsupported registry manifest");
    require(sha.size()==64&&std::all_of(sha.begin(),sha.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),"Manifest identity missing");
    require(j.contains("layers")&&j.at("layers").is_array()&&!j.at("layers").empty()&&j.at("layers").size()<=256,"Invalid registry layer inventory");
    auto bytes=number(j.at("config"),"size"); require(bytes.has_value(),"Registry config size missing");
    std::uint64_t total=*bytes; bool weights=false;
    for(const auto &layer:j.at("layers")) {
        auto size=number(layer,"size"); const auto digest=text(layer,"digest"); const auto type=text(layer,"mediaType");
        require(size.has_value()&&digest.size()==71&&digest.compare(0,7,"sha256:")==0,"Registry layer lacks exact size or identity");
        require(std::all_of(digest.begin()+7,digest.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),"Invalid registry layer identity");
        total=plus(total,*size); if(type=="application/vnd.ollama.image.model"&&*size>0) weights=true;
    }
    require(weights,"Manifest has no verified local model weights"); m.bytes=total; m.digest="sha256:"+sha; m.local=true;
}
std::vector<Model> reconcile(const std::vector<Model> &catalog, const std::vector<Model> &installed, const std::set<std::string> &running) {
    std::map<std::string, Model> all;
    for (auto m : catalog) { m.installed = false; m.running = false; all[m.name] = std::move(m); }
    for (const auto &m : installed) all[m.name] = m;
    std::vector<Model> out; for (auto &[name, m] : all) { m.running = running.count(name) != 0; out.push_back(std::move(m)); } return out;
}
std::optional<std::filesystem::path> manifest_path(const std::string &name) {
    if (!valid_model(name)) return {};
    const auto colon = name.find(':');
    const std::string base = name.substr(0, colon), tag = colon == std::string::npos ? std::string("latest") : name.substr(colon + 1);
    if (tag.empty() || tag.find('/') != std::string::npos || tag == ".") return {};
    std::vector<std::string> parts;
    for (std::size_t start = 0;;) {
        const auto slash = base.find('/', start);
        parts.push_back(base.substr(start, slash - start));
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    if (parts.size() > 3 || std::any_of(parts.begin(), parts.end(), [](const std::string &p) { return p.empty() || p == "."; })) return {};
    // Ollama's documented name defaults: registry.ollama.ai host, library namespace, latest tag.
    std::string host = "registry.ollama.ai", space = "library";
    if (parts.size() == 3) { host = parts[0]; space = parts[1]; }
    else if (parts.size() == 2) space = parts[0];
    return std::filesystem::path("manifests") / host / space / parts.back() / tag;
}
DestinationProbe probe_destination(const DestinationCandidate &c, const std::vector<Model> &installed, std::size_t limit) {
    DestinationProbe p; p.candidate = c;
    if (c.path.empty() || !c.path.is_absolute()) return p;
    std::error_code ec;
    for (const auto &m : installed) {
        if (p.manifests_checked >= limit) break;
        const auto relative = manifest_path(m.name); if (!relative) continue;
        ++p.manifests_checked; if (std::filesystem::is_regular_file(c.path / *relative, ec)) ++p.manifests_found;
    }
    // Before the runtime creates the directory, the free space that matters is its nearest existing ancestor's.
    auto at = c.path;
    while (!std::filesystem::exists(at, ec)) { const auto parent = at.parent_path(); if (parent.empty() || parent == at) return p; at = parent; }
    const auto space = std::filesystem::space(at, ec); if (!ec) p.free_bytes = space.available;
    return p;
}
ModelDestination choose_destination(const std::vector<DestinationProbe> &probes) {
    ModelDestination d; d.candidates = probes.size(); if (probes.empty()) return d;
    const bool installed = std::any_of(probes.begin(), probes.end(), [](const DestinationProbe &p) { return p.manifests_checked > 0; });
    if (installed) {
        for (const auto &p : probes) if (p.manifests_checked > 0 && p.manifests_found == p.manifests_checked) {
            d.path = p.candidate.path; d.source = p.candidate.source; d.proof = DestinationProof::Manifests;
            d.manifests_checked = p.manifests_checked; d.manifests_found = p.manifests_found; d.free_bytes = p.free_bytes; return d;
        }
        // Installed models exist but no documented location holds them: the destination stays unknown.
        const auto best = std::max_element(probes.begin(), probes.end(), [](const DestinationProbe &a, const DestinationProbe &b) { return a.manifests_found < b.manifests_found; });
        d.path = best->candidate.path; d.source = best->candidate.source; d.manifests_checked = best->manifests_checked; d.manifests_found = best->manifests_found;
        return d;
    }
    // Nothing installed to cross-check: follow the documented setting, and when the views of that
    // setting disagree, use the least free space among them.
    d.path = probes.front().candidate.path; d.source = probes.front().candidate.source; d.proof = DestinationProof::Configured;
    std::optional<std::uint64_t> least; bool unknown = false;
    for (const auto &p : probes) {
        if (p.candidate.path.lexically_normal() != d.path.lexically_normal()) d.candidates_disagree = true;
        if (!p.free_bytes) unknown = true; else if (!least || *p.free_bytes < *least) least = p.free_bytes;
    }
    if (!unknown) d.free_bytes = least;
    return d;
}
std::optional<std::uint64_t> usable_vram(const std::vector<GpuAdapter> &gpus) {
    std::optional<std::uint64_t> best;
    for (const auto &g : gpus) {
        if (g.dedicated_bytes == 0) continue;
        const auto usable = g.budget_bytes ? (std::min)(g.dedicated_bytes, *g.budget_bytes) : g.dedicated_bytes;
        if (!best || usable > *best) best = usable;
    }
    return best;
}
std::string adapter_identity(const std::vector<GpuAdapter> &gpus) {
    std::vector<std::string> rows;
    for (const auto &g : gpus) rows.push_back(g.name + "|" + g.driver_version + "|" + std::to_string(g.dedicated_bytes));
    std::sort(rows.begin(), rows.end()); std::string out;
    for (const auto &r : rows) out += (out.empty() ? "" : ";") + r;
    return out;
}
std::vector<RuntimeMemory> runtime_memory(const Json &j) {
    require(j.contains("models") && j.at("models").is_array() && j.at("models").size() <= 10000, "Invalid running-model response");
    std::vector<RuntimeMemory> out;
    for (const auto &v : j.at("models")) {
        require(v.is_object(), "Invalid running-model entry");
        RuntimeMemory r; r.model = text(v, "name"); require(valid_model(r.model), "Invalid running-model name");
        const auto size = number(v, "size"), vram = number(v, "size_vram");
        // Entries without exact sizes are not evidence and are left out rather than read as zero.
        if (!size || !vram || *size == 0 || *vram > *size) continue;
        r.size = *size; r.size_vram = *vram; out.push_back(std::move(r));
    }
    return out;
}
std::optional<BackendObservation> observe_backend(const std::vector<RuntimeMemory> &loaded, const std::string &version, const std::string &adapters, const std::string &at) {
    if (version.empty() || at.empty()) return {};
    const RuntimeMemory *best = nullptr;
    for (const auto &r : loaded) if (!best || r.size_vram > best->size_vram) best = &r;
    if (!best) return {};
    return BackendObservation{at, version, adapters, best->model, best->size, best->size_vram};
}
Json backend_json(const BackendObservation &o) {
    return {{"schema",1},{"observed_at",o.observed_at},{"runtime_version",o.runtime_version},{"adapters",o.adapters},{"model",o.model},{"size",o.size},{"size_vram",o.size_vram}};
}
BackendObservation load_backend(const Json &j) {
    require(j.is_object() && j.value("schema",0) == 1, "Unsupported backend evidence schema");
    BackendObservation o; o.observed_at = text(j,"observed_at",40); o.runtime_version = text(j,"runtime_version",80); o.adapters = text(j,"adapters",4096); o.model = text(j,"model",200);
    require(!o.observed_at.empty() && !o.runtime_version.empty() && valid_model(o.model), "Incomplete backend evidence");
    const auto size = number(j,"size"), vram = number(j,"size_vram"); require(size && vram && *size > 0 && *vram <= *size, "Invalid backend evidence sizes");
    o.size = *size; o.size_vram = *vram; return o;
}
void apply_backend(Hardware &h, const std::optional<BackendObservation> &o, const std::string &version) {
    h.backend_verified = false; h.backend_state = BackendState::Unverified; h.backend.clear(); h.backend_observation = o;
    h.usable_vram = usable_vram(h.gpus);
    if (!o) return;
    // Evidence belongs to the adapters, drivers and runtime it was observed with.
    if (o->adapters != adapter_identity(h.gpus) || (!version.empty() && version != o->runtime_version)) { h.backend_state = BackendState::Changed; return; }
    h.backend_verified = true;
    if (o->size_vram > 0) { h.backend_state = BackendState::Gpu; h.backend = "gpu"; }
    else { h.backend_state = BackendState::CpuOnly; h.backend = "cpu"; h.usable_vram = 0; }
}
std::string kv_cache_name(KvCache c) { switch (c) { case KvCache::Q8_0: return "q8_0"; case KvCache::Q4_0: return "q4_0"; default: return "f16"; } }
std::optional<KvCache> kv_cache_from(const std::string &s) {
    for (auto c : {KvCache::F16, KvCache::Q8_0, KvCache::Q4_0})
        if (kv_cache_name(c) == s) return c;
    return {};
}
std::optional<std::uint64_t> context_memory(const Model &m, std::uint64_t context, KvCache cache) {
    if (context == 0 || !m.block_count || !*m.block_count) return {};
    const auto heads = m.head_count, kv = m.head_count_kv ? m.head_count_kv : m.head_count;
    if (!kv || !*kv) return {};
    std::optional<std::uint64_t> key = m.key_length;
    if (!key && heads && *heads && m.embedding_length && *m.embedding_length) key = (*m.embedding_length + *heads - 1) / *heads;
    const auto value = m.value_length ? m.value_length : key;
    if (!key || !*key || !value || !*value) return {};
    try {
        const auto elements = times(times(times(context, *m.block_count), *kv), plus(*key, *value));
        switch (cache) {
        case KvCache::Q8_0: return times(plus(elements, 31) / 32, 34); // 32 values per 34-byte block
        case KvCache::Q4_0: return times(plus(elements, 31) / 32, 18); // 32 values per 18-byte block
        default: return times(elements, 2);
        }
    } catch (...) { return {}; }
}
const std::vector<std::uint64_t> &fit_contexts() { static const std::vector<std::uint64_t> values{2048, 4096, 8192, 16384, 32768, 65536, 131072}; return values; }
Json fit_settings_json(const FitSettings &s) { return {{"schema",1},{"context",s.context},{"kv_cache",kv_cache_name(s.cache)}}; }
FitSettings load_fit_settings(const Json &j) {
    require(j.is_object() && j.value("schema",0) == 1, "Unsupported fit settings schema");
    FitSettings s; const auto context = number(j,"context"); const auto &allowed = fit_contexts();
    require(context && std::find(allowed.begin(), allowed.end(), *context) != allowed.end(), "Unsupported estimate context");
    const auto cache = kv_cache_from(text(j,"kv_cache",8)); require(cache.has_value(), "Unsupported context cache precision");
    s.context = *context; s.cache = *cache; return s;
}
std::string fit_label(Fit f) { switch (f) { case Fit::RunsWell:return "Runs well"; case Fit::WithLimits:return "Runs with limits"; case Fit::Unlikely:return "Unlikely"; default:return "Unknown"; } }
std::string fit_note(FitNote n) {
    switch (n) {
    case FitNote::Conservative: return "Estimates are conservative and do not guarantee execution.";
    case FitNote::MissingSizeOrDestination: return "Exact model size, destination space, or measurement timestamp is unavailable.";
    case FitNote::StorageOverflow: return "Storage estimate overflow.";
    case FitNote::InsufficientDisk: return "Destination has less than model size plus ten percent safety allowance.";
    case FitNote::MissingMemoryEvidence: return "RAM, architecture, parameters, quantization, context, and context-memory evidence are all required.";
    case FitNote::ContextExceedsModel: return "Requested context exceeds the verified model context.";
    case FitNote::MemoryOverflow: return "Memory estimate overflow.";
    case FitNote::MemoryFormula: return "Memory estimate: blob bytes + twenty percent runtime allowance + context-memory estimate.";
    case FitNote::PartialOffload: return "CPU or partial GPU offload may be necessary; throughput is not predicted.";
    case FitNote::NoMemoryFits: return "Neither measured RAM nor measured VRAM accommodates this estimate.";
    case FitNote::GpuUnverifiedCpuEstimate: return "The runtime has not shown a working GPU backend; the verdict assumes processor-only inference.";
    case FitNote::GpuUnverified: return "Measured RAM alone is not enough and the runtime has not shown a working GPU backend.";
    case FitNote::ContextLimitedToModel: return "The estimate context was limited to the model's verified maximum.";
    }
    return {};
}
FitResult fit(const Model &m, const Hardware &h, std::uint64_t context, std::optional<std::uint64_t> overhead) {
    FitResult r; r.context = context; r.context_bytes = overhead; r.measured_at = h.measured_at; r.notes.push_back(FitNote::Conservative);
    if (!m.bytes || !*m.bytes || !h.free_disk || h.measured_at.empty()) { r.notes.push_back(FitNote::MissingSizeOrDestination); return r; }
    try { r.disk_required = m.installed ? 0 : plus(*m.bytes, *m.bytes / 10); }
    catch (...) { r.notes.push_back(FitNote::StorageOverflow); return r; }
    if (*h.free_disk < *r.disk_required) { r.verdict = Fit::Unlikely; r.notes.push_back(FitNote::InsufficientDisk); return r; }
    if (!h.available_ram || h.architecture.empty() || !m.parameter_count || !m.context_length || m.quantization.empty() || !overhead) {
        r.notes.push_back(FitNote::MissingMemoryEvidence); return r;
    }
    if (context == 0 || context > *m.context_length) { r.verdict = Fit::Unlikely; r.notes.push_back(FitNote::ContextExceedsModel); return r; }
    try { r.memory_required = plus(plus(*m.bytes, *m.bytes / 5), *overhead); }
    catch (...) { r.notes.push_back(FitNote::MemoryOverflow); return r; }
    r.notes.push_back(FitNote::MemoryFormula);
    if (h.backend_verified && h.usable_vram) {
        if (*h.usable_vram >= *r.memory_required && *h.available_ram >= *m.bytes / 5) r.verdict = Fit::RunsWell;
        else if (*h.available_ram >= *r.memory_required) { r.verdict = Fit::WithLimits; r.notes.push_back(FitNote::PartialOffload); }
        else { r.verdict = Fit::Unlikely; r.notes.push_back(FitNote::NoMemoryFits); }
    } else if (*h.available_ram >= *r.memory_required) {
        // Without the runtime's own GPU evidence only processor inference is evidenced, so never better than limits.
        r.verdict = Fit::WithLimits; r.notes.push_back(FitNote::GpuUnverifiedCpuEstimate);
    } else r.notes.push_back(FitNote::GpuUnverified);
    return r;
}
FitResult assess(const Model &m, const Hardware &h, const FitSettings &s) {
    auto context = s.context; bool limited = false;
    if (m.context_length && *m.context_length && context > *m.context_length) { context = *m.context_length; limited = true; }
    auto r = fit(m, h, context, context_memory(m, context, s.cache));
    if (limited) r.notes.insert(r.notes.begin() + 1, FitNote::ContextLimitedToModel);
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
std::string model_family(const std::string &name) { return name.substr(0, name.find(':')); }
std::string model_variant(const std::string &name) { const auto colon = name.find(':'); return colon == std::string::npos ? std::string("latest") : name.substr(colon + 1); }
StoreState store_state(const Model &m) { return m.running ? StoreState::Running : m.installed ? StoreState::Installed : StoreState::Catalog; }
SizeBand size_band(const Model &m) {
    if (!m.bytes) return SizeBand::Unknown;
    constexpr std::uint64_t GiB = 1024ull * 1024ull * 1024ull;
    if (*m.bytes < 2 * GiB) return SizeBand::Under2GiB;
    if (*m.bytes < 8 * GiB) return SizeBand::From2To8GiB;
    if (*m.bytes < 32 * GiB) return SizeBand::From8To32GiB;
    return SizeBand::Over32GiB;
}
namespace {
// Groups appear in a fixed meaningful order: best fit first, running before installed, smaller sizes first,
// alphabetical families and quantizations with unverified values last.
std::pair<int, std::string> group_order(const StoreEntry &e, StoreGroup g) {
    switch (g) {
    case StoreGroup::Family: return {0, model_family(e.model.name)};
    case StoreGroup::State: return {static_cast<int>(store_state(e.model)), {}};
    case StoreGroup::Fit: return {static_cast<int>(e.fit), {}};
    case StoreGroup::Quantization: return {e.model.quantization.empty() ? 1 : 0, e.model.quantization};
    case StoreGroup::Size: return {static_cast<int>(size_band(e.model)), {}};
    default: return {0, {}};
    }
}
bool sorted_before(const StoreEntry &a, const StoreEntry &b, StoreSort s) {
    switch (s) {
    case StoreSort::Family: {
        const auto fa = model_family(a.model.name), fb = model_family(b.model.name);
        if (fa != fb) return fa < fb;
        break;
    }
    case StoreSort::SizeAscending:
    case StoreSort::SizeDescending:
        // Unknown sizes always sort last: they are not small, they are unmeasured.
        if (a.model.bytes.has_value() != b.model.bytes.has_value()) return a.model.bytes.has_value();
        if (a.model.bytes && *a.model.bytes != *b.model.bytes) return s == StoreSort::SizeAscending ? *a.model.bytes < *b.model.bytes : *a.model.bytes > *b.model.bytes;
        break;
    case StoreSort::Fit:
        if (a.fit != b.fit) return static_cast<int>(a.fit) < static_cast<int>(b.fit);
        break;
    default: break;
    }
    return a.model.name < b.model.name;
}
}
std::vector<StoreRow> query_store(const std::vector<StoreEntry> &entries, const StoreQuery &q, const std::function<bool(const Model &)> &text) {
    std::vector<std::size_t> hits;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i]; const auto &m = e.model;
        // Installed includes running models; Running and Catalog are exact.
        if (q.state && (*q.state == StoreState::Installed ? !m.installed : store_state(m) != *q.state)) continue;
        if (q.family && model_family(m.name) != *q.family) continue;
        if (q.variant && model_variant(m.name) != *q.variant) continue;
        if (q.capability) {
            if (q.capability->empty() ? m.capabilities_verified : (!m.capabilities_verified || !m.capabilities.count(*q.capability))) continue;
        }
        if (q.quantization && m.quantization != *q.quantization) continue;
        if (q.size && size_band(m) != *q.size) continue;
        if (q.fit && e.fit != *q.fit) continue;
        if (text && !text(m)) continue;
        hits.push_back(i);
    }
    std::stable_sort(hits.begin(), hits.end(), [&](std::size_t a, std::size_t b) {
        const auto ga = group_order(entries[a], q.group), gb = group_order(entries[b], q.group);
        if (ga != gb) return ga < gb;
        return sorted_before(entries[a], entries[b], q.sort);
    });
    std::vector<StoreRow> rows;
    for (std::size_t i = 0; i < hits.size(); ++i) {
        if (q.group != StoreGroup::None && (i == 0 || group_order(entries[hits[i]], q.group) != group_order(entries[hits[i - 1]], q.group))) {
            StoreRow header; header.header = true; header.entry = hits[i];
            for (std::size_t j = i; j < hits.size() && group_order(entries[hits[j]], q.group) == group_order(entries[hits[i]], q.group); ++j) ++header.members;
            rows.push_back(header);
        }
        StoreRow row; row.entry = hits[i]; rows.push_back(row);
    }
    return rows;
}
std::size_t store_matches(const std::vector<StoreRow> &rows) { return static_cast<std::size_t>(std::count_if(rows.begin(), rows.end(), [](const StoreRow &r) { return !r.header; })); }
StoreFacets store_facets(const std::vector<StoreEntry> &entries) {
    std::set<std::string> families, capabilities, quantizations; StoreFacets f;
    for (const auto &e : entries) {
        families.insert(model_family(e.model.name));
        if (e.model.capabilities_verified) capabilities.insert(e.model.capabilities.begin(), e.model.capabilities.end()); else f.unverified_capabilities = true;
        if (e.model.quantization.empty()) f.unverified_quantization = true; else quantizations.insert(e.model.quantization);
    }
    f.families.assign(families.begin(), families.end()); f.capabilities.assign(capabilities.begin(), capabilities.end()); f.quantizations.assign(quantizations.begin(), quantizations.end());
    return f;
}
std::vector<std::string> family_variants(const std::vector<StoreEntry> &entries, const std::string &family) {
    std::set<std::string> variants;
    for (const auto &e : entries) if (model_family(e.model.name) == family) variants.insert(model_variant(e.model.name));
    return {variants.begin(), variants.end()};
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
    // Validation failures mean the source structure no longer matches; specific stages say otherwise.
    CatalogFailure failure = CatalogFailure::Malformed;
    try {
        while (!pending.empty()) {
            if (cancel.load()) { failure = CatalogFailure::Cancelled; throw std::runtime_error("Catalog refresh cancelled"); }
            if (visited.size() >= 10000) { failure = CatalogFailure::Bound; throw std::runtime_error("Catalog page safety bound reached"); }
            const auto path = *pending.begin(); pending.erase(pending.begin()); if (!visited.insert(path).second) continue;
            CatalogPage p;
            try { p = fetch(path); }
            catch (...) { failure = cancel.load() ? CatalogFailure::Cancelled : CatalogFailure::Unavailable; throw; }
            require(p.path == path && !p.response_identity.empty() && !p.fetched_at.empty(), "Catalog page lacks source receipt");
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
            if (variants.size() > 200000) { failure = CatalogFailure::Bound; throw std::runtime_error("Catalog variant safety bound reached"); }
        }
        for (const auto &n : variants) { Model m; m.name = n; s.models.push_back(std::move(m)); }
        // Require an explicit count for each source collection and exact reconciliation across its pages.
        std::map<std::string, std::set<std::string>> actual; std::map<std::string, std::size_t> expected;
        for (const auto &p : s.pages) {
            auto base = p.path.substr(0,p.path.find('?')); actual[base].insert(p.names.begin(), p.names.end());
            if (p.advertised_count) { auto [i, inserted] = expected.emplace(base,*p.advertised_count); require(inserted || i->second == *p.advertised_count, "Catalog changed during pagination"); }
        }
        for (const auto &[base, names] : actual) counts_verified = counts_verified && expected.count(base) && expected[base] == names.size();
        s.traversal_complete = !families.empty() && !variants.empty(); s.authority_total_known = counts_verified;
        s.complete = counts_verified && s.traversal_complete;
        s.families = families.size();
        s.verdict = s.complete ? CatalogVerdict::Certified : s.traversal_complete ? CatalogVerdict::Traversed : CatalogVerdict::Failed;
        s.reason = s.complete ? "Every advertised collection count reconciled across all pages." : "Every family, tag and page link was traversed. The official HTML has no verified total-count contract, so completeness is by traversal.";
        if (catalog_verified(s)) s.last_successful_refresh = s.refreshed_at;
    } catch (const std::exception &e) {
        // A partial traversal is never presented: no entry from it survives.
        s.reason = e.what(); s.complete = false; s.offline = true; s.traversal_complete = false;
        s.verdict = CatalogVerdict::Failed; s.failure = failure; s.models.clear(); s.families = 0;
    }
    return s;
}
bool catalog_verified(const CatalogSnapshot &s) {
    return (s.verdict == CatalogVerdict::Traversed || s.verdict == CatalogVerdict::Certified) && !s.models.empty() && !s.pages.empty();
}
std::string catalog_receipts(const CatalogSnapshot &s) {
    std::vector<const CatalogPage *> pages; for (const auto &p : s.pages) pages.push_back(&p);
    std::sort(pages.begin(), pages.end(), [](const CatalogPage *a, const CatalogPage *b) { return a->path < b->path; });
    std::string out;
    for (const auto *p : pages) {
        out += p->path + '\t' + p->response_identity + '\t' + (p->advertised_count ? std::to_string(*p->advertised_count) : std::string("-")) + '\t';
        for (const auto &n : p->names) out += n + ' ';
        out += '\t';
        for (const auto &n : p->next_pages) out += n + ' ';
        out += '\n';
    }
    return out;
}
std::string catalog_revision(const CatalogSnapshot &s, const Digest &digest) {
    require(bool(digest) && !s.pages.empty(), "Catalog revision needs pages and a digest");
    const auto revision = digest(catalog_receipts(s)); require(!revision.empty() && revision.size() <= 128, "Invalid catalog revision"); return revision;
}
namespace {
std::string verdict_name(CatalogVerdict v) { switch (v) { case CatalogVerdict::Certified: return "certified"; case CatalogVerdict::Traversed: return "traversed"; case CatalogVerdict::Failed: return "failed"; default: return "none"; } }
CatalogVerdict verdict_from(const std::string &s) {
    for (auto v : {CatalogVerdict::None, CatalogVerdict::Failed, CatalogVerdict::Traversed, CatalogVerdict::Certified}) if (verdict_name(v) == s) return v;
    throw std::runtime_error("Unknown catalog verdict");
}
}
Json catalog_json(const CatalogSnapshot &s) {
    Json j = {{"schema",2},{"verdict",verdict_name(s.verdict)},{"revision",s.revision},{"page_count",s.pages.size()},{"families",s.families},{"variants",s.models.size()},
              {"complete",s.complete},{"traversal_complete",s.traversal_complete},{"authority_total_known",s.authority_total_known},
              {"refreshed_at",s.refreshed_at},{"last_successful_refresh",s.last_successful_refresh},{"pages",Json::array()}};
    for (const auto &p : s.pages) j["pages"].push_back({{"path",p.path},{"response_identity",p.response_identity},{"fetched_at",p.fetched_at},{"names",p.names},{"next_pages",p.next_pages},{"advertised_count",p.advertised_count ? Json(*p.advertised_count) : Json(nullptr)}});
    return j;
}
CatalogSnapshot load_catalog(const Json &j, const Digest &digest) {
    const int schema = j.value("schema",0);
    require((schema == 1 || schema == 2) && j.at("pages").is_array() && j.at("pages").size() <= 10000, "Invalid catalog cache");
    std::map<std::string,CatalogPage> pages;
    for (const auto &p : j.at("pages")) { CatalogPage x; x.path=text(p,"path"); x.response_identity=text(p,"response_identity"); x.fetched_at=text(p,"fetched_at"); x.names=p.at("names").get<std::vector<std::string>>(); x.next_pages=p.at("next_pages").get<std::vector<std::string>>(); if (!p.at("advertised_count").is_null()) x.advertised_count=p.at("advertised_count").get<std::size_t>(); require(pages.emplace(x.path,x).second,"Duplicate cached catalog page"); }
    // The saved pages are traversed again, so a damaged or incomplete cache fails closed instead of being trusted.
    std::atomic_bool cancel{false}; auto s=refresh_catalog([&](const std::string &path) { require(pages.count(path),"Cached catalog page missing"); return pages.at(path); },cancel);
    s.refreshed_at=text(j,"refreshed_at",40); s.offline=false; s.cached=true;
    if (catalog_verified(s)) {
        const auto saved=text(j,"last_successful_refresh",40); s.last_successful_refresh=saved.empty() ? s.refreshed_at : saved;
        if (schema == 2) {
            s.revision=text(j,"revision",128);
            if (digest && !s.revision.empty()) require(catalog_revision(s,digest)==s.revision,"Saved catalog no longer matches its revision");
        }
    } else s.last_successful_refresh.clear();
    return s;
}
CatalogAttempt catalog_attempt(const CatalogSnapshot &s) {
    CatalogAttempt a; a.attempted_at = s.refreshed_at; a.revision = s.revision; a.verdict = s.verdict; a.failure = s.failure; a.pages = s.pages.size(); return a;
}
Json attempt_json(const CatalogAttempt &a) {
    return {{"schema",1},{"attempted_at",a.attempted_at},{"revision",a.revision},{"verdict",verdict_name(a.verdict)},{"failure",static_cast<int>(a.failure)},{"pages",a.pages}};
}
CatalogAttempt load_attempt(const Json &j) {
    require(j.is_object() && j.value("schema",0) == 1, "Unsupported catalog attempt schema");
    CatalogAttempt a; a.attempted_at = text(j,"attempted_at",40); a.revision = text(j,"revision",128); a.verdict = verdict_from(text(j,"verdict",16));
    const int failure = j.at("failure").get<int>(); require(failure >= 0 && failure <= static_cast<int>(CatalogFailure::Bound), "Invalid catalog failure");
    a.failure = static_cast<CatalogFailure>(failure); a.pages = j.at("pages").get<std::size_t>(); require(utc_seconds(a.attempted_at).has_value(), "Invalid attempt time");
    return a;
}
std::optional<std::int64_t> utc_seconds(const std::string &t) {
    // Exactly YYYY-MM-DDTHH:MM:SSZ, as utc_now() writes it.
    if (t.size() != 20 || t[4] != '-' || t[7] != '-' || t[10] != 'T' || t[13] != ':' || t[16] != ':' || t[19] != 'Z') return {};
    auto digits = [&](std::size_t at, std::size_t n) -> std::optional<int> {
        int v = 0; for (std::size_t i = at; i < at + n; ++i) { if (t[i] < '0' || t[i] > '9') return {}; v = v * 10 + (t[i] - '0'); } return v;
    };
    const auto y = digits(0,4), mo = digits(5,2), d = digits(8,2), h = digits(11,2), mi = digits(14,2), se = digits(17,2);
    if (!y || !mo || !d || !h || !mi || !se || *mo < 1 || *mo > 12 || *d < 1 || *d > 31 || *h > 23 || *mi > 59 || *se > 60) return {};
    // Days from the civil date (proleptic Gregorian calendar).
    const std::int64_t year = *y - (*mo <= 2 ? 1 : 0), era = (year >= 0 ? year : year - 399) / 400;
    const std::int64_t yoe = year - era * 400, doy = (153 * (*mo + (*mo > 2 ? -3 : 9)) + 2) / 5 + *d - 1, doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const std::int64_t days = era * 146097 + doe - 719468;
    return days * 86400 + *h * 3600 + *mi * 60 + *se;
}
std::int64_t now_seconds() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
std::optional<std::int64_t> catalog_age(const CatalogSnapshot &s, std::int64_t now) {
    if (!catalog_verified(s)) return {};
    const auto at = utc_seconds(s.last_successful_refresh); if (!at) return {};
    return now > *at ? now - *at : 0;
}
bool catalog_stale(const CatalogSnapshot &s, std::int64_t now) { const auto age = catalog_age(s, now); return !age || *age > catalog_stale_seconds; }
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
