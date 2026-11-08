#include "FeatureCommands.hpp"

namespace Slic3r::GUI::FeatureFinder {

Commands &Commands::instance()
{
    static Commands commands;
    return commands;
}

void Commands::add(const std::string &id, std::function<void()> run)
{
    if (id.empty() || !run) return;
    m_commands[id] = std::move(run);
}

bool Commands::has(std::string_view id) const { return m_commands.find(id) != m_commands.end(); }

bool Commands::run(std::string_view id) const
{
    const auto it = m_commands.find(id);
    if (it == m_commands.end()) return false;
    // Copy first: the command may rebuild menus, which re-registers (and
    // replaces) this very entry while it runs.
    const std::function<void()> command = it->second;
    command();
    return true;
}

std::vector<std::string> Commands::ids() const
{
    std::vector<std::string> out;
    out.reserve(m_commands.size());
    for (const auto &entry : m_commands) out.push_back(entry.first);
    return out;
}

} // namespace Slic3r::GUI::FeatureFinder
