#include <catch2/catch.hpp>

#include "libslic3r/OllamaSuite/OllamaCore.hpp"

#include <map>
#include <stdexcept>

using namespace Slic3r::OllamaSuite;

namespace {

CatalogPage page(std::string path, std::vector<std::string> names, std::vector<std::string> next, std::optional<std::size_t> total = std::nullopt,
                 std::string identity = "identity")
{
    CatalogPage p;
    p.path              = path;
    p.response_identity = identity + ":" + path;
    p.fetched_at        = "2026-10-09T00:00:00Z";
    p.names             = std::move(names);
    p.next_pages        = std::move(next);
    p.advertised_count  = total;
    return p;
}

// The official HTML publishes links but no totals: two families, one with a second tag page.
std::map<std::string, CatalogPage> html_source()
{
    std::map<std::string, CatalogPage> pages;
    pages["/library"]              = page("/library", {"alpha", "beta"}, {});
    pages["/library/alpha/tags"]   = page("/library/alpha/tags", {"alpha:q4", "alpha:q8"}, {"/library/alpha/tags?page=2"});
    pages["/library/alpha/tags?page=2"] = page("/library/alpha/tags?page=2", {"alpha:f16"}, {});
    pages["/library/beta/tags"]    = page("/library/beta/tags", {"beta:latest"}, {});
    return pages;
}

// A stand-in digest: deterministic and sensitive to every byte, which is all a revision needs here.
std::string fnv(const std::string &text)
{
    std::uint64_t h = 1469598103934665603ull;
    for (unsigned char c : text) h = (h ^ c) * 1099511628211ull;
    return std::to_string(h);
}

CatalogSnapshot traverse(const std::map<std::string, CatalogPage> &pages)
{
    std::atomic_bool cancel{false};
    return refresh_catalog([&](const std::string &path) { return pages.at(path); }, cancel);
}

} // namespace

TEST_CASE("A full traversal without published totals is verified as traversed and kept", "[ollama][catalog]")
{
    auto s = traverse(html_source());
    CHECK(s.verdict == CatalogVerdict::Traversed);
    CHECK(catalog_verified(s));
    CHECK_FALSE(s.complete); // never certified without a total to reconcile
    CHECK(s.pages.size() == 4);
    CHECK(s.families == 2);
    CHECK(s.models.size() == 4);
    CHECK(s.last_successful_refresh == s.refreshed_at);
    CHECK(s.failure == CatalogFailure::None);

    SECTION("published totals that reconcile certify the catalog")
    {
        auto pages                                 = html_source();
        pages["/library"].advertised_count         = 2;
        pages["/library/alpha/tags"].advertised_count = 3;
        pages["/library/alpha/tags?page=2"].advertised_count = 3;
        pages["/library/beta/tags"].advertised_count = 1;
        CHECK(traverse(pages).verdict == CatalogVerdict::Certified);
        pages["/library/alpha/tags?page=2"].advertised_count = 4;
        CHECK(traverse(pages).verdict == CatalogVerdict::Failed); // the count changed during pagination
    }
}

TEST_CASE("A failed traversal keeps no entries and says why", "[ollama][catalog]")
{
    SECTION("an unreachable page")
    {
        auto pages = html_source();
        pages.erase("/library/beta/tags");
        const auto s = traverse(pages);
        CHECK(s.verdict == CatalogVerdict::Failed);
        CHECK(s.failure == CatalogFailure::Unavailable);
        CHECK(s.models.empty()); // never a guessed or partial entry
        CHECK_FALSE(catalog_verified(s));
        CHECK(s.last_successful_refresh.empty());
    }
    SECTION("a page whose structure no longer matches")
    {
        auto pages = html_source();
        pages["/library/beta/tags"].names.clear();
        CHECK(traverse(pages).failure == CatalogFailure::Malformed);
    }
    SECTION("cancellation")
    {
        std::atomic_bool cancel{true};
        const auto pages = html_source();
        const auto s     = refresh_catalog([&](const std::string &path) { return pages.at(path); }, cancel);
        CHECK(s.failure == CatalogFailure::Cancelled);
    }
}

TEST_CASE("The revision identifies the exact source responses", "[ollama][catalog]")
{
    auto s = traverse(html_source());
    s.revision = catalog_revision(s, fnv);
    CHECK(s.revision == catalog_revision(traverse(html_source()), fnv));
    auto changed = html_source();
    changed["/library/beta/tags"].response_identity = "different-bytes";
    CHECK(catalog_revision(traverse(changed), fnv) != s.revision);
    CHECK_THROWS(catalog_revision(CatalogSnapshot{}, fnv));
}

TEST_CASE("The last verified traversal persists with its verdict, revision, page count and time", "[ollama][catalog]")
{
    auto s     = traverse(html_source());
    s.revision = catalog_revision(s, fnv);
    const auto saved = catalog_json(s);
    CHECK(saved.at("schema") == 2);
    CHECK(saved.at("verdict") == "traversed");
    CHECK(saved.at("page_count") == 4);
    CHECK(saved.at("revision") == s.revision);

    const auto loaded = load_catalog(saved, fnv);
    CHECK(loaded.cached);
    CHECK(loaded.verdict == CatalogVerdict::Traversed);
    CHECK(loaded.revision == s.revision);
    CHECK(loaded.refreshed_at == s.refreshed_at);
    CHECK(loaded.last_successful_refresh == s.last_successful_refresh);
    CHECK(loaded.models.size() == 4);

    SECTION("a saved entry edited after verification is rejected")
    {
        auto tampered = saved;
        tampered["pages"][1]["names"].push_back("alpha:invented");
        CHECK_THROWS(load_catalog(tampered, fnv));
    }
    SECTION("a missing saved page fails closed instead of presenting a partial catalog")
    {
        auto broken = saved;
        broken["pages"].erase(0);
        const auto partial = load_catalog(broken, fnv);
        CHECK_FALSE(catalog_verified(partial));
        CHECK(partial.models.empty());
    }
}

TEST_CASE("Refresh attempts are recorded separately from the verified catalog", "[ollama][catalog]")
{
    auto pages = html_source();
    pages.erase("/library");
    const auto failed  = traverse(pages);
    const auto attempt = load_attempt(attempt_json(catalog_attempt(failed)));
    CHECK(attempt.verdict == CatalogVerdict::Failed);
    CHECK(attempt.failure == CatalogFailure::Unavailable);
    CHECK(attempt.pages == 0);
    CHECK(attempt.attempted_at == failed.refreshed_at);
    CHECK_THROWS(load_attempt(Json::parse(R"({"schema":1,"attempted_at":"yesterday","revision":"","verdict":"failed","failure":2,"pages":0})")));
}

TEST_CASE("Catalog age and staleness come from the last successful refresh", "[ollama][catalog]")
{
    CHECK(utc_seconds("1970-01-01T00:00:00Z") == 0);
    CHECK(utc_seconds("2026-10-09T00:00:00Z") == 1791504000);
    CHECK(utc_seconds("2000-02-29T12:34:56Z") == 951827696);
    CHECK_FALSE(utc_seconds("2026-13-09T00:00:00Z").has_value());
    CHECK_FALSE(utc_seconds("2026-10-09 00:00:00").has_value());

    auto s = traverse(html_source());
    s.last_successful_refresh = "2026-10-09T00:00:00Z";
    CHECK(catalog_age(s, 1791504000 + 3600) == 3600);
    CHECK_FALSE(catalog_stale(s, 1791504000 + 3600));
    CHECK(catalog_stale(s, 1791504000 + catalog_stale_seconds + 1));
    CHECK(catalog_age(s, 1791504000 - 60) == 0); // a clock behind the record never shows a negative age
    CHECK_FALSE(catalog_age(CatalogSnapshot{}, 1791504000).has_value());
}
