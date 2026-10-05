#include "Schedule.hpp"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <set>
#include <stdexcept>
#include <sstream>
#include <limits>
#include <cstdio>

namespace Slic3r::ScheduledSettings {
namespace {
using Json=nlohmann::json;
bool bounded(const std::string& s,size_t n) { return s.size()<=n && std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32 || c==127;}); }
bool id_ok(const std::string& s) { return !s.empty() && s.size()<=80 && std::all_of(s.begin(),s.end(),[](unsigned char c){return std::isalnum(c)||c=='-'||c=='_'||c=='.';}); }
bool keys(const Json& j,std::initializer_list<const char*> allowed) {
    if(!j.is_object()) return false;
    for(auto it=j.begin();it!=j.end();++it) if(std::none_of(allowed.begin(),allowed.end(),[&](const char* k){return it.key()==k;})) return false;
    return true;
}
Json strict_json(const std::string& bytes) {
    if(bytes.empty()||bytes.size()>max_payload) throw std::runtime_error("Payload must contain 1 to 65536 bytes.");
    std::vector<std::set<std::string>> stack;
    bool bad=false;
    auto cb=[&](int depth,Json::parse_event_t ev,Json& parsed) {
        if(depth>12) throw std::runtime_error("Document nesting exceeds 12 levels.");
        if(ev==Json::parse_event_t::object_start) stack.emplace_back();
        if(ev==Json::parse_event_t::key) {
            const auto k=parsed.get<std::string>();
            if(stack.empty()||!stack.back().insert(k).second||k=="__proto__"||k=="constructor"||k=="prototype") bad=true;
        }
        if(ev==Json::parse_event_t::object_end) stack.pop_back();
        return true;
    };
    auto j=Json::parse(bytes,cb);
    if(bad) throw std::runtime_error("Duplicate or unsafe field.");
    return j;
}
Values values_from(const Json& j) {
    if(!j.is_object()||j.size()>256) throw std::runtime_error("Settings must be a bounded object.");
    Values v;
    for(auto it=j.begin();it!=j.end();++it) {
        if(it.value().is_boolean()) v[it.key()]=it.value().get<bool>();
        else if(it.value().is_number_integer()) { if(it.value().is_number_unsigned() && it.value().get<uint64_t>()>uint64_t(std::numeric_limits<int64_t>::max())) throw std::runtime_error("Integer exceeds its range."); v[it.key()]=it.value().get<int64_t>(); }
        else if(it.value().is_number_float()) v[it.key()]=it.value().get<double>();
        else if(it.value().is_string()) v[it.key()]=it.value().get<std::string>();
        else throw std::runtime_error("Only typed scalar settings are accepted.");
    }
    return v;
}
Json values_json(const Values& v) { Json j=Json::object();for(const auto& p:v) std::visit([&](const auto& x){j[p.first]=x;},p.second);return j; }
Json date_json(const std::optional<Date>& d) { return d ? Json::array({d->year,d->month,d->day}):Json(nullptr); }
std::optional<Date> read_date(const Json& j) {
    if(j.is_null()) return {};
    if(!j.is_array()||j.size()!=3) throw std::runtime_error("Dates require year, month and day.");
    for(const auto& x:j) if(!x.is_number_integer() || x.get<long double>()<0 || x.get<long double>()>9999) throw std::runtime_error("Date components must be integers.");
    return Date{j[0].get<int>(),j[1].get<int>(),j[2].get<int>()};
}
int integer(const Json& j,const char* k) { const auto& v=j.at(k);if(!v.is_number_integer() || v.get<long double>()<std::numeric_limits<int>::min() || v.get<long double>()>std::numeric_limits<int>::max()) throw std::runtime_error("Expected bounded integer.");return v.get<int>(); }
}
void Registry::add(Descriptor d) {
    if(!id_ok(d.key)||m_entries.count(d.key)) throw std::invalid_argument("Invalid or duplicate setting descriptor.");
    m_entries.emplace(d.key,std::move(d));
}
bool Registry::validate(const Values& values,std::string& error) const {
    if(values.size()>256) {error="Too many settings.";return false;}
    for(const auto& p:values) {
        const auto it=m_entries.find(p.first);
        if(it==m_entries.end()) {error="Unknown setting: "+p.first;return false;}
        const auto& d=it->second;
        bool ok=false;
        if(d.kind==Kind::Boolean) ok=std::holds_alternative<bool>(p.second);
        else if(d.kind==Kind::Integer) {if(auto n=std::get_if<int64_t>(&p.second)) ok=*n>=d.minimum&&*n<=d.maximum;}
        else if(d.kind==Kind::Number) {
            double n=std::holds_alternative<double>(p.second)?std::get<double>(p.second):std::holds_alternative<int64_t>(p.second)?double(std::get<int64_t>(p.second)):NAN;
            ok=std::isfinite(n)&&n>=d.minimum&&n<=d.maximum;
        } else if(auto s=std::get_if<std::string>(&p.second)) {
            ok=bounded(*s,d.max_length);
            if(d.kind==Kind::Choice) ok=ok&&std::find(d.choices.begin(),d.choices.end(),*s)!=d.choices.end();
            if(d.kind==Kind::Color) ok=ok&&s->size()==7&&(*s)[0]=='#'&&std::all_of(s->begin()+1,s->end(),[](unsigned char c){return std::isxdigit(c)!=0;});
        }
        if(!ok) {error="Value outside the declared type or range: "+p.first;return false;}
    }
    error.clear();return true;
}
Registry default_registry() {
    Registry r;
    r.add({"language_mode","Language","語言",Kind::Choice,0,0,{"en","yue_HK","bilingual_en_yue_HK"}});
    r.add({"theme","Theme","主題",Kind::Choice,0,0,{"system","light","dark"}});
    r.add({"density","Density","密度",Kind::Choice,0,0,{"comfortable","compact"}});
    r.add({"accent","Accent color","重點顏色",Kind::Color});
    r.add({"font_family","Font family","字體",Kind::Text});
    r.add({"font_size","Font size","字體大小",Kind::Number,8,48});
    r.add({"reduced_motion","Reduced motion","減少動態效果",Kind::Boolean});
    r.add({"display_name","Display name","顯示名稱",Kind::Text,0,0,{},128});
    r.add({"funny_level_en","English tone","英文語氣",Kind::Integer,1,5});
    r.add({"funny_level_yue","Cantonese tone","廣東話語氣",Kind::Integer,1,5});
    r.add({"dialog_emojis","Dialog decorations","對話框裝飾",Kind::Boolean});
    r.add({"narrator_enabled","Narrator enabled","啟用朗讀",Kind::Boolean});
    r.add({"narrator_language","Narrator language","朗讀語言",Kind::Choice,0,0,{"en","yue_HK","both"}});
    r.add({"narrator_voice_en","English narrator voice","英文朗讀聲音",Kind::Text});
    r.add({"narrator_voice_yue","Cantonese narrator voice","廣東話朗讀聲音",Kind::Text});
    for(const auto* key:{"narrator_rate_en","narrator_rate_yue","narrator_pitch_en","narrator_pitch_yue"}) r.add({key,key,"朗讀速度或音調",Kind::Integer,-10,10});
    return r;
}
bool valid_date(Date d) {
    if(d.year<1601||d.year>9999||d.month<1||d.month>12||d.day<1)return false;
    const int md[]={31,28,31,30,31,30,31,31,30,31,30,31};
    const bool leap=d.year%4==0&&(d.year%100!=0||d.year%400==0);
    return d.day<=md[d.month-1]+(d.month==2&&leap?1:0);
}
int64_t day_number(Date d) {
    int y=d.year-(d.month<=2);const int era=(y>=0?y:y-399)/400;
    const unsigned yoe=unsigned(y-era*400),mp=unsigned(d.month+(d.month>2?-3:9));
    return int64_t(era)*146097+yoe*365+yoe/4-yoe/100+(153*mp+2)/5+d.day-1-719468;
}
Date date_from_day(int64_t z) {
    z+=719468;const int64_t era=(z>=0?z:z-146096)/146097;const unsigned doe=unsigned(z-era*146097);
    const unsigned yoe=(doe-doe/1460+doe/36524-doe/146096)/365;int y=int(yoe+era*400);
    const unsigned doy=doe-(365*yoe+yoe/4-yoe/100),mp=(5*doy+2)/153;
    const int day=int(doy-(153*mp+2)/5+1),month=int(mp)+(mp<10?3:-9);y+=(month<=2);
    return {y,month,day};
}
int weekday(Date d) {return int((day_number(d)+3)%7+7)%7;}
bool matches(const Window& w,LocalTime now) {
    if(!valid_date(now.date)||now.minute<0||now.minute>=1440||w.weekdays==0)return false;
    auto anchor=now.date;
    bool active=w.start_minute==w.end_minute;
    if(w.start_minute<w.end_minute) active=now.minute>=w.start_minute&&now.minute<w.end_minute;
    if(w.start_minute>w.end_minute) {
        active=now.minute>=w.start_minute||now.minute<w.end_minute;
        if(now.minute<w.end_minute) anchor=date_from_day(day_number(now.date)-1);
    }
    const auto day=day_number(anchor);
    return active&&(w.weekdays&(1u<<weekday(anchor)))&&(!w.first||day>=day_number(*w.first))&&(!w.last||day<=day_number(*w.last));
}
bool endpoint(const Source& s,Endpoint& e,std::string& error) {
    const auto fail=[&](){error="Use an explicitly approved HTTPS endpoint without credentials, queries, fragments or redirects.";return false;};
    if(s.url.empty()||s.url.size()>2048||s.url.find_first_of("@?#\\\r\n\t ")!=std::string::npos)return fail();
    const auto split=s.url.find("://");if(split==std::string::npos)return fail();
    e.scheme=s.url.substr(0,split);auto rest=s.url.substr(split+3);auto slash=rest.find('/');
    auto authority=rest.substr(0,slash);e.path=slash==std::string::npos?"/":rest.substr(slash);
    // DNS/IPv4 names only. Numeric IPv6 results remain supported by the resolver.
    if(authority.empty()||authority.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-:")!=std::string::npos)return fail();
    auto colon=authority.find(':');e.host=authority.substr(0,colon);e.port=e.scheme=="https"?443:80;
    std::transform(e.host.begin(),e.host.end(),e.host.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(e.host.empty()||e.host.back()=='.'||e.host.front()=='.'||e.host.find("..")!=std::string::npos)return fail();
    if(colon!=std::string::npos) {
        auto p=authority.substr(colon+1);if(p.empty()||p.size()>5||p.find_first_not_of("0123456789")!=std::string::npos)return fail();
        e.port=unsigned(std::stoul(p));if(e.port==0||e.port>65535)return fail();
    }
    if(e.path.find('%')!=std::string::npos||e.path.find("..")!=std::string::npos)return fail();
    if(e.scheme!="https"&&!(e.scheme=="http"&&s.loopback_development&&(e.host=="127.0.0.1"||e.host=="localhost")))return fail();
    if(s.kind==SourceKind::HomeAssistant) {
        const auto dot=s.entity.find('.');
        if(dot==std::string::npos||(s.entity.substr(0,dot)!="binary_sensor"&&s.entity.substr(0,dot)!="input_boolean"))return fail();
        const auto name=s.entity.substr(dot+1);
        if(name.empty()||name.size()>128||name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_")!=std::string::npos)return fail();
        if(e.path!="/")return fail();
    }
    error.clear();return true;
}
bool allowed_address(const std::string& ip,const Source& s) {
    // Explicit loopback development is the only loopback exception. Metadata/link-local,
    // multicast and reserved targets are always refused, even for Home Assistant.
    unsigned a,b,c,d;char tail;
    if(std::sscanf(ip.c_str(),"%u.%u.%u.%u%c",&a,&b,&c,&d,&tail)==4&&a<256&&b<256&&c<256&&d<256) {
        if(a==127)return s.loopback_development;
        if(a==0||a>=224||(a==169&&b==254)||(a==100&&b>=64&&b<=127)||(a==198&&(b==18||b==19)))return false;
        const bool private_ip=a==10||(a==172&&b>=16&&b<=31)||(a==192&&b==168);
        return !private_ip||(s.kind==SourceKind::HomeAssistant&&s.allow_private_network);
    }
    // Conservative IPv6 acceptance: global unicast only, no embedded IPv4 or local addresses.
    std::string lower=ip;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
    if(lower=="::1")return s.loopback_development;
    return !lower.empty()&&(lower[0]=='2'||lower[0]=='3')&&lower.find(':')!=std::string::npos&&lower.find('.')==std::string::npos;
}
bool validate(const Schedule& s,const Registry& registry,std::string& error) {
    if(s.version!=1||s.rules.size()>max_rules||s.timezone.empty()||!bounded(s.timezone,128)) {error="Unsupported version, timezone or rule count.";return false;}
    std::set<std::string> ids;
    for(const auto& r:s.rules) {
        const auto& w=r.window;
        if(!id_ok(r.id)||!ids.insert(r.id).second||r.label.empty()||!bounded(r.label,160)||r.priority<0||r.priority>10000||w.start_minute<0||w.start_minute>=1440||w.end_minute<0||w.end_minute>=1440||w.weekdays>127||w.weekdays==0||(w.first&&!valid_date(*w.first))||(w.last&&!valid_date(*w.last))||(w.first&&w.last&&day_number(*w.first)>day_number(*w.last))) {error="Invalid rule identity, label, date, time, weekdays or priority.";return false;}
        if(r.values.empty()||!registry.validate(r.values,error)) {if(error.empty())error="A rule must contain a setting.";return false;}
        if(int(r.source.kind)<0||int(r.source.kind)>2) {error="Unknown source kind.";return false;}
        if(r.source.refresh_seconds<30||r.source.refresh_seconds>86400) {error="Refresh interval must be 30 to 86400 seconds.";return false;}
        if(r.source.kind!=SourceKind::Local) {Endpoint e;if(!endpoint(r.source,e,error))return false;}
        else if(!r.source.url.empty()||!r.source.entity.empty()||r.source.consent||r.source.allow_private_network||r.source.loopback_development) {error="Local rules cannot retain external-source configuration.";return false;}
    }
    error.clear();return true;
}
std::string serialize(const Schedule& s) {
    Json root={{"version",1},{"timezone",s.timezone},{"rules",Json::array()}};
    for(const auto& r:s.rules) {
        const auto& w=r.window;const auto& x=r.source;
        root["rules"].push_back({{"id",r.id},{"label",r.label},{"enabled",r.enabled},{"priority",r.priority},{"window",{{"first",date_json(w.first)},{"last",date_json(w.last)},{"start",w.start_minute},{"end",w.end_minute},{"weekdays",w.weekdays}}},{"values",values_json(r.values)},{"source",{{"kind",int(x.kind)},{"url",x.url},{"entity",x.entity},{"consent",x.consent},{"privateNetwork",x.allow_private_network},{"loopbackDevelopment",x.loopback_development},{"refreshSeconds",x.refresh_seconds}}}});
    }
    return root.dump();
}
bool parse(const std::string& bytes,const Registry& reg,Schedule& out,std::string& error) {
    try {
        const auto j=strict_json(bytes);
        if(!keys(j,{"version","timezone","rules"})||integer(j,"version")!=1||!j.at("rules").is_array()||j.at("rules").size()>max_rules)throw std::runtime_error("Unsupported schedule document.");
        Schedule s;s.timezone=j.at("timezone").get<std::string>();
        for(const auto& x:j.at("rules")) {
            if(!keys(x,{"id","label","enabled","priority","window","values","source"}))throw std::runtime_error("Unknown rule field.");
            Rule r;r.id=x.at("id").get<std::string>();r.label=x.at("label").get<std::string>();r.enabled=x.at("enabled").get<bool>();r.priority=integer(x,"priority");
            const auto& w=x.at("window");if(!keys(w,{"first","last","start","end","weekdays"}))throw std::runtime_error("Unknown window field.");
            r.window={read_date(w.at("first")),read_date(w.at("last")),integer(w,"start"),integer(w,"end"),unsigned(integer(w,"weekdays"))};r.values=values_from(x.at("values"));
            const auto& q=x.at("source");if(!keys(q,{"kind","url","entity","consent","privateNetwork","loopbackDevelopment","refreshSeconds"}))throw std::runtime_error("Unknown source field.");
            auto kind=integer(q,"kind");if(kind<0||kind>2)throw std::runtime_error("Unknown source kind.");
            r.source={SourceKind(kind),q.at("url").get<std::string>(),q.at("entity").get<std::string>(),q.at("consent").get<bool>(),q.at("privateNetwork").get<bool>(),q.at("loopbackDevelopment").get<bool>(),integer(q,"refreshSeconds")};
            s.rules.push_back(std::move(r));
        }
        if(!validate(s,reg,error))return false;
        out=std::move(s);error.clear();return true;
    } catch(...) {error="Schedule document is invalid, unsupported, or exceeds its limits.";return false;}
}
bool parse_api_response(const std::string& bytes,const Registry& reg,Values& out,std::string& error) {
    try {const auto j=strict_json(bytes);if(!keys(j,{"version","values"})||integer(j,"version")!=1)throw std::runtime_error("Unsupported response.");auto v=values_from(j.at("values"));if(v.empty()||!reg.validate(v,error))return false;out=std::move(v);error.clear();return true;}catch(...){error="External settings response is invalid or unsupported.";return false;}
}
bool parse_home_state(const std::string& bytes,const std::string& entity,bool& on,std::string& error) {
    try {const auto j=strict_json(bytes);if(!j.is_object()||j.at("entity_id").get<std::string>()!=entity)throw std::runtime_error("Entity mismatch.");const auto state=j.at("state").get<std::string>();if(state!="on"&&state!="off")throw std::runtime_error("Not boolean.");on=state=="on";error.clear();return true;}catch(...){error="Home Assistant did not return the selected boolean entity.";return false;}
}
Effective evaluate(const Schedule& s,const Registry& reg,const Values& base,LocalTime now,int64_t tick,const std::map<std::string,ExternalValue>& external) {
    Effective out;std::string error;if(!reg.validate(base,error)||!validate(s,reg,error))return out;out.values=base;
    std::vector<const Rule*> ordered;for(const auto& r:s.rules)ordered.push_back(&r);
    std::stable_sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a->priority<b->priority;});
    for(const auto* r:ordered) {
        if(!r->enabled||!matches(r->window,now)) {out.sources[r->id]=State::Inactive;continue;}
        Values values=r->values;
        if(r->source.kind==SourceKind::Local)out.sources[r->id]=State::Local;
        else {
            if(!r->source.consent){out.sources[r->id]=State::Unauthorized;continue;}
            const auto it=external.find(r->id);if(it==external.end()){out.sources[r->id]=State::Waiting;continue;}
            const auto& x=it->second;out.sources[r->id]=x.state;
            if(x.state!=State::Active)continue;
            if(tick>=x.expires_at){out.sources[r->id]=State::Stale;continue;}
            if(r->source.kind==SourceKind::HomeAssistant&&!x.on){out.sources[r->id]=State::Inactive;continue;}
            if(r->source.kind==SourceKind::HttpsApi) {
                values=x.values;
                // A remote endpoint can modify only settings explicitly selected in this rule.
                bool extra=false;for(const auto& p:values)if(!r->values.count(p.first))extra=true;
                if(extra||values.empty()||!reg.validate(values,error)){out.sources[r->id]=State::Invalid;continue;}
            }
        }
        for(const auto& p:values){out.values[p.first]=p.second;out.winners[p.first]=r->id;}
    }
    return out;
}
}
