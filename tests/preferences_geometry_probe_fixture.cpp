#include "PreferencesGeometryProbe.cpp"
#include <iostream>
#include <cassert>

using Json=nlohmann::json;
struct Rejected : std::runtime_error {
    std::string code;
    Rejected(std::string c, const char* m):std::runtime_error(m),code(std::move(c)) {}
};
Json dispatch_probe(bool local_capabilities_only, const Json& a)
{
    using namespace Slic3r::GUI;
    const std::string op="preferences_geometry";
    // PRODUCTION_DISPATCH
    throw std::runtime_error("missing operation");
}

int run_checks()
{
    using namespace Slic3r::GUI::PreferencesGeometryProbe;
    assert(snapshot()["state"] == "unavailable");
    assert(dispatch_probe(false,Json::object())["state"]=="unavailable");
    try { dispatch_probe(true,Json::object()); assert(false); }
    catch(const Rejected& e) { assert(e.code=="operation_unavailable"); }
    try { dispatch_probe(false,{{"path","neutral-canary"}}); assert(false); }
    catch(const Rejected& e) { assert(e.code=="invalid_arguments"); }
    wxWindow dialog, card, search, strip, page, other_page, sensitive_child;
    wxSimplebook book;
    dialog.top=true; dialog.bounds={20,30,780,640}; dialog.client={764,600}; dialog.minimum={640,480};
    card.parent=&dialog; card.bounds={200,80,570,520}; card.client={570,520};
    book.parent=&card; book.bounds={210,150,540,430}; book.client={540,430};
    search.parent=&card; search.bounds={210,90,540,44}; search.client={540,44};
    strip.parent=&dialog; strip.bounds={30,80,150,520};
    page.parent=&book; page.bounds=book.bounds; page.client=book.client;
    other_page.parent=&book; other_page.bounds={210,150,400,300};
    book.current=&page;
    page.children.push_back(&sensitive_child);
    sensitive_child.parent=&page;
    bind(&dialog,&card,&book,&search,&strip);
    auto result=snapshot();
    assert(result.size()==4 && result["roles"].size()==6);
    assert(result["roles"][0]["minimumSize"]["width"]==640);
    assert(result["roles"][0]["parentClientContains"].is_null());
    for (size_t i=1;i<6;++i) assert(result["roles"][i]["parentClientContains"]==true);
    for (auto& role:result["roles"]) {
        assert(role.size()==10);
        assert(!role.contains("label") && !role.contains("name") && !role.contains("value"));
    }
    // Resizing and an independently selected page must affect measured results.
    card.client={300,200}; book.current=&other_page;
    result=snapshot();
    assert(result["roles"][2]["parentClientContains"]==false);
    assert(result["roles"][5]["screenRect"]["width"]==400);
    search.shown=false; strip.enabled=false; page.deleting=true;
    result=snapshot();
    assert(result["roles"][3]["shownOnScreen"]==false);
    assert(result["roles"][4]["enabled"]==false);
    auto ephemeral=std::make_unique<wxWindow>();
    bind(&dialog,&card,&book,ephemeral.get(),&strip); ephemeral.reset();
    assert(snapshot()["roles"][3]["state"]=="unavailable");
    card.deleting=true;
    assert(snapshot()["roles"][1]["state"]=="destroying");
    dialog.shown=false; assert(snapshot()["state"]=="unavailable");
    dialog.shown=true; dialog.deleting=true; assert(snapshot()["state"]=="unavailable");
    dialog.deleting=false; main_thread=false;
    assert(snapshot()["state"]=="wrong_thread");
    bind(nullptr,nullptr,nullptr,nullptr,nullptr);
    main_thread=true; assert(snapshot()["state"]=="available");
    bind(nullptr,nullptr,nullptr,nullptr,nullptr);
    assert(snapshot()["state"]=="unavailable");
    std::cout << "PASS: geometry, selection, containment, hidden/destroyed roles, thread and content canaries\n";
    return 0;
}

int main()
{
    try { return run_checks(); }
    catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
