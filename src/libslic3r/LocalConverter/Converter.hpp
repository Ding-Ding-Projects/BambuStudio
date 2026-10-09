#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace Slic3r::LocalConverter {

using Bytes = std::vector<unsigned char>;
enum class Kind { Binary, Utf8, Json, Pdf, Png, Jpeg, Bmp, Ppm, Zip, Wave, Mp4 };
enum class Category { Documents, Images, Audio, Video, Archives, Structured, Text, Binary };
enum class Outcome { Converted, Skipped, Cancelled, Failed };
struct Limits {
    static constexpr std::uint64_t input_bytes = 16 * 1024 * 1024;
    static constexpr std::uint64_t output_bytes = 64 * 1024 * 1024;
    static constexpr std::uint64_t wire_bytes = 24 * 1024 * 1024;
    static constexpr std::uint64_t pixels = 4 * 1024 * 1024;
    static constexpr std::size_t items = 100000;
    static constexpr std::size_t depth = 64;
    static constexpr std::size_t page_size = 100;
    static constexpr std::uint32_t seconds = 30;
};
// User-facing text in the registry (name, reason, disclosure, validator) is
// fixed English that doubles as the message-catalogue source; the application
// translates it at display time. A runtime diagnostic never joins that text:
// it travels separately in `detail` as a stable machine code.
struct Adapter {
    std::string id;
    Category category;
    std::string name;
    std::string extension;
    std::vector<Kind> sources;
    bool bundled = false;
    bool enabled = false;
    bool lossy = false;
    std::string reason;
    std::string disclosure;
    std::string validator;
    std::string detail;
};
struct PackageProof {
    std::filesystem::path installed_directory;
    std::filesystem::path worker;
    std::string expected_sha256;
};
// Verification requires the exact worker beside the installed application.
// A PATH lookup and a caller-supplied boolean never enable an adapter.
bool verify_package(const PackageProof &, std::string &reason);
std::vector<Adapter> catalog(const PackageProof &);
const char *category_name(Category);
const char *kind_name(Kind);
Kind detect(const Bytes &);
bool valid_utf8(const Bytes &);
struct Conversion {
    Outcome outcome = Outcome::Failed;
    std::string code;
    Bytes output;
};
// Pure bounded implementation. Production callers must invoke this in WorkerMain.
Conversion transform(const std::string &adapter, const Bytes &source);

struct Result {
    Outcome outcome = Outcome::Failed;
    std::string code;
    std::uint64_t bytes = 0;
};
using Executor = std::function<Conversion(const std::string &, const Bytes &, const std::atomic<bool> &)>;
Result convert_file(const std::filesystem::path &source, const std::filesystem::path &destination,
                    const std::string &adapter, const Executor &, const std::atomic<bool> &cancel,
                    const std::string &options = "", const std::vector<std::filesystem::path> &additional_sources = {});
// Writes through an exclusive sibling file, flushes, validates and publishes
// without replacing an existing destination. Source and destination may not alias.
bool atomic_create(const std::filesystem::path &, const Bytes &, std::string &code);

enum class State { Pending, Running, Converted, Skipped, Cancelled, Failed, RecoveryRequired };
struct Job {
    std::uint64_t id = 0;
    std::filesystem::path source, destination;
    std::string adapter;
    State state = State::Pending;
    std::string code;
    std::uint64_t input_size = 0;
    std::int64_t input_modified = 0;
    std::string options;
    std::vector<std::filesystem::path> additional_sources;
    std::vector<std::uint64_t> additional_sizes;
    std::vector<std::int64_t> additional_modified;
    std::uint64_t cancellation_generation = 0;
};
// One bounded JSON record per item; no vector of all queue paths. Queue roots
// belong in the application's private local data directory, never in a log.
// One owner holds a process-level directory lock for the queue lifetime.
class Queue {
public:
    explicit Queue(std::filesystem::path root);
    ~Queue();
    Queue(const Queue &) = delete;
    Queue &operator=(const Queue &) = delete;
    std::uint64_t enqueue(const std::filesystem::path &, const std::filesystem::path &, const std::string &,
                          const std::string &options = "", const std::vector<std::filesystem::path> &additional_sources = {});
    std::uint64_t record_rejected(const std::filesystem::path &, const std::filesystem::path &, const std::string &, const std::string &code);
    std::vector<Job> page(std::uint64_t after, std::size_t count = Limits::page_size) const;
    bool step(const Executor &, const std::atomic<bool> &cancel);
    void pause(bool);
    bool paused() const;
    void cancel_pending();
    void retry(std::uint64_t);
    std::uint64_t count() const;
private:
    std::filesystem::path m_root;
    mutable std::mutex m_mutex;
    std::uint64_t m_count = 0, m_cursor = 1;
    std::uint64_t m_cancellation_generation = 0;
    bool m_paused = true;
    bool m_active = false;
    void *m_lock = nullptr;
    void save_meta();
    Job read(std::uint64_t) const;
    void write(const Job &);
};
const char *state_name(State);
// English catalogue source describing a stable result or diagnostic code, or
// nullptr for a code the converter never produces. Codes that end in a Windows
// error number (isolated_worker_start_5) share one message for their prefix.
const char *result_message(const std::string &code);

} // namespace Slic3r::LocalConverter
