#ifndef slic3r_GUI_SmartHomeDialog_hpp_
#define slic3r_GUI_SmartHomeDialog_hpp_

#include "Widgets/MD3Dialog.hpp"
#include "HomeAssistant.hpp"

#include <vector>

class Label;
class Button;
class CheckBox;
class SearchField;
class wxListBox;
class wxSlider;
class wxTextCtrl;

namespace Slic3r { namespace GUI {

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

private:
    void refresh_entities();
    void rebuild_list();
    const HomeAssistant::Entity *selected_entity() const;

    std::vector<HomeAssistant::Entity> m_entities;
    std::vector<int>                   m_visible; // filtered indices

    wxTextCtrl  *m_url { nullptr };
    wxTextCtrl  *m_token { nullptr };
    Label       *m_status { nullptr };
    SearchField *m_search { nullptr };
    wxListBox   *m_list { nullptr };
    wxSlider    *m_volume { nullptr };
    Label       *m_speakers_label { nullptr };
    Label       *m_lights_label { nullptr };
    CheckBox    *m_narrator_toggle { nullptr };
    CheckBox    *m_flash_error_toggle { nullptr };
    CheckBox    *m_flash_finish_toggle { nullptr };
};

} } // namespace Slic3r::GUI

#endif // slic3r_GUI_SmartHomeDialog_hpp_
