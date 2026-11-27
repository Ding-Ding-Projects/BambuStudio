#include "SliceMotion.hpp"
#include "SliceMotionDrawing.hpp"
#include "GUI_App.hpp"
#include "SliceToolpathSnapshot.hpp"
#include "BackgroundSlicingProcess.hpp"
#include "Plater.hpp"
#include "PartPlate.hpp"
#include "GLCanvas3D.hpp"
#include "Widgets/MD3Motion.hpp"
#include "I18N.hpp"
#include <wx/dcbuffer.h>
#include <wx/settings.h>

namespace Slic3r::GUI {
namespace {
using Clock = std::chrono::steady_clock;
std::optional<SliceMotionKey> toast, requested;
uint64_t request_number{};
SliceMotionKind origin = SliceMotionKind::Ordinary;
wxImage current_image;
Clock::time_point began;
std::shared_ptr<const SliceToolpathSnapshot> finalized;
}

bool SliceMotionKey::operator==(const SliceMotionKey &o) const
{
    return model == o.model && plate == o.plate && epoch == o.epoch && index == o.index && generation == o.generation;
}

std::optional<SliceMotionKey> SliceMotion::identify(Plater &plater, int index)
{
    auto &plates = plater.get_partplate_list();
    if (index < 0 || index >= plates.get_plate_count()) return {};
    auto *plate = plates.get_plate(index);
    if (!plate || !plate->has_printable_instances()) return {};
    return SliceMotionKey{plater.model().id().id, plate->id().id, plate->get_slice_invalid_epoch(), index,
                          plater.automation_slice_native_generation()};
}

wxImage SliceMotion::geometry(Plater &plater, const SliceMotionKey &key)
{
    auto *canvas = plater.get_view3D_canvas3D();
    ThumbnailData data;
    if (!canvas || !canvas->render_slice_motion_thumbnail(data, key.index) || !data.is_valid()) return {};
    wxImage image(data.width, data.height);
    image.InitAlpha();
    for (unsigned y = 0; y < data.height; ++y)
        for (unsigned x = 0; x < data.width; ++x) {
            const auto *p = &data.pixels[4 * ((data.height - 1 - y) * data.width + x)];
            image.SetRGB(x, y, p[0], p[1], p[2]);
            image.SetAlpha(x, y, p[3]);
        }
    return image;
}

void SliceMotion::request_dialog(Plater &plater)
{
    requested = identify(plater, plater.get_partplate_list().get_curr_plate_index());
    request_number = plater.automation_slice_request_generation() + 1;
}

void SliceMotion::clear_request() { requested.reset(); }

void SliceMotion::begin(Plater &plater, int index)
{
    finalized.reset();
    toast = identify(plater, index);
    origin = SliceMotionKind::Ordinary;
    if (toast && requested && requested->model == toast->model && requested->plate == toast->plate &&
        request_number == plater.automation_slice_request_generation())
        origin = SliceMotionKind::DialogRebuild;
    requested.reset();
    current_image = toast ? geometry(plater, *toast) : wxImage{};
    began = Clock::now();
}

void SliceMotion::stop() { toast.reset(); finalized.reset(); current_image = wxImage{}; }
void SliceMotion::finish(Plater &plater, int index)
{
    if (!toast || index != toast->index || !active(plater)) return;
    auto *plate = plater.get_partplate_list().get_plate(index);
    const auto *result = plate->get_gcode_result();
    if (plate->is_slice_result_valid() && result)
        finalized = make_slice_toolpath_snapshot({toast->generation,index,result->id}, *result, 4096);
}
std::shared_ptr<const SliceToolpathSnapshot> SliceMotion::paths(Plater &plater)
{
    if (!active(plater)) return {};
    return finalized ? finalized : plater.background_process().live_slice_toolpath(toast->generation, toast->index);
}
bool SliceMotion::active(Plater &plater)
{
    return toast && !plater.background_process().cancellation_requested() && plater.get_partplate_list().get_curr_plate_index() == toast->index && identify(plater, toast->index) == toast;
}
SliceMotionKind SliceMotion::kind() { return origin; }
int SliceMotion::plate_index() { return toast ? toast->index : -1; }
const wxImage &SliceMotion::image() { return current_image; }
double SliceMotion::seconds() { return std::chrono::duration<double>(Clock::now()-began).count(); }

SliceMotionPanel::SliceMotionPanel(wxWindow *parent) : wxPanel(parent), m_timer(this)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(FromDIP(wxSize(96, 96)));
    Bind(wxEVT_PAINT, &SliceMotionPanel::paint, this);
    Bind(wxEVT_TIMER, [this](wxTimerEvent &) { tick(); });
    Hide();
}

void SliceMotionPanel::show_required(Plater &plater, int index)
{
    const auto key = SliceMotion::identify(plater, index);
    if (!key) { clear(); return; }
    if (!(key == m_required)) {
        m_required = key;
        const auto image = SliceMotion::geometry(plater, *key);
        m_image = image.IsOk() ? wxBitmap(image) : wxNullBitmap;
        m_start = Clock::now();
    }
    SetName(_L("Plate needs slicing") + " / " + _L("Model geometry"));
    Show();
    tick();
}

void SliceMotionPanel::show_active()
{
    m_required.reset();
    Show();
    tick();
}

void SliceMotionPanel::clear()
{
    m_timer.Stop(); m_required.reset(); m_image = wxNullBitmap; Hide();
}

void SliceMotionPanel::tick()
{
    if (!IsShownOnScreen() || MD3::Motion::reduced()) m_timer.Stop();
    else if (!m_timer.IsRunning()) m_timer.Start(50);
    Refresh(false);
}

void SliceMotionPanel::paint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour())); dc.Clear();
    auto *plater = wxGetApp().plater();
    if (!plater || (!m_required && !SliceMotion::active(*plater))) return;
    if (m_required && !(SliceMotion::identify(*plater, m_required->index) == m_required)) return;
    const bool required = m_required.has_value();
    const int index = required ? m_required->index : SliceMotion::plate_index();
    const auto paths = required ? nullptr : SliceMotion::paths(*plater);
    const wxString state = required ? _L("Plate needs slicing") : _L("Slicing");
    wxString label = state + "  /  " + wxString::Format(_L("Plate %d"), index + 1);
    if (!required) label += "  /  " + (paths && paths->segments.size() < paths->source_segment_count ?
        _L("Sampled print order") : paths ? _L("Print order preview") : _L("Model geometry"));
    if (GetName() != label) SetName(label);
    dc.SetTextForeground(GetForegroundColour());
    dc.SetFont(GetFont()); dc.DrawLabel(label, GetClientRect(), wxALIGN_BOTTOM | wxALIGN_CENTER_HORIZONTAL);
    const int w = GetClientSize().x, h = GetClientSize().y - dc.GetCharHeight() - FromDIP(4);
    if (w <= 0 || h <= 0) return;
    wxImage image = required ? (m_image.IsOk() ? m_image.ConvertToImage() : wxImage{}) : SliceMotion::image();
    if (!image.IsOk()) return;
    const int model_w = paths ? w / 2 : w;
    const int side = std::min(model_w,h);
    dc.DrawBitmap(wxBitmap(image.Scale(side,side,wxIMAGE_QUALITY_HIGH)),(model_w-side)/2,(h-side)/2,true);
    const wxColour accent = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    const double elapsed = required ? std::chrono::duration<double>(Clock::now()-m_start).count() : SliceMotion::seconds();
    if (paths) draw_slice_paths(*paths,elapsed,MD3::Motion::reduced(),[&](double x,double y,double xx,double yy,bool head) {
        dc.SetPen(wxPen(accent,FromDIP(head ? 3 : 1)));
        dc.DrawLine(model_w+x*model_w,y*h,model_w+xx*model_w,yy*h);
    });
    draw_slice_motion(required ? SliceMotionKind::Required : SliceMotion::kind(),elapsed,MD3::Motion::reduced(),
        [&](double x,double y,double x2,double y2,int weight) {
            dc.SetPen(wxPen(GetBackgroundColour(),FromDIP(weight+2))); dc.DrawLine(w*x,h*y,w*x2,h*y2);
            dc.SetPen(wxPen(accent,FromDIP(weight))); dc.DrawLine(w*x,h*y,w*x2,h*y2);
        }, [&](double x,double y,double rx,double ry) {
            dc.SetPen(wxPen(accent,FromDIP(2))); dc.DrawEllipse(w*(x-rx),h*(y-ry),2*w*rx,2*h*ry);
        });
}
}
