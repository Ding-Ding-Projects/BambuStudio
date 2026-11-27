#include "VersionReport.hpp"
#include "ServerSystem.hpp"
#include <condition_variable>
#include <fstream>
#include <random>

namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs=std::filesystem;
namespace {
bool read_reports(const fs::path &root,VersionReport &output)
{
    std::string raw;
    if(!read_file((root/"reports"/"commands.json").u8string(),raw,32*1024*1024)) return false;
    auto commands=nlohmann::json::parse(raw,nullptr,false);
    if(!commands.is_object() || !commands.contains("children") || !commands["children"].is_object()) return false;
    output.commands=std::move(commands);
    if(read_file((root/"reports"/"registries.json").u8string(),raw,64*1024*1024)) {
        const auto reg=nlohmann::json::parse(raw,nullptr,false);
        if(reg.is_object() && reg.contains("minecraft:block") && reg["minecraft:block"].is_object()) {
            const auto &block=reg["minecraft:block"];
            if(block.contains("entries") && block["entries"].is_object()) {
                for(auto it=block["entries"].begin();it!=block["entries"].end();++it) output.blocks.push_back(it.key());
            }
        }
    }
    return true;
}
struct Completion {std::mutex lock;std::condition_variable signal;bool done=false;int code=-1;};
}
bool VersionReport::matches(const std::string &path) const
{
    if(!error.empty() || source!=path || !commands.is_object()) return false;
    std::error_code ec;
    const fs::path p=fs::u8path(path);
    if(fs::file_size(p,ec)!=bytes || ec) return false;
    return fs::last_write_time(p,ec)==modified && !ec;
}
VersionReport build_version_report(const std::string &jar,const std::string &java,const std::string &cache)
{
    VersionReport report;report.source=jar;
    std::error_code ec;const auto src=fs::absolute(fs::u8path(jar),ec);
    report.bytes=fs::file_size(src,ec);
    if(ec || !report.bytes || report.bytes>1024ULL*1024*1024){report.error="The selected server JAR is unavailable or exceeds 1 GiB.";return report;}
    report.modified=fs::last_write_time(src,ec);
    std::ifstream in(src,std::ios::binary);
    if(!in || ec){report.error="Could not read the selected server JAR.";return report;}
    Sha1 hash;char buffer[65536];
    while(in){in.read(buffer,sizeof(buffer));hash.update(buffer,std::size_t(in.gcount()));}
    if(!in.eof()){report.error="Could not finish reading the selected server JAR.";return report;}
    report.sha1=hash.hex();
    const auto root=fs::u8path(cache), final=root/report.sha1;
    if(read_reports(final,report)) return report;
    if(java.empty()){report.error="No compatible Java runtime is available to generate this version's report.";return report;}
    fs::create_directories(root,ec);
    const auto stage=root/("prepare-"+std::to_string(std::random_device{}()));
    if(ec || !fs::create_directory(stage,ec)){report.error="Could not create a new isolated report directory.";return report;}
    // Keep only machine-generated data in the cache. Version-specific libraries stay in the unique staging directory.
    ServerProcess proc;auto completion=std::make_shared<Completion>();std::string error;
    const auto out=stage/"output";
    if(!proc.start({java,"-DbundlerMainClass=net.minecraft.data.Main","-jar",src.u8string(),"--reports","--output",out.u8string()},
            stage.u8string(),[](const std::string &){},[completion](int code){
                std::lock_guard<std::mutex> lock(completion->lock);
                completion->code=code;completion->done=true;completion->signal.notify_all();
            },&error)) report.error=error;
    else {
        std::unique_lock<std::mutex> lock(completion->lock);
        if(!completion->signal.wait_for(lock,std::chrono::minutes(3),[&]{return completion->done;})) {
            proc.kill();
            completion->signal.wait_for(lock,std::chrono::seconds(10),[&]{return completion->done;});
            report.error="The version report generator exceeded its time limit.";
        } else if(completion->code!=0) report.error="The JAR data generator exited with code "+std::to_string(completion->code)+".";
        else if(!read_reports(out,report)) report.error="The selected distribution did not produce the expected commands report.";
        else if(!report.matches(jar)) report.error="The selected server JAR changed during report generation.";
        else {
            fs::rename(out,final,ec);
            if(ec && !fs::is_regular_file(final/"reports"/"commands.json")) report.error="The generated report could not be promoted.";
        }
    }
    if(!proc.running()) fs::remove_all(stage,ec);
    return report;
}
}}}
