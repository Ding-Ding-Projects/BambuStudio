#include "Service.hpp"
namespace Slic3r::ScheduledSettings {
Service::Service(Registry r,ServiceHooks h):m_registry(std::move(r)),m_hooks(std::move(h)),m_cancel(std::make_shared<std::atomic_bool>(false)),m_worker([this]{work();}){}
Service::~Service() {
    m_cancel->store(true);{std::lock_guard<std::mutex> lock(m_mutex);m_stop=true;m_jobs.clear();}m_cv.notify_one();m_worker.join();
}
void Service::invalidate() {
    m_cancel->store(true);m_cancel=std::make_shared<std::atomic_bool>(false);++m_generation;
    {std::lock_guard<std::mutex> lock(m_mutex);m_jobs.clear();m_done.clear();}
    m_external.clear();m_due.clear();m_pending.clear();
}
bool Service::load(const std::string& document,std::string& error) {
    Schedule next;if(!parse(document,m_registry,next,error))return false;invalidate();m_schedule=std::move(next);return true;
}
bool Service::replace(const Schedule& next,std::string& error) {
    if(!validate(next,m_registry,error))return false;
    const auto bytes=serialize(next);if(bytes.size()>max_payload){error="Schedule exceeds the storage limit.";return false;}
    if(!m_hooks.save||!m_hooks.save(bytes)){error="Schedules could not be saved. Existing schedules remain active.";return false;}
    invalidate();m_schedule=next;return true;
}
bool Service::set_base(Values next,std::string& error) {if(!m_registry.validate(next,error))return false;m_base=std::move(next);return true;}
void Service::refresh(){invalidate();}
void Service::tick(LocalTime now,int64_t tick) {
    std::set<std::string> active;for(const auto& r:m_schedule.rules)if(r.enabled&&matches(r.window,now))active.insert(r.id);
    if(active!=m_active){invalidate();m_active=std::move(active);}
    std::deque<Done> completed;{std::lock_guard<std::mutex> lock(m_mutex);completed.swap(m_done);}
    for(auto& d:completed) {
        if(d.generation!=m_generation)continue;
        m_pending.erase(d.rule.id);ExternalValue value;value.state=d.result.state;std::string error;
        if(value.state==State::Active) {
            const bool valid=d.rule.source.kind==SourceKind::HttpsApi?parse_api_response(d.result.body,m_registry,value.values,error):parse_home_state(d.result.body,d.rule.source.entity,value.on,error);
            if(!valid)value.state=State::Invalid;
            value.expires_at=tick+d.rule.source.refresh_seconds;
        }
        const auto previous=m_external.find(d.rule.id);
        if(value.state!=State::Active&&(previous==m_external.end()||previous->second.state!=value.state)&&m_hooks.notify)m_hooks.notify(value.state);
        m_external[d.rule.id]=std::move(value);m_due[d.rule.id]=tick+d.rule.source.refresh_seconds;
    }
    for(const auto& r:m_schedule.rules) {
        const bool rule_active=r.enabled&&matches(r.window,now);
        if(!rule_active){m_external.erase(r.id);m_due.erase(r.id);continue;}
        if(r.source.kind==SourceKind::Local||!r.source.consent||m_pending.count(r.id))continue;
        auto due=m_due.find(r.id);if(due!=m_due.end()&&tick<due->second)continue;
        if(!m_hooks.fetch){m_external[r.id]={State::Unavailable};m_due[r.id]=tick+r.source.refresh_seconds;continue;}
        m_pending.insert(r.id);m_due[r.id]=tick+r.source.refresh_seconds;
        {std::lock_guard<std::mutex> lock(m_mutex);m_jobs.push_back({m_generation,r,m_cancel});}m_cv.notify_one();
    }
    auto next=evaluate(m_schedule,m_registry,m_base,now,tick,m_external);
    if(next.values!=m_effective.values||next.winners!=m_effective.winners||next.sources!=m_effective.sources){m_effective=std::move(next);if(m_hooks.apply)m_hooks.apply(m_effective);}
}
void Service::work() {
    for(;;){
        Job job;{std::unique_lock<std::mutex> lock(m_mutex);m_cv.wait(lock,[&]{return m_stop||!m_jobs.empty();});if(m_stop)return;job=std::move(m_jobs.front());m_jobs.pop_front();}
        if(job.cancel->load())continue;
        FetchResult result;try {result=m_hooks.fetch(job.rule.source,job.rule.id,*job.cancel);}catch(...){result.state=State::Offline;}
        if(job.cancel->load())continue;
        if(result.body.size()>max_payload){result.body.clear();result.state=State::Invalid;}
        {std::lock_guard<std::mutex> lock(m_mutex);if(!m_stop)m_done.push_back({job.generation,std::move(job.rule),std::move(result)});}
    }
}
}
