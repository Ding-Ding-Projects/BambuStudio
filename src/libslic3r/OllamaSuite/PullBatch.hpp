#pragma once
#include "OllamaCore.hpp"

namespace Slic3r::OllamaSuite {
struct BatchSummary { std::uint64_t count=0, download_bytes=0, additional_disk_bytes=0; std::string created_at; };
struct PullProgress { std::uint64_t completed=0,total=0; };
enum class TransferOutcome { Pulled, Failed, Cancelled };
struct BatchOutcome { std::uint64_t pulled=0,skipped=0,failed=0,cancelled=0; bool complete=false; };
class BatchPlan {
public:
    const BatchSummary &summary() const { return m_summary; }
    const std::filesystem::path &review_directory() const { return m_items; }
private:
    BatchSummary m_summary; std::filesystem::path m_items; bool m_consumed=false;
    friend class PullBatch;
};
// Metadata must come from the official registry adapter. Capacity is measured in
// the explicitly verified Ollama model destination, never the application-data drive by assumption.
class PullBatch {
public:
    using Metadata = std::function<Model(const std::string &)>;
    using Installed = std::function<bool(const std::string &exact_tag,const std::string &manifest_digest)>;
    using Capacity = std::function<std::uint64_t()>;
    using Transfer = std::function<TransferOutcome(const std::string &,const std::atomic_bool &,const std::function<void(PullProgress)> &)>;
    static BatchPlan prepare(PullQueue &,const std::filesystem::path &plans_root,const Metadata &,const Capacity &);
    // Calling run is the explicit reviewed transfer action. It never installs a runtime.
    static BatchOutcome run(BatchPlan &,PullQueue &,unsigned parallelism,const Metadata &,const Installed &,
                            const Capacity &,const Transfer &,const std::atomic_bool &cancel);
};
}
