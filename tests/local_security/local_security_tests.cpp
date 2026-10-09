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
    LadderAllowance allowance;
    for(unsigned round=0;round<4;++round) {
        for(unsigned n=0;n<5;++n)one.submit(Factor::Pin,false,now);
        check(one.attempts(now).remaining==0&&one.attempts(now).wait_seconds>0,"attempt budget enforced");
        check(!one.submit(Factor::Pin,true,now)&&one.locked(now),"lockout blocks even correct answer");
        check(one.clear_wait(allowance,now)==(round<3),"rolling skip cap");
        check(one.locked(now),"skip does not authenticate");
    }
    check(one.attempts(now+std::chrono::hours(1)).remaining==5,"ordinary wait restores same budget");
    rejects([&]{LockSession invalid({static_cast<Policy>(99),Duration::UntilExit,5});},"invalid policy rejected");
}
ClockAnchor anchor(Time steady,std::int64_t unix_ms) {ClockAnchor result;result.steady=steady;result.unix_ms=unix_ms;return result;}
void persisted_budgets() {
    using std::chrono::seconds;
    MemoryVault vault;const auto now=Time{}+std::chrono::hours(1);const std::int64_t wall=1800000000000;
    {
        AttemptBudget budget(vault,"test.attempts",anchor(now,wall));
        check(budget.state(now).remaining==5&&!vault.records.count("test.attempts"),"an untouched budget stores nothing");
        for(unsigned i=0;i<4;++i)budget.failed(now);
        check(vault.records.count("test.attempts")==1,"failures are persisted");
    }
    // A restart: a new object, a new steady clock, ten seconds later on the wall clock.
    const auto later=Time{}+std::chrono::hours(5);
    AttemptBudget restarted(vault,"test.attempts",anchor(later,wall+10000));
    check(restarted.state(later).remaining==1,"restarting refunds no attempts");
    restarted.failed(later);
    check(restarted.state(later).remaining==0&&restarted.state(later).wait_seconds==30,"the fifth failure after a restart locks");
    const auto lockout=restarted.state(later).lockout;
    AttemptBudget again(vault,"test.attempts",anchor(Time{}+std::chrono::hours(9),wall+20000));
    check(again.state(Time{}+std::chrono::hours(9)).wait_seconds==20&&again.state(Time{}+std::chrono::hours(9)).lockout==lockout,"a restart keeps the rest of the wait");
    AttemptBudget other(vault,"test.attempts",anchor(later,wall+10000));
    check(other.state(later+seconds(5)).wait_seconds==25,"every instance sharing the record sees one lockout");
    other.failed(later+seconds(5));check(restarted.state(later+seconds(5)).wait_seconds==25,"failures during a wait change nothing");
    check(other.state(later+seconds(30)).remaining==5&&vault.records.count("test.attempts")==1,"expiry restores attempts and keeps the escalation");
    AttemptBudget third(vault,"test.attempts",anchor(later+seconds(40),wall+50000));
    for(unsigned i=0;i<5;++i)third.failed(later+seconds(40));
    check(third.state(later+seconds(40)).wait_seconds==60&&third.state(later+seconds(40)).lockout!=lockout,"escalation survives restarts");
    third.succeeded();
    check(!vault.records.count("test.attempts")&&third.state(later).remaining==5,"a successful answer leaves no record");
    AttemptBudget fresh(vault,"test.attempts",anchor(now,wall));for(unsigned i=0;i<5;++i)fresh.failed(now);
    AttemptBudget rolled(vault,"test.attempts",anchor(now,wall-86400000));
    check(rolled.state(now).wait_seconds==900,"a clock set back cannot stretch a wait past 900 seconds");
    check(rolled.state(now+seconds(900)).wait_seconds==0&&rolled.state(now+seconds(900)).remaining==5,"the capped wait still ends");
    vault.records["test.attempts"]={1,5,0};
    AttemptBudget damaged(vault,"test.attempts",anchor(now,wall));
    check(damaged.state(now).remaining==0&&damaged.state(now).wait_seconds==900,"a damaged record is the longest wait, never a refund");
    vault.unavailable=true;rejects([&]{damaged.state(now);},"an unavailable vault fails closed");vault.unavailable=false;
    rejects([&]{AttemptBudget invalid(vault,"display name");},"budget accounts are stable identifiers");
    MemoryVault lock_vault;
    {
        LockSession lock({Policy::Pin,Duration::UntilExit,5},AttemptBudget(lock_vault,"test.lock",anchor(now,wall)));
        for(unsigned i=0;i<5;++i)lock.submit(Factor::Pin,false,now);
    }
    LockSession reopened({Policy::Pin,Duration::UntilExit,5},AttemptBudget(lock_vault,"test.lock",anchor(now,wall)));
    check(reopened.attempts(now).wait_seconds==30&&!reopened.expected(now)&&!reopened.submit(Factor::Pin,true,now),"a reopened lock session still waits");
}
void ladder_allowance() {
    using std::chrono::hours;
    MemoryVault vault;const auto now=Time{}+hours(1);const std::int64_t wall=1800000000000;
    LadderAllowance first(vault,shared_ladder_account,anchor(now,wall)),second(vault,shared_ladder_account,anchor(now,wall));
    check(first.remaining(now)==ladder_skips_per_hour&&!vault.records.count(shared_ladder_account),"an unspent allowance stores nothing");
    std::array<AttemptBudget,4> surfaces;
    for(auto& surface:surfaces)for(unsigned i=0;i<5;++i)surface.failed(now);
    check(surfaces[0].clear_wait(first,now)&&surfaces[1].clear_wait(second,now)&&surfaces[2].clear_wait(first,now),"three skips across surfaces");
    check(!surfaces[3].clear_wait(second,now)&&surfaces[3].state(now).wait_seconds>0,"a fourth skip on any surface is refused");
    check(first.remaining(now)==0&&second.remaining(now)==0,"every surface spends one allowance");
    LadderAllowance restarted(vault,shared_ladder_account,anchor(Time{}+hours(7),wall+60000));
    check(restarted.remaining(Time{}+hours(7))==0,"restarting refunds no skips");
    check(first.remaining(now+hours(1))==ladder_skips_per_hour&&!vault.records.count(shared_ladder_account),"skips leave the rolling hour");
    MemoryVault back_vault;LadderAllowance before(back_vault,shared_ladder_account,anchor(now,wall));AttemptBudget budget;
    for(unsigned i=0;i<5;++i)budget.failed(now);
    check(budget.clear_wait(before,now),"a skip within the allowance");
    LadderAllowance back(back_vault,shared_ladder_account,anchor(now,wall-86400000));
    check(back.remaining(now)==2&&back.remaining(now+std::chrono::minutes(59))==2,"a clock set back keeps the skip");
    check(back.remaining(now+hours(1))==3,"it leaves an hour after the clock was set back");
    back_vault.records[shared_ladder_account]={1,9};
    check(back.remaining(now)==0,"a damaged allowance counts as spent");
    vault.unavailable=true;rejects([&]{first.remaining(now);},"an unavailable allowance fails closed");vault.unavailable=false;
    rejects([&]{LadderAllowance invalid(vault,"display name");},"allowance accounts are stable identifiers");
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
    Secret saved(vault.records.at("bambustudio.authenticator.entry."+first));
    store.remove(first);check(store.list().size()==1&&store.list()[0].id==second,"remove only selected entry");
    rejects([&]{store.code(first,59);},"removed entry unavailable");
    store.restore_entry(first,saved);check(store.list().size()==2&&store.code(first,59).current==current,"restore preserves original identity and code");
    rejects([&]{store.restore_entry(first,Secret("invalid snapshot"));},"restore validates snapshot before mutation");
    rejects([&]{AuthenticatorStore invalid(vault,{});},"history callback required");
}
void element_locks() {
    MemoryVault vault;unsigned recorded=0;auto history=[&](IdentityAction action,const std::string&,const Secret& snapshot){check(action==IdentityAction::LockCreated&&snapshot.size()==5,"lock history excludes credentials");++recorded;};
    auto now=Time{}+std::chrono::hours(1);ElementLock first(vault,"11111111111111111111111111111111",history);
    rejects([&]{ElementLock invalid(vault,"display-label",history);},"element identity agrees with history contract");
    check(!first.configured()&&first.allows_action(now),"unconfigured element permitted");
    LockEnrollment enrollment;enrollment.settings={Policy::PasswordPinTotp,Duration::Minutes,1};enrollment.pin=Secret("123456");enrollment.password=Secret("test password answer");
    Enrollment otp;otp.secret=Secret("12345678901234567890");enrollment.confirmation_code=totp(otp.secret,otp.parameters,59).current;enrollment.otp=std::move(otp);
    first.create(std::move(enrollment),59);check(recorded==1&&first.configured()&&!first.allows_action(now),"created element blocks actions");
    check(!first.submit(Secret("test password answer"),now,59)&&first.expected(now)==Factor::Pin,"backend password factor");
    check(!first.submit(Secret("123456"),now,59)&&first.expected(now)==Factor::Totp,"backend PIN factor");
    check(first.submit(Secret("287082"),now,59)&&first.allows_action(now),"backend OTP completes ordered policy");
    first.relock();first.submit(Secret("test password answer"),now,59);first.submit(Secret("123456"),now,59);
    check(!first.submit(Secret("287082"),now,59)&&!first.allows_action(now),"used OTP step cannot replay");
    ElementLock restarted(vault,"11111111111111111111111111111111",history);check(restarted.configured()&&!restarted.allows_action(now),"configured element relocks on restart");
    ElementLock other(vault,"22222222222222222222222222222222",history);check(other.allows_action(now),"lock credentials do not inherit");
    check(restarted.budget()!=nullptr&&other.budget()==nullptr,"only a configured element has a budget");
    for(unsigned i=0;i<5&&!restarted.attempts(now).wait_seconds;++i)restarted.submit(Secret("not the password"),now,59);
    check(restarted.attempts(now).wait_seconds>0,"element attempt budget enforced");
    ElementLock reopened(vault,"11111111111111111111111111111111",history);
    check(reopened.attempts(now).remaining==0&&reopened.attempts(now).wait_seconds>0,"element lockout survives restart");
    check(!reopened.submit(Secret("test password answer"),now,59)&&!reopened.expected(now),"a restarted element still waits");
    check(vault.records.count("bambustudio.element-lock.11111111111111111111111111111111.attempts")==1,"element attempts persist beside the lock");
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
    try { rfc_vectors();parsing();credentials();locks();persisted_budgets();ladder_allowance();encryption();authenticator();element_locks();support_tickets();native_vault();application_vault();std::cout<<"PASS "<<checks<<" local security behavioral checks\n";return 0; }
    catch(const std::exception& e) { std::cerr<<"FAIL after "<<checks<<" checks: "<<e.what()<<'\n';return 1; }
}
