#ifndef slic3r_GUI_FeatureFinder_FeatureCommands_hpp_
#define slic3r_GUI_FeatureFinder_FeatureCommands_hpp_

// Commands the Every feature page can run, registered by the code that owns
// them (mostly MainFrame, next to the menu item that runs the same handler).
// A "command:<id>" action whose id is not registered here is shown as not
// available instead of offering an Open button that does nothing.

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace Slic3r::GUI::FeatureFinder {

class Commands
{
public:
    static Commands &instance();

    // Registers or replaces a command. Menus can be rebuilt (language change),
    // so registering the same id again replaces the old handler.
    void add(const std::string &id, std::function<void()> run);
    bool has(std::string_view id) const;
    // Runs the command; false when it is not registered.
    bool run(std::string_view id) const;
    std::vector<std::string> ids() const;
    void clear() { m_commands.clear(); }

private:
    std::map<std::string, std::function<void()>, std::less<>> m_commands;
};

} // namespace Slic3r::GUI::FeatureFinder

#endif // slic3r_GUI_FeatureFinder_FeatureCommands_hpp_
