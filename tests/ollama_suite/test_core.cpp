#include "libslic3r/OllamaSuite/OllamaCore.hpp"
#include <iostream>
#include <stdexcept>
using namespace Slic3r::OllamaSuite;
namespace {
int checks=0;
void check(bool v,const char *message) { ++checks; if(!v) throw std::runtime_error(message); }
template<class F> void rejects(F f,const char *message) { bool rejected=false; try { f(); } catch(...) { rejected=true; } check(rejected,message); }
CatalogPage page(std::string path,std::vector<std::string> names,std::vector<std::string> next,std::size_t total) {
    return {std::move(path),"fixture-response-identity","2026-10-05T00:00:00Z",std::move(names),std::move(next),total};
}
}
int main() {
try {
    check(valid_model("example:7b-q4"),"exact tag accepted"); check(!valid_model("https://host/model"),"URL rejected"); check(!valid_model("../../model"),"traversal rejected");
    check(!valid_model("model;run"),"shell syntax rejected"); check(request(Operation::Version).path=="/api/version","version route");
    check(request(Operation::Pull,{{"model","example:tag"}}).streaming,"pull streaming required");
    rejects([] { request(Operation::Pull,{{"model","example"},{"insecure",true}}); },"unknown pull field rejected");
    rejects([] { parse_json(std::string(max_json_bytes+1,'x')); },"oversized response rejected");
    rejects([] { parse_json(std::string(40,'[')+std::string(40,']')); },"nested response rejected");
    rejects([] { parse_json("[]"); },"non-object response rejected");
    int lines=0; Ndjson decoder([&](const Json &j) { ++lines; return j.contains("done"); });
    check(decoder.feed("{\"do",4),"fragment accepted"); const std::string rest="ne\":false}\n{\"done\":true}"; check(decoder.feed(rest.data(),rest.size()),"split frames accepted"); check(decoder.finish()&&lines==2,"final unterminated frame accepted");
    Ndjson malformed([](const Json &) { return true; }); check(!malformed.feed("no json\n",8),"malformed stream fails");
    Ndjson oversized([](const Json &) { return true; }); auto giant=std::string(max_stream_line+1,'x'); check(!oversized.feed(giant.data(),giant.size()),"oversized stream line fails");
    Ndjson remote_error([](const Json &) { return true; }); std::string error="{\"error\":\"private payload\"}\n"; check(!remote_error.feed(error.data(),error.size())&&remote_error.error().find("private payload")==std::string::npos,"raw runtime error not reflected");
    auto models=installed_models({{"models",Json::array({{{"name","example:tag"},{"size",std::uint64_t(1000)},{"digest","abc"},{"details",{{"family","example"},{"parameter_size","1B"},{"quantization_level","Q4"}}}}})}});
    check(models.size()==1&&models[0].bytes==1000,"installed metadata parsed"); auto m=models[0];
    apply_details(m,{{"capabilities",Json::array({"completion","vision"})},{"model_info",{{"general.architecture","example"},{"general.parameter_count",std::uint64_t(1000000)},{"example.context_length",std::uint64_t(4096)}}}});
    check(m.capabilities_verified&&m.context_length==4096,"capability evidence applied");
    auto merged=reconcile({Model{"other:tag"}},models,{"example:tag"}); check(merged.size()==2&&merged[0].running,"catalog and installed union retained");
    Hardware h; h.available_ram=10000; h.usable_vram=10000; h.free_disk=10000; h.backend_verified=true; h.architecture="x86_64"; h.measured_at="now";
    check(fit(m,h,2048,100).verdict==Fit::RunsWell,"fit ample verified memory"); h.usable_vram=0;
    check(fit(m,h,2048,100).verdict==Fit::WithLimits,"fit CPU-limited"); h.available_ram=100;
    check(fit(m,h,2048,100).verdict==Fit::Unlikely,"fit low memory"); h.backend_verified=false;
    check(fit(m,h,2048,100).verdict==Fit::Unknown,"missing backend evidence never guessed"); h.backend_verified=true;
    check(fit(m,h,2048,{}).verdict==Fit::Unknown,"missing context overhead never zero");
    auto messages=Json::array({{{"role","user"},{"content","hello"}}}); auto payload=chat_payload(m,messages); check(request(Operation::Chat,payload).streaming,"validated chat streams");
    rejects([&] { chat_payload(m,messages,3); },"invalid temperature"); rejects([&] { chat_payload(m,messages,0.7,8192); },"model context limit");
    auto image_messages=Json::array({{{"role","user"},{"content","image"},{"images",Json::array({"YWJj"})}}}); check(chat_payload(m,image_messages).contains("messages"),"vision attachment supported");
    m.capabilities.erase("vision"); rejects([&] { chat_payload(m,image_messages); },"attachment capability negative");
    apply_details(m,{{"remote_model","remote"},{"capabilities",Json::array({"completion"})}}); rejects([&] { chat_payload(m,messages); },"cloud metadata blocks local chat");
    check(official_catalog_path("/library/example/tags?page=2"),"bounded official pagination"); check(!official_catalog_path("/library/example/tags?redirect=https://evil"),"catalog query allowlist"); check(!official_catalog_path("/libraryevil"),"catalog prefix confusion blocked");
    auto html=parse_catalog_html("/library/example/tags","<a href=\"/library/example:q4\">one</a><a href=\"/library/example:q4\">duplicate</a><a href=\"/library/example:q8\">two</a><a href=\"/library/example/tags?page=2\">next</a>","sha256","time");
    check(html.names.size()==2&&html.next_pages.size()==1&&!html.advertised_count,"HTML deduplication does not invent authority count");
    std::map<std::string,CatalogPage> pages;
    pages["/library"]=page("/library",{"a"},{"/library?page=2"},2); pages["/library?page=2"]=page("/library?page=2",{"b"},{},2);
    pages["/library/a/tags"]=page("/library/a/tags",{"a:q4"},{"/library/a/tags?page=2"},2); pages["/library/a/tags?page=2"]=page("/library/a/tags?page=2",{"a:q8"},{},2);
    pages["/library/b/tags"]=page("/library/b/tags",{"b:q4"},{},1); std::atomic_bool cancel{false};
    auto fetch=[&](const std::string &p) { return pages.at(p); }; auto catalog=refresh_catalog(fetch,cancel);
    check(catalog.complete&&catalog.models.size()==3&&catalog.pages.size()==5,"all pages and tags reconciled");
    pages.erase("/library/a/tags?page=2"); check(!refresh_catalog(fetch,cancel).complete,"removed page turns completeness red");
    pages["/library/a/tags?page=2"]=page("/library/a/tags?page=2",{"a:q8"},{},2); check(refresh_catalog(fetch,cancel).complete,"restored page turns completeness green");
    pages["/library/a/tags"].advertised_count.reset(); check(!refresh_catalog(fetch,cancel).complete,"authority unknown never complete");
    check(load_catalog(catalog_json(catalog)).complete,"offline cache revalidates traversal");
    auto broken=catalog_json(catalog); broken["pages"].erase(0); check(!load_catalog(broken).complete,"missing cached page fails closed");
    auto root=std::filesystem::temp_directory_path()/("ollama-test-"+unique_id()); std::filesystem::create_directories(root);
    PullQueue q(root/"pulls"); auto i=q.add("example:q4"); i.state=PullState::Pulling; q.save(i); q.recover(); check(q.read(i.id).state==PullState::Interrupted,"interrupted pull not success");
    q.retry(i.id); check(q.next().has_value(),"retry queues exact tag"); check(q.page(0,1).size()==1,"bounded queue page"); rejects([&] { q.page(0,101); },"page bound enforced"); rejects([&] { q.read("../escape"); },"queue traversal rejected");
    ChatStore chats(root/"chats"); ChatSession session{unique_id(),"private title","example:q4",image_messages}; chats.save(session); auto read=chats.read(session.id);
    check(!read.messages[0].contains("images"),"attachment bytes omitted from persisted history"); auto exported=chats.redacted_export(read).dump(); check(exported.find("private title")==std::string::npos&&exported.find("YWJj")==std::string::npos,"ordinary export redacted");
    rejects([&] { chats.remove(session.id,false); },"chat deletion needs confirmation"); chats.remove(session.id,true);
    std::filesystem::remove_all(root);
    std::cout<<"ollama core: "<<checks<<" checks passed\n"; return 0;
} catch(const std::exception &e) { std::cerr<<"FAIL: "<<e.what()<<" after "<<checks<<" checks\n"; return 1; }
}
