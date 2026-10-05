#pragma once
#include <memory>
namespace Slic3r { namespace GUI {
class GUI_App;
class AutomationBridge {
public:
    explicit AutomationBridge(GUI_App& app);
    ~AutomationBridge();
    void start();
    void stop();
private:
    struct State;
    std::shared_ptr<State> m_state;
};
}}
