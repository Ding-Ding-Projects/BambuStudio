#include "LocalToolsActivity.hpp"

#include "../GUI_App.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Utils.hpp"


#include <ctime>
#include <map>
#include <memory>

namespace Slic3r::GUI::LocalTools {

namespace {

struct Store
{
    std::unique_ptr<ActivityLog>               log;
    std::map<int, std::function<void()>>       listeners;
    int                                        next_token = 1;
};

Store &store()
{
    static Store s;
    return s;
}

ActivityLog &log()
{
    Store &s = store();
    if (!s.log) {
        s.log = std::make_unique<ActivityLog>(std::filesystem::u8path(data_dir()) / "local-tools-activity.jsonl");
        s.log->load();
    }
    return *s.log;
}

void notify()
{
    // A copy: a listener may unsubscribe while it runs.
    const std::map<int, std::function<void()>> listeners = store().listeners;
    for (const auto &[token, changed] : listeners)
        if (changed) changed();
}

} // namespace

bool activity_on()
{
    AppConfig *cfg = wxGetApp().app_config;
    return cfg == nullptr || cfg->get(kActivityKey) != "0";
}

void set_activity_on(bool on)
{
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(kActivityKey, on ? "1" : "0");
    notify();
}

void record_activity(const std::string &tool, const std::string &action, const std::string &subject, const std::string &result)
{
    if (!activity_on()) return;
    Activity a;
    a.time    = std::int64_t(std::time(nullptr));
    a.tool    = tool;
    a.action  = action;
    a.subject = subject;
    a.result  = result;
    if (log().append(std::move(a))) notify();
}

std::vector<Activity> newest_activity(std::size_t limit, const std::string &tool)
{
    return log().newest(limit, tool);
}

void clear_activity()
{
    log().clear();
    notify();
}

int subscribe_activity(std::function<void()> changed)
{
    Store &s = store();
    const int token = s.next_token++;
    s.listeners.emplace(token, std::move(changed));
    return token;
}

void unsubscribe_activity(int token) { store().listeners.erase(token); }

} // namespace Slic3r::GUI::LocalTools
