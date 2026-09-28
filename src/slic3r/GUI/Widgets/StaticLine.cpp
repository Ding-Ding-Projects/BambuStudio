#include "StaticLine.hpp"
#include "Label.hpp"
#include "StateColor.hpp"
#include "../BilingualDecorator.hpp"

#include <wx/dcclient.h>
#include <wx/dcgraph.h>

BEGIN_EVENT_TABLE(StaticLine, wxWindow)

// catch paint events
EVT_PAINT(StaticLine::paintEvent)

END_EVENT_TABLE()

StaticLine::StaticLine(wxWindow *parent, bool vertical, const wxString &label, const wxString &icon)
    : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    , vertical(vertical)
{
    wxWindow::SetBackgroundColour(parent->GetBackgroundColour());
    // Divider tone follows the kit's 1px OutlineVariant. Store the light value;
    // render() runs it through darkModeColorFor(), which remaps #c5c6d0 -> the
    // dark OutlineVariant so the line stays theme-adaptive.
    this->lineColor = MD3::resolve(MD3::Role::OutlineVariant, false);
    DisableFocusFromKeyboard();
    SetFont(Label::Body_14);
    wxWindow::SetLabel(label);
    SetIcon(icon);
}

void StaticLine::SetLabel(const wxString& label)
{
    wxWindow::SetLabel(label);
    messureSize();
    Refresh();
}

void StaticLine::SetIcon(const wxString &icon)
{
    this->icon = icon.IsEmpty() ? ScalableBitmap()
        : ScalableBitmap(this, icon.ToStdString(), 18);
    messureSize();
    Refresh();
}

void StaticLine::SetLineColour(wxColour color)
{
    this->lineColor = color;
}

void StaticLine::DoSetToolTipText(wxString const &tip)
{
    if (tip != bilingual_base_tooltip)
        bilingual_note.Clear(); // that note was for the old text; the next paint decides afresh
    bilingual_base_tooltip = tip;
    wxWindow::DoSetToolTipText(bilingual_note.empty() ? tip : (tip.empty() ? bilingual_note : tip + "\n\n" + bilingual_note));
}

void StaticLine::Rescale()
{
    if (this->icon.bmp().IsOk())
        this->icon.msw_rescale();
    messureSize();
}

void StaticLine::paintEvent(wxPaintEvent& evt)
{
    // depending on your system you may need to look at double-buffered dcs
    wxPaintDC dc(this);
    render(dc);
}

void StaticLine::messureSize()
{
    wxClientDC dc(this);
    wxSize textSize = dc.GetTextExtent(GetLabel());
    wxSize szContent = textSize;
    if (this->icon.bmp().IsOk()) {
        if (szContent.y > 0) {
            // BBS norrow size between text and icon
            szContent.x += 5;
        }
        wxSize szIcon = this->icon.GetBmpSize();
        szContent.x += szIcon.x;
        if (szIcon.y > szContent.y) szContent.y = szIcon.y;
    }
    if (vertical)
        szContent = {std::max(szContent.x, 1), szContent.y + 10};
    else
        szContent = { szContent.x + 10, std::max(szContent.y, 1)};
    SetMinSize(szContent);
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void StaticLine::render(wxDC& dc)
{
    wxSize size = GetSize();
    wxSize textSize;
    auto   label = GetLabel();
    if (!label.IsEmpty()) textSize = dc.GetTextExtent(label);
    wxRect titleRect{{0, 0}, size};
    titleRect.height = wxMax(icon.GetBmpHeight(), textSize.GetHeight());
    int contentWidth = icon.GetBmpWidth() + ((icon.bmp().IsOk() && textSize.GetWidth() > 0) ? 5 : 0) +
                textSize.GetWidth();
    if (vertical) titleRect.Deflate((size.GetWidth() - contentWidth) / 2, 0);
    if (icon.bmp().IsOk()) {
        dc.DrawBitmap(icon.bmp(), {0, (size.y - icon.GetBmpHeight()) / 2});
        titleRect.x += icon.GetBmpWidth() + 5;
    }
    if (!label.IsEmpty()) {
        dc.SetTextForeground(StateColor::darkModeColorFor(GetForegroundColour()));
        // Bilingual mode: the compact English-plus-Cantonese form when it fits
        // the room this divider already has; otherwise the caption stays
        // English and the note joins whatever tooltip the caller set.
        wxString note;
        const wxString shown_label = Slic3r::GUI::I18N::fit_bilingual(dc, label, std::max(0, size.x - titleRect.x), &note);
        if (note != bilingual_note) {
            bilingual_note = note;
            wxWindow::DoSetToolTipText(note.empty() ? bilingual_base_tooltip
                                                     : (bilingual_base_tooltip.empty() ? note : bilingual_base_tooltip + "\n\n" + note));
        }
        dc.DrawText(shown_label, titleRect.x, (size.GetHeight() - textSize.GetHeight()) / 2);
        titleRect.x += textSize.GetWidth() + 5;
    }
    dc.SetPen(wxPen(StateColor::darkModeColorFor(lineColor)));
    if (vertical) {
        size.x /= 2;
        if (titleRect.y > 0) titleRect.y += 5;
        dc.DrawLine(size.x, titleRect.y, size.x, size.y);
    } else {
        size.y /= 2;
        dc.DrawLine(titleRect.x, size.y, size.x, size.y);
    }
}
