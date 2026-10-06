#include "slic3r/GUI/LocalCapabilities/NativeCapabilityDispatch.hpp"
#include <iostream>
using namespace Slic3r::GUI::LocalCapabilities;
int checks=0;
void check(bool value){++checks;if(!value)throw std::runtime_error("assertion failed");}
template<class Action> void denied(Action action,const std::string& code)
{try{action();throw std::runtime_error("expected rejection");}catch(const Rejected& result){check(result.what()==code);}}
int main()
{
    Registry::Clock::time_point now{};
    Registry registry([]{return std::string(64,'A');},[&]{return now;});
    bool available=true;int calls=0;bool shown=false;
    registry.register_handler(Capability::SchoolState,[&]{return available;},[&]{++calls;return Result{false,true,"Example mode"};});
    check(registry.available().size()==1);
    denied([&]{registry.claim_pairing();},"approval_expired");
    for(const auto& origin:{"http://example.com","https://example.com/","https://example.com:443/path","https://localhost","https://127.0.0.1","https://example.com?x","https://example.com#x","https://user@example.com"})
        check(!Registry::valid_origin(origin));
    check(Registry::valid_origin("https://example.com"));
    denied([&]{registry.approve_pairing("https://example.com",{Capability::VaultManage},[](const Offer&){});},"capability_unavailable");
    auto approved=registry.approve_pairing("https://example.com",{Capability::SchoolState},[&](const Offer&){shown=true;});
    auto claim=registry.claim_pairing(); check(claim.id==approved.id);
    denied([&]{registry.claim_pairing();},"pairing_unavailable");
    denied([&]{registry.invoke(claim.id,Capability::SchoolState);},"capability_not_granted");
    denied([&]{registry.publish_offer(claim.id,{"http://remote.example:1234/",std::string(64,'B'),60});},"invalid_offer");
    registry.publish_offer(claim.id,{"http://127.0.0.1:1234/",std::string(64,'B'),60}); check(shown);
    check(registry.invoke(claim.id,Capability::SchoolState).enabled);check(calls==1);
    denied([&]{registry.invoke(claim.id,Capability::VaultManage);},"capability_not_granted");
    available=false;denied([&]{registry.invoke(claim.id,Capability::SchoolState);},"capability_unavailable");available=true;
    auto output=dispatch(registry,{{"action","invoke"},{"approvalId",claim.id},{"capability","school.state"}});check(output["enabled"]==true);
    denied([&]{dispatch(registry,{{"action","invoke"},{"approvalId",claim.id},{"capability","school.state"},{"path","anything"}});},"invalid_request");
    denied([&]{dispatch(registry,{{"action","invoke"},{"approvalId",claim.id},{"capability","shell.execute"}});},"unknown_capability");
    bool wrong_thread=false;std::thread worker([&]{try{registry.available();}catch(const Rejected& result){wrong_thread=std::string(result.what())=="wrong_thread";}});worker.join();check(wrong_thread);
    now+=std::chrono::minutes(10);check(!registry.active(claim.id));
    denied([&]{registry.invoke(claim.id,Capability::SchoolState);},"approval_expired");
    approved=registry.approve_pairing("https://example.com",{Capability::SchoolState},[](const Offer&){});
    registry.revoke(approved.id);check(!registry.active(approved.id));
    std::cout<<"Native capability registry: "<<checks<<" checks passed.\n";
}
