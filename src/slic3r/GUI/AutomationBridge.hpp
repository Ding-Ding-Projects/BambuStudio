#pragma once
#include <memory>
namespace Slic3r { namespace GUI {
class GUI_App;
class AutomationBridge {
public:
    explicit AutomationBridge(GUI_App& app);
    ~AutomationBridge();
    void start();
    // Native consent UI only. Starts a restricted pipe without enabling generic automation.
    void start_local_capabilities();
    void stop();
private:
    struct State;
    std::shared_ptr<State> m_state;
};
}}
