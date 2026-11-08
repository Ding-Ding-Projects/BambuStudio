#ifndef slic3r_GUI_GameServers_ServersPanel_hpp_
#define slic3r_GUI_GameServers_ServersPanel_hpp_

// File > Minecraft servers: the Java servers on this PC the person created or
// adopted, each with its overview, console, every setting in
// server.properties, players, worlds, versions from the Mojang list, backups
// checked as zip files, schedules, a world generator and history. Servers keep
// running while the page is closed and are stopped safely when the app exits.
// The rules live in ServerModel; what touches the PC in ServerSystem.

#include <wx/panel.h>

#include <memory>

namespace Slic3r { namespace GUI { namespace GameServers {

class ServersPanel : public wxPanel
{
public:
    explicit ServersPanel(wxWindow *parent);
    ~ServersPanel() override;
    static void open();

    struct Ui; // the page's controls and state, kept out of this header

private:
    std::unique_ptr<Ui> m_ui;
};

// At app exit: asks every running server to stop and waits a little for it.
void shutdown();

}}} // namespace Slic3r::GUI::GameServers

#endif // slic3r_GUI_GameServers_ServersPanel_hpp_
