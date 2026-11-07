#pragma once
#include <memory>
#include <stdexcept>
#include <vector>

inline bool main_thread = true;
inline bool wxIsMainThread() { return main_thread; }
struct wxPoint { int x, y; wxPoint(int a, int b): x(a), y(b) {} };
struct wxSize { int x, y; };
struct wxRect {
    int x, y, width, height;
    wxRect(int a, int b, int c, int d): x(a), y(b), width(c), height(d) {}
    wxRect(wxPoint p, wxSize s): wxRect(p.x, p.y, s.x, s.y) {}
    bool Contains(const wxRect& r) const {
        return r.x >= x && r.y >= y && r.x+r.width <= x+width && r.y+r.height <= y+height;
    }
};
class wxWindow {
public:
    std::shared_ptr<int> life = std::make_shared<int>(1);
    wxWindow* parent = nullptr;
    wxRect bounds{0,0,100,100};
    wxSize client{100,100}, minimum{1,1}, dpi{96,96};
    bool shown=true, enabled=true, deleting=false, top=false;
    std::vector<wxWindow*> children;
    virtual ~wxWindow() = default;
    bool IsBeingDeleted() const { return deleting; }
    wxRect GetScreenRect() const { return bounds; }
    wxWindow* GetParent() const { return parent; }
    wxSize GetClientSize() const { return client; }
    wxSize GetMinSize() const { return minimum; }
    wxSize GetDPI() const { return dpi; }
    bool IsShown() const { return shown; }
    bool IsShownOnScreen() const { return shown && (!parent || parent->IsShownOnScreen()); }
    bool IsEnabled() const { return enabled; }
    bool IsTopLevel() const { return top; }
    wxPoint ClientToScreen(wxPoint p) const { return {bounds.x+p.x,bounds.y+p.y}; }
    // These canaries are deliberately available to production code. Any use
    // fails at runtime, including a traversal that merely asks for children.
    const char* GetLabel() const { throw std::runtime_error("label canary accessed"); }
    const char* GetName() const { throw std::runtime_error("name canary accessed"); }
    const char* GetValue() const { throw std::runtime_error("value canary accessed"); }
    const std::vector<wxWindow*>& GetChildren() const { throw std::runtime_error("children canary accessed"); }
};
class wxSimplebook : public wxWindow {
public:
    wxWindow* current = nullptr;
    wxWindow* GetCurrentPage() const { return current; }
};
template<class T> class wxWeakRef {
    T* pointer=nullptr;
    std::weak_ptr<int> life;
public:
    wxWeakRef& operator=(T* p) { pointer=p; life=p?p->life:std::weak_ptr<int>{}; return *this; }
    T* get() const { return life.expired()?nullptr:pointer; }
};
