#include "PullBatch.hpp"
#include <chrono>
#include <limits>
#include <mutex>
#include <thread>

namespace Slic3r::OllamaSuite {
namespace {
void require(bool b,const char *m) { if(!b) throw std::runtime_error(m); }
std::uint64_t sum(std::uint64_t a,std::uint64_t b) { require(b<=(std::numeric_limits<std::uint64_t>::max)()-a,"Batch size overflow"); return a+b; }
}
BatchPlan PullBatch::prepare(PullQueue &queue,const std::filesystem::path &root,const Metadata &metadata,const Capacity &capacity) {
    require(bool(metadata)&&bool(capacity),"Batch metadata and destination evidence are required");
    BatchPlan plan; plan.m_items=root/unique_id(); plan.m_summary.created_at=utc_now(); std::filesystem::create_directories(plan.m_items);
    std::size_t offset=0;
    for(;;) {
        auto page=queue.page(offset,100); if(page.empty()) break; offset+=page.size();
        for(const auto &item:page) {
            if(item.state!=PullState::Queued) continue;
            auto m=metadata(item.model); require(m.name==item.model&&m.local&&m.bytes&&*m.bytes&&m.digest.size()==71&&m.digest.compare(0,7,"sha256:")==0,"Exact local model manifest evidence is required");
            const auto disk=m.installed?0:sum(*m.bytes,*m.bytes/10);
            plan.m_summary.count=sum(plan.m_summary.count,1); plan.m_summary.download_bytes=sum(plan.m_summary.download_bytes,m.installed?0:*m.bytes);
            plan.m_summary.additional_disk_bytes=sum(plan.m_summary.additional_disk_bytes,disk);
            atomic_json(plan.m_items/(item.id+".json"),{{"schema",1},{"id",item.id},{"model",item.model},{"digest",m.digest},{"bytes",*m.bytes},{"disk_bytes",disk}});
        }
    }
    require(plan.m_summary.count>0,"No queued models to review"); require(capacity()>=plan.m_summary.additional_disk_bytes,"Insufficient verified destination space");
    return plan;
}
BatchOutcome PullBatch::run(BatchPlan &plan,PullQueue &queue,unsigned parallelism,const Metadata &metadata,const Installed &installed,
                           const Capacity &capacity,const Transfer &transfer,const std::atomic_bool &cancel) {
    require(!plan.m_consumed&&parallelism>=1&&parallelism<=4&&bool(metadata)&&bool(installed)&&bool(capacity)&&bool(transfer),"Invalid or consumed batch plan");
    require(capacity()>=plan.m_summary.additional_disk_bytes,"Destination space changed before transfer");
    plan.m_consumed=true; BatchOutcome out; std::mutex mutex;
    auto cursor=std::filesystem::directory_iterator(plan.m_items),end=std::filesystem::directory_iterator();
    auto worker=[&] {
        for(;;) {
            std::filesystem::path path;
            { std::lock_guard<std::mutex> lock(mutex); if(cursor==end) return; path=cursor->path(); ++cursor; }
            PullItem item; bool loaded=false;
            try {
                const auto row=read_json(path,4096); require(row.value("schema",0)==1,"Invalid reviewed batch row");
                item=queue.read(row.at("id").get<std::string>()); loaded=true;
                require(item.model==row.at("model").get<std::string>()&&item.state==PullState::Queued,"Queue item changed after review");
                const auto digest=row.at("digest").get<std::string>();
                if(cancel.load()) { item.state=PullState::Cancelled; item.message="Cancelled before transfer."; }
                else if(installed(item.model,digest)) { item.state=PullState::Skipped; item.message="Exact tag and reviewed manifest already installed."; }
                else {
                    const auto current=metadata(item.model);
                    require(current.name==item.model&&current.digest==digest&&current.bytes&&*current.bytes==row.at("bytes").get<std::uint64_t>(),"Registry tag changed after review");
                    require(capacity()>=row.at("disk_bytes").get<std::uint64_t>(),"Destination space changed before item transfer");
                    item.state=PullState::Pulling; item.message="Downloading through the local Ollama API."; queue.save(item);
                    auto last=std::chrono::steady_clock::now();
                    auto result=transfer(item.model,cancel,[&](PullProgress p) {
                        require(p.total==0||p.completed<=p.total,"Invalid local byte progress"); item.completed=p.completed; item.total=p.total;
                        auto now=std::chrono::steady_clock::now(); if(now-last>=std::chrono::seconds(1)) { queue.save(item); last=now; }
                    });
                    if(result==TransferOutcome::Pulled && installed(item.model,digest)) { item.state=PullState::Pulled; item.message="Pull completed and installed manifest reconciled."; }
                    else if(result==TransferOutcome::Cancelled||cancel.load()) { item.state=PullState::Cancelled; item.message="Transfer cancelled. Existing valid models were retained."; }
                    else { item.state=PullState::Failed; item.message="Transfer failed or installed tag could not be reconciled. Retry this item after inspecting runtime health."; }
                }
                queue.save(item);
            } catch(...) {
                if(loaded) { item.state=PullState::Failed; item.message="Reviewed item failed validation, transfer or persistence. No installed model was deleted."; try {queue.save(item);}catch(...){} }
            }
            std::lock_guard<std::mutex> lock(mutex);
            if(loaded&&item.state==PullState::Pulled) ++out.pulled;
            else if(loaded&&item.state==PullState::Skipped) ++out.skipped;
            else if(loaded&&item.state==PullState::Cancelled) ++out.cancelled;
            else ++out.failed;
        }
    };
    std::vector<std::thread> workers;
    try { for(unsigned i=0;i<parallelism;++i) workers.emplace_back(worker); }
    catch(...) { for(auto &t:workers) if(t.joinable()) t.join(); throw; }
    for(auto &t:workers) t.join();
    out.complete=out.failed==0&&out.cancelled==0&&sum(out.pulled,out.skipped)==plan.m_summary.count; return out;
}
}
