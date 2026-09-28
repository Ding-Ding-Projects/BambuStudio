#ifndef slic3r_GUI_BilingualDecorator_hpp_
#define slic3r_GUI_BilingualDecorator_hpp_

#include <wx/string.h>

class wxDC;
class wxWindow;

namespace Slic3r { namespace GUI { namespace I18N {

// The display side of bilingual mode. Every legacy lookup hands English to the
// UI and records the Hong Kong Cantonese that belongs to it (BilingualRegistry);
// this watcher gives each shown label, button, check box, radio button, group
// box, tooltip and native menu item its second language:
//
//  * single-line text reads "English · 廣東話" when that fits the space its
//    layout can give it (a dialog may grow inside the screen for it); otherwise
//    it stays English and the tooltip adds "廣東話：…";
//  * wrapped and multi-line text shows the Cantonese below the English;
//  * tooltips show the Cantonese below the English and never accumulate.
//
// Text the user types, list and combo values, and windows that already render
// both languages themselves (apply_localized_text) are never touched.
void enable_bilingual_decorator(bool enable);

// Ask for another pass over the window's top-level window soon, for code that
// rebuilds its labels without showing anything.
void refresh_bilingual_decoration(wxWindow *window);

// For text a widget paints itself (settings labels, tab captions, steps).
// Returns what to draw for `english` in `width` pixels with the DC's current
// font: "English · 廣東話" when that fits, otherwise the English alone, with
// "廣東話：…" in `tooltip_note` for the widget's tooltip. Outside bilingual
// mode, or for text without a Cantonese entry, it returns `english` unchanged
// and leaves the note empty.
wxString fit_bilingual(wxDC &dc, const wxString &english, int width, wxString *tooltip_note = nullptr);

// The Cantonese recorded for English on screen, or empty (not bilingual mode,
// or no catalogue entry). For stacked text and tooltips a widget builds itself.
wxString bilingual_secondary(const wxString &english);

}}} // namespace Slic3r::GUI::I18N

#endif // slic3r_GUI_BilingualDecorator_hpp_
