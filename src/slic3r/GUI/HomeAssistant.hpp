#ifndef slic3r_GUI_HomeAssistant_hpp_
#define slic3r_GUI_HomeAssistant_hpp_

#include <functional>
#include <string>
#include <vector>

#include <wx/string.h>

namespace Slic3r { namespace GUI { namespace HomeAssistant {

// Minimal Home Assistant REST client. Connection comes from the config
// (`ha_url`, e.g. http://homeassistant.local:8123, and `ha_token`, a
// long-lived access token entered in the Smart home dialog and stored in
// BambuStudio.conf — the config-export secrets warning covers it). All calls
// are asynchronous (worker thread + curl via Slic3r::Http) and failures are
// log-only: a Home Assistant that is down must never nag or block the app.

struct Entity
{
    std::string entity_id;   // e.g. media_player.kitchen, light.desk
    std::string friendly_name;
    std::string state;
};

bool configured();

// GET /api/states filtered to the given domain ("media_player", "light", or
// "" for all). Callback fires on the UI thread.
void list_entities(const std::string &domain,
                   std::function<void(std::vector<Entity>, std::string /*error*/)> done);

// POST /api/services/<domain>/<service> with a JSON body.
void call_service(const std::string &domain, const std::string &service,
                  const std::string &json_body);

// Media player conveniences (rich controls in the Smart home dialog).
void media_command(const std::string &entity_id, const std::string &service); // media_play_pause, media_next_track, ...
void media_volume(const std::string &entity_id, double level_0_1);

// Speak `line` on every configured announcement speaker (`ha_speakers`,
// semicolon-separated media_player entity ids) via tts.speak / tts.google_
// translate_say fallback. No-op when unconfigured.
void speak_on_speakers(const wxString &line);

// Light actions ("Philips Hue and friends" — any HA light entity).
// Flash the configured lights (`ha_lights`) in a colour; used by the
// narrator hooks (error -> red flash, finish -> green pulse) when the
// corresponding toggles are on.
void flash_lights(int r, int g, int b, int flashes = 3);

} } } // namespace Slic3r::GUI::HomeAssistant

#endif // slic3r_GUI_HomeAssistant_hpp_
