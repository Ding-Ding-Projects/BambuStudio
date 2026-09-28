#pragma once

#include "ModelCreatorBackend.hpp"
#include "../Widgets/MD3Dialog.hpp"

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <wx/timer.h>

class Button;
class ComboBox;
class wxStaticText;
class wxTextCtrl;

namespace Slic3r::GUI::ModelCreator {
struct ProviderLookupState;

// Navigation creates this dialog. The plate import callback is invoked only
// after the user presses Add to plate on a successfully rendered revision.
class ModelCreatorDialog final : public MD3Dialog {
public:
    using AddToPlate = std::function<void(const std::filesystem::path &)>;
    ModelCreatorDialog(wxWindow *parent, AddToPlate add_to_plate);
    ~ModelCreatorDialog() override;

private:
    void generate();
    void cancel_generation();
    void update_controls();
    void save_key();
    void test_key();
    void clear_key();
    void select_revision();
    void update_renderer_path();
    void save_preferences();
    void lookup_provider_path();
    void collect_provider_paths();

    AddToPlate m_add_to_plate;
    std::shared_ptr<std::atomic_bool> m_cancel;
    std::shared_ptr<std::atomic_bool> m_alive;
    std::thread m_worker;
    std::thread m_history_worker;
    std::shared_ptr<ProviderLookupState> m_provider_lookup_state;
    wxTimer m_provider_lookup_timer;
    bool m_provider_lookup_started[2] = {false, false};
    bool m_provider_lookup_collected[2] = {false, false};
    std::filesystem::path m_discovered_provider_paths[2];
    bool m_provider_path_autofilled = false;
    bool m_provider_path_user_edited = false;
    std::wstring m_provider_lookup_expected_paths[2];
    std::vector<Revision> m_revisions;
    bool m_busy = false;
    bool m_loading_history = false;
    ComboBox *m_provider = nullptr;
    ComboBox *m_renderer = nullptr;
    ComboBox *m_history = nullptr;
    // These point at the native editor inside a kit TextInput/TextArea (see the
    // anonymous-namespace field() helper in the .cpp); the wrapper widget itself
    // is owned by the dialog's sizer, not tracked here.
    wxTextCtrl *m_model = nullptr;
    wxTextCtrl *m_prompt = nullptr;
    wxTextCtrl *m_note = nullptr;
    wxTextCtrl *m_provider_path = nullptr;
    wxTextCtrl *m_renderer_path = nullptr;
    wxTextCtrl *m_key = nullptr;
    wxTextCtrl *m_status = nullptr;
    wxStaticText *m_connection = nullptr;
    wxStaticText *m_renderer_status = nullptr;
    Button *m_generate = nullptr;
    Button *m_test_key = nullptr;
    Button *m_cancel_button = nullptr;
    Button *m_preview = nullptr;
    Button *m_add = nullptr;
};

} // namespace Slic3r::GUI::ModelCreator
