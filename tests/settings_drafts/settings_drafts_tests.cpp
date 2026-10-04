#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#include "slic3r/GUI/SettingsDraftStore.hpp"
#include <nlohmann/json.hpp>
using namespace Slic3r;
using Slic3r::GUI::SettingsDraftStore;

TEST_CASE("Draft edits and duplicates own their configuration", "[drafts]") {
    DynamicPrintConfig live;
    live.set_deserialize_strict("layer_height", "0.2");
    SettingsDraftStore store;
    SettingsDraftStore::Target target{"project-a", "process-a", "revision-1"};
    const auto id = store.create(Preset::TYPE_PRINT, "Source", live, target);
    REQUIRE_FALSE(id.empty());
    store.find(id)->config.set_deserialize_strict("layer_height", "0.3");
    REQUIRE(live.opt_serialize("layer_height") == "0.2");
    REQUIRE(store.dirty(id));
    const auto duplicate = store.duplicate(id);
    REQUIRE_FALSE(duplicate.empty());
    store.find(duplicate)->config.set_deserialize_strict("layer_height", "0.4");
    REQUIRE(store.find(id)->config.opt_serialize("layer_height") == "0.3");
    const auto apply = store.prepare_apply(id, live, target);
    REQUIRE(apply.ready);
    REQUIRE(apply.changed_keys == std::vector<std::string>{"layer_height"});
    REQUIRE(apply.delta.opt_serialize("layer_height") == "0.3");
    REQUIRE(live.opt_serialize("layer_height") == "0.2");
}

TEST_CASE("Draft apply rejects moved targets and edited live settings", "[drafts]") {
    DynamicPrintConfig live;
    live.set_deserialize_strict("layer_height", "0.2");
    SettingsDraftStore store;
    SettingsDraftStore::Target target{"project-a", "process-a", "revision-1"};
    const auto id = store.create(Preset::TYPE_PRINT, "Source", live, target);
    SECTION("Project replaced") { target.project_id = "project-b"; }
    SECTION("Preset replaced") { target.target_id = "process-b"; }
    SECTION("Revision changed") { target.revision = "revision-2"; }
    SECTION("Live option changed") { live.set_deserialize_strict("layer_height", "0.25"); }
    REQUIRE_FALSE(store.prepare_apply(id, live, target).ready);
}

TEST_CASE("Draft persistence survives missing source and rejects corruption transactionally", "[drafts]") {
    DynamicPrintConfig config;
    config.set_deserialize_strict("layer_height", "0.2");
    SettingsDraftStore store;
    const auto id = store.create(Preset::TYPE_PRINT, "Deleted source preset", config, {"project", "target", "rev"});
    store.find(id)->config.set_deserialize_strict("layer_height", "0.3");
    SettingsDraftStore restored;
    REQUIRE(restored.restore(store.serialize()));
    REQUIRE(restored.find(id)->source_preset_name == "Deleted source preset");
    REQUIRE(restored.find(id)->config.opt_serialize("layer_height") == "0.3");
    REQUIRE(restored.dirty(id));
    auto invalid = nlohmann::json::parse(store.serialize());
    invalid["drafts"][0]["baseline_fingerprint"] = "wrong";
    REQUIRE_FALSE(restored.restore(invalid.dump()));
    REQUIRE(restored.find(id)->config.opt_serialize("layer_height") == "0.3");
    REQUIRE_FALSE(restored.restore("{broken"));
    REQUIRE(restored.ids().size() == 1);
}

TEST_CASE("Draft snapshots exclude device credentials", "[drafts]") {
    DynamicPrintConfig config;
    config.set_deserialize_strict("layer_height", "0.2");
    config.set_key_value("printer_access_code", new ConfigOptionString("test-only-placeholder"));
    SettingsDraftStore store;
    const auto id = store.create(Preset::TYPE_PRINT, "Source", config, {});
    REQUIRE_FALSE(store.find(id)->config.has("printer_access_code"));
    REQUIRE(store.serialize().find("test-only-placeholder") == std::string::npos);
    REQUIRE_FALSE(SettingsDraftStore::permitted_key("API_TOKEN"));
    REQUIRE_FALSE(SettingsDraftStore::permitted_key("device_password"));
    REQUIRE(store.prepare_apply(id, config, {}).ready);
}
