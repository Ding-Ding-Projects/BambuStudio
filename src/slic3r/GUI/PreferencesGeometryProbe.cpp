#include "PreferencesGeometryProbe.hpp"

#include <nlohmann/json.hpp>
#include <wx/simplebook.h>
#include <wx/thread.h>
#include <wx/weakref.h>
#include <array>

namespace Slic3r { namespace GUI { namespace PreferencesGeometryProbe {
namespace {
using Json = nlohmann::json;
wxWeakRef<wxWindow> dialog_ref, card_ref, search_ref, strip_ref;
wxWeakRef<wxSimplebook> book_ref;

Json rect(const wxRect& r)
{
    return {{"x", r.x}, {"y", r.y}, {"width", r.width}, {"height", r.height}};
}
Json size(const wxSize& s) { return {{"width", s.x}, {"height", s.y}}; }

Json measure(const char* role, wxWindow* window)
{
    Json result = {{"role", role}, {"state", "unavailable"}};
    if (!window) return result;
    if (window->IsBeingDeleted()) { result["state"] = "destroying"; return result; }
    const auto bounds = window->GetScreenRect();
    auto* parent = window->GetParent();
    result["state"] = "available";
    result["screenRect"] = rect(bounds);
    result["clientSize"] = size(window->GetClientSize());
    result["minimumSize"] = size(window->GetMinSize());
    result["dpi"] = size(window->GetDPI());
    result["shown"] = window->IsShown();
    result["shownOnScreen"] = window->IsShownOnScreen();
    result["enabled"] = window->IsEnabled();
    result["parentClientContains"] = nullptr;
    // A top-level dialog need not fit its owner's client area. Do not report
    // that relationship as a child-layout violation.
    if (!window->IsTopLevel() && parent && !parent->IsBeingDeleted()) {
        const wxRect parent_client(parent->ClientToScreen(wxPoint(0, 0)), parent->GetClientSize());
        result["parentClientContains"] = parent_client.Contains(bounds);
    }
    return result;
}
}

void bind(wxWindow* dialog, wxWindow* card, wxSimplebook* book,
          wxWindow* search, wxWindow* strip)
{
    if (!wxIsMainThread()) return;
    dialog_ref = dialog; card_ref = card; book_ref = book;
    search_ref = search; strip_ref = strip;
}

Json snapshot()
{
    Json result = {{"schemaVersion", 1}, {"kind", "preferences-geometry"},
                   {"state", "unavailable"}, {"roles", Json::array()}};
    if (!wxIsMainThread()) { result["state"] = "wrong_thread"; return result; }
    auto* dialog = dialog_ref.get();
    if (!dialog || dialog->IsBeingDeleted() || !dialog->IsShown()) return result;
    auto* book = book_ref.get();
    // GetCurrentPage reads only the known book's selection, never child text
    // or a descendant inventory. The page itself is a container measurement.
    auto* viewport = book && !book->IsBeingDeleted() ? book->GetCurrentPage() : nullptr;
    const std::array<std::pair<const char*, wxWindow*>, 6> roles{{
        {"dialog", dialog}, {"content_card", card_ref.get()}, {"book", book},
        {"search", search_ref.get()}, {"tab_strip", strip_ref.get()}, {"viewport", viewport}
    }};
    result["state"] = "available";
    for (const auto& role : roles) result["roles"].push_back(measure(role.first, role.second));
    return result;
}
}}}
