#include "ElementLock.hpp"
#include <algorithm>

namespace Slic3r::LocalSecurity {
namespace {
void require(bool ok,Error e=Error::InvalidInput){if(!ok)throw Failure(e);}
Secret settings_record(LockSettings s){return Secret(std::vector<unsigned char>{1,static_cast<unsigned char>(s.policy),static_cast<unsigned char>(s.duration),static_cast<unsigned char>(s.minutes>>8),static_cast<unsigned char>(s.minutes)});}
LockSettings settings(const Secret& s){require(s.size()==5&&s.data()[0]==1,Error::Corrupt);LockSettings r{static_cast<Policy>(s.data()[1]),static_cast<Duration>(s.data()[2]),unsigned(s.data()[3])*256+s.data()[4]};LockSession validate(r);return r;}
Secret otp_record(const Enrollment& e){
    require(e.secret.size()>0&&e.secret.size()<=512);std::vector<unsigned char> b{1,static_cast<unsigned char>(e.parameters.algorithm),static_cast<unsigned char>(e.parameters.digits)};
    for(int i=3;i>=0;--i)b.push_back(static_cast<unsigned char>(e.parameters.period>>(8*i)));
    b.insert(b.end(),e.secret.data(),e.secret.data()+e.secret.size());return Secret(std::move(b));
}
}
ElementLock::ElementLock(Vault& vault,std::string id,RecordIdentityMutation history):m_vault(vault),m_id(std::move(id)),m_history(std::move(history)){
    require(valid_stable_id(m_id)&&m_id.size()<=100&&bool(m_history));auto existing=m_vault.read(account("config"));
    if(existing){auto s=settings(*existing);for(auto f:factors(s.policy)){auto value=m_vault.read(account(f==Factor::Pin?"pin":f==Factor::Password?"password":"totp"));require(value.has_value(),Error::Missing);}m_session=std::make_unique<LockSession>(s);}
}
std::string ElementLock::account(const char* suffix) const{return "bambustudio.element-lock."+m_id+"."+suffix;}
bool ElementLock::allows_action(Time now){return !m_session||!m_session->locked(now);}
void ElementLock::create(LockEnrollment enrollment,std::uint64_t seconds){
    require(!m_session&&!m_vault.read(account("config")),Error::Authentication);auto session=std::make_unique<LockSession>(enrollment.settings);
    const auto needed=factors(enrollment.settings.policy);
    if(std::find(needed.begin(),needed.end(),Factor::Totp)!=needed.end())require(enrollment.otp&&verify_totp(enrollment.otp->secret,enrollment.otp->parameters,enrollment.confirmation_code,seconds).has_value(),Error::Authentication);
    Credentials credentials(m_vault);std::vector<std::string> created;
    try{
        for(auto factor:needed){
            const auto name=account(factor==Factor::Pin?"pin":factor==Factor::Password?"password":"totp");require(!m_vault.read(name),Error::Corrupt);
            if(factor==Factor::Totp)m_vault.write(name,otp_record(*enrollment.otp));
            else credentials.enroll(name,factor==Factor::Pin?CredentialKind::Pin:CredentialKind::Password,factor==Factor::Pin?enrollment.pin:enrollment.password);
            created.push_back(name);
        }
        auto record=settings_record(enrollment.settings);m_history(IdentityAction::LockCreated,m_id,record);m_vault.write(account("config"),record);m_session=std::move(session);
    }catch(...){for(const auto& name:created){try{m_vault.erase(name);}catch(...){}}throw;}
}
std::optional<Factor> ElementLock::expected(Time now){return m_session?m_session->expected(now):std::nullopt;}
bool ElementLock::submit(const Secret& answer,Time now,std::uint64_t seconds){
    auto f=expected(now);if(!f)return false;bool verified=false;std::optional<std::uint64_t> step;
    if(*f==Factor::Totp){
        auto raw=m_vault.read(account("totp"));require(raw&&raw->size()>7&&raw->size()<=519&&raw->data()[0]==1&&raw->data()[1]<=2,Error::Corrupt);
        TotpParameters p;p.algorithm=static_cast<Algorithm>(raw->data()[1]);p.digits=raw->data()[2];p.period=0;for(unsigned i=3;i<7;++i)p.period=(p.period<<8)|raw->data()[i];
        Secret secret(std::vector<unsigned char>(raw->data()+7,raw->data()+raw->size()));step=verify_totp(secret,p,std::string_view(reinterpret_cast<const char*>(answer.data()),answer.size()),seconds);
        verified=step&&(!m_last_otp_step||*step>*m_last_otp_step);
    }else{Credentials c(m_vault);verified=c.verify(account(*f==Factor::Pin?"pin":"password"),answer);}
    auto result=m_session->submit(*f,verified,now);if(result&&step)m_last_otp_step=step;return result;
}
AttemptState ElementLock::attempts(Time now){return m_session?m_session->attempts(now):AttemptState{5,0};}
void ElementLock::cancel(){if(m_session)m_session->cancel();}
void ElementLock::leave_surface(){if(m_session)m_session->leave_surface();}
void ElementLock::relock(){if(m_session)m_session->relock();}
}
