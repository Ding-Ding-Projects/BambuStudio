#ifndef slic3r_GUI_SmartHomeDialog_hpp_
#define slic3r_GUI_SmartHomeDialog_hpp_

#include "Widgets/MD3Dialog.hpp"
#include "Widgets/Slider.hpp"
#include "Widgets/ListBox.hpp"
#include "HomeAssistant.hpp"

#include <memory>
#include <string>
#include <vector>

#include <wx/timer.h>

class Label;
class Button;
class CheckBox;
class SearchField;
class TextInput;
class wxSlider;
class wxScrolledWindow;

namespace Slic3r { namespace GUI {

namespace HomeAssistant {
class SharingService;
}

// Smart home hub: Home Assistant connection, an entity browser (search bar +
// listbox over media players and lights), a media-player card with rich
// controls (previous / play-pause / next, volume), announcement-speaker
// selection for the TTS narrator, and alert-light bindings (flash the
// configured lights red on printer errors, pulse green on finish — with a
// snapshot/restore guard so real room lights never stay stuck on the alert
// colour).
//
// The long-lived HA token is entered here and stored in BambuStudio.conf
// (covered by the config-export secrets warning). Everything degrades
// gracefully when Home Assistant is absent.
class SmartHomeDialog final : public MD3Dialog
{
public:
    explicit SmartHomeDialog(wxWindow *parent);
    ~SmartHomeDialog() override;

private:
    void refresh_entities();
    void rebuild_list();
    void add_printers_to_home_assistant();
    void set_discovery_sharing(bool enabled, bool expired = false);
    void update_wrapped_label(Label *label, const wxString &text);
    void update_config_limit_notice();
    void relayout_content();
    void constrain_to_work_area(bool use_preferred_size);
    void on_dpi_changed(const wxRect &suggested_rect) override;
    const HomeAssistant::Entity *selected_entity() const;

    std::vector<HomeAssistant::Entity> m_entities;
    std::vector<int>                   m_visible; // filtered indices

    TextInput   *m_url { nullptr };
    TextInput   *m_token { nullptr };
    Label       *m_status { nullptr };
    Label       *m_config_limit_status { nullptr };
    Button      *m_add_printers { nullptr };
    CheckBox    *m_discovery_toggle { nullptr };
    Label       *m_discovery_status { nullptr };
    wxScrolledWindow *m_scroll { nullptr };
    SearchField *m_search { nullptr };
    ListBox     *m_list { nullptr };
    Label       *m_results_status { nullptr };
    Slider      *m_volume { nullptr };
    Label       *m_speakers_label { nullptr };
    Label       *m_lights_label { nullptr };
    CheckBox    *m_narrator_toggle { nullptr };
    CheckBox    *m_flash_error_toggle { nullptr };
    CheckBox    *m_flash_finish_toggle { nullptr };
    std::unique_ptr<HomeAssistant::SharingService> m_sharing_service;
    wxTimer      m_volume_debounce_timer;
    wxTimer      m_discovery_expiry_timer;
    std::string  m_pending_volume_entity;
    double       m_pending_volume_level { 0.5 };
    bool         m_entity_refresh_in_flight { false };
    bool         m_entity_refresh_pending { false };
    bool         m_entity_fetch_truncated { false };
};

} } // namespace Slic3r::GUI

#endif // slic3r_GUI_SmartHomeDialog_hpp_
