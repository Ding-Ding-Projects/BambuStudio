#include "DeviceLiveStrip.hpp"

#include <algorithm>

#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>
#include <wx/graphics.h>

#include "../I18N.hpp"
#include "Label.hpp"
#include "MD3Tokens.hpp"
#include "StateColor.hpp"

namespace Slic3r { namespace GUI {

namespace {
constexpr int kChipPadX   = 10; // logical px
constexpr int kChipPadY   = 4;
constexpr int kChipGap    = 6;  // gap between the label and the value
constexpr int kChipMargin = 4;  // space around each chip in the wrapping row
} // namespace

// One read-only chip. It is not a control: it takes no focus and no input, and exposes its
// full "label: value" text as its name and tooltip for assistive technology and pointer users.
class DeviceLiveStrip::ChipWindow : public wxWindow
{
public:
    ChipWindow(wxWindow *parent, const Reading &reading)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE), m_reading(reading)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
#ifdef __WINDOWS__
        SetDoubleBuffered(true);
#endif
        SetFont(::Label::Body_12);
        const wxString full = reading.label + ": " + reading.value;
        SetName(full);
        SetToolTip(full);
        Bind(wxEVT_PAINT, &ChipWindow::on_paint, this);
        SetMinSize(DoGetBestSize());
    }

    bool AcceptsFocus() const override { return false; }

    void msw_rescale()
    {
        SetFont(::Label::Body_12);
        InvalidateBestSize();
        SetMinSize(DoGetBestSize());
        Refresh();
    }

protected:
    wxSize DoGetBestSize() const override
    {
        int lw = 0, lh = 0, vw = 0, vh = 0;
        GetTextExtent(m_reading.label, &lw, &lh, nullptr, nullptr, &::Label::Body_12);
        GetTextExtent(m_reading.value, &vw, &vh, nullptr, nullptr, &::Label::Head_12);
        return wxSize(lw + vw + FromDIP(kChipGap) + 2 * FromDIP(kChipPadX), std::max(lh, vh) + 2 * FromDIP(kChipPadY));
    }

private:
    void on_paint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC pdc(this);
        const wxSize          sz = GetClientSize();

        pdc.SetBackground(wxBrush(GetParent() ? GetParent()->GetBackgroundColour() : StateColor::semantic(MD3::Role::Surface)));
        pdc.Clear();

        {
            wxGCDC              dc(pdc);
            wxGraphicsContext *gc = dc.GetGraphicsContext();
            if (gc) {
                gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
                gc->SetPen(wxPen(StateColor::semantic(MD3::Role::OutlineVariant), 1));
                gc->SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHigh)));
                gc->DrawRoundedRectangle(0.5, 0.5, sz.x - 1.0, sz.y - 1.0, (sz.y - 1.0) / 2.0);
            }
        }

        int lw = 0, lh = 0, vw = 0, vh = 0;
        pdc.GetTextExtent(m_reading.label, &lw, &lh, nullptr, nullptr, &::Label::Body_12);
        pdc.GetTextExtent(m_reading.value, &vw, &vh, nullptr, nullptr, &::Label::Head_12);

        int x = FromDIP(kChipPadX);
        pdc.SetFont(::Label::Body_12);
        pdc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        pdc.DrawText(m_reading.label, x, (sz.y - lh) / 2);
        x += lw + FromDIP(kChipGap);
        pdc.SetFont(::Label::Head_12);
        pdc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurface));
        pdc.DrawText(m_reading.value, x, (sz.y - vh) / 2);
    }

    Reading m_reading;
};

DeviceLiveStrip::DeviceLiveStrip(wxWindow *parent) : wxPanel(parent, wxID_ANY)
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    SetName(_L("Live printer readings"));
    m_sizer = new wxWrapSizer(wxHORIZONTAL);
    SetSizer(m_sizer);
    Hide();
}

void DeviceLiveStrip::SetReadings(const std::vector<Reading> &readings)
{
    if (readings == m_readings) return;
    m_readings = readings;

    Freeze();
    m_sizer->Clear(true); // destroys the old chip windows
    m_chips.clear();
    for (const Reading &reading : m_readings) {
        auto *chip = new ChipWindow(this, reading);
        m_chips.push_back(chip);
        m_sizer->Add(chip, 0, wxALL | wxALIGN_CENTER_VERTICAL, FromDIP(kChipMargin));
    }
    Show(!m_readings.empty());
    Thaw();

    if (wxWindow *parent = GetParent()) parent->Layout();
}

void DeviceLiveStrip::msw_rescale()
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    for (ChipWindow *chip : m_chips) chip->msw_rescale();
    Layout();
    Refresh();
}

}} // namespace Slic3r::GUI
