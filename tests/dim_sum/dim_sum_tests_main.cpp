#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <catch_main.hpp>

#include "slic3r/GUI/DimSumSurpriseModel.hpp"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

using namespace Slic3r::GUI::DimSum;

namespace {

Dish sample_dish(const std::string &id, const std::string &file)
{
    Dish dish;
    dish.id           = id;
    dish.name_en      = "Classic Har Gow";
    dish.name_zh_hant = "\xE8\x9D\xA6\xE9\xA4\x83"; // 蝦餃
    dish.alt_en       = "Warm tea-house photograph of Classic Har Gow";
    dish.alt_yue      = "\xE6\xB8\xAF\xE5\xBC\x8F\xE8\x8C\xB6\xE6\xA8\x93";
    dish.image_file   = file;
    return dish;
}

const char *SAMPLE_CATALOG = R"JSON({
  "schemaVersion": "1.0.0",
  "catalogStatus": "in-progress",
  "dishes": [
    {"id": "hk-dish-0001", "name": {"en": "Classic Har Gow", "zhHant": "蝦餃"},
     "image": {"path": "images/hk-dish-0001-classic-har-gow.png",
               "alt": {"en": "Warm tea-house photograph of Classic Har Gow", "yue": "港式茶樓木枱上嘅蝦餃"}}},
    {"id": "hk-dish-0011", "name": {"en": "Classic Siu Mai", "zhHant": "燒賣"},
     "image": {"path": "images/hk-dish-0011-classic-siu-mai.png"}},
    {"id": "hk-dish-0002", "name": {"en": "", "zhHant": "帶子蝦餃"},
     "image": {"path": "images/hk-dish-0002-scallop-har-gow.png"}},
    {"id": "hk-dish-0003", "name": {"en": "Bamboo Shoot Har Gow"},
     "image": {"path": "images/hk-dish-0003-bamboo-shoot-har-gow.png"}},
    {"id": "hk-dish-0004", "name": {"en": "Bad Path", "zhHant": "壞"},
     "image": {"path": "images/../etc/passwd.png"}},
    {"id": "not-a-dish", "name": {"en": "Bad Id", "zhHant": "壞"},
     "image": {"path": "images/hk-dish-0005-bad-id.png"}},
    {"id": "hk-dish-0006", "name": {"en": "No Image", "zhHant": "無圖"}},
    42,
    {"id": "hk-dish-0007", "name": {"en": "Upper Case", "zhHant": "大楷"},
     "image": {"path": "images/HK-DISH-0007-Upper.png"}}
  ]
})JSON";

} // namespace

TEST_CASE("The draw lands near one in ten with a deterministic seed", "[DimSum][draw]")
{
    std::mt19937 rng(20260908u);
    const int trials = 200000;
    int hits = 0;
    for (int i = 0; i < trials; ++i)
        if (draw(rng))
            ++hits;
    const double fraction = double(hits) / trials;
    INFO("fraction " << fraction);
    REQUIRE(fraction > 0.09);
    REQUIRE(fraction < 0.11);
    REQUIRE(SURPRISE_CHANCE == Approx(0.10));
}

TEST_CASE("Two differently seeded streams do not share a draw pattern", "[DimSum][draw]")
{
    std::mt19937 a(1u), b(2u);
    int same = 0;
    for (int i = 0; i < 1000; ++i)
        if (draw(a) == draw(b))
            ++same;
    // Independent 10 % Bernoulli streams agree about 82 % of the time; identical
    // streams would agree 100 % of the time.
    REQUIRE(same < 950);
}

TEST_CASE("The launch guard yields exactly once per process", "[DimSum][guard]")
{
    LaunchGuard guard;
    REQUIRE_FALSE(guard.claimed());
    REQUIRE(guard.claim());
    REQUIRE(guard.claimed());
    REQUIRE_FALSE(guard.claim());
    REQUIRE_FALSE(guard.claim());
}

TEST_CASE("Eligibility matrix", "[DimSum][eligibility]")
{
    Eligibility ok;
    ok.prior_launch_recorded = true;
    ok.onboarding_finished   = true;
    ok.main_frame_visible    = true;
    REQUIRE(eligible(ok));
    REQUIRE(ineligibility_reason(ok) == nullptr);

    SECTION("first run: onboarding unfinished")
    {
        Eligibility e = ok;
        e.onboarding_finished = false;
        REQUIRE_FALSE(eligible(e));
        REQUIRE(std::string(ineligibility_reason(e)).find("first run") != std::string::npos);
    }
    SECTION("first run: no prior launch marker")
    {
        Eligibility e = ok;
        e.prior_launch_recorded = false;
        REQUIRE_FALSE(eligible(e));
        REQUIRE(std::string(ineligibility_reason(e)).find("prior launch") != std::string::npos);
    }
    SECTION("mid-task: command line input files")
    {
        Eligibility e = ok;
        e.cli_input_files = true;
        REQUIRE_FALSE(eligible(e));
        REQUIRE(std::string(ineligibility_reason(e)).find("mid-task") != std::string::npos);
    }
    SECTION("config wizard shown")
    {
        Eligibility e = ok;
        e.config_wizard_shown = true;
        REQUIRE_FALSE(eligible(e));
    }
    SECTION("startup error shown")
    {
        Eligibility e = ok;
        e.startup_error_shown = true;
        REQUIRE_FALSE(eligible(e));
        REQUIRE(std::string(ineligibility_reason(e)).find("error") != std::string::npos);
    }
    SECTION("modal dialog open, such as an update prompt")
    {
        Eligibility e = ok;
        e.modal_dialog_open = true;
        REQUIRE_FALSE(eligible(e));
        REQUIRE(std::string(ineligibility_reason(e)).find("modal") != std::string::npos);
    }
    SECTION("OS quiet mode")
    {
        Eligibility e = ok;
        e.quiet_mode = true;
        REQUIRE_FALSE(eligible(e));
        REQUIRE(std::string(ineligibility_reason(e)).find("quiet") != std::string::npos);
    }
    SECTION("main frame hidden or iconized")
    {
        Eligibility e = ok;
        e.main_frame_visible = false;
        REQUIRE_FALSE(eligible(e));
    }
    SECTION("the default-constructed state is never eligible")
    {
        REQUIRE_FALSE(eligible(Eligibility {}));
    }
}

TEST_CASE("Catalog JSON parse keeps only complete, well-formed records", "[DimSum][catalog]")
{
    const std::vector<Dish> dishes = parse_catalog(SAMPLE_CATALOG);
    REQUIRE(dishes.size() == 2);

    REQUIRE(dishes[0].id == "hk-dish-0001");
    REQUIRE(dishes[0].name_en == "Classic Har Gow");
    REQUIRE(dishes[0].name_zh_hant == "\xE8\x9D\xA6\xE9\xA4\x83");
    REQUIRE(dishes[0].image_file == "hk-dish-0001-classic-har-gow.png");
    REQUIRE(dishes[0].alt_en == "Warm tea-house photograph of Classic Har Gow");
    REQUIRE_FALSE(dishes[0].alt_yue.empty());
    REQUIRE(dishes[0].number() == 1);

    // Missing alt text falls back to the dish name, never to an empty label.
    REQUIRE(dishes[1].id == "hk-dish-0011");
    REQUIRE(dishes[1].alt_en == "Classic Siu Mai");
    REQUIRE(dishes[1].alt_yue == dishes[1].name_zh_hant);
    REQUIRE(dishes[1].number() == 11);

    SECTION("malformed input yields nothing rather than throwing")
    {
        REQUIRE(parse_catalog("").empty());
        REQUIRE(parse_catalog("{").empty());
        REQUIRE(parse_catalog("[]").empty());
        REQUIRE(parse_catalog(R"({"dishes": {}})").empty());
        REQUIRE(parse_catalog(R"({"schemaVersion":"2.0.0","dishes":[]})").empty());
    }
}

TEST_CASE("Image file validation", "[DimSum][catalog]")
{
    REQUIRE(valid_image_file("hk-dish-0001-classic-har-gow.png"));
    REQUIRE(valid_image_file("hk-dish-3070-hong-kong-matcha-pear-steamed-bun.png"));
    REQUIRE_FALSE(valid_image_file("hk-dish-0001-classic-har-gow.PNG"));
    REQUIRE_FALSE(valid_image_file("hk-dish-0001.png"));
    REQUIRE_FALSE(valid_image_file("hk-dish-0001-.png"));
    REQUIRE_FALSE(valid_image_file("hk-dish-1-x.png"));
    REQUIRE_FALSE(valid_image_file("../hk-dish-0001-x.png"));
    REQUIRE_FALSE(valid_image_file("hk-dish-0001-x/y.png"));
    REQUIRE_FALSE(valid_image_file(""));
    REQUIRE(basename_of("images/hk-dish-0001-x.png") == "hk-dish-0001-x.png");
    REQUIRE(basename_of("hk-dish-0001-x.png") == "hk-dish-0001-x.png");
}

TEST_CASE("Asset URLs target the published catalog volume for the dish number first", "[DimSum][assets]")
{
    const std::string root = "https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/";

    SECTION("volume one")
    {
        auto urls = asset_url_candidates(sample_dish("hk-dish-0001", "hk-dish-0001-classic-har-gow.png"));
        REQUIRE(urls.size() == 3);
        REQUIRE(urls[0] == root + "catalog-v1/hk-dish-0001-classic-har-gow.png");
        REQUIRE(urls[1] == root + "catalog-v1-part-002/hk-dish-0001-classic-har-gow.png");
        REQUIRE(urls[2] == root + "catalog-v1-part-003/hk-dish-0001-classic-har-gow.png");
    }
    SECTION("volume two boundary")
    {
        auto urls = asset_url_candidates(sample_dish("hk-dish-0996", "hk-dish-0996-typhoon-shelter-okra.png"));
        REQUIRE(urls[0] == root + "catalog-v1-part-002/hk-dish-0996-typhoon-shelter-okra.png");
        auto last_of_one = asset_url_candidates(sample_dish("hk-dish-0995", "hk-dish-0995-yuzu-honey-cauliflower.png"));
        REQUIRE(last_of_one[0] == root + "catalog-v1/hk-dish-0995-yuzu-honey-cauliflower.png");
    }
    SECTION("volume three")
    {
        auto urls = asset_url_candidates(sample_dish("hk-dish-2500", "hk-dish-2500-some-bun.png"));
        REQUIRE(urls[0] == root + "catalog-v1-part-003/hk-dish-2500-some-bun.png");
    }
    SECTION("a number past every known volume tries them all in order")
    {
        auto urls = asset_url_candidates(sample_dish("hk-dish-4000", "hk-dish-4000-future-dish.png"));
        REQUIRE(urls.size() == 3);
        REQUIRE(urls[0] == root + "catalog-v1/hk-dish-4000-future-dish.png");
    }
    SECTION("an invalid file name produces no URL at all")
    {
        REQUIRE(asset_url_candidates(sample_dish("hk-dish-0001", "../evil.png")).empty());
    }
}

TEST_CASE("Cache record round trip", "[DimSum][cache]")
{
    CatalogCache cache;
    cache.source_url = CATALOG_URL;
    cache.revision   = "f77ea1169db0bfc17365414c44ff495a823c6823";
    cache.fetched_at = "2026-09-08T12:00:00Z";
    cache.dishes.push_back(sample_dish("hk-dish-0001", "hk-dish-0001-classic-har-gow.png"));
    cache.dishes.push_back(sample_dish("hk-dish-0011", "hk-dish-0011-classic-siu-mai.png"));

    const std::string text = serialize_cache(cache);
    REQUIRE(text.find("sourceUrl") != std::string::npos);
    REQUIRE(text.find(cache.revision) != std::string::npos);

    const auto back = parse_cache(text);
    REQUIRE(back.has_value());
    REQUIRE(back->source_url == cache.source_url);
    REQUIRE(back->revision == cache.revision);
    REQUIRE(back->fetched_at == cache.fetched_at);
    REQUIRE(back->dishes == cache.dishes);

    SECTION("a corrupt or foreign record is rejected")
    {
        REQUIRE_FALSE(parse_cache("").has_value());
        REQUIRE_FALSE(parse_cache("{}").has_value());
        REQUIRE_FALSE(parse_cache(R"({"schemaVersion":99,"dishes":[]})").has_value());
    }
    SECTION("a tampered dish entry is dropped, the rest survive")
    {
        std::string tampered = text;
        const auto at = tampered.find("hk-dish-0011-classic-siu-mai.png");
        REQUIRE(at != std::string::npos);
        tampered.replace(at, 32, "../../evil.png");
        const auto parsed = parse_cache(tampered);
        REQUIRE(parsed.has_value());
        REQUIRE(parsed->dishes.size() == 1);
        REQUIRE(parsed->dishes[0].id == "hk-dish-0001");
    }
}

TEST_CASE("Picking only ever returns a dish whose photo is cached", "[DimSum][pick]")
{
    std::vector<Dish> dishes = {
        sample_dish("hk-dish-0001", "hk-dish-0001-a.png"),
        sample_dish("hk-dish-0002", "hk-dish-0002-b.png"),
        sample_dish("hk-dish-0003", "hk-dish-0003-c.png"),
    };
    std::mt19937 rng(7u);
    auto cached = [](const Dish &d) { return d.id == "hk-dish-0002"; };
    for (int i = 0; i < 50; ++i) {
        auto pick = pick_dish(dishes, cached, rng);
        REQUIRE(pick.has_value());
        REQUIRE(pick->id == "hk-dish-0002");
    }
    REQUIRE_FALSE(pick_dish(dishes, [](const Dish &) { return false; }, rng).has_value());
    auto prefetch = pick_prefetch(dishes, cached, rng);
    REQUIRE(prefetch.has_value());
    REQUIRE(prefetch->id != "hk-dish-0002");
}

TEST_CASE("Copy honours the funny level but never touches the dish name", "[DimSum][copy]")
{
    REQUIRE(parse_funny_level("") == 3);
    REQUIRE(parse_funny_level("1") == 1);
    REQUIRE(parse_funny_level("5") == 5);
    REQUIRE(parse_funny_level("9") == 5);
    REQUIRE(parse_funny_level("0") == 1);
    REQUIRE(parse_funny_level("abc") == 3);
    REQUIRE(parse_funny_level("abc", 1) == 1);

    const Dish dish = sample_dish("hk-dish-0001", "hk-dish-0001-classic-har-gow.png");
    const std::string name = display_name(dish, false);
    REQUIRE(name == "Classic Har Gow \xC2\xB7 \xE8\x9D\xA6\xE9\xA4\x83");
    REQUIRE(display_name(dish, true) == "\xE8\x9D\xA6\xE9\xA4\x83 \xC2\xB7 Classic Har Gow");
    REQUIRE(alt_text(dish, false).find(dish.alt_en) == 0);
    REQUIRE(alt_text(dish, true).find(dish.alt_yue) == 0);

    for (int level = 1; level <= 5; ++level) {
        const std::string en  = apply_dish_name(surprise_line_english_source(level), name);
        const std::string yue = apply_dish_name(surprise_line_cantonese(level), name);
        INFO("level " << level << ": " << en);
        REQUIRE(en.find(name) != std::string::npos);
        REQUIRE(yue.find(name) != std::string::npos);
        REQUIRE(en.find("%s") == std::string::npos);
        REQUIRE(yue.find("%s") == std::string::npos);
    }
    REQUIRE(std::string(surprise_line_english_source(1)) != surprise_line_english_source(5));
    REQUIRE(std::string(surprise_line_english_source(1)).find("trolley") == std::string::npos);
    REQUIRE(std::string(surprise_line_english_source(5)).find("trolley") != std::string::npos);
    REQUIRE(apply_dish_name("no placeholder", "X") == "no placeholder X");
}

// Optional: run the parser over a real copy of the public catalog. The file is
// never committed; point DIM_SUM_TEST_CATALOG at a downloaded index.json.
TEST_CASE("The real public catalog parses", "[DimSum][catalog][.optional]")
{
    const char *path = std::getenv("DIM_SUM_TEST_CATALOG");
    if (path == nullptr) {
        WARN("DIM_SUM_TEST_CATALOG not set; skipping the live-catalog parse");
        return;
    }
    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.good());
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto dishes = parse_catalog(text);
    REQUIRE(dishes.size() >= 2000);
    REQUIRE(dishes[0].id == "hk-dish-0001");
    REQUIRE(dishes[0].name_en == "Classic Har Gow");
    REQUIRE(dishes[0].name_zh_hant == "\xE8\x9D\xA6\xE9\xA4\x83");
    for (const Dish &dish : dishes)
        REQUIRE_FALSE(asset_url_candidates(dish).empty());
}
