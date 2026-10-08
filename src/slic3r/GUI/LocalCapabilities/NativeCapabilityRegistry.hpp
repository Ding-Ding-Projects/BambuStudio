#pragma once
#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace Slic3r { namespace GUI { namespace LocalCapabilities {
enum class Capability { SchoolState, SchoolManage, VaultManage, ConverterManage, OllamaManage };
struct Result { bool opened{false}; bool enabled{false}; std::string display_name; };
struct Approval { std::string id, origin; std::vector<Capability> grants; };
struct Offer { std::string endpoint, nonce; int lifetime_seconds{60}; };
struct Rejected : std::runtime_error { explicit Rejected(const char* code) : std::runtime_error(code) {} };

// All methods are GUI-thread-only. No callback receives browser JSON or arguments.
class Registry {
public:
    using Clock = std::chrono::steady_clock;
    using Random = std::function<std::string()>;
    using Now = std::function<Clock::time_point()>;
    // No default-argument lambda here: MSVC cannot resolve the class-scope Clock
    // inside one (C2653, C2440), so the one-argument form delegates instead.
    explicit Registry(Random random) : Registry(std::move(random), Now(&Registry::steady_now)) {}
    Registry(Random random, Now now) : m_owner(std::this_thread::get_id()), m_random(std::move(random)), m_now(std::move(now)) {}
    void register_handler(Capability capability, std::function<bool()> available, std::function<Result()> invoke)
    {
        thread_check();
        if (capability<Capability::SchoolState || capability>Capability::OllamaManage || !available || !invoke || m_handlers.count(capability)) throw Rejected("invalid_registration");
        m_handlers.emplace(capability, Handler{std::move(available),std::move(invoke)});
    }
    std::vector<Capability> available() const
    {
        thread_check(); std::vector<Capability> result;
        for (const auto& entry : m_handlers) if (entry.second.available()) result.push_back(entry.first);
        return result;
    }
    // Call only after the native UI confirms this exact origin and every selected grant.
    Approval approve_pairing(const std::string& origin, const std::vector<Capability>& grants,
                             std::function<void(const Offer&)> show_offer)
    {
        thread_check();
        if (!valid_origin(origin) || grants.empty() || grants.size()>5 || !show_offer) throw Rejected("invalid_pairing");
        std::set<Capability> unique;
        for (auto capability : grants)
            if (!unique.insert(capability).second || !is_available(capability)) throw Rejected("capability_unavailable");
        const auto id=m_random();
        if (!valid_secret(id)) throw Rejected("random_unavailable");
        revoke();
        m_approval={id,origin,grants}; m_expires=m_now()+std::chrono::minutes(10);
        m_show_offer=std::move(show_offer); m_pending=true;
        return m_approval;
    }
    Approval claim_pairing()
    {
        thread_check(); require_active();
        if (!m_pending) throw Rejected("pairing_unavailable");
        m_pending=false; return m_approval;
    }
    void publish_offer(const std::string& id, const Offer& offer)
    {
        thread_check(); require_id(id);
        if (m_pending || m_published || !valid_endpoint(offer.endpoint) || !valid_secret(offer.nonce) ||
            offer.lifetime_seconds<1 || offer.lifetime_seconds>60) throw Rejected("invalid_offer");
        m_published=true;
        m_show_offer(offer);
    }
    bool active(const std::string& id) const
    { thread_check(); return !id.empty() && id==m_approval.id && m_now()<m_expires; }
    Result invoke(const std::string& id, Capability capability)
    {
        thread_check(); require_id(id);
        if (!m_published || std::find(m_approval.grants.begin(),m_approval.grants.end(),capability)==m_approval.grants.end())
            throw Rejected("capability_not_granted");
        if (!is_available(capability)) throw Rejected("capability_unavailable");
        return m_handlers.at(capability).invoke();
    }
    void revoke(const std::string& id)
    { thread_check(); if (id==m_approval.id) revoke(); }
    void revoke()
    {
        thread_check();
        std::fill(m_approval.id.begin(),m_approval.id.end(),'\0'); m_approval={};
        m_show_offer={}; m_pending=false; m_published=false;
    }
    static bool valid_secret(const std::string& value)
    { return value.size()==64 && std::all_of(value.begin(),value.end(),[](unsigned char ch){return (ch>='0'&&ch<='9')||(ch>='A'&&ch<='F');}); }
    static bool valid_endpoint(const std::string& value)
    {
        const std::string prefix="http://127.0.0.1:";
        if (value.compare(0,prefix.size(),prefix)!=0) return false;
        auto port=value.substr(prefix.size());
        if (!port.empty() && port.back()=='/') port.pop_back();
        return valid_port(port);
    }
    static bool valid_origin(const std::string& value)
    {
        if (value.size()>253 || value.compare(0,8,"https://")!=0) return false;
        auto host=value.substr(8); const auto colon=host.find(':');
        if (colon!=std::string::npos) {if(host.substr(colon+1)=="443" || !valid_port(host.substr(colon+1))) return false; host.resize(colon);}
        if(host.empty() || host=="localhost" || host.front()=='.' || host.back()=='.') return false;
        bool has_letter=false; char previous='.';
        for (unsigned char ch:host) {
            if (!((ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='-'||ch=='.')) return false;
            if ((previous=='.'&&(ch=='.'||ch=='-')) || (previous=='-'&&ch=='.')) return false;
            if(ch>='a'&&ch<='z') has_letter=true;
            previous=static_cast<char>(ch);
        }
        return has_letter && host.back()!='-';
    }
private:
    static Clock::time_point steady_now() { return Clock::now(); }
    struct Handler { std::function<bool()> available; std::function<Result()> invoke; };
    std::thread::id m_owner;
    Random m_random;
    Now m_now;
    std::map<Capability,Handler> m_handlers;
    Approval m_approval;
    Clock::time_point m_expires{};
    std::function<void(const Offer&)> m_show_offer;
    bool m_pending{false},m_published{false};
    void thread_check() const {if(std::this_thread::get_id()!=m_owner) throw Rejected("wrong_thread");}
    bool is_available(Capability capability) const
    { const auto found=m_handlers.find(capability); return found!=m_handlers.end() && found->second.available(); }
    void require_active() const {if(m_approval.id.empty()||m_now()>=m_expires) throw Rejected("approval_expired");}
    void require_id(const std::string& id) const {require_active(); if(id!=m_approval.id) throw Rejected("invalid_approval");}
    static bool valid_port(const std::string& text)
    {
        if(text.empty() || text.size()>5 || text.front()=='0') return false;
        unsigned value=0;
        for(unsigned char ch:text) {if(ch<'0'||ch>'9') return false;value=value*10+(ch-'0');}
        return value>0&&value<=65535;
    }
};
// Construct on the GUI thread before starting the native automation listener.
Registry& registry();
}}}
