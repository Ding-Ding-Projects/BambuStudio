#include "ErrorMsgStaticText.hpp"
#include "../BilingualDecorator.hpp"
#include <wx/dcclient.h>

ErrorMsgStaticText::ErrorMsgStaticText() {}

ErrorMsgStaticText::ErrorMsgStaticText(wxWindow *      parent,
                             wxWindowID      id,
                             const wxPoint & pos,
                             const wxSize &  size)
{
    Create(parent, id, pos, size);
    Bind(wxEVT_PAINT, &ErrorMsgStaticText::paintEvent, this);
}

void ErrorMsgStaticText::SetLabel(const wxString &msg)
{
    m_msg = msg;
    Refresh();
}

void ErrorMsgStaticText::DoSetToolTipText(wxString const &tip)
{
    if (tip != m_bilingual_base_tooltip)
        m_bilingual_note.Clear(); // that note was for the old text; the next paint decides afresh
    m_bilingual_base_tooltip = tip;
    wxWindow::DoSetToolTipText(m_bilingual_note.empty() ? tip : (tip.empty() ? m_bilingual_note : tip + "\n\n" + m_bilingual_note));
}

void ErrorMsgStaticText::paintEvent(wxPaintEvent &evt)
{
    auto size = GetSize();
    wxPaintDC dc(this);
    auto text_height = dc.GetCharHeight();

    // Bilingual mode: the compact English-plus-Cantonese form when the whole
    // message fits on one line at the control's width (rare for a genuine
    // error message); otherwise the message stays English, word-wrapped
    // exactly as before, and the note joins whatever tooltip the caller set.
    wxString note;
    const wxString source_msg = Slic3r::GUI::I18N::fit_bilingual(dc, m_msg, size.x, &note);
    if (note != m_bilingual_note) {
        m_bilingual_note = note;
        wxWindow::DoSetToolTipText(note.empty() ? m_bilingual_base_tooltip
                                                 : (m_bilingual_base_tooltip.empty() ? note : m_bilingual_base_tooltip + "\n\n" + note));
    }

    wxString  out_txt = source_msg;
    wxString  count_txt  = "";
    int line_count  = 1;
    int new_line_pos = 0;
    bool is_ch = false;

    if (source_msg[0] > 0x80 && source_msg[1] > 0x80)is_ch = true;

    for (int i = 0; i < source_msg.length(); i++) {
        auto text_size = dc.GetTextExtent(count_txt);
        if (text_size.x < (size.x)) {
            count_txt += source_msg[i];
            if (source_msg[i] == ' ' ||
                source_msg[i] == ',' ||
                source_msg[i] == '.' ||
                source_msg[i] == '\n')
            {
                new_line_pos = i;
            }
        } else {
            if (!is_ch)
            {
                out_txt[new_line_pos] = '\n';
                i = new_line_pos;
            } else {
                out_txt.insert(i-1,'\n');
            }
            count_txt = "";
            line_count++;
        }
    }
    SetSize(wxSize(-1, line_count * text_height));
    SetMinSize(wxSize(-1, line_count * text_height));
    SetMaxSize(wxSize(-1, line_count * text_height));
    dc.DrawText(out_txt, 0, 0);
}