#pragma once

#include "ModelCreatorBackend.hpp"
#include "../Widgets/MD3Dialog.hpp"

#include <functional>
#include <memory>
#include <thread>

class wxButton;
class wxChoice;
class wxStaticText;
class wxTextCtrl;

namespace Slic3r::GUI::ModelCreator {

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

    AddToPlate m_add_to_plate;
    std::shared_ptr<std::atomic_bool> m_cancel;
    std::shared_ptr<std::atomic_bool> m_alive;
    std::thread m_worker;
    std::thread m_history_worker;
    std::vector<Revision> m_revisions;
    bool m_busy = false;
    bool m_loading_history = false;
    wxChoice *m_provider = nullptr;
    wxChoice *m_renderer = nullptr;
    wxChoice *m_history = nullptr;
    wxTextCtrl *m_model = nullptr;
    wxTextCtrl *m_prompt = nullptr;
    wxTextCtrl *m_note = nullptr;
    wxTextCtrl *m_provider_path = nullptr;
    wxTextCtrl *m_renderer_path = nullptr;
    wxTextCtrl *m_key = nullptr;
    wxTextCtrl *m_status = nullptr;
    wxStaticText *m_connection = nullptr;
    wxStaticText *m_renderer_status = nullptr;
    wxButton *m_generate = nullptr;
    wxButton *m_test_key = nullptr;
    wxButton *m_cancel_button = nullptr;
    wxButton *m_preview = nullptr;
    wxButton *m_add = nullptr;
};

} // namespace Slic3r::GUI::ModelCreator
