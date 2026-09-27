#include <GL/glew.h>

#include "GLIconGlyphBridge.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include <wx/bitmap.h>
#include <wx/image.h>

#include "slic3r/GUI/3DScene.hpp" // glsafe(), glAssertRecentCall

namespace Slic3r {
namespace GUI {
namespace GLIconGlyphBridge {

namespace {

// Fraction of the sprite tile the glyph mark fills. The legacy toolbar SVGs were
// rasterised to fill the whole tile but carried internal padding, so the visible
// mark sat at roughly this ratio; matching it keeps icon weight consistent when a
// row swaps from an SVG sprite to a glyph.
constexpr double kGlyphFillRatio = 0.72;

struct Tint
{
    wxColour      colour;
    unsigned char alpha; // extra multiplier on the glyph coverage (MD3 state emphasis)
};

// Map a GLToolbarItem::EState column to an MD3 role + emphasis. The column order
// is fixed by GLToolbarItem: Normal, Pressed, Disabled, Hover, HoverPressed,
// HoverDisabled, then the two rendered highlight states (HighlightedShown,
// HighlightedHidden). Selection reads as the accent (Primary) and disabled dims
// OnSurfaceVariant, so the whole set is colour-driven and the _dark sprites retire.
Tint tint_for_state(int state_index, bool dark, MD3::ColorScheme scheme)
{
    MD3::Role     role  = MD3::Role::OnSurfaceVariant;
    unsigned char alpha = 255;
    switch (state_index) {
    case 0: role = MD3::Role::OnSurfaceVariant; break;             // Normal (idle)
    case 1: role = MD3::Role::Primary;          break;             // Pressed (selected)
    case 2: role = MD3::Role::OnSurfaceVariant; alpha = 97; break; // Disabled (~38%)
    case 3: role = MD3::Role::OnSurface;        break;             // Hover
    case 4: role = MD3::Role::Primary;          break;             // HoverPressed
    case 5: role = MD3::Role::OnSurfaceVariant; alpha = 97; break; // HoverDisabled
    case 6: role = MD3::Role::OnSurface;        break;             // HighlightedShown
    default: role = MD3::Role::OnSurfaceVariant; alpha = 128; break; // HighlightedHidden
    }
    return { MD3::resolve(role, dark, scheme), alpha };
}

// Render a centred white glyph into a sprite_px x sprite_px coverage mask (one
// byte per pixel). Reuses the well-tested MaterialIcon::bitmap AA path and only
// composites on the CPU, so it inherits the same alpha/antialiasing as the rest
// of the migrated wxDC icons. Returns false when nothing was drawn.
bool render_glyph_mask(uint32_t glyph, int sprite_px, std::vector<unsigned char>& mask)
{
    mask.assign(static_cast<size_t>(sprite_px) * sprite_px, 0);
    const int glyph_px = std::max(1, static_cast<int>(std::lround(sprite_px * kGlyphFillRatio)));

    wxBitmap bmp = MaterialIcon::bitmap(nullptr, glyph, glyph_px, wxColour(255, 255, 255));
    if (!bmp.IsOk())
        return false;
    wxImage img = bmp.ConvertToImage();
    if (!img.IsOk())
        return false;

    const int             gw   = img.GetWidth();
    const int             gh   = img.GetHeight();
    const unsigned char*  rgb  = img.GetData();
    const bool            hasA = img.HasAlpha();
    const unsigned char*  al   = hasA ? img.GetAlpha() : nullptr;
    if (rgb == nullptr)
        return false;

    const int offx = (sprite_px - gw) / 2;
    const int offy = (sprite_px - gh) / 2;

    bool any = false;
    for (int y = 0; y < gh; ++y) {
        const int ty = offy + y;
        if (ty < 0 || ty >= sprite_px)
            continue;
        for (int x = 0; x < gw; ++x) {
            const int tx = offx + x;
            if (tx < 0 || tx >= sprite_px)
                continue;
            unsigned char cov;
            if (al != nullptr)
                cov = al[y * gw + x];
            else {
                const int i = (y * gw + x) * 3;
                cov = std::max(rgb[i], std::max(rgb[i + 1], rgb[i + 2]));
            }
            if (cov != 0) {
                mask[static_cast<size_t>(ty) * sprite_px + tx] = cov;
                any = true;
            }
        }
    }
    return any;
}

} // namespace

bool available() { return MaterialIcon::available(); }

uint32_t glyph_for_toolbar_item(const std::string& item_name)
{
    using G = MaterialIcon::Glyph;
    static const std::unordered_map<std::string, uint32_t> map = {
        // --- main 3D-editor toolbar (GLCanvas3D::_init_main_toolbar) ---
        {"add", G::FolderOpen},
        {"addplate", G::GridView},
        // 'screen_rotation' is absent from the vendored font; the auto-orient tool
        // uses the '3d_rotation' mark (distinct from the Rotate gizmo's rotate_right).
        {"orient", G::Rotation3D},
        {"arrange", G::AutoAwesomeMosaic},
        {"layersediting", G::Layers},
        {"splitobjects", G::CallSplit},
        {"splitvolumes", G::ContentCut},
        {"More", G::MoreHoriz},
        {"assembly_view", G::ViewInAr},
        // --- gizmo rail (GLGizmosManager::convert_gizmo_type_to_string) ---
        {"Move", G::OpenWith},
        {"Rotate", G::RotateRight},
        {"Scale", G::OpenInFull},
        {"Flatten", G::AlignHorizontalLeft},
        {"Cut", G::ContentCut},
        {"MeshBoolean", G::JoinInner},
        {"FdmSupports", G::Hardware},
        {"Seam", G::Timeline},
        {"Text", G::TextFields},
        {"Svg", G::ShapeLine},
        {"Color Painting", G::Brush},
        {"FuzzySkin", G::Grain},
        {"Mesause", G::Straighten}, // legacy spelling emitted by convert_gizmo_type_to_string
        {"Assembly", G::ViewInAr},
        {"Simplify", G::Compress},
        {"BrimEars", G::Padding},
    };
    const auto it = map.find(item_name);
    return it == map.end() ? 0u : it->second;
}

int overpaint_toolbar_atlas(unsigned int gl_tex_id,
                            int          tex_width,
                            int          tex_height,
                            int          sprite_size_px,
                            int          num_states,
                            const std::vector<AtlasGlyphRow>& rows,
                            bool             dark,
                            MD3::ColorScheme scheme)
{
    if (!available() || gl_tex_id == 0 || sprite_size_px <= 0 || num_states <= 0 || rows.empty())
        return 0;
    if (tex_width <= 0 || tex_height <= 0)
        return 0;

    // Tile pitch: load_from_svg_files_as_sprites_array pads every tile by 1px so
    // linear sampling on a tile edge never bleeds into its neighbour.
    const int tile_pitch = sprite_size_px + 1;

    glsafe(::glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(gl_tex_id)));
    glsafe(::glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

    std::vector<unsigned char> mask;
    std::vector<unsigned char> tile(static_cast<size_t>(sprite_size_px) * sprite_size_px * 4, 0);

    int painted = 0;
    for (const AtlasGlyphRow& row : rows) {
        if (row.glyph == 0)
            continue;
        const int yoff = row.row_index * tile_pitch + 1; // +1: skip the tile's top border
        if (yoff < 0 || yoff + sprite_size_px > tex_height)
            continue; // never corrupt neighbouring memory
        if (!render_glyph_mask(row.glyph, sprite_size_px, mask))
            continue;

        for (int s = 0; s < num_states; ++s) {
            const int xoff = s * tile_pitch + 1; // +1: skip the tile's left border
            if (xoff + sprite_size_px > tex_width)
                break;
            const Tint t = tint_for_state(s, dark, scheme);
            const unsigned char cr = t.colour.Red();
            const unsigned char cg = t.colour.Green();
            const unsigned char cb = t.colour.Blue();
            for (size_t p = 0; p < mask.size(); ++p) {
                unsigned char*      d   = &tile[p * 4];
                const unsigned char cov = mask[p];
                d[0] = cr;
                d[1] = cg;
                d[2] = cb;
                d[3] = static_cast<unsigned char>((cov * t.alpha) / 255);
            }
            glsafe(::glTexSubImage2D(GL_TEXTURE_2D, 0, xoff, yoff, sprite_size_px, sprite_size_px,
                                     GL_RGBA, GL_UNSIGNED_BYTE, tile.data()));
        }
        ++painted;
    }

    glsafe(::glBindTexture(GL_TEXTURE_2D, 0));
    return painted;
}

unsigned int make_glyph_texture(uint32_t glyph, int px, const wxColour& colour, int* out_w, int* out_h)
{
    if (out_w != nullptr) *out_w = 0;
    if (out_h != nullptr) *out_h = 0;
    if (!available() || glyph == 0 || px <= 0)
        return 0;

    wxBitmap bmp = MaterialIcon::bitmap(nullptr, glyph, px, colour);
    if (!bmp.IsOk())
        return 0;
    wxImage img = bmp.ConvertToImage();
    if (!img.IsOk())
        return 0;

    const int            w    = img.GetWidth();
    const int            h    = img.GetHeight();
    const unsigned char* rgb  = img.GetData();
    const bool           hasA = img.HasAlpha();
    const unsigned char* al   = hasA ? img.GetAlpha() : nullptr;
    if (rgb == nullptr || w <= 0 || h <= 0)
        return 0;

    std::vector<unsigned char> data(static_cast<size_t>(w) * h * 4, 0);
    for (int i = 0; i < w * h; ++i) {
        data[i * 4 + 0] = rgb[i * 3 + 0];
        data[i * 4 + 1] = rgb[i * 3 + 1];
        data[i * 4 + 2] = rgb[i * 3 + 2];
        data[i * 4 + 3] = al != nullptr ? al[i]
                                        : std::max(rgb[i * 3], std::max(rgb[i * 3 + 1], rgb[i * 3 + 2]));
    }

    GLuint id = 0;
    glsafe(::glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
    glsafe(::glGenTextures(1, &id));
    glsafe(::glBindTexture(GL_TEXTURE_2D, id));
    glsafe(::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data()));
    glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
    glsafe(::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0));
    glsafe(::glBindTexture(GL_TEXTURE_2D, 0));

    if (out_w != nullptr) *out_w = w;
    if (out_h != nullptr) *out_h = h;
    return static_cast<unsigned int>(id);
}

} // namespace GLIconGlyphBridge
} // namespace GUI
} // namespace Slic3r
