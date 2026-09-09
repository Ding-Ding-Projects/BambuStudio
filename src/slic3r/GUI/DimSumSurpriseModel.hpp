#ifndef slic3r_GUI_DimSumSurpriseModel_hpp_
#define slic3r_GUI_DimSumSurpriseModel_hpp_

// Pure, wxWidgets-free model for the startup dim sum surprise.
//
// Everything that can be reasoned about without a window lives here so the
// Catch2 target under tests/dim_sum can exercise it directly: the ten percent
// draw, the once-per-process guard, the eligibility matrix, the public catalog
// parser, asset URL construction and the application-data cache record.
// DimSumSurprise.cpp owns the HTTP transfers and the card itself.

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r { namespace GUI { namespace DimSum {

// One launch in ten. The draw is a fresh std::random_device seed per process,
// never a stored counter, so the frequency can neither drift nor be tuned.
inline constexpr double SURPRISE_CHANCE = 0.10;

// The only source of dish names, alt text and photos. The catalog lives on
// the default branch; photos are release assets split across volumes.
inline constexpr const char *CATALOG_URL =
    "https://raw.githubusercontent.com/Ding-Ding-Projects/dim-sum-photos/main/catalog/index.json";
inline constexpr const char *RELEASE_ROOT =
    "https://github.com/Ding-Ding-Projects/dim-sum-photos/releases/download/";

// Published catalog volumes and the dish number range each one carries. The
// first candidate is the volume the number maps to; the rest are fallbacks so
// a re-published catalog that moved a dish between volumes is still found.
struct ReleaseVolume {
    const char *tag;
    int         first_number;
    int         last_number;
};
inline constexpr ReleaseVolume RELEASE_VOLUMES[] = {
    {"catalog-v1", 1, 995},
    {"catalog-v1-part-002", 996, 1985},
    {"catalog-v1-part-003", 1986, 3070},
};

// Transfer bounds. The catalog is about eight megabytes today; a hard cap of
// three times that keeps a runaway response from filling memory. Photos are
// square 1024px PNGs, well under the eight megabyte cap.
inline constexpr std::size_t CATALOG_SIZE_LIMIT_BYTES = 24u * 1024u * 1024u;
inline constexpr std::size_t PHOTO_SIZE_LIMIT_BYTES   = 8u * 1024u * 1024u;
inline constexpr long        CATALOG_TIMEOUT_SECONDS  = 30;
inline constexpr long        PHOTO_TIMEOUT_SECONDS    = 30;

// How many photos the cache keeps warm so consecutive surprises vary.
inline constexpr std::size_t PHOTO_CACHE_TARGET = 6;

// The card auto-dismisses after this long unless the pointer rests on it.
inline constexpr int AUTO_DISMISS_MILLISECONDS = 8000;

// The card is shown this long after the main frame settles, so it never races
// the startup work and never appears before the window is usable.
inline constexpr int STARTUP_DELAY_MILLISECONDS = 1500;

// Cache schema version written into catalog.json under data_dir()/dim-sum/.
inline constexpr int CACHE_SCHEMA_VERSION = 1;

struct Dish {
    std::string id;           // "hk-dish-0001"
    std::string name_en;      // catalog name.en (authoritative)
    std::string name_zh_hant; // catalog name.zhHant (authoritative)
    std::string alt_en;       // image.alt.en
    std::string alt_yue;      // image.alt.yue
    std::string image_file;   // basename of image.path, e.g. hk-dish-0001-classic-har-gow.png

    // Numeric part of the id, or -1 when the id is malformed.
    int number() const
    {
        const std::string_view prefix = "hk-dish-";
        if (id.size() <= prefix.size() || id.compare(0, prefix.size(), prefix) != 0)
            return -1;
        int value = 0;
        for (std::size_t i = prefix.size(); i < id.size(); ++i) {
            const char c = id[i];
            if (c < '0' || c > '9')
                return -1;
            value = value * 10 + (c - '0');
            if (value > 1000000)
                return -1;
        }
        return value;
    }

    bool operator==(const Dish &other) const
    {
        return id == other.id && name_en == other.name_en && name_zh_hant == other.name_zh_hant &&
               alt_en == other.alt_en && alt_yue == other.alt_yue && image_file == other.image_file;
    }
};

// What the application-data cache records beside the dishes, so the source of
// every name and photo is auditable without reading code.
struct CatalogCache {
    std::string       source_url;  // CATALOG_URL at the time of fetch
    std::string       revision;    // ETag of the raw catalog response (its blob hash), or "unknown"
    std::string       fetched_at;  // ISO-8601 UTC timestamp of the fetch
    std::vector<Dish> dishes;
};

// ------------------------------------------------------------------ the draw

// True on roughly one call in ten. Any std::mt19937 works; production seeds it
// from std::random_device, tests seed it deterministically.
inline bool draw(std::mt19937 &rng)
{
    std::uniform_real_distribution<double> unit(0.0, 1.0);
    return unit(rng) < SURPRISE_CHANCE;
}

// The draw happens once per process. The first caller wins; every later
// caller sees false regardless of what the draw returned, so a second call
// site can never produce a second card.
class LaunchGuard
{
public:
    // Returns true exactly once.
    bool claim()
    {
        if (m_claimed)
            return false;
        m_claimed = true;
        return true;
    }
    bool claimed() const { return m_claimed; }

private:
    bool m_claimed { false };
};

// ------------------------------------------------------------- eligibility

struct Eligibility {
    bool prior_launch_recorded { false }; // AppConfig marker written by an earlier launch
    bool onboarding_finished { false };   // firstguide/finish == "true"
    bool cli_input_files { false };       // a file or URL was passed on the command line
    bool config_wizard_shown { false };   // the startup wizard ran this launch
    bool startup_error_shown { false };   // an error dialog was raised during startup
    bool modal_dialog_open { false };     // any modal (update prompt, privacy, ...) is up
    bool quiet_mode { false };            // OS quiet/presentation/full-screen state
    bool main_frame_visible { false };    // the main window is shown and not iconized
};

// Why the surprise stayed home, for the once-per-launch log line.
inline const char *ineligibility_reason(const Eligibility &e)
{
    if (!e.onboarding_finished)   return "first run: onboarding not finished";
    if (!e.prior_launch_recorded) return "first run: no prior launch marker";
    if (e.cli_input_files)        return "mid-task: input files on the command line";
    if (e.config_wizard_shown)    return "config wizard shown this launch";
    if (e.startup_error_shown)    return "error shown during startup";
    if (e.modal_dialog_open)      return "a modal dialog is open";
    if (e.quiet_mode)             return "quiet mode active";
    if (!e.main_frame_visible)    return "main frame not visible";
    return nullptr;
}

inline bool eligible(const Eligibility &e) { return ineligibility_reason(e) == nullptr; }

// ------------------------------------------------------------ catalog parse

inline bool valid_dish_id(std::string_view id)
{
    // hk-dish-NNNN with at least four digits.
    const std::string_view prefix = "hk-dish-";
    if (id.size() < prefix.size() + 4 || id.substr(0, prefix.size()) != prefix)
        return false;
    for (std::size_t i = prefix.size(); i < id.size(); ++i)
        if (id[i] < '0' || id[i] > '9')
            return false;
    return true;
}

inline bool valid_image_file(std::string_view file)
{
    // hk-dish-NNNN-<slug>.png, slug of lowercase letters, digits and hyphens.
    if (file.size() < 4 || file.substr(file.size() - 4) != ".png")
        return false;
    const std::string_view stem = file.substr(0, file.size() - 4);
    const std::string_view prefix = "hk-dish-";
    if (stem.size() < prefix.size() + 6 || stem.substr(0, prefix.size()) != prefix)
        return false;
    std::size_t i = prefix.size();
    std::size_t digits = 0;
    while (i < stem.size() && stem[i] >= '0' && stem[i] <= '9') { ++i; ++digits; }
    if (digits < 4 || i >= stem.size() || stem[i] != '-')
        return false;
    for (; i < stem.size(); ++i) {
        const char c = stem[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    }
    return stem.back() != '-';
}

inline std::string basename_of(std::string_view path)
{
    const std::size_t slash = path.find_last_of("/\\");
    return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

// Parses the public catalog (schemaVersion 1.x, "dishes" array). Records that
// fail validation are skipped, never repaired: a dish without both names and a
// well-formed photo file name is not something the card may show. Returns an
// empty vector for malformed JSON or an unexpected shape.
inline std::vector<Dish> parse_catalog(const std::string &json_text)
{
    std::vector<Dish> out;
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(json_text);
    } catch (...) {
        return out;
    }
    if (!doc.is_object() || !doc.contains("dishes") || !doc["dishes"].is_array())
        return out;
    const std::string schema = doc.value("schemaVersion", std::string());
    if (schema.empty() || schema[0] != '1')
        return out;
    for (const auto &record : doc["dishes"]) {
        if (!record.is_object())
            continue;
        Dish dish;
        dish.id = record.value("id", std::string());
        if (record.contains("name") && record["name"].is_object()) {
            dish.name_en      = record["name"].value("en", std::string());
            dish.name_zh_hant = record["name"].value("zhHant", std::string());
        }
        if (record.contains("image") && record["image"].is_object()) {
            const auto &image = record["image"];
            dish.image_file = basename_of(image.value("path", std::string()));
            if (image.contains("alt") && image["alt"].is_object()) {
                dish.alt_en  = image["alt"].value("en", std::string());
                dish.alt_yue = image["alt"].value("yue", std::string());
            }
        }
        if (!valid_dish_id(dish.id) || dish.name_en.empty() || dish.name_zh_hant.empty() ||
            !valid_image_file(dish.image_file))
            continue;
        if (dish.alt_en.empty())
            dish.alt_en = dish.name_en;
        if (dish.alt_yue.empty())
            dish.alt_yue = dish.name_zh_hant;
        out.push_back(std::move(dish));
    }
    return out;
}

// ----------------------------------------------------------------- asset URL

// Ordered candidate URLs for a dish photo: the volume its number falls into
// first, then every other published volume as a fallback.
inline std::vector<std::string> asset_url_candidates(const Dish &dish)
{
    std::vector<std::string> urls;
    if (!valid_image_file(dish.image_file))
        return urls;
    const int number = dish.number();
    const ReleaseVolume *home = nullptr;
    for (const ReleaseVolume &volume : RELEASE_VOLUMES)
        if (number >= volume.first_number && number <= volume.last_number)
            home = &volume;
    auto url_for = [&dish](const ReleaseVolume &volume) {
        return std::string(RELEASE_ROOT) + volume.tag + "/" + dish.image_file;
    };
    if (home != nullptr)
        urls.push_back(url_for(*home));
    for (const ReleaseVolume &volume : RELEASE_VOLUMES)
        if (&volume != home)
            urls.push_back(url_for(volume));
    return urls;
}

// ------------------------------------------------------------- cache record

inline std::string serialize_cache(const CatalogCache &cache)
{
    nlohmann::json doc;
    doc["schemaVersion"] = CACHE_SCHEMA_VERSION;
    doc["sourceUrl"]     = cache.source_url;
    doc["revision"]      = cache.revision;
    doc["fetchedAt"]     = cache.fetched_at;
    nlohmann::json dishes = nlohmann::json::array();
    for (const Dish &dish : cache.dishes) {
        dishes.push_back({
            {"id", dish.id},
            {"en", dish.name_en},
            {"zhHant", dish.name_zh_hant},
            {"altEn", dish.alt_en},
            {"altYue", dish.alt_yue},
            {"file", dish.image_file},
        });
    }
    doc["dishes"] = std::move(dishes);
    return doc.dump(1);
}

inline std::optional<CatalogCache> parse_cache(const std::string &json_text)
{
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(json_text);
    } catch (...) {
        return std::nullopt;
    }
    if (!doc.is_object() || doc.value("schemaVersion", 0) != CACHE_SCHEMA_VERSION ||
        !doc.contains("dishes") || !doc["dishes"].is_array())
        return std::nullopt;
    CatalogCache cache;
    cache.source_url = doc.value("sourceUrl", std::string());
    cache.revision   = doc.value("revision", std::string());
    cache.fetched_at = doc.value("fetchedAt", std::string());
    for (const auto &record : doc["dishes"]) {
        if (!record.is_object())
            continue;
        Dish dish;
        dish.id           = record.value("id", std::string());
        dish.name_en      = record.value("en", std::string());
        dish.name_zh_hant = record.value("zhHant", std::string());
        dish.alt_en       = record.value("altEn", std::string());
        dish.alt_yue      = record.value("altYue", std::string());
        dish.image_file   = record.value("file", std::string());
        if (!valid_dish_id(dish.id) || dish.name_en.empty() || dish.name_zh_hant.empty() ||
            !valid_image_file(dish.image_file))
            continue;
        cache.dishes.push_back(std::move(dish));
    }
    return cache;
}

// ------------------------------------------------------------------ picking

// Picks uniformly among the dishes the predicate accepts (those whose photo is
// already on disk). Empty when none qualifies: the card shows nothing rather
// than a placeholder.
template <typename Predicate>
std::optional<Dish> pick_dish(const std::vector<Dish> &dishes, Predicate &&photo_cached, std::mt19937 &rng)
{
    std::vector<const Dish *> candidates;
    for (const Dish &dish : dishes)
        if (photo_cached(dish))
            candidates.push_back(&dish);
    if (candidates.empty())
        return std::nullopt;
    std::uniform_int_distribution<std::size_t> index(0, candidates.size() - 1);
    return *candidates[index(rng)];
}

// Uniform pick of a dish to prefetch, excluding ones already cached.
template <typename Predicate>
std::optional<Dish> pick_prefetch(const std::vector<Dish> &dishes, Predicate &&photo_cached, std::mt19937 &rng)
{
    return pick_dish(dishes, [&](const Dish &d) { return !photo_cached(d); }, rng);
}

// --------------------------------------------------------------------- copy

// Funny level 1 (fully serious) to 5 (maximum playfulness). The dish name is
// a fact and never changes; only the sentence around it does.
inline constexpr int FUNNY_LEVEL_MIN     = 1;
inline constexpr int FUNNY_LEVEL_MAX     = 5;
inline constexpr int FUNNY_LEVEL_DEFAULT = 3;

inline int clamp_funny_level(int level)
{
    return std::max(FUNNY_LEVEL_MIN, std::min(FUNNY_LEVEL_MAX, level));
}

inline int parse_funny_level(std::string_view stored, int fallback = FUNNY_LEVEL_DEFAULT)
{
    if (stored.empty())
        return clamp_funny_level(fallback);
    int value = 0;
    for (char c : stored) {
        if (c < '0' || c > '9')
            return clamp_funny_level(fallback);
        value = value * 10 + (c - '0');
        if (value > 100)
            return clamp_funny_level(fallback);
    }
    return clamp_funny_level(value);
}

// English source strings; the app wraps them in _L() so a catalog may override.
// %s is the bilingual dish name.
inline const char *surprise_line_english_source(int level)
{
    switch (clamp_funny_level(level)) {
    case 1: return "A one-in-ten launch. Today it is %s.";
    case 2: return "A one-in-ten launch. Today it is %s.";
    case 3: return "One launch in ten gets a dish. Yours is %s.";
    case 4: return "One launch in ten gets a dish, and yours is %s. Enjoy.";
    default: return "One launch in ten gets a dish, and the trolley stopped at yours: %s. Eat it before it goes cold.";
    }
}

// Hong Kong Cantonese lines, UTF-8. Kept beside the English so the bilingual
// card never depends on a catalog entry that may not exist yet.
inline const char *surprise_line_cantonese(int level)
{
    switch (clamp_funny_level(level)) {
    case 1: return "十分之一嘅機會。今次係%s。";
    case 2: return "十分之一嘅機會。今次係%s。";
    case 3: return "十次入面有一次有點心，你今次係%s。";
    case 4: return "十次入面有一次有點心，你今次抽到%s。慢用。";
    default: return "十次入面有一次有點心，架點心車啱啱停咗喺你度：%s。趁熱食啦。";
    }
}

inline const char *badge_english_source() { return "Dim sum surprise"; }
inline const char *badge_cantonese()      { return "點心驚喜"; }
inline const char *dismiss_english_source() { return "Dismiss"; }
inline const char *dismiss_cantonese()      { return "關閉"; }

// "Classic Har Gow · 蝦餃", or Cantonese first when that is the primary language.
inline std::string display_name(const Dish &dish, bool cantonese_first)
{
    const char *separator = " \xC2\xB7 ";
    return cantonese_first ? dish.name_zh_hant + separator + dish.name_en
                           : dish.name_en + separator + dish.name_zh_hant;
}

inline std::string alt_text(const Dish &dish, bool cantonese_first)
{
    return cantonese_first ? dish.alt_yue + " / " + dish.alt_en : dish.alt_en + " / " + dish.alt_yue;
}

// Replaces the single %s placeholder; a template without one gets the name appended.
inline std::string apply_dish_name(std::string line, const std::string &name)
{
    const std::size_t at = line.find("%s");
    if (at == std::string::npos)
        return line + " " + name;
    line.replace(at, 2, name);
    return line;
}

} } } // namespace Slic3r::GUI::DimSum

#endif // slic3r_GUI_DimSumSurpriseModel_hpp_
