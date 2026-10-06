#include "libslic3r/LocalSecurity/IdentityHistory.hpp"
#include <git2.h>
#include <openssl/hmac.h>
#include <cstdio>
#include "slic3r/GUI/Widgets/SuperConfirmState.hpp"
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
    bool unavailable=false,fail_anchor=false,fail_store_anchor=false;
    std::optional<Secret> read(const std::string& id)override{
        if(unavailable)throw Failure(Error::Unavailable);
        auto it=records.find(id);if(it==records.end())return {};
        return Secret(it->second);
    }
    void write(const std::string& id,const Secret& value)override{
        if(unavailable||(fail_anchor&&id==IdentityHistory::anchor_account)||(fail_store_anchor&&id==IdentityHistory::key_store_anchor_account))throw Failure(Error::Unavailable);
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
    check(rows[0].committed_at_utc_seconds>0&&rows[0].format_version==2&&!rows[0].pruned,"timestamp and honest v2 state");
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

Slic3r::GUI::SuperConfirm::State authorized_confirmation(){
    Slic3r::GUI::SuperConfirm::State state;state.toggle_key(0);state.toggle_key(1);state.set_slider(100);return state;
}
std::vector<unsigned char> file_bytes(const std::filesystem::path& path){
    std::ifstream input(path,std::ios::binary);return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
void write_bytes(const std::filesystem::path& path,const std::vector<unsigned char>& bytes){
    std::ofstream output(path,std::ios::binary|std::ios::trunc);output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
}
std::string legacy_fixture(Fixture& f,const std::string& identity,const Secret& plaintext){
    git_repository* repo=nullptr;check(git_repository_open_bare(&repo,(f.root/"identity-history-v1.git").u8string().c_str())==0,"v1 fixture repository");
    auto key=f.vault.read(IdentityHistory::key_account);const auto message="v1.created."+identity+"."+new_stable_id();
    const auto encrypted=encrypt_snapshot(*key,plaintext,message);git_oid blob{},tree_id{},commit_id{};
    check(git_blob_create_frombuffer(&blob,repo,encrypted.data(),encrypted.size())==0,"v1 fixture blob");
    git_treebuilder* builder=nullptr;check(git_treebuilder_new(&builder,repo,nullptr)==0,"v1 fixture builder");
    check(git_treebuilder_insert(nullptr,builder,"snapshot.enc",&blob,GIT_FILEMODE_BLOB)==0,"v1 fixture tree entry");
    check(git_treebuilder_write(&tree_id,builder)==0,"v1 fixture tree");git_treebuilder_free(builder);
    git_tree* tree=nullptr;check(git_tree_lookup(&tree,repo,&tree_id)==0,"v1 fixture tree lookup");
    git_signature* signature=nullptr;check(git_signature_now(&signature,"Synthetic fixture","fixture@invalid")==0,"v1 fixture signature");
    check(git_commit_create(&commit_id,repo,"refs/heads/identity-history",signature,signature,nullptr,message.c_str(),tree,0,nullptr)==0,"v1 fixture commit");
    char text[GIT_OID_HEXSZ+1]{};git_oid_tostr(text,sizeof(text),&commit_id);
    git_signature_free(signature);git_tree_free(tree);git_repository_free(repo);
    f.vault.write(IdentityHistory::anchor_account,Secret(text));return text;
}
void v2_retention(){
    Fixture f;IdentityHistory history(f.root,f.vault);Secret answer("history-password-123"),payload("synthetic recoverable seed"),label("Temporary label");
    history.initialize(CredentialKind::Password,answer);const auto identity=new_stable_id();
    auto first=history.append(HistoryAction::Created,identity,payload);
    auto second=history.append_label(answer,identity,label);
    check(f.vault.records.size()==4,"fixed number of native vault records");
    auto initial=history.capacity(answer);check(initial.active_keys==2&&initial.tombstones==0&&initial.maximum_active_keys==512&&initial.maximum_tombstones==4096,"bounded key capacity state");
    const auto original_store=file_bytes(f.root/"identity-history-keys-v2.enc");
    const auto original_anchor=f.vault.records[IdentityHistory::key_store_anchor_account];
    auto preview=history.preview_prune(answer,{first,second});check(preview.rows().size()==2,"selection bound preview");
    Slic3r::GUI::SuperConfirm::State state;
    rejects([&]{history.prune(answer,preview,state);},"pruning requires native confirmation");
    state.toggle_key(0);state.set_slider(100);rejects([&]{history.prune(answer,preview,state);},"one key cannot authorize");
    state.toggle_key(1);state.set_slider(99);rejects([&]{history.prune(answer,preview,state);},"partial slider cannot authorize");
    state.set_slider(100);const auto audit=history.prune(answer,preview,state);
    check(state.cancelled&&!state.may_fire(),"confirmation consumed");
    auto rows=history.read(answer);check(rows.size()==3&&rows.front().revision==audit&&rows.front().action==HistoryAction::Pruned,"append-only prune audit");
    check(rows[1].revision==second&&rows[1].pruned&&rows[1].snapshot.size()==0&&rows[2].revision==first&&rows[2].pruned&&rows[2].snapshot.size()==0,"pruned rows remain without plaintext");
    rejects([&]{history.read_label(answer,second);},"pruned label cannot decrypt");
    auto after=history.capacity(answer);check(after.active_keys==1&&after.tombstones==2,"only audit key remains active");
    rejects([&]{history.preview_prune(answer,{audit});},"prune audit cannot be pruned");
    rejects([&]{history.preview_prune(answer,{first});},"already pruned selection rejected");
    rejects([&]{history.preview_prune(answer,{audit,audit});},"duplicate selections rejected");
    rejects([&]{history.preview_prune(answer,{std::string(40,'0')});},"unreachable revision rejected");
    auto fresh_state=authorized_confirmation();rejects([&]{history.prune(answer,preview,fresh_state);},"stale preview rejected");
    check(fresh_state.may_fire(),"failed validation does not consume confirmation");
    IdentityHistory reopened(f.root,f.vault);check(reopened.read_metadata(answer)[1].pruned,"tombstones persist across service instances");
    const auto active=history.append(HistoryAction::Restored,identity,payload);
    auto active_preview=history.preview_prune(answer,{active});
    rejects([&]{history.prune(answer,active_preview,state);},"consumed native state cannot authorize another selection");
    check(history.read_revision(answer,active).snapshot.size()==payload.size(),"single-revision read ignores pruned siblings");
    rejects([&]{history.read_revision(answer,first);},"single-revision read rejects pruned target");
    check(history.read(answer).size()==4,"mixed active and pruned page stays readable");
    const auto current_store=file_bytes(f.root/"identity-history-keys-v2.enc");
    write_bytes(f.root/"identity-history-keys-v2.enc",original_store);
    rejects([&]{history.read(answer);},"key store rollback rejected by vault anchor");
    f.vault.records[IdentityHistory::key_store_anchor_account]=original_anchor;
    rejects([&]{history.read(answer);},"restoring store and its anchor still fails head binding");
    write_bytes(f.root/"identity-history-keys-v2.enc",current_store);
}
void legacy_and_stale_preview(){
    Fixture f;IdentityHistory history(f.root,f.vault);Secret answer("history-password-123"),payload("legacy synthetic seed");
    history.initialize(CredentialKind::Password,answer);const auto identity=new_stable_id();
    const auto old=legacy_fixture(f,identity,payload);auto old_rows=history.read(answer);
    check(old_rows.size()==1&&old_rows[0].format_version==1&&old_rows[0].snapshot.size()==payload.size(),"v1 snapshot stays readable");
    rejects([&]{history.preview_prune(answer,{old});},"v1 shared-key pruning refused");
    const auto modern=history.append(HistoryAction::Renamed,identity,payload);
    auto mixed=history.read(answer);check(mixed[0].format_version==2&&mixed[1].format_version==1,"v1 and v2 coexist");
    auto preview=history.preview_prune(answer,{modern});
    IdentityHistory other(f.root,f.vault);auto authorized=authorized_confirmation();
    rejects([&]{other.prune(answer,preview,authorized);},"preview cannot cross service ownership");
    check(authorized.may_fire(),"owner rejection preserves confirmation");
    history.append(HistoryAction::Removed,identity,payload);
    rejects([&]{history.prune(answer,preview,authorized);},"intervening append invalidates preview");
    rejects([&]{history.append(HistoryAction::Pruned,identity,payload);},"trusted append cannot forge prune action");
    auto current_preview=history.preview_prune(answer,{modern});history.prune(answer,current_preview,authorized);
    auto retained=history.read_revision(answer,old);
    check(retained.format_version==1&&retained.snapshot.size()==payload.size(),"v2 pruning preserves v1 decryption");
}
void store_failure_paths(){
    Secret answer("history-password-123"),payload("synthetic payload");
    {
        Fixture f;IdentityHistory history(f.root,f.vault);history.initialize(CredentialKind::Password,answer);
        f.vault.fail_store_anchor=true;
        rejects([&]{history.append(HistoryAction::Created,new_stable_id(),payload);},"key-store anchor failure reported");
        f.vault.fail_store_anchor=false;rejects([&]{history.read(answer);},"unanchored key store fails closed");
    }
    {
        Fixture f;IdentityHistory history(f.root,f.vault);history.initialize(CredentialKind::Password,answer);
        history.append(HistoryAction::Created,new_stable_id(),payload);
        std::filesystem::rename(f.root/"identity-history-keys-v2.enc",f.root/"retained.enc");
        rejects([&]{history.read(answer);},"missing key store rejected");
        rejects([&]{history.append(HistoryAction::Created,new_stable_id(),payload);},"missing key store cannot reinitialize");
    }
    {
        Fixture f;IdentityHistory history(f.root,f.vault);history.initialize(CredentialKind::Password,answer);
        history.append(HistoryAction::Created,new_stable_id(),payload);
        write_bytes(f.root/"identity-history-keys-v2.enc.pending",{1});
        rejects([&]{history.read(answer);},"unfinished atomic file blocks access");
    }
    {
        Fixture f;IdentityHistory history(f.root,f.vault);history.initialize(CredentialKind::Password,answer);
        history.append(HistoryAction::Created,new_stable_id(),payload);
        auto bytes=file_bytes(f.root/"identity-history-keys-v2.enc");bytes.back()^=1;
        write_bytes(f.root/"identity-history-keys-v2.enc",bytes);
        rejects([&]{history.capacity(answer);},"key-store corruption rejected");
    }
}
void interrupted_prune(){
    Fixture f;IdentityHistory history(f.root,f.vault);Secret answer("history-password-123"),payload("synthetic payload");
    history.initialize(CredentialKind::Password,answer);
    const auto revision=history.append(HistoryAction::Created,new_stable_id(),payload);
    const auto preview=history.preview_prune(answer,{revision});auto state=authorized_confirmation();
    f.vault.fail_store_anchor=true;
    rejects([&]{history.prune(answer,preview,state);},"interrupted prune reports storage failure");
    check(!state.may_fire()&&state.cancelled,"execution consumes state even on later storage failure");
    f.vault.fail_store_anchor=false;
    rejects([&]{history.read_metadata(answer);},"interrupted prune cannot claim a completed state");
}

void authenticated_store_fixture(Fixture& f,const std::function<void(std::vector<unsigned char>&)>& transform){
    auto master=f.vault.read(IdentityHistory::key_account);
    auto plain=decrypt_snapshot(*master,file_bytes(f.root/"identity-history-keys-v2.enc"),"identity-history-keys-v2");
    std::vector<unsigned char> bytes(plain.data(),plain.data()+plain.size());transform(bytes);
    const auto encrypted=encrypt_snapshot(*master,Secret(std::move(bytes)),"identity-history-keys-v2");
    unsigned length=0;Secret mac(std::vector<unsigned char>(32));
    check(HMAC(EVP_sha256(),master->data(),static_cast<int>(master->size()),encrypted.data(),encrypted.size(),mac.data(),&length)!=nullptr&&length==32,"synthetic store MAC");
    write_bytes(f.root/"identity-history-keys-v2.enc",encrypted);
    f.vault.write(IdentityHistory::key_store_anchor_account,mac);
}
void authenticated_store_validation(){
    Secret answer("history-password-123"),payload("synthetic payload");
    {
        Fixture f;IdentityHistory history(f.root,f.vault);history.initialize(CredentialKind::Password,answer);
        history.append(HistoryAction::Created,new_stable_id(),payload);
        authenticated_store_fixture(f,[](std::vector<unsigned char>& bytes){
            const auto count_position=6+bytes[5];bytes[count_position]=0;bytes[count_position+1]=0;
            bytes.erase(bytes.begin()+count_position+4,bytes.end());
        });
        rejects([&]{history.read_metadata(answer);},"unknown missing event key is not treated as pruning");
    }
    {
        Fixture f;IdentityHistory history(f.root,f.vault);history.initialize(CredentialKind::Password,answer);
        history.append(HistoryAction::Created,new_stable_id(),payload);
        authenticated_store_fixture(f,[](std::vector<unsigned char>& bytes){
            const auto position=6+bytes[5];bytes[position]=2;bytes[position+1]=1;
        });
        rejects([&]{history.capacity(answer);},"authenticated oversized key count rejected");
    }
    {
        Fixture f;IdentityHistory history(f.root,f.vault);history.initialize(CredentialKind::Password,answer);
        const auto revision=history.append(HistoryAction::Created,new_stable_id(),payload);
        authenticated_store_fixture(f,[](std::vector<unsigned char>& bytes){
            const auto position=6+bytes[5];bytes[position+2]=16;bytes[position+3]=0;
            for(unsigned i=0;i<4096;++i){char id[33]{};std::snprintf(id,sizeof(id),"%032x",i);bytes.insert(bytes.end(),id,id+32);}
        });
        check(history.capacity(answer).tombstones==4096,"exact tombstone capacity reported");
        rejects([&]{history.preview_prune(answer,{revision});},"tombstone overflow rejected before authorization");
        check(history.read_revision(answer,revision).snapshot.size()==payload.size(),"capacity refusal preserves active payload");
    }
}

void capacity_limits(){
    Fixture f;IdentityHistory history(f.root,f.vault);Secret answer("history-password-123"),payload("small snapshot");
    history.initialize(CredentialKind::Password,answer);const auto identity=new_stable_id();std::vector<std::string> selected;
    for(unsigned i=0;i<512;++i){auto revision=history.append(HistoryAction::Created,identity,payload);if(i<2)selected.push_back(revision);}
    check(history.capacity(answer).active_keys==512,"active keys reach exact bound");
    rejects([&]{history.append(HistoryAction::Created,identity,payload);},"active key overflow rejected");
    auto preview=history.preview_prune(answer,selected);auto state=authorized_confirmation();history.prune(answer,preview,state);
    check(history.capacity(answer).active_keys==511,"batch prune frees capacity after retaining audit key");
    history.append(HistoryAction::Created,identity,payload);check(history.capacity(answer).active_keys==512,"freed capacity reusable");
}

void throttling(){
    Fixture f;IdentityHistory history(f.root,f.vault);Secret answer("history-password-123"),wrong("incorrect-password");
    history.initialize(CredentialKind::Password,answer);
    for(unsigned i=0;i<5;++i)rejects([&]{history.read(wrong);},"incorrect credential rejected");
    bool limited=false;try{history.read(answer);}catch(const Failure& e){limited=e.code()==Error::RateLimited;}
    check(limited,"attempt budget enforces wait");
}
}
int main(){try{behavior();interrupted_transaction();missing_repository();metadata_and_labels();throttling();v2_retention();legacy_and_stale_preview();store_failure_paths();interrupted_prune();authenticated_store_validation();capacity_limits();std::cout<<"PASS "<<checks<<" identity history checks\n";return 0;}catch(const std::exception&){std::cerr<<"FAIL identity history checks\n";return 1;}}
