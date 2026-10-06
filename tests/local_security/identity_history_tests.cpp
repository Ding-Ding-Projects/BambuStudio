#include "libslic3r/LocalSecurity/IdentityHistory.hpp"
#include <git2.h>
#include <functional>
#include <algorithm>
#include <iostream>
#include <map>
#include <fstream>
using namespace Slic3r::LocalSecurity;
namespace {
unsigned checks=0;
void check(bool value,const char* label){++checks;if(!value)throw std::runtime_error(label);}
void rejects(const std::function<void()>& fn,const char* label){bool rejected=false;try{fn();}catch(const Failure&){rejected=true;}check(rejected,label);}
class MemoryVault final:public Vault {
public:
    std::map<std::string,std::vector<unsigned char>> records;
    bool unavailable=false,fail_anchor=false;
    std::optional<Secret> read(const std::string& id)override{
        if(unavailable)throw Failure(Error::Unavailable);
        auto it=records.find(id);if(it==records.end())return {};
        return Secret(it->second);
    }
    void write(const std::string& id,const Secret& value)override{
        if(unavailable||(fail_anchor&&id==IdentityHistory::anchor_account))throw Failure(Error::Unavailable);
        records[id]={value.data(),value.data()+value.size()};
    }
    void erase(const std::string& id)override{records.erase(id);}
};
struct Fixture {
    std::filesystem::path root=std::filesystem::temp_directory_path()/("identity-history-test-"+new_stable_id());
    MemoryVault vault;
    Fixture(){std::filesystem::create_directory(root);}
    ~Fixture(){std::error_code ignored;std::filesystem::remove_all(root,ignored);}
};
void behavior(){
    Fixture f;IdentityHistory history(f.root,f.vault);Secret answer("history-password-123"),wrong("wrong-password-123");
    rejects([&]{history.read(answer);},"missing state is not auto-created");
    history.initialize(CredentialKind::Password,answer);
    check(history.read(answer).empty(),"new history empty");
    rejects([&]{history.initialize(CredentialKind::Password,answer);},"enrollment cannot overwrite");
    const auto id=new_stable_id();Secret first("first private label"),second("second private label");
    auto a=history.append(HistoryAction::Created,id,first);
    auto b=history.append(HistoryAction::Renamed,id,second);
    check(a!=b,"append creates a new revision");
    rejects([&]{history.read(wrong);},"separate credential required");
    auto entries=history.read(answer);
    check(entries.size()==2&&entries[0].revision==b&&entries[1].revision==a,"newest first and append retention");
    check(entries[0].identity==id&&entries[1].identity==id,"rename keeps stable identity");
    check(entries[0].action==HistoryAction::Renamed&&entries[1].action==HistoryAction::Created,"redacted action enums");
    check(entries[0].snapshot.size()==second.size()&&std::equal(second.data(),second.data()+second.size(),entries[0].snapshot.data()),"encrypted snapshot round trip");
    check(history.read(answer,1,1).front().revision==a,"bounded page");
    rejects([&]{history.read(answer,0,17);},"page ceiling");
    rejects([&]{history.read(answer,10001,1);},"offset ceiling");
    rejects([&]{history.append(HistoryAction::Created,"user-visible-label",first);},"metadata rejects labels");
    rejects([&]{history.append(static_cast<HistoryAction>(123),id,first);},"unknown action rejected");
    Secret replacement("replacement-history-password");history.replace_credential(answer,CredentialKind::Password,replacement);
    rejects([&]{history.read(answer);},"old credential invalidated");
    check(history.read(replacement).size()==2,"replacement preserves encrypted history");
    // Inspect reachable raw object data, not compressed bytes or source strings.
    git_repository* raw_repo=nullptr;check(git_repository_open_bare(&raw_repo,(f.root/"identity-history-v1.git").u8string().c_str())==0,"bare repository opens");
    git_oid oid{};check(git_oid_fromstr(&oid,b.c_str())==0,"revision valid");git_commit* commit=nullptr;
    check(git_commit_lookup(&commit,raw_repo,&oid)==0,"revision exists");
    check(std::string(git_commit_message(commit)).find("private label")==std::string::npos,"metadata redacted");
    git_tree* tree=nullptr;check(git_commit_tree(&tree,commit)==0,"snapshot tree exists");
    auto entry=git_tree_entry_byname(tree,"snapshot.enc");git_blob* blob=nullptr;
    check(entry&&git_blob_lookup(&blob,raw_repo,git_tree_entry_id(entry))==0,"encrypted blob exists");
    std::string bytes(static_cast<const char*>(git_blob_rawcontent(blob)),static_cast<std::size_t>(git_blob_rawsize(blob)));
    check(bytes.find("second private label")==std::string::npos,"snapshot plaintext absent from raw object");
    git_blob_free(blob);git_tree_free(tree);git_commit_free(commit);git_repository_free(raw_repo);
    auto saved=f.vault.records[IdentityHistory::key_account];f.vault.records.erase(IdentityHistory::key_account);
    rejects([&]{history.read(replacement);},"missing key fails closed");
    rejects([&]{history.append(HistoryAction::Removed,id,first);},"missing key blocks append");
    f.vault.records[IdentityHistory::key_account]=std::vector<unsigned char>(32,0);
    rejects([&]{history.read(replacement);},"wrong key authentication fails");
    f.vault.records[IdentityHistory::key_account]=saved;
    f.vault.unavailable=true;rejects([&]{history.read(replacement);},"vault unavailability fails closed");f.vault.unavailable=false;
    // A stale external anchor cannot authorize a different repository head.
    f.vault.records[IdentityHistory::anchor_account]={a.begin(),a.end()};
    rejects([&]{history.read(replacement);},"head-anchor mismatch rejected");
    rejects([&]{history.append(HistoryAction::Removed,id,first);},"anchor mismatch blocks mutations");
}
void interrupted_transaction(){
    Fixture f;IdentityHistory history(f.root,f.vault);Secret answer("history-password-123"),data("noncredential snapshot");
    history.initialize(CredentialKind::Password,answer);f.vault.fail_anchor=true;
    rejects([&]{history.append(HistoryAction::Created,new_stable_id(),data);},"anchor failure reported");
    f.vault.fail_anchor=false;
    rejects([&]{history.read(answer);},"incomplete transaction cannot silently recover");
    rejects([&]{history.initialize(CredentialKind::Password,answer);},"incomplete state cannot reinitialize");
}
void missing_repository(){
    Fixture f;IdentityHistory history(f.root,f.vault);Secret answer("history-password-123"),data("snapshot");
    history.initialize(CredentialKind::Password,answer);
    history.append(HistoryAction::Created,new_stable_id(),data);
    std::filesystem::rename(f.root/"identity-history-v1.git",f.root/"retained-history.git");
    rejects([&]{history.read(answer);},"missing repository fails closed");
    rejects([&]{history.initialize(CredentialKind::Password,answer);},"missing repository does not re-key");
}
void metadata_and_labels(){
    Fixture f;IdentityHistory history(f.root,f.vault);Secret answer("history-password-123"),wrong("incorrect-password");
    history.initialize(CredentialKind::Password,answer);
    const auto identity=new_stable_id();Secret payload("private seed-bearing snapshot"),label("Before account rename");
    const auto revision=history.append(HistoryAction::Created,identity,payload);
    auto rows=history.read_metadata(answer);
    check(rows.size()==1&&rows[0].revision==revision&&rows[0].identity==identity,"metadata page identity");
    check(rows[0].committed_at_utc_seconds>0&&rows[0].format_version==1&&!rows[0].pruned,"timestamp and honest v1 state");
    check(history.read(answer)[0].committed_at_utc_seconds==rows[0].committed_at_utc_seconds,"timestamp consistent across read surfaces");
    const auto label_revision=history.append_label(answer,identity,label);
    auto decrypted=history.read_label(answer,label_revision);
    check(decrypted.size()==label.size()&&std::equal(label.data(),label.data()+label.size(),decrypted.data()),"encrypted label roundtrip");
    auto labelled_rows=history.read_metadata(answer);
    check(labelled_rows[0].action==HistoryAction::Labelled&&labelled_rows[1].revision==revision,"label appends without rewriting snapshot");
    auto diff=IdentityHistory::redacted_diff(labelled_rows[1],labelled_rows[0]);
    check(diff.same_identity&&diff.action_changed,"redacted diff contains only enum and identity comparisons");
    const auto exported=history.export_redacted(answer);
    check(exported.find("private seed-bearing snapshot")==std::string::npos&&exported.find("Before account rename")==std::string::npos,"export excludes snapshots and labels");
    check(exported.find(label_revision)!=std::string::npos&&exported.find("committed_at_utc_seconds")!=std::string::npos,"export has revision and timestamp");
    rejects([&]{history.read_metadata(wrong);},"metadata still authenticated");
    rejects([&]{history.append_label(wrong,identity,label);},"label authentication required");
    rejects([&]{history.export_redacted(wrong);},"export authentication required");
    rejects([&]{history.read_label(answer,revision);},"ordinary snapshot cannot be returned as label");
    rejects([&]{history.read_label(answer,"../config");},"label rejects path as revision");
    rejects([&]{history.append_label(answer,identity,Secret("bad\nlabel"));},"label control characters rejected");
    rejects([&]{history.append_label(answer,identity,Secret(std::vector<unsigned char>{0xc0,0x80}));},"overlong UTF-8 label rejected");
    rejects([&]{history.append_label(answer,identity,Secret(std::string(257,'a')));},"oversized label rejected");
    check(history.read_metadata(answer).size()==2,"rejected label appends leave history intact");
}
void throttling(){
    Fixture f;IdentityHistory history(f.root,f.vault);Secret answer("history-password-123"),wrong("incorrect-password");
    history.initialize(CredentialKind::Password,answer);
    for(unsigned i=0;i<5;++i)rejects([&]{history.read(wrong);},"incorrect credential rejected");
    bool limited=false;try{history.read(answer);}catch(const Failure& e){limited=e.code()==Error::RateLimited;}
    check(limited,"attempt budget enforces wait");
}
}
int main(){try{behavior();interrupted_transaction();missing_repository();metadata_and_labels();throttling();std::cout<<"PASS "<<checks<<" identity history checks\n";return 0;}catch(const std::exception&){std::cerr<<"FAIL identity history checks\n";return 1;}}
