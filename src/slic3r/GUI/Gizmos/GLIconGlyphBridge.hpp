#ifndef slic3r_GUI_GLIconGlyphBridge_hpp_
#define slic3r_GUI_GLIconGlyphBridge_hpp_

#include <cstdint>
#include <string>
#include <vector>

#include <wx/colour.h>

#include "slic3r/GUI/Widgets/MaterialIcon.hpp"
#include "slic3r/GUI/Widgets/MD3Tokens.hpp"

namespace Slic3r {
namespace GUI {

// The single glyph->GL-texture bridge for the OpenGL viewport chrome (main
// 3D-editor toolbar, the left gizmo rail, and the assembly toolbar). It renders
// Material Symbols glyphs (via MaterialIcon, the wxDC/wxGraphicsContext path used
// by the rest of the migrated UI) into GPU textures so those GL toolbars resolve
// to MD3 icons instead of the legacy toolbar_*.svg light/dark sprite pairs.
//
// State is expressed purely through COLOUR (idle OnSurfaceVariant, selected
// Primary, disabled dimmed), never the font FILL axis, so the _dark sprite
// variants retire. Every entry point degrades gracefully when the icon font is
// absent (MaterialIcon::available() == false): the caller keeps its existing
// raster/SVG sprite path unchanged, so the viewport never regresses to tofu.
//
// This is a foundational primitive designed ONCE and reused: overpaint_toolbar_atlas
// serves the GLToolbar sprite-array path (both the main toolbar and the gizmo rail,
// which share GLToolbar), while make_glyph_texture serves the single-texture
// ImGui / IMTexture gizmo-chrome path.
namespace GLIconGlyphBridge {

// True when the Material Symbols face is registered and resolvable. Thin wrapper
// over MaterialIcon::available() so call sites read intent-first.
bool available();

// Map a GLToolbar item name to a Material Symbols codepoint. The key is the exact
// GLToolbarItem::Data::name assigned at toolbar build time:
//   - main 3D-editor toolbar: add / addplate / arrange / layersediting /
//     splitobjects / splitvolumes / More / assembly_view
//   - gizmo rail: the strings from GLGizmosManager::convert_gizmo_type_to_string
//     (Move / Rotate / Scale / Flatten / Cut / MeshBoolean / FdmSupports / Seam /
//     Text / Svg / "Color Painting" / FuzzySkin / "Mesause" / Assembly / Simplify /
//     BrimEars)
// Returns 0 when the name has no MD3 mapping (e.g. the separator sprites): the
// caller keeps that item's raster sprite. "orient" maps to Rotation3D (the
// Material '3d_rotation' mark) because the exact 'screen_rotation' glyph is
// absent from the vendored font.
uint32_t glyph_for_toolbar_item(const std::string& item_name);

// One row of the toolbar sprite atlas that should be overpainted with a glyph.
struct AtlasGlyphRow
{
    int      row_index; // 0-based index among items with a non-empty icon filename
    uint32_t glyph;     // Material Symbols codepoint (non-zero)
};

// Overpaint the given rows of an already-built GLToolbar sprite-array texture with
// MD3-tinted glyphs, in place, via glTexSubImage2D. The atlas layout MUST match
// GLTexture::load_from_svg_files_as_sprites_array: num_states columns x N item
// rows, tile pitch (sprite_size_px + 1), each tile's content inset by (1,1). Only
// the mapped rows are rewritten; every other row keeps its SVG sprite, so unmapped
// items and the whole texture (when the font is missing) are left untouched.
//
// The atlas must be uncompressed GL_RGBA (as the toolbar always builds it with
// compress=false). Must run with a live GL context. Returns the number of rows
// painted.
int overpaint_toolbar_atlas(unsigned int gl_tex_id,
                            int          tex_width,
                            int          tex_height,
                            int          sprite_size_px,
                            int          num_states,
                            const std::vector<AtlasGlyphRow>& rows,
                            bool             dark,
                            MD3::ColorScheme scheme);

// Render a single glyph to a standalone RGBA GL texture (GL_LINEAR, no mipmaps),
// tinted with the given colour, for the ImGui / IMTexture gizmo-chrome path. The
// texture is sized to the glyph's natural extent. Returns 0 on failure (missing
// font/glyph, no GL context). The CALLER owns the texture and must glDeleteTextures
// it. Out params report the pixel size for ImGui::Image sizing.
unsigned int make_glyph_texture(uint32_t glyph, int px, const wxColour& colour, int* out_w = nullptr, int* out_h = nullptr);

} // namespace GLIconGlyphBridge

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GUI_GLIconGlyphBridge_hpp_
