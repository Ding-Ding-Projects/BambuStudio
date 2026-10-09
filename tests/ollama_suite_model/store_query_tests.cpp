#include <catch2/catch.hpp>

#include "libslic3r/OllamaSuite/OllamaCore.hpp"

#include <algorithm>

using namespace Slic3r::OllamaSuite;

namespace {

constexpr std::uint64_t GiB = 1024ull * 1024ull * 1024ull;

StoreEntry entry(const std::string &name, bool installed, bool running, std::string quantization, std::optional<std::uint64_t> bytes,
                 std::optional<std::set<std::string>> capabilities, Fit fit)
{
    StoreEntry e;
    e.model.name         = name;
    e.model.installed    = installed;
    e.model.running      = running;
    e.model.quantization = std::move(quantization);
    e.model.bytes        = bytes;
    if (capabilities) {
        e.model.capabilities          = *capabilities;
        e.model.capabilities_verified = true;
    }
    e.fit = fit;
    return e;
}

// Installed models carry verified metadata; catalog entries carry only their exact name until inspected.
std::vector<StoreEntry> inventory()
{
    return {
        entry("llama3.1:8b", true, true, "Q4_K_M", 4920734016ull, std::set<std::string>{"completion", "tools"}, Fit::RunsWell),
        entry("llama3.1:70b", false, false, "", std::nullopt, std::nullopt, Fit::Unknown),
        entry("llama3.1:8b-instruct-q8_0", false, false, "", 9 * GiB, std::nullopt, Fit::Unlikely),
        entry("llava:7b", true, false, "Q4_0", 4733363377ull, std::set<std::string>{"completion", "vision"}, Fit::WithLimits),
        entry("llava:13b", false, false, "", std::nullopt, std::nullopt, Fit::Unknown),
        entry("nomic-embed-text:latest", true, false, "F16", 274302450ull, std::set<std::string>{"embedding"}, Fit::RunsWell),
        entry("qwen3:0.6b", false, false, "", std::nullopt, std::nullopt, Fit::Unknown),
        entry("user/custom:latest", true, false, "", 3 * GiB, std::nullopt, Fit::Unknown),
    };
}

std::vector<std::string> names(const std::vector<StoreEntry> &entries, const std::vector<StoreRow> &rows)
{
    std::vector<std::string> out;
    for (const auto &r : rows)
        out.push_back(r.header ? "#" + std::to_string(r.members) : entries[r.entry].model.name);
    return out;
}

std::vector<std::string> matching(const StoreQuery &q)
{
    const auto entries = inventory();
    return names(entries, query_store(entries, q));
}

} // namespace

TEST_CASE("Store filters cover state, family, variant, capability, quantization, size and fit", "[ollama][store]")
{
    StoreQuery q;
    CHECK(matching(q).size() == 8);

    q       = {};
    q.state = StoreState::Running;
    CHECK(matching(q) == std::vector<std::string>{"llama3.1:8b"});
    q.state = StoreState::Installed; // installed includes running
    CHECK(matching(q) == std::vector<std::string>{"llama3.1:8b", "llava:7b", "nomic-embed-text:latest", "user/custom:latest"});
    q.state = StoreState::Catalog;
    CHECK(matching(q).size() == 4);

    q        = {};
    q.family = "llama3.1";
    CHECK(matching(q).size() == 3);
    q.variant = "8b";
    CHECK(matching(q) == std::vector<std::string>{"llama3.1:8b"});

    q            = {};
    q.capability = "vision";
    CHECK(matching(q) == std::vector<std::string>{"llava:7b"}); // llava:13b is not guessed from its name
    q.capability = "";
    CHECK(matching(q).size() == 5); // every entry whose capabilities are not verified

    q              = {};
    q.quantization = "Q4_0";
    CHECK(matching(q) == std::vector<std::string>{"llava:7b"});
    q.quantization = "";
    CHECK(matching(q).size() == 5);

    q      = {};
    q.size = SizeBand::Under2GiB;
    CHECK(matching(q) == std::vector<std::string>{"nomic-embed-text:latest"});
    q.size = SizeBand::From8To32GiB;
    CHECK(matching(q) == std::vector<std::string>{"llama3.1:8b-instruct-q8_0"});
    q.size = SizeBand::Unknown;
    CHECK(matching(q) == std::vector<std::string>{"llama3.1:70b", "llava:13b", "qwen3:0.6b"});

    q     = {};
    q.fit = Fit::RunsWell;
    CHECK(matching(q) == std::vector<std::string>{"llama3.1:8b", "nomic-embed-text:latest"});

    SECTION("filters combine and the text search narrows them further")
    {
        StoreQuery both;
        both.state      = StoreState::Installed;
        both.capability = "completion";
        const auto entries = inventory();
        CHECK(names(entries, query_store(entries, both)) == std::vector<std::string>{"llama3.1:8b", "llava:7b"});
        const auto rows = query_store(entries, both, [](const Model &m) { return m.name.find("llava") != std::string::npos; });
        CHECK(names(entries, rows) == std::vector<std::string>{"llava:7b"});
    }
}

TEST_CASE("Grouping adds counted headings in a meaningful order", "[ollama][store]")
{
    const auto entries = inventory();
    StoreQuery q;
    q.group = StoreGroup::Family;
    auto rows = query_store(entries, q);
    CHECK(names(entries, rows) == std::vector<std::string>{"#3", "llama3.1:70b", "llama3.1:8b", "llama3.1:8b-instruct-q8_0",
                                                           "#2", "llava:13b", "llava:7b",
                                                           "#1", "nomic-embed-text:latest",
                                                           "#1", "qwen3:0.6b",
                                                           "#1", "user/custom:latest"});
    CHECK(store_matches(rows) == 8);
    CHECK(model_family(entries[rows[0].entry].model.name) == "llama3.1");

    q.group = StoreGroup::Fit; // best verdict first, Unknown last
    rows    = query_store(entries, q);
    CHECK(entries[rows[0].entry].fit == Fit::RunsWell);
    CHECK(rows[0].members == 2);
    CHECK(entries[rows.back().entry].fit == Fit::Unknown);

    q.group = StoreGroup::Quantization; // unverified values group last
    rows    = query_store(entries, q);
    CHECK(entries[rows[0].entry].model.quantization == "F16");
    const auto last_heading = std::find_if(rows.rbegin(), rows.rend(), [](const StoreRow &r) { return r.header; });
    CHECK(entries[last_heading->entry].model.quantization.empty());
    CHECK(last_heading->members == 5);

    q.group = StoreGroup::State;
    rows    = query_store(entries, q);
    CHECK(store_state(entries[rows[0].entry].model) == StoreState::Running);
    CHECK(store_state(entries[rows.back().entry].model) == StoreState::Catalog);
}

TEST_CASE("Sorting by size keeps unknown sizes last in both directions", "[ollama][store]")
{
    const auto entries = inventory();
    StoreQuery q;
    q.sort    = StoreSort::SizeAscending;
    auto rows = names(entries, query_store(entries, q));
    CHECK(rows.front() == "nomic-embed-text:latest");
    CHECK(rows[4] == "llama3.1:8b-instruct-q8_0");
    CHECK(rows[5] == "llama3.1:70b"); // unknown sizes follow by name
    q.sort = StoreSort::SizeDescending;
    rows   = names(entries, query_store(entries, q));
    CHECK(rows.front() == "llama3.1:8b-instruct-q8_0");
    CHECK(rows[4] == "nomic-embed-text:latest");
    CHECK(rows.back() == "qwen3:0.6b");
    q.sort = StoreSort::Fit;
    rows   = names(entries, query_store(entries, q));
    CHECK(rows.front() == "llama3.1:8b");
    CHECK(rows[2] == "llava:7b");
}

TEST_CASE("Picker values come from the inventory itself", "[ollama][store]")
{
    const auto entries = inventory();
    const auto facets  = store_facets(entries);
    CHECK(facets.families == std::vector<std::string>{"llama3.1", "llava", "nomic-embed-text", "qwen3", "user/custom"});
    CHECK(facets.capabilities == std::vector<std::string>{"completion", "embedding", "tools", "vision"});
    CHECK(facets.quantizations == std::vector<std::string>{"F16", "Q4_0", "Q4_K_M"});
    CHECK(facets.unverified_capabilities);
    CHECK(facets.unverified_quantization);
    CHECK(family_variants(entries, "llama3.1") == std::vector<std::string>{"70b", "8b", "8b-instruct-q8_0"});
    CHECK(model_variant("example") == "latest");
    CHECK(size_band(entries[1].model) == SizeBand::Unknown);
}
