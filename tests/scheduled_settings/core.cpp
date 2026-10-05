#include "libslic3r/ScheduledSettings/Service.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <future>
using namespace Slic3r::ScheduledSettings;
int count=0;
void check(bool b,const char* message){++count;if(!b)throw std::runtime_error(message);}
Rule rule(std::string id="a"){Rule r;r.id=id;r.label="Night theme";r.values={{"theme",std::string("dark")}};return r;}
int main(){try{
    auto reg=default_registry();Schedule s;s.rules={rule()};std::string err;check(validate(s,reg,err),"valid schedule");
    auto bytes=serialize(s);Schedule loaded;check(parse(bytes,reg,loaded,err)&&serialize(loaded)==bytes,"persistent roundtrip");
    auto duplicate=bytes;duplicate.insert(1,"\"version\":1,");check(!parse(duplicate,reg,loaded,err),"duplicate key rejected");
    check(!parse("{\"version\":2}",reg,loaded,err),"unknown version");
    check(!parse(std::string(max_payload+1,'x'),reg,loaded,err),"size bound");
    Values v;check(!parse_api_response("{\"version\":1,\"values\":{\"theme\":\"wrong\"}}",reg,v,err),"typed enum");
    check(!parse_api_response("{\"version\":1,\"values\":{\"password\":\"x\"}}",reg,v,err),"unknown setting");
    check(parse_api_response("{\"version\":1,\"values\":{\"theme\":\"dark\"}}",reg,v,err),"valid API");
    check(!reg.validate({{"funny_level_en",int64_t(0)}},err),"integer lower bound");
    check(!reg.validate({{"dialog_emojis",int64_t(1)}},err),"boolean exact type");
    check(!reg.validate({{"accent",std::string("#GG0000")}},err),"color validation");
    Window w;w.start_minute=1320;w.end_minute=120;w.weekdays=1;w.first=Date{2026,10,5};w.last=w.first;
    check(matches(w,{{2026,10,5},1320}),"overnight start inclusive");
    check(matches(w,{{2026,10,6},119}),"overnight weekday and date anchor");
    check(!matches(w,{{2026,10,6},120}),"end exclusive");
    check(!matches(w,{{2026,10,6},1320}),"wrong anchor day");
    w.start_minute=w.end_minute=600;check(matches(w,{{2026,10,5},10}),"equal means all day");
    check(!valid_date({2025,2,29})&&valid_date({2024,2,29}),"leap date");
    check(day_number(date_from_day(day_number({2026,1,1})))==day_number({2026,1,1}),"calendar roundtrip");
    Values base={{"theme",std::string("light")}};auto r2=rule("b");r2.values["theme"]=std::string("system");s.rules.push_back(r2);
    check(std::get<std::string>(evaluate(s,reg,base,{{2026,10,5},0},0,{}).values["theme"])=="system","later tie wins");
    s.rules[0].priority=2;check(evaluate(s,reg,base,{{2026,10,5},0},0,{}).winners["theme"]=="a","priority wins");
    s.rules[0].enabled=false;s.rules[1].enabled=false;check(evaluate(s,reg,base,{{2026,10,5},0},0,{}).values==base,"base restored");
    Source source;source.kind=SourceKind::HttpsApi;source.url="https://example.com/settings";Endpoint e;check(endpoint(source,e,err),"HTTPS endpoint");
    for(const auto* url:{"http://example.com/","https://user:secret@example.com/","https://example.com/?token=x","https://example.com/../private","file:///settings","https://example.com/%2e"}){source.url=url;check(!endpoint(source,e,err),"unsafe URL rejected");}
    source.url="https://example.com/settings";check(!allowed_address("127.0.0.1",source)&&!allowed_address("169.254.169.254",source)&&!allowed_address("10.0.0.1",source),"SSRF addresses");
    check(!allowed_address("::ffff:127.0.0.1",source)&&!allowed_address("fc00::1",source),"IPv6 SSRF");
    check(allowed_address("93.184.216.34",source),"public address");
    check(!allowed_address("2002:7f00:1::",source)&&!allowed_address("2001::1",source),"IPv6 transition routes rejected");
    check(!allowed_address("192.0.0.1",source)&&!allowed_address("198.51.100.1",source),"special purpose destinations rejected");
    source.kind=SourceKind::HomeAssistant;source.allow_private_network=true;check(allowed_address("192.168.1.2",source)&&!allowed_address("169.254.169.254",source),"explicit HA private consent");
    bool on=false;check(parse_home_state("{\"entity_id\":\"input_boolean.night\",\"state\":\"on\"}","input_boolean.night",on,err)&&on,"HA on");
    check(parse_home_state("{\"entity_id\":\"input_boolean.night\",\"state\":\"off\"}","input_boolean.night",on,err)&&!on,"HA off");
    check(!parse_home_state("{\"entity_id\":\"input_boolean.other\",\"state\":\"on\"}","input_boolean.night",on,err),"HA entity mismatch");
    s.rules={rule()};s.rules[0].source={SourceKind::HttpsApi,"https://example.com/settings","",true,false,false,30};
    std::map<std::string,ExternalValue> ext{{"a",{State::Active,{{"theme",std::string("dark")}},false,30}}};
    check(evaluate(s,reg,base,{{2026,10,5},0},29,ext).winners.count("theme")==1,"fresh response");
    check(evaluate(s,reg,base,{{2026,10,5},0},30,ext).values==base,"stale response fallback");
    ext["a"].values["dialog_emojis"]=true;check(evaluate(s,reg,base,{{2026,10,5},0},0,ext).values==base,"API cannot add unselected setting");
    s.rules[0].source.consent=false;check(evaluate(s,reg,base,{{2026,10,5},0},0,ext).sources["a"]==State::Unauthorized,"source consent required");
    int calls=0;ServiceHooks hooks;hooks.save=[&](const std::string&){return true;};hooks.apply=[&](const Effective&){++calls;};
    Service service(reg,hooks);check(service.set_base(base,err),"base accepted");s.rules={rule()};check(service.replace(s,err),"save accepted");service.tick({{2026,10,5},0},0);check(calls==1,"consumer receives effect");
    s.rules.clear();check(service.replace(s,err),"empty schedule saved");service.tick({{2026,10,5},0},1);check(service.effective().values==base,"consumer restores base");
    hooks.save=[](const std::string&){return false;};Service fail(reg,hooks);s.rules={rule()};check(!fail.replace(s,err)&&fail.schedule().rules.empty(),"failed persistence leaves prior state");
    auto bad=s;bad.rules[0].source.kind=SourceKind(99);check(!validate(bad,reg,err),"invalid enum rejected");
    check(!parse_api_response("{\"version\":1,\"values\":{\"funny_level_en\":18446744073709551615}}",reg,v,err),"unsigned overflow rejected");
    for(int year:{1601,1900,2000,2026,9999})for(int month=1;month<=12;++month){Date d{year,month,1};auto round=date_from_day(day_number(d));check(round.year==year&&round.month==month&&round.day==1,"calendar wide range");}
    std::atomic_int fetched{0};std::promise<void> started;auto started_future=started.get_future();
    ServiceHooks async_hooks;async_hooks.save=[](const std::string&){return true;};
    async_hooks.fetch=[&](const Source&,const std::string&,const std::atomic_bool& cancel){++fetched;started.set_value();while(!cancel.load())std::this_thread::yield();return FetchResult{State::Active,"{\"version\":1,\"values\":{\"theme\":\"dark\"}}"};};
    {Service async(reg,async_hooks);check(async.set_base(base,err),"async base");s.rules={rule()};s.rules[0].source={SourceKind::HttpsApi,"https://example.com/settings","",true,false,false,30};check(async.replace(s,err),"async rule");async.tick({{2026,10,5},0},0);check(started_future.wait_for(std::chrono::seconds(2))==std::future_status::ready,"background worker started");for(int i=0;i<10;++i)async.tick({{2026,10,5},0},i);check(fetched.load()==1,"no overlapping refreshes");s.rules.clear();check(async.replace(s,err),"edit cancels old generation");async.tick({{2026,10,5},0},11);check(async.effective().values==base,"superseded response cannot override base");}
    std::atomic_int completed{0};ServiceHooks ok_hooks;ok_hooks.save=[](const std::string&){return true;};ok_hooks.fetch=[&](const Source&,const std::string&,const std::atomic_bool&){++completed;return FetchResult{State::Active,"{\"version\":1,\"values\":{\"theme\":\"dark\"}}"};};
    {Service remote(reg,ok_hooks);check(remote.set_base(base,err),"remote base");s.rules={rule()};s.rules[0].source={SourceKind::HttpsApi,"https://example.com/settings","",true,false,false,30};check(remote.replace(s,err),"remote schedule");remote.tick({{2026,10,5},0},0);for(int i=0;i<2000&&remote.effective().winners.empty();++i){std::this_thread::sleep_for(std::chrono::milliseconds(1));remote.tick({{2026,10,5},0},1);}check(remote.effective().winners.count("theme")==1,"validated worker response reaches consumer");remote.tick({{2026,10,5},0},2);check(completed.load()==1,"bounded refresh interval");}
    std::cout<<"PASS "<<count<<" scheduled-settings assertions\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<count<<": "<<e.what()<<'\n';return 1;}}
