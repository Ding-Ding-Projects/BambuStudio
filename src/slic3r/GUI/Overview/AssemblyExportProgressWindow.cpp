#include "AssemblyExportProgressWindow.hpp"

#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/StateColor.hpp"

#include <wx/bitmap.h>
#include <wx/brush.h>
#include <wx/dcmemory.h>
#include <wx/gauge.h>
#include <wx/panel.h>
#include <wx/region.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/window.h>

#include <algorithm>

namespace Slic3r {
namespace GUI {

// The card is a borderless, shaped frame like the busy notice (BusyInfo in
// MsgDialog.cpp): no system border line, a SurfaceContainerHigh fill that follows
// the theme, and the rounded dialog silhouette cut out of the window itself.
// wxFRAME_SHAPED is what lets SetShape() take effect.
AssemblyExportProgressWindow::AssemblyExportProgressWindow(wxWindow *parent)
    : wxFrame(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
              wxFRAME_NO_TASKBAR | wxFRAME_SHAPED | wxBORDER_NONE | wxSTAY_ON_TOP)
{
    Bind(wxEVT_SHOW, [this](wxShowEvent &event) {
        if (event.GetEventObject() == this && !event.IsShown())
            m_entrance.Stop();
        event.Skip();
    });
    SetFont(wxGetApp().normal_font());
    const wxColour surface = StateColor::semantic(MD3::Role::SurfaceContainerHigh);
    SetBackgroundColour(surface);

    wxPanel *panel = new wxPanel(this, wxID_ANY);
    panel->SetBackgroundColour(surface);

    m_message = new Label(panel, wxEmptyString);
    m_message->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_message->SetFont(::Label::Body_13);

    m_gauge = new ProgressBar(panel, wxID_ANY, 100, wxDefaultPosition, wxSize(FromDIP(360), FromDIP(8)));
    m_gauge->SetMinSize(wxSize(FromDIP(300), FromDIP(8)));

    m_percent = new Label(panel, "0%");
    m_percent->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_percent->SetFont(::Label::Body_13);
    m_percent->SetMinSize(wxSize(FromDIP(40), -1));

    m_cancel = new Button(panel, _L("Cancel"));
    m_cancel->SetMinSize(wxSize(FromDIP(58), FromDIP(22)));
    m_cancel->SetMaxSize(wxSize(FromDIP(58), FromDIP(22)));
    m_cancel->SetVariant(Button::Variant::Outlined);
    m_cancel->SetCornerRadius(FromDIP(12));
    m_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_cancel(); });

    wxBoxSizer *row = new wxBoxSizer(wxHORIZONTAL);
    row->Add(m_gauge, 1, wxALIGN_CENTER_VERTICAL);
    row->Add(m_percent, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(10));
    row->Add(m_cancel, 0, wxALIGN_CENTER_VERTICAL);

    // The rounded corners cut into the card, so the content keeps a little more
    // room from the edge than a square frame needed.
    const int edge = FromDIP(16);
    wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_message, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, edge);
    sizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, edge);
    panel->SetSizer(sizer);

    wxBoxSizer *root_sizer = new wxBoxSizer(wxVERTICAL);
    root_sizer->Add(panel, 1, wxEXPAND);
    SetSizer(root_sizer);
    Fit();
    apply_shape();
}

AssemblyExportProgressWindow::~AssemblyExportProgressWindow()
{
    m_entrance.Stop();
}

// Cuts the rounded dialog silhouette out of the window, the same mask-bitmap to
// wxRegion route the busy notice uses. It is redone only when the size changed,
// because update_progress() fits the frame on every tick.
void AssemblyExportProgressWindow::apply_shape()
{
    const wxSize size = GetSize();
    if (size.GetWidth() <= 0 || size.GetHeight() <= 0 || size == m_shape_size)
        return;

    wxBitmap mask(size.GetWidth(), size.GetHeight(), 32);
    {
        wxMemoryDC dc;
        dc.SelectObject(mask);
        dc.SetBackground(wxBrush(wxColour(0, 0, 0)));
        dc.Clear();
        dc.SetBrush(*wxWHITE_BRUSH); // the mask: any colour but the black background
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRoundedRectangle(0, 0, size.GetWidth(), size.GetHeight(), FromDIP(MD3::Metrics::radius_dialog));
        dc.SelectObject(wxNullBitmap);
    }

    wxRegion region(mask, wxColour(0, 0, 0));
    if (region.IsOk() && SetShape(region))
        m_shape_size = size;
}

void AssemblyExportProgressWindow::set_cancel_callback(std::function<void()> cb)
{
    m_cancel_cb = std::move(cb);
}

void AssemblyExportProgressWindow::enable_cancel(bool enable)
{
    if (m_cancel)
        m_cancel->Enable(enable);
}

void AssemblyExportProgressWindow::on_cancel()
{
    if (m_cancel)
        m_cancel->Enable(false);
    if (m_cancel_cb)
        m_cancel_cb();
}

void AssemblyExportProgressWindow::update_progress(const wxString &message, int value, int maximum, wxWindow *anchor)
{
    if (maximum <= 0)
        maximum = 1;
    value = std::max(0, std::min(value, maximum));

    if (m_message)
        m_message->SetLabel(message);
    if (m_gauge) {
        if (m_gauge->GetRange() != maximum)
            m_gauge->SetRange(maximum);
        m_gauge->SetValue(value);
    }
    if (m_percent)
        m_percent->SetLabel(wxString::Format("%d%%", value * 100 / maximum));

    Layout();
    Fit();
    apply_shape();
    position_near_anchor(anchor);
    if (!IsShown()) {
        ShowWithoutActivating();
        // Start only on a real reveal, never on each progress update. The
        // controller defers native opacity until visibility is established.
        if (IsShown())
            m_entrance.Show(this, MD3::Motion::short2);
    }
    Raise();
}

void AssemblyExportProgressWindow::position_near_anchor(wxWindow *anchor)
{
    if (!anchor)
        return;

    const wxSize anchor_size = anchor->GetClientSize();
    const wxSize win_size    = GetSize();
    const int margin         = FromDIP(16);
    wxPoint pos = anchor->ClientToScreen(wxPoint(margin, std::max(margin, anchor_size.y - win_size.y - margin)));
    Move(pos);
}

} // namespace GUI
} // namespace Slic3r
