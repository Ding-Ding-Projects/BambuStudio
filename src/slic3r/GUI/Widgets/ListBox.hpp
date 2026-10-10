#ifndef slic3r_GUI_ListBox_hpp_
#define slic3r_GUI_ListBox_hpp_

#include <vector>

#include <wx/arrstr.h>
#include <wx/vlbox.h>

#include "MD3ScrollBars.hpp"
#include "MD3Tokens.hpp"
#include "MD3Motion.hpp"

namespace Slic3r { namespace GUI {

// The kit single-selection list. It replaces the stock wxListBox with an
// owner-drawn wxVListBox that paints rows the way the kit DropDown paints its
// menu rows: SurfaceContainer field, a SurfaceContainerHigh hover pane and a
// SecondaryContainer selected pane, both rounded and inset, OnSurface text in
// the kit body face. wxVListBox keeps the native keyboard model (arrows,
// Home/End, Page keys) and emits wxEVT_LISTBOX on selection, so callers that
// used wxListBox::Set/GetSelection keep working. Long rows ellipsize at the
// end; the full text is the row's tooltip so nothing is unreachable. Its
// scrollbar is the kit scrollbar (MD3ScrollBars), not the Windows one.
// Rows use density minima and measured caller fonts. The keyboard focus ring
// sits on the current row, selected or not; hover paint uses owner-bound,
// reduced-motion-aware feedback.
// Created with wxLB_MULTIPLE it replaces a wxListBox with wxLB_EXTENDED:
// wxVListBox then gives the extended model (a click selects one row,
// Ctrl+click toggles a row, Shift+click and Shift+arrows extend the range,
// Ctrl+arrows move the current row without selecting it, Ctrl+Space toggles
// the current row and Space alone selects only the current row) and
// GetSelections() reports the rows.
// Screen readers get the list a stock wxListBox gave them through an
// accessible peer: a named list whose rows are its children, each named by its
// text with its selected, focused and checked states.
class ListBox : public wxVListBox
{
public:
    ListBox(wxWindow *parent, wxWindowID id = wxID_ANY, const wxSize &size = wxDefaultSize, long style = 0);
    ~ListBox() override;

    void     Set(const std::vector<wxString> &rows);
    void     Set(const wxArrayString &rows) { Set(std::vector<wxString>(rows.begin(), rows.end())); }
    void     Append(const wxString &row);
    void     Clear();
    unsigned GetCount() const { return unsigned(m_rows.size()); }
    wxString GetString(unsigned index) const { return index < m_rows.size() ? m_rows[index] : wxString(); }
    // The selected rows in ascending order, in either selection mode, as
    // wxListBox::GetSelections() reported them; returns their count.
    int      GetSelections(wxArrayInt &selections) const;
    // wxVListBox::SetSelection, then tells screen readers about the change.
    void     SetSelection(int selection);
    // wxVListBox::SetItemCount, then tells screen readers the rows changed.
    void     SetItemCount(size_t count) override;
    // The row the keyboard is on, or wxNOT_FOUND; in a multiple-selection
    // list it need not be selected.
    int      GetCurrentRow() const;

    // Recolor the selected pane to a workspace accent (Preview / Device).
    void SetColorScheme(MD3::ColorScheme scheme);

    // Checkable rows, in place of wxCheckListBox: a Material check box glyph
    // starts every row. A click on the glyph or the Space key toggles it and
    // sends wxEVT_CHECKLISTBOX with the row in GetInt(), as wxCheckListBox did.
    void EnableChecks(bool enable = true);
    void Check(unsigned index, bool checked = true);
    bool IsChecked(unsigned index) const { return index < m_checked.size() && m_checked[index] != 0; }
    void Rescale();
    bool SetFont(const wxFont &font) override;

    // Whether the kit bar for orient (wxVERTICAL or wxHORIZONTAL) is shown.
    bool IsBarShown(int orient) const { return m_bars.IsShown(orient); }

#ifdef __WXMSW__
    void      SetScrollbar(int orient, int pos, int thumbVisible, int range, bool refresh = true) override;
    void      SetScrollPos(int orient, int pos, bool refresh = true) override;
    int       GetScrollPos(int orient) const override;
    int       GetScrollThumb(int orient) const override;
    int       GetScrollRange(int orient) const override;
    WXDWORD   MSWGetStyle(long flags, WXDWORD *exstyle = nullptr) const override;
    WXLRESULT MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

protected:
    void    OnDrawItem(wxDC &dc, const wxRect &rect, size_t n) const override;
    void    OnDrawBackground(wxDC &dc, const wxRect &rect, size_t n) const override;
    wxCoord OnMeasureItem(size_t n) const override;

private:
    void onMotion(wxMouseEvent &evt);
    void onLeave(wxMouseEvent &evt);
    void onLeftDown(wxMouseEvent &evt);
    void onKey(wxKeyEvent &evt);
    int  checkWidth() const;
    void toggle(size_t row);
    void animateHover(int previous);
    // Raise the accessibility events for a change of the current row or the
    // selection since the last call, as the stock list box did.
    void announce();

#if wxUSE_ACCESSIBILITY
    class Accessible;
#endif

    std::vector<wxString> m_rows;
    int                   m_announced_current { wxNOT_FOUND };
    std::vector<int>      m_announced_selection;
    std::vector<char>     m_announced_checked;
    int                   m_hover { -1 };
    int                   m_previous_hover { -1 };
    double                m_hover_progress { 1.0 };
    MD3::Motion::Anim     m_hover_motion;
    std::vector<char>     m_checked;
    bool                  m_checks { false };
    bool                  m_custom_font { false };
    MD3::ColorScheme      m_scheme { MD3::ColorScheme::Brand };
    MD3ScrollBars         m_bars { this };
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_ListBox_hpp_
