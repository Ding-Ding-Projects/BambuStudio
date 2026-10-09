#ifndef slic3r_GUI_CollapsibleFilterBar_hpp_
#define slic3r_GUI_CollapsibleFilterBar_hpp_

#include <functional>
#include <string>
#include <vector>

#include <wx/colour.h>
#include <wx/string.h>

#include "Button.hpp"
#include "CollapsibleFilterState.hpp"

class Label;
class wxPanel;
class wxSizer;

// Shared collapsible container for search bars, filter rows and statistics
// panels.
//
// The bar itself is the disclosure header: a Material text button with an
// expand_more / expand_less chevron and the section title. It owns two
// siblings created on the same parent:
//
//   * a body panel that holds the collapsible controls. Hosts create their
//     search field, chips or statistics with GetBody() as the parent and add
//     them to GetBodySizer(), so collapsing never touches the shown state the
//     host gives its own controls;
//   * a disclosure line, shown only while the bar is collapsed and something
//     is still narrowing the collection ("Active filters (2): Errors ·
//     Search: abc"). Clicking it expands the bar.
//
// GetSectionSizer() stacks header, disclosure line and body (Layout::Stacked)
// or lines them up in one row for toolbars (Layout::Inline); the host adds it
// to its own layout once (the host's sizer then owns it).
//
// Behaviour (rules in CollapsibleFilterState.hpp):
//   * Purpose::Narrows (search bars, filter rows) starts expanded,
//     Purpose::Describes (statistics) starts collapsed;
//   * the user's choice is stored per surface id in the application config
//     section "collapsible_filters" and restored on the next start;
//   * Space or Enter toggles it from the keyboard and the button draws the
//     shared focus ring; the accessible object reports the disclosure as
//     expanded or collapsed, names the active-filter count and fires state,
//     name and description change events when they move;
//   * collapsing while focus is inside the body moves focus to the header so
//     it is never left on a hidden control.
class CollapsibleFilterBar : public Button
{
public:
    using Purpose = Slic3r::GUI::CollapsibleFilters::Purpose;
    enum class Layout { Stacked, Inline };

    CollapsibleFilterBar(wxWindow *parent, const std::string &surface_id, const wxString &title,
                         Purpose purpose = Purpose::Narrows, Layout layout = Layout::Stacked);

    // Parent for the collapsible controls, and the sizer they go into.
    wxPanel *GetBody() const { return m_body; }
    wxSizer *GetBodySizer() const;

    // Header + disclosure line + body. Add it to the host layout once.
    wxSizer *GetSectionSizer() const { return m_section; }

    // Short, already translated labels for every filter that currently
    // excludes items ("Errors", "Search: abc"). Statistics bars leave it empty.
    void SetActiveFilters(const std::vector<wxString> &labels);
    const std::vector<wxString> &GetActiveFilters() const { return m_active; }

    bool IsExpanded() const { return m_state.expanded(); }
    // A user choice: laid out, announced and, unless `remember` is false,
    // stored. Pass remember=false to reveal the controls for this session
    // only, for example when a shortcut asks to focus the search field.
    void SetExpanded(bool expanded, bool remember = true);
    void Toggle() { SetExpanded(!IsExpanded()); }

    // Show or hide the whole section (header, disclosure line and body) when
    // the host hides the collection it belongs to. The expanded state is kept.
    void ShowSection(bool show);
    bool IsSectionShown() const { return m_section_shown; }

    // Labels for SetActiveFilters(). FilterLabel gives "Name: value" with the
    // value shortened; SearchFilterLabel is empty for a blank query;
    // ExcludedFilterLabel names a category that is switched off.
    static wxString FilterLabel(const wxString &name, const wxString &value);
    static wxString SearchFilterLabel(const wxString &query);
    static wxString ExcludedFilterLabel(const wxString &what);

    // Called after the expanded state changed.
    void SetOnToggled(std::function<void(bool expanded)> cb) { m_on_toggled = std::move(cb); }

    // Follow the host background after a theme switch. The bar also does this
    // on its own when it repaints; hosts with an explicit theme pass may call it.
    void SyncTheme();
    void Rescale();

    // Texts the accessible object reports (also used by tests and tooltips).
    wxString AccessibleName() const;
    wxString AccessibleDescription() const { return m_disclosure_text; }
    wxString GetTitle() const { return m_title; }

private:
    void applyVisibility();
    void applyState(bool user_change);
    void updateDisclosure(bool notify);
    void relayout();

    wxString                                     m_title;
    Slic3r::GUI::CollapsibleFilters::Section     m_state;
    std::vector<wxString>                        m_active;
    wxString                                     m_disclosure_text;
    wxColour                                     m_synced_background;
    Label *                                      m_summary = nullptr;
    wxPanel *                                    m_body    = nullptr;
    wxSizer *                                    m_section = nullptr;
    bool                                         m_section_shown = true;
    std::function<void(bool)>                    m_on_toggled;
};

#endif // !slic3r_GUI_CollapsibleFilterBar_hpp_
