#include "ProgressBar.hpp"
#include "StateColor.hpp"
#include "../I18N.hpp"
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include "Label.hpp"
#include <algorithm>



wxDEFINE_EVENT(wxCUSTOMEVT_SET_TEMP_FINISH, wxCommandEvent);
wxDEFINE_EVENT(EVT_PROGRESS_BAR_HEIGHT_CHANGED, wxCommandEvent);
BEGIN_EVENT_TABLE(ProgressBar, wxWindow)
EVT_PAINT(ProgressBar::paintEvent)
EVT_MOTION(ProgressBar::mouseMove)
EVT_LEAVE_WINDOW(ProgressBar::mouseLeave)
END_EVENT_TABLE()

ProgressBar::ProgressBar(wxWindow *parent, wxWindowID id, int max, const wxPoint &pos, const wxSize &size, bool shown)
{
    m_shownumber = shown;
    // Theme-adaptive erase colour matching the track's own SurfaceContainerHighest
    // fill, instead of a raw white literal that showed through in dark mode.
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerHighest));

    if (size.y >= miniHeight) {
        m_miniHeight = size.y;
    } else {
        m_miniHeight = miniHeight;
    }

    m_max = max;
    m_radius = defaultRadius;
    wxSize temp_size(size.x, m_miniHeight);

    SetFont(Label::Head_12);
    create(parent, id, pos, temp_size);
    m_pulse_timer.SetOwner(this);
    Bind(wxEVT_TIMER, &ProgressBar::onPulseTick, this, m_pulse_timer.GetId());
}

void ProgressBar::SetRange(int range)
{
    if (range <= 0) return;
    m_max = range;
    if (m_step > m_max) m_step = m_max;
    Refresh();
}

void ProgressBar::Pulse()
{
    m_disable = false;
    if (!m_indeterminate) {
        m_indeterminate = true;
        m_pulse_phase   = 0.0;
    }
    if (!m_pulse_timer.IsRunning())
        m_pulse_timer.Start(40);
    Refresh();
}

void ProgressBar::onPulseTick(wxTimerEvent &)
{
    if (!m_indeterminate || !IsShownOnScreen()) {
        m_pulse_timer.Stop();
        return;
    }
    m_pulse_phase += 0.02;
    if (m_pulse_phase > 1.0) m_pulse_phase -= 1.0;
    Refresh();
}


ProgressBar::~ProgressBar() {}


void ProgressBar::create(wxWindow *parent, wxWindowID id, const wxPoint &pos,  wxSize &size)
{
    wxWindow::Create(parent, id, pos, size);
    // m_static_info = new wxStaticText(this, wxID_ANY,wxT(""),wxPoint(this->padding, 20), wxSize(GetSize().GetWidth() - this->padding * 3, -1), wxST_ELLIPSIZE_END);
    // m_static_info->Wrap(-1);

   /* wxBoxSizer *m_sizer_body  = new wxBoxSizer(wxHORIZONTAL);

     auto m_progress_bk = new StaticBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
     m_progress_bk->SetBackgroundColour(wxColour(238, 130, 238));
     StateColor btn_bg_green(std::pair<wxColour, int>(ThemeColor::BrandGreenPressed, StateColor::Pressed), std::pair<wxColour, int>(ThemeColor::BrandGreenHovered, StateColor::Hovered),
                             std::pair<wxColour, int>(ThemeColor::BrandGreen, StateColor::Normal));

     wxBoxSizer *m_sizer_progress= new wxBoxSizer(wxHORIZONTAL);

     auto m_progress = new wxPanel(m_progress_bk, wxID_ANY, wxDefaultPosition, wxSize(50, -1), wxTAB_TRAVERSAL);
     m_progress->SetBackgroundColour(wxColour(128, 0, 255));

     m_sizer_progress->Add(m_progress, 0, wxEXPAND, 0);

     m_progress_bk->SetSizer(m_sizer_progress);
     m_progress_bk->Layout();
     m_sizer_progress->Fit(m_progress_bk);
     m_sizer_body->Add(m_progress_bk, 1, wxEXPAND, 0);

     this->SetSizer(m_sizer_body);
     this->Layout();*/
}


void ProgressBar::SetRadius(double radius) {
    m_radius = radius;
    Refresh();
}

void ProgressBar::SetProgressForedColour(wxColour colour)
{
    m_progress_background_colour = colour;
    Refresh();
}

void ProgressBar::SetProgressBackgroundColour(wxColour colour)
{
    m_progress_colour = colour;
     Refresh();
}

void ProgressBar::SetMarkers(const std::vector<Marker> &markers)
{
    const bool unchanged = m_markers.size() == markers.size() &&
                           std::equal(m_markers.begin(), m_markers.end(), markers.begin(),
                                      [](const auto &lhs, const auto &rhs) {
                                          return lhs.m_position == rhs.m_position && lhs.m_label == rhs.m_label;
                                      });
    if (unchanged)
        return;

    m_markers = markers;
    m_hoveredMarker = m_lastMousePosition ? findHoveredMarker(*m_lastMousePosition) : -1;
    updateControlHeight();
    Refresh();
}

void ProgressBar::SetHeight(int height)
{
    m_barHeight = std::max(1, height);
    m_minHeight = m_barHeight;
    m_radius    = m_barHeight / 2;
    updateControlHeight();
}

void ProgressBar::updateControlHeight()
{
    const bool hasLabel = std::any_of(m_markers.begin(), m_markers.end(), [](const auto &marker) {
        return !marker.m_label.empty();
    });
    const int height = m_barHeight + (hasLabel ? FromDIP(34) : 0);
    if (GetMinSize().GetHeight() == height && GetSize().GetHeight() == height)
        return;

    wxSize minSize = GetMinSize();
    minSize.SetHeight(height);
    wxWindow::SetMinSize(minSize);
    SetSize(GetSize().GetWidth(), height);
    wxCommandEvent event(EVT_PROGRESS_BAR_HEIGHT_CHANGED, GetId());
    event.SetEventObject(this);
    event.StopPropagation();
    ProcessWindowEvent(event);
}

void ProgressBar::Rescale()
{
    ;
}

void ProgressBar::ShowNumber(bool shown)
{
    m_shownumber = shown;
    Refresh();
}

void ProgressBar::Disable(wxString text)
{
    if (m_disable) return;
    m_disable_text = text;
    m_disable = true;
    Refresh();
}

void ProgressBar::SetValue(int step)
{
    m_disable = false;
    SetProgress(step);
}

void ProgressBar::Reset()
{
    m_step = 0;
    SetValue(0);
}

void ProgressBar::SetProgress(int step)
{
    if (step < 0) return;
    const bool was_indeterminate = m_indeterminate;
    m_indeterminate = false;
    m_pulse_timer.Stop();
    if (!was_indeterminate && m_disable == false && m_step == step)
    {
        return;
    }

    m_disable = false;
    m_step = step;
    Refresh();
}


void ProgressBar::SetMinSize(const wxSize &size)
{
    if (size.y >= miniHeight) {
        m_miniHeight = size.y;
    } else {
        return;
    }

    m_radius = defaultRadius;
    wxWindow::SetMinSize({size.x, m_miniHeight});
    // SetSize(size);
    SetRadius(m_radius);
}


void ProgressBar::paintEvent(wxPaintEvent &evt)
{

    wxPaintDC dc(this);
    render(dc);
}

int ProgressBar::findHoveredMarker(const wxPoint &position) const
{
    const wxSize size = GetClientSize();
    if (position.y < 0 || position.y > m_barHeight || size.x <= 0 || m_max <= 0)
        return -1;

    const int hoverTolerance = FromDIP(7);
    for (size_t index = 0; index < m_markers.size(); ++index) {
        if (m_markers[index].m_label.empty())
            continue;

        const int markerX = size.x * std::clamp(m_markers[index].m_position, 0, m_max) / m_max;
        if (position.x >= markerX - hoverTolerance && position.x <= markerX + hoverTolerance)
            return static_cast<int>(index);
    }
    return -1;
}

void ProgressBar::mouseMove(wxMouseEvent &evt)
{
    m_lastMousePosition = evt.GetPosition();
    const int hoveredMarker = findHoveredMarker(*m_lastMousePosition);
    if (m_hoveredMarker != hoveredMarker) {
        m_hoveredMarker = hoveredMarker;
        Refresh();
    }
    evt.Skip();
}

void ProgressBar::mouseLeave(wxMouseEvent &evt)
{
    m_lastMousePosition.reset();
    if (m_hoveredMarker != -1) {
        m_hoveredMarker = -1;
        Refresh();
    }
    evt.Skip();
}

void ProgressBar::render(wxDC &dc)
{
#ifdef __WXMSW__
    wxSize     size = GetSize();
    wxMemoryDC memdc;
    wxBitmap   bmp(size.x, size.y);
    memdc.SelectObject(bmp);
    memdc.Blit({0, 0}, size, &dc, {0, 0});

    {
        wxGCDC dc2(memdc);
        doRender(dc2);
    }

    memdc.SelectObject(wxNullBitmap);
    dc.DrawBitmap(bmp, 0, 0);
#else
    doRender(dc);
#endif
}

void ProgressBar::doRender(wxDC &dc)
{
    if (m_step >= m_max) m_step = m_max;
    wxSize size   = GetSize();
    // The track is m_barHeight tall; the window can be taller when marker
    // bubbles sit below it (upstream SetHeight), so every track draw uses
    // barHeight, never the window height.
    const int barHeight = std::max(1, std::min(m_barHeight, size.y));
    // The kit uses a fixed soft radius (r6); clamp it to the track's half-height
    // so short bars round to a clean stadium end (matching CSS border-radius)
    // while taller bars keep the soft r6 corner instead of a full height/2 pill.
    const double drawRadius = (m_radius > barHeight / 2.0) ? barHeight / 2.0 : m_radius;
    dc.SetPen(wxPen(m_progress_background_colour, 1));
    dc.SetBrush(wxBrush(m_progress_background_colour));
    if (m_radius == 0) {
        dc.DrawRectangle(0, 0, size.x, barHeight);
    } else {
        dc.DrawRoundedRectangle(0, 0, size.x, barHeight, drawRadius);
    }

    //draw progress
    if (m_disable) {
        m_proportion = float(size.x * float(this->m_step) / float(this->m_max));
        if (m_proportion < m_radius * 2 && m_proportion != 0) { m_proportion = m_radius * 2; }

        dc.SetPen(wxPen(m_progress_colour_disable, 1));
        dc.SetBrush(wxBrush(m_progress_colour_disable));
        if (m_radius == 0) {
            dc.DrawRectangle(0, 0, m_proportion, barHeight);
        } else {
            dc.DrawRoundedRectangle(0, 0, m_proportion, barHeight, drawRadius);
        }

        dc.SetFont(::Label::Head_12);
        auto textSize = dc.GetMultiLineTextExtent(m_disable_text);
        dc.SetTextForeground(ThemeColor::TextDisabled);
        auto pt = wxPoint();
        pt.x    = (size.x - textSize.x) / 2;
        pt.y    = (barHeight - textSize.y) / 2;
        dc.DrawText(m_disable_text, pt);

    } else if (m_indeterminate) {
        // Indeterminate: a 30% Primary segment sweeping left to right.
        const double seg  = std::max(size.x * 0.3, m_radius * 2.0);
        const double span = size.x + seg;
        const double x    = m_pulse_phase * span - seg;
        dc.SetPen(wxPen(m_progress_colour, 1));
        dc.SetBrush(wxBrush(m_progress_colour));
        if (m_radius == 0) {
            dc.DrawRectangle(x, 0, seg, size.y);
        } else {
            dc.DrawRoundedRectangle(x, 0, seg, barHeight, drawRadius);
        }
    } else {
        m_proportion = float(size.x * float(this->m_step) / float(this->m_max));
        if (m_proportion < m_radius * 2  && m_proportion != 0) { m_proportion = m_radius * 2; }

        dc.SetPen(wxPen(m_progress_colour, 1));
        dc.SetBrush(wxBrush(m_progress_colour));
        if (m_radius == 0) {
            dc.DrawRectangle(0, 0, m_proportion, barHeight);
        } else {
            dc.DrawRoundedRectangle(0, 0, m_proportion, barHeight, drawRadius);
        }

        // Kit ProgressBar bakes no percentage text into the bar itself
        // (ui-md3 containment/ProgressBar.jsx); any readout is externalized
        // to an adjacent label by the caller.
    }

    renderMarkers(dc, size, barHeight);

}

void ProgressBar::renderMarkers(wxDC &dc, const wxSize &size, int barHeight)
{
    if (m_markers.empty() || size.x <= 0 || m_max <= 0)
        return;

    const wxColour background = GetBackgroundColour();
    const int      gapWidth   = std::max(2, FromDIP(2));
    for (const auto &marker : m_markers) {
        const int position = std::clamp(marker.m_position, 0, m_max);
        const int markerX  = size.x * position / m_max;
        if (markerX > 0 && markerX < size.x) {
            dc.SetPen(wxPen(background, 1));
            dc.SetBrush(wxBrush(background));
            dc.DrawRectangle(markerX - gapWidth / 2, 0, gapWidth, barHeight);
        }
    }

    for (size_t index = 0; index < m_markers.size(); ++index) {
        const auto &marker = m_markers[index];
        if (marker.m_label.empty() || static_cast<int>(index) != m_hoveredMarker)
            continue;

        const int position = std::clamp(marker.m_position, 0, m_max);
        const int markerX  = size.x * position / m_max;
        dc.SetFont(::Label::Body_11);
        const wxSize textSize    = dc.GetTextExtent(marker.m_label);
        const int    bubbleWidth = std::min(size.x, textSize.x + FromDIP(20));
        const int    bubbleHeight = textSize.y + FromDIP(12);
        const int    bubbleY      = barHeight + FromDIP(5);
        const int    bubbleX      = std::clamp(markerX - bubbleWidth / 2, 0, std::max(0, size.x - bubbleWidth));
        const wxColour bubbleBackground = ThemeColor::Grey700;
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(bubbleBackground));
        dc.DrawRoundedRectangle(bubbleX, bubbleY, bubbleWidth, bubbleHeight, FromDIP(5));
        wxPoint pointer[] = {{markerX, barHeight + FromDIP(1)},
                             {markerX - FromDIP(4), bubbleY + FromDIP(1)},
                             {markerX + FromDIP(4), bubbleY + FromDIP(1)}};
        dc.DrawPolygon(3, pointer);
        dc.SetTextForeground(ThemeColor::White);
        dc.DrawText(marker.m_label, bubbleX + (bubbleWidth - textSize.x) / 2,
                    bubbleY + (bubbleHeight - textSize.y) / 2);
        break;
    }

}


void ProgressBar::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    wxWindow::DoSetSize(x, y, width, height, sizeFlags);
}
