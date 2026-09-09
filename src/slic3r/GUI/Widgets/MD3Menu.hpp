#ifndef slic3r_GUI_MD3Menu_hpp_
#define slic3r_GUI_MD3Menu_hpp_

// Material Design 3 replacement for native wxMenu popups.
//
// A wxMenu stays the data model (ids, labels, accelerators, check state,
// wxEVT_UPDATE_UI enable rules, submenus). MD3MenuPopup renders it as an
// owner-drawn floating surface: SurfaceContainer fill, 1px OutlineVariant
// frame, radius 12, 24dp leading icon/check slot, body-s labels, caption
// shortcuts, hover state layer, keyboard selection, a search field once the
// menu is long enough, nested submenus, and a wxWindowAccessible tree so a
// screen reader sees a menu popup with menu items.
//
// The row model, filtering and placement arithmetic live in MD3MenuModel.hpp
// so they stay testable without a running application.

#include <functional>
#include <memory>
#include <vector>

#include <wx/menu.h>
#include <wx/timer.h>
#include <wx/weakref.h>

#include "MD3MenuModel.hpp"
#include "PopupWindow.hpp"

class SearchField;

namespace Slic3r { namespace GUI {

class MD3MenuList;

class MD3MenuPopup : public PopupWindow
{
public:
    // Number of actionable rows at which the search field appears.
    static constexpr int kSearchThreshold = 6;
    // Frame-only elevation: the drop shadow is not painted (transient popup
    // HWNDs have square corners, so a painted shadow would show its box).
    static constexpr bool kDrawShadow = false;
    // Hover-to-open delay for submenus (ms). Immediate under reduced motion.
    static constexpr int kSubmenuHoverDelayMs = 200;

    // owner: the window the menu belongs to (focus is restored to whatever had
    // it when the menu opened; owner is used for DPI and display lookup).
    // parent_popup: non-null for a submenu surface.
    MD3MenuPopup(wxWindow *owner, wxMenu *menu, MD3MenuPopup *parent_popup = nullptr);
    ~MD3MenuPopup() override;

    // Open as a root menu next to an anchor rectangle in screen coordinates
    // (below it when there is room, otherwise above / right / left).
    void PopupAt(const wxRect &anchor_screen);
    // Open as a submenu to the right of a parent row (screen coordinates).
    void PopupBesideRow(const wxRect &row_screen);

    // Id of the activated item, wxID_NONE until something was activated.
    int  Result() const { return m_result; }
    // Check state after activation, for Check / Radio items (false otherwise).
    bool ResultChecked() const { return m_result_checked; }

    // Called exactly once when the root menu has fully closed, after any
    // wxEVT_MENU event was delivered.
    void SetCloseCallback(std::function<void()> cb) { m_close_cb = std::move(cb); }
    // When true (default) activation sends wxEVT_MENU through the owning
    // wxMenu; PopupMenuSelection turns this off.
    void SetSendEvents(bool send) { m_send_events = send; }

    void Popup(wxWindow *focus = nullptr) override;
    void Dismiss() override;
    void OnDismiss() override;
    bool ProcessLeftDown(wxMouseEvent &event) override;

    // --- used by MD3MenuList -------------------------------------------------
    wxMenu *      Menu() const { return m_menu; }
    MD3MenuPopup *ParentPopup() const { return m_parent; }
    MD3MenuPopup *RootPopup();
    bool          IsSubmenuShown() const;
    // The wxMenu shown by the open submenu surface, or null.
    wxMenu *      OpenSubmenuMenu() const { return m_child ? m_child->m_menu : nullptr; }
    // Activate a row (Normal/Check/Radio) and close the whole menu chain.
    void ActivateItem(const MD3::Menu::Item &item);
    // Open the submenu carried by a row; focus_first moves keyboard selection
    // into it (keyboard Right / Enter), hover-open leaves the parent selected.
    void OpenSubmenu(const MD3::Menu::Item &item, const wxRect &row_screen, bool focus_first);
    void CloseSubmenu();
    // Close this submenu and give keyboard focus back to the parent surface.
    void ReturnToParent();
    // Escape on a root closes everything; on a submenu it returns to the parent.
    void RequestEscape();
    // Re-run filtering after the search query changed.
    void ApplyFilter();
    // Focus the list (used when a child submenu hands control back).
    void FocusList();

private:
    // Outside: focus/activation lost to something else. ReturnToParent: Left
    // or Escape inside a submenu. Chain: closed by the parent or by an
    // activation walking the chain. Escape: Escape on the root.
    enum class DismissReason { Outside, ReturnToParent, Chain, Escape };

    void build();
    void layout(const wxRect &target);
    void paintEvent(wxPaintEvent &evt);
    void onCharHook(wxKeyEvent &evt);
    void onChildDismissed(MD3MenuPopup *child, DismissReason reason);
    void finalizeClose();
    void restoreInvokerFocus();
    wxRect displayArea() const;
    wxSize wantedSize() const;

    wxWindow *    m_owner { nullptr };
    wxMenu *      m_menu { nullptr };
    MD3MenuPopup *m_parent { nullptr };
    MD3MenuPopup *m_child { nullptr };

    SearchField *m_search { nullptr };
    MD3MenuList *m_list { nullptr };

    std::vector<MD3::Menu::Item> m_rows;
    MD3::Menu::SecondaryLookup   m_secondary;

    wxWeakRef<wxWindow> m_invoker;
    bool                m_restoring_focus { false };

    int     m_result { wxID_NONE };
    bool    m_result_checked { false };
    // Anchor the surface was opened against, so a filter that changes the row
    // count can re-place and re-size the card instead of leaving dead space.
    wxRect  m_anchor_rect;
    bool    m_anchor_is_row { false };
    bool    m_result_kind_checkable { false }; // Check / Radio item activated
    wxMenu *m_result_menu { nullptr };         // wxMenu owning the activated item
    bool m_send_events { true };
    bool m_closed { false };
    bool m_finalized { false };
    DismissReason m_reason { DismissReason::Outside };

    std::function<void()> m_close_cb;

    int m_radius { 12 };
};

}} // namespace Slic3r::GUI

// Drop-in replacements for wxWindow::PopupMenu / GetPopupMenuSelectionFromUser.
// These live in the global MD3 namespace beside MD3::Role and MD3::Menu.
namespace MD3 {

// Blocking: shows the menu, runs a nested event loop until it closes, and
// delivers the chosen item's wxEVT_MENU through `menu` (invoking window set to
// `owner`) before returning. screen_pos defaults to the mouse position.
// Returns false only when owner or menu is null.
bool PopupMenu(wxWindow *owner, wxMenu *menu, wxPoint screen_pos = wxDefaultPosition);

// Same surface; returns the chosen id (wxID_NONE when dismissed) and never
// sends wxEVT_MENU.
int PopupMenuSelection(wxWindow *owner, wxMenu &menu, wxPoint screen_pos = wxDefaultPosition);

// Blocking, anchored to the full screen rectangle of `anchor` (opens below it).
bool PopupMenuBelow(wxWindow *anchor, wxMenu *menu);

} // namespace MD3

#endif // slic3r_GUI_MD3Menu_hpp_
