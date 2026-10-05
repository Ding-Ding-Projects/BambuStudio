#pragma once
#include "Schedule.hpp"
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <set>
namespace Slic3r::ScheduledSettings {
struct ServiceHooks {
    // Persist and record history transactionally. No external-response data enters history.
    std::function<bool(const std::string& document)> save;
    std::function<void(const Effective&)> apply;
    std::function<void(State)> notify;
    Fetch fetch;
};
class Service {
public:
    Service(Registry, ServiceHooks);
    ~Service();
    Service(const Service&)=delete;
    Service& operator=(const Service&)=delete;
    bool load(const std::string&,std::string& error);
    bool replace(const Schedule&,std::string& error);
    bool set_base(Values,std::string& error);
    void tick(LocalTime,int64_t monotonic_seconds);
    void refresh();
    const Registry& registry() const { return m_registry; }
    const Schedule& schedule() const { return m_schedule; }
    const Effective& effective() const { return m_effective; }
private:
    struct Job { uint64_t generation; Rule rule; std::shared_ptr<std::atomic_bool> cancel; };
    struct Done { uint64_t generation; Rule rule; FetchResult result; };
    void invalidate();
    void work();
    Registry m_registry;
    ServiceHooks m_hooks;
    Schedule m_schedule;
    Values m_base;
    Effective m_effective;
    std::map<std::string,ExternalValue> m_external;
    std::map<std::string,int64_t> m_due;
    std::set<std::string> m_pending, m_active;
    uint64_t m_generation=1;
    std::shared_ptr<std::atomic_bool> m_cancel;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<Job> m_jobs;
    std::deque<Done> m_done;
    bool m_stop=false;
    std::thread m_worker;
};
// Native transport is optional at link time. An absent transport reports Unavailable.
FetchResult native_fetch(const Source&,const std::string&,const std::atomic_bool&);
bool store_home_credential(const std::string& rule_id,const std::wstring& secret);
bool remove_home_credential(const std::string& rule_id);
struct Zone { std::string id,label; };
std::vector<Zone> timezones();
bool local_time(const std::string& timezone,LocalTime& out);
}
