#pragma once
#include "BedrockDistributions.hpp"
#include <ctime>
#include <map>

namespace Slic3r { namespace GUI { namespace GameServers {
struct ObservedDistribution {BedrockDistribution file;std::time_t first=0,last=0;bool current=false;};
inline std::string distribution_type(const BedrockDistribution &d){return std::string("serverBedrock")+(d.preview?"Preview":"")+(d.windows?"Windows":"Linux");}
inline std::vector<ObservedDistribution> read_bedrock_history(const std::string &raw)
{
    auto doc=nlohmann::json::parse(raw,nullptr,false);std::vector<ObservedDistribution> out;
    if(!doc.is_object() || !doc.contains("schema") || doc["schema"]!=1 || !doc.contains("entries") || !doc["entries"].is_array() || doc["entries"].size()>8192)return out;
    for(const auto &entry:doc["entries"]){
        if(!entry.is_object() || !entry.contains("first") || !entry["first"].is_number_integer() || !entry.contains("last") || !entry["last"].is_number_integer() ||
           !entry.contains("current") || !entry["current"].is_boolean())continue;
        auto rows=bedrock_distributions(nlohmann::json({{"result",{{"links",nlohmann::json::array({entry})}}}}).dump());
        if(rows.size()!=1)continue;
        auto first=entry["first"].get<std::time_t>(),last=entry["last"].get<std::time_t>();if(first<=0 || last<first)continue;
        out.push_back({std::move(rows.front()),first,last,entry["current"].get<bool>()});
    }
    return out;
}
inline std::vector<ObservedDistribution> merge_bedrock_history(const std::vector<ObservedDistribution>&prior,const std::vector<BedrockDistribution>&fresh,std::time_t now)
{
    if(fresh.empty())return prior;
    std::map<std::string,ObservedDistribution> all;
    for(auto item:prior){item.current=false;all[item.file.url]=std::move(item);}
    for(const auto &f:fresh){auto it=all.find(f.url);auto first=it==all.end()?now:std::min(now,it->second.first);all[f.url]={f,first,now,true};}
    std::vector<ObservedDistribution> out;for(auto &entry:all)out.push_back(std::move(entry.second));
    std::sort(out.begin(),out.end(),[](const auto&a,const auto&b){
        if(a.current!=b.current)return a.current>b.current;
        if(a.last!=b.last)return a.last>b.last;
        if(a.file.preview!=b.file.preview)return !a.file.preview;
        return a.file.windows>b.file.windows;
    });
    return out;
}
inline std::string write_bedrock_history(const std::vector<ObservedDistribution>&entries)
{
    auto items=nlohmann::json::array();
    for(const auto &e:entries)items.push_back({{"downloadType",distribution_type(e.file)},{"downloadUrl",e.file.url},{"first",e.first},{"last",e.last},{"current",e.current}});
    return nlohmann::json({{"schema",1},{"entries",std::move(items)}}).dump();
}
}}}
