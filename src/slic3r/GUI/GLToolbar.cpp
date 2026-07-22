#include "libslic3r/Point.hpp"
#include "libslic3r/libslic3r.h"

#include "GLToolbar.hpp"

#include "slic3r/GUI/GLCanvas3D.hpp"
#include "slic3r/GUI/GLModel.hpp"
#include "slic3r/GUI/GLShader.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/Camera.hpp"
#include "slic3r/GUI/Plater.hpp"
#include "slic3r/GUI/Gizmos/GLIconGlyphBridge.hpp"
#include "slic3r/GUI/Widgets/MD3Tokens.hpp"

#include <wx/event.h>
#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <wx/settings.h>
#include <wx/glcanvas.h>

namespace Slic3r {
namespace GUI {

//BBS: GUI refactor: GLToolbar
wxDEFINE_EVENT(EVT_GLTOOLBAR_OPEN_PROJECT, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_SLICE_ALL, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_SLICE_PLATE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_PRINT_ALL, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_PRINT_PLATE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_EXPORT_GCODE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_SEND_GCODE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_UPLOAD_GCODE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_EXPORT_SLICED_FILE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_EXPORT_ALL_SLICED_FILE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_PRINT_SELECT, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_SEND_TO_PRINTER, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_SEND_TO_PRINTER_ALL, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_PRINT_MULTI_MACHINE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_SEND_MULTI_APP, SimpleEvent);


wxDEFINE_EVENT(EVT_GLTOOLBAR_ADD, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_DELETE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_DELETE_ALL, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_ADD_PLATE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_DEL_PLATE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_ORIENT, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_ARRANGE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_CUT, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_COPY, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_PASTE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_LAYERSEDITING, SimpleEvent);
//BBS: add clone event
wxDEFINE_EVENT(EVT_GLTOOLBAR_CLONE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_MORE, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_FEWER, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_SPLIT_OBJECTS, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_SPLIT_VOLUMES, SimpleEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_FILLCOLOR, IntEvent);
wxDEFINE_EVENT(EVT_GLTOOLBAR_SELECT_SLICED_PLATE, wxCommandEvent);

wxDEFINE_EVENT(EVT_GLVIEWTOOLBAR_3D, SimpleEvent);
wxDEFINE_EVENT(EVT_GLVIEWTOOLBAR_PREVIEW, SimpleEvent);
wxDEFINE_EVENT(EVT_GLVIEWTOOLBAR_ASSEMBLE, SimpleEvent);

const GLToolbarItem::ActionCallback GLToolbarItem::Default_Action_Callback = [](){};
const GLToolbarItem::VisibilityCallback GLToolbarItem::Default_Visibility_Callback = []()->bool { return true; };
const GLToolbarItem::EnablingCallback GLToolbarItem::Default_Enabling_Callback = []()->bool { return true; };
const GLToolbarItem::RenderCallback GLToolbarItem::Default_Render_Callback = [](float, float, float, float, float){};

GLToolbarItem::Data::Option::Option()
    : toggable(false)
    , action_callback(Default_Action_Callback)
    , render_callback(nullptr)
{
}

GLToolbarItem::Data::Data()
    : name("")
    , tooltip("")
    , additional_tooltip("")
    , sprite_id(-1)
    , visible(true)
    , visibility_callback(Default_Visibility_Callback)
    , enabling_callback(Default_Enabling_Callback)
    //BBS: GUI refactor
    , extra_size_ratio(0.0)
    , button_text("")
    //BBS gei a default value
    , image_width(40)
    , image_height(40)
{
}

const GLToolbarItem::Data& GLToolbarItem::get_data() const
{
    return m_data;
}

void GLToolbarItem::set_visible(bool visible)
{
    m_data.visible = visible;
}

GLToolbarItem::EType GLToolbarItem::get_type() const
{
    return m_type;
}

bool GLToolbarItem::is_inside(const Vec2d& scaled_mouse_pos) const
{
    bool inside = (render_rect[0] <= (float)scaled_mouse_pos(0))
        && ((float)scaled_mouse_pos(0) <= render_rect[1])
        && (render_rect[2] <= (float)scaled_mouse_pos(1))
        && ((float)scaled_mouse_pos(1) <= render_rect[3]);
    return inside;
}

bool GLToolbarItem::is_collapsible() const
{
    return m_data.b_collapsible;
}

bool GLToolbarItem::is_collapse_button() const
{
    return m_data.b_collapse_button;
}

void GLToolbarItem::set_collapsed(bool value)
{
    if (!is_collapsible()) {
        return;
    }
    m_data.b_collapsed = value;
}

bool GLToolbarItem::is_collapsed() const
{
    if (!is_collapsible()) {
        return false;
    }
    return m_data.b_collapsed;
}

bool GLToolbarItem::recheck_pressed() const
{
    bool rt = false;
    if (m_data.pressed_recheck_callback) {
        const bool recheck_rt = m_data.pressed_recheck_callback();
        rt = (is_pressed() != recheck_rt);
    }
    return rt;
}

GLToolbarItem::GLToolbarItem(GLToolbarItem::EType type, const GLToolbarItem::Data& data)
    : m_type(type)
    , m_state(Normal)
    , m_data(data)
    , m_last_action_type(Undefined)
    , m_highlight_state(NotHighlighted)
{
}

void GLToolbarItem::set_state(EState state)
{
    if (m_data.name == "arrange" || m_data.name == "layersediting" || m_data.name == "assembly_view") {
        if (m_state == Hover && state == HoverPressed) {
            start = std::chrono::system_clock::now();
        }
        else if ((m_state == HoverPressed && state == Hover) ||
                 (m_state == Pressed && state == Normal) ||
                 (m_state == HoverPressed && state == Normal)) {
            if (m_data.name != "assembly_view") {
                std::chrono::system_clock::time_point end = std::chrono::system_clock::now();
                std::chrono::duration<int> duration = std::chrono::duration_cast<std::chrono::duration<int>>(end - start);
                int times = duration.count();

                NetworkAgent* agent = GUI::wxGetApp().getAgent();
                if (agent) {
                    std::string name = m_data.name + "_duration";
                    std::string value = "";
                    int existing_time = 0;

                    agent->track_get_property(name, value);
                    try {
                        if (value != "") {
                            existing_time = std::stoi(value);
                        }
                    }
                    catch (...) {}

                    BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << " tool name:" << name << " duration: " << times + existing_time;
                    agent->track_update_property(name, std::to_string(times + existing_time));
                }
            }
        }
    }
    m_state = state;
}

std::string GLToolbarItem::get_icon_filename(bool is_dark_mode) const
{
    if (m_data.icon_filename_callback) {
        return m_data.icon_filename_callback(is_dark_mode);
    }
    return "";
}

void GLToolbarItem::set_last_action_type(GLToolbarItem::EActionType type)
{
    m_last_action_type = type;
}

void GLToolbarItem::do_left_action()
{
    m_last_action_type = Left;
    m_data.left.action_callback();
}

void GLToolbarItem::do_right_action()
{
    m_last_action_type = Right;
    m_data.right.action_callback();
}

bool GLToolbarItem::is_visible() const
{
    bool rt = m_data.visible;
    return rt;
}

bool GLToolbarItem::toggle_disable_others() const
{
    return m_data.b_toggle_disable_others;
}

bool GLToolbarItem::toggle_affectable() const
{
    return m_data.b_toggle_affectable;
}

bool GLToolbarItem::update_visibility()
{
    bool visible = m_data.visibility_callback();
    bool ret = (m_data.visible != visible);
    if (ret)
        m_data.visible = visible;
    // Return false for separator as it would always return true.
    return is_separator() ? false : ret;
}

bool GLToolbarItem::update_enabled_state()
{
    bool enabled = m_data.enabling_callback();
    bool ret = (is_enabled() != enabled);
    if (ret)
        m_state = enabled ? GLToolbarItem::Normal : GLToolbarItem::Disabled;

    return ret;
}

//BBS: GUI refactor: GLToolbar
void GLToolbarItem::render_text() const
{
    if (is_collapsed()) {
        return;
    }
    float tex_width = (float)m_data.text_texture.get_width();
    float tex_height = (float)m_data.text_texture.get_height();
    //float inv_tex_width = (tex_width != 0.0f) ? 1.0f / tex_width : 0.0f;
    //float inv_tex_height = (tex_height != 0.0f) ? 1.0f / tex_height : 0.0f;

    float internal_left_uv = 0.0f;
    float internal_right_uv = (float)m_data.text_texture.m_original_width / tex_width;
    float internal_top_uv = 0.0f;
    float internal_bottom_uv = (float)m_data.text_texture.m_original_height / tex_height;

    GLTexture::render_sub_texture(m_data.text_texture.get_id(), render_rect[0], render_rect[1], render_rect[2], render_rect[3], {{internal_left_uv, internal_bottom_uv}, {internal_right_uv, internal_bottom_uv}, {internal_right_uv, internal_top_uv}, {internal_left_uv, internal_top_uv}});
}

//BBS: GUI refactor: GLToolbar
int GLToolbarItem::generate_texture(wxFont& font)
{
    int ret = 0;
    bool result;

    if (m_type != ActionWithText && m_type != ActionWithTextImage)
        return -1;

    result = m_data.text_texture.generate_from_text_string(m_data.button_text, font);
    if (!result)
        ret = -1;

    return ret;
}


int GLToolbarItem::generate_image_texture()
{
    int ret = 0;
    bool result = false;
    if (m_type != ActionWithTextImage)
        return -1;

    /* load default texture when image is empty */
    if (m_data.image_data.empty()) {
        std::string default_image = resources_dir() + "/images/default_thumbnail.svg";
        result = m_data.image_texture.load_from_svg_file(default_image, true, false, false, m_data.image_width);
    }  else {
        result = m_data.image_texture.load_from_raw_data(m_data.image_data, m_data.image_width, m_data.image_height);
    }
    if (!result)
        ret = -1;

    return ret;
}

void GLToolbarItem::render(unsigned int tex_id, unsigned int tex_width, unsigned int tex_height, unsigned int icon_size, float toolbar_height, bool b_flip_v, float glyph_ratio, bool draw_icon) const
{
    auto uvs = [this](unsigned int tex_width, unsigned int tex_height, unsigned int icon_size, bool b_flip_v) -> GLTexture::Quad_UVs
    {
        assert((tex_width != 0) && (tex_height != 0));
        GLTexture::Quad_UVs ret;
        // tiles in the texture are spaced by 1 pixel
        float icon_size_px = (float)(tex_width - 1) / ((float)Num_States + (float)Num_Rendered_Highlight_States);
        char render_state = (m_highlight_state ==  NotHighlighted ? m_state : Num_States + m_highlight_state);
        float inv_tex_width = 1.0f / (float)tex_width;
        float inv_tex_height = 1.0f / (float)tex_height;
        // tiles in the texture are spaced by 1 pixel
        float u_offset = 1.0f * inv_tex_width;
        float v_offset = 1.0f * inv_tex_height;
        float du = icon_size_px * inv_tex_width;
        float dv = icon_size_px * inv_tex_height;
        float left = u_offset + (float)render_state * du;
        float right = left + du - u_offset;
        float top = v_offset + (float)m_data.sprite_id * dv;
        float bottom = top + dv - v_offset;

        if (b_flip_v) {
            std::swap(top, bottom);
        }
        ret.left_top = { left, top };
        ret.left_bottom = { left, bottom };
        ret.right_bottom = { right, bottom };
        ret.right_top = { right, top };
        return ret;
    };

    float* t_render_rect = render_rect;
    if (is_visible()) {
        if (!is_collapsed()) {
            if (draw_icon) {
                // MD3: centre the sprite inside its tile so the mark reads at the
                // kit size while render_rect (hit-test) stays the full tile.
                float l = render_rect[0], r = render_rect[1], b = render_rect[2], t = render_rect[3];
                if (glyph_ratio > 0.0f && glyph_ratio < 1.0f) {
                    const float cx = 0.5f * (l + r), cy = 0.5f * (b + t);
                    const float hw = 0.5f * (r - l) * glyph_ratio, hh = 0.5f * (t - b) * glyph_ratio;
                    l = cx - hw; r = cx + hw; b = cy - hh; t = cy + hh;
                }
                GLTexture::render_sub_texture(tex_id, l, r, b, t, uvs(tex_width, tex_height, icon_size, b_flip_v));
            }
        }
        else if (override_render_rect) {
            t_render_rect = override_render_rect;
        }
    }

    if (is_pressed())
    {
        if ((m_last_action_type == Left) && m_data.left.can_render())
            m_data.left.render_callback(t_render_rect[0], t_render_rect[1], t_render_rect[2], t_render_rect[3], toolbar_height);
        else if ((m_last_action_type == Right) && m_data.right.can_render())
            m_data.right.render_callback(t_render_rect[0], t_render_rect[1], t_render_rect[2], t_render_rect[3], toolbar_height);
    }
}

void GLToolbarItem::render_image(unsigned int tex_id, float left, float right, float bottom, float top, unsigned int tex_width, unsigned int tex_height, unsigned int icon_size) const
{
    GLTexture::Quad_UVs image_uvs = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f } };
    //GLTexture::Quad_UVs image_uvs = { { 0.0f, 1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f } };

    if (is_visible()) {
        GLTexture::render_sub_texture(tex_id, left, right, bottom, top, image_uvs);
    }

    if (is_pressed()) {
        if ((m_last_action_type == Left) && m_data.left.can_render())
            m_data.left.render_callback(left, right, bottom, top, 0.0f);
        else if ((m_last_action_type == Right) && m_data.right.can_render())
            m_data.right.render_callback(left, right, bottom, top, 0.0f);
    }
}

const float GLToolbar::Default_Icons_Size = 40.0f;

ToolbarLayout::ToolbarLayout()
    : type(Horizontal)
    , horizontal_orientation(HO_Center)
    , vertical_orientation(VO_Center)
    , position_mode(EPositionMode::TopMiddle)
    , offset(0.0f)
    , top(0.0f)
    , left(0.0f)
    , border(0.0f)
    , separator_size(0.0f)
    , gap_size(0.0f)
    , icons_size(GLToolbar::Default_Icons_Size)
    , scale(1.0f)
    , width(0.0f)
    , height(0.0f)
    , collapsed_offset(0.0f)
    , dirty(true)
{
}

GLToolbar::GLToolbar(GLToolbar::EType type, const std::string& name)
    : m_type(type)
    , m_name(name)
    , m_pressed_toggable_id(-1)
{
}

GLToolbar::~GLToolbar()
{
    if (m_sel_glyph_tex != 0) {
        GLuint id = static_cast<GLuint>(m_sel_glyph_tex);
        glsafe(::glDeleteTextures(1, &id));
        m_sel_glyph_tex = 0;
    }
}

unsigned int GLToolbar::ensure_selected_glyph_texture(uint32_t codepoint, int px, int* out_w, int* out_h) const
{
    const bool dark = wxGetApp().dark_mode();
    if (codepoint == 0 || px <= 0 || !GLIconGlyphBridge::available()) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return 0;
    }
    if (m_sel_glyph_tex != 0 && codepoint == m_sel_glyph_cp && px == m_sel_glyph_px && dark == m_sel_glyph_dark) {
        if (out_w) *out_w = m_sel_glyph_w;
        if (out_h) *out_h = m_sel_glyph_h;
        return m_sel_glyph_tex;
    }
    // Bail out of a permanently failing raster (same key) without re-attempting it
    // every frame, but always retry once the codepoint/size/theme changes.
    const bool same_key = codepoint == m_sel_glyph_cp && px == m_sel_glyph_px && dark == m_sel_glyph_dark;
    if (m_sel_glyph_failed && same_key) {
        if (out_w) *out_w = 0;
        if (out_h) *out_h = 0;
        return 0;
    }
    if (m_sel_glyph_tex != 0) {
        GLuint old = static_cast<GLuint>(m_sel_glyph_tex);
        glsafe(::glDeleteTextures(1, &old));
        m_sel_glyph_tex = 0;
    }
    int w = 0, h = 0;
    const wxColour on_primary = MD3::resolve(MD3::Role::OnPrimary, dark);
    const unsigned int tex = GLIconGlyphBridge::make_glyph_texture(codepoint, px, on_primary, &w, &h);
    m_sel_glyph_cp = codepoint;
    m_sel_glyph_px = px;
    m_sel_glyph_dark = dark;
    m_sel_glyph_tex = tex;
    m_sel_glyph_w = w;
    m_sel_glyph_h = h;
    m_sel_glyph_failed = (tex == 0);
    if (out_w) *out_w = w;
    if (out_h) *out_h = h;
    return tex;
}

bool GLToolbar::init(const BackgroundTexture::Metadata& background_texture)
{
    std::string path = resources_dir() + "/images/";
    bool res = false;

    if (!background_texture.filename.empty())
        res = m_background_texture.texture.load_from_file(path + background_texture.filename, false, GLTexture::SingleThreaded, false);

    if (res)
        m_background_texture.metadata = background_texture;

    return res;
}

bool GLToolbar::init_arrow(const BackgroundTexture::Metadata& arrow_texture)
{
    if (m_arrow_texture.texture.get_id() != 0)
        return true;

    std::string path = resources_dir() + "/images/";
    bool res = false;

    if (!arrow_texture.filename.empty()) {
        res = m_arrow_texture.texture.load_from_svg_file(path + arrow_texture.filename, false, false, false, 1000);
    }
    if (res)
        m_arrow_texture.metadata = arrow_texture;

    return res;
}

const ToolbarLayout& GLToolbar::get_layout() const
{
    if (m_layout.dirty) {
        calc_layout();
    }
    return m_layout;
}

void GLToolbar::set_layout_type(ToolbarLayout::EType type)
{
    if (m_layout.type == type) {
        return;
    }
    m_layout.type = type;
    m_layout.dirty = true;
}

ToolbarLayout::EHorizontalOrientation GLToolbar::get_horizontal_orientation() const
{
    return m_layout.horizontal_orientation;
}

void GLToolbar::set_horizontal_orientation(ToolbarLayout::EHorizontalOrientation orientation)
{
    m_layout.horizontal_orientation = orientation;
}

ToolbarLayout::EVerticalOrientation GLToolbar::get_vertical_orientation() const
{
    return m_layout.vertical_orientation;
}

void GLToolbar::set_vertical_orientation(ToolbarLayout::EVerticalOrientation orientation)
{
    m_layout.vertical_orientation = orientation;
}

void GLToolbar::set_position_mode(ToolbarLayout::EPositionMode t_position_mode)
{
    m_layout.position_mode = t_position_mode;
}

void GLToolbar::set_offset(float offset)
{
    m_layout.offset = offset;
}

void GLToolbar::set_position(float top, float left)
{
    m_layout.top = top;
    m_layout.left = left;
}

void GLToolbar::set_border(float border)
{
    m_layout.border = border;
    m_layout.dirty = true;
}

void GLToolbar::set_separator_size(float size)
{
    m_layout.separator_size = size;
    m_layout.dirty = true;
}

void GLToolbar::set_gap_size(float size)
{
    m_layout.gap_size = size;
    m_layout.dirty = true;
}

void GLToolbar::set_icons_size(float size)
{
    if (abs(m_layout.icons_size - size) < 1e-6f) {
        return;
    }
    m_layout.icons_size = size;
    m_layout.dirty = true;
    m_icons_texture_dirty = true;
}

void GLToolbar::set_text_size(float size)
{
    if (m_layout.text_size != size)
    {
        m_layout.text_size = size;
        m_layout.dirty = true;
    }
}

void GLToolbar::set_scale(float scale)
{
    if (m_layout.scale != scale) {
        m_layout.scale = scale;
        m_layout.dirty = true;
        m_icons_texture_dirty = true;
    }
}

float GLToolbar::get_scale() const
{
    return m_layout.scale;
}

void GLToolbar::set_icon_dirty()
{
    m_icons_texture_dirty = true;
}

bool GLToolbar::is_enabled() const
{
    return m_enabled;
}

void GLToolbar::set_enabled(bool enable)
{
    m_enabled = enable;
}

float GLToolbar::get_icons_size() const
{
    return m_layout.icons_size;
}

float GLToolbar::get_width() const
{
    if (m_layout.dirty)
        calc_layout();

    return m_layout.width;
}

float GLToolbar::get_height() const
{
    if (m_layout.dirty)
        calc_layout();

    return m_layout.height;
}

bool GLToolbar::add_item(const GLToolbarItem::Data& data, GLToolbarItem::EType type)
{
    const auto item = std::make_shared<GLToolbarItem>(type, data);
    m_items.emplace_back(item);
    m_layout.dirty = true;
    return true;
}

bool GLToolbar::add_separator()
{
    GLToolbarItem::Data data;
    const auto item = std::make_shared<GLToolbarItem>(GLToolbarItem::Separator, data);
    m_items.push_back(item);
    m_layout.dirty = true;
    return true;
}

bool GLToolbar::insert_separator_after(const std::string& name)
{
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i] && m_items[i]->get_name() == name) {
            GLToolbarItem::Data data;
            const auto sep = std::make_shared<GLToolbarItem>(GLToolbarItem::Separator, data);
            m_items.insert(m_items.begin() + i + 1, sep);
            m_layout.dirty = true;
            return true;
        }
    }
    return false;
}

bool GLToolbar::del_all_item()
{
    m_items.clear();
    m_pressed_toggable_id = -1;
    m_mouse_capture.reset();
    m_icons_texture_dirty = true;
    m_layout.dirty = true;
    return true;
}

void GLToolbar::select_item(const std::string& name)
{
    if (is_item_disabled(name))
        return;

    for (const auto& item : m_items)
    {
        if (!item->is_disabled())
        {
            bool hover = item->is_hovered();
            item->set_state((item->get_name() == name) ? (hover ? GLToolbarItem::HoverPressed : GLToolbarItem::Pressed) : (hover ? GLToolbarItem::Hover : GLToolbarItem::Normal));
        }
    }
}

bool GLToolbar::is_item_pressed(const std::string& name) const
{
    for (const auto& item : m_items)
    {
        if (item->get_name() == name)
            return item->is_pressed();
    }

    return false;
}

bool GLToolbar::is_item_disabled(const std::string& name) const
{
    for (const auto& item : m_items)
    {
        if (item->get_name() == name)
            return item->is_disabled();
    }

    return false;
}

bool GLToolbar::is_item_visible(const std::string& name) const
{
    for (const auto& item : m_items)
    {
        if (item->get_name() == name)
            return item->is_visible();
    }

    return false;
}

bool GLToolbar::is_any_item_pressed() const
{
    for (const auto& item : m_items)
    {
        if (item->is_pressed())
            return true;
    }

    return false;
}

int GLToolbar::get_item_id(const std::string& name) const
{
    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        if (m_items[i]->get_name() == name)
            return i;
    }

    return -1;
}

std::string GLToolbar::get_tooltip() const
{
    std::string tooltip;

    for (const auto& item : m_items)
    {
        if (item->is_collapsed()) {
            continue;
        }
        if (item->is_hovered())
        {
            const auto t_item_data = item->get_data();
            if (t_item_data.on_hover) {
                tooltip = t_item_data.on_hover();
                break;
            }
            else {
                tooltip = item->get_tooltip();
                if (!item->is_enabled())
                {
                    const std::string& additional_tooltip = item->get_additional_tooltip();
                    if (!additional_tooltip.empty())
                        tooltip += ":\n" + additional_tooltip;

                    break;
                }
            }
        }
    }

    return tooltip;
}

void GLToolbar::set_tooltip(int item_id, const std::string& text)
{
    if (0 <= item_id && item_id < (int)m_items.size())
        m_items[item_id]->set_tooltip(text);
}

void GLToolbar::get_additional_tooltip(int item_id, std::string& text)
{
    if (0 <= item_id && item_id < (int)m_items.size())
    {
        text = m_items[item_id]->get_additional_tooltip();
        return;
    }

    text.clear();
}

void GLToolbar::set_additional_tooltip(int item_id, const std::string& text)
{
    if (0 <= item_id && item_id < (int)m_items.size())
        m_items[item_id]->set_additional_tooltip(text);
}

int GLToolbar::get_visible_items_cnt() const
{
    int cnt = 0;
    for (unsigned int i = 0; i < (unsigned int)m_items.size(); ++i)
        if (m_items[i]->is_visible() && !m_items[i]->is_separator())
            cnt++;

    return cnt;
}

const std::shared_ptr<GLToolbarItem>& GLToolbar::get_item(const std::string& item_name) const
{
    if (m_enabled)
    {
        for (const auto& item : m_items)
        {
            if (item->get_name() == item_name)
            {
                return item;
            }
        }
    }
    static std::shared_ptr<GLToolbarItem> s_empty{ nullptr };
    return s_empty;
}

void GLToolbar::calc_layout() const
{
    switch (m_layout.type)
    {
    default:
    case ToolbarLayout::EType::Horizontal:
    {
        m_layout.width = get_width_horizontal();
        m_layout.height = get_height_horizontal();
        break;
    }
    case ToolbarLayout::EType::Vertical:
    {
        m_layout.width = get_width_vertical();
        m_layout.height = get_height_vertical();
        break;
    }
    }

    m_layout.dirty = false;
}

const std::shared_ptr<ToolbarRenderer>& GLToolbar::get_renderer() const
{
    if (!m_p_renderer || m_p_renderer->get_mode() != m_rendering_mode) {
        switch (m_rendering_mode) {
        case EToolbarRenderingMode::KeepSize:
            m_p_renderer = std::make_shared<ToolbarKeepSizeRenderer>();
            break;
        case EToolbarRenderingMode::Auto:
        default:
            m_p_renderer = std::make_shared<ToolbarAutoSizeRenderer>();
            break;
        }
        for (const auto& p_item : m_items) {
            p_item->set_collapsed(false);
        }
    }
    return m_p_renderer;
}

float GLToolbar::get_width_horizontal() const
{
    float size = 2.0f * m_layout.border;
    for (unsigned int i = 0; i < (unsigned int)m_items.size(); ++i)
    {
        if (!m_items[i]->is_visible())
            continue;

        if (m_items[i]->is_separator())
            size += m_layout.separator_size;
        else if (m_items[i]->get_type() == GLToolbarItem::EType::SeparatorLine) {
            size += ((float)m_layout.icons_size * 0.5f);
        }
        else
        {
            size += (float)m_layout.icons_size;
            if (m_items[i]->is_action_with_text())
                size += m_items[i]->get_extra_size_ratio() * m_layout.icons_size;
            if (m_items[i]->is_action_with_text_image())
                size += m_layout.text_size;
        }

        if (i < m_items.size() - 1) {
            size += m_layout.gap_size;
        }
    }

    return size * m_layout.scale;
}

float GLToolbar::get_width_vertical() const
{
    float max_extra_text_size = 0.0;
    for (unsigned int i = 0; i < (unsigned int)m_items.size(); ++i)
    {
        if (m_items[i]->is_action_with_text())
        {
            float temp_size = m_items[i]->get_extra_size_ratio() * m_layout.icons_size;

            max_extra_text_size = (temp_size > max_extra_text_size) ? temp_size : max_extra_text_size;
        }

        if (m_items[i]->is_action_with_text_image())
        {
            max_extra_text_size = m_layout.text_size;
        }
    }

    return (2.0f * m_layout.border + m_layout.icons_size + max_extra_text_size) * m_layout.scale;
}

float GLToolbar::get_height_horizontal() const
{
    return (2.0f * m_layout.border + m_layout.icons_size) * m_layout.scale;
}

float GLToolbar::get_height_vertical() const
{
    return get_main_size();
}

float GLToolbar::get_main_size() const
{
    float size = 2.0f * m_layout.border;
    for (unsigned int i = 0; i < (unsigned int)m_items.size(); ++i)
    {
        if (!m_items[i]->is_visible())
            continue;

        if (m_items[i]->is_separator())
            size += m_layout.separator_size;
        else
            size += (float)m_layout.icons_size;
    }

    if (m_items.size() > 1)
        size += ((float)m_items.size() - 1.0f) * m_layout.gap_size;

    return size * m_layout.scale;
}

bool GLToolbar::generate_icons_texture() const
{
    std::string path = resources_dir() + "/images/";
    std::vector<std::string> filenames;
    for (const auto& item : m_items) {
        const std::string icon_filename = item->get_icon_filename(m_b_dark_mode_enabled);
        if (!icon_filename.empty())
            filenames.push_back(path + icon_filename);
    }

    std::vector<std::pair<int, bool>> states;
    //1: white only, 2: gray only, 0 : normal
    //true/false: apply background or not
    if (m_type == Normal) {
        states.push_back({ 1, false }); // Normal
        states.push_back({ 0, false }); // Pressed
        states.push_back({ 2, false }); // Disabled
        states.push_back({ 0, false }); // Hover
        states.push_back({ 0, false }); // HoverPressed
        states.push_back({ 2, false }); // HoverDisabled
        states.push_back({ 0, false }); // HighlightedShown
        states.push_back({ 2, false }); // HighlightedHidden
    }
    else {
        states.push_back({ 1, false }); // Normal
        states.push_back({ 0, true });  // Pressed
        states.push_back({ 2, false }); // Disabled
        states.push_back({ 0, false }); // Hover
        states.push_back({ 1, true });  // HoverPressed
        states.push_back({ 1, false }); // HoverDisabled
        states.push_back({ 0, false }); // HighlightedShown
        states.push_back({ 1, false }); // HighlightedHidden
    }

    unsigned int sprite_size_px = (unsigned int)(m_layout.icons_size * m_layout.scale);
    //    // force even size
    //    if (sprite_size_px % 2 != 0)
    //        sprite_size_px += 1;

    bool res = m_icons_texture.load_from_svg_files_as_sprites_array(filenames, states, sprite_size_px, false);
    if (res)
        m_icons_texture_dirty = false;

    // MD3 glyph bridge: overpaint the rows whose item maps to a Material Symbols
    // glyph so the main toolbar and the gizmo rail resolve to MD3 icons (idle
    // OnSurfaceVariant / selected Primary), retiring the toolbar_*.svg light/dark
    // pairs. This is an in-place glTexSubImage2D over the atlas that was just
    // built, so the sprite layout, dimensions and every render/hit-test path stay
    // identical. Unmapped items (e.g. "orient" -> screen_rotation, still absent
    // from the vendored font, and the separator sprites) keep their SVG sprite,
    // and when the icon font is unavailable the whole texture is left as SVG.
    if (res && GLIconGlyphBridge::available()) {
        std::vector<GLIconGlyphBridge::AtlasGlyphRow> glyph_rows;
        int row = 0;
        for (const auto& item : m_items) {
            // Mirror the exact filter used to build `filenames` above so the row
            // index matches the atlas row the item renders from.
            if (item->get_icon_filename(m_b_dark_mode_enabled).empty())
                continue;
            const uint32_t glyph = GLIconGlyphBridge::glyph_for_toolbar_item(item->get_name());
            if (glyph != 0)
                glyph_rows.push_back({ row, glyph });
            ++row;
        }
        if (!glyph_rows.empty()) {
            GLIconGlyphBridge::overpaint_toolbar_atlas(
                m_icons_texture.get_id(), m_icons_texture.get_width(), m_icons_texture.get_height(),
                (int)sprite_size_px, (int)states.size(), glyph_rows,
                m_b_dark_mode_enabled, MD3::ColorScheme::Brand);
        }
    }

    return res;
}

bool GLToolbar::update_items_visibility()
{
    bool ret = false;

    for (const auto& item : m_items) {
        ret |= item->update_visibility();
    }

    if (ret)
        m_layout.dirty = true;

    // updates separators visibility to avoid having two of them consecutive
    bool any_item_visible = false;
    for (const auto& item : m_items) {
        if (!item->is_separator())
            any_item_visible |= item->is_visible();
        else {
            item->set_visible(any_item_visible);
            any_item_visible = false;
        }
    }

    return ret;
}

bool GLToolbar::update_items_enabled_state()
{
    bool ret = false;

    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        const auto& item = m_items[i];
        ret |= item->update_enabled_state();
        if ((m_pressed_toggable_id != -1) && (m_pressed_toggable_id != i)) {
            const auto& pressed_item = m_items[m_pressed_toggable_id];
            if (pressed_item->toggle_disable_others()) {
                if (item->is_enabled() && item->toggle_affectable()) {
                    ret = true;
                    item->set_state(GLToolbarItem::Disabled);
                }
            }
        }
    }

    if (ret)
        m_layout.dirty = true;

    return ret;
}

bool GLToolbar::update_items_pressed_state()
{
    bool ret = false;

    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        const auto& item = m_items[i];
        if (!item) {
            continue;
        }

        if (!item->recheck_pressed()) {
            continue;
        }
        ret = true;
        if (item->is_pressed()) {
            item->set_state(GLToolbarItem::EState::Normal);
            if (m_pressed_toggable_id == i) {
                m_pressed_toggable_id = -1;
            }
        }
        else {
            item->set_state(GLToolbarItem::EState::Pressed);
            m_pressed_toggable_id = i;
            item->set_last_action_type(GLToolbarItem::EActionType::Left);
            set_collapsed();
        }
    }

    return ret;
}

void GLToolbar::render(const Camera& t_camera)
{
    if (!m_enabled || m_items.empty())
        return;

    const auto& p_renderer = get_renderer();

    if (!p_renderer) {
        return;
    }
    p_renderer->render(*this, t_camera);
}

bool GLToolbar::update_items_state()
{
    bool ret = false;
    ret |= update_items_visibility();
    ret |= update_items_enabled_state();
    ret |= update_items_pressed_state();
    if (!is_any_item_pressed())
        m_pressed_toggable_id = -1;

    return ret;
}

void GLToolbar::set_dark_mode_enabled(bool is_enabled)
{
    if (m_b_dark_mode_enabled == is_enabled) {
        return;
    }

    m_b_dark_mode_enabled = is_enabled;
    set_icon_dirty();
}

const std::vector<std::shared_ptr<GLToolbarItem>>& GLToolbar::get_items() const
{
    return m_items;
}

const GLTexture& GLToolbar::get_icon_texture() const
{
    if (m_icons_texture_dirty) {
        generate_icons_texture();
    }
    return m_icons_texture;
}

const BackgroundTexture& GLToolbar::get_background_texture() const
{
    return m_background_texture;
}

const BackgroundTexture& GLToolbar::get_arrow_texture() const
{
    return m_arrow_texture;
}

bool GLToolbar::needs_collapsed() const
{
    const auto& p_renderer = get_renderer();
    if (!p_renderer) {
        return false;
    }
    return p_renderer->needs_collapsed();
}

void GLToolbar::toggle_collapsed()
{
    m_b_collapsed = !m_b_collapsed;
}

void GLToolbar::set_collapsed()
{
    m_b_collapsed = true;
}

bool GLToolbar::is_collapsed() const
{
    return m_b_collapsed;
}

void GLToolbar::set_collapsed_offset(uint32_t offset_in_pixel)
{
    m_layout.collapsed_offset = offset_in_pixel;
    m_layout.dirty = true;
}

uint32_t GLToolbar::get_collapsed_offset()
{
    return m_layout.collapsed_offset;
}

GLToolbar::EToolbarRenderingMode GLToolbar::get_rendering_mode()
{
    return m_rendering_mode;
}

void GLToolbar::set_rendering_mode(EToolbarRenderingMode mode)
{
    m_rendering_mode = mode;
}

void GLToolbar::render_arrow(const std::weak_ptr<GLToolbarItem>& highlighted_item)
{
    if (!m_enabled || m_items.empty()) {
        return;
    }
    const auto& p_renderer = get_renderer();
    if (!p_renderer) {
        return;
    }
    p_renderer->render_arrow(*this, highlighted_item);
}

bool GLToolbar::on_mouse(wxMouseEvent& evt, GLCanvas3D& parent)
{
    if (!m_enabled)
        return false;

    Vec2d mouse_pos((double)evt.GetX(), (double)evt.GetY());
    bool processed = false;

    // mouse anywhere
    if (!evt.Dragging() && !evt.Leaving() && !evt.Entering() && m_mouse_capture.parent != nullptr) {
        if (m_mouse_capture.any() && (evt.LeftUp() || evt.MiddleUp() || evt.RightUp())) {
            // prevents loosing selection into the scene if mouse down was done inside the toolbar and mouse up was down outside it,
            // as when switching between views
            m_mouse_capture.reset();
            return true;
        }
        m_mouse_capture.reset();
    }

    if (evt.Moving())
        update_hover_state(mouse_pos, parent);
    else if (evt.LeftUp()) {
        if (m_mouse_capture.left) {
            processed = true;
            m_mouse_capture.left = false;
        }
        else
            return false;
    }
    else if (evt.MiddleUp()) {
        if (m_mouse_capture.middle) {
            processed = true;
            m_mouse_capture.middle = false;
        }
        else
            return false;
    }
    else if (evt.RightUp()) {
        if (m_mouse_capture.right) {
            processed = true;
            m_mouse_capture.right = false;
        }
        else
            return false;
    }
    else if (evt.Dragging()) {
        if (m_mouse_capture.any())
            // if the button down was done on this toolbar, prevent from dragging into the scene
            processed = true;
        else
            return false;
    }

    int item_id = contains_mouse(mouse_pos, parent);
    if (item_id != -1) {
        // mouse inside toolbar
        if (evt.LeftDown() || evt.LeftDClick()) {
            m_mouse_capture.left = true;
            m_mouse_capture.parent = &parent;
            processed = true;
            bool rt = item_id != -2 && !m_items[item_id]->is_separator() && !m_items[item_id]->is_disabled() &&
                (m_pressed_toggable_id == -1
                    || m_items[item_id]->get_last_action_type() == GLToolbarItem::Left);
            if (!rt) {
                if (item_id >= 0 && item_id < m_items.size()) {
                    rt = !m_items[item_id]->toggle_affectable();
                }
            }
            if (!rt) {
                if (m_pressed_toggable_id >= 0 && m_pressed_toggable_id < m_items.size()) {
                    rt = !m_items[m_pressed_toggable_id]->toggle_disable_others();
                }
            }
            if (rt) {
                // mouse is inside an icon
                do_action(GLToolbarItem::Left, item_id, parent, true);
                parent.set_as_dirty();
                evt.StopPropagation();
                processed = true;
            }
        }
        else if (evt.MiddleDown()) {
            m_mouse_capture.middle = true;
            m_mouse_capture.parent = &parent;
        }
        else if (evt.RightDown()) {
            m_mouse_capture.right = true;
            m_mouse_capture.parent = &parent;
            processed = true;
            if (item_id != -2 && !m_items[item_id]->is_separator() && !m_items[item_id]->is_disabled() &&
                (m_pressed_toggable_id == -1 || m_items[item_id]->get_last_action_type() == GLToolbarItem::Right)) {
                // mouse is inside an icon
                do_action(GLToolbarItem::Right, item_id, parent, true);
                parent.set_as_dirty();
                evt.StopPropagation();
                processed = true;
            }
        }
    }

    return processed;
}

void GLToolbar::do_action(GLToolbarItem::EActionType type, int item_id, GLCanvas3D& parent, bool check_hover)
{
    if (item_id < 0 || item_id >= (int)m_items.size()) {
        return;
    }
    const auto& item = m_items[item_id];
    if (!item || item->is_separator() || item->is_disabled() || (check_hover && !item->is_hovered())) {
        return;
    }

    auto do_item_action = [this](GLToolbarItem::EActionType type, const std::shared_ptr<GLToolbarItem>& item, int item_id, GLCanvas3D& parent)->void {
        if (((type == GLToolbarItem::Right) && item->is_right_toggable()) ||
            ((type == GLToolbarItem::Left) && item->is_left_toggable()))
        {
            GLToolbarItem::EState state = item->get_state();
            if (state == GLToolbarItem::Hover)
                item->set_state(GLToolbarItem::HoverPressed);
            else if (state == GLToolbarItem::HoverPressed)
                item->set_state(GLToolbarItem::Hover);
            else if (state == GLToolbarItem::Pressed)
                item->set_state(GLToolbarItem::Normal);
            else if (state == GLToolbarItem::Normal)
                item->set_state(GLToolbarItem::Pressed);

            m_pressed_toggable_id = item->is_pressed() ? item_id : -1;
            item->reset_last_action_type();

            switch (type)
            {
            default:
            case GLToolbarItem::Left: { item->do_left_action(); break; }
            case GLToolbarItem::Right: { item->do_right_action(); break; }
            }

            parent.set_as_dirty();
        }
        else
        {
            if (m_type == Radio)
                select_item(item->get_name());
            else
                item->set_state(item->is_hovered() ? GLToolbarItem::HoverPressed : GLToolbarItem::Pressed);

            item->reset_last_action_type();
            switch (type)
            {
            default:
            case GLToolbarItem::Left: { item->do_left_action(); break; }
            case GLToolbarItem::Right: { item->do_right_action(); break; }
            }
            if (item->get_continuous_click_flag()) {
                item->set_state(GLToolbarItem::Hover);
            }
            else if ((m_type == Normal) && (item->get_state() != GLToolbarItem::Disabled) && !item->get_continuous_click_flag())
            {
                // the item may get disabled during the action, if not, set it back to normal state
                item->set_state(GLToolbarItem::Normal);
            }

            if (m_pressed_toggable_id == item_id && !item->is_pressed()) {
                m_pressed_toggable_id = -1;
            }

            parent.set_as_dirty();
        }

        if (item->is_collapse_button()) {
            toggle_collapsed();
        }
        else {
            set_collapsed();
        }
    };
    if ((m_pressed_toggable_id != -1) && (m_pressed_toggable_id != item_id))
    {
        do_item_action(type, m_items[m_pressed_toggable_id], m_pressed_toggable_id, parent);
    }
    do_item_action(type, item, item_id, parent);
}

void GLToolbar::update_hover_state(const Vec2d& mouse_pos, GLCanvas3D& parent)
{
    if (!m_enabled)
        return;

    float inv_zoom = (float)wxGetApp().plater()->get_camera().get_inv_zoom();
    Size cnv_size = parent.get_canvas_size();
    Vec2d scaled_mouse_pos((mouse_pos(0) - 0.5 * (double)cnv_size.get_width()) * inv_zoom, (0.5 * (double)cnv_size.get_height() - mouse_pos(1)) * inv_zoom);

    for (const auto& item : m_items)
    {
        if (!item->is_visible())
            continue;

        if (item->is_separator()) {
            continue;
        }

        if (item->get_type() == GLToolbarItem::EType::SeparatorLine) {
            continue;
        }
        bool inside = item->is_inside(scaled_mouse_pos);
        GLToolbarItem::EState state = item->get_state();
        switch (state)
        {
        case GLToolbarItem::Normal:
        {
            if (inside)
            {
                item->set_state(GLToolbarItem::Hover);
                parent.set_as_dirty();
            }

            break;
        }
        case GLToolbarItem::Hover:
        {
            if (!inside)
            {
                item->set_state(GLToolbarItem::Normal);
                parent.set_as_dirty();
            }

            break;
        }
        case GLToolbarItem::Pressed:
        {
            if (inside)
            {
                item->set_state(GLToolbarItem::HoverPressed);
                parent.set_as_dirty();
            }

            break;
        }
        case GLToolbarItem::HoverPressed:
        {
            if (!inside)
            {
                item->set_state(GLToolbarItem::Pressed);
                parent.set_as_dirty();
            }

            break;
        }
        case GLToolbarItem::Disabled:
        {
            if (inside)
            {
                item->set_state(GLToolbarItem::HoverDisabled);
                parent.set_as_dirty();
            }

            break;
        }
        case GLToolbarItem::HoverDisabled:
        {
            if (!inside)
            {
                item->set_state(GLToolbarItem::Disabled);
                parent.set_as_dirty();
            }

            break;
        }
        default:
        {
            break;
        }
        }
    }
}

int GLToolbar::contains_mouse(const Vec2d& mouse_pos, const GLCanvas3D& parent) const
{
    if (!m_enabled)
        return -1;

    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i]->is_collapsed()) {
            continue;
        }
        if (m_items[i]->is_hovered()) {
            return i;
        }
    }

    return -1;
}

//BBS: GUI refactor: GLToolbar
int GLToolbar::generate_button_text_textures(wxFont& font)
{
    int ret = 0;

    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        const auto& item = m_items[i];

        if (item->is_action_with_text())
        {
            ret |= item->generate_texture(font);
        }

        if (item->is_action_with_text_image())
        {
            ret |= item->generate_texture(font);
        }
    }

    return ret;
}

int GLToolbar::generate_image_textures()
{
    int ret = 0;
    for (int i = 0; i < (int)m_items.size(); ++i)
    {
        const auto& item = m_items[i];
        if (item->is_action_with_text_image()) {
            ret |= item->generate_image_texture();
        }
    }
    return ret;
}

//BBS: GUI refactor: GLToolbar
float GLToolbar::get_scaled_icon_size()
{
    return m_layout.icons_size * m_layout.scale;
}

ToolbarRenderer::ToolbarRenderer()
{
}

ToolbarRenderer::~ToolbarRenderer()
{
}

bool ToolbarRenderer::needs_collapsed() const
{
    return false;
}

// --- MD3 viewport-toolbar geometry (forward declarations) ----------------------
// The kit decoration helpers are defined in the anonymous namespace further down,
// beside render_md3_toolbar_backdrop (they share md3_append_rounded_rect). They
// are forward-declared here so the item render loops that precede that block can
// draw per-item MD3 decoration. Constants and the kind enum are defined inline.
namespace {

enum class MD3ToolbarKind : uint8_t { None, Rail, Pill };

// The atlas bakes each Material Symbol at this fraction of its sprite cell (kept
// in sync with GLIconGlyphBridge kGlyphFillRatio). To show a mark at a target
// fraction of the tile the sprite quad is drawn at target / fill of the tile.
constexpr float kMD3AtlasGlyphFill = 0.72f;
// Visible mark as a fraction of the button tile, matching the kit (rail: 21px in
// a 44px tile; pill: 22px in a 40px tile).
constexpr float kMD3RailMarkRatio = 21.0f / 44.0f;
constexpr float kMD3PillMarkRatio = 22.0f / 40.0f;
// Tile corner radius as a fraction of the tile (rail r12 on 44, pill r10 on 40).
constexpr float kMD3RailCornerRatio = 12.0f / 44.0f;
constexpr float kMD3PillCornerRatio = 10.0f / 40.0f;
// Group-divider length as a fraction of the tile (kit 28px line on a 44px rail).
constexpr float kMD3DividerRatio = 28.0f / 44.0f;

MD3ToolbarKind md3_toolbar_kind(const ToolbarLayout& layout);
float          md3_glyph_ratio(MD3ToolbarKind kind);
// Fills a token-coloured (rounded) rect in the toolbar's billboard space via the
// flat shader. r <= 0 yields a plain rect (dividers/state layers). No-op when the
// flat shader is unavailable, so any non-flat context keeps its texture path.
void md3_fill_rounded_world(float x0, float y0, float x1, float y1, float r, MD3::Role role, float alpha = 1.0f);
// Draws the pill's per-item chrome from item.render_rect (set by the caller): a
// SurfaceContainerHigh r10 hover state layer, or a token OutlineVariant divider in
// place of a SeparatorLine's SVG. Returns true when the sprite must be suppressed
// (a divider replaced it). one_px is one device pixel in world units.
bool md3_decorate_pill_item(const GLToolbarItem& item, float tile_world, float one_px);
// Draws the rail's selected-tile chrome: a Primary r12 fill plus the OnPrimary
// mark (kit selected = Primary fill + OnPrimary glyph). Returns true when it drew
// the OnPrimary mark and the atlas sprite must be suppressed; false leaves the
// atlas Primary-tinted glyph as the selected indication. tile_device is the tile
// size in device pixels (icons_size * layout scale).
bool md3_decorate_rail_item(const GLToolbar& tb, const GLToolbarItem& item, float tile_world, float glyph_ratio, float inv_zoom, float tile_device);

} // namespace

ToolbarAutoSizeRenderer::ToolbarAutoSizeRenderer()
{
}

ToolbarAutoSizeRenderer::~ToolbarAutoSizeRenderer()
{
}

void ToolbarAutoSizeRenderer::render(const GLToolbar& t_toolbar, const Camera& t_camera)
{
    const auto& t_layout = t_toolbar.get_layout();
    switch (t_layout.type)
    {
    default:
    case ToolbarLayout::EType::Horizontal: { render_horizontal(t_toolbar, t_camera); break; }
    case ToolbarLayout::EType::Vertical: { render_vertical(t_toolbar, t_camera); break; }
    }
}

GLToolbar::EToolbarRenderingMode ToolbarAutoSizeRenderer::get_mode() const
{
    return GLToolbar::EToolbarRenderingMode::Auto;
}

void ToolbarAutoSizeRenderer::render_horizontal(const GLToolbar& t_toolbar, const Camera& t_camera)
{
    const auto& t_layout = t_toolbar.get_layout();
    float inv_zoom = (float)t_camera.get_inv_zoom();
    float factor = inv_zoom * t_layout.scale;

    float scaled_icons_size = t_layout.icons_size * factor;
    float scaled_separator_size = t_layout.separator_size * factor;
    float scaled_gap_size = t_layout.gap_size * factor;
    float scaled_border = t_layout.border * factor;
    float scaled_width = t_toolbar.get_width() * inv_zoom;
    float scaled_height = t_toolbar.get_height() * inv_zoom;

    float separator_stride = scaled_separator_size + scaled_gap_size;
    float icon_stride = scaled_icons_size + scaled_gap_size;

    float left = 0.0f;
    float top = 0.0f;
    calculate_position(t_toolbar, t_camera, left, top);
    float right = left + scaled_width;
    float bottom = top - scaled_height;

    render_background(t_toolbar, left, top, right, bottom, scaled_border);

    left += scaled_border;
    top -= scaled_border;

    // MD3: the horizontal main toolbar becomes the kit pill — 40px r10 tiles with
    // 22px glyphs, a SurfaceContainerHigh hover state layer and token dividers.
    // Gated on the glyph bridge so the SVG-sprite fallback keeps filling the tile.
    const MD3ToolbarKind md3_kind = md3_toolbar_kind(t_layout);
    const bool md3_on = (md3_kind == MD3ToolbarKind::Pill) && GLIconGlyphBridge::available();
    const float md3_gr = md3_on ? md3_glyph_ratio(md3_kind) : 1.0f;

    // renders icons
    const auto& t_items = t_toolbar.get_items();
    const auto& t_icon_texture = t_toolbar.get_icon_texture();
    for (const auto& item : t_items)
    {
        if (!item->is_visible())
            continue;

        if (item->is_separator())
            left += separator_stride;
        else
        {
            if (!item->is_action_with_text_image()) {
                unsigned int tex_id = t_icon_texture.get_id();
                int tex_width = t_icon_texture.get_width();
                int tex_height = t_icon_texture.get_height();
                if ((tex_id == 0) || (tex_width <= 0) || (tex_height <= 0))
                    return;
                item->render_rect[0] = left;
                item->render_rect[1] = left + scaled_icons_size;
                item->render_rect[2] = top - scaled_icons_size;
                item->render_rect[3] = top;
                bool md3_suppress = false;
                if (md3_on)
                    md3_suppress = md3_decorate_pill_item(*item, scaled_icons_size, inv_zoom * t_layout.scale);
                item->render(tex_id, (unsigned int)tex_width, (unsigned int)tex_height, (unsigned int)(t_layout.icons_size * t_layout.scale), t_toolbar.get_height(), false, md3_gr, !md3_suppress);
            }
            //BBS: GUI refactor: GLToolbar
            if (item->is_action_with_text())
            {
                float scaled_text_size = item->get_extra_size_ratio() * scaled_icons_size;
                item->render_rect[0] = left + scaled_icons_size;
                item->render_rect[1] = left + scaled_icons_size + scaled_text_size;
                item->render_rect[2] = top - scaled_icons_size;
                item->render_rect[3] = top;
                item->render_text();
                left += scaled_text_size;
            }
            if (item->get_type() == GLToolbarItem::EType::SeparatorLine) {
                left += (icon_stride - 0.5f * scaled_icons_size);
            }
            else {
                left += icon_stride;
            }
        }
    }
}

void ToolbarAutoSizeRenderer::render_vertical(const GLToolbar& t_toolbar, const Camera& t_camera)
{
    const auto& t_layout = t_toolbar.get_layout();
    float inv_zoom = (float)wxGetApp().plater()->get_camera().get_inv_zoom();
    float factor = inv_zoom * t_layout.scale;

    float scaled_icons_size = t_layout.icons_size * factor;
    float scaled_separator_size = t_layout.separator_size * factor;
    float scaled_gap_size = t_layout.gap_size * factor;
    float scaled_border = t_layout.border * factor;
    float scaled_width = t_toolbar.get_width() * inv_zoom;
    float scaled_height = t_toolbar.get_height() * inv_zoom;

    float separator_stride = scaled_separator_size + scaled_gap_size;
    float icon_stride = scaled_icons_size + scaled_gap_size;

    float left = 0.0f;
    float top = 0.0f;
    calculate_position(t_toolbar, t_camera, left, top);
    float right = left + scaled_width;
    float bottom = top - scaled_height;

    render_background(t_toolbar, left, top, right, bottom, scaled_border);

    left += scaled_border;
    top -= scaled_border;

    // MD3: the vertical gizmo rail becomes the kit rail — 44px r12 tiles, 21px
    // glyphs, a Primary-filled + OnPrimary selected tile and 28px OutlineVariant
    // group dividers. Gated on the glyph bridge so the SVG fallback is untouched.
    const MD3ToolbarKind md3_kind = md3_toolbar_kind(t_layout);
    const bool md3_on = (md3_kind == MD3ToolbarKind::Rail) && GLIconGlyphBridge::available();
    const float md3_gr = md3_on ? md3_glyph_ratio(md3_kind) : 1.0f;
    const float md3_one_px = inv_zoom * t_layout.scale;
    const float md3_tile_device = t_layout.icons_size * t_layout.scale;

    // renders icons
    const auto& t_items = t_toolbar.get_items();
    const auto& t_icon_texture = t_toolbar.get_icon_texture();
    for (const auto& item : t_items) {
        if (!item->is_visible())
            continue;

        if (item->is_separator()) {
            if (md3_on) {
                // 28px OutlineVariant divider centred in the separator band.
                const float cx = left + 0.5f * scaled_icons_size;
                const float cy = top - 0.5f * separator_stride;
                const float half_w = 0.5f * kMD3DividerRatio * scaled_icons_size;
                const float half_h = 0.5f * std::max(md3_one_px, (1.5f / 44.0f) * scaled_icons_size);
                md3_fill_rounded_world(cx - half_w, cy - half_h, cx + half_w, cy + half_h, 0.0f, MD3::Role::OutlineVariant);
            }
            top -= separator_stride;
        }
        else {
            unsigned int tex_id;
            int tex_width, tex_height;
            if (item->is_action_with_text_image()) {
                float scaled_text_size = t_layout.text_size * factor;
                float scaled_text_width = item->get_extra_size_ratio() * scaled_icons_size;
                float scaled_text_border = 2.5 * factor;
                float scaled_text_height = scaled_icons_size / 2.0f;
                item->render_rect[0] = left;
                item->render_rect[1] = left + scaled_text_size;
                item->render_rect[2] = top - scaled_text_border - scaled_text_height;
                item->render_rect[3] = top - scaled_text_border;
                item->render_text();

                float image_left = left + scaled_text_size;
                const auto& item_data = item->get_data();
                tex_id = item_data.image_texture.get_id();
                tex_width = item_data.image_texture.get_width();
                tex_height = item_data.image_texture.get_height();
                if ((tex_id == 0) || (tex_width <= 0) || (tex_height <= 0))
                    return;
                item->render_image(tex_id, image_left, image_left + scaled_icons_size, top - scaled_icons_size, top, (unsigned int)tex_width, (unsigned int)tex_height, (unsigned int)(t_layout.icons_size * t_layout.scale));
            }
            else {
                tex_id = t_icon_texture.get_id();
                tex_width = t_icon_texture.get_width();
                tex_height = t_icon_texture.get_height();
                if ((tex_id == 0) || (tex_width <= 0) || (tex_height <= 0))
                    return;
                item->render_rect[0] = left;
                item->render_rect[1] = left + scaled_icons_size;
                item->render_rect[2] = top - scaled_icons_size;
                item->render_rect[3] = top;
                bool md3_suppress = false;
                if (md3_on)
                    md3_suppress = md3_decorate_rail_item(t_toolbar, *item, scaled_icons_size, md3_gr, inv_zoom, md3_tile_device);
                item->render(tex_id, (unsigned int)tex_width, (unsigned int)tex_height, (unsigned int)(t_layout.icons_size * t_layout.scale), t_toolbar.get_width(), false, md3_gr, !md3_suppress);
                //BBS: GUI refactor: GLToolbar
            }
            if (item->is_action_with_text())
            {
                float scaled_text_width = item->get_extra_size_ratio() * scaled_icons_size;
                float scaled_text_height = scaled_icons_size;

                item->render_rect[0] = left + scaled_icons_size;
                item->render_rect[1] = left + scaled_icons_size + scaled_text_width;
                item->render_rect[2] = top - scaled_text_height;
                item->render_rect[3] = top;
                item->render_text();
            }
            if (item->get_type() == GLToolbarItem::EType::SeparatorLine) {
                top -= (icon_stride - 0.5f * scaled_icons_size);
            }
            else {
                top -= icon_stride;
            }
        }
    }
}

// --- MD3 viewport-toolbar background chrome ------------------------------------
// The two migrated OpenGL viewport toolbars — the left gizmo rail and the top
// scene toolbar — resolve their icons to Material Symbols (see GLIconGlyphBridge)
// but still painted the legacy 9-slice toolbar_background.png behind them. These
// helpers paint the kit background instead: a SurfaceContainerLow rail with a 1px
// OutlineVariant right divider, and a rounded SurfaceContainer pill with a 1px
// OutlineVariant border + soft elevation. The flat_texture shader carries no tint
// uniform, so the chrome is drawn as token-coloured geometry through the "flat"
// shader in the same billboard space the textures used. Every other toolbar and
// any context without the shader keeps its texture background untouched.
namespace {

constexpr float kMD3HalfPi = 1.57079632679f;

inline ColorRGBA md3_toolbar_color(MD3::Role role, float alpha = 1.0f)
{
    const wxColour& c = MD3::resolve(role, wxGetApp().dark_mode());
    return ColorRGBA(c.Red() / 255.0f, c.Green() / 255.0f, c.Blue() / 255.0f, alpha);
}

// Append a filled axis-aligned rectangle (two triangles, z = 0) to a P3 geometry.
void md3_append_rect(GLModel::Geometry& geo, float x0, float y0, float x1, float y1)
{
    const unsigned int base = static_cast<unsigned int>(geo.vertices_count());
    geo.add_vertex(Vec3f(x0, y0, 0.0f));
    geo.add_vertex(Vec3f(x1, y0, 0.0f));
    geo.add_vertex(Vec3f(x1, y1, 0.0f));
    geo.add_vertex(Vec3f(x0, y1, 0.0f));
    geo.add_triangle(base + 0, base + 1, base + 2);
    geo.add_triangle(base + 0, base + 2, base + 3);
}

// Append a filled rounded rectangle (three centre bars + four corner fans).
void md3_append_rounded_rect(GLModel::Geometry& geo, float x0, float y0, float x1, float y1, float r)
{
    const float w = x1 - x0;
    const float h = y1 - y0;
    if (w <= 0.0f || h <= 0.0f)
        return;
    r = std::min(r, 0.5f * std::min(w, h));
    if (r <= 0.0f) {
        md3_append_rect(geo, x0, y0, x1, y1);
        return;
    }
    md3_append_rect(geo, x0, y0 + r, x1, y1 - r);     // middle band (full width)
    md3_append_rect(geo, x0 + r, y0, x1 - r, y0 + r); // bottom edge
    md3_append_rect(geo, x0 + r, y1 - r, x1 - r, y1); // top edge

    const struct { float cx, cy, a0; } corners[4] = {
        { x1 - r, y0 + r, -kMD3HalfPi }, // bottom-right
        { x1 - r, y1 - r, 0.0f },        // top-right
        { x0 + r, y1 - r, kMD3HalfPi },  // top-left
        { x0 + r, y0 + r, 2.0f * kMD3HalfPi }, // bottom-left
    };
    const int seg = 6;
    for (const auto& c : corners) {
        const unsigned int center = static_cast<unsigned int>(geo.vertices_count());
        geo.add_vertex(Vec3f(c.cx, c.cy, 0.0f));
        for (int i = 0; i <= seg; ++i) {
            const float a = c.a0 + kMD3HalfPi * (static_cast<float>(i) / static_cast<float>(seg));
            geo.add_vertex(Vec3f(c.cx + r * std::cos(a), c.cy + r * std::sin(a), 0.0f));
        }
        for (int i = 0; i < seg; ++i)
            geo.add_triangle(center, center + 1 + i, center + 2 + i);
    }
}

// Classify a toolbar as one of the two MD3-migrated viewport toolbars. The left
// gizmo rail is the only vertical toolbar centred on both axes (the collapse rail
// is HO_Right/VO_Top, so it is left untouched); the top scene toolbar is the
// horizontal main toolbar. Every other toolbar keeps its legacy rendering.
MD3ToolbarKind md3_toolbar_kind(const ToolbarLayout& layout)
{
    if (layout.type == ToolbarLayout::EType::Vertical
        && layout.horizontal_orientation == ToolbarLayout::HO_Center
        && layout.vertical_orientation == ToolbarLayout::VO_Center)
        return MD3ToolbarKind::Rail;
    if (layout.type == ToolbarLayout::EType::Horizontal)
        return MD3ToolbarKind::Pill;
    return MD3ToolbarKind::None;
}

// Fraction of the tile occupied by the atlas sprite quad so the visible mark lands
// at the kit size (mark = fill * quad; quad = markRatio / fill of the tile).
float md3_glyph_ratio(MD3ToolbarKind kind)
{
    switch (kind) {
    case MD3ToolbarKind::Rail: return kMD3RailMarkRatio / kMD3AtlasGlyphFill;
    case MD3ToolbarKind::Pill: return kMD3PillMarkRatio / kMD3AtlasGlyphFill;
    default:                   return 1.0f;
    }
}

void md3_fill_rounded_world(float x0, float y0, float x1, float y1, float r, MD3::Role role, float alpha)
{
    const auto& shader = wxGetApp().get_shader("flat");
    if (!shader)
        return;
    const float lo_x = std::min(x0, x1), hi_x = std::max(x0, x1);
    const float lo_y = std::min(y0, y1), hi_y = std::max(y0, y1);
    if (hi_x - lo_x <= 0.0f || hi_y - lo_y <= 0.0f)
        return;
    GLModel::Geometry g;
    g.format.type = GLModel::PrimitiveType::Triangles;
    g.format.vertex_layout = GLModel::Geometry::EVertexLayout::P3;
    md3_append_rounded_rect(g, lo_x, lo_y, hi_x, hi_y, r);
    if (g.vertices_count() == 0)
        return;
    const Camera& camera = wxGetApp().plater()->get_camera();
    glsafe(::glEnable(GL_BLEND));
    glsafe(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    wxGetApp().bind_shader(shader);
    shader->set_uniform("view_model_matrix", camera.get_view_matrix_for_billboard());
    shader->set_uniform("projection_matrix", camera.get_projection_matrix());
    GLModel model;
    model.init_from(std::move(g));
    model.set_color(md3_toolbar_color(role, alpha));
    model.render_geometry();
    wxGetApp().unbind_shader();
    glsafe(::glDisable(GL_BLEND));
}

bool md3_decorate_pill_item(const GLToolbarItem& item, float tile_world, float one_px)
{
    if (item.get_type() == GLToolbarItem::EType::SeparatorLine) {
        // Vertical group divider (kit 1px OutlineVariant rule) instead of the SVG.
        const float cx = 0.5f * (item.render_rect[0] + item.render_rect[1]);
        const float cy = 0.5f * (item.render_rect[2] + item.render_rect[3]);
        const float half_h = 0.5f * kMD3DividerRatio * tile_world;
        const float half_w = 0.5f * std::max(one_px, (1.5f / 44.0f) * tile_world);
        md3_fill_rounded_world(cx - half_w, cy - half_h, cx + half_w, cy + half_h, 0.0f, MD3::Role::OutlineVariant);
        return true;
    }
    if (item.is_hovered() && !item.is_disabled()) {
        // SurfaceContainerHigh hover state layer (kit IconButton hover), r10.
        const float r = kMD3PillCornerRatio * tile_world;
        md3_fill_rounded_world(item.render_rect[0], item.render_rect[2], item.render_rect[1], item.render_rect[3], r, MD3::Role::SurfaceContainerHigh);
    }
    return false;
}

bool md3_decorate_rail_item(const GLToolbar& tb, const GLToolbarItem& item, float tile_world, float glyph_ratio, float inv_zoom, float tile_device)
{
    if (!item.is_pressed())
        return false;
    const uint32_t cp = GLIconGlyphBridge::glyph_for_toolbar_item(item.get_name());
    if (cp == 0)
        return false; // unmapped: keep the atlas Primary glyph as the selected mark
    int gw = 0, gh = 0;
    const int gpx = std::max(1, static_cast<int>(std::lround(kMD3AtlasGlyphFill * tile_device)));
    const unsigned int gtex = tb.ensure_selected_glyph_texture(cp, gpx, &gw, &gh);
    if (gtex == 0 || gw <= 0 || gh <= 0)
        return false; // raster failed: fall back to the atlas Primary glyph
    // Primary r12 fill, then the OnPrimary mark at the same footprint as the atlas
    // idle glyphs (the sprite renders at glyph_ratio of its native pixels).
    const float r = kMD3RailCornerRatio * tile_world;
    md3_fill_rounded_world(item.render_rect[0], item.render_rect[2], item.render_rect[1], item.render_rect[3], r, MD3::Role::Primary);
    const float cx = 0.5f * (item.render_rect[0] + item.render_rect[1]);
    const float cy = 0.5f * (item.render_rect[2] + item.render_rect[3]);
    const float half_w = 0.5f * static_cast<float>(gw) * glyph_ratio * inv_zoom;
    const float half_h = 0.5f * static_cast<float>(gh) * glyph_ratio * inv_zoom;
    GLTexture::render_sub_texture(gtex, cx - half_w, cx + half_w, cy - half_h, cy + half_h, GLTexture::FullTextureUVs);
    return true;
}

// Paint the MD3 background for the gizmo rail / scene toolbar. Returns true when
// it drew the chrome (the caller then skips the legacy texture); false for any
// other toolbar or when the flat shader is unavailable (legacy path preserved).
bool render_md3_toolbar_backdrop(const GLToolbar& t_toolbar, float left, float top, float right, float bottom)
{
    const ToolbarLayout& layout = t_toolbar.get_layout();
    const MD3ToolbarKind kind = md3_toolbar_kind(layout);
    const bool is_rail = kind == MD3ToolbarKind::Rail;
    const bool is_pill = kind == MD3ToolbarKind::Pill;
    if (!is_rail && !is_pill)
        return false;

    const auto& shader = wxGetApp().get_shader("flat");
    if (!shader)
        return false;

    const Camera& camera = wxGetApp().plater()->get_camera();
    const float inv_zoom = static_cast<float>(camera.get_inv_zoom());
    const float px = inv_zoom * layout.scale; // one device-independent pixel in world units
    if (!(px > 0.0f))
        return false;

    const float x0 = std::min(left, right);
    const float x1 = std::max(left, right);
    const float y0 = std::min(top, bottom);
    const float y1 = std::max(top, bottom);
    if (x1 - x0 <= 0.0f || y1 - y0 <= 0.0f)
        return false;

    glsafe(::glEnable(GL_BLEND));
    glsafe(::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    wxGetApp().bind_shader(shader);
    shader->set_uniform("view_model_matrix", camera.get_view_matrix_for_billboard());
    shader->set_uniform("projection_matrix", camera.get_projection_matrix());

    auto draw = [](GLModel::Geometry&& g, const ColorRGBA& col) {
        if (g.vertices_count() == 0)
            return;
        GLModel model;
        model.init_from(std::move(g));
        model.set_color(col);
        model.render_geometry();
    };
    auto new_geo = []() {
        GLModel::Geometry g;
        g.format.type = GLModel::PrimitiveType::Triangles;
        g.format.vertex_layout = GLModel::Geometry::EVertexLayout::P3;
        return g;
    };

    if (is_rail) {
        GLModel::Geometry fill = new_geo();
        md3_append_rect(fill, x0, y0, x1, y1);
        draw(std::move(fill), md3_toolbar_color(MD3::Role::SurfaceContainerLow));

        GLModel::Geometry edge = new_geo();
        md3_append_rect(edge, x1 - px, y0, x1, y1);
        draw(std::move(edge), md3_toolbar_color(MD3::Role::OutlineVariant));
    }
    else {
        const float r = 16.0f * px; // r16 pill
        // Soft elevation-3 shadow, feathered across a few translucent layers so the
        // hard-edged fill approximates the kit's blurred drop shadow.
        const wxColour& sh = MD3::shadowTint(wxGetApp().dark_mode());
        const float sh_dy = static_cast<float>(MD3::Metrics::elev3.dy) * px;
        const float base_a = (sh.Alpha() / 255.0f) * 0.5f;
        for (int i = 3; i >= 1; --i) {
            const float grow = static_cast<float>(i) * px;
            GLModel::Geometry s = new_geo();
            md3_append_rounded_rect(s, x0 - grow, y0 - sh_dy - grow, x1 + grow, y1 - sh_dy + grow, r + grow);
            draw(std::move(s), ColorRGBA(sh.Red() / 255.0f, sh.Green() / 255.0f, sh.Blue() / 255.0f, base_a / static_cast<float>(i)));
        }

        GLModel::Geometry border = new_geo();
        md3_append_rounded_rect(border, x0, y0, x1, y1, r);
        draw(std::move(border), md3_toolbar_color(MD3::Role::OutlineVariant));

        GLModel::Geometry fill = new_geo();
        md3_append_rounded_rect(fill, x0 + px, y0 + px, x1 - px, y1 - px, r - px);
        draw(std::move(fill), md3_toolbar_color(MD3::Role::SurfaceContainer));
    }

    wxGetApp().unbind_shader();
    glsafe(::glDisable(GL_BLEND));
    return true;
}

} // namespace

void ToolbarAutoSizeRenderer::render_background(const GLToolbar& t_toolbar, float left, float top, float right, float bottom, float border) const
{
    if (render_md3_toolbar_backdrop(t_toolbar, left, top, right, bottom))
        return;

    const auto& t_background_texture = t_toolbar.get_background_texture();
    unsigned int tex_id = t_background_texture.texture.get_id();
    float tex_width = (float)t_background_texture.texture.get_width();
    float tex_height = (float)t_background_texture.texture.get_height();
    if ((tex_id != 0) && (tex_width > 0) && (tex_height > 0))
    {
        float inv_tex_width = (tex_width != 0.0f) ? 1.0f / tex_width : 0.0f;
        float inv_tex_height = (tex_height != 0.0f) ? 1.0f / tex_height : 0.0f;

        float internal_left = left + border;
        float internal_right = right - border;
        float internal_top = top - border;
        float internal_bottom = bottom + border;

        float left_uv = 0.0f;
        float right_uv = 1.0f;
        float top_uv = 1.0f;
        float bottom_uv = 0.0f;

        float internal_left_uv = (float)t_background_texture.metadata.left * inv_tex_width;
        float internal_right_uv = 1.0f - (float)t_background_texture.metadata.right * inv_tex_width;
        float internal_top_uv = 1.0f - (float)t_background_texture.metadata.top * inv_tex_height;
        float internal_bottom_uv = (float)t_background_texture.metadata.bottom * inv_tex_height;

        const auto& t_layout = t_toolbar.get_layout();
        // top-left corner
        if ((t_layout.horizontal_orientation == ToolbarLayout::HO_Left) || (t_layout.vertical_orientation == ToolbarLayout::VO_Top))
            GLTexture::render_sub_texture(tex_id, left, internal_left, internal_top, top, { { internal_left_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_right_uv, internal_top_uv }, { internal_left_uv, internal_top_uv } });
        else
            GLTexture::render_sub_texture(tex_id, left, internal_left, internal_top, top, { { left_uv, internal_top_uv }, { internal_left_uv, internal_top_uv }, { internal_left_uv, top_uv }, { left_uv, top_uv } });

        // top edge
        if (t_layout.vertical_orientation == ToolbarLayout::VO_Top)
            GLTexture::render_sub_texture(tex_id, internal_left, internal_right, internal_top, top, { { internal_left_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_right_uv, internal_top_uv }, { internal_left_uv, internal_top_uv } });
        else
            GLTexture::render_sub_texture(tex_id, internal_left, internal_right, internal_top, top, { { internal_left_uv, internal_top_uv }, { internal_right_uv, internal_top_uv }, { internal_right_uv, top_uv }, { internal_left_uv, top_uv } });

        // top-right corner
        if ((t_layout.horizontal_orientation == ToolbarLayout::HO_Right) || (t_layout.vertical_orientation == ToolbarLayout::VO_Top))
            GLTexture::render_sub_texture(tex_id, internal_right, right, internal_top, top, { { internal_left_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_right_uv, internal_top_uv }, { internal_left_uv, internal_top_uv } });
        else
            GLTexture::render_sub_texture(tex_id, internal_right, right, internal_top, top, { { internal_right_uv, internal_top_uv }, { right_uv, internal_top_uv }, { right_uv, top_uv }, { internal_right_uv, top_uv } });

        // center-left edge
        if (t_layout.horizontal_orientation == ToolbarLayout::HO_Left)
            GLTexture::render_sub_texture(tex_id, left, internal_left, internal_bottom, internal_top, { { internal_left_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_right_uv, internal_top_uv }, { internal_left_uv, internal_top_uv } });
        else
            GLTexture::render_sub_texture(tex_id, left, internal_left, internal_bottom, internal_top, { { left_uv, internal_bottom_uv }, { internal_left_uv, internal_bottom_uv }, { internal_left_uv, internal_top_uv }, { left_uv, internal_top_uv } });

        // center
        GLTexture::render_sub_texture(tex_id, internal_left, internal_right, internal_bottom, internal_top, { { internal_left_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_right_uv, internal_top_uv }, { internal_left_uv, internal_top_uv } });

        // center-right edge
        if (t_layout.horizontal_orientation == ToolbarLayout::HO_Right)
            GLTexture::render_sub_texture(tex_id, internal_right, right, internal_bottom, internal_top, { { internal_left_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_right_uv, internal_top_uv }, { internal_left_uv, internal_top_uv } });
        else
            GLTexture::render_sub_texture(tex_id, internal_right, right, internal_bottom, internal_top, { { internal_right_uv, internal_bottom_uv }, { right_uv, internal_bottom_uv }, { right_uv, internal_top_uv }, { internal_right_uv, internal_top_uv } });

        // bottom-left corner
        if ((t_layout.horizontal_orientation == ToolbarLayout::HO_Left) || (t_layout.vertical_orientation == ToolbarLayout::VO_Bottom))
            GLTexture::render_sub_texture(tex_id, left, internal_left, bottom, internal_bottom, { { internal_left_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_right_uv, internal_top_uv }, { internal_left_uv, internal_top_uv } });
        else
            GLTexture::render_sub_texture(tex_id, left, internal_left, bottom, internal_bottom, { { left_uv, bottom_uv }, { internal_left_uv, bottom_uv }, { internal_left_uv, internal_bottom_uv }, { left_uv, internal_bottom_uv } });

        // bottom edge
        if (t_layout.vertical_orientation == ToolbarLayout::VO_Bottom)
            GLTexture::render_sub_texture(tex_id, internal_left, internal_right, bottom, internal_bottom, { { internal_left_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_right_uv, internal_top_uv }, { internal_left_uv, internal_top_uv } });
        else
            GLTexture::render_sub_texture(tex_id, internal_left, internal_right, bottom, internal_bottom, { { internal_left_uv, bottom_uv }, { internal_right_uv, bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_left_uv, internal_bottom_uv } });

        // bottom-right corner
        if ((t_layout.horizontal_orientation == ToolbarLayout::HO_Right) || (t_layout.vertical_orientation == ToolbarLayout::VO_Bottom))
            GLTexture::render_sub_texture(tex_id, internal_right, right, bottom, internal_bottom, { { internal_left_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_right_uv, internal_top_uv }, { internal_left_uv, internal_top_uv } });
        else
            GLTexture::render_sub_texture(tex_id, internal_right, right, bottom, internal_bottom, { { internal_right_uv, bottom_uv }, { right_uv, bottom_uv }, { right_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv } });
    }
}

void ToolbarAutoSizeRenderer::calculate_position(const GLToolbar& t_toolbar, const Camera& t_camera, float& left, float& top)
{
    const auto& t_layout = t_toolbar.get_layout();
    switch (t_layout.position_mode) {
    case ToolbarLayout::EPositionMode::TopLeft:
    {
        float inv_zoom = (float)t_camera.get_inv_zoom();
        const auto& t_viewport = t_camera.get_viewport();
        top = 0.5f * (float)t_viewport[3] * inv_zoom;
        left = -0.5f * (float)t_viewport[2] * inv_zoom;

        break;
    }
    case ToolbarLayout::EPositionMode::TopMiddle:
    {
        float inv_zoom = (float)t_camera.get_inv_zoom();
        const auto& t_viewport = t_camera.get_viewport();
        const auto cnv_width = t_viewport[2];
        const auto cnv_height = t_viewport[3];
        top = 0.5f * (float)cnv_height * inv_zoom;
        left = -0.5f * (float)cnv_width * inv_zoom;
        const auto final_width = t_toolbar.get_width() + t_layout.offset;
        if (cnv_width < final_width) {
            left += (t_layout.offset * inv_zoom);
        }
        else {
            const float offset = (cnv_width - final_width) / 2.f;
            left += (offset + t_layout.offset) * inv_zoom;
        }

        break;
    }
    case ToolbarLayout::EPositionMode::Custom:
    default:
    {
        left = t_layout.left;
        top = t_layout.top;
        break;
    }
    }
}

void ToolbarAutoSizeRenderer::render_arrow(const GLToolbar& t_toolbar, const std::weak_ptr<GLToolbarItem>& highlighted_item)
{
    const auto p_item = highlighted_item.lock();
    if (!p_item) {
        return;
    }
    const auto& t_arrow_texture = t_toolbar.get_arrow_texture();
    // arrow texture not initialized
    if (t_arrow_texture.texture.get_id() == 0)
        return;

    const auto& t_layout = t_toolbar.get_layout();
    float inv_zoom = (float)wxGetApp().plater()->get_camera().get_inv_zoom();
    float factor = inv_zoom * t_layout.scale;

    float scaled_icons_size = t_layout.icons_size * factor;
    float scaled_separator_size = t_layout.separator_size * factor;
    float scaled_gap_size = t_layout.gap_size * factor;
    float border = t_layout.border * factor;

    float separator_stride = scaled_separator_size + scaled_gap_size;
    float icon_stride = scaled_icons_size + scaled_gap_size;

    float left = t_layout.left;
    float top = t_layout.top - icon_stride;

    bool found = false;
    const auto& t_items = t_toolbar.get_items();
    for (const auto& item : t_items) {
        if (!item->is_visible())
            continue;

        if (item->is_separator())
            left += separator_stride;
        else {
            if (item->get_name() == p_item->get_name()) {
                found = true;
                break;
            }
            if (item->get_type() == GLToolbarItem::EType::SeparatorLine) {
                left += (icon_stride - 0.5f * scaled_icons_size);
            }
            else {
                left += icon_stride;
            }
        }
    }
    if (!found)
        return;

    left += border;
    top -= separator_stride;
    float right = left + scaled_icons_size;

    unsigned int tex_id = t_arrow_texture.texture.get_id();
    // arrow width and height
    float arr_tex_width = (float)t_arrow_texture.texture.get_width();
    float arr_tex_height = (float)t_arrow_texture.texture.get_height();
    if ((tex_id != 0) && (arr_tex_width > 0) && (arr_tex_height > 0)) {
        float inv_tex_width = (arr_tex_width != 0.0f) ? 1.0f / arr_tex_width : 0.0f;
        float inv_tex_height = (arr_tex_height != 0.0f) ? 1.0f / arr_tex_height : 0.0f;

        float internal_left = left + border - scaled_icons_size * 1.5f; // add scaled_icons_size for huge arrow
        float internal_right = right - border + scaled_icons_size * 1.5f;
        float internal_top = top - border;
        // bottom is not moving and should be calculated from arrow texture sides ratio
        float arrow_sides_ratio = (float)t_arrow_texture.texture.get_height() / (float)t_arrow_texture.texture.get_width();
        float internal_bottom = internal_top - (internal_right - internal_left) * arrow_sides_ratio;

        float internal_left_uv = (float)t_arrow_texture.metadata.left * inv_tex_width;
        float internal_right_uv = 1.0f - (float)t_arrow_texture.metadata.right * inv_tex_width;
        float internal_top_uv = 1.0f - (float)t_arrow_texture.metadata.top * inv_tex_height;
        float internal_bottom_uv = (float)t_arrow_texture.metadata.bottom * inv_tex_height;

        GLTexture::render_sub_texture(tex_id, internal_left, internal_right, internal_bottom, internal_top, { { internal_left_uv, internal_top_uv }, { internal_right_uv, internal_top_uv }, { internal_right_uv, internal_bottom_uv }, { internal_left_uv, internal_bottom_uv } });
    }
}

ToolbarKeepSizeRenderer::ToolbarKeepSizeRenderer()
{
}

ToolbarKeepSizeRenderer::~ToolbarKeepSizeRenderer()
{
}

void ToolbarKeepSizeRenderer::render(const GLToolbar& t_toolbar, const Camera& t_camera)
{
    const auto& t_layout = t_toolbar.get_layout();

    const auto& t_viewport = t_camera.get_viewport();
    const auto canvas_width = t_viewport[2];
    const auto canvas_height = t_viewport[3];

    const auto toolbar_width = t_toolbar.get_width();
    const auto toolbar_height = t_toolbar.get_height();
    const auto toolbar_offset = t_layout.offset >= canvas_width ? 0.0f : t_layout.offset;

    float final_toolbar_width = toolbar_width;
    float final_toolbar_height = toolbar_height;

    const auto final_canvas_width = canvas_width - toolbar_offset;

    float collapse_width = 0.0f;
    recalculate_item_pos(t_toolbar, t_camera, final_toolbar_width, final_toolbar_height, collapse_width);

    if (t_toolbar.is_collapsed()) {
        final_toolbar_height = toolbar_height;
    }
    float inv_zoom = (float)t_camera.get_inv_zoom();
    float factor = inv_zoom * t_layout.scale;
    float scaled_border = t_layout.border * factor;

    float left = 0.0f;
    float top = 0.0f;
    calculate_position(t_toolbar, t_camera, final_toolbar_width, final_toolbar_height, left, top);

    if (t_layout.collapsed_offset < 1e-6f || t_toolbar.is_collapsed()) {
        float scaled_width = final_toolbar_width * inv_zoom;
        float scaled_height = final_toolbar_height * inv_zoom;
        float right = left + scaled_width;
        float bottom = top - scaled_height;

        render_background(t_toolbar, left, top, right, bottom, scaled_border);
    }
    else {
        float scaled_width = final_toolbar_width * inv_zoom;
        float scaled_height = toolbar_height * inv_zoom;
        float right = left + scaled_width;
        float bottom = top - scaled_height;

        render_background(t_toolbar, left, top, right, bottom, scaled_border);

        const auto others_height = final_toolbar_height - toolbar_height;
        if (others_height > 1e-6f) {
            scaled_width = collapse_width * inv_zoom;
            right = left + scaled_width;
            top = bottom - t_layout.collapsed_offset * factor;
            scaled_height = others_height * inv_zoom;
            bottom = top - scaled_height;
            render_background(t_toolbar, left, top, right, bottom, scaled_border);
        }
    }

    // renders icons
    const auto& t_icon_texture = t_toolbar.get_icon_texture();
    unsigned int tex_id = t_icon_texture.get_id();
    int tex_width = t_icon_texture.get_width();
    int tex_height = t_icon_texture.get_height();
    if ((tex_id == 0) || (tex_width <= 0) || (tex_height <= 0))
        return;

    // MD3: same kit-pill treatment as the auto-size horizontal path (22px glyphs,
    // SurfaceContainerHigh hover state layer, token dividers). KeepSize is the
    // default main-toolbar mode. Collapsed items keep the legacy sprite (their
    // stacked override_render_rect is not a plain tile).
    const MD3ToolbarKind md3_kind = md3_toolbar_kind(t_layout);
    const bool md3_on = (md3_kind == MD3ToolbarKind::Pill) && GLIconGlyphBridge::available();
    const float md3_gr = md3_on ? md3_glyph_ratio(md3_kind) : 1.0f;
    const float md3_one_px = inv_zoom * t_layout.scale;
    const float md3_tile_world = t_layout.icons_size * factor;

    const auto& t_items = t_toolbar.get_items();
    for (size_t i = 0; i < t_items.size(); ++i) {
        const auto& current_item = t_items[i];
        current_item->override_render_rect = m_p_override_render_rect;
        if (current_item->is_action() || current_item->get_type() == GLToolbarItem::EType::SeparatorLine) {
            const bool b_filp_v = !t_toolbar.is_collapsed() && current_item->is_collapse_button();
            bool md3_suppress = false;
            if (md3_on && !current_item->is_collapsed())
                md3_suppress = md3_decorate_pill_item(*current_item, md3_tile_world, md3_one_px);
            current_item->render(tex_id, (unsigned int)tex_width, (unsigned int)tex_height, (unsigned int)(t_layout.icons_size * t_layout.scale), t_toolbar.get_height(), b_filp_v, md3_gr, !md3_suppress);
        }
        //BBS: GUI refactor: GLToolbar
        if (current_item->is_action_with_text())
        {
            current_item->render_text();
        }
    }
}

GLToolbar::EToolbarRenderingMode ToolbarKeepSizeRenderer::get_mode() const
{
    return GLToolbar::EToolbarRenderingMode::KeepSize;
}

void ToolbarKeepSizeRenderer::render_arrow(const GLToolbar& t_toolbar, const std::weak_ptr<GLToolbarItem>& highlighted_item)
{
}

bool ToolbarKeepSizeRenderer::needs_collapsed() const
{
    return m_b_needs_collapsed;
}

void ToolbarKeepSizeRenderer::render_horizontal(const GLToolbar& t_toolbar)
{
}

void ToolbarKeepSizeRenderer::render_vertical(const GLToolbar& t_toolbar)
{
}

void ToolbarKeepSizeRenderer::render_background(const GLToolbar& t_toolbar, float left, float top, float right, float bottom, float border) const
{
    if (render_md3_toolbar_backdrop(t_toolbar, left, top, right, bottom))
        return;

    const auto& t_background_texture = t_toolbar.get_background_texture();
    unsigned int tex_id = t_background_texture.texture.get_id();
    if (tex_id < 0) {
        return;
    }

    float tex_width = (float)t_background_texture.texture.get_width();
    if (tex_width <= 0) {
        return;
    }

    float tex_height = (float)t_background_texture.texture.get_height();
    if (tex_height <= 0) {
        return;
    }

    float inv_tex_width = (tex_width != 0.0f) ? 1.0f / tex_width : 0.0f;
    float inv_tex_height = (tex_height != 0.0f) ? 1.0f / tex_height : 0.0f;

    float internal_left = left + border;
    float internal_right = right - border;
    float internal_top = top - border;
    float internal_bottom = bottom + border;

    float left_uv = 0.0f;
    float right_uv = 1.0f;
    float top_uv = 1.0f;
    float bottom_uv = 0.0f;

    float internal_left_uv = (float)t_background_texture.metadata.left * inv_tex_width;
    float internal_right_uv = 1.0f - (float)t_background_texture.metadata.right * inv_tex_width;
    float internal_top_uv = 1.0f - (float)t_background_texture.metadata.top * inv_tex_height;
    float internal_bottom_uv = (float)t_background_texture.metadata.bottom * inv_tex_height;

    const auto& t_layout = t_toolbar.get_layout();
    GLTexture::render_sub_texture(tex_id, left, right, bottom, top, { { internal_left_uv, internal_bottom_uv }, { internal_right_uv, internal_bottom_uv }, { internal_right_uv, internal_top_uv }, { internal_left_uv, internal_top_uv } });
}

void ToolbarKeepSizeRenderer::calculate_position(const GLToolbar& t_toolbar, const Camera& t_camera, float toolbar_width, float toolbar_height, float& left, float& top)
{
    const auto& t_layout = t_toolbar.get_layout();
    switch (t_layout.position_mode) {
    case ToolbarLayout::EPositionMode::TopLeft:
    {
        float inv_zoom = (float)t_camera.get_inv_zoom();
        const auto& t_viewport = t_camera.get_viewport();
        top = 0.5f * (float)t_viewport[3] * inv_zoom;
        left = -0.5f * (float)t_viewport[2] * inv_zoom;

        break;
    }
    case ToolbarLayout::EPositionMode::TopMiddle:
    {
        float inv_zoom = (float)t_camera.get_inv_zoom();
        const auto& t_viewport = t_camera.get_viewport();
        const auto cnv_width = t_viewport[2];
        const auto cnv_height = t_viewport[3];
        top = 0.5f * (float)cnv_height * inv_zoom;
        left = -0.5f * (float)cnv_width * inv_zoom;
        const auto final_width = toolbar_width + t_layout.offset;
        if (cnv_width < final_width) {
            left += (t_layout.offset * inv_zoom);
        }
        else {
            const float offset = (cnv_width - final_width) / 2.f;
            left += (offset + t_layout.offset) * inv_zoom;
        }

        break;
    }
    case ToolbarLayout::EPositionMode::Custom:
    default:
    {
        left = t_layout.left;
        top = t_layout.top;
        break;
    }
    }
}

void ToolbarKeepSizeRenderer::recalculate_item_pos(const GLToolbar& t_toolbar, const Camera& t_camera, float& final_toolbar_width, float& final_toolbar_height, float& collapse_width)
{
    const auto& t_layout = t_toolbar.get_layout();

    const auto& t_viewport = t_camera.get_viewport();
    const auto canvas_width = t_viewport[2];
    const auto canvas_height = t_viewport[3];

    const auto toolbar_width = t_toolbar.get_width();
    const auto toolbar_height = t_toolbar.get_height();
    const auto toolbar_offset = t_layout.offset >= canvas_width ? 0.0f : t_layout.offset;

    const auto final_canvas_width = canvas_width - toolbar_offset;

    m_b_needs_collapsed = false;
    const auto& t_items = t_toolbar.get_items();
    m_indices_to_draw.clear();

    float total_collapse_item_width = 2.0f * t_layout.border;
    for (size_t i = 0; i < t_items.size(); ++i)
    {
        const auto& current_item = t_items[i];

        if (!current_item) {
            continue;
        }

        if (!current_item->is_visible())
            continue;

        current_item->set_collapsed(false);
        m_indices_to_draw.emplace_back(i);
    }
    
    float collapse_button_width = 0.0f;
    
    if (final_canvas_width - toolbar_width < 1e-6f) {
        float current_width = 2.0f * t_layout.border;
        float uncollapsible_width = 2.0f * t_layout.border;
        std::vector<size_t> t_other_visible_indices;
        std::vector<size_t> t_uncollapsible_indices;
        t_other_visible_indices.reserve(10);
        t_uncollapsible_indices.reserve(10);
        for (size_t i = 0; i < m_indices_to_draw.size(); ++i)
        {
            const auto& current_index = m_indices_to_draw[i];
            const auto& current_item = t_items[current_index];

            if (!current_item) {
                continue;
            }

            if (!current_item->is_visible())
                continue;

            if (current_item->is_collapsible()) {
                t_other_visible_indices.emplace_back(i);
            }
            else {
                t_uncollapsible_indices.emplace_back(i);
                float item_width = 0.0f;
                if (current_item->is_separator())
                    item_width += t_layout.separator_size;
                else if (current_item->get_type() == GLToolbarItem::EType::SeparatorLine) {
                    item_width += ((float)t_layout.icons_size * 0.5f);
                }
                else
                {
                    item_width += (float)t_layout.icons_size;
                    if (current_item->is_action_with_text())
                        item_width += current_item->get_extra_size_ratio() * t_layout.icons_size;
                    if (current_item->is_action_with_text_image())
                        item_width += t_layout.text_size;
                }

                if (i < m_indices_to_draw.size() - 1) {
                    item_width += t_layout.gap_size;
                }
                
                if (current_item->is_collapse_button()) {
                    collapse_button_width = item_width;
                    continue;
                }
                
                uncollapsible_width += item_width;
            }
        }

        current_width = uncollapsible_width;
        size_t final_index = 0;

        if (current_width * t_layout.scale - final_canvas_width < 1e-6f) {
            for (; final_index < t_other_visible_indices.size(); ++final_index) {
                const auto& item_index = m_indices_to_draw[t_other_visible_indices[final_index]];
                const auto& current_item = t_items[item_index];
                float item_width = 0.0f;
                if (current_item->is_separator())
                    item_width += t_layout.separator_size;
                else if (current_item->get_type() == GLToolbarItem::EType::SeparatorLine) {
                    item_width += ((float)t_layout.icons_size * 0.5f);
                }
                else
                {
                    item_width += (float)t_layout.icons_size;
                    if (current_item->is_action_with_text())
                        item_width += current_item->get_extra_size_ratio() * t_layout.icons_size;
                    if (current_item->is_action_with_text_image())
                        item_width += t_layout.text_size;
                }

                if (item_index < m_indices_to_draw.size() - 1) {
                    item_width += t_layout.gap_size;
                }

                if ((current_width + item_width) * t_layout.scale - final_canvas_width > 1e-6f) {
                    break;
                }
                current_width += item_width;
            }
        }

        if (final_index < t_other_visible_indices.size()) {
            current_width = std::max(GLToolbar::Default_Icons_Size + 2.0f * t_layout.border, current_width + collapse_button_width);

            final_toolbar_width = current_width * t_layout.scale;
            while (final_canvas_width - final_toolbar_width < 1e-6f) {
                if (final_index < 1) {
                    break;
                }
                const auto item_index = m_indices_to_draw[t_other_visible_indices[final_index]];
                float item_width = 0.0f;
                const auto& current_item = t_items[item_index];
                if (current_item->get_type() == GLToolbarItem::EType::SeparatorLine) {
                    item_width += ((float)t_layout.icons_size * 0.5f);
                    item_width += t_layout.gap_size;
                }
                else if (current_item->is_separator()) {
                    item_width += t_layout.separator_size;
                    item_width += t_layout.gap_size;
                }
                else
                {
                    item_width += (float)t_layout.icons_size;
                    if (current_item->is_action_with_text())
                        item_width += current_item->get_extra_size_ratio() * t_layout.icons_size;
                    if (current_item->is_action_with_text_image())
                        item_width += t_layout.text_size;
                    item_width += t_layout.gap_size;
                }
                
                final_toolbar_width = final_toolbar_width - (item_width * t_layout.scale);

                --final_index;
            }
            m_b_needs_collapsed = true;
        }

        if (m_b_needs_collapsed) {
            if (final_index < t_other_visible_indices.size()) {
                std::vector<size_t> temp_indices;
                temp_indices.reserve(m_indices_to_draw.size());
                for (size_t i = 0; i < final_index; ++i) {
                    const auto item_index = m_indices_to_draw[t_other_visible_indices[i]];
                    temp_indices.emplace_back(item_index);
                }
                for (size_t i = 0; i < t_uncollapsible_indices.size(); ++i) {
                    const auto item_index = m_indices_to_draw[t_uncollapsible_indices[i]];
                    temp_indices.emplace_back(item_index);
                }
                for (size_t i = final_index; i < t_other_visible_indices.size(); ++i) {
                    const auto item_index = m_indices_to_draw[t_other_visible_indices[i]];
                    const auto& p_item = t_items[item_index];
                    temp_indices.emplace_back(item_index);
                    p_item->set_collapsed(t_toolbar.is_collapsed());

                    if (p_item->is_separator()) {
                        total_collapse_item_width += t_layout.separator_size;
                        total_collapse_item_width += t_layout.gap_size;
                    }
                    else if (p_item->is_action() || p_item->get_type() == GLToolbarItem::EType::SeparatorLine) {
                        if (p_item->is_action()) {
                            total_collapse_item_width += (float)t_layout.icons_size;
                            total_collapse_item_width += t_layout.gap_size;
                        }
                        else {
                            total_collapse_item_width += ((float)t_layout.icons_size * 0.5f);
                            total_collapse_item_width += t_layout.gap_size;
                        }
                    }
                    else if (p_item->is_action_with_text())
                    {
                        total_collapse_item_width += p_item->get_extra_size_ratio() * t_layout.icons_size;
                        total_collapse_item_width += t_layout.gap_size;
                    }
                }
                m_indices_to_draw = std::move(temp_indices);
            }
        }
    }

    float inv_zoom = (float)t_camera.get_inv_zoom();
    float factor = inv_zoom * t_layout.scale;
    float scaled_icons_size = t_layout.icons_size * factor;
    float scaled_separator_size = t_layout.separator_size * factor;
    float scaled_gap_size = t_layout.gap_size * factor;
    float scaled_border = t_layout.border * factor;

    float separator_stride = scaled_separator_size + scaled_gap_size;
    float icon_stride = scaled_icons_size + scaled_gap_size;

    float left = 0.0f;
    float top = 0.0f;
    calculate_position(t_toolbar, t_camera, final_toolbar_width, final_toolbar_height, left, top);

    left += scaled_border;
    top -= scaled_border;

    float temp_left = left;
    float temp_top = top;
    float temp_width = 2.0f * t_layout.border;
    bool line_start = true;
    final_toolbar_height = 0;

    bool offset_flag = true;
    collapse_width = 0.0f;
    bool b_needs_to_double_check_collapse_width = true;
    m_p_override_render_rect = nullptr;
    std::vector<size_t> temp_indices;
    temp_indices.reserve(m_indices_to_draw.size());
    for (size_t i = 0; i < m_indices_to_draw.size(); ++i) {
        const auto& current_item = t_items[m_indices_to_draw[i]];

        if (line_start && (current_item->get_type() == GLToolbarItem::EType::SeparatorLine || current_item->is_separator())) {
            continue;
        }

        if (current_item->is_separator()) {
            temp_left += separator_stride;
            temp_width += t_layout.separator_size;
            temp_width += t_layout.gap_size;
            continue;
        }

        if (current_item->is_action() || current_item->get_type() == GLToolbarItem::EType::SeparatorLine) {
            if (current_item->is_action()) {
                temp_width += (float)t_layout.icons_size;
                temp_width += t_layout.gap_size;
            }
            else {
                temp_width += ((float)t_layout.icons_size * 0.5f);
                temp_width += t_layout.gap_size;
            }
            current_item->render_rect[0] = temp_left;
            current_item->render_rect[1] = temp_left + scaled_icons_size;
            current_item->render_rect[2] = temp_top - scaled_icons_size;
            current_item->render_rect[3] = temp_top;
        }
        //BBS: GUI refactor: GLToolbar
        if (current_item->is_action_with_text())
        {
            float scaled_text_size = current_item->get_extra_size_ratio() * scaled_icons_size;
            current_item->render_rect[0] = temp_left + scaled_icons_size;
            current_item->render_rect[1] = temp_left + scaled_icons_size + scaled_text_size;
            current_item->render_rect[2] = temp_top - scaled_icons_size;
            current_item->render_rect[3] = temp_top;
            temp_left += scaled_text_size;
            temp_width += current_item->get_extra_size_ratio() * t_layout.icons_size;
            temp_width += t_layout.gap_size;
        }
        if (current_item->get_type() == GLToolbarItem::EType::SeparatorLine) {
            temp_left += (icon_stride - 0.5f * scaled_icons_size);
        }
        else {
            temp_left += icon_stride;
        }

        if (line_start) {
            final_toolbar_height += toolbar_height;
        }

        if (current_item->is_collapse_button()) {
            collapse_width = temp_width;
            if (total_collapse_item_width < collapse_width) {
                collapse_width = total_collapse_item_width;
            }
            else {
                collapse_width = (total_collapse_item_width) / 2.0f + GLToolbar::Default_Icons_Size;
            }
            collapse_width = std::min(collapse_width * t_layout.scale, final_toolbar_width);
        }

        temp_indices.emplace_back(m_indices_to_draw[i]);

        line_start = false;

        bool new_line = false;
        if (offset_flag) {
            new_line = final_toolbar_width - temp_width * t_layout.scale < GLToolbar::Default_Icons_Size * t_layout.scale;
        }
        else {
            new_line = collapse_width - temp_width * t_layout.scale < GLToolbar::Default_Icons_Size * t_layout.scale;
        }
        if (new_line) {
            temp_left = left;
            temp_top -= toolbar_height * inv_zoom;
            if (offset_flag) {
                temp_top -= t_layout.collapsed_offset * factor;
                offset_flag = false;
            }
            else {
                if (b_needs_to_double_check_collapse_width) {
                    b_needs_to_double_check_collapse_width = false;
                    collapse_width = temp_width * t_layout.scale;
                }
            }
            temp_width = 2.0f * t_layout.border;
            line_start = true;
        }

        if (current_item->is_collapse_button()) {
            m_p_override_render_rect = current_item->render_rect;
        }
    }
    m_indices_to_draw.clear();
    m_indices_to_draw = std::move(temp_indices);

}

} // namespace GUI
} // namespace Slic3r
