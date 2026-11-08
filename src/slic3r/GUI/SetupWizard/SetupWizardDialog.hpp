#ifndef slic3r_GUI_SetupWizard_SetupWizardDialog_hpp_
#define slic3r_GUI_SetupWizard_SetupWizardDialog_hpp_

// Help > Set up Bambu Studio: a practical setup wizard whose steps do real
// things (language and humour, look and motion, printers, inks and filaments,
// backups) and save as they go, so setup can be closed and continued. The
// rules live in the wx-free SetupModel.

#include "SetupModel.hpp"

#include "../GUI_Utils.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class Button;
class Label;
class MultiSwitchButton;
class ProgressBar;
class StaticBox;
class SwitchButton;
class wxBoxSizer;
class wxSimplebook;

namespace Slic3r { namespace GUI {

class SetupWizardDialog final : public DPIDialog
{
public:
    SetupWizardDialog(wxWindow *parent, bool first_run);
    ~SetupWizardDialog() override;

    // Shows the wizard modally, then carries out a language change it asked
    // for (the main window is rebuilt and the wizard opens again).
    static void open(wxWindow *parent, bool first_run = false);
    // Called at the end of GUI_App::recreate_GUI.
    static void after_gui_rebuild();
    // Called after the first-run printer guide on a new installation.
    static void open_for_first_run();

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    struct RailItem
    {
        wxWindow *icon  = nullptr;
        Label    *title = nullptr;
        Label    *state = nullptr;
        std::uint32_t glyph = 0;
    };

    void      build();
    wxWindow *build_language(wxWindow *parent);
    wxWindow *build_look(wxWindow *parent);
    wxWindow *build_printers(wxWindow *parent);
    wxWindow *build_inks(wxWindow *parent);
    wxWindow *build_backups(wxWindow *parent);
    wxWindow *build_extensions(wxWindow *parent);
    wxWindow *build_finish(wxWindow *parent);
    wxBoxSizer *results_box(wxWindow *page, int step);

    void show_step(int index);
    void refresh_rail();
    void refresh_header();
    void refresh_finish();
    // The Extensions step: the built-ins, each turned on only through its consent prompt.
    void refresh_extensions(wxWindow *page);
    void toggle_extension(wxWindow *page, const std::string &id);
    void refresh_printers();
    void refresh_inks();
    // Adds a line under Result of each action; an optional button acts on it.
    void add_result(int step, std::uint32_t glyph, bool ok, const wxString &title, const wxString &detail,
                    const wxString &action = wxString(), std::function<void()> on_action = {});
    void import_printer_profile();
    // Saves, closes setup and shows the Device tab for a printer found on the network.
    void connect_on_device_tab(const std::string &dev_id);
    void on_next();
    void on_back();
    void on_skip();
    void save();
    void save_and_close();
    void start_over();
    Setup::Facts facts() const;

    // Language changes rebuild the main window, so they run after the wizard closes.
    void request_language(const std::string &mode);

    Setup::Progress              m_progress;
    int                          m_index      = 0;
    bool                         m_next_anyway = false;
    std::string                  m_pending_language;
    bool                         m_first_run  = false;

    wxSimplebook                *m_book          = nullptr;
    std::vector<RailItem>        m_rail;
    wxWindow                    *m_banner        = nullptr;
    Label                       *m_banner_text   = nullptr;
    Label                       *m_step_of       = nullptr;
    Label                       *m_required_left = nullptr;
    ProgressBar                 *m_progress_bar  = nullptr;
    Label                       *m_title         = nullptr;
    Label                       *m_lead          = nullptr;
    StaticBox                   *m_missing_card  = nullptr;
    Label                       *m_missing_text  = nullptr;
    Label                       *m_why_text      = nullptr;
    StaticBox                   *m_rule_card     = nullptr;
    Label                       *m_rule_text     = nullptr;
    Label                       *m_saved         = nullptr;
    Button                      *m_back          = nullptr;
    Button                      *m_next          = nullptr;
    Button                      *m_skip          = nullptr;
    std::array<wxBoxSizer *, Setup::kStepCount> m_results {};
    std::array<wxWindow *, Setup::kStepCount>   m_pages {};

    // Step widgets that refresh from the live app.
    MultiSwitchButton *m_language   = nullptr;
    Label             *m_printers_list = nullptr;
    Label             *m_inks_list     = nullptr;
    Label             *m_finish_summary = nullptr;
    wxBoxSizer        *m_extension_list = nullptr;
    Label             *m_finish_title   = nullptr;
    MultiSwitchButton *m_backup_interval = nullptr;
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_SetupWizard_SetupWizardDialog_hpp_
