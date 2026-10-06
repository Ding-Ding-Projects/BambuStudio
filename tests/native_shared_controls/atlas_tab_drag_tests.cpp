#include <algorithm>
#include <functional>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "atlas_tab_overflow.inc"
#include "atlas_tab_drag.inc"

// Only wxString title/payload storage is replaced by std::string. The add,
// index lookup, pinned count and move bodies below are extracted verbatim
// from TabStripModel.hpp, so assertions exercise the production final order.
struct Tab {
    std::string id, title, payload;
    bool pinned = true, hidden = false, dirty = false;
    int group_id = -1;
};
struct Model {
    std::vector<Tab> m_tabs;
    std::string m_active;
#include "atlas_tab_model_operations.inc"
};
struct Control { std::string id; int width, center = 0; bool shown = false; };

Model make_model(bool hidden_tail = false, bool unpinned_tail = false)
{
    Model model;
    for (const char* id : {"A", "B", "C", "D"}) model.add({id, id, id, true});
    if (hidden_tail) model.add({"E", "E", "E", true});
    if (unpinned_tail) {
        model.add({"E", "E", "E", false});
        model.add({"F", "F", "F", false});
    }
    return model;
}
std::string order(const Model& model)
{
    std::string value;
    for (const auto& tab : model.m_tabs) value += tab.id;
    return value;
}
void layout(std::vector<Control>& controls, int width)
{
    std::vector<int> extents;
    for (const auto& control : controls) extents.push_back(control.width);
    const auto projection = atlasTabOverflow(extents, std::vector<bool>(controls.size(), true), width, 36, 4);
    for (auto& control : controls) control.shown = false; // hidden rectangles intentionally remain stale
    int cursor = 0;
    for (int index : projection.visible) {
        auto& control = controls[index];
        control.shown = true;
        control.center = cursor + control.width / 2;
        cursor += control.width + 4;
    }
}
bool drop(Model& model, const std::vector<Control>& controls, const std::string& id, int coordinate)
{
    std::vector<AtlasDragTab> visible;
    for (const auto& control : controls)
        if (control.shown) visible.push_back({control.id, control.center});
    const int from = model.index_of(id);
    const int target = atlasTabDragTarget(id, from, visible, coordinate,
        [&model](const std::string& neighbor) { return model.index_of(neighbor); });
    return model.move(from, target);
}
int main()
{
    int assertions = 0, failures = 0;
    auto verify = [&](bool value) { ++assertions; if (!value) ++failures; };
    std::vector<Control> controls{{"A", 100}, {"B", 400}, {"C", 100}, {"D", 100}};
    layout(controls, 800);
    verify(controls[1].shown && controls[1].center == 304);
    layout(controls, 400);
    verify(!controls[1].shown && controls[1].center == 304);
    verify(controls[2].center == 154 && controls[3].center == 258);

    auto model = make_model();
    verify(!drop(model, controls, "D", 180)); // after C is already D's current position
    verify(order(model) == "ABCD");

    model = make_model();
    verify(drop(model, controls, "D", 100)); // before C, with hidden B remaining before that boundary
    verify(order(model) == "ABDC");

    model = make_model();
    verify(drop(model, controls, "D", 0));
    verify(order(model) == "DABC");

    model = make_model();
    verify(drop(model, controls, "A", 300)); // after the last visible neighbor
    verify(order(model) == "BCDA");

    model = make_model();
    verify(!drop(model, controls, "C", 154));
    verify(order(model) == "ABCD");

    model = make_model(true);
    verify(drop(model, controls, "A", 300)); // hidden tail must not become the insertion anchor
    verify(order(model) == "BCDAE");

    model = make_model();
    verify(!drop(model, {}, "C", 100));
    verify(order(model) == "ABCD");

    model = make_model(false, true);
    auto extended = controls;
    extended.push_back({"E", 100, 362, true});
    extended.push_back({"F", 100, 466, true});
    verify(!drop(model, extended, "E", 0)); // model move still protects the pinned region
    verify(order(model) == "ABCDEF");
    verify(drop(model, extended, "A", 600));
    verify(order(model) == "BCDAEF");
    verify(model.pinned_count() == 4);
    for (const auto& tab : model.m_tabs) verify(tab.title == tab.id && tab.payload == tab.id);

    std::cout << "8 drag-order cases; " << assertions << " assertions; " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
