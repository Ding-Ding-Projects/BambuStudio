#include "libslic3r/OllamaSuite/PullBatch.hpp"
#include <iostream>
#include <mutex>
using namespace Slic3r::OllamaSuite;
int main() {
    auto root=std::filesystem::temp_directory_path()/("ollama-batch-test-"+unique_id()); int checks=0;
    auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("batch assertion");};
    try {
        PullQueue queue(root/"queue"); auto a=queue.add("example:a"),b=queue.add("example:b");
        auto metadata=[](const std::string &name){Model m;m.name=name;m.bytes=100;m.digest="sha256:"+std::string(64,'a');return m;};
        auto capacity=[](){return std::uint64_t(1000);};
        auto plan=PullBatch::prepare(queue,root/"plans",metadata,capacity); check(plan.summary().count==2&&plan.summary().additional_disk_bytes==220);
        std::set<std::string> installed; std::mutex mutex; std::atomic_bool cancel{false}; std::atomic_int active{0},maximum{0};
        auto is_installed=[&](const std::string &name,const std::string &digest){std::lock_guard<std::mutex> lock(mutex);return digest=="sha256:"+std::string(64,'a')&&installed.count(name)!=0;};
        auto transfer=[&](const std::string &name,const std::atomic_bool &,const std::function<void(PullProgress)> &progress){int n=++active; if(n>maximum)maximum=n; progress({50,100}); {std::lock_guard<std::mutex> lock(mutex);installed.insert(name);} --active; return name=="example:b"?TransferOutcome::Failed:TransferOutcome::Pulled;};
        auto outcome=PullBatch::run(plan,queue,2,metadata,is_installed,capacity,transfer,cancel); check(!outcome.complete&&outcome.pulled==1&&outcome.failed==1);check(maximum<=2);check(queue.read(a.id).state==PullState::Pulled);check(queue.read(b.id).state==PullState::Failed);
        bool repeated=false;try{PullBatch::run(plan,queue,2,metadata,is_installed,capacity,transfer,cancel);}catch(...){repeated=true;}check(repeated);
        queue.retry(b.id); auto retry=PullBatch::prepare(queue,root/"plans",metadata,capacity); auto retried=PullBatch::run(retry,queue,1,metadata,is_installed,capacity,transfer,cancel);check(retried.complete&&retried.skipped==1);
        auto c=queue.add("example:c"); auto cancelled=PullBatch::prepare(queue,root/"plans",metadata,capacity);cancel=true;auto result=PullBatch::run(cancelled,queue,1,metadata,is_installed,capacity,transfer,cancel);check(!result.complete&&result.cancelled==1&&queue.read(c.id).state==PullState::Cancelled);
        queue.add("example:d"); bool disk=false;try{PullBatch::prepare(queue,root/"plans",metadata,[]{return std::uint64_t(1);});}catch(...){disk=true;}check(disk);
        bool unknown=false;try{PullBatch::prepare(queue,root/"plans",[](const std::string &name){Model m;m.name=name;return m;},capacity);}catch(...){unknown=true;}check(unknown);
        std::filesystem::remove_all(root);std::cout<<"ollama batch: "<<checks<<" checks passed\n";return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<"\n";return 1;}
}
