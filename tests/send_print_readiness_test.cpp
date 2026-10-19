// Standalone assertions for src/slic3r/GUI/SendPrintReadiness.hpp, the
// decisions behind the Send print job dialog's plate status, its nozzle-swap
// gate and the plate snapshot that keeps opening the dialog from changing the
// plate. Build without NDEBUG (this file undefines it), for example:
//   g++ -std=c++17 -Wall -Wextra -Werror tests/send_print_readiness_test.cpp -o sprt && ./sprt
// tests/send_print_readiness.test.mjs compiles and runs it in the contract gate.
#undef NDEBUG
#include "../src/slic3r/GUI/SendPrintReadiness.hpp"
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace Slic3r::GUI;
static int assertions = 0;
#define CHECK(expr) do { assert(expr); ++assertions; } while (0)

static SendPlateFacts plate(int index, bool objects, bool slice) { return {index, objects, slice}; }

static void readiness_assertions()
{
    // The reported case: a plate with a model whose slice was invalidated
    // after slicing is not an empty plate.
    auto v = classify_send_plates({plate(0, true, false)});
    CHECK(v.readiness == SendReadiness::NeedsSlice && v.plate_index == 0);
    v = classify_send_plates({plate(3, true, false)});
    CHECK(v.readiness == SendReadiness::NeedsSlice && v.plate_index == 3);
    // Only a plate with nothing to print is empty.
    v = classify_send_plates({plate(0, false, false)});
    CHECK(v.readiness == SendReadiness::EmptyPlate && v.plate_index == -1);
    v = classify_send_plates({});
    CHECK(v.readiness == SendReadiness::EmptyPlate);
    // A valid slice is sendable, as before (also when the object check says nothing is on it).
    CHECK(classify_send_plates({plate(0, true, true)}).readiness == SendReadiness::Ready);
    CHECK(classify_send_plates({plate(0, false, true)}).readiness == SendReadiness::Ready);

    // Print all looks at every plate, not only the current one.
    // The current plate empty does not make the job empty...
    v = classify_send_plates({plate(0, false, false), plate(1, true, true)});
    CHECK(v.readiness == SendReadiness::Ready);
    // ...and a stale plate that is not current still blocks, named by index.
    v = classify_send_plates({plate(0, true, true), plate(1, false, false), plate(2, true, false), plate(3, true, false)});
    CHECK(v.readiness == SendReadiness::NeedsSlice && v.plate_index == 2);
    v = classify_send_plates({plate(0, true, false), plate(1, true, true)});
    CHECK(v.readiness == SendReadiness::NeedsSlice && v.plate_index == 0);
    v = classify_send_plates({plate(0, false, false), plate(1, false, false)});
    CHECK(v.readiness == SendReadiness::EmptyPlate);
    v = classify_send_plates({plate(0, true, true), plate(1, true, true), plate(2, false, false)});
    CHECK(v.readiness == SendReadiness::Ready && v.plate_index == -1);
}

static void swap_gate_assertions()
{
    const std::vector<int> applied{2, 1, 1};
    CHECK(!quick_swap_pending({}, applied));         // nothing proposed yet
    CHECK(!quick_swap_pending(applied, applied));    // nothing moved
    std::vector<int> pending = applied;
    pending[0] = 1;                                  // "Move material 1 left"
    CHECK(quick_swap_pending(pending, applied));
    pending[0] = 2;                                  // undone
    CHECK(!quick_swap_pending(pending, applied));
    CHECK(quick_swap_pending({1, 2}, {}));           // a map against no applied map
}

static void volume_type_assertions()
{
    const int standard = 0, high_flow = 1, hybrid = 2, tpu_high_flow = 3;
    // Two fixed-type nozzles: every material on a nozzle takes that nozzle's type.
    CHECK((quick_swap_volume_types({2, 1}, {standard, standard}, {standard, high_flow}, hybrid, standard) ==
           std::vector<int>{high_flow, standard}));
    CHECK((quick_swap_volume_types({1, 2}, {high_flow, standard}, {standard, high_flow}, hybrid, standard) ==
           std::vector<int>{standard, high_flow}));
    // Two standard nozzles (H2C 0.4/0.4): moving a material keeps it standard.
    CHECK((quick_swap_volume_types({1}, {standard}, {standard, standard}, hybrid, standard) == std::vector<int>{standard}));
    // A hybrid nozzle keeps the material's own type, or standard without one.
    CHECK((quick_swap_volume_types({2, 2, 2}, {tpu_high_flow, high_flow}, {standard, hybrid}, hybrid, standard) ==
           std::vector<int>{tpu_high_flow, high_flow, standard}));
    // Materials without a nozzle, or with an unknown one, keep their type.
    CHECK((quick_swap_volume_types({0, 3, 1}, {high_flow, tpu_high_flow, high_flow}, {standard, hybrid}, hybrid, standard) ==
           std::vector<int>{high_flow, tpu_high_flow, standard}));
    CHECK((quick_swap_volume_types({0}, {}, {standard, hybrid}, hybrid, standard) == std::vector<int>{standard}));
    CHECK(quick_swap_volume_types({}, {high_flow}, {standard, hybrid}, hybrid, standard).empty());
}

// A configuration with the DynamicConfig calls the snapshot uses.
struct FakeOption
{
    std::string value;
    FakeOption *clone() const { return new FakeOption(*this); }
    bool        operator==(const FakeOption &rhs) const { return value == rhs.value; }
};
struct FakeConfig
{
    std::map<std::string, std::unique_ptr<FakeOption>> options;
    bool has(const std::string &key) const { return options.count(key) > 0; }
    const FakeOption *optptr(const std::string &key) const
    {
        auto it = options.find(key);
        return it == options.end() ? nullptr : it->second.get();
    }
    bool set_key_value(const std::string &key, FakeOption *option)
    {
        const bool added = !has(key);
        options[key].reset(option);
        return added;
    }
    bool erase(const std::string &key) { return options.erase(key) > 0; }
    void set(const std::string &key, const std::string &value) { set_key_value(key, new FakeOption{value}); }
    std::map<std::string, std::string> dump() const
    {
        std::map<std::string, std::string> out;
        for (const auto &[key, option] : options) out[key] = option->value;
        return out;
    }
};

// The parts of PartPlate that the old restore went through:
// set_filament_map_mode() clears every map when the effective mode changes.
struct FakePlate
{
    FakeConfig  config;
    std::string project_mode{"auto"};
    std::string real_mode() const { return config.has("filament_map_mode") ? config.optptr("filament_map_mode")->value : project_mode; }
    void set_filament_map_mode(const std::string &mode)
    {
        const std::string new_real = mode.empty() ? project_mode : mode;
        if (real_mode() != new_real) {
            config.erase("filament_map");
            config.erase("filament_nozzle_map");
            config.erase("filament_volume_map");
        }
        if (mode.empty()) config.erase("filament_map_mode");
        else config.set("filament_map_mode", mode);
    }
};

static const std::vector<std::string> plate_keys{"filament_map_mode", "filament_map", "filament_nozzle_map", "filament_volume_map"};

// What the swap check does to the plate for the length of the check.
static void write_proposal(FakeConfig &config)
{
    config.set("filament_map_mode", "manual");
    config.set("filament_map", "1,1");
}

static void snapshot_assertions()
{
    // The reported plate: no plate mode, project mode automatic, and the maps
    // the slice wrote back to the plate.
    FakePlate sliced;
    sliced.config.set("filament_map", "2,1");
    sliced.config.set("filament_nozzle_map", "1,0");
    sliced.config.set("filament_volume_map", "0,0");
    sliced.config.set("other", "untouched");
    const auto before = sliced.config.dump();

    // The old restore (mode through the clearing setter, only filament_map put
    // back) loses the slice-written nozzle and volume maps. This is what made
    // the next background apply discard the slice.
    {
        FakePlate old_way;
        old_way.config.set("filament_map", "2,1");
        old_way.config.set("filament_nozzle_map", "1,0");
        old_way.config.set("filament_volume_map", "0,0");
        old_way.set_filament_map_mode("manual");
        old_way.config.set("filament_map", "1,1");
        old_way.config.set("filament_map", "2,1");
        old_way.set_filament_map_mode("");
        CHECK(!old_way.config.has("filament_nozzle_map") && !old_way.config.has("filament_volume_map"));
    }

    // The snapshot puts back every key exactly.
    {
        const ConfigKeySnapshot<FakeConfig, FakeOption> saved(sliced.config, plate_keys);
        CHECK(saved.matches(sliced.config));
        write_proposal(sliced.config);
        CHECK(!saved.matches(sliced.config));
        saved.restore(sliced.config);
        CHECK(saved.matches(sliced.config));
        CHECK(sliced.config.dump() == before);
        CHECK(!sliced.config.has("filament_map_mode")); // absent stays absent
        saved.restore(sliced.config);                    // a second restore (the error path) changes nothing
        CHECK(sliced.config.dump() == before);
    }

    // An explicit automatic plate mode: the old restore set the mode after the
    // map, which erased the map it had just put back. The snapshot keeps all.
    {
        FakePlate explicit_auto;
        explicit_auto.config.set("filament_map_mode", "auto-flush");
        explicit_auto.config.set("filament_map", "2,1");
        explicit_auto.config.set("filament_nozzle_map", "1,0");
        explicit_auto.config.set("filament_volume_map", "1,0");
        const auto original = explicit_auto.config.dump();
        const ConfigKeySnapshot<FakeConfig, FakeOption> saved(explicit_auto.config, plate_keys);
        write_proposal(explicit_auto.config);
        saved.restore(explicit_auto.config);
        CHECK(explicit_auto.config.dump() == original);
    }

    // A plate that was never sliced: the keys the check added are erased again.
    {
        FakeConfig unsliced;
        unsliced.set("other", "untouched");
        const ConfigKeySnapshot<FakeConfig, FakeOption> saved(unsliced, plate_keys);
        write_proposal(unsliced);
        unsliced.set("filament_volume_map", "0");
        saved.restore(unsliced);
        CHECK((unsliced.dump() == std::map<std::string, std::string>{{"other", "untouched"}}));
    }

    // matches() notices a changed value, a lost key and an added key.
    {
        FakeConfig config;
        config.set("filament_map", "1");
        config.set("filament_nozzle_map", "0");
        const ConfigKeySnapshot<FakeConfig, FakeOption> saved(config, plate_keys);
        config.set("filament_map", "2");
        CHECK(!saved.matches(config));
        saved.restore(config);
        config.erase("filament_nozzle_map");
        CHECK(!saved.matches(config));
        saved.restore(config);
        config.set("filament_volume_map", "0");
        CHECK(!saved.matches(config));
        saved.restore(config);
        CHECK(saved.matches(config));
    }
}

static void express_assertions()
{
    // Off: the choices pass through unchanged, auto included.
    PrintPrepareChoice chosen{true, true, 2, 2, 2};
    CHECK(apply_express_print_mode(false, chosen) == chosen);
    PrintPrepareChoice partial{false, true, 0, 1, 2};
    CHECK(apply_express_print_mode(false, partial) == partial);
    // On: every preparation flag is off, whatever the rows show.
    const PrintPrepareChoice off{};
    CHECK(apply_express_print_mode(true, chosen) == off);
    CHECK(apply_express_print_mode(true, partial) == off);
    CHECK(apply_express_print_mode(true, off) == off);
    const PrintPrepareChoice sent = apply_express_print_mode(true, chosen);
    CHECK(!sent.bed_leveling_on && !sent.flow_cali_on);
    CHECK(sent.bed_leveling == 0 && sent.flow_cali == 0 && sent.nozzle_offset_cali == 0);
    // The row toggles only when Slice again can act.
    CHECK(express_toggle_enabled(true, false, true));
    CHECK(!express_toggle_enabled(false, false, true)); // SD card view
    CHECK(!express_toggle_enabled(true, true, true));   // sending
    CHECK(!express_toggle_enabled(true, false, false)); // Print all, or another plate
}

int main()
{
    readiness_assertions();
    swap_gate_assertions();
    volume_type_assertions();
    snapshot_assertions();
    express_assertions();
    std::cout << assertions << " send print readiness assertions passed\n";
    return 0;
}
