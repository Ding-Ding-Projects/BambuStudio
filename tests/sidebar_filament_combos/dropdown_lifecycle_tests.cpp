#include <catch_main.hpp>
#include <functional>
#include <cassert>
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// Compile the production method bodies unchanged. These deterministic doubles
// exercise callback order and destroyed owners, not platform popup behavior.
struct Lifetime { std::shared_ptr<bool> alive = std::make_shared<bool>(true); ~Lifetime() { *alive = false; } };
template<class T> class wxWeakRef {
    T *pointer = nullptr; std::weak_ptr<bool> alive;
public:
    wxWeakRef() = default;
    wxWeakRef(T *p) : pointer(p) { if (p) alive = p->alive; }
    explicit operator bool() const { auto a = alive.lock(); return a && *a; }
    T *operator->() const { return pointer; }
};
using wxPoint = int;
constexpr int wxEVT_COMBOBOX = 1;
constexpr int DD_ITEM_STYLE_DISABLED = 2;
struct wxCommandEvent {
    int index = -1, id = 0; long revision = 0; void *data = nullptr, *object = nullptr; std::string text;
    wxCommandEvent(int, int i) : id(i) {}
    void SetEventObject(void *v) { object = v; }
    void SetInt(int v) { index = v; } int GetInt() const { return index; }
    void SetId(int v) { id = v; }
    void SetString(std::string v) { text = std::move(v); } const std::string &GetString() const { return text; }
    void SetClientData(void *v) { data = v; } void *GetClientData() const { return data; }
    void SetExtraLong(long v) { revision = v; } long GetExtraLong() const { return revision; }
};
struct Handler { std::function<void(wxCommandEvent &)> callback; void ProcessEvent(wxCommandEvent &e) { auto cb = callback; if (cb) cb(e); } };
struct Item { std::string text; void *data = nullptr; int style = 0; };
struct DropDown : Lifetime {
    std::vector<Item> &items; DropDown *mainDropDown = nullptr, *subDropDown = nullptr;
    int hover_item = 0, selection = -1; wxPoint offset = 0;
    unsigned long item_revision = 1; bool shown = true, need_sync = false;
    Handler handler; std::function<void()> on_close;
    explicit DropDown(std::vector<Item> &i) : items(i) {}
    int hoverIndex() const { return hover_item; } int GetId() const { return 7; }
    bool IsShown() const { return shown; } Handler *GetEventHandler() { return &handler; }
    void DismissAndNotify() { if (!shown) return; shown = false; hover_item = -1; auto cb = on_close; if (cb) cb(); }
    void sendDropDownEvent();
    void Invalidate(bool clear = false, bool items_changed = false);
};
struct ComboBox : Lifetime {
    std::vector<Item> &items; DropDown &drop; Handler output; int selection = -1;
    ComboBox(std::vector<Item> &i, DropDown &d) : items(i), drop(d) {}
    void SetSelection(int i) { selection = i; } int GetId() const { return 42; }
    Handler *GetEventHandler() { return &output; }
    void dispatch(wxCommandEvent &e);
};
#include "dropdown_dispatch.inc"
#include "dropdown_invalidate.inc"
void ComboBox::dispatch(wxCommandEvent &e) {
#include "combo_dispatch.inc"
}

constexpr int wxID_NONE = -1;
struct wxWindow : Lifetime {
    std::function<void()> on_focus;
    bool IsShown() const { return true; } bool IsEnabled() const { return true; }
    void SetFocus() { auto cb = on_focus; if (cb) cb(); }
};
struct wxMenu : Lifetime {
    std::function<void()> on_send;
    void SendEvent(int, int) { auto cb = on_send; if (cb) cb(); }
};
struct PopupWindow : Lifetime {
    std::function<void()> on_dismiss;
    void OnDismiss() { auto cb = on_dismiss; if (cb) cb(); }
};
struct App { void set_side_menu_popup_status(bool) {} };
App &wxGetApp() { static App app; return app; }
struct MD3MenuPopup : PopupWindow {
    bool m_restoring_focus = false, m_finalized = false, m_send_events = true;
    bool m_result_kind_checkable = false, m_result_checked = false;
    int m_result = 12;
    wxWeakRef<wxWindow> m_invoker;
    wxWeakRef<wxMenu> m_result_menu;
    std::function<void()> m_close_cb;
    void restoreInvokerFocus(); void finalizeClose();
};
#include "menu_restoreInvokerFocus.inc"
#include "menu_finalizeClose.inc"

TEST_CASE("menu focus destroying popup preserves close notification", "[native-lifecycle]") {
    wxWindow owner; wxMenu menu; auto *popup = new MD3MenuPopup;
    int closes = 0, sends = 0;
    popup->m_invoker = &owner; popup->m_result_menu = &menu;
    popup->m_close_cb = [&] { ++closes; };
    owner.on_focus = [popup] { delete popup; };
    menu.on_send = [&] { ++sends; };
    popup->finalizeClose();
    REQUIRE(closes == 1); REQUIRE(sends == 1);
}
TEST_CASE("platform dismissal destroying popup preserves close notification", "[native-lifecycle]") {
    wxMenu menu; auto *popup = new MD3MenuPopup;
    int closes = 0, sends = 0;
    popup->m_result_menu = &menu; popup->m_close_cb = [&] { ++closes; };
    popup->on_dismiss = [popup] { delete popup; };
    menu.on_send = [&] { ++sends; };
    popup->finalizeClose();
    REQUIRE(closes == 1); REQUIRE(sends == 1);
}
TEST_CASE("menu destroyed during close suppresses command delivery", "[native-lifecycle]") {
    auto *menu = new wxMenu; MD3MenuPopup popup; int sends = 0;
    popup.m_result_menu = menu; menu->on_send = [&] { ++sends; };
    popup.m_close_cb = [menu] { delete menu; };
    popup.finalizeClose(); REQUIRE(sends == 0);
}
TEST_CASE("blocking menu closes without delivering inside popup stack", "[native-lifecycle]") {
    wxMenu menu; MD3MenuPopup popup; int closes = 0, sends = 0;
    popup.m_send_events = false; popup.m_result_menu = &menu;
    popup.m_close_cb = [&] { ++closes; }; menu.on_send = [&] { ++sends; };
    popup.finalizeClose(); popup.finalizeClose();
    REQUIRE(closes == 1); REQUIRE(sends == 0);
}

TEST_CASE("dropdown closes before selection and preserves row identity", "[native-lifecycle]") {
    std::vector<Item> rows{{"P1S"}, {"H2C", reinterpret_cast<void *>(3)}};
    DropDown root(rows); ComboBox combo(rows, root); root.hover_item = 1;
    std::vector<std::string> order;
    root.on_close = [&] { order.push_back("close"); };
    root.handler.callback = [&](auto &e) { combo.dispatch(e); };
    combo.output.callback = [&](auto &e) { REQUIRE_FALSE(root.IsShown()); REQUIRE(e.object == &combo); REQUIRE(e.id == 42); order.push_back("select"); };
    root.sendDropDownEvent();
    REQUIRE((order == std::vector<std::string>{"close", "select"}));
    REQUIRE(combo.selection == 1);
}
TEST_CASE("submenu closes child then root before dispatch", "[native-lifecycle]") {
    std::vector<Item> rows{{"H2C"}}; DropDown root(rows), child(rows); ComboBox combo(rows, root);
    child.mainDropDown = &root; root.subDropDown = &child;
    std::vector<std::string> order;
    child.on_close = [&] { order.push_back("child"); }; root.on_close = [&] { order.push_back("root"); };
    root.handler.callback = [&](auto &e) { combo.dispatch(e); };
    combo.output.callback = [&](auto &) { order.push_back("select"); };
    child.sendDropDownEvent();
    REQUIRE((order == std::vector<std::string>{"child", "root", "select"}));
}
TEST_CASE("closeup row rebuild rejects even identical row text and data", "[native-lifecycle]") {
    std::vector<Item> rows{{"P1S"}}; DropDown root(rows); ComboBox combo(rows, root); int calls = 0;
    root.on_close = [&] { root.Invalidate(false, true); };
    root.handler.callback = [&](auto &e) { combo.dispatch(e); };
    combo.output.callback = [&](auto &) { ++calls; };
    root.sendDropDownEvent(); REQUIRE(calls == 0); REQUIRE(combo.selection == -1);
}
TEST_CASE("layout-only closeup invalidation keeps a valid selection", "[native-lifecycle]") {
    std::vector<Item> rows{{"P1S"}}; DropDown root(rows); ComboBox combo(rows, root); int calls = 0;
    root.on_close = [&] { root.Invalidate(); };
    root.handler.callback = [&](auto &e) { combo.dispatch(e); };
    combo.output.callback = [&](auto &) { ++calls; };
    root.sendDropDownEvent(); REQUIRE(calls == 1); REQUIRE(combo.selection == 0);
}
TEST_CASE("root destroyed by closeup suppresses selection", "[native-lifecycle]") {
    std::vector<Item> rows{{"P1S"}}; auto *root = new DropDown(rows); int calls = 0;
    root->handler.callback = [&](auto &) { ++calls; }; root->on_close = [root] { delete root; };
    root->sendDropDownEvent(); REQUIRE(calls == 0);
}
TEST_CASE("child close destroying root is safe", "[native-lifecycle]") {
    std::vector<Item> rows{{"P1S"}}; auto *root = new DropDown(rows); DropDown child(rows); int calls = 0;
    root->subDropDown = &child; child.mainDropDown = root;
    root->handler.callback = [&](auto &) { ++calls; }; child.on_close = [root] { delete root; };
    child.sendDropDownEvent(); REQUIRE(calls == 0);
}
TEST_CASE("selection can destroy its popup after dismissal", "[native-lifecycle]") {
    std::vector<Item> rows{{"P1S"}}; auto *root = new DropDown(rows); int calls = 0;
    root->handler.callback = [&, root](auto &) { REQUIRE_FALSE(root->IsShown()); ++calls; delete root; };
    root->sendDropDownEvent(); REQUIRE(calls == 1);
}
TEST_CASE("disabled and invalid rows dispatch nothing", "[native-lifecycle]") {
    std::vector<Item> rows{{"P1S", nullptr, DD_ITEM_STYLE_DISABLED}}; DropDown root(rows); int calls = 0;
    root.handler.callback = [&](auto &) { ++calls; }; root.sendDropDownEvent();
    root.hover_item = -1; root.sendDropDownEvent();
    root.hover_item = 9; root.sendDropDownEvent(); REQUIRE(calls == 0); REQUIRE(root.IsShown());
}
