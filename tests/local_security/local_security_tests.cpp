#include "libslic3r/LocalSecurity/LocalSecurity.hpp"
#include "libslic3r/LocalSecurity/Authenticator.hpp"
#include "libslic3r/LocalSecurity/ElementLock.hpp"
#include "libslic3r/LocalSecurity/SupportTickets.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <map>
#include <functional>
#include <fstream>

using namespace Slic3r::LocalSecurity;
namespace {
unsigned checks=0;
void check(bool condition, const char* label) { ++checks; if(!condition)throw std::runtime_error(label); }
void rejects(const std::function<void()>& fn,const char* label) {
    bool rejected=false;try{fn();}catch(const Failure&){rejected=true;}check(rejected,label);
}
class MemoryVault : public Vault {
public:
    std::map<std::string,std::vector<unsigned char>> records;
    bool unavailable=false;
    std::optional<Secret> read(const std::string& id) override {
        if(unavailable)throw Failure(Error::Unavailable);
        auto it=records.find(id);if(it==records.end())return {};
        return Secret(it->second);
    }
    void write(const std::string& id,const Secret& value) override {
        if(unavailable)throw Failure(Error::Unavailable);
        records[id]=std::vector<unsigned char>(value.data(),value.data()+value.size());
    }
    void erase(const std::string& id) override { if(unavailable)throw Failure(Error::Unavailable);records.erase(id); }
};
void rfc_vectors() {
    const std::array<std::uint64_t,6> times{59,1111111109,1111111111,1234567890,2000000000,20000000000ULL};
    const std::array<std::array<const char*,6>,3> expected{{
        {{"94287082","07081804","14050471","89005924","69279037","65353130"}},
        {{"46119246","68084774","67062674","91819424","90698825","77737706"}},
        {{"90693936","25091201","99943326","93441116","38618901","47863826"}}
    }};
    const std::array<std::string,3> keys{"12345678901234567890","12345678901234567890123456789012","1234567890123456789012345678901234567890123456789012345678901234"};
    for(unsigned a=0;a<3;++a)for(unsigned i=0;i<6;++i) {
        Secret key(keys[a]);TotpParameters p{static_cast<Algorithm>(a),8,30};auto code=totp(key,p,times[i]);
        check(code.current==expected[a][i],"RFC 6238 eight digit vector");
        check(verify_totp(key,p,code.current,times[i],0).has_value(),"RFC verification");
        p.digits=6;check(totp(key,p,times[i]).current==std::string(expected[a][i]).substr(2),"RFC truncated six digit vector");
    }
    const std::array<const char*,10> hotps{"755224","287082","359152","969429","338314","254676","287922","162583","399871","520489"};
    Secret key(keys[0]);for(unsigned i=0;i<10;++i)check(hotp(key,i,Algorithm::Sha1,6)==hotps[i],"RFC 4226 vector");
    check(totp(key,{},59).next==totp(key,{},60).current,"next code rollover");
    check(totp(key,{},59).seconds_remaining==1 && totp(key,{},60).seconds_remaining==30,"countdown rollover");
    check(verify_totp(key,{},totp(key,{},30).current,60).has_value(),"inside skew");
    check(!verify_totp(key,{},totp(key,{},0).current,90).has_value(),"outside skew");
    check(!verify_totp(key,{},"abcdef",60).has_value(),"non-numeric rejected");
    rejects([&]{totp(key,{Algorithm::Sha1,6,0},0);},"zero period rejected");
    rejects([&]{totp(key,{Algorithm::Sha1,9,30},0);},"oversized digits rejected");
    rejects([&]{verify_totp(key,{},"123456",0,3);},"excess skew rejected");
}
void parsing() {
    Secret raw("12345678901234567890");auto encoded=encode_base32(raw);auto decoded=decode_base32(encoded);
    check(decoded.size()==raw.size() && std::equal(decoded.data(),decoded.data()+decoded.size(),raw.data()),"base32 roundtrip");
    Enrollment e; e.issuer="Example issuer";e.account="someone@example.test";e.secret=Secret("12345678901234567890");e.parameters={Algorithm::Sha512,8,45};
    auto parsed=parse_otpauth(pairing_uri(e));check(parsed.issuer==e.issuer&&parsed.account==e.account,"URI labels roundtrip");
    check(parsed.parameters.algorithm==Algorithm::Sha512&&parsed.parameters.digits==8&&parsed.parameters.period==45,"URI parameters honored");
    check(totp(parsed.secret,parsed.parameters,90).current==totp(e.secret,e.parameters,90).current,"URI secret roundtrip");
    for(const char* bad:{"A","AAA","AAAAAA","MZ","MY=====","MY======A","M0",""})rejects([&]{decode_base32(bad);},"invalid base32");
    rejects([&]{parse_otpauth("otpauth://hotp/test?secret=MY");},"HOTP import rejected");
    rejects([&]{parse_otpauth("otpauth://totp/test?secret=MY&secret=MY");},"duplicate parameters rejected");
    rejects([&]{parse_otpauth("otpauth://totp/one:test?secret=MY&issuer=two");},"issuer mismatch rejected");
    rejects([&]{parse_otpauth("otpauth://totp/test?secret=MY&period=0");},"URI zero period rejected");
    rejects([&]{parse_otpauth("otpauth://totp/test?secret=MY&algorithm=MD5");},"unsupported digest rejected");
    check(!valid_stable_id("../secret")&&!valid_stable_id("renamed label")&&valid_stable_id(shared_mode_account),"account key validation");
}
void credentials() {
    MemoryVault vault;Credentials c(vault);Secret pin("123456"),wrong("000000"),password("a-local-test-password");
    check(!c.metadata("test.account").configured,"unconfigured metadata");
    auto first=c.enroll("test.account",CredentialKind::Pin,pin);check(first.configured&&!first.generation.empty(),"enrollment metadata");
    check(c.verify("test.account",pin)&&!c.verify("test.account",wrong),"correct and incorrect answers");
    rejects([&]{c.enroll("test.account",CredentialKind::Pin,wrong);},"enroll does not overwrite");
    rejects([&]{c.replace("test.account",wrong,CredentialKind::Password,password);},"replacement needs old answer");
    auto second=c.replace("test.account",pin,CredentialKind::Password,password);check(second.generation!=first.generation,"replacement generation changes");
    check(c.verify("test.account",password)&&!c.verify("test.account",pin),"old credential retired");
    auto& bytes=vault.records.at("test.account");std::string record(bytes.begin(),bytes.end());
    check(record.find("a-local-test-password")==std::string::npos&&record.find("123456")==std::string::npos,"record contains no plaintext answer");
    rejects([&]{c.reset("test.account",pin);},"reset requires current answer");c.reset("test.account",password);
    check(!c.metadata("test.account").configured,"reset removes record");
    rejects([&]{c.verify("test.account",pin);},"missing vault record fails closed");
    rejects([&]{c.enroll("bad.pin",CredentialKind::Pin,password);},"non-numeric PIN rejected");
    vault.records["corrupt"]={1};rejects([&]{c.metadata("corrupt");},"corrupt credential rejected");
    vault.unavailable=true;rejects([&]{c.enroll("unavailable",CredentialKind::Pin,pin);},"unavailable vault fails closed");
}
void locks() {
    auto now=Time{}+std::chrono::hours(1);
    const std::array<std::vector<Factor>,6> orders{{{Factor::Pin},{Factor::Password},{Factor::Pin,Factor::Password},{Factor::Password,Factor::Totp},{Factor::Pin,Factor::Totp},{Factor::Password,Factor::Pin,Factor::Totp}}};
    for(unsigned p=0;p<6;++p) {
        auto policy=static_cast<Policy>(p);check(factors(policy)==orders[p],"exact six factor policies");
        LockSession lock({policy,Duration::Minutes,1});check(lock.locked(now),"locked at launch");
        for(unsigned i=0;i<orders[p].size();++i) {
            check(lock.expected(now)==orders[p][i],"ordered factor prompt");
            check(lock.submit(orders[p][i],true,now)==(i+1==orders[p].size()),"all factors required");
        }
        check(!lock.locked(now)&&lock.locked(now+std::chrono::minutes(1)),"duration expires");
    }
    LockSession order({Policy::PasswordPinTotp,Duration::UntilExit,5});
    check(!order.submit(Factor::Totp,true,now)&&order.expected(now)==Factor::Password,"wrong order fails closed");
    order.submit(Factor::Password,true,now);check(order.expected(now+std::chrono::seconds(121))==Factor::Password,"partial attempt expires");
    LockSession single({Policy::Pin,Duration::ThisSurface,5});single.submit(Factor::Pin,true,now);single.leave_surface();check(single.locked(now),"surface exit relocks");
    LockSession one({Policy::Pin,Duration::UntilExit,5}),two({Policy::Pin,Duration::UntilExit,5});
    one.submit(Factor::Pin,true,now);check(!one.locked(now)&&two.locked(now),"locks independent");one.relock();check(one.locked(now),"explicit relock");
    for(unsigned round=0;round<4;++round) {
        for(unsigned n=0;n<5;++n)one.submit(Factor::Pin,false,now);
        check(one.attempts(now).remaining==0&&one.attempts(now).wait_seconds>0,"attempt budget enforced");
        check(!one.submit(Factor::Pin,true,now)&&one.locked(now),"lockout blocks even correct answer");
        check(one.clear_wait(now)==(round<3),"rolling skip cap");
        check(one.locked(now),"skip does not authenticate");
    }
    check(one.attempts(now+std::chrono::hours(1)).remaining==5,"ordinary wait restores same budget");
    rejects([&]{LockSession invalid({static_cast<Policy>(99),Duration::UntilExit,5});},"invalid policy rejected");
}
void encryption() {
    auto key=random_secret(32);Secret plain("private snapshot fixture");auto encrypted=encrypt_snapshot(key,plain,"record.1");
    auto restored=decrypt_snapshot(key,encrypted,"record.1");check(restored.size()==plain.size()&&std::equal(restored.data(),restored.data()+restored.size(),plain.data()),"AES-GCM roundtrip");
    check(std::string(encrypted.begin(),encrypted.end()).find("private snapshot fixture")==std::string::npos,"snapshot not plaintext");
    auto other=random_secret(32);rejects([&]{decrypt_snapshot(other,encrypted,"record.1");},"wrong key rejected");
    rejects([&]{decrypt_snapshot(key,encrypted,"record.2");},"record substitution rejected");
    encrypted[14]^=1;rejects([&]{decrypt_snapshot(key,encrypted,"record.1");},"tamper rejected");
    Secret maximum(std::vector<unsigned char>(1024*1024,0x5a));auto full=encrypt_snapshot(key,maximum,"maximum");check(decrypt_snapshot(key,full,"maximum").size()==maximum.size(),"maximum snapshot decrypts");
    Secret empty;check(decrypt_snapshot(key,encrypt_snapshot(key,empty,"empty"),"empty").size()==0,"empty snapshot decrypts");
}
void authenticator() {
    MemoryVault vault;unsigned recorded=0;bool history_available=true;
    AuthenticatorStore store(vault,[&](IdentityAction,const std::string& id,const Secret& snapshot){
        if(!history_available)throw Failure(Error::History);
        check(valid_stable_id(id)&&snapshot.size()>0,"history receives stable identity and snapshot");++recorded;
    });
    check(store.list().empty(),"authenticator honest empty state");
    auto make=[] {Enrollment e;e.issuer="Example";e.account="user@example.test";e.secret=Secret("12345678901234567890");return e;};
    auto e=make();auto current=totp(e.secret,e.parameters,59).current;
    rejects([&]{store.add(make(),"000000",59);},"pairing wrong code rejected");check(store.list().empty()&&recorded==0,"unconfirmed pairing writes nothing");
    history_available=false;rejects([&]{store.add(make(),current,59);},"history failure surfaced");check(store.list().empty(),"history failure preserves live state");history_available=true;
    auto first=store.add(make(),current,59);auto second=store.add(make(),current,59);
    check(store.list().size()==2&&first!=second,"independent stable entry IDs");
    AuthenticatorStore restarted(vault,[&](IdentityAction,const std::string&,const Secret&){++recorded;});
    check(restarted.code(first,59).current==current,"authenticator vault restart");
    store.rename(first,"Changed issuer","another@example.test","Personal");auto entry=store.list().front();
    check(entry.id==first&&entry.issuer=="Changed issuer"&&entry.group=="Personal","rename preserves stable identity");
    check(store.code(first,59).current==current,"rename preserves secret");
    store.move(first,1);check(store.list()[1].id==first,"reorder persisted");
    auto exported=store.export_redacted();check(exported.find("Secrets and generated codes omitted.")!=std::string::npos,"export declares omitted fields");
    check(exported.find(current)==std::string::npos&&exported.find("12345678901234567890")==std::string::npos&&exported.find("GEZDGNBV")==std::string::npos,"export excludes secret and codes");
    store.remove(first);check(store.list().size()==1&&store.list()[0].id==second,"remove only selected entry");
    rejects([&]{store.code(first,59);},"removed entry unavailable");
    rejects([&]{AuthenticatorStore invalid(vault,{});},"history callback required");
}
void element_locks() {
    MemoryVault vault;unsigned recorded=0;auto history=[&](IdentityAction action,const std::string&,const Secret& snapshot){check(action==IdentityAction::LockCreated&&snapshot.size()==5,"lock history excludes credentials");++recorded;};
    auto now=Time{}+std::chrono::hours(1);ElementLock first(vault,"element.one",history);
    check(!first.configured()&&first.allows_action(now),"unconfigured element permitted");
    LockEnrollment enrollment;enrollment.settings={Policy::PasswordPinTotp,Duration::Minutes,1};enrollment.pin=Secret("123456");enrollment.password=Secret("test password answer");
    Enrollment otp;otp.secret=Secret("12345678901234567890");enrollment.confirmation_code=totp(otp.secret,otp.parameters,59).current;enrollment.otp=std::move(otp);
    first.create(std::move(enrollment),59);check(recorded==1&&first.configured()&&!first.allows_action(now),"created element blocks actions");
    check(!first.submit(Secret("test password answer"),now,59)&&first.expected(now)==Factor::Pin,"backend password factor");
    check(!first.submit(Secret("123456"),now,59)&&first.expected(now)==Factor::Totp,"backend PIN factor");
    check(first.submit(Secret("287082"),now,59)&&first.allows_action(now),"backend OTP completes ordered policy");
    first.relock();first.submit(Secret("test password answer"),now,59);first.submit(Secret("123456"),now,59);
    check(!first.submit(Secret("287082"),now,59)&&!first.allows_action(now),"used OTP step cannot replay");
    ElementLock restarted(vault,"element.one",history);check(restarted.configured()&&!restarted.allows_action(now),"configured element relocks on restart");
    ElementLock other(vault,"element.two",history);check(other.allows_action(now),"lock credentials do not inherit");
}
void native_vault() {
#ifdef _WIN32
    auto vault=make_os_vault();const auto id="test.local-security."+new_stable_id();Secret fixture("non-production-test-record");
    try {
        check(!vault->read(id).has_value(),"native initial absence");vault->write(id,fixture);auto record=vault->read(id);
        check(record.has_value()&&record->size()==fixture.size()&&std::equal(record->data(),record->data()+record->size(),fixture.data()),"native vault roundtrip");
        vault->erase(id);check(!vault->read(id).has_value(),"native owned record erased");
    } catch(...) {vault->erase(id);throw;}
#endif
}
void support_tickets() {
    MemoryVault vault;auto folder=std::filesystem::temp_directory_path()/("local-ticket-test-"+new_stable_id());SupportTickets tickets(vault,folder);
    check(tickets.list().empty(),"support honest empty state");
    auto id=tickets.create(TicketCategory::ForgottenAnswer,5,"Forgot local answer");
    check(tickets.list().size()==1&&tickets.list()[0].id==id,"ticket created and listed");
    SupportTickets restart(vault,folder);check(restart.list()[0].stage==TicketStage::Created,"ticket persists across restart");
    tickets.advance(id);check(tickets.list()[0].stage==TicketStage::Reviewed,"ticket advances to review");tickets.advance(id);check(tickets.list()[0].stage==TicketStage::Resolution,"ticket advances to resolution");
    const auto file=folder/"local_security"/"support-tickets-v1.enc";std::ifstream input(file,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());input.close();
    check(bytes.find("Forgot local answer")==std::string::npos,"ticket description encrypted on disk");
    check(tickets.export_text().find("Nothing was sent.")!=std::string::npos,"ticket export states local boundary");
    rejects([&]{tickets.create(TicketCategory::LocalReset,0,"invalid severity");},"ticket severity bounded");
    rejects([&]{tickets.create(TicketCategory::LocalReset,1,std::string(161,'a'));},"ticket description bounded");
    tickets.remove(id);check(tickets.list().empty(),"ticket removal persisted");
    std::filesystem::remove(file);std::filesystem::remove(file.parent_path());std::filesystem::remove(folder);
    check(tickets.list().empty(),"folder reset removes tickets");
}
void application_vault() {
#ifdef _WIN32
    auto folder=std::filesystem::temp_directory_path()/("local-vault-test-"+new_stable_id());auto first=make_application_vault(folder);Secret value("test scoped value");first->write("test-record",value);
    auto same=make_application_vault(folder);check(same->read("test-record").has_value(),"application marker survives restart");
    auto marker=folder/"local_security"/"instance-v1";std::filesystem::remove(marker);auto reset=make_application_vault(folder);
    check(!reset->read("test-record").has_value(),"removed marker yields fresh namespace");first->erase("test-record");
    std::filesystem::remove(marker);std::filesystem::remove(marker.parent_path());std::filesystem::remove(folder);
#endif
}
}
int main() {
    try { rfc_vectors();parsing();credentials();locks();encryption();authenticator();element_locks();support_tickets();native_vault();application_vault();std::cout<<"PASS "<<checks<<" local security behavioral checks\n";return 0; }
    catch(const std::exception& e) { std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1; }
}
