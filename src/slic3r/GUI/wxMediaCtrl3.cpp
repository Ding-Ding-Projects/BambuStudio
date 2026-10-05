#include "wxMediaCtrl3.h"
#include "AVVideoDecoder.hpp"
#include "I18N.hpp"
#include "libslic3r/Utils.hpp"
#include <boost/log/trivial.hpp>
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/graphics.h>
#include "nanosvg/nanosvg.h"
#include "nanosvg/nanosvgrast.h"
#ifdef __WIN32__
#include <versionhelpers.h>
#include <wx/msw/registry.h>
#include <shellapi.h>
#endif

wxDEFINE_EVENT(EVT_MEDIA_CTRL_VIEW_CHANGED, wxCommandEvent);

BEGIN_EVENT_TABLE(wxMediaCtrl3, wxWindow)

// catch paint events
EVT_PAINT(wxMediaCtrl3::paintEvent)

END_EVENT_TABLE()

struct StaticBambuLib : BambuLib
{
    static StaticBambuLib &get(BambuLib *);
};

wxMediaCtrl3::wxMediaCtrl3(wxWindow *parent)
    : wxWindow(parent, wxID_ANY)
    , BambuLib(StaticBambuLib::get(this))
    , m_thread([this] { PlayThread(); })
    , m_frame_buffer(9)
{
    SetBackgroundColour("#000001ff");
    m_render_timer.SetOwner(this);
    Bind(wxEVT_TIMER, &wxMediaCtrl3::OnRenderTimer, this);
    Bind(wxEVT_MOUSEWHEEL, &wxMediaCtrl3::mouseWheelEvent, this);
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &evt) {
        SetFocus();
        if (GetZoom() > 1) { m_dragging = true; m_drag_point = evt.GetPosition(); CaptureMouse(); }
        else evt.Skip();
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &evt) {
        if (m_dragging) { m_dragging = false; if (HasCapture()) ReleaseMouse(); } evt.Skip();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) { m_dragging = false; });
    Bind(wxEVT_MOTION, [this](wxMouseEvent &evt) {
        if (m_dragging && evt.LeftIsDown()) {
            const wxPoint point = evt.GetPosition();
            PanBy(point.x - m_drag_point.x, point.y - m_drag_point.y); m_drag_point = point;
        } else evt.Skip();
    });
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &evt) {
        const int key = evt.GetKeyCode();
        if (key == '+' || key == '=' || key == WXK_ADD || key == WXK_NUMPAD_ADD) SetZoom(GetZoom() * 1.1);
        else if (key == '-' || key == WXK_SUBTRACT || key == WXK_NUMPAD_SUBTRACT) SetZoom(GetZoom() / 1.1);
        else if (key == '0' || key == WXK_NUMPAD0) ResetCameraView();
        else if (key == WXK_LEFT) PanBy(FromDIP(30), 0);
        else if (key == WXK_RIGHT) PanBy(-FromDIP(30), 0);
        else if (key == WXK_UP) PanBy(0, FromDIP(30));
        else if (key == WXK_DOWN) PanBy(0, -FromDIP(30));
        else evt.Skip();
    });
#if wxUSE_GESTURES
    EnableTouchEvents(wxTOUCH_ZOOM_GESTURE);
    Bind(wxEVT_GESTURE_ZOOM, [this](wxZoomGestureEvent &evt) {
        if (evt.IsGestureStart()) m_gesture_start_zoom = GetZoom();
        auto geometry = CameraGeometry(); const auto point = evt.GetPosition();
        geometry.zoom_at(m_gesture_start_zoom * evt.GetZoomFactor(), point.x, point.y); SetCameraView(geometry.view);
    });
#endif
}

Slic3r::GUI::CameraViewGeometry wxMediaCtrl3::CameraGeometry()
{
    Slic3r::GUI::CameraViewGeometry geometry;
    geometry.view = m_camera_view;
    const auto size = GetClientSize(); geometry.width = size.x; geometry.height = size.y;
    { std::lock_guard<std::mutex> lock(m_ui_mutex);
      if (m_frame.IsOk()) { const auto frame = m_frame.GetSize(); geometry.image_width = frame.x; geometry.image_height = frame.y; } }
    geometry.clamp(); return geometry;
}
void wxMediaCtrl3::NotifyCameraViewChanged()
{
    Refresh(false);
    wxCommandEvent event(EVT_MEDIA_CTRL_VIEW_CHANGED, GetId());
    event.SetEventObject(this); event.SetInt(wxRound(GetZoom() * 100)); ProcessWindowEvent(event);
}
void wxMediaCtrl3::SetCameraView(const Slic3r::GUI::CameraViewState &view)
{
    auto geometry = CameraGeometry(); geometry.view = view; geometry.clamp();
    const auto &next = geometry.view;
    if (next.zoom == m_camera_view.zoom && next.center_x == m_camera_view.center_x && next.center_y == m_camera_view.center_y) return;
    m_camera_view = next; NotifyCameraViewChanged();
}
void wxMediaCtrl3::SetZoom(double zoom)
{
    auto geometry = CameraGeometry(); geometry.zoom_at(zoom, geometry.width / 2, geometry.height / 2); SetCameraView(geometry.view);
}
void wxMediaCtrl3::ZoomAt(double factor, const wxPoint &anchor)
{
    if (!std::isfinite(factor) || factor <= 0) return;
    auto geometry = CameraGeometry(); geometry.zoom_at(GetZoom() * factor, anchor.x, anchor.y); SetCameraView(geometry.view);
}
void wxMediaCtrl3::PanBy(double dx, double dy)
{
    auto geometry = CameraGeometry(); geometry.pan(dx, dy); SetCameraView(geometry.view);
}
void wxMediaCtrl3::mouseWheelEvent(wxMouseEvent &evt)
{
    if (!evt.GetWheelRotation() || evt.GetWheelAxis() != wxMOUSE_WHEEL_VERTICAL) { evt.Skip(); return; }
    const int delta = evt.GetWheelDelta();
    ZoomAt(std::pow(1.1, double(evt.GetWheelRotation()) / (delta > 0 ? delta : 120)), evt.GetPosition());
}
wxMediaCtrl3::~wxMediaCtrl3()
{
    {
        std::unique_lock<std::mutex> lk(m_ui_mutex);
        m_frame = wxImage(m_idle_image);
        m_frame_is_default_bg = !m_idle_image.empty();
    }
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_url.reset(new wxURI);
        m_cond.notify_all();
    }
    m_thread.join();
}

void wxMediaCtrl3::Load(wxURI url, std::chrono::system_clock::time_point play_start_time)
{
    std::unique_lock<std::mutex> lk(m_mutex);
    m_video_size = wxDefaultSize;
    m_error = 0;
    m_play_start_time = play_start_time;
    m_url.reset(new wxURI(url));
    m_cond.notify_all();
}

void wxMediaCtrl3::Play()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_state != wxMEDIASTATE_PLAYING) {
        m_state = wxMEDIASTATE_PLAYING;
        wxMediaEvent event(wxEVT_MEDIA_STATECHANGED);
        event.SetId(GetId());
        event.SetEventObject(this);
        wxPostEvent(this, event);
    }
}

void wxMediaCtrl3::Stop()
{
    {
        std::unique_lock<std::mutex> lk(m_ui_mutex);
        m_frame = wxImage(m_idle_image);
        m_frame_is_default_bg = !m_idle_image.empty();
    }
    std::unique_lock<std::mutex> lk(m_mutex);
    m_url.reset();
    NotifyStopped();
    m_cond.notify_all();
    Refresh();
}

void wxMediaCtrl3::SetIdleImage(wxString const &image, wxString const &watermark_text)
{
    if (m_idle_image == image && m_watermark_text == watermark_text)
        return;
    m_idle_image = image;
    m_watermark_text = watermark_text;
    if (m_url == nullptr) {
        std::unique_lock<std::mutex> lk(m_ui_mutex);
        m_frame = wxImage(m_idle_image);
        m_frame_is_default_bg = true;
        assert(m_frame.IsOk());
        Refresh();
    }
}

void wxMediaCtrl3::SetIdleImage(const wxImage &image, wxString const &watermark_text)
{
    if (!image.IsOk())
        return;
    m_idle_image.clear();
    m_watermark_text = watermark_text;
    if (m_url == nullptr) {
        std::unique_lock<std::mutex> lk(m_ui_mutex);
        m_frame = image;
        m_frame_is_default_bg = false;
        assert(m_frame.IsOk());
        Refresh();
    }
}

wxMediaState wxMediaCtrl3::GetState()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    return m_state;
}

int wxMediaCtrl3::GetLastError()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    return m_error;
}

wxSize wxMediaCtrl3::GetVideoSize()
{
    std::unique_lock<std::mutex> lk(m_mutex);
    return m_video_size;
}

wxSize wxMediaCtrl3::DoGetBestSize() const
{
    return {-1, -1};
}

static void adjust_frame_size(wxSize & frame, wxSize const & video, wxSize const & window)
{
    if (video.x <= 0 || video.y <= 0 || window.x <= 0 || window.y <= 0) { frame = wxDefaultSize; return; }
    if (video.x * window.y < video.y * window.x)
        frame = { video.x * window.y / video.y, window.y };
    else
        frame = { window.x, video.y * window.x / video.x };
}

void wxMediaCtrl3::paintEvent(wxPaintEvent &evt)
{
    wxPaintDC dc(this);
    auto      size = GetSize();
    if (size.x <= 0 || size.y <= 0)
        return;
    PlayFrame current_frame;
    bool      current_is_default_bg;
    {
        std::unique_lock<std::mutex> lk(m_ui_mutex);
        if (!m_frame.IsOk()) {
            return;
        }
        current_frame         = m_frame;
        current_is_default_bg = m_frame_is_default_bg;
    }
    wxSize frame_size;
    {
        std::unique_lock<std::mutex> lk(m_mutex);
        frame_size = m_frame_size;
    }
    auto size2 = current_frame.GetSize();
    if (size2.x != frame_size.x && size2.y == frame_size.y)
        size2.x = frame_size.x;

    // Treat the default background as idle content so the bg and its logo are always
    // drawn together, even if m_url flips to playing before the first video frame arrives.
    const bool is_idle = (m_url == nullptr) || current_is_default_bg;
    if (is_idle) {
        // Draw the static idle image through wxGraphicsContext with high-quality
        // interpolation. wxDC::SetUserScale + DrawBitmap uses GDI StretchBlt
        // (COLORONCOLOR, no interpolation) which aliases badly when scaling a
        // hi-res image down. Only wxGraphicsContext::DrawBitmap accepts an explicit
        // destination size, so compute an aspect-fit rectangle here.
        double dst_w = size.x;
        double dst_h = size.y;
        if (size2.x > 0 && size2.y > 0) {
            if (size.x * size2.y > size.y * size2.x) {
                dst_h = size.y;
                dst_w = double(size2.x) * size.y / size2.y;
            } else {
                dst_w = size.x;
                dst_h = double(size2.y) * size.x / size2.x;
            }
        }
        double dst_x = (size.x - dst_w) / 2;
        double dst_y = (size.y - dst_h) / 2;

        if (wxGraphicsContext *gc = wxGraphicsContext::Create(dc)) {
            gc->SetInterpolationQuality(wxINTERPOLATION_BEST);
            wxBitmap bmp(current_frame);
            gc->DrawBitmap(bmp, dst_x, dst_y, dst_w, dst_h);

            // Logo is bound to the default background frame so they appear/disappear together.
            if (current_is_default_bg) {
                DrawLiveviewLogo(gc, dst_x, dst_y, dst_w, dst_h);
            }
            delete gc;
        }
    } else {
        auto geometry = CameraGeometry();
        geometry.image_width = size2.x; geometry.image_height = size2.y; geometry.clamp();
        m_camera_view = geometry.view;
        const auto destination = geometry.rect();
        if (destination.width > 0 && destination.height > 0) {
            if (wxGraphicsContext *gc = wxGraphicsContext::Create(dc)) {
                gc->SetInterpolationQuality(wxINTERPOLATION_BEST);
                gc->Clip(0, 0, size.x, size.y);
                gc->DrawBitmap(wxBitmap(current_frame), destination.x, destination.y, destination.width, destination.height);
                delete gc;
            }
        }
    }

    // Draw watermark overlay when showing device preview image
    if (!m_watermark_text.empty() && is_idle) {
        // Reset user scale for watermark (draw at 1:1)
        dc.SetUserScale(1.0, 1.0);

        wxString watermark_text = m_watermark_text;

        // Setup font
        wxFont font = dc.GetFont();
        font.SetPointSize(10);
        font.SetWeight(wxFONTWEIGHT_BOLD);
        dc.SetFont(font);
        wxSize text_size = dc.GetTextExtent(watermark_text);

        // Calculate watermark rectangle with padding
        int pad_h = FromDIP(12);
        int pad_v = FromDIP(8);
        int wm_w = text_size.GetWidth() + 2 * pad_h;
        int wm_h = text_size.GetHeight() + 2 * pad_v;
        int wm_x = (size.GetWidth() - wm_w) / 2;
        int wm_y = size.GetHeight() - wm_h - FromDIP(10);
        int radius = 8;

        // Use wxGCDC for alpha-blended rounded rectangle
        wxGCDC gcdc(dc);
        gcdc.SetBrush(wxBrush(wxColour(51, 51, 51, 160)));
        gcdc.SetPen(*wxTRANSPARENT_PEN);
        gcdc.DrawRoundedRectangle(wm_x, wm_y, wm_w, wm_h, radius);

        // Draw text centered in the rectangle
        gcdc.SetTextForeground(wxColour(220, 220, 220));
        gcdc.SetFont(font);
        int tx = wm_x + (wm_w - text_size.GetWidth()) / 2;
        int ty = wm_y + (wm_h - text_size.GetHeight()) / 2;
        gcdc.DrawText(watermark_text, tx, ty);
    }
}

void wxMediaCtrl3::DrawLiveviewLogo(wxGraphicsContext *gc, double bg_x, double bg_y, double bg_w, double bg_h)
{
    if (gc == nullptr || bg_w <= 0 || bg_h <= 0)
        return;

    // Logo is 448x128 centered in the original 1920x1080 artwork.
    constexpr double logo_aspect      = 448.0 / 128.0;
    constexpr double logo_width_ratio = 448.0 / 1920.0;
    const double     logo_w           = bg_w * logo_width_ratio;
    const double     logo_h           = logo_w / logo_aspect;
    const double     logo_x           = bg_x + (bg_w - logo_w) / 2.0;
    const double     logo_y           = bg_y + (bg_h - logo_h) / 2.0;

    // Rasterize at exact pixel size (cached) to avoid aliasing.
    const wxSize target_px(std::lround(logo_w), std::lround(logo_h));
    if (target_px.x <= 0 || target_px.y <= 0)
        return;

    if (!m_logo_bitmap.IsOk() || m_logo_bitmap_size != target_px) {
        const std::string logo_path = Slic3r::resources_dir() + "/images/liveview_logo.svg";
        NSVGimage *image = ::nsvgParseFromFile(logo_path.c_str(), "px", 96.0f);
        if (image == nullptr)
            return;

        const float scale = (image->height > 0.f) ? float(target_px.y) / image->height : 1.f;
        NSVGrasterizer *rast = ::nsvgCreateRasterizer();
        if (rast == nullptr) {
            ::nsvgDelete(image);
            return;
        }
        std::vector<unsigned char> rgba(size_t(target_px.x) * target_px.y * 4, 0);
        ::nsvgRasterize(rast, image, 0, 0, scale, rgba.data(), target_px.x, target_px.y, target_px.x * 4);
        ::nsvgDeleteRasterizer(rast);
        ::nsvgDelete(image);

        // NanoSVG emits non-premultiplied RGBA; split into wxImage RGB + alpha.
        wxImage wx_image(target_px.x, target_px.y);
        wx_image.InitAlpha();
        unsigned char *rgb   = wx_image.GetData();
        unsigned char *alpha = wx_image.GetAlpha();
        for (int i = 0; i < target_px.x * target_px.y; ++i) {
            rgb[i * 3 + 0] = rgba[i * 4 + 0];
            rgb[i * 3 + 1] = rgba[i * 4 + 1];
            rgb[i * 3 + 2] = rgba[i * 4 + 2];
            alpha[i]       = rgba[i * 4 + 3];
        }
        m_logo_bitmap      = wxBitmap(wx_image);
        m_logo_bitmap_size = target_px;
    }

    if (m_logo_bitmap.IsOk())
        gc->DrawBitmap(m_logo_bitmap, logo_x, logo_y, logo_w, logo_h);
}

void wxMediaCtrl3::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    wxWindow::DoSetSize(x, y, width, height, sizeFlags);
    if (sizeFlags == wxSIZE_USE_EXISTING) return;
    wxMediaCtrl_OnSize(this, m_video_size, width, height);
    std::unique_lock<std::mutex> lk(m_mutex);
    adjust_frame_size(m_frame_size, m_video_size, GetSize());
    lk.unlock();
    SetCameraView(m_camera_view);
    Refresh();
}

void wxMediaCtrl3::bambu_log(void *ctx, int level, tchar const *msg2)
{
#ifdef _WIN32
    wxString msg(msg2);
#else
    wxString msg = wxString::FromUTF8(msg2);
#endif
    if (level == 1) {
        if (msg.EndsWith("]")) {
            int n = msg.find_last_of('[');
            if (n != wxString::npos) {
                long val = 0;
                wxMediaCtrl3 *ctrl = (wxMediaCtrl3 *) ctx;
                if (msg.SubString(n + 1, msg.Length() - 2).ToLong(&val)) {
                    std::unique_lock<std::mutex> lk(ctrl->m_mutex);
                    ctrl->m_error = (int) val;
                }
            }
        } else if (msg.Contains("stat_log")) {
            wxCommandEvent evt(EVT_MEDIA_CTRL_STAT);
            wxMediaCtrl3 *ctrl = (wxMediaCtrl3 *) ctx;
            evt.SetEventObject(ctrl);
            evt.SetString(msg.Mid(msg.Find(' ') + 1));
            wxPostEvent(ctrl, evt);
        }
    }
    BOOST_LOG_TRIVIAL(info) << msg.ToUTF8().data();
}

void wxMediaCtrl3::bambu_streaminfo(void *ctx, Bambu_StreamInfo *info)
{
    if (ctx == nullptr || info == nullptr)
        return;
    wxMediaCtrl3 *self = static_cast<wxMediaCtrl3 *>(ctx);
    {
        std::lock_guard<std::mutex> lk(self->m_pending_stream.mutex);
        self->m_pending_stream.info = *info;
    }
    self->m_pending_stream.changed.store(true);
    BOOST_LOG_TRIVIAL(info) << "wxMediaCtrl3: stream info changed, video "
                            << info->format.video.width << "x" << info->format.video.height
                            << " fps=" << info->format.video.frame_rate
                            << " sub_type=" << info->sub_type;
}

void wxMediaCtrl3::on_player_track_event(void *ctx, const PlayerEventC *event)
{
    if (event == nullptr || event->event_name == nullptr) {
        return;
    }

    auto *channel =
        static_cast<const BambuLiveViewTrack::ChannelInfo *>(ctx);

    BambuLiveViewTrack::EmitParams params;
    if (event->module)        params.module        = event->module;
    if (event->phase)         params.phase         = event->phase;
    if (event->result)        params.result        = event->result;
    if (event->error_code)    params.error_code    = event->error_code;
    if (event->error_message) params.error_message = event->error_message;
    if (event->event_data_body) {
        params.event_data = nlohmann::json::parse(event->event_data_body, nullptr, false);
        if (params.event_data.is_discarded())
            params.event_data = nlohmann::json::object();
    }

    BambuLiveViewTrack::LiveViewTrackContext::instance()
        .emit(event->event_name, params, channel);
}

void wxMediaCtrl3::UpdateSessionStat()
{
    auto t = m_tunnel.load();
    if (t && Bambu_GetSessionStat)
        Bambu_GetSessionStat(t, &m_session_stat);
}

void wxMediaCtrl3::SetTrackChannel(const BambuLiveViewTrack::ChannelInfo& info)
{
    m_track_channel = info;
}

void wxMediaCtrl3::PlayThread()
{
    using namespace std::chrono_literals;
    std::shared_ptr<wxURI> url;
    const int decode_warn_thres = 33;
    std::unique_lock<std::mutex> lk(m_mutex);

    //frame count
    int                                                frameCount = 0;
    std::chrono::time_point<std::chrono::system_clock> lastSecondTime;

    while (true) {
        m_cond.wait(lk, [this, &url] { return m_url != url; });
        url = m_url;
        if (url == nullptr)
            continue;
        if (!url->HasScheme())
            break;


        //reset frame
        frameCount     = 0;
        lastSecondTime = std::chrono::system_clock::now();
        m_first_frame_info = FirstFrameInfo{};
        m_video_decode_error_count = 0;
        m_render_error_count       = 0;

        m_pending_stream.changed.store(false);

        lk.unlock();

        Bambu_Tunnel tunnel = nullptr;
        auto t0 = std::chrono::steady_clock::now();
        int error = Bambu_Create(&tunnel, m_url->BuildURI().ToUTF8());
        if (error == 0) m_tunnel.store(tunnel);
        if (error == 0) {
            Bambu_SetLogger(tunnel, &wxMediaCtrl3::bambu_log, this);
            // Older BambuSource dylibs do not export Bambu_SetStreamInfoCallback;
            // skip subscribing in that case so a stale plugin still plays video
            // (only loses dynamic resolution adjustment on camera switch).
            if (Bambu_SetStreamInfoCallback)
                Bambu_SetStreamInfoCallback(tunnel, &wxMediaCtrl3::bambu_streaminfo, this);
            else
                BOOST_LOG_TRIVIAL(warning) << "wxMediaCtrl3: Bambu_SetStreamInfoCallback not available, dynamic stream-info changes will not be observed";
            if (Bambu_SetTrackReporter) {
                Bambu_SetTrackReporter(tunnel,
                                       &wxMediaCtrl3::on_player_track_event,
                                       &m_track_channel);
            }
            error = Bambu_Open(tunnel);
            auto t1 = std::chrono::steady_clock::now();
            BOOST_LOG_TRIVIAL(info) << "wxMediaCtrl3: Bambu_Open took "
                                    << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count() << "ms, error=" << error;
            if (error == 0)
                error = Bambu_would_block;

            else if (error == -2)
            {
                m_error = error;
                BOOST_LOG_TRIVIAL(info) << "MediaPlayCtrl::DLL load error ";
                lk.lock();
                NotifyStopped();
                continue;
            }
        }
        lk.lock();
        auto t_stream_start = std::chrono::steady_clock::now();
        while (error == int(Bambu_would_block)) {
            m_cond.wait_for(lk, 100ms);
            if (m_url != url) {
                error = 1;
                break;
            }
            lk.unlock();
            error = Bambu_StartStream(tunnel, true);
            lk.lock();
        }
        {
            auto t_stream_end = std::chrono::steady_clock::now();
            BOOST_LOG_TRIVIAL(info) << "wxMediaCtrl3: Bambu_StartStream loop took "
                                    << std::chrono::duration_cast<std::chrono::milliseconds>(t_stream_end - t_stream_start).count()
                                    << "ms, error=" << error;
        }
        Bambu_StreamInfo info;
        if (error == 0)
            error = Bambu_GetStreamInfo(tunnel, 0, &info);
        AVVideoDecoder decoder;
        if (error == 0) {
            decoder.open(info);
            m_video_size = { info.format.video.width, info.format.video.height };
            m_first_frame_info.video_codec      = info.sub_type;
            m_first_frame_info.resolution_width  = info.format.video.width;
            m_first_frame_info.resolution_height = info.format.video.height;
            adjust_frame_size(m_frame_size, m_video_size, GetSize());
            NotifyStopped();
            size_t buffer_cap = (size_t) (m_buffer_time * info.format.video.frame_rate / 1000);
            if (buffer_cap == 0) {
                buffer_cap = 1;
            }
            m_frame_buffer.set_capacity(buffer_cap);
            m_get_frame_exit.store(false);
            m_get_frame_thread = std::thread(&wxMediaCtrl3::GetFrameThread, this, info.format.video.frame_rate);
            m_need_refresh.store(false);
            m_render_timer.Start(1000 / (info.format.video.frame_rate + 5));
        }
        Bambu_Sample sample;
        while (error == 0) {
            lk.unlock();
            error = Bambu_ReadSample(tunnel, &sample);
            lk.lock();
            while (error == int(Bambu_would_block)) {
                m_cond.wait_for(lk, 10ms);
                if (m_url != url) {
                    error = 1;
                    break;
                }
                lk.unlock();
                error = Bambu_ReadSample(tunnel, &sample);
                lk.lock();
            }
            if (error == 0) {
                auto frame_size = m_frame_size;
                static thread_local int post_reopen_samples = -1;
                static thread_local std::chrono::steady_clock::time_point reopen_done_time;
                if (m_pending_stream.changed.exchange(false)) {
                    Bambu_StreamInfo new_info;
                    {
                        std::lock_guard<std::mutex> psk(m_pending_stream.mutex);
                        new_info = m_pending_stream.info;
                    }
                    lk.unlock();
                    BOOST_LOG_TRIVIAL(info) << "wxMediaCtrl3: reopening decoder for stream change "
                                            << new_info.format.video.width << "x" << new_info.format.video.height
                                            << " sub_type=" << new_info.sub_type;
                    auto reopen_t0 = std::chrono::steady_clock::now();
                    int reopen_ret = decoder.reopen(new_info);
                    reopen_done_time = std::chrono::steady_clock::now();
                    BOOST_LOG_TRIVIAL(info) << "wxMediaCtrl3: reopen done ret=" << reopen_ret
                                            << " took=" << std::chrono::duration_cast<std::chrono::milliseconds>(reopen_done_time - reopen_t0).count() << "ms";
                    post_reopen_samples = 0;
                    lk.lock();
                    m_video_size = { new_info.format.video.width, new_info.format.video.height };
                    adjust_frame_size(m_frame_size, m_video_size, GetSize());
                    frame_size = m_frame_size;
                }
                lk.unlock();
                if (m_first_frame_info.first_packet_time_ms == 0) {
                    m_first_frame_info.first_packet_time_ms =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                }
                bool is_idr = (sample.flags & f_sync) != 0;
                int sample_size = sample.size;
                uint32_t buf_head = 0;
                if (sample.buffer && sample.size >= 5) {
                    buf_head = (uint32_t(sample.buffer[0]) << 24) |
                               (uint32_t(sample.buffer[1]) << 16) |
                               (uint32_t(sample.buffer[2]) <<  8) |
                                uint32_t(sample.buffer[3]);
                }
                uint8_t first_nal_type = (sample.buffer && sample.size >= 5)
                                             ? (sample.buffer[4] & 0x1f) : 0;
                PlayFrame bm;
                auto start_decode = std::chrono::steady_clock::now();
                int decode_ret = decoder.decode(sample);
                if (decode_ret != 0) ++m_video_decode_error_count;
                auto end_decode = std::chrono::steady_clock::now();
                bool got_frame = decoder.got_frame();
#ifdef _WIN32
                decoder.toWxBitmap(bm, frame_size);
#else
                decoder.toWxImage(bm, frame_size);
#endif
                auto end_convert = std::chrono::steady_clock::now();
                if (post_reopen_samples >= 0 && post_reopen_samples < 8) {
                    int since_reopen_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_decode - reopen_done_time).count();
                    BOOST_LOG_TRIVIAL(info)
                        << "wxMediaCtrl3: post-reopen sample #" << post_reopen_samples
                        << " is_idr=" << is_idr
                        << " size=" << sample_size
                        << " head=0x" << std::hex << buf_head << std::dec
                        << " nal_type=" << int(first_nal_type)
                        << " decode_ret=" << decode_ret
                        << " got_frame=" << got_frame
                        << " bm_ok=" << bm.IsOk()
                        << " since_reopen=" << since_reopen_ms << "ms";
                    if (post_reopen_samples == 0 && !is_idr) {
                        BOOST_LOG_TRIVIAL(warning)
                            << "wxMediaCtrl3: post-reopen first sample is NOT IDR, decoder will likely drop frames until next IDR";
                    }
                    ++post_reopen_samples;
                    if (got_frame && bm.IsOk()) {
                        BOOST_LOG_TRIVIAL(info)
                            << "wxMediaCtrl3: post-reopen first decoded frame after "
                            << std::chrono::duration_cast<std::chrono::milliseconds>(end_decode - reopen_done_time).count()
                            << "ms (samples consumed=" << post_reopen_samples << ")";
                        post_reopen_samples = -1;
                    } else if (post_reopen_samples >= 8) {
                        BOOST_LOG_TRIVIAL(warning)
                            << "wxMediaCtrl3: post-reopen still no frame after 8 samples, giving up tracking";
                        post_reopen_samples = -1;
                    }
                }
                int elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_convert - start_decode).count();
                if (elapsed_ms > decode_warn_thres) {
                    BOOST_LOG_TRIVIAL(warning) << "wxMediaCtrl3: decode + convert too long, decode: "
                                               << std::chrono::duration_cast<std::chrono::milliseconds>(end_decode - start_decode).count()
                                               << " convert: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_convert - end_decode).count();
                }
                lk.lock();
                if (m_url != url) {
                    error = 1;
                    break;
                }
                if (bm.IsOk() && m_first_frame_info.decode_first_frame_time_ms == 0) {
                    m_first_frame_info.decode_first_frame_time_ms =
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                }
                if (bm.IsOk()) {
                    auto now = std::chrono::system_clock::now();
                    frameCount++;
                    auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSecondTime).count();
                    if (elapsedTime >= 10000) {
                        int fps = static_cast<int>(frameCount * 1000 / elapsedTime); // 100 is from frameCount * 1000 / elapsedTime * 10 , becasue  calculate the average rate over 10s
                        BOOST_LOG_TRIVIAL(info) << "wxMediaCtrl3:Decode Real Rate: " << fps << " FPS";
                        frameCount     = 0;
                        lastSecondTime = now;
                    }

                    m_frame_buffer.enqueue(bm);
                } else if (decode_ret == 0) {
                    ++m_render_error_count;
                }
            }
        }
        if (m_get_frame_thread.joinable()) {
            m_get_frame_exit.store(true);
            m_get_frame_thread.join();
        }
        m_frame_buffer.reset();
        if (tunnel) {
            lk.unlock();
            if (Bambu_GetSessionStat)
                Bambu_GetSessionStat(tunnel, &m_session_stat);
            auto t_close_start = std::chrono::steady_clock::now();
            Bambu_Close(tunnel);
            Bambu_Destroy(tunnel);
            auto t_close_end = std::chrono::steady_clock::now();
            auto close_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_close_end - t_close_start).count();
            if (close_ms > 3000) {
                BOOST_LOG_TRIVIAL(warning) << "wxMediaCtrl3: Bambu_Close+Destroy took " << close_ms << "ms (>3s, potential hang source)";
            } else {
                BOOST_LOG_TRIVIAL(info) << "wxMediaCtrl3: Bambu_Close+Destroy took " << close_ms << "ms";
            }
            tunnel = nullptr;
            m_tunnel.store(nullptr);
            lk.lock();
        }
        m_render_timer.Stop();
        if (m_url == url)
            m_error = error;
        m_frame_size = wxDefaultSize;
        m_video_size = wxDefaultSize;
        {
            Bambu_SessionStat stat = m_session_stat;
            CallAfter([this, stat] {
                wxCommandEvent evt(EVT_MEDIA_CTRL_SESSION_END);
                evt.SetEventObject(this);
                evt.SetClientData(new Bambu_SessionStat(stat));
                wxPostEvent(this, evt);
            });
        }
        NotifyStopped();
    }
}

void wxMediaCtrl3::NotifyStopped()
{
    m_state = wxMEDIASTATE_STOPPED;
    wxMediaEvent event(wxEVT_MEDIA_STATECHANGED);
    event.SetId(GetId());
    event.SetEventObject(this);
    wxPostEvent(this, event);
}

void wxMediaCtrl3::GetFrameThread(int frame_rate)
{
    PlayFrame temp_frame;
    long long frame_count = 0;
    bool pop_success = false;
    bool first_frame_fired = false;
    std::chrono::system_clock::time_point first_frame_time;
    while (m_get_frame_exit.load() == false) {
        if (m_frame_buffer.try_dequeue(temp_frame) == true) {
            if (!temp_frame.IsOk()) {
                continue;
            }
            {
                std::unique_lock<std::mutex> lk(m_ui_mutex);
                m_frame = temp_frame;
                m_frame_is_default_bg = false;
                m_need_refresh.store(true);
            }
            if (pop_success == false) {
                first_frame_time = std::chrono::system_clock::now();
                pop_success = true;
                frame_count = 0;
                if (!first_frame_fired) {
                    first_frame_fired = true;
                    auto play_start = m_play_start_time;
                    if (play_start != std::chrono::system_clock::time_point{}) {
                        int ms = (int) std::chrono::duration_cast<std::chrono::milliseconds>(first_frame_time - play_start).count();
                        m_first_frame_info.first_frame_cost_ms        = ms;
                        m_first_frame_info.render_first_frame_time_ms =
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                first_frame_time.time_since_epoch()).count();
                        FirstFrameInfo info = m_first_frame_info;
                        CallAfter([this, info] {
                            wxCommandEvent evt(EVT_MEDIA_CTRL_FIRST_FRAME);
                            evt.SetEventObject(this);
                            evt.SetInt(info.first_frame_cost_ms);
                            evt.SetClientData(new FirstFrameInfo(info));
                            wxPostEvent(this, evt);
                        });
                    }
                }
            }
            ++frame_count;
            long long  pts_gap = (frame_count * 1000) / frame_rate;
            auto wake_up_time = first_frame_time + std::chrono::milliseconds(pts_gap);
            std::this_thread::sleep_until(wake_up_time);
        } else {
            if (pop_success == true) {
                BOOST_LOG_TRIVIAL(info) << "wxMediaCtrl3:decode too slow or unsteady network, bitmap buffer running out...";
                pop_success = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    {
        std::unique_lock<std::mutex> lk(m_ui_mutex);
        m_frame = wxImage(m_idle_image);
        m_frame_is_default_bg = !m_idle_image.empty();
        m_need_refresh.store(true);
        CallAfter([this] { Refresh(false); });
    }
}

void wxMediaCtrl3::OnRenderTimer(wxTimerEvent &evt)
{
    if (m_need_refresh.load() == true) {
        Refresh(false);
        Update();
        m_need_refresh.store(false);
    }
}
