#include "HomeAssistant.hpp"

#include "GUI_App.hpp"
#include "slic3r/Utils/Http.hpp"

#include "libslic3r/AppConfig.hpp"

#include "nlohmann/json.hpp"

#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/log/trivial.hpp>

#include <chrono>
#include <thread>

#include <wx/app.h>

namespace Slic3r { namespace GUI { namespace HomeAssistant {

namespace {

std::string base_url()
{
    std::string url = wxGetApp().app_config->get("ha_url");
    while (!url.empty() && url.back() == '/')
        url.pop_back();
    return url;
}

std::string token()
{
    return wxGetApp().app_config->get("ha_token");
}

std::vector<std::string> configured_list(const std::string &key)
{
    std::vector<std::string> out;
    const std::string raw = wxGetApp().app_config->get(key);
    if (raw.empty())
        return out;
    boost::split(out, raw, boost::is_any_of(";"), boost::token_compress_on);
    out.erase(std::remove_if(out.begin(), out.end(), [](const std::string &s) { return s.empty(); }), out.end());
    return out;
}

void post_service_async(const std::string &path, const std::string &body)
{
    if (!configured())
        return;
    const std::string url  = base_url() + path;
    const std::string auth = "Bearer " + token();
    std::thread([url, auth, body]() {
        auto http = Http::post(url);
        http.header("Authorization", auth)
            .header("Content-Type", "application/json")
            .set_post_body(body)
            .timeout_max(10)
            .on_error([](std::string, std::string error, unsigned) {
                BOOST_LOG_TRIVIAL(info) << "HomeAssistant: request failed: " << error;
            })
            .perform_sync();
    }).detach();
}

} // namespace

bool configured()
{
    return !base_url().empty() && !token().empty();
}

void list_entities(const std::string &domain,
                   std::function<void(std::vector<Entity>, std::string)> done)
{
    if (!configured()) {
        done({}, "Home Assistant is not configured");
        return;
    }
    const std::string url  = base_url() + "/api/states";
    const std::string auth = "Bearer " + token();
    std::thread([url, auth, domain, done]() {
        std::vector<Entity> entities;
        std::string         error;
        auto http = Http::get(url);
        http.header("Authorization", auth)
            .timeout_max(10)
            .on_complete([&](std::string body, unsigned) {
                try {
                    for (const auto &state : nlohmann::json::parse(body)) {
                        Entity entity;
                        entity.entity_id = state.value("entity_id", "");
                        if (!domain.empty() && entity.entity_id.rfind(domain + ".", 0) != 0)
                            continue;
                        entity.state = state.value("state", "");
                        if (state.contains("attributes"))
                            entity.friendly_name = state["attributes"].value("friendly_name", "");
                        if (entity.friendly_name.empty())
                            entity.friendly_name = entity.entity_id;
                        entities.push_back(std::move(entity));
                    }
                } catch (const std::exception &e) {
                    error = e.what();
                }
            })
            .on_error([&](std::string, std::string err, unsigned) { error = err; })
            .perform_sync();
        wxTheApp->CallAfter([done, entities = std::move(entities), error]() { done(entities, error); });
    }).detach();
}

void call_service(const std::string &domain, const std::string &service, const std::string &json_body)
{
    post_service_async("/api/services/" + domain + "/" + service, json_body);
}

void media_command(const std::string &entity_id, const std::string &service)
{
    call_service("media_player", service,
                 nlohmann::json{{"entity_id", entity_id}}.dump());
}

void media_volume(const std::string &entity_id, double level_0_1)
{
    call_service("media_player", "volume_set",
                 nlohmann::json{{"entity_id", entity_id}, {"volume_level", level_0_1}}.dump());
}

void speak_on_speakers(const wxString &line)
{
    const auto speakers = configured_list("ha_speakers");
    if (speakers.empty() || !configured())
        return;
    for (const std::string &speaker : speakers) {
        // Modern HA: tts.speak against the default TTS engine entity.
        nlohmann::json body = {
            {"entity_id", wxGetApp().app_config->get("ha_tts_entity").empty()
                              ? std::string("tts.google_translate_en_com")
                              : wxGetApp().app_config->get("ha_tts_entity")},
            {"media_player_entity_id", speaker},
            {"message", std::string(line.ToUTF8())},
        };
        call_service("tts", "speak", body.dump());
    }
}

void add_printers(const std::vector<PrinterHandover> &printers,
                  std::function<void(int, std::vector<std::string>)> done)
{
    if (!configured()) {
        wxTheApp->CallAfter([done]() {
            done(0, {std::string("Home Assistant is not configured")});
        });
        return;
    }
    if (printers.empty()) {
        wxTheApp->CallAfter([done]() { done(0, {}); });
        return;
    }

    const std::string url  = base_url() + "/api/services/bambu_lab/add_printer";
    const std::string auth = "Bearer " + token();

    std::thread([url, auth, printers, done]() {
        int                      added = 0;
        std::vector<std::string> errors;

        for (const PrinterHandover &printer : printers) {
            nlohmann::json body = {
                {"serial", printer.serial},
                {"host", printer.host},
                {"access_code", printer.access_code},
            };
            if (!printer.name.empty())
                body["name"] = printer.name;

            bool ok = false;
            auto http = Http::post(url);
            http.header("Authorization", auth)
                .header("Content-Type", "application/json")
                .set_post_body(body.dump())
                .timeout_max(30) // the integration verifies the printer before answering
                .on_complete([&ok](std::string, unsigned) { ok = true; })
                .on_error([&errors, &printer](std::string body, std::string error, unsigned status) {
                    // The serial identifies which printer failed; the body may
                    // echo request data, so it is deliberately not included.
                    errors.push_back(printer.serial + ": " +
                                     (status ? "HTTP " + std::to_string(status) : error));
                })
                .perform_sync();
            if (ok)
                ++added;
        }

        wxTheApp->CallAfter([done, added, errors = std::move(errors)]() { done(added, errors); });
    }).detach();
}

void flash_lights(int r, int g, int b, int flashes)
{
    const auto lights = configured_list("ha_lights");
    if (lights.empty() || !configured())
        return;
    // These are the user's REAL room lights: never leave them stuck on the
    // alert colour. Snapshot every configured light into a Home Assistant
    // scene first, flash, then restore the snapshot a few seconds later.
    // Even if this app dies mid-flash, the snapshot scene
    // (scene.bambustudio_light_restore) survives inside Home Assistant and
    // can be applied manually.
    nlohmann::json snapshot = {
        {"scene_id", "bambustudio_light_restore"},
        {"snapshot_entities", lights},
    };
    call_service("scene", "create", snapshot.dump());
    for (const std::string &light : lights) {
        nlohmann::json body = {
            {"entity_id", light},
            {"rgb_color", {r, g, b}},
            // HA's own flash effect keeps timing on the bridge side.
            {"flash", flashes > 1 ? "long" : "short"},
        };
        call_service("light", "turn_on", body.dump());
    }
    // Deferred restore (worker thread; fire twice for reliability — the calls
    // are idempotent). 4s covers the "long" flash duration.
    std::thread([]() {
        std::this_thread::sleep_for(std::chrono::seconds(4));
        call_service("scene", "turn_on", nlohmann::json{{"entity_id", "scene.bambustudio_light_restore"}}.dump());
        std::this_thread::sleep_for(std::chrono::seconds(2));
        call_service("scene", "turn_on", nlohmann::json{{"entity_id", "scene.bambustudio_light_restore"}}.dump());
    }).detach();
}

} } } // namespace Slic3r::GUI::HomeAssistant
