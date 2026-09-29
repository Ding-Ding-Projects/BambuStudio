#ifndef slic3r_GUI_ListBox_hpp_
#define slic3r_GUI_ListBox_hpp_

#include <vector>

#include <wx/arrstr.h>
#include <wx/vlbox.h>

#include "MD3Tokens.hpp"

namespace Slic3r { namespace GUI {

// The kit single-selection list. It replaces the stock wxListBox with an
// owner-drawn wxVListBox that paints rows the way the kit DropDown paints its
// menu rows: SurfaceContainer field, a SurfaceContainerHigh hover pane and a
// SecondaryContainer selected pane, both rounded and inset, OnSurface text in
// the kit body face. wxVListBox keeps the native keyboard model (arrows,
// Home/End, Page keys) and emits wxEVT_LISTBOX on selection, so callers that
// used wxListBox::Set/GetSelection keep working. Long rows ellipsize at the
// end; the full text is the row's tooltip so nothing is unreachable.
class ListBox : public wxVListBox
{
public:
    ListBox(wxWindow *parent, wxWindowID id = wxID_ANY, const wxSize &size = wxDefaultSize, long style = 0);

    void     Set(const std::vector<wxString> &rows);
    void     Set(const wxArrayString &rows) { Set(std::vector<wxString>(rows.begin(), rows.end())); }
    void     Append(const wxString &row);
    void     Clear();
    unsigned GetCount() const { return unsigned(m_rows.size()); }
    wxString GetString(unsigned index) const { return index < m_rows.size() ? m_rows[index] : wxString(); }

    // Recolor the selected pane to a workspace accent (Preview / Device).
    void SetColorScheme(MD3::ColorScheme scheme);

    // Checkable rows, in place of wxCheckListBox: a Material check box glyph
    // starts every row. A click on the glyph or the Space key toggles it and
    // sends wxEVT_CHECKLISTBOX with the row in GetInt(), as wxCheckListBox did.
    void EnableChecks(bool enable = true);
    void Check(unsigned index, bool checked = true);
    bool IsChecked(unsigned index) const { return index < m_checked.size() && m_checked[index] != 0; }
    void Rescale();

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

    std::vector<wxString> m_rows;
    int                   m_hover { -1 };
    std::vector<char>     m_checked;
    bool                  m_checks { false };
    MD3::ColorScheme      m_scheme { MD3::ColorScheme::Brand };
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_ListBox_hpp_
