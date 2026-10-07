#ifndef slic3r_GUI_PreferencesSearchTraversal_hpp_
#define slic3r_GUI_PreferencesSearchTraversal_hpp_

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace Slic3r { namespace GUI { namespace PreferencesSearch {

struct Identity {
    const void *sizer = nullptr;
    const void *window = nullptr;
};

struct Registration {
    Identity identity;
    std::string key;
};

inline std::vector<std::string> matching_keys(Identity row, const std::vector<Registration> &registrations)
{
    std::vector<std::string> keys;
    for (const auto &entry : registrations)
        if ((row.sizer && row.sizer == entry.identity.sizer) ||
            (row.window && row.window == entry.identity.window))
            keys.push_back(entry.key);
    return keys;
}

template<class Item> struct Row {
    Item item;
    bool baseline_shown;
    std::vector<std::string> keys;
};

// Registered items are atomic row boundaries. Only a container proven to own
// such a boundary is traversed; ordinary compound rows remain intact. Containers
// are never emitted alongside their children, so filtering cannot hide a card
// that owns a matching row. Hidden ancestors still constrain baseline visibility.
template<class Item, class IdentityOf, class ChildrenOf, class IsShown>
std::vector<Row<Item>> collect_rows(const std::vector<Item> &roots,
                                   const std::vector<Registration> &registrations,
                                   IdentityOf identity_of, ChildrenOf children_of, IsShown is_shown)
{
    std::function<bool(Item)> contains_registered = [&](Item item) {
        if (!matching_keys(identity_of(item), registrations).empty()) return true;
        for (Item child : children_of(item))
            if (contains_registered(child)) return true;
        return false;
    };
    std::vector<Row<Item>> rows;
    std::function<void(Item, bool)> visit = [&](Item item, bool ancestors_shown) {
        auto keys = matching_keys(identity_of(item), registrations);
        const bool shown = ancestors_shown && is_shown(item);
        if (keys.empty()) {
            const auto children = children_of(item);
            bool structural = false;
            for (Item child : children)
                if (contains_registered(child)) { structural = true; break; }
            if (structural) {
                for (Item child : children) visit(child, shown);
                return;
            }
        }
        rows.push_back({item, shown, std::move(keys)});
    };
    for (Item root : roots) visit(root, true);
    return rows;
}

}}} // namespace Slic3r::GUI::PreferencesSearch
#endif
