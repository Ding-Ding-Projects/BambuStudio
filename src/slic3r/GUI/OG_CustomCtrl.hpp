#ifndef slic3r_OG_CustomCtrl_hpp_
#define slic3r_OG_CustomCtrl_hpp_

#include <wx/stattext.h>
#include <wx/settings.h>

#include <map>
#include <functional>

#include "libslic3r/Config.hpp"
#include "libslic3r/PrintConfig.hpp"

#include "OptionsGroup.hpp"
#include "I18N.hpp"

// Translate the ifdef 
#ifdef __WXOSX__
    #define wxOSX true
#else
    #define wxOSX false
#endif

namespace Slic3r { namespace GUI {

//  Static text shown among the options.
class OG_CustomCtrl :public wxPanel
{
    wxFont  m_font;
    int     m_v_gap;
    int     m_v_gap2;
    int     m_h_gap;
    int     m_em_unit;

    wxSize  m_bmp_mode_sz;
    wxSize  m_bmp_blinking_sz;

    int     m_max_win_width{0};

    struct CtrlLine {
        wxCoord           width{ wxDefaultCoord };
        wxCoord           height{ wxDefaultCoord };
        OG_CustomCtrl*    ctrl    { nullptr };
        const Line&       og_line;

        bool draw_just_act_buttons  { false };
        bool draw_mode_bitmap       { true };
        bool is_visible             { true };
        bool is_focused             { false };
        // Stacked row (OptionsGroup::stack_full_width_label): the height of the label line at the
        // top of the row, including its gap; the buttons and the field sit in the band below it.
        // Zero for every other row, so the row's height is one band.
        wxCoord label_band          { 0 };

        CtrlLine(   wxCoord         height,
                    OG_CustomCtrl*  ctrl,
                    const Line&     og_line,
                    bool            draw_just_act_buttons = false,
                    bool            draw_mode_bitmap = true);
        ~CtrlLine() { ctrl = nullptr; }

        int     get_max_win_width();
        void    correct_items_positions();
        void    msw_rescale();
        void    update_visibility(ConfigOptionMode mode);

        void render_separator(wxDC& dc, wxCoord v_pos);

        void    render(wxDC& dc, wxCoord h_pos, wxCoord v_pos);
        wxCoord draw_text      (wxDC& dc, wxPoint pos, const wxString& text, const wxColour* color, int width, bool is_url = false, bool is_main = false);
        wxPoint draw_blinking_bmp(wxDC& dc, wxPoint pos, bool is_blinking);
        wxCoord draw_act_bmps(wxDC& dc, wxPoint pos, const wxBitmap& bmp_undo_to_sys, const wxBitmap& bmp_undo, bool is_blinking, size_t rect_id = 0, bool skip_vertical_adjust = false);
        bool    launch_browser() const;
        bool    is_separator() const { return og_line.is_separator(); }
        void    ensure_rects_size(size_t size);
        void    update_multi_variant_height();
        // A labelled single row-wide option in a group that stacks: label above, field below.
        bool    is_stacked() const;
        // Height from the label line and the field window. init_ctrl_lines runs before
        // build_field, so the field may not exist yet; then the field line is one text line
        // until update_visibility measures the real window.
        void    update_stacked_height();

        std::vector<wxRect> rects_undo_icon;
        std::vector<wxRect> rects_undo_to_sys_icon;
        wxRect              rect_label;
        // Bilingual mode: "廣東話：…" for a main label whose Cantonese did not
        // fit the label column; the label tooltip shows it instead.
        wxString            bilingual_note;
    };

    std::vector<CtrlLine> ctrl_lines;

public:
    OG_CustomCtrl(  wxWindow* parent,
                    OptionsGroup* og,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxDefaultSize,
                    const wxValidator& val = wxDefaultValidator,
                    const wxString& name = wxEmptyString);
    ~OG_CustomCtrl() {}

    void    OnPaint(wxPaintEvent&);
    void    OnMotion(wxMouseEvent& event);
    void    OnLeftDown(wxMouseEvent& event);
    void    OnLeaveWin(wxMouseEvent& event);

    void    init_ctrl_lines();
    bool    update_visibility(ConfigOptionMode mode);
    void    correct_window_position(wxWindow* win, const Line& line, Field* field = nullptr);
    void    correct_widgets_position(wxSizer* widget, const Line& line, Field* field = nullptr);
    void    init_max_win_width();
    void    set_max_win_width(int max_win_width);
    int     get_max_win_width() { return m_max_win_width; }

    //BBS
    int    get_title_width();
    // BBS
    void fixup_items_positions();

    void    msw_rescale();
    void    sys_color_changed();

    wxPoint get_pos(const Line& line, Field* field = nullptr);
    int     get_height(const Line& line);
    // The label band of a stacked row (0 for every other row): the field band starts this far
    // below the row top.
    int     get_label_band(const Line& line);

    void    update_line_height_for_field(const t_config_option_key &opt_id);
    void    recalculate_and_refresh();

    OptionsGroup*  opt_group;

};

}}

#endif /* slic3r_OG_CustomCtrl_hpp_ */
