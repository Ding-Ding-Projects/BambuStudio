#include "SupportTickets.hpp"
#include <algorithm>
#include <set>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace Slic3r::LocalSecurity {
namespace {
constexpr const char* account="bambustudio.local-support.key.v1";
void require(bool ok,Error e=Error::InvalidInput){if(!ok)throw Failure(e);}
}
std::vector<SupportTicket> SupportTickets::list(){
    if(!std::filesystem::exists(m_file))return {};
    require(std::filesystem::is_regular_file(m_file)&&!std::filesystem::is_symlink(m_file)&&std::filesystem::file_size(m_file)<=4096,Error::Corrupt);
    auto key=m_vault.read(account);require(key.has_value(),Error::Missing);
    std::ifstream input(m_file,std::ios::binary);require(bool(input),Error::Unavailable);
    std::vector<unsigned char> encrypted(4097);input.read(reinterpret_cast<char*>(encrypted.data()),static_cast<std::streamsize>(encrypted.size()));
    require(input.eof()&&input.gcount()<=4096,Error::Corrupt);encrypted.resize(static_cast<std::size_t>(input.gcount()));
    auto plain=decrypt_snapshot(*key,encrypted,"support-tickets-v1");auto* value=&plain;
    require(value->size()>=2&&value->data()[0]==1&&value->data()[1]<=8,Error::Corrupt);
    std::vector<SupportTicket> out;std::size_t at=2;std::set<std::string> seen;
    for(unsigned i=0;i<value->data()[1];++i){
        require(at+37<=value->size(),Error::Corrupt);auto p=value->data()+at;
        SupportTicket t{std::string(reinterpret_cast<const char*>(p),32),static_cast<TicketCategory>(p[32]),static_cast<TicketStage>(p[33]),p[34],{}};
        auto length=std::size_t(p[35])*256+p[36];at+=37;
        require(valid_stable_id(t.id)&&seen.insert(t.id).second&&p[32]<=2&&p[33]<=2&&t.severity>=1&&t.severity<=5&&length<=160&&at+length<=value->size(),Error::Corrupt);
        t.description=std::string(reinterpret_cast<const char*>(value->data()+at),length);at+=length;out.push_back(std::move(t));
    }
    require(at==value->size(),Error::Corrupt);return out;
}
void SupportTickets::save(const std::vector<SupportTicket>& tickets){
    require(tickets.size()<=8);std::vector<unsigned char> bytes{1,static_cast<unsigned char>(tickets.size())};
    for(const auto& t:tickets){require(t.id.size()==32&&valid_stable_id(t.id)&&t.description.size()<=160);bytes.insert(bytes.end(),t.id.begin(),t.id.end());bytes.push_back(static_cast<unsigned char>(t.category));bytes.push_back(static_cast<unsigned char>(t.stage));bytes.push_back(static_cast<unsigned char>(t.severity));bytes.push_back(static_cast<unsigned char>(t.description.size()>>8));bytes.push_back(static_cast<unsigned char>(t.description.size()));bytes.insert(bytes.end(),t.description.begin(),t.description.end());}
    auto key=m_vault.read(account);
    if(!key){require(!std::filesystem::exists(m_file),Error::Missing);key=random_secret(32);m_vault.write(account,*key);}
    auto encrypted=encrypt_snapshot(*key,Secret(std::move(bytes)),"support-tickets-v1");
    std::filesystem::create_directories(m_file.parent_path());
    require(!std::filesystem::is_symlink(m_file.parent_path()),Error::Unavailable);
    auto temporary=m_file;temporary+="."+new_stable_id()+".tmp";
    {
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);require(bool(output),Error::Unavailable);
        output.write(reinterpret_cast<const char*>(encrypted.data()),static_cast<std::streamsize>(encrypted.size()));output.flush();require(bool(output),Error::Unavailable);
    }
#ifdef _WIN32
    require(MoveFileExW(temporary.c_str(),m_file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,Error::Unavailable);
#else
    std::filesystem::rename(temporary,m_file);
#endif
}
std::string SupportTickets::create(TicketCategory category,unsigned severity,std::string description){
    require(static_cast<unsigned>(category)<=2&&severity>=1&&severity<=5&&!description.empty()&&description.size()<=160);
    for(unsigned char c:description)require(c>=32&&c!=127);
    auto tickets=list();require(tickets.size()<8);auto id=new_stable_id();tickets.push_back({id,category,TicketStage::Created,severity,std::move(description)});save(tickets);return id;
}
void SupportTickets::advance(const std::string& id){auto tickets=list();auto t=std::find_if(tickets.begin(),tickets.end(),[&](const SupportTicket& t){return t.id==id;});require(t!=tickets.end(),Error::Missing);if(t->stage==TicketStage::Resolution)return;t->stage=static_cast<TicketStage>(static_cast<unsigned>(t->stage)+1);save(tickets);}
void SupportTickets::remove(const std::string& id){auto tickets=list();auto t=std::find_if(tickets.begin(),tickets.end(),[&](const SupportTicket& t){return t.id==id;});require(t!=tickets.end(),Error::Missing);tickets.erase(t);save(tickets);}
std::string SupportTickets::export_text(){std::string out="Local fictional support tickets. Nothing was sent.\n";for(const auto& t:list())out+=t.id+" | category "+std::to_string(static_cast<unsigned>(t.category))+" | stage "+std::to_string(static_cast<unsigned>(t.stage))+" | severity "+std::to_string(t.severity)+" | "+t.description+"\n";return out;}
}
