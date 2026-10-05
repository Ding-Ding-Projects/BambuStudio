#include "slic3r/GUI/NotificationHistory.hpp"
#include <cassert>
#include <iostream>
using Slic3r::GUI::NotificationHistory;
int main() {
    NotificationHistory history(3);
    const auto first = history.append(3, "info", "first", 1000);
    const auto second = history.append(7, "warning", "second", 2000);
    NotificationHistory::Selection selection;
    selection.set(first, true);
    NotificationHistory::Filter filter;
    const auto reviewed = history.reviewed_selection(selection, history.filtered_ids(filter));
    selection.clear(); selection.set(second, true);
    assert(reviewed == std::set<std::uint64_t>{first});
    assert(history.erase(reviewed) == 1);
    assert(history.find(second));
    const auto stale = history.reviewed_selection(selection, history.filtered_ids(filter));
    history.append(3, "info", "third", 3000);
    history.append(3, "info", "fourth", 4000);
    history.append(3, "info", "fifth", 5000);
    assert(history.erase(stale) == 0);
    assert(history.size() == 3);
    selection.set(9999, true);
    assert(history.reviewed_selection(selection, {9999}).empty());
    filter.query = "fourth";
    const auto data = history.export_dataset(history.filtered_ids(filter), filter);
    assert(data.rows.size() == 1);
    assert(data.columns.size() == 13);
    assert(data.rows[0].size() == data.columns.size());
    assert(data.rows[0][6].string == "fourth");
    assert(data.rows[0][11].string.find("fourth") != std::string::npos);
    assert(data.rows[0][12].integer == 3);
    assert(data.rows[0][0].type == Slic3r::GUI::Export::Value::Type::String);
    std::cout << "PASS: reviewed selection, retention, missing targets, filtered complete export\n";
}
