#include "IdentityHistory.hpp"
#include <git2.h>
#include <algorithm>
#include <cstring>
#include <mutex>
#include <sstream>
#include <locale>
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
    }
    throw Failure(Error::InvalidInput);
}
HistoryAction parse_action(const std::string& a) {
    for(auto candidate : {HistoryAction::Created,HistoryAction::Renamed,HistoryAction::CredentialChanged,HistoryAction::Removed,HistoryAction::Restored,HistoryAction::Labelled})
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
}
struct IdentityHistory::Impl {
    std::filesystem::path data, path;
    Vault& vault;
    AttemptBudget attempts;
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
    require(!m_impl->vault.read(key_account)&&!m_impl->vault.read(anchor_account)&&!m_impl->vault.read(credential_account),Error::History);
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
    require(identity_id(identity)&&snapshot.size()<=maximum_snapshot,Error::InvalidInput);
    if(action==HistoryAction::Labelled)require(valid_label(snapshot),Error::InvalidInput);
    const auto metadata="v1."+action_name(action)+"."+identity+"."+new_stable_id();
    std::lock_guard<std::mutex> guard(history_mutex);safe_directory(m_impl->data);
    FileLock lock(m_impl->data/"identity-history-v1.lock");
    if(authorization)m_impl->authenticate(*authorization);
    auto repo=m_impl->open();auto key=m_impl->key();auto parent=m_impl->head(repo.get());
    require(Credentials(m_impl->vault).metadata(credential_account).configured,Error::Missing);
    const auto encrypted=encrypt_snapshot(key,snapshot,metadata);
    git_oid blob_id{},tree_id{},commit_id{};ok(git_blob_create_frombuffer(&blob_id,repo.get(),encrypted.data(),encrypted.size()));
    git_treebuilder* raw_builder=nullptr;ok(git_treebuilder_new(&raw_builder,repo.get(),nullptr));
    Owner<git_treebuilder,git_treebuilder_free> builder(raw_builder,git_treebuilder_free);
    ok(git_treebuilder_insert(nullptr,builder.get(),"snapshot.enc",&blob_id,GIT_FILEMODE_BLOB));
    ok(git_treebuilder_write(&tree_id,builder.get()));
    git_tree* raw_tree=nullptr;ok(git_tree_lookup(&raw_tree,repo.get(),&tree_id));Tree tree(raw_tree,git_tree_free);
    git_signature* raw_signature=nullptr;ok(git_signature_now(&raw_signature,"Local identity history","local@invalid"));
    Owner<git_signature,git_signature_free> signature(raw_signature,git_signature_free);
    const git_commit* parents[]={parent.get()};
    ok(git_commit_create(&commit_id,repo.get(),nullptr,signature.get(),signature.get(),nullptr,metadata.c_str(),tree.get(),parent?1:0,parents));
    git_reference* raw_reference=nullptr;
    if(parent)ok(git_reference_create_matching(&raw_reference,repo.get(),history_ref,&commit_id,1,git_commit_id(parent.get()),"identity event"));
    else ok(git_reference_create(&raw_reference,repo.get(),history_ref,&commit_id,0,"identity event"));
    Owner<git_reference,git_reference_free> reference(raw_reference,git_reference_free);
    // The repository and native vault cannot form one atomic transaction. If
    // this write fails, the mismatch blocks subsequent reads and writes.
    const auto revision=oid_text(&commit_id);m_impl->vault.write(anchor_account,Secret(revision));return revision;
}
std::vector<IdentityHistoryEntry> IdentityHistory::read(const Secret& answer,unsigned offset,unsigned count) {
    return read_impl(answer,offset,count,true);
}
std::vector<IdentityHistoryEntry> IdentityHistory::read_impl(const Secret& answer,unsigned offset,unsigned count,bool decrypt_payload,const std::string& revision) {
    require(count>0&&count<=16&&offset<=10000,Error::InvalidInput);
    std::lock_guard<std::mutex> guard(history_mutex);safe_directory(m_impl->data);
    FileLock lock(m_impl->data/"identity-history-v1.lock");m_impl->authenticate(answer);
    auto repo=m_impl->open();auto key=m_impl->key();auto current=m_impl->head(repo.get());
    std::vector<IdentityHistoryEntry> output;
    for(unsigned index=0;current&&index<(revision.empty()?offset+count:10000u);++index) {
        require(git_commit_parentcount(current.get())<=1,Error::Corrupt);
        if(index>=offset&&(revision.empty()||oid_text(git_commit_id(current.get()))==revision)) {
            const char* raw=git_commit_message(current.get());require(raw!=nullptr,Error::Corrupt);
            const auto length=std::strlen(raw);require(length<=160,Error::Corrupt);
            const std::string message(raw,length);const auto first=message.find('.',3),second=message.find('.',first==std::string::npos?message.size():first+1);
            require(message.rfind("v1.",0)==0&&first!=std::string::npos&&second!=std::string::npos,Error::Corrupt);
            const auto action=parse_action(message.substr(3,first-3));
            const auto identity=message.substr(first+1,second-first-1);
            require(identity_id(identity)&&identity_id(message.substr(second+1)),Error::Corrupt);
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
            if(decrypt_payload) {
                plain=decrypt_snapshot(key,std::vector<unsigned char>(bytes,bytes+size),message);
                if(action==HistoryAction::Labelled)require(valid_label(plain),Error::Corrupt);
            }
            const auto committed=git_commit_time(current.get());
            const auto offset_minutes=git_commit_time_offset(current.get());
            require(offset_minutes>=-1440&&offset_minutes<=1440,Error::Corrupt);
            output.push_back({oid_text(git_commit_id(current.get())),identity,action,std::move(plain),
                              static_cast<std::int64_t>(committed),offset_minutes,1,false});
            if(!revision.empty())break;
        }
        if(git_commit_parentcount(current.get())==0)break;
        bounded_object(repo.get(),git_commit_parent_id(current.get(),0),GIT_OBJECT_COMMIT,8192);
        git_commit* raw_parent=nullptr;ok(git_commit_parent(&raw_parent,current.get(),0));current.reset(raw_parent);
    }
    return output;
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
    require(entries.size()==1&&entries.front().action==HistoryAction::Labelled,Error::Missing);
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
void IdentityHistory::replace_credential(const Secret& old_answer,CredentialKind kind,const Secret& new_answer) {
    std::lock_guard<std::mutex> guard(history_mutex);safe_directory(m_impl->data);
    FileLock lock(m_impl->data/"identity-history-v1.lock");m_impl->authenticate(old_answer);
    auto repo=m_impl->open();auto key=m_impl->key();auto current=m_impl->head(repo.get());
    Credentials(m_impl->vault).replace(credential_account,old_answer,kind,new_answer);
}
}
