#ifndef slic3r_GUI_ModelPreviewDialog_hpp_
#define slic3r_GUI_ModelPreviewDialog_hpp_

#include "GUI_Utils.hpp"

#include <wx/glcanvas.h>
#include <wx/panel.h>
#include <wx/sizer.h>

#include <array>
#include <string>
#include <vector>

class Button;

namespace Slic3r { namespace GUI {

// Lightweight 3D preview panel using wxGLCanvas. Mirrors the GL context
// handling, camera math and mesh upload of TexturePreviewCanvas
// (TextureImportDialog), reduced to a single neutral-shaded mesh with
// orbit-rotate (left drag), pan (right/middle drag), wheel zoom and
// fit-to-view.
class ModelPreviewCanvas : public wxGLCanvas
{
public:
    ModelPreviewCanvas(wxWindow* parent, const wxGLAttributes& attrs);
    ~ModelPreviewCanvas() override;

    void set_mesh_data(const std::vector<std::array<float, 3>>& vertices,
                       const std::vector<std::array<int, 3>>&   indices);
    // Frame the whole model in the default orbit view (fit-to-view).
    void reset_view();

private:
    void on_paint(wxPaintEvent& evt);
    void on_size(wxSizeEvent& evt);
    void on_mouse(wxMouseEvent& evt);
    void ensure_gl_ready();
    void render();
    void render_mesh();
    void compute_smooth_normals();
    void update_bounding_box();

    wxGLContext* m_context        = nullptr;
    bool         m_gl_initialized = false;

    float   m_zoom  = 1.0f;
    float   m_rot_x = -30.0f;
    float   m_rot_y = 30.0f;
    float   m_pan_x = 0.0f;
    float   m_pan_y = 0.0f;
    wxPoint m_last_mouse_pos;
    enum class DragMode { None, Rotate, Pan };
    DragMode m_drag_mode = DragMode::None;

    std::vector<std::array<float, 3>> m_vertices;
    std::vector<std::array<int, 3>>   m_indices;
    std::vector<std::array<float, 3>> m_vertex_normals;

    std::array<float, 3> m_center = {0, 0, 0};
    float                m_radius = 1.0f;
};

// MD3-styled preview dialog for a downloaded MakerWorld model. Presents an
// interactive OpenGL preview and offers [Open in Prepare] (returns wxID_OK,
// continues the existing import) and [Close] (returns wxID_CANCEL). Geometry
// is supplied pre-extracted so the caller controls whether a preview is worth
// showing at all.
class ModelPreviewDialog : public DPIDialog
{
public:
    ModelPreviewDialog(wxWindow*                                 parent,
                       const wxString&                           model_name,
                       const std::vector<std::array<float, 3>>&  vertices,
                       const std::vector<std::array<int, 3>>&    indices);
    ~ModelPreviewDialog() override = default;

    void on_dpi_changed(const wxRect& suggested_rect) override;

    // Load merged geometry from a model file (3mf/stl/obj/...) into the
    // canvas-ready vertex/index arrays. Returns true on a non-empty mesh.
    // Never throws — failures return false so the caller can fall back to the
    // plain import path.
    static bool load_geometry(const std::string&                 path,
                              std::vector<std::array<float, 3>>&  vertices,
                              std::vector<std::array<int, 3>>&    indices);

private:
    void build_ui(const wxString& model_name);

    ModelPreviewCanvas* m_canvas    = nullptr;
    Button*             m_btn_fit   = nullptr;
    Button*             m_btn_close = nullptr;
    Button*             m_btn_open  = nullptr;
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_ModelPreviewDialog_hpp_
