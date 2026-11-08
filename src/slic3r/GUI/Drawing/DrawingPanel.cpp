#include "DrawingPanel.hpp"

#include "../DocsBrowserDialog.hpp"
#include "../FeatureFinder/LocalToolsActivity.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../MsgDialog.hpp"
#include "../PartPlate.hpp"
#include "../Plater.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3ColorPicker.hpp"
#include "../Widgets/MD3DataView.hpp"
#include "../Widgets/MD3Menu.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/SuperConfirmGate.hpp"

#include "libslic3r/GCode/ThumbnailData.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/Utils.hpp"

#include <wx/base64.h>
#include <wx/datetime.h>
#include <wx/dcbuffer.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/graphics.h>
#include <wx/image.h>
#include <wx/menu.h>
#include <wx/mstream.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <fstream>
#include <memory>
#include <sstream>

namespace Slic3r { namespace GUI { namespace Drawing {

namespace fs = std::filesystem;

namespace {

constexpr const char *kGuide = "docs/features/windows/drawing-board.md";

const std::uint32_t kGlyphs[] = {0xF097, 0xE6D3, 0xF603, 0xE6D2, 0xE6D1, 0xE3AE, 0xF365, 0xE6D0};
enum : std::uint32_t {
    GlyphUndo    = 0xE166,
    GlyphRedo    = 0xE15A,
    GlyphExport  = 0xE6B8,
    GlyphClear   = 0xE16C,
    GlyphCapture = 0xE412,
    GlyphHistory = 0xE8B3,
    GlyphPalette = 0xE40A,
};

wxColour role(MD3::Role r) { return StateColor::semantic(r); }
wxColour rgb(std::uint32_t c) { return wxColour((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF); }
double   now_seconds() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

wxString pen_title(Pen pen)
{
    switch (pen) {
    case Pen::Pencil: return _L("Pencil");
    case Pen::Pen: return _L("Pen");
    case Pen::Fineliner: return _L("Fineliner");
    case Pen::Marker: return _L("Marker");
    case Pen::Highlighter: return _L("Highlighter");
    case Pen::Brush: return _L("Brush");
    case Pen::Fountain: return _L("Fountain pen");
    case Pen::Eraser: return _L("Eraser");
    default: return wxString();
    }
}

Button *tool_button(wxWindow *parent, const wxString &text, std::uint32_t glyph, Button::Variant variant = Button::Variant::Outlined)
{
    auto *b = new Button(parent, text);
    b->SetVariant(variant);
    b->SetButtonSize(Button::Size::Small);
    if (glyph) b->SetGlyph(glyph);
    return b;
}

Label *text_label(wxWindow *parent, const wxFont &font, const wxString &text, bool muted, bool wrap = false)
{
    auto *l = new Label(parent, font, text, wrap ? (LB_AUTO_WRAP | wxST_NO_AUTORESIZE) : 0);
    l->SetForegroundColour(role(muted ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface));
    return l;
}

// Draws strokes with a graphics context; a see-through pen fades as one
// layer, so a stroke crossing itself does not darken.
void paint_strokes(wxGraphicsContext *gc, const std::vector<Stroke> &strokes, double scale)
{
    for (const Stroke &s : strokes) {
        if (s.points.empty()) continue;
        const bool layered = s.opacity < 0.999f;
        if (layered) gc->BeginLayer(s.opacity);
        const wxPenCap cap = info(s.pen).square ? wxCAP_PROJECTING : wxCAP_ROUND;
        if (s.points.size() == 1) {
            const Point &p = s.points.front();
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->SetBrush(wxBrush(rgb(s.color)));
            gc->DrawEllipse((p.x - p.width / 2) * scale, (p.y - p.width / 2) * scale, p.width * scale, p.width * scale);
        }
        for (std::size_t i = 1; i < s.points.size(); ++i) {
            const Point      &a = s.points[i - 1];
            const Point      &b = s.points[i];
            wxGraphicsPenInfo pen(rgb(s.color), std::max(0.5, double(a.width + b.width) / 2.0 * scale));
            pen.Cap(cap);
            gc->SetPen(gc->CreatePen(pen));
            gc->StrokeLine(a.x * scale, a.y * scale, b.x * scale, b.y * scale);
        }
        if (layered) gc->EndLayer();
    }
}

fs::path plate_file(const fs::path &folder, int plate, const char *extension)
{
    return folder / ("plate-" + std::to_string(plate) + extension);
}

} // namespace

// ---------------------------------------------------------------- the board

class BoardCanvas : public wxWindow
{
public:
    BoardCanvas(wxWindow *parent, DrawingPanel *owner)
        : wxWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS | wxFULL_REPAINT_ON_RESIZE), m_owner(owner)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetMinSize(FromDIP(wxSize(520, 340)));
        SetName(_L("Drawing board"));
        Bind(wxEVT_PAINT, &BoardCanvas::on_paint, this);
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
            SetFocus();
            if (!HasCapture()) CaptureMouse();
            m_keyboard_pen = false;
            begin(to_doc(e.GetPosition()));
        });
        Bind(wxEVT_MOTION, [this](wxMouseEvent &e) {
            m_hover  = to_doc(e.GetPosition());
            m_inside = true;
            if (e.LeftIsDown()) extend(m_hover, e.ShiftDown());
            else Refresh();
        });
        Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) {
            if (HasCapture()) ReleaseMouse();
            end();
        });
        Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &) { m_inside = true; });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &) {
            m_inside = false;
            Refresh();
        });
        Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) { end(); });
        Bind(wxEVT_KEY_DOWN, &BoardCanvas::on_key, this);
        // Right-click, Shift+F10 or the Menu key: every pen with its key, from the same table as the keys.
        Bind(wxEVT_CONTEXT_MENU, &BoardCanvas::on_menu, this);
        Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) {
            Refresh();
            e.Skip();
        });
        Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) {
            if (m_pen_down) {
                m_pen_down = false;
                end();
            }
            Refresh();
            e.Skip();
        });
    }
    bool AcceptsFocus() const override { return true; }

private:
    double scale() const
    {
        const wxSize s = GetClientSize();
        return std::max(0.05, std::min(double(s.x) / m_owner->m_doc.width, double(s.y) / m_owner->m_doc.height));
    }
    wxPoint2DDouble origin() const
    {
        const wxSize s = GetClientSize();
        const double k = scale();
        return wxPoint2DDouble((s.x - m_owner->m_doc.width * k) / 2, (s.y - m_owner->m_doc.height * k) / 2);
    }
    wxPoint2DDouble to_doc(const wxPoint &p) const
    {
        const wxPoint2DDouble o = origin();
        const double          k = scale();
        return wxPoint2DDouble((p.x - o.m_x) / k, (p.y - o.m_y) / k);
    }

    void begin(const wxPoint2DDouble &p)
    {
        DrawingPanel *o = m_owner;
        if (o->m_pen == Pen::Eraser) {
            m_erasing = true;
            m_erased  = erase(o->m_doc.strokes, float(p.m_x), float(p.m_y), o->m_widths[int(Pen::Eraser)] / 2);
            Refresh();
            return;
        }
        m_builder.start(o->m_pen, o->m_color, o->m_widths[int(o->m_pen)], o->m_opacity[int(o->m_pen)], o->m_smoothing, float(p.m_x), float(p.m_y),
                        now_seconds());
        Refresh();
    }
    void extend(const wxPoint2DDouble &p, bool lock)
    {
        DrawingPanel *o = m_owner;
        if (m_erasing) {
            if (erase(o->m_doc.strokes, float(p.m_x), float(p.m_y), o->m_widths[int(Pen::Eraser)] / 2)) m_erased = true;
        } else if (m_builder.active()) {
            m_builder.add(float(p.m_x), float(p.m_y), now_seconds(), 1, lock);
        }
        Refresh();
    }
    void end()
    {
        if (m_erasing) {
            m_erasing = false;
            if (m_erased) m_owner->changed();
            m_erased = false;
            Refresh();
            return;
        }
        if (!m_builder.active()) return;
        m_owner->m_doc.strokes.push_back(m_builder.finish());
        m_owner->changed();
        Refresh();
    }

    void on_key(wxKeyEvent &e)
    {
        const int key = e.GetKeyCode();
        if (e.ControlDown() && e.ShiftDown() && key == 'E') return m_owner->export_drawing();
        if (e.ControlDown() && key == 'Z') return m_owner->undo();
        if (e.ControlDown() && key == 'Y') return m_owner->redo();
        if (!e.ControlDown() && !e.AltDown()) {
            if (key == 'C') return m_owner->pick_custom_color();
            if (key == 'S') return focus_width();
            for (const PenInfo &p : pens())
                if (key == p.key || key == std::tolower(static_cast<unsigned char>(p.key))) return m_owner->set_pen(p.pen);
        }
        // The keyboard pen: the arrow keys move it, Space puts it down and lifts it.
        if (!m_keyboard_pen) {
            m_keyboard     = m_inside ? m_hover : wxPoint2DDouble(m_owner->m_doc.width / 2.0, m_owner->m_doc.height / 2.0);
            m_keyboard_pen = key == WXK_LEFT || key == WXK_RIGHT || key == WXK_UP || key == WXK_DOWN || key == WXK_SPACE;
        }
        const double    step = e.ShiftDown() ? 2 : 12;
        wxPoint2DDouble next = m_keyboard;
        switch (key) {
        case WXK_LEFT: next.m_x -= step; break;
        case WXK_RIGHT: next.m_x += step; break;
        case WXK_UP: next.m_y -= step; break;
        case WXK_DOWN: next.m_y += step; break;
        case WXK_SPACE:
            m_pen_down = !m_pen_down;
            if (m_pen_down) begin(m_keyboard);
            else end();
            Refresh();
            return;
        case WXK_ESCAPE:
            if (m_pen_down) {
                m_pen_down = false;
                end();
            }
            m_keyboard_pen = false;
            Refresh();
            return;
        default: e.Skip(); return;
        }
        next.m_x   = std::clamp(next.m_x, 0.0, double(m_owner->m_doc.width));
        next.m_y   = std::clamp(next.m_y, 0.0, double(m_owner->m_doc.height));
        m_keyboard = next;
        if (m_pen_down) extend(next, false);
        Refresh();
    }

    void focus_width()
    {
        if (wxTextCtrl *text = m_owner->m_width_input->GetTextCtrl()) {
            text->SetFocus();
            text->SelectAll();
        }
    }

    void on_menu(wxContextMenuEvent &e)
    {
        enum : int { First = wxID_HIGHEST + 100, Colour = First + 20, Width, Undo, Redo, Keep, Export, Clear };
        wxMenu menu;
        for (const PenInfo &p : pens()) {
            wxMenuItem *item = menu.AppendRadioItem(First + int(p.pen), wxString::Format("%s\t%c", pen_title(p.pen), p.key));
            item->Check(p.pen == m_owner->m_pen);
        }
        menu.AppendSeparator();
        menu.Append(Colour, _L("Colour...") + "\tC");
        menu.Append(Width, _L("Width (px)") + "\tS");
        menu.AppendSeparator();
        menu.Append(Undo, _L("Undo") + "\tCtrl+Z")->Enable(m_owner->m_history.can_undo());
        menu.Append(Redo, _L("Redo") + "\tCtrl+Y")->Enable(m_owner->m_history.can_redo());
        menu.Append(Keep, _L("Keep this version"));
        menu.Append(Export, _L("Export...") + "\tCtrl+Shift+E");
        menu.AppendSeparator();
        menu.Append(Clear, _L("Clear..."))->Enable(!m_owner->m_doc.strokes.empty());
        wxPoint at = e.GetPosition();
        if (at == wxDefaultPosition) at = ClientToScreen(wxPoint(GetClientSize().x / 2, GetClientSize().y / 2));
        const int chosen = MD3::PopupMenuSelection(this, menu, at);
        if (chosen >= First && chosen < First + int(Pen::Count)) return m_owner->set_pen(Pen(chosen - First));
        switch (chosen) {
        case Colour: m_owner->pick_custom_color(); break;
        case Width: focus_width(); break;
        case Undo: m_owner->undo(); break;
        case Redo: m_owner->redo(); break;
        case Keep: m_owner->keep_version(into_u8(_L("Kept by you"))); break;
        case Export: m_owner->export_drawing(); break;
        case Clear: m_owner->clear_all(); break;
        default: break;
        }
    }

    void on_paint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(role(MD3::Role::SurfaceContainerLowest)));
        dc.Clear();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (!gc) return;
        const wxPoint2DDouble o = origin();
        const double          k = scale();
        const double          w = m_owner->m_doc.width * k;
        const double          h = m_owner->m_doc.height * k;
        gc->SetBrush(wxBrush(wxColour(255, 255, 255)));
        gc->SetPen(wxPen(role(MD3::Role::OutlineVariant)));
        gc->DrawRectangle(o.m_x, o.m_y, w, h);
        if (m_owner->m_background.IsOk()) gc->DrawBitmap(m_owner->m_background, o.m_x, o.m_y, w, h);
        gc->PushState();
        gc->Clip(o.m_x, o.m_y, w, h);
        gc->Translate(o.m_x, o.m_y);
        paint_strokes(gc.get(), m_owner->m_doc.strokes, k);
        if (m_builder.active()) paint_strokes(gc.get(), {m_builder.stroke()}, k);
        // Where the pen or the eraser would touch.
        if (m_keyboard_pen || m_inside) {
            const wxPoint2DDouble &at = m_keyboard_pen ? m_keyboard : m_hover;
            const double           r  = std::max(3.0, double(m_owner->m_widths[int(m_owner->m_pen)]) / 2 * k);
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            gc->SetPen(wxPen(role(MD3::Role::Primary), m_pen_down ? 2 : 1));
            gc->DrawEllipse(at.m_x * k - r, at.m_y * k - r, r * 2, r * 2);
        }
        gc->PopState();
        if (HasFocus()) {
            gc->SetPen(wxPen(role(MD3::Role::Primary), FromDIP(2)));
            gc->SetBrush(*wxTRANSPARENT_BRUSH);
            gc->DrawRectangle(1, 1, GetClientSize().x - 2, GetClientSize().y - 2);
        }
    }

    DrawingPanel   *m_owner;
    StrokeBuilder   m_builder;
    bool            m_erasing = false, m_erased = false, m_pen_down = false, m_keyboard_pen = false, m_inside = false;
    wxPoint2DDouble m_hover{0, 0}, m_keyboard{0, 0};
};

// ---------------------------------------------------------------- the panel

void DrawingPanel::open()
{
    if (MainFrame *frame = wxGetApp().mainframe)
        frame->open_service("drawing", _L("Drawing board"), [](wxWindow *parent) { return new DrawingPanel(parent); });
}

DrawingPanel::DrawingPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY)
{
    SetName("drawing-board");
    SetBackgroundColour(role(MD3::Role::Surface));
    for (const PenInfo &p : pens()) {
        m_widths[int(p.pen)]  = p.default_width;
        m_opacity[int(p.pen)] = p.default_opacity;
    }
    if (Plater *plater = wxGetApp().plater()) m_plate = plater->get_partplate_list().get_curr_plate_index() + 1;
    m_folder = fs::u8path(data_dir()) / "drawings";

    const int pad  = FromDIP(MD3::Metrics::active().padding);
    auto     *root = new wxBoxSizer(wxVERTICAL);
    auto     *head = new wxBoxSizer(wxHORIZONTAL);
    m_title        = text_label(this, Label::Head_20, wxString(), false);
    head->Add(m_title, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    m_plate_button = tool_button(this, _L("Another plate..."), 0);
    m_plate_button->SetToolTip(_L("Draw on a different plate of this project; each plate keeps its own drawing"));
    m_plate_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { choose_plate(); });
    head->Add(m_plate_button, 0, wxALIGN_CENTER_VERTICAL);
    head->AddStretchSpacer(1);
    auto *guide = tool_button(this, _L("How it works"), 0, Button::Variant::Text);
    guide->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { DocsBrowserDialog::ShowArticle(wxGetApp().mainframe, kGuide); });
    head->Add(guide, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(head, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);
    m_status = text_label(this, Label::Body_13, wxString(), true);
    root->Add(m_status, 0, wxLEFT | wxRIGHT | wxTOP, pad / 2);
    build_toolbar(root);

    auto *body = new wxBoxSizer(wxHORIZONTAL);
    m_canvas   = new BoardCanvas(this, this);
    body->Add(m_canvas, 1, wxEXPAND | wxRIGHT, pad);
    auto *side = new wxBoxSizer(wxVERTICAL);
    auto *vh   = new wxBoxSizer(wxHORIZONTAL);
    vh->Add(text_label(this, Label::Head_14, _L("Versions kept on this computer"), false), 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    auto *keep = tool_button(this, _L("Keep this version"), GlyphHistory);
    keep->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { keep_version(into_u8(_L("Kept by you"))); });
    vh->Add(keep, 0, wxALIGN_CENTER_VERTICAL);
    side->Add(vh, 0, wxEXPAND | wxBOTTOM, FromDIP(6));
    m_version_list = new MD3DataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(320), -1), wxDV_ROW_LINES | wxDV_SINGLE);
    m_version_list->AppendTextColumn(_L("Saved"), wxDATAVIEW_CELL_INERT, FromDIP(120), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_version_list->AppendTextColumn(_L("What"), wxDATAVIEW_CELL_INERT, FromDIP(130), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_version_list->AppendTextColumn(_L("Strokes"), wxDATAVIEW_CELL_INERT, FromDIP(64), wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
    m_version_list->SetName(_L("Versions kept on this computer"));
    m_version_list->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent &) { restore_version(m_version_list->GetSelectedRow()); });
    side->Add(m_version_list, 1, wxEXPAND);
    auto *restore = tool_button(this, _L("Bring this version back"), 0);
    restore->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { restore_version(m_version_list->GetSelectedRow()); });
    side->Add(restore, 0, wxTOP, FromDIP(6));
    auto *note = text_label(this, Label::Body_12,
                            _L("Bringing a version back replaces the drawing and can be undone. The last 20 versions are kept; clearing keeps one first."),
                            true, true);
    note->SetMinSize(wxSize(FromDIP(320), -1));
    side->Add(note, 0, wxEXPAND | wxTOP, FromDIP(6));
    body->Add(side, 0, wxEXPAND);
    root->Add(body, 1, wxEXPAND | wxALL, pad);
    auto *keys = text_label(this, Label::Body_12,
                            _L("1 to 7 pick a pen and E the eraser, C a colour and S the width. Hold Shift to keep a stroke on one of eight directions. "
                               "Ctrl+Z and Ctrl+Y undo and redo, Ctrl+Shift+E exports. With the board focused, the arrow keys move the keyboard pen (Shift "
                               "for small steps), Space puts it down and lifts it, and Esc lifts it. Right-click or Shift+F10 lists every pen with its key."),
                            true, true);
    root->Add(keys, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, pad);
    SetSizer(root);
    load();
    refresh_tools();
}

DrawingPanel::~DrawingPanel() { save(); }

void DrawingPanel::build_toolbar(wxSizer *root)
{
    const int pad   = FromDIP(MD3::Metrics::active().padding);
    auto     *tools = new wxWrapSizer(wxHORIZONTAL);
    for (const PenInfo &p : pens()) {
        const wxString title = pen_title(p.pen);
        auto          *b     = tool_button(this, title, kGlyphs[int(p.pen)]);
        b->SetToolTip(wxString::Format(_L("%s (key %c)"), title, p.key));
        b->SetName(title);
        const Pen pen = p.pen;
        b->Bind(wxEVT_BUTTON, [this, pen](wxCommandEvent &) { set_pen(pen); });
        m_pen_buttons.push_back(b);
        tools->Add(b, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
    }
    root->Add(tools, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *ink       = new wxWrapSizer(wxHORIZONTAL);
    m_swatch_parent = this;
    m_swatches      = new wxBoxSizer(wxHORIZONTAL);
    ink->Add(m_swatches, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(4));
    auto *custom = tool_button(this, _L("Colour..."), GlyphPalette);
    custom->SetToolTip(_L("Pick any colour; the last eight stay beside this project's filament colours"));
    custom->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { pick_custom_color(); });
    ink->Add(custom, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(12));
    const auto spin = [&](const wxString &caption, int min, int max, SpinInput *&out, std::function<void(int)> on_change) {
        ink->Add(text_label(this, Label::Body_13, caption, false), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(4));
        SpinInput *input = new SpinInput(this, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(84), -1), 0, min, max, min);
        input->SetName(caption);
        input->Bind(wxEVT_SPINCTRL, [input, on_change](wxCommandEvent &) { on_change(input->GetValue()); });
        ink->Add(input, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, FromDIP(10));
        out = input;
    };
    spin(_L("Width (px)"), 1, 120, m_width_input, [this](int v) {
        m_widths[int(m_pen)] = float(v);
        m_canvas->Refresh();
    });
    spin(_L("Opacity (%)"), 5, 100, m_opacity_input, [this](int v) { m_opacity[int(m_pen)] = float(v) / 100.0f; });
    spin(_L("Smoothing (%)"), 0, 90, m_smoothing_input, [this](int v) { m_smoothing = float(v) / 100.0f; });
    root->Add(ink, 0, wxEXPAND | wxLEFT | wxRIGHT, pad);

    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    m_undo        = tool_button(this, _L("Undo"), GlyphUndo);
    m_undo->SetToolTip(_L("Undo (Ctrl+Z)"));
    m_undo->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { undo(); });
    m_redo = tool_button(this, _L("Redo"), GlyphRedo);
    m_redo->SetToolTip(_L("Redo (Ctrl+Y)"));
    m_redo->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { redo(); });
    auto *capture = tool_button(this, _L("Capture the plate again"), GlyphCapture);
    capture->SetToolTip(_L("Take a new picture of the plate to draw on; your strokes stay where they are"));
    capture->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { capture_plate(true); });
    auto *exporter = tool_button(this, _L("Export..."), GlyphExport, Button::Variant::Tonal);
    exporter->SetToolTip(_L("Save the drawing as PNG at 1x, 2x or 3x, on a transparent background, or as SVG") + " (Ctrl+Shift+E)");
    exporter->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { export_drawing(); });
    auto *clear = tool_button(this, _L("Clear..."), GlyphClear);
    clear->SetToolTip(_L("Remove every stroke, behind the two-key confirmation; a version is kept first"));
    clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { clear_all(); });
    for (Button *b : {m_undo, m_redo, capture, exporter}) actions->Add(b, 0, wxRIGHT, FromDIP(6));
    actions->AddStretchSpacer(1);
    actions->Add(clear, 0);
    root->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad / 2);
}

void DrawingPanel::refresh_tools()
{
    for (std::size_t i = 0; i < m_pen_buttons.size(); ++i) {
        m_pen_buttons[i]->SetVariant(int(i) == int(m_pen) ? Button::Variant::Tonal : Button::Variant::Outlined);
        m_pen_buttons[i]->Refresh();
    }
    m_width_input->SetValue(int(std::lround(m_widths[int(m_pen)])));
    m_opacity_input->SetValue(int(std::lround(m_opacity[int(m_pen)] * 100)));
    m_smoothing_input->SetValue(int(std::lround(m_smoothing * 100)));
    m_opacity_input->Enable(m_pen != Pen::Eraser);

    // This project's filament colours first, then recent colours.
    std::vector<std::uint32_t> colors;
    if (PresetBundle *bundle = wxGetApp().preset_bundle)
        if (const auto *filaments = bundle->project_config.option<ConfigOptionStrings>("filament_colour"))
            for (const std::string &c : filaments->values) {
                std::uint32_t v = 0;
                if (c.size() >= 7 && parse_hex(std::string_view(c).substr(0, 7), v) && std::find(colors.begin(), colors.end(), v) == colors.end())
                    colors.push_back(v);
            }
    for (std::uint32_t c : m_recent)
        if (std::find(colors.begin(), colors.end(), c) == colors.end()) colors.push_back(c);
    if (std::find(colors.begin(), colors.end(), m_color) == colors.end()) colors.insert(colors.begin(), m_color);
    if (colors.size() > 14) colors.resize(14);
    m_swatches->Clear(true);
    for (std::uint32_t c : colors) {
        auto          *sw     = new wxWindow(m_swatch_parent, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(26, 26)));
        const wxString name   = wxString::FromUTF8(hex_color(c));
        const bool     chosen = c == m_color;
        sw->SetToolTip(chosen ? wxString::Format(_L("%s (drawing with it)"), name) : name);
        sw->SetName(name);
        sw->SetCursor(wxCursor(wxCURSOR_HAND));
        sw->Bind(wxEVT_PAINT, [sw, c, chosen](wxPaintEvent &) {
            wxPaintDC dc(sw);
            dc.SetBrush(wxBrush(rgb(c)));
            dc.SetPen(wxPen(chosen ? role(MD3::Role::Primary) : role(MD3::Role::OutlineVariant), chosen ? 3 : 1));
            dc.DrawRoundedRectangle(1, 1, sw->GetSize().x - 2, sw->GetSize().y - 2, 6);
        });
        sw->Bind(wxEVT_LEFT_DOWN, [this, c](wxMouseEvent &) { CallAfter([this, c]() { set_color(c); }); });
        m_swatches->Add(sw, 0, wxRIGHT, FromDIP(4));
    }
    refresh_status();
    Layout();
    m_canvas->Refresh();
}

void DrawingPanel::set_pen(Pen pen)
{
    m_pen = pen;
    refresh_tools();
}

void DrawingPanel::set_color(std::uint32_t c)
{
    m_color = c & 0xFFFFFF;
    m_recent.erase(std::remove(m_recent.begin(), m_recent.end(), m_color), m_recent.end());
    m_recent.insert(m_recent.begin(), m_color);
    if (m_recent.size() > 8) m_recent.resize(8);
    if (m_pen == Pen::Eraser) m_pen = Pen::Marker;
    refresh_tools();
}

void DrawingPanel::pick_custom_color()
{
    MD3ColorPickerDialog::Options options;
    for (std::uint32_t c : m_recent) options.recent.push_back(rgb(c));
    options.opacity = false; // each pen keeps its own opacity
    options.title   = _L("Drawing colour");
    MD3ColorPickerDialog dialog(this, rgb(m_color), options);
    if (dialog.ShowModal() != wxID_OK) return;
    const wxColour c = dialog.GetColour();
    set_color((std::uint32_t(c.Red()) << 16) | (std::uint32_t(c.Green()) << 8) | c.Blue());
}

bool DrawingPanel::capture_plate(bool tell_when_unavailable)
{
    Plater    *plater = wxGetApp().plater();
    PartPlate *plate  = nullptr;
    if (plater) {
        plater->update_all_plate_thumbnails(true);
        plate = plater->get_partplate_list().get_plate(m_plate - 1);
    }
    if (!plate || !plate->thumbnail_data.is_valid()) {
        if (tell_when_unavailable)
            MessageDialog(this, _L("The plate could not be captured. Open Prepare once so the 3D view is ready, then try again."), _L("Drawing board"),
                          wxOK | wxICON_INFORMATION)
                .ShowModal();
        return false;
    }
    const ThumbnailData &t = plate->thumbnail_data;
    wxImage              image(int(t.width), int(t.height));
    image.InitAlpha();
    for (unsigned int y = 0; y < t.height; ++y)
        for (unsigned int x = 0; x < t.width; ++x) {
            // The 3D view hands rows over bottom first.
            const unsigned char *px = &t.pixels[(std::size_t(t.height - 1 - y) * t.width + x) * 4];
            image.SetRGB(int(x), int(y), px[0], px[1], px[2]);
            image.SetAlpha(int(x), int(y), px[3]);
        }
    m_background = wxBitmap(image);
    m_doc.width  = int(t.width);
    m_doc.height = int(t.height);
    std::error_code ec;
    fs::create_directories(m_folder, ec);
    image.SaveFile(wxString::FromUTF8(plate_file(m_folder, m_plate, ".png").u8string()), wxBITMAP_TYPE_PNG);
    save();
    m_canvas->Refresh();
    return true;
}

void DrawingPanel::choose_plate()
{
    Plater *plater = wxGetApp().plater();
    if (!plater) return;
    const int count = plater->get_partplate_list().get_plate_count();
    wxMenu    menu;
    for (int i = 1; i <= count; ++i) {
        wxMenuItem *item = menu.AppendRadioItem(wxID_HIGHEST + i, wxString::Format(_L("Plate %d"), i));
        item->Check(i == m_plate);
    }
    const int chosen = MD3::PopupMenuSelection(this, menu, m_plate_button->ClientToScreen(wxPoint(0, m_plate_button->GetSize().y)));
    if (chosen > wxID_HIGHEST && chosen <= wxID_HIGHEST + count) switch_plate(chosen - wxID_HIGHEST);
}

void DrawingPanel::switch_plate(int plate)
{
    if (plate == m_plate) return;
    save();
    m_plate = plate;
    m_doc   = Document{};
    m_versions.clear();
    m_background = wxBitmap();
    load();
    refresh_tools();
}

void DrawingPanel::load()
{
    m_title->SetLabel(wxString::Format(_L("Drawing on Plate %d"), m_plate));
    std::ifstream in(plate_file(m_folder, m_plate, ".json"), std::ios::binary);
    if (in) {
        std::stringstream text;
        text << in.rdbuf();
        from_json(text.str(), m_doc, m_versions);
    }
    const fs::path  png = plate_file(m_folder, m_plate, ".png");
    wxImage         image;
    std::error_code ec;
    if (fs::exists(png, ec) && image.LoadFile(wxString::FromUTF8(png.u8string()), wxBITMAP_TYPE_PNG)) m_background = wxBitmap(image);
    else capture_plate(false);
    m_history.reset(m_doc.strokes);
    refresh_versions();
    refresh_status();
}

void DrawingPanel::save()
{
    std::error_code ec;
    fs::create_directories(m_folder, ec);
    const fs::path file    = plate_file(m_folder, m_plate, ".json");
    const fs::path partial = plate_file(m_folder, m_plate, ".json.partial");
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        out << to_json(m_doc, m_versions);
        if (!out) return;
    }
    fs::rename(partial, file, ec);
    if (!ec) m_saved_at = std::int64_t(std::time(nullptr));
}

void DrawingPanel::changed()
{
    m_history.push(m_doc.strokes);
    save();
    refresh_status();
}

void DrawingPanel::undo()
{
    if (!m_history.can_undo()) return;
    m_doc.strokes = m_history.undo();
    save();
    refresh_status();
    m_canvas->Refresh();
}

void DrawingPanel::redo()
{
    if (!m_history.can_redo()) return;
    m_doc.strokes = m_history.redo();
    save();
    refresh_status();
    m_canvas->Refresh();
}

void DrawingPanel::keep_version(const std::string &label)
{
    m_versions.insert(m_versions.begin(), Version{std::int64_t(std::time(nullptr)), label, m_doc.strokes});
    if (m_versions.size() > 20) m_versions.resize(20);
    save();
    refresh_versions();
    refresh_status();
}

void DrawingPanel::restore_version(int index)
{
    if (index < 0 || index >= int(m_versions.size())) return;
    m_doc.strokes = m_versions[std::size_t(index)].strokes;
    changed();
    m_canvas->Refresh();
}

void DrawingPanel::clear_all()
{
    if (m_doc.strokes.empty()) return;
    SuperConfirmGate::Spec spec;
    spec.action      = _L("Clear the drawing");
    spec.consequence = wxString::Format(_L("All %d strokes on Plate %d are removed. The drawing is kept as a version first, so it can be brought back."),
                                        int(m_doc.strokes.size()), m_plate);
    spec.affected    = {wxString::Format(_L("Drawing on Plate %d"), m_plate)};
    if (!SuperConfirmGate::Run(m_canvas, spec)) return;
    keep_version(into_u8(_L("Before clearing")));
    m_doc.strokes.clear();
    changed();
    m_canvas->Refresh();
}

void DrawingPanel::export_drawing()
{
    const wxString filters = _L("PNG image") + " (*.png)|*.png|" + _L("PNG at twice the size") + " (*.png)|*.png|" + _L("PNG at three times the size") +
                             " (*.png)|*.png|" + _L("PNG on a transparent background") + " (*.png)|*.png|" + _L("SVG with the plate capture") +
                             " (*.svg)|*.svg|" + _L("SVG of the strokes only") + " (*.svg)|*.svg";
    wxFileDialog dialog(this, _L("Export the drawing"), wxString(), wxString::Format("plate-%d-drawing", m_plate), filters,
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const int  kind = dialog.GetFilterIndex();
    wxString   path = dialog.GetPath();
    const bool svg  = kind >= 4;
    if (!path.Lower().EndsWith(svg ? ".svg" : ".png")) path += svg ? ".svg" : ".png";
    bool ok = false;
    if (!svg) {
        const double scale       = kind == 1 ? 2.0 : kind == 2 ? 3.0 : 1.0;
        const bool   transparent = kind == 3;
        const int    w           = int(std::lround(m_doc.width * scale));
        const int    h           = int(std::lround(m_doc.height * scale));
        wxImage      image(w, h);
        image.InitAlpha();
        std::fill_n(image.GetAlpha(), std::size_t(w) * std::size_t(h), static_cast<unsigned char>(transparent ? 0 : 255));
        if (!transparent) image.SetRGB(wxRect(0, 0, w, h), 255, 255, 255);
        {
            std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(image));
            if (gc) {
                if (!transparent && m_background.IsOk()) gc->DrawBitmap(m_background, 0, 0, w, h);
                paint_strokes(gc.get(), m_doc.strokes, scale);
            }
        }
        ok = image.SaveFile(path, wxBITMAP_TYPE_PNG);
    } else {
        std::string background;
        if (kind == 4 && m_background.IsOk()) {
            wxMemoryOutputStream stream;
            m_background.ConvertToImage().SaveFile(stream, wxBITMAP_TYPE_PNG);
            const std::size_t size = stream.GetSize();
            std::vector<char> bytes(size);
            stream.CopyTo(bytes.data(), size);
            background = into_u8(wxBase64Encode(bytes.data(), size));
        }
        std::ofstream out(fs::u8path(into_u8(path)), std::ios::binary | std::ios::trunc);
        out << to_svg(m_doc, 1.0f, background);
        ok = bool(out);
    }
    LocalTools::record_activity("drawing", L("Exported"), into_u8(wxFileName(path).GetFullName()), ok ? "Done" : "Failed");
    if (!ok) MessageDialog(this, _L("The drawing could not be saved there."), _L("Drawing board"), wxOK | wxICON_WARNING).ShowModal();
}

void DrawingPanel::refresh_status()
{
    const wxString dot  = wxString("  ") + wxUniChar(0x00B7) + "  ";
    wxString       text = m_saved_at > 0 ? wxString::Format(_L("Saved on this computer at %s"), wxDateTime(static_cast<time_t>(m_saved_at)).Format("%H:%M"))
                                         : _L("Saved on this computer as you draw");
    text << dot << wxString::Format(_L("%d strokes"), int(m_doc.strokes.size()));
    text << dot << wxString::Format(_L("%d versions kept"), int(m_versions.size()));
    text << dot << _L("never uploaded");
    m_status->SetLabel(text);
    if (m_undo) m_undo->Enable(m_history.can_undo());
    if (m_redo) m_redo->Enable(m_history.can_redo());
}

void DrawingPanel::refresh_versions()
{
    m_version_list->DeleteAllItems();
    for (const Version &v : m_versions) {
        wxVector<wxVariant> row;
        row.push_back(wxVariant(wxDateTime(static_cast<time_t>(v.time)).Format("%Y-%m-%d %H:%M")));
        row.push_back(wxVariant(wxString::FromUTF8(v.label)));
        row.push_back(wxVariant(wxString::Format("%d", int(v.strokes.size()))));
        m_version_list->AppendItem(row);
    }
}

}}} // namespace Slic3r::GUI::Drawing
