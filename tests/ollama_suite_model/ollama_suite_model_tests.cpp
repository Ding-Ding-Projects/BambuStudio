#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <catch_main.hpp>

#include "libslic3r/OllamaSuite/OllamaCore.hpp"

#include <filesystem>
#include <fstream>

using namespace Slic3r::OllamaSuite;

namespace {

constexpr std::uint64_t GiB = 1024ull * 1024ull * 1024ull;

// Shape of a published 8B grouped-query model as /api/show reports it.
Json eight_billion_details()
{
    return {{"capabilities", Json::array({"completion"})},
            {"model_info",
             {{"general.architecture", "llama"},
              {"general.parameter_count", std::uint64_t(8030261248ull)},
              {"llama.context_length", std::uint64_t(131072)},
              {"llama.block_count", std::uint64_t(32)},
              {"llama.embedding_length", std::uint64_t(4096)},
              {"llama.attention.head_count", std::uint64_t(32)},
              {"llama.attention.head_count_kv", std::uint64_t(8)}}}};
}

Model installed_model(const std::string &name)
{
    Model m;
    m.name         = name;
    m.bytes        = 4920734016ull;
    m.quantization = "Q4_K_M";
    m.installed    = true;
    apply_details(m, eight_billion_details());
    return m;
}

Hardware measured(std::uint64_t ram, std::uint64_t dedicated, std::uint64_t budget)
{
    Hardware h;
    h.available_ram = ram;
    h.total_ram     = ram;
    h.architecture  = "x86_64";
    h.measured_at   = "2026-10-09T00:00:00Z";
    h.free_disk     = 500 * GiB;
    h.gpus          = {GpuAdapter{"Example discrete adapter", "32.0.15.6094", dedicated, budget}};
    return h;
}

BackendObservation gpu_seen(const Hardware &h)
{
    return *observe_backend({RuntimeMemory{"example:8b", 6 * GiB, 6 * GiB}}, "0.12.3", adapter_identity(h.gpus), "2026-10-08T12:00:00Z");
}

struct TempRoot
{
    std::filesystem::path path = std::filesystem::temp_directory_path() / ("ollama-suite-model-" + unique_id());
    TempRoot() { std::filesystem::create_directories(path); }
    ~TempRoot()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

void touch(const std::filesystem::path &file)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << "{}";
}

} // namespace

TEST_CASE("Attention geometry from /api/show sizes the context cache exactly", "[ollama][fit]")
{
    const auto m = installed_model("example:8b");
    REQUIRE(m.block_count == 32u);
    REQUIRE(m.head_count_kv == 8u);
    // 4096 tokens x 32 layers x 8 key/value heads x (128 + 128) dimensions.
    CHECK(context_memory(m, 4096, KvCache::F16) == 536870912u);
    CHECK(context_memory(m, 4096, KvCache::Q8_0) == 285212672u);
    CHECK(context_memory(m, 4096, KvCache::Q4_0) == 150994944u);

    SECTION("per-layer head counts contribute their largest entry")
    {
        auto details = eight_billion_details();
        details["model_info"]["llama.attention.head_count_kv"] = Json::parse("[0, 8, 4, 8]");
        Model hybrid = m;
        apply_details(hybrid, details);
        CHECK(hybrid.head_count_kv == 8u);
    }
    SECTION("absent key/value heads fall back to the documented attention head count")
    {
        Model mha = m;
        mha.head_count_kv.reset();
        CHECK(context_memory(mha, 4096, KvCache::F16) == 4u * 536870912u);
    }
    SECTION("missing geometry is unknown, never zero")
    {
        Model bare = m;
        bare.block_count.reset();
        CHECK_FALSE(context_memory(bare, 4096, KvCache::F16).has_value());
        Model no_heads = m;
        no_heads.head_count.reset();
        no_heads.head_count_kv.reset();
        CHECK_FALSE(context_memory(no_heads, 4096, KvCache::F16).has_value());
    }
}

TEST_CASE("Manifest locations follow Ollama's documented name defaults", "[ollama][destination]")
{
    CHECK(manifest_path("example") == std::filesystem::path("manifests/registry.ollama.ai/library/example/latest"));
    CHECK(manifest_path("example:q4") == std::filesystem::path("manifests/registry.ollama.ai/library/example/q4"));
    CHECK(manifest_path("user/custom:latest") == std::filesystem::path("manifests/registry.ollama.ai/user/custom/latest"));
    CHECK(manifest_path("hf.co/org/repo:Q4_K_M") == std::filesystem::path("manifests/hf.co/org/repo/Q4_K_M"));
    CHECK_FALSE(manifest_path("a/b/c/d:tag").has_value());
    CHECK_FALSE(manifest_path("../escape").has_value());
    CHECK_FALSE(manifest_path("./model:tag").has_value());
}

TEST_CASE("The runtime's model destination is proven by installed manifests", "[ollama][destination]")
{
    TempRoot root;
    const auto configured = root.path / "configured" / "models";
    const auto absent     = root.path / "not-created-yet" / "models";
    std::vector<Model> installed(3);
    installed[0].name = "example:q4";
    installed[1].name = "user/custom:latest";
    installed[2].name = "hf.co/org/repo:Q4_K_M";
    for (const auto &m : installed) touch(configured / *manifest_path(m.name));

    const auto found   = probe_destination({configured, DestinationSource::UserSetting}, installed);
    const auto missing = probe_destination({absent, DestinationSource::Default}, installed);
    CHECK(found.manifests_checked == 3);
    CHECK(found.manifests_found == 3);
    CHECK(missing.manifests_found == 0);
    REQUIRE(missing.free_bytes.has_value()); // nearest existing ancestor

    SECTION("a candidate holding every checked manifest is the verified destination")
    {
        const auto d = choose_destination({missing, found});
        CHECK(d.proof == DestinationProof::Manifests);
        CHECK(d.path == configured);
        CHECK(d.source == DestinationSource::UserSetting);
        CHECK(d.free_bytes == found.free_bytes);
    }
    SECTION("installed models found under no documented location leave free space unknown")
    {
        const auto d = choose_destination({missing});
        CHECK(d.proof == DestinationProof::Unknown);
        CHECK_FALSE(d.free_bytes.has_value());
        Hardware h = measured(64 * GiB, 0, 0);
        h.free_disk = d.free_bytes;
        CHECK(assess(installed_model("example:8b"), h, FitSettings{}).verdict == Fit::Unknown);
    }
    SECTION("with nothing installed the documented setting decides, using the least free space when views disagree")
    {
        const auto a = probe_destination({configured, DestinationSource::UserSetting}, {});
        const auto b = probe_destination({absent, DestinationSource::Default}, {});
        const auto d = choose_destination({a, b});
        CHECK(d.proof == DestinationProof::Configured);
        CHECK(d.candidates_disagree);
        REQUIRE(d.free_bytes.has_value());
        CHECK(*d.free_bytes <= *a.free_bytes);
        CHECK(*d.free_bytes <= *b.free_bytes);
    }
}

TEST_CASE("GPU backend evidence comes from the runtime and is bound to adapters, drivers and version", "[ollama][backend]")
{
    // Parsed from text so numbers carry the unsigned type the local API's JSON produces.
    const auto loaded = runtime_memory(Json::parse(R"({"models":[{"name":"a:1","size":100,"size_vram":60},
                                                                 {"name":"b:1","size":50,"size_vram":0},
                                                                 {"name":"c:1","size":10}]})"));
    REQUIRE(loaded.size() == 2); // an entry without exact GPU bytes is not evidence
    Hardware h = measured(16 * GiB, 8 * GiB, 7 * GiB);
    const auto seen = observe_backend(loaded, "0.12.3", adapter_identity(h.gpus), "2026-10-08T12:00:00Z");
    REQUIRE(seen.has_value());
    CHECK(seen->model == "a:1");
    const auto restored = load_backend(backend_json(*seen));
    CHECK(restored.size_vram == 60u);
    CHECK(restored.adapters == seen->adapters);

    apply_backend(h, std::nullopt, "0.12.3");
    CHECK(h.backend_state == BackendState::Unverified);
    CHECK_FALSE(h.backend_verified);

    apply_backend(h, restored, "0.12.3");
    CHECK(h.backend_state == BackendState::Gpu);
    CHECK(h.backend_verified);
    CHECK(h.usable_vram == 7 * GiB); // dedicated memory capped by the current budget

    SECTION("a driver change invalidates the observation")
    {
        Hardware updated = h;
        updated.gpus[0].driver_version = "32.0.15.7000";
        apply_backend(updated, restored, "0.12.3");
        CHECK(updated.backend_state == BackendState::Changed);
        CHECK_FALSE(updated.backend_verified);
    }
    SECTION("a runtime upgrade invalidates the observation")
    {
        apply_backend(h, restored, "0.13.0");
        CHECK(h.backend_state == BackendState::Changed);
    }
    SECTION("a model held only in system memory proves processor-only inference")
    {
        const auto cpu = observe_backend({RuntimeMemory{"b:1", 50, 0}}, "0.12.3", adapter_identity(h.gpus), "2026-10-08T12:00:00Z");
        apply_backend(h, cpu, "0.12.3");
        CHECK(h.backend_state == BackendState::CpuOnly);
        CHECK(h.usable_vram == 0u);
    }
    CHECK_THROWS(load_backend(Json::parse(R"({"schema":1,"observed_at":"x","runtime_version":"1","adapters":"","model":"a:1","size":1,"size_vram":2})")));
}

TEST_CASE("Every fit verdict is reachable from measured evidence and recomputes when inputs change", "[ollama][fit]")
{
    const auto model = installed_model("example:8b");
    FitSettings settings; // 4096 tokens, f16 cache
    Hardware h = measured(16 * GiB, 8 * GiB, 15 * GiB / 2);
    apply_backend(h, gpu_seen(h), "0.12.3");

    auto r = assess(model, h, settings);
    CHECK(r.verdict == Fit::RunsWell);
    CHECK(r.context_bytes == 536870912u);
    CHECK(r.memory_required == 4920734016u + 4920734016u / 5 + 536870912u);
    CHECK(r.measured_at == h.measured_at);

    SECTION("a smaller GPU needs partial offload")
    {
        Hardware small = measured(16 * GiB, 4 * GiB, 4 * GiB);
        apply_backend(small, gpu_seen(small), "0.12.3");
        CHECK(assess(model, small, settings).verdict == Fit::WithLimits);
        small.available_ram = 4 * GiB;
        CHECK(assess(model, small, settings).verdict == Fit::Unlikely);
    }
    SECTION("without the runtime's GPU evidence the verdict is processor-only or unknown")
    {
        Hardware unverified = measured(16 * GiB, 24 * GiB, 24 * GiB);
        apply_backend(unverified, std::nullopt, "0.12.3");
        const auto cpu = assess(model, unverified, settings);
        CHECK(cpu.verdict == Fit::WithLimits);
        CHECK(std::find(cpu.notes.begin(), cpu.notes.end(), FitNote::GpuUnverifiedCpuEstimate) != cpu.notes.end());
        unverified.available_ram = 4 * GiB;
        CHECK(assess(model, unverified, settings).verdict == Fit::Unknown);
    }
    SECTION("a destination without room is unlikely before any memory estimate")
    {
        Model catalog = model;
        catalog.installed = false;
        h.free_disk       = 5 * GiB;
        CHECK(assess(catalog, h, settings).verdict == Fit::Unlikely);
    }
    SECTION("a larger context or cache precision changes the verdict")
    {
        settings.context = 131072;
        CHECK(assess(model, h, settings).verdict == Fit::Unlikely);
        settings.cache = KvCache::Q4_0;
        CHECK(assess(model, h, settings).verdict == Fit::WithLimits);
    }
    SECTION("the estimate context is limited to the model's verified maximum")
    {
        Model short_context = model;
        short_context.context_length = 2048;
        const auto limited = assess(short_context, h, settings);
        CHECK(limited.context == 2048u);
        CHECK(std::find(limited.notes.begin(), limited.notes.end(), FitNote::ContextLimitedToModel) != limited.notes.end());
    }
    SECTION("a name is never evidence")
    {
        Model renamed = model;
        renamed.name = "zz-unrelated:custom";
        CHECK(assess(renamed, h, settings).verdict == r.verdict);
        Model named_only;
        named_only.name      = "example:8b";
        named_only.installed = true;
        named_only.bytes     = model.bytes;
        CHECK(assess(named_only, h, settings).verdict == Fit::Unknown);
    }
}

TEST_CASE("Estimate settings persist only documented values", "[ollama][fit]")
{
    FitSettings s;
    s.context = 16384;
    s.cache   = KvCache::Q8_0;
    const auto loaded = load_fit_settings(fit_settings_json(s));
    CHECK(loaded.context == 16384u);
    CHECK(loaded.cache == KvCache::Q8_0);
    CHECK_THROWS(load_fit_settings(Json::parse(R"({"schema":1,"context":3000,"kv_cache":"f16"})")));
    CHECK_THROWS(load_fit_settings(Json::parse(R"({"schema":1,"context":4096,"kv_cache":"f32"})")));
    CHECK(usable_vram({GpuAdapter{"a", "1", 4 * GiB, std::nullopt}, GpuAdapter{"b", "1", 12 * GiB, 10 * GiB}}) == 10 * GiB);
    CHECK_FALSE(usable_vram({}).has_value());
}
