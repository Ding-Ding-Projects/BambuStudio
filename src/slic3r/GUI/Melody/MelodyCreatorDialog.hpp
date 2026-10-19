#ifndef slic3r_GUI_MelodyCreatorDialog_hpp_
#define slic3r_GUI_MelodyCreatorDialog_hpp_

// The Melody creator: compose a tune the printer plays through M1006, hear a
// preview on this computer, play it on the connected printer, copy its G-code,
// put it into the printer preset's start or end G-code, and keep it as a JSON
// file. The model lives in MelodyModel.hpp and is tested on its own.

#include "MelodyModel.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3Dialog.hpp"
#include "../Widgets/MD3Grid.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/TextInput.hpp"

#include <memory>
#include <string>
#include <vector>

class wxGridEvent;
class wxSound;

namespace Slic3r { namespace GUI {

class MelodyCreatorDialog final : public MD3Dialog
{
public:
    explicit MelodyCreatorDialog(wxWindow *parent);
    ~MelodyCreatorDialog() override;

    const Melody &melody() const { return m_melody; }

private:
    // model <-> controls
    void refresh_grid();
    void read_name_and_tick();
    void on_cell_changed(wxGridEvent &event);
    void update_status(const wxString &message = wxEmptyString, bool problem = false);
    bool melody_ready(wxString &why) const;
    int  selected_step() const;
    void select_step(int index);

    // actions
    void add_step();
    void duplicate_step();
    void move_step(int delta);
    void delete_step();
    void import_notation();
    void load_preset(int index);
    void preview();
    void play_on_printer();
    void copy_gcode();
    void insert_into_printer_gcode(const char *key, const wxString &field_name);
    void save_to_file();
    void open_from_file();

    Melody                   m_melody;
    TextInput               *m_name{nullptr};
    SpinInput               *m_tick_ms{nullptr};
    ComboBox                *m_presets{nullptr};
    MD3Grid                 *m_grid{nullptr};
    TextInput               *m_notation{nullptr};
    Label                   *m_status{nullptr};
    Button                  *m_preview{nullptr};
    Button                  *m_play{nullptr};
    std::unique_ptr<wxSound> m_sound;
    std::vector<std::uint8_t> m_sound_data;
    bool                     m_refreshing{false};
};

}} // namespace Slic3r::GUI

#endif
