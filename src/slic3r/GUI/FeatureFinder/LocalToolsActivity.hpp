#ifndef slic3r_GUI_FeatureFinder_LocalToolsActivity_hpp_
#define slic3r_GUI_FeatureFinder_LocalToolsActivity_hpp_

// What the local tools did, for Recent activity on the Local tools page. A
// tool calls record_activity() on the main thread when it finishes something
// the person asked for: names only (a file, a disk, a server, an extension),
// never what a file holds. Kept in local-tools-activity.jsonl in the data
// folder, at most 200 entries, and never sent anywhere. With Keep recent
// activity off nothing is written. The rules live in the wx-free LocalToolsHub.

#include <functional>
#include <string>
#include <vector>

#include "LocalToolsHub.hpp"

namespace Slic3r::GUI::LocalTools {

constexpr const char *kActivityKey = "local_tools_activity"; // "0" turns recording off

// `action` is untranslated English the page translates ("Encoded", "Start");
// `result` is one of Done, Failed, Cancelled, Incomplete, Started.
void record_activity(const std::string &tool, const std::string &action, const std::string &subject,
                     const std::string &result = "Done");

bool                  activity_on();
void                  set_activity_on(bool on);
std::vector<Activity> newest_activity(std::size_t limit, const std::string &tool = std::string());
void                  clear_activity();

// The page refreshes when something is recorded or cleared.
int  subscribe_activity(std::function<void()> changed);
void unsubscribe_activity(int token);

} // namespace Slic3r::GUI::LocalTools

#endif // slic3r_GUI_FeatureFinder_LocalToolsActivity_hpp_
