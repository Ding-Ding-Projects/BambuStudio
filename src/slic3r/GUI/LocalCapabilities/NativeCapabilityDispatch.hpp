#pragma once
#include "NativeCapabilityRegistry.hpp"
#include <nlohmann/json.hpp>
namespace Slic3r { namespace GUI { namespace LocalCapabilities {
inline const char* name(Capability value)
{
    switch(value) {
    case Capability::SchoolState:return "school.state";
    case Capability::SchoolManage:return "school.manage";
    case Capability::VaultManage:return "vault.manage";
    case Capability::ConverterManage:return "converter.manage";
    case Capability::OllamaManage:return "ollama.manage";
    }
    throw Rejected("unknown_capability");
}
inline Capability parse(const std::string& value)
{
    for(auto capability:{Capability::SchoolState,Capability::SchoolManage,Capability::VaultManage,Capability::ConverterManage,Capability::OllamaManage})
        if(value==name(capability)) return capability;
    throw Rejected("unknown_capability");
}
inline nlohmann::json dispatch(Registry& value,const nlohmann::json& args)
{
    using Json=nlohmann::json;
    if(!args.is_object() || args.dump().size()>4096 || !args.contains("action") || !args["action"].is_string()) throw Rejected("invalid_request");
    const auto action=args["action"].get<std::string>();
    const auto fields=[&](std::initializer_list<const char*> expected) {
        if(args.size()!=expected.size()) throw Rejected("invalid_request");
        for(const auto* field:expected) if(!args.contains(field)) throw Rejected("invalid_request");
    };
    const auto text=[&](const char* key) {if(!args.contains(key)||!args[key].is_string())throw Rejected("invalid_request");return args[key].get<std::string>();};
    if(action=="claim_pairing") {
        fields({"action"}); const auto approval=value.claim_pairing(); Json grants=Json::array();
        for(auto capability:approval.grants) grants.push_back(name(capability));
        return {{"approvalId",approval.id},{"origin",approval.origin},{"grants",grants}};
    }
    if(action=="publish_offer") {
        fields({"action","approvalId","endpoint","nonce"});
        value.publish_offer(text("approvalId"),{text("endpoint"),text("nonce"),60}); return {{"published",true}};
    }
    if(action=="status") {fields({"action","approvalId"});return {{"active",value.active(text("approvalId"))}};}
    if(action=="revoke") {fields({"action","approvalId"});value.revoke(text("approvalId"));return {{"revoked",true}};}
    if(action=="invoke") {
        fields({"action","approvalId","capability"});const auto capability=parse(text("capability"));
        const auto result=value.invoke(text("approvalId"),capability);
        if(capability==Capability::SchoolState) {
            if(result.display_name.size()>512 || std::any_of(result.display_name.begin(),result.display_name.end(),[](unsigned char ch){return ch<32||ch==127;})) throw Rejected("invalid_native_result");
            return {{"enabled",result.enabled},{"displayName",result.display_name}};
        }
        return {{"opened",result.opened}};
    }
    throw Rejected("unknown_action");
}
}}}
