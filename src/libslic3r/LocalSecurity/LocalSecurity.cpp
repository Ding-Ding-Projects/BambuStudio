#include "LocalSecurity.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincred.h>
#endif

namespace Slic3r::LocalSecurity {
namespace {
constexpr unsigned iterations = 600000;
constexpr std::size_t max_secret = 2048;
const char* message(Error e) {
    switch (e) {
    case Error::InvalidInput: return "Invalid local security input.";
    case Error::Unavailable: return "The operating-system credential vault is unavailable.";
    case Error::Missing: return "The local credential is unavailable. Use the documented local reset route.";
    case Error::Corrupt: return "The local security record is invalid. Use the documented recovery route.";
    case Error::Authentication: return "The answer did not match.";
    case Error::RateLimited: return "Wait before trying again.";
    case Error::History: return "Local history was not recorded. The change is incomplete.";
    }
    return "Local security operation failed.";
}
void require(bool ok, Error e = Error::InvalidInput) { if (!ok) throw Failure(e); }
void check_parameters(const TotpParameters& p) {
    require(p.digits >= 6 && p.digits <= 8 && p.period > 0 && p.period <= 86400);
}
const EVP_MD* digest(Algorithm a) {
    switch(a) { case Algorithm::Sha1: return EVP_sha1(); case Algorithm::Sha256: return EVP_sha256(); case Algorithm::Sha512: return EVP_sha512(); }
    throw Failure(Error::InvalidInput);
}
void put32(unsigned char* p, unsigned value) {
    p[0] = static_cast<unsigned char>(value >> 24); p[1] = static_cast<unsigned char>(value >> 16);
    p[2] = static_cast<unsigned char>(value >> 8); p[3] = static_cast<unsigned char>(value);
}
unsigned get32(const unsigned char* p) { return (unsigned(p[0]) << 24) | (unsigned(p[1]) << 16) | (unsigned(p[2]) << 8) | p[3]; }
std::string hex(const unsigned char* p, std::size_t n) {
    static constexpr char chars[] = "0123456789abcdef";
    std::string result; result.reserve(n * 2);
    for (std::size_t i = 0; i < n; ++i) { result += chars[p[i] >> 4]; result += chars[p[i] & 15]; }
    return result;
}
void validate_answer(CredentialKind kind, const Secret& answer) {
    if (kind == CredentialKind::Pin) {
        require(answer.size() >= 4 && answer.size() <= 32);
        for (std::size_t i = 0; i < answer.size(); ++i) require(answer.data()[i] >= '0' && answer.data()[i] <= '9');
    } else {
        require(kind == CredentialKind::Password && answer.size() >= 8 && answer.size() <= 1024);
    }
}
// v1 | kind | work factor | salt(16) | derived key(32) | generation(16).
constexpr std::size_t credential_size = 70;
void validate_record(const Secret& r) {
    require(r.size() == credential_size, Error::Corrupt);
    require(r.data()[0] == 1 && r.data()[1] <= 1 && get32(r.data() + 2) == iterations, Error::Corrupt);
}
Secret derive(const Secret& answer, const unsigned char* salt) {
    require(answer.size() <= 1024);
    Secret result(std::vector<unsigned char>(32));
    require(PKCS5_PBKDF2_HMAC(reinterpret_cast<const char*>(answer.data()), static_cast<int>(answer.size()),
                             salt, 16, iterations, EVP_sha256(), 32, result.data()) == 1, Error::Unavailable);
    return result;
}
std::string percent_encode(std::string_view s) {
    constexpr char h[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out += c;
        else { out += '%'; out += h[c >> 4]; out += h[c & 15]; }
    }
    return out;
}
std::string percent_decode(std::string_view s) {
    auto nibble = [](char c) -> unsigned { if (c >= '0' && c <= '9') return c-'0'; if(c >= 'a' && c <= 'f') return c-'a'+10; if(c >= 'A' && c <= 'F') return c-'A'+10; throw Failure(Error::InvalidInput); };
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == '%') { require(i + 2 < s.size()); c = static_cast<unsigned char>((nibble(s[i+1]) << 4) | nibble(s[i+2])); i += 2; }
        require(c >= 32 && c != 127); out += c;
    }
    require(out.size() <= 1024); return out;
}
unsigned number(std::string_view s) {
    require(!s.empty() && s.size() <= 5); unsigned n = 0;
    for(char c : s) { require(c >= '0' && c <= '9'); n = n * 10 + unsigned(c-'0'); }
    return n;
}
}

Failure::Failure(Error error) : std::runtime_error(message(error)), m_code(error) {}
Secret::Secret(std::string_view v) : m_bytes(v.begin(), v.end()) { require(v.size() <= 1024 * 1024); }
Secret::Secret(std::vector<unsigned char> v) : m_bytes(std::move(v)) { require(m_bytes.size() <= 1024 * 1024); }
Secret::~Secret() { if (!m_bytes.empty()) OPENSSL_cleanse(m_bytes.data(), m_bytes.size()); }
Secret::Secret(Secret&& v) noexcept : m_bytes(std::move(v.m_bytes)) {}
Secret& Secret::operator=(Secret&& v) noexcept {
    if (this != &v) { if (!m_bytes.empty()) OPENSSL_cleanse(m_bytes.data(), m_bytes.size()); m_bytes = std::move(v.m_bytes); }
    return *this;
}
bool valid_stable_id(std::string_view s) noexcept {
    if (s.empty() || s.size() > 180) return false;
    for (unsigned char c : s) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_')) return false;
    return true;
}
Secret random_secret(std::size_t bytes) {
    require(bytes >= 16 && bytes <= max_secret);
    Secret result(std::vector<unsigned char>(bytes, 0));
    require(RAND_bytes(result.data(), static_cast<int>(bytes)) == 1, Error::Unavailable); return result;
}
std::string new_stable_id() { auto bytes = random_secret(16); return hex(bytes.data(), bytes.size()); }

namespace {
class OsVault final : public Vault {
#ifdef _WIN32
    static std::wstring target(const std::string& account) {
        require(valid_stable_id(account)); return L"DingDing.LocalSecurity.v1/" + std::wstring(account.begin(), account.end());
    }
#endif
public:
    std::optional<Secret> read(const std::string& account) override {
        require(valid_stable_id(account));
#ifdef _WIN32
        auto name = target(account); PCREDENTIALW raw = nullptr;
        if (!CredReadW(name.c_str(), CRED_TYPE_GENERIC, 0, &raw)) {
            if (GetLastError() == ERROR_NOT_FOUND) return std::nullopt;
            throw Failure(Error::Unavailable);
        }
        struct Free { void operator()(CREDENTIALW* p) { if (p) { if(p->CredentialBlob) SecureZeroMemory(p->CredentialBlob, p->CredentialBlobSize); CredFree(p); } } };
        std::unique_ptr<CREDENTIALW, Free> holder(raw);
        require(raw->CredentialBlobSize > 0 && raw->CredentialBlobSize <= max_secret, Error::Corrupt);
        return Secret(std::vector<unsigned char>(raw->CredentialBlob, raw->CredentialBlob + raw->CredentialBlobSize));
#else
        throw Failure(Error::Unavailable);
#endif
    }
    void write(const std::string& account, const Secret& secret) override {
        require(valid_stable_id(account) && secret.size() > 0 && secret.size() <= max_secret);
#ifdef _WIN32
        auto name = target(account); CREDENTIALW record{}; record.Type = CRED_TYPE_GENERIC;
        record.TargetName = name.data(); record.Persist = CRED_PERSIST_LOCAL_MACHINE;
        record.CredentialBlobSize = static_cast<DWORD>(secret.size()); record.CredentialBlob = const_cast<BYTE*>(secret.data());
        require(CredWriteW(&record, 0) != 0, Error::Unavailable);
#else
        throw Failure(Error::Unavailable);
#endif
    }
    void erase(const std::string& account) override {
        require(valid_stable_id(account));
#ifdef _WIN32
        auto name = target(account);
        if (!CredDeleteW(name.c_str(), CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND) throw Failure(Error::Unavailable);
#else
        throw Failure(Error::Unavailable);
#endif
    }
};
}
std::unique_ptr<Vault> make_os_vault() { return std::make_unique<OsVault>(); }
CredentialMetadata Credentials::metadata(const std::string& account) {
    auto r = m_vault.read(account); if (!r) return {};
    validate_record(*r); return {true, hex(r->data()+54,16)};
}
CredentialMetadata Credentials::store(const std::string& account, CredentialKind kind, const Secret& answer) {
    require(valid_stable_id(account)); validate_answer(kind, answer);
    Secret record(std::vector<unsigned char>(credential_size, 0)); record.data()[0]=1;
    record.data()[1] = kind == CredentialKind::Pin ? 0 : 1; put32(record.data()+2, iterations);
    auto salt = random_secret(16); auto generation = random_secret(16);
    std::memcpy(record.data()+6,salt.data(),16); auto hash = derive(answer, salt.data());
    std::memcpy(record.data()+22,hash.data(),32); std::memcpy(record.data()+54,generation.data(),16);
    m_vault.write(account,record); return {true,hex(generation.data(),16)};
}
CredentialMetadata Credentials::enroll(const std::string& account, CredentialKind kind, const Secret& answer) {
    require(!m_vault.read(account).has_value(), Error::Authentication); return store(account,kind,answer);
}
bool Credentials::verify(const std::string& account, const Secret& answer) {
    auto r = m_vault.read(account); require(r.has_value(),Error::Missing); validate_record(*r);
    if (answer.size() > 1024) return false;
    auto candidate = derive(answer,r->data()+6); return CRYPTO_memcmp(candidate.data(),r->data()+22,32)==0;
}
CredentialMetadata Credentials::replace(const std::string& account, const Secret& old_answer, CredentialKind kind, const Secret& answer) {
    require(verify(account,old_answer),Error::Authentication); return store(account,kind,answer);
}
void Credentials::reset(const std::string& account, const Secret& answer) {
    require(verify(account,answer),Error::Authentication); m_vault.erase(account);
}

Secret decode_base32(std::string_view encoded) {
    require(!encoded.empty() && encoded.size() <= 2048);
    std::vector<unsigned char> out; unsigned acc=0, bits=0; bool padded=false; unsigned padding=0, symbols=0;
    for(unsigned char c:encoded) {
        if (c==' ' || c=='-') continue;
        if(c=='=') { padded=true; ++padding; continue; }
        require(!padded); if(c>='a' && c<='z') c=static_cast<unsigned char>(c-'a'+'A');
        unsigned v; if(c>='A' && c<='Z') v=c-'A'; else if(c>='2' && c<='7') v=c-'2'+26; else throw Failure(Error::InvalidInput);
        ++symbols; acc=(acc<<5)|v; bits+=5;
        if(bits>=8) { bits-=8; out.push_back(static_cast<unsigned char>(acc>>bits)); acc &= (1u<<bits)-1; }
    }
    require(symbols%8!=1 && symbols%8!=3 && symbols%8!=6 && acc==0);
    if(padding) require((symbols+padding)%8==0 && padding<7);
    require(!out.empty() && out.size()<=1024); return Secret(std::move(out));
}
std::string encode_base32(const Secret& s) {
    require(s.size()>0 && s.size()<=1024); constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string out; unsigned acc=0,bits=0;
    for(std::size_t i=0;i<s.size();++i) { acc=(acc<<8)|s.data()[i]; bits+=8; while(bits>=5) {bits-=5;out+=alphabet[(acc>>bits)&31];} acc &= (1u<<bits)-1; }
    if(bits) out+=alphabet[(acc<<(5-bits))&31];
    return out;
}
std::string hotp(const Secret& secret,std::uint64_t counter,Algorithm a,unsigned digits) {
    require(secret.size()>0 && secret.size()<=1024 && digits>=6 && digits<=8);
    unsigned char c[8]; for(int i=7;i>=0;--i) {c[i]=static_cast<unsigned char>(counter);counter>>=8;}
    Secret hash(std::vector<unsigned char>(EVP_MAX_MD_SIZE,0)); unsigned length=0;
    require(HMAC(digest(a),secret.data(),static_cast<int>(secret.size()),c,8,hash.data(),&length)!=nullptr,Error::Unavailable);
    const unsigned offset=hash.data()[length-1]&15;
    const unsigned binary=get32(hash.data()+offset)&0x7fffffff; unsigned mod=1; for(unsigned i=0;i<digits;++i)mod*=10;
    auto value=std::to_string(binary%mod); return std::string(digits-value.size(),'0')+value;
}
TotpCode totp(const Secret& s,const TotpParameters& p,std::uint64_t seconds) {
    check_parameters(p); auto step=seconds/p.period;
    require(step<std::numeric_limits<std::uint64_t>::max());
    return {hotp(s,step,p.algorithm,p.digits),hotp(s,step+1,p.algorithm,p.digits),p.period-static_cast<unsigned>(seconds%p.period)};
}
std::optional<std::uint64_t> verify_totp(const Secret& s,const TotpParameters& p,std::string_view code,std::uint64_t seconds,unsigned skew) {
    check_parameters(p); require(skew<=2); if(code.size()!=p.digits)return {};
    for(char c:code)if(c<'0'||c>'9')return {};
    auto step=seconds/p.period; std::optional<std::uint64_t> match;
    for(int delta=-static_cast<int>(skew);delta<=static_cast<int>(skew);++delta) {
        if(delta<0 && step<static_cast<unsigned>(-delta))continue;
        if(delta>0 && step>std::numeric_limits<std::uint64_t>::max()-delta)continue;
        auto candidate=step+delta; auto expected=hotp(s,candidate,p.algorithm,p.digits);
        if(CRYPTO_memcmp(expected.data(),code.data(),p.digits)==0)match=candidate;
        OPENSSL_cleanse(expected.data(),expected.size());
    }
    return match;
}
Enrollment parse_otpauth(std::string_view uri) {
    require(uri.size()<=8192 && uri.substr(0,15)=="otpauth://totp/" && uri.find('#')==std::string_view::npos);
    auto q=uri.find('?'); require(q!=std::string_view::npos); auto label=percent_decode(uri.substr(15,q-15));
    require(!label.empty()); std::map<std::string,std::string> params;
    auto query=uri.substr(q+1);
    while(!query.empty()) {
        auto end=query.find('&'); auto field=query.substr(0,end); auto equal=field.find('='); require(equal!=std::string_view::npos);
        auto key=percent_decode(field.substr(0,equal)),value=percent_decode(field.substr(equal+1));
        require(key=="secret"||key=="issuer"||key=="algorithm"||key=="digits"||key=="period");
        require(params.emplace(key,std::move(value)).second);
        if(end==std::string_view::npos)break;
        query.remove_prefix(end+1); require(!query.empty());
    }
    require(params.count("secret")!=0);
    Enrollment result; result.secret=decode_base32(params.at("secret"));
    OPENSSL_cleanse(params.at("secret").data(),params.at("secret").size());
    auto colon=label.find(':'); result.account=colon==std::string::npos?label:label.substr(colon+1);
    result.issuer=colon==std::string::npos?std::string{}:label.substr(0,colon);
    if(params.count("issuer")) { require(result.issuer.empty()||result.issuer==params.at("issuer"));result.issuer=params.at("issuer"); }
    require(!result.account.empty() && result.account.size()<=256 && result.issuer.size()<=256);
    if(params.count("algorithm")) {auto a=params.at("algorithm"); if(a=="SHA1")result.parameters.algorithm=Algorithm::Sha1;else if(a=="SHA256")result.parameters.algorithm=Algorithm::Sha256;else if(a=="SHA512")result.parameters.algorithm=Algorithm::Sha512;else throw Failure(Error::InvalidInput);}
    if(params.count("digits"))result.parameters.digits=number(params.at("digits"));
    if(params.count("period"))result.parameters.period=number(params.at("period"));
    check_parameters(result.parameters); return result;
}
std::string pairing_uri(const Enrollment& e) {
    check_parameters(e.parameters); require(!e.account.empty()&&e.account.size()<=256&&e.issuer.size()<=256);
    std::string a=e.parameters.algorithm==Algorithm::Sha1?"SHA1":e.parameters.algorithm==Algorithm::Sha256?"SHA256":"SHA512";
    return "otpauth://totp/"+percent_encode(e.issuer.empty()?e.account:e.issuer+":"+e.account)+"?secret="+encode_base32(e.secret)+"&issuer="+percent_encode(e.issuer)+"&algorithm="+a+"&digits="+std::to_string(e.parameters.digits)+"&period="+std::to_string(e.parameters.period);
}
std::vector<Factor> factors(Policy p) {
    switch(p) {case Policy::Pin:return {Factor::Pin};case Policy::Password:return {Factor::Password};case Policy::PinPassword:return {Factor::Pin,Factor::Password};case Policy::PasswordTotp:return {Factor::Password,Factor::Totp};case Policy::PinTotp:return {Factor::Pin,Factor::Totp};case Policy::PasswordPinTotp:return {Factor::Password,Factor::Pin,Factor::Totp};}
    throw Failure(Error::InvalidInput);
}
AttemptState AttemptBudget::state(Time now) {
    if(m_wait_until!=Time{} && now>=m_wait_until) {m_wait_until={};m_remaining=5;}
    unsigned seconds=0; if(m_wait_until>now)seconds=static_cast<unsigned>(std::chrono::duration_cast<std::chrono::seconds>(m_wait_until-now).count()+1);
    return {m_remaining,seconds};
}
void AttemptBudget::failed(Time now) {
    if(state(now).wait_seconds)return;
    if(m_remaining)--m_remaining;
    if(!m_remaining) {unsigned seconds=std::min(900u,30u<<std::min(m_escalation,5u));m_escalation=std::min(5u,m_escalation+1);m_wait_until=now+std::chrono::seconds(seconds);}
}
void AttemptBudget::succeeded() {m_remaining=5;m_escalation=0;m_wait_until={};}
bool AttemptBudget::clear_wait(Time now) {
    if(!state(now).wait_seconds)return false;
    m_skips.erase(std::remove_if(m_skips.begin(),m_skips.end(),[now](Time t){return now-t>=std::chrono::hours(1);}),m_skips.end());
    if(m_skips.size()>=3)return false;
    m_skips.push_back(now);m_wait_until={};m_remaining=5;return true;
}
LockSession::LockSession(LockSettings settings):m_settings(settings),m_factors(factors(settings.policy)) {
    require(settings.duration==Duration::ThisSurface||settings.duration==Duration::Minutes||settings.duration==Duration::UntilExit);
    require(settings.minutes>=1 && settings.minutes<=1440);
}
bool LockSession::locked(Time now) { if(m_unlocked&&m_settings.duration==Duration::Minutes&&now>=m_until)relock();return !m_unlocked; }
std::optional<Factor> LockSession::expected(Time now) {
    if(!locked(now)||m_budget.state(now).wait_seconds)return {};
    if(m_step && now>=m_attempt_until)cancel();
    return m_factors[m_step];
}
bool LockSession::submit(Factor f,bool verified,Time now) {
    auto e=expected(now);if(!e)return false;
    if(f!=*e||!verified) {cancel();m_budget.failed(now);return false;}
    if(!m_step)m_attempt_until=now+std::chrono::seconds(120);
    if(++m_step!=m_factors.size())return false;
    m_step=0;m_unlocked=true;m_until=now+std::chrono::minutes(m_settings.minutes);m_budget.succeeded();return true;
}
void LockSession::cancel(){m_step=0;m_attempt_until={};}
void LockSession::leave_surface(){if(m_settings.duration==Duration::ThisSurface)relock();else cancel();}
void LockSession::relock(){m_unlocked=false;m_until={};cancel();}

std::vector<unsigned char> encrypt_snapshot(const Secret& key,const Secret& plain,std::string_view id) {
    require(key.size()==32&&plain.size()<=1024*1024&&valid_stable_id(id));
    auto nonce=random_secret(16); std::vector<unsigned char> out(1+12+plain.size()+16);out[0]=1;std::memcpy(out.data()+1,nonce.data(),12);
    std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);require(bool(ctx),Error::Unavailable);
    int n=0,total=0;
    require(EVP_EncryptInit_ex(ctx.get(),EVP_aes_256_gcm(),nullptr,key.data(),out.data()+1)==1,Error::Unavailable);
    require(EVP_EncryptUpdate(ctx.get(),nullptr,&n,reinterpret_cast<const unsigned char*>(id.data()),static_cast<int>(id.size()))==1,Error::Unavailable);
    require(EVP_EncryptUpdate(ctx.get(),out.data()+13,&n,plain.data(),static_cast<int>(plain.size()))==1,Error::Unavailable);total=n;
    require(EVP_EncryptFinal_ex(ctx.get(),out.data()+13+total,&n)==1,Error::Unavailable);total+=n;
    require(static_cast<std::size_t>(total)==plain.size(),Error::Unavailable);
    require(EVP_CIPHER_CTX_ctrl(ctx.get(),EVP_CTRL_GCM_GET_TAG,16,out.data()+13+total)==1,Error::Unavailable);return out;
}
Secret decrypt_snapshot(const Secret& key,const std::vector<unsigned char>& encrypted,std::string_view id) {
    require(key.size()==32&&encrypted.size()>=29&&encrypted.size()<=1024*1024+29&&encrypted[0]==1&&valid_stable_id(id),Error::Corrupt);
    auto bytes=encrypted.size()-29;Secret out(std::vector<unsigned char>(bytes+16,0));
    std::unique_ptr<EVP_CIPHER_CTX,decltype(&EVP_CIPHER_CTX_free)> ctx(EVP_CIPHER_CTX_new(),EVP_CIPHER_CTX_free);require(bool(ctx),Error::Unavailable);
    int n=0,total=0;
    require(EVP_DecryptInit_ex(ctx.get(),EVP_aes_256_gcm(),nullptr,key.data(),encrypted.data()+1)==1,Error::Unavailable);
    require(EVP_DecryptUpdate(ctx.get(),nullptr,&n,reinterpret_cast<const unsigned char*>(id.data()),static_cast<int>(id.size()))==1,Error::Unavailable);
    require(EVP_DecryptUpdate(ctx.get(),out.data(),&n,encrypted.data()+13,static_cast<int>(bytes))==1,Error::Corrupt);total=n;
    require(EVP_CIPHER_CTX_ctrl(ctx.get(),EVP_CTRL_GCM_SET_TAG,16,const_cast<unsigned char*>(encrypted.data()+13+bytes))==1,Error::Corrupt);
    require(EVP_DecryptFinal_ex(ctx.get(),out.data()+total,&n)==1,Error::Corrupt);total+=n;
    require(static_cast<std::size_t>(total)==bytes,Error::Corrupt);
    return Secret(std::vector<unsigned char>(out.data(),out.data()+total));
}
} // namespace Slic3r::LocalSecurity
