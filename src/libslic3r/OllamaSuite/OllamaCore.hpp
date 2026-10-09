#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Slic3r::OllamaSuite {
using Json = nlohmann::json;
constexpr std::size_t max_json_bytes = 8 * 1024 * 1024;
constexpr std::size_t max_stream_line = 256 * 1024;
constexpr std::size_t max_chat_bytes = 512 * 1024;
enum class Operation { Version, Installed, Running, Show, Pull, Delete, Copy, Chat, Generate };
struct Request { std::string method, path, body; bool streaming = false; long timeout_seconds = 30; };
bool valid_model(const std::string &name);
Request request(Operation operation, const Json &payload = Json::object());
Json parse_json(const std::string &bytes, std::size_t limit = max_json_bytes);

// Incremental framing never accumulates a whole stream. Callback failure aborts it.
class Ndjson {
public:
    explicit Ndjson(std::function<bool(const Json &)> consume);
    bool feed(const char *bytes, std::size_t length);
    bool finish();
    const std::string &error() const { return m_error; }
private:
    bool line();
    std::string m_buffer, m_error;
    std::function<bool(const Json &)> m_consume;
    std::size_t m_total = 0;
};
struct Model {
    std::string name, digest, family, parameters, quantization;
    std::optional<std::uint64_t> bytes, parameter_count, context_length;
    // Attention geometry from /api/show model_info. It feeds only the context-memory estimate.
    std::optional<std::uint64_t> block_count, head_count, head_count_kv, key_length, value_length, embedding_length;
    std::set<std::string> capabilities;
    bool installed = false, running = false, capabilities_verified = false, local = true;
};
std::vector<Model> installed_models(const Json &response);
void apply_manifest(Model &model, const Json &manifest, const std::string &response_sha256);
void apply_details(Model &model, const Json &response);
std::vector<Model> reconcile(const std::vector<Model> &catalog, const std::vector<Model> &installed,
                             const std::set<std::string> &running);

// Ollama keeps models under OLLAMA_MODELS, or <home>/.ollama/models when that setting is absent.
// Only those documented sources become candidates. A candidate is proven to be the runtime's
// destination when every checked installed model reported by the local API has its manifest there.
enum class DestinationSource { ProcessSetting, UserSetting, MachineSetting, Default };
enum class DestinationProof { Unknown, Configured, Manifests };
struct DestinationCandidate { std::filesystem::path path; DestinationSource source = DestinationSource::Default; };
struct DestinationProbe {
    DestinationCandidate candidate;
    std::size_t manifests_checked = 0, manifests_found = 0;
    std::optional<std::uint64_t> free_bytes;
};
struct ModelDestination {
    std::filesystem::path path;
    DestinationSource source = DestinationSource::Default;
    DestinationProof proof = DestinationProof::Unknown;
    std::size_t manifests_checked = 0, manifests_found = 0, candidates = 0;
    std::optional<std::uint64_t> free_bytes;
    bool candidates_disagree = false;
};
// Relative manifest location of an exact model name: manifests/<host>/<namespace>/<model>/<tag>.
std::optional<std::filesystem::path> manifest_path(const std::string &model);
DestinationProbe probe_destination(const DestinationCandidate &, const std::vector<Model> &installed, std::size_t limit = 64);
ModelDestination choose_destination(const std::vector<DestinationProbe> &);

struct GpuAdapter { std::string name, driver_version; std::uint64_t dedicated_bytes = 0; std::optional<std::uint64_t> budget_bytes; };
// Largest single adapter's dedicated memory, capped by the operating system's current budget.
std::optional<std::uint64_t> usable_vram(const std::vector<GpuAdapter> &);
std::string adapter_identity(const std::vector<GpuAdapter> &);
// What the runtime reported for loaded models (GET /api/ps): exact total and GPU-resident bytes.
struct RuntimeMemory { std::string model; std::uint64_t size = 0, size_vram = 0; };
std::vector<RuntimeMemory> runtime_memory(const Json &response);
// The runtime's own evidence of a working GPU backend, bound to the adapters, drivers and runtime version it was seen with.
struct BackendObservation { std::string observed_at, runtime_version, adapters, model; std::uint64_t size = 0, size_vram = 0; };
std::optional<BackendObservation> observe_backend(const std::vector<RuntimeMemory> &, const std::string &runtime_version,
                                                  const std::string &adapters, const std::string &observed_at);
Json backend_json(const BackendObservation &);
BackendObservation load_backend(const Json &);
enum class BackendState { Unverified, Gpu, CpuOnly, Changed };
struct Hardware {
    std::optional<std::uint64_t> available_ram, total_ram, usable_vram, free_disk;
    std::string architecture, gpu, backend, measured_at;
    bool backend_verified = false;
    // Whether the installed list used to prove the destination came from the runtime in this session.
    bool inventory_known = false;
    std::vector<GpuAdapter> gpus;
    ModelDestination destination;
    BackendState backend_state = BackendState::Unverified;
    std::optional<BackendObservation> backend_observation;
};
void apply_backend(Hardware &, const std::optional<BackendObservation> &, const std::string &runtime_version);

// Context cache precision as documented for OLLAMA_KV_CACHE_TYPE. F16 is the runtime default and the largest.
enum class KvCache { F16, Q8_0, Q4_0 };
std::string kv_cache_name(KvCache);
std::optional<KvCache> kv_cache_from(const std::string &);
// Key/value cache bytes for `context` tokens from verified attention geometry, or nothing when any input is missing.
std::optional<std::uint64_t> context_memory(const Model &, std::uint64_t context, KvCache);
struct FitSettings { std::uint64_t context = 4096; KvCache cache = KvCache::F16; };
const std::vector<std::uint64_t> &fit_contexts();
Json fit_settings_json(const FitSettings &);
FitSettings load_fit_settings(const Json &);

enum class Fit { RunsWell, WithLimits, Unlikely, Unknown };
enum class FitNote {
    Conservative, MissingSizeOrDestination, StorageOverflow, InsufficientDisk, MissingMemoryEvidence,
    ContextExceedsModel, MemoryOverflow, MemoryFormula, PartialOffload, NoMemoryFits, GpuUnverifiedCpuEstimate,
    GpuUnverified, ContextLimitedToModel
};
struct FitResult {
    Fit verdict = Fit::Unknown; std::vector<FitNote> notes;
    std::optional<std::uint64_t> memory_required, disk_required, context_bytes;
    std::uint64_t context = 0; std::string measured_at;
};
FitResult fit(const Model &, const Hardware &, std::uint64_t context, std::optional<std::uint64_t> context_bytes);
// Combines the model, the measured hardware and the configured context settings into one verdict.
FitResult assess(const Model &, const Hardware &, const FitSettings &);
std::string fit_label(Fit);
std::string fit_note(FitNote);
Json chat_payload(const Model &, const Json &messages, double temperature = 0.7,
                  std::uint64_t context = 2048, std::uint64_t output_tokens = 512);

struct CatalogPage {
    std::string path, response_identity, fetched_at;
    std::vector<std::string> names, next_pages;
    std::optional<std::size_t> advertised_count;
};
CatalogPage parse_catalog_html(const std::string &path, const std::string &html,
                               const std::string &identity, const std::string &timestamp);
bool official_catalog_path(const std::string &path);
// Traversed: every family, tag and pagination link was fetched and parsed, but the source publishes no
// verifiable totals. Certified: every advertised collection count also reconciled across its pages.
enum class CatalogVerdict { None, Failed, Traversed, Certified };
enum class CatalogFailure { None, Cancelled, Unavailable, Malformed, Bound };
struct CatalogSnapshot {
    std::vector<Model> models;
    std::vector<CatalogPage> pages;
    std::string refreshed_at, last_successful_refresh, reason, revision;
    bool complete = false, offline = false, traversal_complete = false, authority_total_known = false;
    bool cached = false; // read back from the saved last verified traversal
    CatalogVerdict verdict = CatalogVerdict::None;
    CatalogFailure failure = CatalogFailure::None;
    std::size_t families = 0;
};
using CatalogFetcher = std::function<CatalogPage(const std::string &)>;
using Digest = std::function<std::string(const std::string &)>;
CatalogSnapshot refresh_catalog(const CatalogFetcher &, const std::atomic_bool &cancel);
bool catalog_verified(const CatalogSnapshot &);
// Canonical receipt of every page (path, response identity, count, entries, links), sorted by path.
// Its digest is the catalog revision: equal revisions mean byte-identical source responses.
std::string catalog_receipts(const CatalogSnapshot &);
std::string catalog_revision(const CatalogSnapshot &, const Digest &);
Json catalog_json(const CatalogSnapshot &);
// Re-traverses the saved pages; with a digest, a revision that no longer matches them is rejected.
CatalogSnapshot load_catalog(const Json &, const Digest &digest = {});
// The latest refresh attempt, kept separately so a failure never replaces the last verified catalog.
struct CatalogAttempt {
    std::string attempted_at, revision;
    CatalogVerdict verdict = CatalogVerdict::None;
    CatalogFailure failure = CatalogFailure::None;
    std::size_t pages = 0;
};
CatalogAttempt catalog_attempt(const CatalogSnapshot &);
Json attempt_json(const CatalogAttempt &);
CatalogAttempt load_attempt(const Json &);
constexpr std::int64_t catalog_stale_seconds = 24 * 60 * 60;
std::optional<std::int64_t> utc_seconds(const std::string &timestamp);
std::int64_t now_seconds();
std::optional<std::int64_t> catalog_age(const CatalogSnapshot &, std::int64_t now);
bool catalog_stale(const CatalogSnapshot &, std::int64_t now);

enum class PullState { Queued, Pulling, Pulled, Skipped, Cancelled, Failed, Interrupted };
struct PullItem { std::string id, model, message; PullState state = PullState::Queued; std::uint64_t completed = 0, total = 0; };
// One file per item and paged directory scans keep queue length independent of RAM.
class PullQueue {
public:
    explicit PullQueue(std::filesystem::path root);
    PullItem add(const std::string &model);
    std::vector<PullItem> page(std::size_t offset, std::size_t count = 100) const;
    void save(const PullItem &);
    void recover();
    void retry(const std::string &id);
    std::optional<PullItem> next() const;
    PullItem read(const std::string &id) const;
private:
    std::filesystem::path m_root;
};
void atomic_json(const std::filesystem::path &, const Json &);
Json read_json(const std::filesystem::path &, std::size_t limit = max_json_bytes);
struct ChatSession { std::string id, title, model; Json messages = Json::array(); };
class ChatStore {
public:
    explicit ChatStore(std::filesystem::path root);
    void save(const ChatSession &);
    ChatSession read(const std::string &) const;
    std::vector<ChatSession> page(std::size_t offset, std::size_t count = 50) const;
    void remove(const std::string &, bool confirmed);
    Json redacted_export(const ChatSession &) const;
private:
    std::filesystem::path m_root;
};
std::string unique_id();
std::string utc_now();
}
