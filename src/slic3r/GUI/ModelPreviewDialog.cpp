#include <GL/glew.h>

#include "ModelPreviewDialog.hpp"

#include "I18N.hpp"
#include "GUI_App.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/StateColor.hpp"

#include "libslic3r/Model.hpp"
#include "libslic3r/TriangleMesh.hpp"

#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include <boost/log/trivial.hpp>

namespace Slic3r { namespace GUI {

// A stable value for M_PI without depending on <cmath> platform macros.
static constexpr float MODEL_PREVIEW_PI = 3.14159265358979323846f;

static bool is_dark() { return wxGetApp().dark_mode(); }

// On macOS the GL framebuffer is sized in physical pixels; elsewhere the
// logical client size already matches the framebuffer.
static wxSize gl_viewport_size(wxWindow* win, const wxSize& logical_size)
{
    wxSize viewport_size = logical_size;
#ifdef __APPLE__
    const double scale = win ? win->GetContentScaleFactor() : 1.0;
    if (scale > 0.0) {
        viewport_size.x = std::max(1, (int) std::round(viewport_size.x * scale));
        viewport_size.y = std::max(1, (int) std::round(viewport_size.y * scale));
    }
#else
    (void) win;
#endif
    return viewport_size;
}

// ============================================================
// ModelPreviewCanvas
// ============================================================

ModelPreviewCanvas::ModelPreviewCanvas(wxWindow* parent, const wxGLAttributes& attrs)
    : wxGLCanvas(parent, attrs, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
{
    m_context = new wxGLContext(this);

    Bind(wxEVT_PAINT, &ModelPreviewCanvas::on_paint, this);
    Bind(wxEVT_SIZE,  &ModelPreviewCanvas::on_size,  this);
    Bind(wxEVT_MOUSEWHEEL,  &ModelPreviewCanvas::on_mouse, this);
    Bind(wxEVT_LEFT_DOWN,   &ModelPreviewCanvas::on_mouse, this);
    Bind(wxEVT_LEFT_UP,     &ModelPreviewCanvas::on_mouse, this);
    Bind(wxEVT_RIGHT_DOWN,  &ModelPreviewCanvas::on_mouse, this);
    Bind(wxEVT_RIGHT_UP,    &ModelPreviewCanvas::on_mouse, this);
    Bind(wxEVT_MIDDLE_DOWN, &ModelPreviewCanvas::on_mouse, this);
    Bind(wxEVT_MIDDLE_UP,   &ModelPreviewCanvas::on_mouse, this);
    Bind(wxEVT_MOTION,      &ModelPreviewCanvas::on_mouse, this);
    Bind(wxEVT_LEAVE_WINDOW, &ModelPreviewCanvas::on_mouse, this);
    // Double-click anywhere re-frames the model (fit-to-view).
    Bind(wxEVT_LEFT_DCLICK, [this](wxMouseEvent&) { reset_view(); });
}

ModelPreviewCanvas::~ModelPreviewCanvas()
{
    // No GL objects to release (immediate-mode rendering only), so the context
    // does not need to be made current here.
    delete m_context;
}

void ModelPreviewCanvas::set_mesh_data(
    const std::vector<std::array<float, 3>>& vertices,
    const std::vector<std::array<int, 3>>&   indices)
{
    m_vertices = vertices;
    m_indices  = indices;
    update_bounding_box();
    compute_smooth_normals();
    Refresh();
}

void ModelPreviewCanvas::compute_smooth_normals()
{
    m_vertex_normals.clear();
    if (m_vertices.empty() || m_indices.empty()) return;

    m_vertex_normals.resize(m_vertices.size(), {0.f, 0.f, 0.f});

    for (const auto& face : m_indices) {
        int i0 = face[0], i1 = face[1], i2 = face[2];
        if (i0 < 0 || i0 >= (int) m_vertices.size() ||
            i1 < 0 || i1 >= (int) m_vertices.size() ||
            i2 < 0 || i2 >= (int) m_vertices.size())
            continue;

        const auto& v0 = m_vertices[i0];
        const auto& v1 = m_vertices[i1];
        const auto& v2 = m_vertices[i2];

        float nx = (v1[1]-v0[1])*(v2[2]-v0[2]) - (v1[2]-v0[2])*(v2[1]-v0[1]);
        float ny = (v1[2]-v0[2])*(v2[0]-v0[0]) - (v1[0]-v0[0])*(v2[2]-v0[2]);
        float nz = (v1[0]-v0[0])*(v2[1]-v0[1]) - (v1[1]-v0[1])*(v2[0]-v0[0]);

        m_vertex_normals[i0][0] += nx; m_vertex_normals[i0][1] += ny; m_vertex_normals[i0][2] += nz;
        m_vertex_normals[i1][0] += nx; m_vertex_normals[i1][1] += ny; m_vertex_normals[i1][2] += nz;
        m_vertex_normals[i2][0] += nx; m_vertex_normals[i2][1] += ny; m_vertex_normals[i2][2] += nz;
    }

    for (auto& n : m_vertex_normals) {
        float len = std::sqrt(n[0]*n[0] + n[1]*n[1] + n[2]*n[2]);
        if (len > 1e-8f) { n[0] /= len; n[1] /= len; n[2] /= len; }
    }
}

void ModelPreviewCanvas::update_bounding_box()
{
    if (m_vertices.empty()) return;
    std::array<float, 3> mn = m_vertices[0], mx = m_vertices[0];
    for (const auto& v : m_vertices) {
        for (int i = 0; i < 3; ++i) {
            mn[i] = std::min(mn[i], v[i]);
            mx[i] = std::max(mx[i], v[i]);
        }
    }
    m_center = { (mn[0]+mx[0])/2, (mn[1]+mx[1])/2, (mn[2]+mx[2])/2 };
    float dx = mx[0]-mn[0], dy = mx[1]-mn[1], dz = mx[2]-mn[2];
    m_radius = std::sqrt(dx*dx + dy*dy + dz*dz) / 2.0f;
    if (m_radius < 1e-6f) m_radius = 1.0f;
}

void ModelPreviewCanvas::reset_view()
{
    m_zoom  = 1.0f;
    m_rot_x = -30.0f;
    m_rot_y = 30.0f;
    m_pan_x = 0.0f;
    m_pan_y = 0.0f;
    Refresh();
}

void ModelPreviewCanvas::ensure_gl_ready()
{
    if (m_gl_initialized) return;

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        BOOST_LOG_TRIVIAL(error) << "ModelPreviewCanvas: glewInit failed: "
                                 << glewGetErrorString(err);
        return;
    }
    while (glGetError() != GL_NO_ERROR) {}

    m_gl_initialized = true;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    GLfloat light_pos[]     = { 0.5f, 1.0f, 1.0f, 0.0f };
    GLfloat light_ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat light_diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT,  light_ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE,  light_diffuse);
}

void ModelPreviewCanvas::on_paint(wxPaintEvent&)
{
    wxPaintDC dc(this);
    if (!m_context) return;
    SetCurrent(*m_context);
    ensure_gl_ready();
    render();
    SwapBuffers();
}

void ModelPreviewCanvas::on_size(wxSizeEvent&)
{
    Refresh();
}

void ModelPreviewCanvas::on_mouse(wxMouseEvent& evt)
{
    if (evt.LeftDown()) {
        m_drag_mode = DragMode::Rotate;
        m_last_mouse_pos = evt.GetPosition();
        if (!HasCapture()) CaptureMouse();
    }
    else if (evt.LeftUp()) {
        if (m_drag_mode == DragMode::Rotate) {
            m_drag_mode = DragMode::None;
            if (HasCapture()) ReleaseMouse();
        }
    }
    else if (evt.RightDown() || evt.MiddleDown()) {
        m_drag_mode = DragMode::Pan;
        m_last_mouse_pos = evt.GetPosition();
        if (!HasCapture()) CaptureMouse();
    }
    else if (evt.RightUp() || evt.MiddleUp()) {
        if (m_drag_mode == DragMode::Pan) {
            m_drag_mode = DragMode::None;
            if (HasCapture()) ReleaseMouse();
        }
    }
    else if (evt.Dragging() && m_drag_mode != DragMode::None) {
        wxPoint pos = evt.GetPosition();
        float dx = (float)(pos.x - m_last_mouse_pos.x);
        float dy = (float)(pos.y - m_last_mouse_pos.y);

        if (m_drag_mode == DragMode::Rotate) {
            m_rot_y += dx * 0.5f;
            m_rot_x += dy * 0.5f;
            m_rot_x = std::max(-89.0f, std::min(89.0f, m_rot_x));
        } else if (m_drag_mode == DragMode::Pan) {
            wxSize sz = GetClientSize();
            if (sz.x > 0)
                m_pan_x += dx / (float)sz.x * m_radius * 2.0f / m_zoom;
            if (sz.y > 0)
                m_pan_y -= dy / (float)sz.y * m_radius * 2.0f / m_zoom;
        }

        m_last_mouse_pos = pos;
        Refresh();
    }
    else if (evt.GetWheelRotation() != 0) {
        float delta = evt.GetWheelRotation() > 0 ? 1.1f : 0.9f;
        m_zoom *= delta;
        m_zoom = std::max(0.1f, std::min(20.0f, m_zoom));
        Refresh();
    }
}

void ModelPreviewCanvas::render()
{
    wxSize sz = GetClientSize();
    if (sz.x <= 0 || sz.y <= 0) return;

    wxSize viewport_sz = gl_viewport_size(this, sz);
    glViewport(0, 0, viewport_sz.x, viewport_sz.y);

    const bool     dark = is_dark();
    const wxColour bg   = MD3::resolve(MD3::Role::SurfaceContainer, dark);
    glClearColor(bg.Red() / 255.f, bg.Green() / 255.f, bg.Blue() / 255.f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect     = (float) viewport_sz.x / (float) viewport_sz.y;
    float dist       = m_radius * 3.0f / m_zoom;
    float near_plane = dist * 0.01f;
    float far_plane  = dist * 10.0f;
    float fov_rad    = 45.0f * MODEL_PREVIEW_PI / 180.0f;
    float f          = 1.0f / std::tan(fov_rad / 2.0f);
    float proj[16]   = {};
    proj[0]  = f / aspect;
    proj[5]  = f;
    proj[10] = (far_plane + near_plane) / (near_plane - far_plane);
    proj[11] = -1.0f;
    proj[14] = (2.0f * far_plane * near_plane) / (near_plane - far_plane);
    glMultMatrixf(proj);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -dist);
    glTranslatef(m_pan_x, m_pan_y, 0.0f);
    glRotatef(m_rot_x, 1.0f, 0.0f, 0.0f);
    glRotatef(m_rot_y, 0.0f, 1.0f, 0.0f);
    glTranslatef(-m_center[0], -m_center[1], -m_center[2]);

    render_mesh();
}

void ModelPreviewCanvas::render_mesh()
{
    if (m_vertices.empty() || m_indices.empty()) return;

    const bool has_smooth = (m_vertex_normals.size() == m_vertices.size());

    // Neutral plastic material — a faithful model preview, not UI chrome, so it
    // deliberately stays a data colour rather than an MD3 role. The tone is
    // lifted in dark mode for contrast against the dark viewport clear.
    const bool dark = is_dark();
    if (dark)
        glColor3f(0.80f, 0.81f, 0.85f);
    else
        glColor3f(0.70f, 0.71f, 0.76f);

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);

    glBegin(GL_TRIANGLES);
    for (const auto& face : m_indices) {
        for (int vi = 0; vi < 3; ++vi) {
            int idx = face[vi];
            if (idx < 0 || idx >= (int) m_vertices.size()) continue;

            if (has_smooth) {
                glNormal3fv(m_vertex_normals[idx].data());
            } else if (vi == 0) {
                const auto& v0 = m_vertices[face[0]];
                const auto& v1 = m_vertices[face[1]];
                const auto& v2 = m_vertices[face[2]];
                float nx = (v1[1]-v0[1])*(v2[2]-v0[2]) - (v1[2]-v0[2])*(v2[1]-v0[1]);
                float ny = (v1[2]-v0[2])*(v2[0]-v0[0]) - (v1[0]-v0[0])*(v2[2]-v0[2]);
                float nz = (v1[0]-v0[0])*(v2[1]-v0[1]) - (v1[1]-v0[1])*(v2[0]-v0[0]);
                float len = std::sqrt(nx*nx + ny*ny + nz*nz);
                if (len > 1e-8f) { nx /= len; ny /= len; nz /= len; }
                glNormal3f(nx, ny, nz);
            }

            glVertex3fv(m_vertices[idx].data());
        }
    }
    glEnd();
}

// ============================================================
// ModelPreviewDialog
// ============================================================

bool ModelPreviewDialog::load_geometry(
    const std::string&                 path,
    std::vector<std::array<float, 3>>& vertices,
    std::vector<std::array<int, 3>>&   indices)
{
    vertices.clear();
    indices.clear();

    Model model;
    try {
        model = Model::read_from_file(path, nullptr, nullptr,
                    LoadStrategy::AddDefaultInstances | LoadStrategy::LoadModel);
    }
    catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(warning) << "ModelPreviewDialog::load_geometry failed: " << e.what();
        return false;
    }
    catch (...) {
        BOOST_LOG_TRIVIAL(warning) << "ModelPreviewDialog::load_geometry failed: unknown error";
        return false;
    }

    TriangleMesh mesh = model.mesh();
    const indexed_triangle_set& its = mesh.its;
    if (its.vertices.empty() || its.indices.empty())
        return false;

    vertices.reserve(its.vertices.size());
    for (const auto& v : its.vertices)
        vertices.push_back({ v.x(), v.y(), v.z() });

    indices.reserve(its.indices.size());
    for (const auto& fidx : its.indices)
        indices.push_back({ fidx[0], fidx[1], fidx[2] });

    return true;
}

ModelPreviewDialog::ModelPreviewDialog(
    wxWindow*                                 parent,
    const wxString&                           model_name,
    const std::vector<std::array<float, 3>>&  vertices,
    const std::vector<std::array<int, 3>>&    indices)
    : DPIDialog(parent, wxID_ANY, _L("Preview model"),
                wxDefaultPosition, wxDefaultSize,
                (wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) & ~(wxMINIMIZE_BOX | wxMAXIMIZE_BOX))
{
    SetSize(wxSize(FromDIP(680), FromDIP(560)));

    build_ui(model_name);

    SetMinSize(wxSize(FromDIP(480), FromDIP(420)));
    CenterOnParent();
    wxGetApp().UpdateDlgDarkUI(this);

    if (m_canvas)
        m_canvas->set_mesh_data(vertices, indices);
}

void ModelPreviewDialog::build_ui(const wxString& model_name)
{
    const wxColour dialog_bg = StateColor::semantic(MD3::Role::SurfaceContainerLowest);
    SetBackgroundColour(dialog_bg);
    SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));

    wxBoxSizer* root_sizer = new wxBoxSizer(wxVERTICAL);

    // Top hairline — the MD3 dialog top divider.
    auto* line_top = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
    line_top->SetBackgroundColour(StateColor::semantic(MD3::Role::OutlineVariant));
    root_sizer->Add(line_top, 0, wxEXPAND);

    // ---- Header: dialog title 18/600 + model name supporting text ----
    wxBoxSizer* header_sizer = new wxBoxSizer(wxVERTICAL);

    auto* title = new Label(this, Label::Head_18, _L("Preview model"));
    title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    header_sizer->Add(title, 0, wxALIGN_LEFT);

    if (!model_name.empty()) {
        auto* name = new Label(this, Label::Body_14, model_name);
        name->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        header_sizer->Add(name, 0, wxALIGN_LEFT | wxTOP, FromDIP(2));
    }

    root_sizer->Add(header_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(20));

    // ---- Preview: hero card panel (radius_dialog scaled), GL canvas inside ----
    const wxColour preview_bg = StateColor::semantic(MD3::Role::SurfaceContainer);
    const wxColour preview_bd = StateColor::semantic(MD3::Role::OutlineVariant);
    const int      card_radius = FromDIP(MD3::Metrics::radius_rail); // 12 — hero-card corner; canvas inset keeps its square corners inside the arc

    wxPanel* preview_container = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE);
    preview_container->SetBackgroundColour(preview_bg);
    preview_container->SetBackgroundStyle(wxBG_STYLE_PAINT);
    preview_container->Bind(wxEVT_PAINT, [preview_bg, preview_bd, card_radius](wxPaintEvent& e) {
        auto* p = static_cast<wxPanel*>(e.GetEventObject());
        wxAutoBufferedPaintDC dc(p);
        wxSize sz = p->GetClientSize();
        dc.SetBrush(wxBrush(p->GetParent()->GetBackgroundColour()));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(0, 0, sz.x, sz.y);
        dc.SetBrush(wxBrush(preview_bg));
        dc.SetPen(wxPen(preview_bd, 1));
        dc.DrawRoundedRectangle(0, 0, sz.x, sz.y, card_radius);
    });

    wxBoxSizer* container_sizer = new wxBoxSizer(wxVERTICAL);

    wxGLAttributes canvas_attrs;
    canvas_attrs.PlatformDefaults().RGBA().DoubleBuffer().Depth(24).EndList();
    m_canvas = new ModelPreviewCanvas(preview_container, canvas_attrs);
    m_canvas->SetToolTip(_L("Drag to rotate, scroll to zoom, double-click to fit"));
    container_sizer->Add(m_canvas, 1, wxEXPAND | wxALL, FromDIP(4));

    preview_container->SetSizer(container_sizer);
    root_sizer->Add(preview_container, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    // ---- Action bar: pill buttons ----
    const int btn_height = FromDIP(40);
    const int btn_radius = btn_height / 2;

    // Fit to view — tonal utility button (left aligned).
    m_btn_fit = new Button(this, _L("Fit to view"));
    m_btn_fit->SetCornerRadius(btn_radius);
    m_btn_fit->SetMinSize(wxSize(FromDIP(120), btn_height));
    {
        StateColor fit_bg(
            std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHighest), StateColor::Pressed),
            std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Hovered),
            std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainer), StateColor::Normal));
        StateColor fit_bd(
            std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OutlineVariant), StateColor::Normal));
        StateColor fit_text(
            std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OnSurface), StateColor::Normal));
        m_btn_fit->SetBackgroundColor(fit_bg);
        m_btn_fit->SetBorderColor(fit_bd);
        m_btn_fit->SetTextColor(fit_text);
    }

    // Close — outlined button.
    m_btn_close = new Button(this, _L("Close"));
    m_btn_close->SetId(wxID_CANCEL);
    m_btn_close->SetCornerRadius(btn_radius);
    m_btn_close->SetMinSize(wxSize(FromDIP(112), btn_height));
    {
        StateColor close_bg(
            std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OutlineVariant), StateColor::Pressed),
            std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainer), StateColor::Hovered),
            std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerLowest), StateColor::Normal));
        StateColor close_bd(
            std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OutlineVariant), StateColor::Normal));
        StateColor close_text(
            std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OnSurfaceVariant), StateColor::Normal));
        m_btn_close->SetBackgroundColor(close_bg);
        m_btn_close->SetBorderColor(close_bd);
        m_btn_close->SetTextColor(close_text);
    }

    // Open in Prepare — filled primary button (continues the existing import).
    m_btn_open = new Button(this, _L("Open in Prepare"));
    m_btn_open->SetId(wxID_OK);
    m_btn_open->SetCornerRadius(btn_radius);
    m_btn_open->SetMinSize(wxSize(FromDIP(168), btn_height));
    {
        StateColor open_bg(
            std::pair<wxColour, int>(ThemeColor::BrandGreenPressed, StateColor::Pressed),
            std::pair<wxColour, int>(ThemeColor::BrandGreenHovered, StateColor::Hovered),
            std::pair<wxColour, int>(ThemeColor::BrandGreen, StateColor::Normal));
        StateColor open_bd(
            std::pair<wxColour, int>(ThemeColor::BrandGreen, StateColor::Normal));
        StateColor open_text(
            std::pair<wxColour, int>(ThemeColor::White, StateColor::Normal));
        m_btn_open->SetBackgroundColor(open_bg);
        m_btn_open->SetBorderColor(open_bd);
        m_btn_open->SetTextColor(open_text);
    }

    m_btn_fit->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (m_canvas) m_canvas->reset_view();
    });
    m_btn_close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_CANCEL); });
    m_btn_open->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { EndModal(wxID_OK); });

    wxBoxSizer* btn_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->Add(m_btn_fit, 0, wxALIGN_CENTER_VERTICAL);
    btn_sizer->AddStretchSpacer();
    btn_sizer->Add(m_btn_close, 0, wxRIGHT, FromDIP(12));
    btn_sizer->Add(m_btn_open, 0);
    root_sizer->Add(btn_sizer, 0, wxEXPAND | wxALL, FromDIP(16));

    SetSizer(root_sizer);
    Layout();
}

void ModelPreviewDialog::on_dpi_changed(const wxRect&)
{
    // Control sizes below are baked into persistent properties (min size,
    // corner radius, fixed wxSize) using FromDIP() at build time; the base
    // DPIAware::rescale() only rescales fonts. Re-apply them so the layout
    // stays consistent when the dialog moves to a screen with a different DPI.
    SetMinSize(wxSize(FromDIP(480), FromDIP(420)));

    const int btn_height = FromDIP(40);
    const int btn_radius = btn_height / 2;
    if (m_btn_fit) {
        m_btn_fit->SetCornerRadius(btn_radius);
        m_btn_fit->SetMinSize(wxSize(FromDIP(120), btn_height));
    }
    if (m_btn_close) {
        m_btn_close->SetCornerRadius(btn_radius);
        m_btn_close->SetMinSize(wxSize(FromDIP(112), btn_height));
    }
    if (m_btn_open) {
        m_btn_open->SetCornerRadius(btn_radius);
        m_btn_open->SetMinSize(wxSize(FromDIP(168), btn_height));
    }

    Layout();
    Refresh();
}

}} // namespace Slic3r::GUI
