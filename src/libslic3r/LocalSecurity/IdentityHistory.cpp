#include "IdentityHistory.hpp"
#include <git2.h>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <sstream>
#include <locale>
#include <map>
#include <set>
#include <fstream>
#include <openssl/hmac.h>
#include <openssl/crypto.h>
#include "slic3r/GUI/Widgets/SuperConfirmState.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Slic3r::LocalSecurity {
namespace {
constexpr const char* history_ref = "refs/heads/identity-history";
constexpr const char* empty_anchor = "empty-v1";
constexpr std::size_t maximum_snapshot = 1024 * 1024;
std::mutex history_mutex;
void require(bool b, Error e = Error::History) { if (!b) throw Failure(e); }
void ok(int code) { require(code >= 0); }
template<class T, void (*F)(T*)> using Owner = std::unique_ptr<T, decltype(F)>;
using Repo = Owner<git_repository, git_repository_free>;
using Commit = Owner<git_commit, git_commit_free>;
using Tree = Owner<git_tree, git_tree_free>;
using Blob = Owner<git_blob, git_blob_free>;
std::string oid_text(const git_oid* id) { char text[GIT_OID_HEXSZ+1]{}; git_oid_tostr(text, sizeof(text), id); return text; }
void bounded_object(git_repository* repo,const git_oid* id,git_object_t expected,std::size_t limit) {
    git_odb* raw=nullptr;ok(git_repository_odb(&raw,repo));
    Owner<git_odb,git_odb_free> odb(raw,git_odb_free);
    std::size_t size=0;git_object_t type=GIT_OBJECT_INVALID;
    ok(git_odb_read_header(&size,&type,odb.get(),id));
    require(type==expected&&size<=limit,Error::Corrupt);
}
bool equal_secret(const Secret& value, const std::string& text) {
    return value.size() == text.size() && std::equal(value.data(),value.data()+value.size(),text.begin());
}
std::string action_name(HistoryAction a) {
    switch(a) {
    case HistoryAction::Created:return "created";
    case HistoryAction::Renamed:return "renamed";
    case HistoryAction::CredentialChanged:return "credential-changed";
    case HistoryAction::Removed:return "removed";
    case HistoryAction::Restored:return "restored";
    case HistoryAction::Labelled:return "labelled";
    case HistoryAction::Pruned:return "pruned";
    }
    throw Failure(Error::InvalidInput);
}
HistoryAction parse_action(const std::string& a) {
    for(auto candidate : {HistoryAction::Created,HistoryAction::Renamed,HistoryAction::CredentialChanged,HistoryAction::Removed,HistoryAction::Restored,HistoryAction::Labelled,HistoryAction::Pruned})
        if(action_name(candidate)==a)return candidate;
    throw Failure(Error::Corrupt);
}
// Stable IDs are random hex identifiers; user-visible labels are never accepted.
bool identity_id(const std::string& id) {
    return id.size()==32 && std::all_of(id.begin(),id.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
bool valid_label(const Secret& label) {
    if(label.size()==0||label.size()>256)return false;
    const auto* data=label.data();
    for(std::size_t i=0;i<label.size();) {
        const unsigned lead=data[i++];
        if(lead<0x80){if(lead<0x20||lead==0x7f)return false;continue;}
        unsigned continuation=0,value=0,minimum=0;
        if(lead>=0xc2&&lead<=0xdf){continuation=1;value=lead&31;minimum=0x80;}
        else if(lead>=0xe0&&lead<=0xef){continuation=2;value=lead&15;minimum=0x800;}
        else if(lead>=0xf0&&lead<=0xf4){continuation=3;value=lead&7;minimum=0x10000;}
        else return false;
        if(continuation>label.size()-i)return false;
        while(continuation--){const unsigned next=data[i++];if((next&0xc0)!=0x80)return false;value=(value<<6)|(next&63);}
        if(value<minimum||value>0x10ffff||(value>=0xd800&&value<=0xdfff)||(value>=0x80&&value<=0x9f))return false;
    }
    return true;
}
class FileLock {
#ifdef _WIN32
    HANDLE m_handle=INVALID_HANDLE_VALUE;
#else
    int m_handle=-1;
#endif
public:
    explicit FileLock(const std::filesystem::path& path) {
#ifdef _WIN32
        m_handle=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OPEN_REPARSE_POINT,nullptr);
        require(m_handle!=INVALID_HANDLE_VALUE,Error::Unavailable);
        BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(m_handle,&info)||(info.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)) {
            CloseHandle(m_handle);m_handle=INVALID_HANDLE_VALUE;throw Failure(Error::Unavailable);
        }
#else
        m_handle=::open(path.c_str(),O_CREAT|O_RDWR|O_NOFOLLOW,0600);
        require(m_handle>=0,Error::Unavailable);
        if(flock(m_handle,LOCK_EX|LOCK_NB)!=0){::close(m_handle);m_handle=-1;throw Failure(Error::Unavailable);}
#endif
    }
    ~FileLock(){
#ifdef _WIN32
        if(m_handle!=INVALID_HANDLE_VALUE)CloseHandle(m_handle);
#else
        if(m_handle>=0){flock(m_handle,LOCK_UN);::close(m_handle);}
#endif
    }
};
void safe_directory(const std::filesystem::path& path) {
    require(path.is_absolute(),Error::InvalidInput);
    std::filesystem::path current;
    for(const auto& part:path) {
        current/=part;
        std::error_code ec;
        const auto status=std::filesystem::symlink_status(current,ec);
        require(!ec && std::filesystem::is_directory(status) && !std::filesystem::is_symlink(status),Error::Unavailable);
#ifdef _WIN32
        const auto attrs=GetFileAttributesW(current.c_str());
        require(attrs!=INVALID_FILE_ATTRIBUTES && !(attrs&FILE_ATTRIBUTE_REPARSE_POINT),Error::Unavailable);
#endif
    }
}

constexpr std::size_t maximum_keys=512, maximum_tombstones=4096, maximum_store=196608;
constexpr const char* store_binding="identity-history-keys-v2";
struct EventHeader {
    std::string binding, identity, event;
    HistoryAction action;
    unsigned version;
};
EventHeader event_header(const git_commit* commit) {
    const char* raw=git_commit_message(commit);require(raw!=nullptr,Error::Corrupt);
    const std::string message(raw);require(message.size()<=160,Error::Corrupt);
    require(message.rfind("v1.",0)==0||message.rfind("v2.",0)==0,Error::Corrupt);
    const auto first=message.find('.',3),second=message.find('.',first==std::string::npos?message.size():first+1);
    require(first!=std::string::npos&&second!=std::string::npos,Error::Corrupt);
    EventHeader result{message,message.substr(first+1,second-first-1),message.substr(second+1),
                       parse_action(message.substr(3,first-3)),message[1]=='1'?1u:2u};
    require(identity_id(result.identity)&&identity_id(result.event),Error::Corrupt);
    require(result.version==2||result.action!=HistoryAction::Pruned,Error::Corrupt);
    return result;
}
struct KeyStore {
    std::string head;
    std::map<std::string,Secret> keys;
    std::set<std::string> tombstones;
};
Secret store_mac(const Secret& master,const std::vector<unsigned char>& bytes) {
    Secret result(std::vector<unsigned char>(32));unsigned length=0;
    require(HMAC(EVP_sha256(),master.data(),static_cast<int>(master.size()),bytes.data(),bytes.size(),result.data(),&length)!=nullptr&&length==32,Error::Unavailable);
    return result;
}
void regular_file(const std::filesystem::path& path) {
    std::error_code ec;const auto status=std::filesystem::symlink_status(path,ec);
    require(!ec&&std::filesystem::is_regular_file(status)&&!std::filesystem::is_symlink(status),Error::Corrupt);
#ifdef _WIN32
    const auto attrs=GetFileAttributesW(path.c_str());
    require(attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_REPARSE_POINT),Error::Corrupt);
#endif
}
std::vector<unsigned char> read_store_file(const std::filesystem::path& path) {
    regular_file(path);
    const auto size=std::filesystem::file_size(path);
    require(size>=29&&size<=maximum_store,Error::Corrupt);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    std::ifstream input(path,std::ios::binary);require(static_cast<bool>(input),Error::Unavailable);
    input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    require(input.gcount()==static_cast<std::streamsize>(bytes.size())&&input.peek()==std::char_traits<char>::eof(),Error::Corrupt);
    return bytes;
}
void atomic_store_file(const std::filesystem::path& path,const std::vector<unsigned char>& bytes) {
    require(bytes.size()<=maximum_store,Error::InvalidInput);
    const auto pending=std::filesystem::path(path.native()+std::filesystem::path(".pending").native());
    require(!std::filesystem::exists(pending),Error::History);
    if(std::filesystem::exists(path))regular_file(path);
#ifdef _WIN32
    HANDLE file=CreateFileW(pending.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    require(file!=INVALID_HANDLE_VALUE,Error::Unavailable);
    DWORD written=0;
    const bool saved=WriteFile(file,bytes.data(),static_cast<DWORD>(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file);
    CloseHandle(file);require(saved,Error::Unavailable);
    require(MoveFileExW(pending.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0,Error::Unavailable);
#else
    int file=::open(pending.c_str(),O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW,0600);
    require(file>=0,Error::Unavailable);
    std::size_t written=0;bool saved=true;
    while(written<bytes.size()) { const auto n=::write(file,bytes.data()+written,bytes.size()-written);if(n<=0){saved=false;break;}written+=static_cast<std::size_t>(n); }
    if(saved)saved=::fsync(file)==0;::close(file);require(saved,Error::Unavailable);
    require(::rename(pending.c_str(),path.c_str())==0,Error::Unavailable);
    int directory=::open(path.parent_path().c_str(),O_RDONLY|O_DIRECTORY);
    require(directory>=0,Error::Unavailable);const bool synced=::fsync(directory)==0;::close(directory);require(synced,Error::Unavailable);
#endif
}
Secret serialize_store(const KeyStore& store) {
    require(store.keys.size()<=maximum_keys&&store.tombstones.size()<=maximum_tombstones,Error::History);
    require(store.head.empty()||store.head.size()==GIT_OID_HEXSZ,Error::Corrupt);
    // Allocate once in the cleansing buffer: vector growth must not leave
    // retired allocations containing copies of event keys.
    Secret out(std::vector<unsigned char>(10+store.head.size()+store.keys.size()*64+store.tombstones.size()*32));
    auto* bytes=out.data();std::memcpy(bytes,"IHKS",4);bytes[4]=2;bytes[5]=static_cast<unsigned char>(store.head.size());
    std::size_t position=6;
    if(!store.head.empty())std::memcpy(bytes+position,store.head.data(),store.head.size());position+=store.head.size();
    auto put_count=[&](std::size_t count){bytes[position++]=static_cast<unsigned char>(count>>8);bytes[position++]=static_cast<unsigned char>(count);};
    put_count(store.keys.size());put_count(store.tombstones.size());
    for(const auto& item:store.keys) {
        require(identity_id(item.first)&&item.second.size()==32&&store.tombstones.count(item.first)==0,Error::Corrupt);
        std::memcpy(bytes+position,item.first.data(),32);position+=32;
        std::memcpy(bytes+position,item.second.data(),32);position+=32;
    }
    for(const auto& id:store.tombstones){require(identity_id(id),Error::Corrupt);std::memcpy(bytes+position,id.data(),32);position+=32;}
    require(position==out.size(),Error::Corrupt);return out;
}
KeyStore deserialize_store(const Secret& plain) {
    require(plain.size()>=10&&std::memcmp(plain.data(),"IHKS",4)==0&&plain.data()[4]==2,Error::Corrupt);
    const unsigned head_length=plain.data()[5];require(head_length==0||head_length==GIT_OID_HEXSZ,Error::Corrupt);
    require(plain.size()>=10+head_length,Error::Corrupt);std::size_t position=6;
    KeyStore result;result.head.assign(reinterpret_cast<const char*>(plain.data()+position),head_length);position+=head_length;
    require(result.head.empty()||std::all_of(result.head.begin(),result.head.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),Error::Corrupt);
    auto count=[&](){const auto n=unsigned(plain.data()[position])*256+plain.data()[position+1];position+=2;return n;};
    const auto keys=count(),tombstones=count();require(keys<=maximum_keys&&tombstones<=maximum_tombstones,Error::Corrupt);
    require(plain.size()==position+keys*64+tombstones*32,Error::Corrupt);
    for(unsigned index=0;index<keys;++index) {
        std::string id(reinterpret_cast<const char*>(plain.data()+position),32);position+=32;require(identity_id(id),Error::Corrupt);
        Secret key(std::vector<unsigned char>(plain.data()+position,plain.data()+position+32));position+=32;
        require(result.keys.emplace(id,std::move(key)).second,Error::Corrupt);
    }
    for(unsigned index=0;index<tombstones;++index) {
        std::string id(reinterpret_cast<const char*>(plain.data()+position),32);position+=32;
        require(identity_id(id)&&result.keys.count(id)==0&&result.tombstones.insert(id).second,Error::Corrupt);
    }
    return result;
}
IdentityHistoryMetadata metadata_row(const git_commit* commit,const EventHeader& header,const KeyStore& store) {
    const auto offset=git_commit_time_offset(commit);require(offset>=-1440&&offset<=1440,Error::Corrupt);
    const bool pruned=header.version==2&&store.tombstones.count(header.event)!=0;
    if(header.version==2)require(pruned||store.keys.count(header.event)==1,Error::Missing);
    return {oid_text(git_commit_id(commit)),header.identity,header.action,
            static_cast<std::int64_t>(git_commit_time(commit)),offset,header.version,pruned};
}
}
struct IdentityHistory::Impl {
    std::filesystem::path data, path;
    Vault& vault;
    AttemptBudget attempts;
    const std::string owner=new_stable_id();
    explicit Impl(std::filesystem::path root,Vault& v):data(std::move(root)),path(data/"identity-history-v1.git"),vault(v) {
        require(data.is_absolute(),Error::InvalidInput); ok(git_libgit2_init());
    }
    ~Impl(){git_libgit2_shutdown();}
    Repo open() {
        safe_directory(path);
        git_repository* raw=nullptr;
        ok(git_repository_open_ext(&raw,path.u8string().c_str(),GIT_REPOSITORY_OPEN_NO_SEARCH|GIT_REPOSITORY_OPEN_BARE,nullptr));
        Repo repo(raw,git_repository_free);require(git_repository_is_bare(repo.get())==1);
        git_strarray remotes{};ok(git_remote_list(&remotes,repo.get()));
        const bool isolated=remotes.count==0;git_strarray_dispose(&remotes);require(isolated);
        return repo;
    }
    Secret key() {
        auto value=vault.read(key_account);require(value.has_value(),Error::Missing);
        require(value->size()==32,Error::Corrupt);return std::move(*value);
    }
    Commit head(git_repository* repo) {
        auto anchor=vault.read(anchor_account);require(anchor.has_value(),Error::Missing);
        git_oid id{};const int rc=git_reference_name_to_id(&id,repo,history_ref);
        if(rc==GIT_ENOTFOUND){require(equal_secret(*anchor,empty_anchor));return Commit(nullptr,git_commit_free);}
        ok(rc);require(equal_secret(*anchor,oid_text(&id)),Error::Corrupt);
        bounded_object(repo,&id,GIT_OBJECT_COMMIT,8192);
        git_commit* raw=nullptr;ok(git_commit_lookup(&raw,repo,&id));return Commit(raw,git_commit_free);
    }
    std::string head_text(const Commit& current) { return current?oid_text(git_commit_id(current.get())):std::string(); }
    KeyStore load_store(const Secret& master,const Commit& current) {
        const auto file=data/"identity-history-keys-v2.enc";
        require(!std::filesystem::exists(data/"identity-history-keys-v2.enc.pending"),Error::History);
        const auto anchor=vault.read(key_store_anchor_account);
        const bool exists=std::filesystem::exists(file);
        if(!anchor&&!exists) {
            require(!current||event_header(current.get()).version==1,Error::Missing);
            KeyStore empty;empty.head=head_text(current);return empty;
        }
        require(anchor.has_value()&&exists,Error::Missing);
        auto encrypted=read_store_file(file);auto mac=store_mac(master,encrypted);
        require(anchor->size()==mac.size()&&CRYPTO_memcmp(anchor->data(),mac.data(),mac.size())==0,Error::Corrupt);
        auto plain=decrypt_snapshot(master,encrypted,store_binding);auto store=deserialize_store(plain);
        require(store.head==head_text(current),Error::Corrupt);return store;
    }
    void save_store(const Secret& master,const KeyStore& store) {
        auto plain=serialize_store(store);auto encrypted=encrypt_snapshot(master,plain,store_binding);
        auto mac=store_mac(master,encrypted);
        atomic_store_file(data/"identity-history-keys-v2.enc",encrypted);
        vault.write(key_store_anchor_account,mac);
    }
    std::vector<std::pair<IdentityHistoryMetadata,std::string>> selected(git_repository* repo,const Commit& head,const KeyStore& store,const std::vector<std::string>& revisions) {
        require(!revisions.empty()&&revisions.size()<=16,Error::InvalidInput);
        std::set<std::string> remaining;
        for(const auto& id:revisions) {
            require(id.size()==GIT_OID_HEXSZ&&std::all_of(id.begin(),id.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),Error::InvalidInput);
            require(remaining.insert(id).second,Error::InvalidInput);
        }
        Commit current(nullptr,git_commit_free);
        if(head){git_commit* copy=nullptr;ok(git_commit_dup(&copy,head.get()));current.reset(copy);}
        std::vector<std::pair<IdentityHistoryMetadata,std::string>> result;
        for(unsigned index=0;current&&index<10000&&!remaining.empty();++index) {
            require(git_commit_parentcount(current.get())<=1,Error::Corrupt);
            const auto revision=oid_text(git_commit_id(current.get()));
            if(remaining.erase(revision)) {
                const auto header=event_header(current.get());const auto row=metadata_row(current.get(),header,store);
                require(row.format_version==2&&!row.pruned&&row.action!=HistoryAction::Pruned,Error::InvalidInput);
                result.emplace_back(row,header.event);
            }
            if(git_commit_parentcount(current.get())==0)break;
            bounded_object(repo,git_commit_parent_id(current.get(),0),GIT_OBJECT_COMMIT,8192);
            git_commit* parent=nullptr;ok(git_commit_parent(&parent,current.get(),0));current.reset(parent);
        }
        require(remaining.empty(),Error::Missing);return result;
    }
    std::string write_event(git_repository* repo,const Commit& parent,const Secret& master,KeyStore& store,
                            HistoryAction action,const std::string& identity,const Secret& snapshot) {
        require(store.keys.size()<maximum_keys,Error::History);
        const auto event=new_stable_id();require(store.keys.count(event)==0&&store.tombstones.count(event)==0,Error::History);
        const auto metadata="v2."+action_name(action)+"."+identity+"."+event;
        auto event_key=random_secret(32);const auto encrypted=encrypt_snapshot(event_key,snapshot,metadata);
        store.keys.emplace(event,std::move(event_key));
        git_oid blob_id{},tree_id{},commit_id{};ok(git_blob_create_frombuffer(&blob_id,repo,encrypted.data(),encrypted.size()));
        git_treebuilder* raw_builder=nullptr;ok(git_treebuilder_new(&raw_builder,repo,nullptr));
        Owner<git_treebuilder,git_treebuilder_free> builder(raw_builder,git_treebuilder_free);
        ok(git_treebuilder_insert(nullptr,builder.get(),"snapshot.enc",&blob_id,GIT_FILEMODE_BLOB));ok(git_treebuilder_write(&tree_id,builder.get()));
        git_tree* raw_tree=nullptr;ok(git_tree_lookup(&raw_tree,repo,&tree_id));Tree tree(raw_tree,git_tree_free);
        git_signature* raw_signature=nullptr;ok(git_signature_now(&raw_signature,"Local identity history","local@invalid"));
        Owner<git_signature,git_signature_free> signature(raw_signature,git_signature_free);
        const git_commit* parents[]={parent.get()};
        ok(git_commit_create(&commit_id,repo,nullptr,signature.get(),signature.get(),nullptr,metadata.c_str(),tree.get(),parent?1:0,parents));
        store.head=oid_text(&commit_id);
        // The store binds the next head before publication. Every interrupted
        // boundary is detected by the store anchor, head anchor, or head binding.
        save_store(master,store);
        git_reference* raw_reference=nullptr;
        if(parent)ok(git_reference_create_matching(&raw_reference,repo,history_ref,&commit_id,1,git_commit_id(parent.get()),"identity event"));
        else ok(git_reference_create(&raw_reference,repo,history_ref,&commit_id,0,"identity event"));
        Owner<git_reference,git_reference_free> reference(raw_reference,git_reference_free);
        vault.write(anchor_account,Secret(store.head));return store.head;
    }
    void authenticate(const Secret& answer) {
        const auto now=std::chrono::steady_clock::now();
        require(attempts.state(now).wait_seconds==0,Error::RateLimited);
        Credentials credentials(vault);
        if(!credentials.verify(credential_account,answer)){attempts.failed(now);throw Failure(Error::Authentication);}
        attempts.succeeded();
    }
};
IdentityHistory::IdentityHistory(std::filesystem::path root,Vault& vault):m_impl(std::make_unique<Impl>(std::move(root),vault)){}
IdentityHistory::~IdentityHistory()=default;
void IdentityHistory::initialize(CredentialKind kind,const Secret& answer) {
    std::lock_guard<std::mutex> guard(history_mutex);safe_directory(m_impl->data);
    FileLock lock(m_impl->data/"identity-history-v1.lock");
    require(!std::filesystem::exists(m_impl->path),Error::History);
    require(!m_impl->vault.read(key_account)&&!m_impl->vault.read(anchor_account)&&!m_impl->vault.read(credential_account)&&!m_impl->vault.read(key_store_anchor_account),Error::History);
    require(!std::filesystem::exists(m_impl->data/"identity-history-keys-v2.enc")&&!std::filesystem::exists(m_impl->data/"identity-history-keys-v2.enc.pending"),Error::History);
    // Any interruption leaves explicit partial state, which is not auto-reset.
    Credentials credentials(m_impl->vault);credentials.enroll(credential_account,kind,answer);
    auto key=random_secret(32);m_impl->vault.write(key_account,key);
    git_repository* raw=nullptr;ok(git_repository_init(&raw,m_impl->path.u8string().c_str(),1));
    Repo repo(raw,git_repository_free);m_impl->vault.write(anchor_account,Secret(empty_anchor));
}
std::string IdentityHistory::append(HistoryAction action,const std::string& identity,const Secret& snapshot) {
    return append_impl(action,identity,snapshot,nullptr);
}
std::string IdentityHistory::append_impl(HistoryAction action,const std::string& identity,const Secret& snapshot,const Secret* authorization) {
    require(identity_id(identity)&&snapshot.size()<=maximum_snapshot&&action!=HistoryAction::Pruned,Error::InvalidInput);
    if(action==HistoryAction::Labelled)require(valid_label(snapshot),Error::InvalidInput);
    action_name(action);
    std::lock_guard<std::mutex> guard(history_mutex);safe_directory(m_impl->data);
    FileLock lock(m_impl->data/"identity-history-v1.lock");
    if(authorization)m_impl->authenticate(*authorization);
    auto repo=m_impl->open();auto master=m_impl->key();auto parent=m_impl->head(repo.get());
    require(Credentials(m_impl->vault).metadata(credential_account).configured,Error::Missing);
    auto store=m_impl->load_store(master,parent);
    return m_impl->write_event(repo.get(),parent,master,store,action,identity,snapshot);
}
std::vector<IdentityHistoryEntry> IdentityHistory::read(const Secret& answer,unsigned offset,unsigned count) {
    return read_impl(answer,offset,count,true);
}
std::vector<IdentityHistoryEntry> IdentityHistory::read_impl(const Secret& answer,unsigned offset,unsigned count,bool decrypt_payload,const std::string& revision) {
    require(count>0&&count<=16&&offset<=10000,Error::InvalidInput);
    std::lock_guard<std::mutex> guard(history_mutex);safe_directory(m_impl->data);
    FileLock lock(m_impl->data/"identity-history-v1.lock");m_impl->authenticate(answer);
    auto repo=m_impl->open();auto key=m_impl->key();auto current=m_impl->head(repo.get());
    auto store=m_impl->load_store(key,current);
    std::vector<IdentityHistoryEntry> output;
    for(unsigned index=0;current&&index<(revision.empty()?offset+count:10000u);++index) {
        require(git_commit_parentcount(current.get())<=1,Error::Corrupt);
        if(index>=offset&&(revision.empty()||oid_text(git_commit_id(current.get()))==revision)) {
            const auto header=event_header(current.get());
            const auto row=metadata_row(current.get(),header,store);
            bounded_object(repo.get(),git_commit_tree_id(current.get()),GIT_OBJECT_TREE,1024);
            git_tree* raw_tree=nullptr;ok(git_commit_tree(&raw_tree,current.get()));Tree tree(raw_tree,git_tree_free);
            require(git_tree_entrycount(tree.get())==1,Error::Corrupt);
            const auto* entry=git_tree_entry_byname(tree.get(),"snapshot.enc");
            require(entry&&git_tree_entry_type(entry)==GIT_OBJECT_BLOB&&git_tree_entry_filemode(entry)==GIT_FILEMODE_BLOB,Error::Corrupt);
            // Check object header before materializing a possibly hostile blob.
            git_odb* raw_odb=nullptr;ok(git_repository_odb(&raw_odb,repo.get()));Owner<git_odb,git_odb_free> odb(raw_odb,git_odb_free);
            std::size_t size=0;git_object_t type=GIT_OBJECT_INVALID;ok(git_odb_read_header(&size,&type,odb.get(),git_tree_entry_id(entry)));
            require(type==GIT_OBJECT_BLOB&&size>=29&&size<=maximum_snapshot+29,Error::Corrupt);
            git_blob* raw_blob=nullptr;ok(git_blob_lookup(&raw_blob,repo.get(),git_tree_entry_id(entry)));Blob blob(raw_blob,git_blob_free);
            require(git_blob_rawsize(blob.get())==size,Error::Corrupt);
            const auto* bytes=static_cast<const unsigned char*>(git_blob_rawcontent(blob.get()));
            Secret plain;
            if(decrypt_payload&&!row.pruned) {
                const Secret& payload_key=header.version==1?key:store.keys.at(header.event);
                plain=decrypt_snapshot(payload_key,std::vector<unsigned char>(bytes,bytes+size),header.binding);
                if(header.action==HistoryAction::Labelled)require(valid_label(plain),Error::Corrupt);
            }
            output.push_back({row.revision,row.identity,row.action,std::move(plain),
                              row.committed_at_utc_seconds,row.utc_offset_minutes,row.format_version,row.pruned});
            if(!revision.empty())break;
        }
        if(git_commit_parentcount(current.get())==0)break;
        bounded_object(repo.get(),git_commit_parent_id(current.get(),0),GIT_OBJECT_COMMIT,8192);
        git_commit* raw_parent=nullptr;ok(git_commit_parent(&raw_parent,current.get(),0));current.reset(raw_parent);
    }
    return output;
}
IdentityHistoryEntry IdentityHistory::read_revision(const Secret& answer,const std::string& revision) {
    require(revision.size()==GIT_OID_HEXSZ&&std::all_of(revision.begin(),revision.end(),[](char c){
        return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),Error::InvalidInput);
    auto entries=read_impl(answer,0,1,true,revision);
    require(entries.size()==1&&!entries.front().pruned,Error::Missing);
    return std::move(entries.front());
}
std::vector<IdentityHistoryMetadata> IdentityHistory::read_metadata(const Secret& answer,unsigned offset,unsigned count) {
    auto entries=read_impl(answer,offset,count,false);
    std::vector<IdentityHistoryMetadata> result;
    for(const auto& entry:entries)result.push_back({entry.revision,entry.identity,entry.action,
        entry.committed_at_utc_seconds,entry.utc_offset_minutes,entry.format_version,entry.pruned});
    return result;
}
std::string IdentityHistory::append_label(const Secret& answer,const std::string& identity,const Secret& label) {
    require(valid_label(label),Error::InvalidInput);
    return append_impl(HistoryAction::Labelled,identity,label,&answer);
}
Secret IdentityHistory::read_label(const Secret& answer,const std::string& revision) {
    require(revision.size()==GIT_OID_HEXSZ&&std::all_of(revision.begin(),revision.end(),[](char c){
        return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),Error::InvalidInput);
    auto entries=read_impl(answer,0,1,true,revision);
    require(entries.size()==1&&entries.front().action==HistoryAction::Labelled&&!entries.front().pruned,Error::Missing);
    return std::move(entries.front().snapshot);
}
IdentityHistoryRedactedDiff IdentityHistory::redacted_diff(const IdentityHistoryMetadata& before,const IdentityHistoryMetadata& after) {
    return {before.identity==after.identity,before.action!=after.action};
}
std::string IdentityHistory::export_redacted(const Secret& answer,unsigned offset,unsigned count) {
    const auto entries=read_metadata(answer,offset,count);
    std::ostringstream out;out.imbue(std::locale::classic());out<<"{\"schema\":1,\"offset\":"<<offset<<",\"events\":[";
    bool first=true;
    for(const auto& entry:entries) {
        if(!first)out<<',';first=false;
        // Every string is generated hexadecimal or an allowlisted action.
        out<<"{\"revision\":\""<<entry.revision<<"\",\"identity\":\""<<entry.identity
           <<"\",\"action\":\""<<action_name(entry.action)<<"\",\"committed_at_utc_seconds\":"
           <<entry.committed_at_utc_seconds<<",\"utc_offset_minutes\":"<<entry.utc_offset_minutes
           <<",\"format_version\":"<<entry.format_version<<",\"pruned\":"<<(entry.pruned?"true":"false")<<'}';
    }
    out<<"]}";return out.str();
}
HistoryPrunePreview IdentityHistory::preview_prune(const Secret& answer,const std::vector<std::string>& revisions) {
    std::lock_guard<std::mutex> guard(history_mutex);safe_directory(m_impl->data);
    FileLock lock(m_impl->data/"identity-history-v1.lock");m_impl->authenticate(answer);
    auto repo=m_impl->open();auto master=m_impl->key();auto current=m_impl->head(repo.get());auto store=m_impl->load_store(master,current);
    const auto chosen=m_impl->selected(repo.get(),current,store,revisions);
    require(chosen.size()+store.tombstones.size()<=maximum_tombstones,Error::History);
    HistoryPrunePreview preview;preview.m_owner=m_impl->owner;preview.m_head=m_impl->head_text(current);
    for(const auto& item:chosen)preview.m_rows.push_back(item.first);
    return preview;
}
std::string IdentityHistory::prune(const Secret& answer,const HistoryPrunePreview& preview,GUI::SuperConfirm::State& confirmation) {
    std::lock_guard<std::mutex> guard(history_mutex);safe_directory(m_impl->data);
    FileLock lock(m_impl->data/"identity-history-v1.lock");m_impl->authenticate(answer);
    auto repo=m_impl->open();auto master=m_impl->key();auto current=m_impl->head(repo.get());auto store=m_impl->load_store(master,current);
    require(preview.m_owner==m_impl->owner&&preview.m_head==m_impl->head_text(current),Error::InvalidInput);
    std::vector<std::string> revisions;for(const auto& row:preview.m_rows)revisions.push_back(row.revision);
    const auto chosen=m_impl->selected(repo.get(),current,store,revisions);
    require(chosen.size()+store.tombstones.size()<=maximum_tombstones,Error::History);
    require(confirmation.may_fire(),Error::Authentication);
    // Consume the actual native gate state, never a caller-provided boolean.
    confirmation=GUI::SuperConfirm::State{};confirmation.cancel();
    std::string audit="prune-v2\n";
    for(const auto& item:chosen) {
        require(store.keys.erase(item.second)==1,Error::Missing);store.tombstones.insert(item.second);
        audit+=item.first.revision+"\n";
    }
    return m_impl->write_event(repo.get(),current,master,store,HistoryAction::Pruned,new_stable_id(),Secret(audit));
}
IdentityHistoryCapacity IdentityHistory::capacity(const Secret& answer) {
    std::lock_guard<std::mutex> guard(history_mutex);safe_directory(m_impl->data);
    FileLock lock(m_impl->data/"identity-history-v1.lock");m_impl->authenticate(answer);
    auto repo=m_impl->open();auto master=m_impl->key();auto current=m_impl->head(repo.get());auto store=m_impl->load_store(master,current);
    return {static_cast<unsigned>(store.keys.size()),static_cast<unsigned>(store.tombstones.size()),
            static_cast<unsigned>(maximum_keys),static_cast<unsigned>(maximum_tombstones)};
}
void IdentityHistory::replace_credential(const Secret& old_answer,CredentialKind kind,const Secret& new_answer) {
    std::lock_guard<std::mutex> guard(history_mutex);safe_directory(m_impl->data);
    FileLock lock(m_impl->data/"identity-history-v1.lock");m_impl->authenticate(old_answer);
    auto repo=m_impl->open();auto key=m_impl->key();auto current=m_impl->head(repo.get());
    auto store=m_impl->load_store(key,current);
    Credentials(m_impl->vault).replace(credential_account,old_answer,kind,new_answer);
}
}
