#pragma once
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Slic3r { namespace GUI { namespace GameServers {

enum class CommandVerdict { Valid, Invalid, Unavailable };
struct CommandCheck { CommandVerdict verdict; std::string reason; };
namespace command_report_detail {
struct Word { std::string value; bool quoted=false; };

inline bool tokenize(const std::string &input,std::vector<Word> &words)
{
    std::size_t pos=0;
    while (pos<input.size()) {
        while (pos<input.size() && input[pos]==' ') ++pos;
        if(pos==input.size()) break;
        if(words.size()>=512) return false;
        Word word;
        const char quote=input[pos]=='"' || input[pos]=='\'' ? input[pos++] : '\0';
        word.quoted=quote!=0;
        bool closed=!quote;
        while(pos<input.size()) {
            const char c=input[pos++];
            if(quote && c==quote) { closed=true;break; }
            if(!quote && c==' ') break;
            if(quote && c=='\\') {
                if(pos==input.size() || (input[pos]!=quote && input[pos]!='\\')) return false;
                word.value+=input[pos++];
            } else word.value+=c;
        }
        if(!closed || (quote && pos<input.size() && input[pos]!=' ')) return false;
        words.push_back(std::move(word));
    }
    return true;
}

inline CommandVerdict consume(const nlohmann::json &node,const std::vector<Word> &words,std::size_t &pos)
{
    if(pos>=words.size() || !node.contains("parser") || !node["parser"].is_string()) return CommandVerdict::Unavailable;
    const auto parser=node["parser"].get<std::string>();
    const auto &w=words[pos++];
    const auto props=node.value("properties",nlohmann::json::object());
    if (!props.is_object()) return CommandVerdict::Unavailable;
    if(parser=="brigadier:string") {
        const auto mode=props.value("type",std::string("word"));
        if(mode=="greedy") pos=words.size();
        else if(mode=="word" && (w.quoted || w.value.empty() || w.value.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_+-.")!=std::string::npos)) return CommandVerdict::Invalid;
        else if(mode!="phrase" && mode!="word") return CommandVerdict::Unavailable;
        return CommandVerdict::Valid;
    }
    if(parser=="minecraft:message") {pos=words.size();return CommandVerdict::Valid;}
    if(w.quoted) return CommandVerdict::Unavailable;
    if(parser=="brigadier:bool") return w.value=="true" || w.value=="false" ? CommandVerdict::Valid : CommandVerdict::Invalid;
    const bool integer=parser=="brigadier:integer" || parser=="brigadier:long";
    if(integer || parser=="brigadier:float" || parser=="brigadier:double") {
        if(w.value.empty() || w.value.find_first_not_of(integer?"-0123456789":"-0123456789.")!=std::string::npos) return CommandVerdict::Invalid;
        char *end=nullptr;errno=0;
        const auto n=integer ? static_cast<long double>(std::strtoll(w.value.c_str(),&end,10)) : static_cast<long double>(std::strtod(w.value.c_str(),&end));
        if(!end || *end || end==w.value.c_str() || errno==ERANGE || !std::isfinite(n)) return CommandVerdict::Invalid;
        if(parser=="brigadier:integer" && (n<std::numeric_limits<int>::min() || n>std::numeric_limits<int>::max())) return CommandVerdict::Invalid;
        if(parser=="brigadier:float" && std::abs(n)>std::numeric_limits<float>::max()) return CommandVerdict::Invalid;
        if((props.contains("min") && n<props["min"].get<double>()) || (props.contains("max") && n>props["max"].get<double>())) return CommandVerdict::Invalid;
        return CommandVerdict::Valid;
    }
    // Coordinates, selectors, registries, SNBT and plugin arguments require their real parser.
    return CommandVerdict::Unavailable;
}

inline CommandVerdict walk(const nlohmann::json &root,const nlohmann::json &node,const std::vector<Word> &words,std::size_t pos,unsigned depth,int &budget)
{
    if(--budget<0 || depth>128) return CommandVerdict::Unavailable;
    if(pos==words.size() && node.value("executable",false)) return CommandVerdict::Valid;
    if(node.contains("redirect") && node["redirect"].is_array()) {
        const auto *dest=&root;
        for(const auto &part:node["redirect"]) {
            if(!part.is_string() || !dest->contains("children") || !(*dest)["children"].contains(part.get<std::string>())) return CommandVerdict::Unavailable;
            dest=&(*dest)["children"][part.get<std::string>()];
        }
        return walk(root,*dest,words,pos,depth+1,budget);
    }
    if(pos==words.size() || !node.contains("children") || !node["children"].is_object()) return CommandVerdict::Invalid;
    const auto &children=node["children"];
    if(!words[pos].quoted && children.contains(words[pos].value) && children[words[pos].value].value("type",std::string())=="literal")
        return walk(root,children[words[pos].value],words,pos+1,depth+1,budget);
    bool unknown=false;
    for(auto it=children.begin();it!=children.end();++it) {
        if(it->value("type",std::string())!="argument") continue;
        auto next=pos;
        auto matched=consume(*it,words,next);
        if(matched==CommandVerdict::Valid) matched=walk(root,*it,words,next,depth+1,budget);
        if(matched==CommandVerdict::Valid) return matched;
        unknown|=matched==CommandVerdict::Unavailable;
    }
    return unknown?CommandVerdict::Unavailable:CommandVerdict::Invalid;
}
}

inline CommandCheck check_command_report(const nlohmann::json &root,const std::string &line)
{
    using namespace command_report_detail;
    if(!root.is_object() || !root.contains("children") || !root["children"].is_object()) return {CommandVerdict::Unavailable,"No command tree is available."};
    std::vector<Word> words;
    // Complex server-specific arguments may contain quoting not handled by the basic parser.
    if(!tokenize(line,words) || words.empty()) return {CommandVerdict::Unavailable,"This command needs the server parser."};
    if(!root["children"].contains(words.front().value)) return {CommandVerdict::Unavailable,"The command is absent from this report; server plugins can add commands."};
    int budget=4096;
    try {
        const auto result=walk(root,root,words,0,0,budget);
        if(result==CommandVerdict::Valid) return {result,"Command matches the selected server report."};
        if(result==CommandVerdict::Invalid) return {result,"This command or argument does not match the selected server report."};
    } catch (const nlohmann::json::exception &) {}
    return {CommandVerdict::Unavailable,"This argument requires the server's own parser."};
}
}}}
