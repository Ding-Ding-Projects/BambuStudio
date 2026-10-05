#include "Authenticator.hpp"
#include <algorithm>
#include <set>

namespace Slic3r::LocalSecurity {
namespace {
constexpr const char* index_account="bambustudio.authenticator.index.v1";
constexpr std::size_t maximum_entries=32;
void require(bool ok,Error e=Error::InvalidInput){if(!ok)throw Failure(e);}
std::string key(const std::string& id){require(id.size()==32&&valid_stable_id(id));return "bambustudio.authenticator.entry."+id;}
void put(std::vector<unsigned char>& bytes,std::string_view value){
    require(value.size()<=1024);bytes.push_back(static_cast<unsigned char>(value.size()>>8));bytes.push_back(static_cast<unsigned char>(value.size()));bytes.insert(bytes.end(),value.begin(),value.end());
}
std::string get(const Secret& bytes,std::size_t& pos){
    require(pos+2<=bytes.size(),Error::Corrupt);auto n=std::size_t(bytes.data()[pos])*256+bytes.data()[pos+1];pos+=2;
    require(n<=1024&&n<=bytes.size()-pos,Error::Corrupt);std::string value(reinterpret_cast<const char*>(bytes.data()+pos),n);pos+=n;return value;
}
void label(const std::string& s,bool empty=true){require(s.size()<=256&&(empty||!s.empty()));for(unsigned char c:s)require(c>=32&&c!=127);}
Secret pack(const Enrollment& e,const std::string& group){
    label(e.issuer);label(e.account,false);label(group);require(e.secret.size()>0&&e.secret.size()<=512);
    // Parameters are validated by generation before any persistent write.
    (void)totp(e.secret,e.parameters,0);
    std::vector<unsigned char> out{1,static_cast<unsigned char>(e.parameters.algorithm),static_cast<unsigned char>(e.parameters.digits)};
    for(int i=3;i>=0;--i)out.push_back(static_cast<unsigned char>(e.parameters.period>>(i*8)));
    put(out,e.issuer);put(out,e.account);put(out,group);put(out,std::string_view(reinterpret_cast<const char*>(e.secret.data()),e.secret.size()));return Secret(std::move(out));
}
Enrollment unpack(const Secret& record,std::string* group){
    require(record.size()>=7&&record.data()[0]==1&&record.data()[1]<=2,Error::Corrupt);
    Enrollment result;result.parameters.algorithm=static_cast<Algorithm>(record.data()[1]);result.parameters.digits=record.data()[2];
    result.parameters.period=0;for(unsigned i=3;i<7;++i)result.parameters.period=(result.parameters.period<<8)|record.data()[i];
    std::size_t pos=7;result.issuer=get(record,pos);result.account=get(record,pos);auto g=get(record,pos);if(group)*group=std::move(g);
    // Read the secret directly into its wiping buffer, without a string copy.
    require(pos+2<=record.size(),Error::Corrupt);auto n=std::size_t(record.data()[pos])*256+record.data()[pos+1];pos+=2;
    require(n>0&&n<=512&&n==record.size()-pos,Error::Corrupt);result.secret=Secret(std::vector<unsigned char>(record.data()+pos,record.data()+pos+n));
    label(result.issuer);label(result.account,false);(void)totp(result.secret,result.parameters,0);return result;
}
std::string csv(std::string_view value){std::string out="\"";for(char c:value){if(c=='\"')out+='\"';out+=c;}return out+'\"';}
}
AuthenticatorStore::AuthenticatorStore(Vault& vault,RecordIdentityMutation record):m_vault(vault),m_record(std::move(record)){require(bool(m_record),Error::History);}
std::vector<std::string> AuthenticatorStore::ids(){
    auto raw=m_vault.read(index_account);if(!raw)return {};
    require(raw->size()>=2&&raw->data()[0]==1,Error::Corrupt);auto count=raw->data()[1];require(count<=maximum_entries&&raw->size()==2+std::size_t(count)*32,Error::Corrupt);
    std::vector<std::string> result;std::set<std::string> seen;
    for(unsigned i=0;i<count;++i){std::string id(reinterpret_cast<const char*>(raw->data()+2+i*32),32);require(valid_stable_id(id)&&seen.insert(id).second,Error::Corrupt);result.push_back(std::move(id));}return result;
}
void AuthenticatorStore::store_ids(const std::vector<std::string>& ids){
    require(ids.size()<=maximum_entries);std::vector<unsigned char> raw{1,static_cast<unsigned char>(ids.size())};
    for(const auto& id:ids){require(id.size()==32&&valid_stable_id(id));raw.insert(raw.end(),id.begin(),id.end());}m_vault.write(index_account,Secret(std::move(raw)));
}
Secret AuthenticatorStore::read_record(const std::string& id){auto all=ids();require(std::find(all.begin(),all.end(),id)!=all.end(),Error::Missing);auto raw=m_vault.read(key(id));require(raw.has_value(),Error::Missing);return std::move(*raw);}
std::vector<AuthenticatorEntry> AuthenticatorStore::list(){
    std::vector<AuthenticatorEntry> result;for(const auto& id:ids()){auto raw=m_vault.read(key(id));require(raw.has_value(),Error::Missing);std::string group;auto e=unpack(*raw,&group);result.push_back({id,std::move(e.issuer),std::move(e.account),std::move(group),e.parameters});}return result;
}
std::string AuthenticatorStore::add(Enrollment e,std::string_view confirmation,std::uint64_t seconds){
    require(verify_totp(e.secret,e.parameters,confirmation,seconds).has_value(),Error::Authentication);auto all=ids();require(all.size()<maximum_entries);
    auto id=new_stable_id();auto raw=pack(e,{});m_record(IdentityAction::AuthenticatorAdded,id,raw);
    m_vault.write(key(id),raw);all.push_back(id);
    try{store_ids(all);}catch(...){m_vault.erase(key(id));throw;}
    return id;
}
void AuthenticatorStore::remove(const std::string& id){
    auto raw=read_record(id);m_record(IdentityAction::AuthenticatorRemoved,id,raw);auto all=ids();all.erase(std::find(all.begin(),all.end(),id));
    // Removing the index first prevents a live entry referencing a deleted key.
    // A failed erase is surfaced; an unindexed vault record is safe to recover.
    store_ids(all);m_vault.erase(key(id));
}
void AuthenticatorStore::rename(const std::string& id,std::string issuer,std::string account,std::string group){
    auto old=read_record(id);auto e=unpack(old,nullptr);e.issuer=std::move(issuer);e.account=std::move(account);auto raw=pack(e,group);
    m_record(IdentityAction::AuthenticatorChanged,id,raw);m_vault.write(key(id),raw);
}
void AuthenticatorStore::move(const std::string& id,int direction){
    require(direction==1||direction==-1);auto all=ids();auto found=std::find(all.begin(),all.end(),id);require(found!=all.end(),Error::Missing);
    auto index=std::distance(all.begin(),found);auto next=index+direction;require(next>=0&&next<static_cast<std::ptrdiff_t>(all.size()));std::swap(all[index],all[next]);
    std::vector<unsigned char> order;for(const auto& item:all)order.insert(order.end(),item.begin(),item.end());m_record(IdentityAction::AuthenticatorReordered,id,Secret(std::move(order)));store_ids(all);
}
TotpCode AuthenticatorStore::code(const std::string& id,std::uint64_t seconds){auto raw=read_record(id);auto e=unpack(raw,nullptr);return totp(e.secret,e.parameters,seconds);}
void AuthenticatorStore::restore_entry(const std::string& id,const Secret& snapshot){
    (void)key(id);std::string group;auto enrollment=unpack(snapshot,&group);auto validated=pack(enrollment,group);auto all=ids();
    bool present=std::find(all.begin(),all.end(),id)!=all.end();require(present||all.size()<maximum_entries);
    m_record(IdentityAction::AuthenticatorRestored,id,validated);m_vault.write(key(id),validated);
    if(!present){all.push_back(id);store_ids(all);}
}
std::string AuthenticatorStore::export_redacted(){
    std::string result="Secrets and generated codes omitted.\nIssuer,Account,Group,Algorithm,Digits,Period\n";
    for(const auto& e:list()){const char* a=e.parameters.algorithm==Algorithm::Sha1?"SHA1":e.parameters.algorithm==Algorithm::Sha256?"SHA256":"SHA512";result+=csv(e.issuer)+","+csv(e.account)+","+csv(e.group)+","+a+","+std::to_string(e.parameters.digits)+","+std::to_string(e.parameters.period)+"\n";}return result;
}
}
