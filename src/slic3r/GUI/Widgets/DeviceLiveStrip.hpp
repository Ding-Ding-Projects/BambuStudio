#ifndef slic3r_GUI_DeviceLiveStrip_hpp_
#define slic3r_GUI_DeviceLiveStrip_hpp_

#include <vector>

#include <wx/panel.h>
#include <wx/string.h>
#include <wx/window.h>
#include <wx/wrapsizer.h>

namespace Slic3r { namespace GUI {

// A wrapping row of read-only status chips ("Wi-Fi signal  -61 dBm  Strong") shown on the
// print monitor. The strip owns no printer state: StatusPanel hands it the readings that
// the printer actually reported, and it hides itself when there are none, so a printer that
// reports nothing never shows an empty band or a placeholder value.
class DeviceLiveStrip : public wxPanel
{
public:
    struct Reading
    {
        wxString label; // already localized
        wxString value; // already localized and formatted

        bool operator==(const Reading &other) const { return label == other.label && value == other.value; }
        bool operator!=(const Reading &other) const { return !(*this == other); }
    };

    explicit DeviceLiveStrip(wxWindow *parent);

    // Replaces the chips. A no-op when the readings are unchanged, so it is safe to call on
    // every status refresh. An empty list hides the strip.
    void SetReadings(const std::vector<Reading> &readings);

    // Re-measures the chips after a DPI, font or theme change.
    void msw_rescale();

private:
    class ChipWindow;

    wxWrapSizer *            m_sizer{nullptr};
    std::vector<Reading>     m_readings;
    std::vector<ChipWindow *> m_chips;
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_DeviceLiveStrip_hpp_
