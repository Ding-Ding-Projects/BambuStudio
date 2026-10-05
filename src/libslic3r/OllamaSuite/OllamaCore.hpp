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
    std::set<std::string> capabilities;
    bool installed = false, running = false, capabilities_verified = false, local = true;
};
std::vector<Model> installed_models(const Json &response);
void apply_details(Model &model, const Json &response);
std::vector<Model> reconcile(const std::vector<Model> &catalog, const std::vector<Model> &installed,
                             const std::set<std::string> &running);
struct Hardware {
    std::optional<std::uint64_t> available_ram, usable_vram, free_disk;
    std::string architecture, gpu, backend, measured_at;
    bool backend_verified = false;
};
enum class Fit { RunsWell, WithLimits, Unlikely, Unknown };
struct FitResult { Fit verdict = Fit::Unknown; std::vector<std::string> evidence; std::optional<std::uint64_t> memory_required, disk_required; };
FitResult fit(const Model &, const Hardware &, std::uint64_t context, std::optional<std::uint64_t> context_bytes);
std::string fit_label(Fit);
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
struct CatalogSnapshot {
    std::vector<Model> models;
    std::vector<CatalogPage> pages;
    std::string refreshed_at, last_successful_refresh, reason;
    bool complete = false, offline = false;
};
using CatalogFetcher = std::function<CatalogPage(const std::string &)>;
CatalogSnapshot refresh_catalog(const CatalogFetcher &, const std::atomic_bool &cancel);
Json catalog_json(const CatalogSnapshot &);
CatalogSnapshot load_catalog(const Json &);

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
