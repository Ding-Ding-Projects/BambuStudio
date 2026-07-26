#ifndef slic3r_GUI_TabCtrl_hpp_
#define slic3r_GUI_TabCtrl_hpp_

#include "Button.hpp"

wxDECLARE_EVENT( wxEVT_TAB_SEL_CHANGING, wxCommandEvent );
wxDECLARE_EVENT( wxEVT_TAB_SEL_CHANGED, wxCommandEvent );

class TabCtrl : public StaticBox
{
    std::vector<Button*> btns;
    wxImageList* images = nullptr;
    wxBoxSizer * sizer = nullptr;

    int sel = -1;
    wxFont bold;

    // Current workspace accent scheme. The active-tab label and the active
    // indicator resolve Primary through this scheme so a sub-tab strip hosted in
    // a Preview/Device workspace recolours to that context (defaults to Brand).
    MD3::ColorScheme m_scheme = MD3::ColorScheme::Brand;

    // Opt-in MD3 NavItem-pill styling (preset-editor setting-category nav). Off
    // by default so the flat secondary-tab-strip consumers (media storage tabs,
    // user-preset collections) keep the underline-indicator look. When on, each
    // item renders as an h44 r22 pill (selected SecondaryContainer, hover
    // SurfaceContainerHigh, idle blending into the strip) and the Primary
    // underline indicator is suppressed. Toggled via SetNavItemStyle().
    bool m_pill_style = false;

public:
    TabCtrl(wxWindow *      parent,
             wxWindowID      id,
             const wxPoint & pos       = wxDefaultPosition,
             const wxSize &  size      = wxDefaultSize,
             long            style     = 0);

    ~TabCtrl();

public:
    virtual bool SetFont(wxFont const & font) override;

public:
    int AppendItem(const wxString &item, int image = -1, int selImage = -1, void *clientData = nullptr);

    bool DeleteItem(int item);

    void DeleteAllItems();

    unsigned int GetCount() const;

    int  GetSelection() const;

    void SelectItem(int item);

    void Unselect();

    virtual void Rescale();

    wxString GetItemText(unsigned int item) const;
    void     SetItemText(unsigned int item, wxString const &value);

    bool     GetItemBold(unsigned int item) const;
    void     SetItemBold(unsigned int item, bool bold);

    void*    GetItemData(unsigned int item) const;
    void     SetItemData(unsigned int item, void *clientData);

    void AssignImageList(wxImageList *imageList);

    void SetItemPaddingSize(unsigned int item, const wxSize &size);

    void SetItemTextColour(unsigned int item, const StateColor &col);

    // Optional leading Material Symbols glyph for one item, drawn ~20px leading
    // the label in the item's state-resolved text colour (selected accent / idle
    // OnSurfaceVariant; state is expressed through colour, never the FILL axis).
    // Capability-gated inside Button::SetGlyph (MaterialIcon::available()): when
    // the face is missing it falls back to the raster-icon path, leaving the
    // assigned wxImageList mechanism intact. codepoint 0 clears the glyph.
    void SetItemGlyph(unsigned int item, uint32_t glyph);

    // Enable/disable MD3 NavItem-pill styling for the whole control (see
    // m_pill_style). Restyles every existing item and every item appended after.
    void SetNavItemStyle(bool pill);

    // Re-bake the (theme-varying) pill fill for every item. The pill background
    // StateColor captures the current theme's SecondaryContainer/High at build
    // time, so a runtime theme toggle (which does not recreate the item buttons)
    // must call this to refresh it. No-op when not in pill mode; the item text
    // colour is refreshed by the owner (Tab::update_changed_tree_ui).
    void RefreshItemStyles();

    // Recolour the active-label accent and active indicator for the current
    // workspace scheme (Brand / Preview / Device). Re-applies the scheme-aware
    // default label colour to every tab and repaints the indicator.
    void SetColorScheme(MD3::ColorScheme scheme);

    /* fakes */
    int GetFirstVisibleItem() const;
    int GetNextVisible(int item) const;
    bool IsVisible(unsigned int item) const;

private:
    virtual void DoSetSize(int x, int y, int width, int height, int sizeFlags = wxSIZE_AUTO);

#ifdef __WIN32__
    WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) override;
#endif

    void relayout();

    // Scheme-aware default tab label colour. In pill mode: active (Checked) ->
    // OnSecondaryContainer on the SecondaryContainer pill, inactive ->
    // OnSurfaceVariant. In the flat strip: active -> Primary in the current
    // scheme, inactive -> OnSurfaceVariant. Weight (600/400) is carried
    // separately by the bold-font mechanism, and the leading glyph inherits this
    // colour via Button::render().
    StateColor tabTextColor() const;

    // Pill-mode background StateColor: selected SecondaryContainer (current
    // scheme), hover SurfaceContainerHigh, idle = the strip background.
    StateColor navPillBg() const;

    // Apply the current mode's geometry + fill to one item button (pill vs flat).
    void applyItemStyle(Button *btn);

    void buttonClicked(wxCommandEvent & event);
    void keyDown(wxKeyEvent &event);

    void doRender(wxDC & dc) override;

    // some useful events
    bool sendTabCtrlEvent(bool changing = false);

    DECLARE_EVENT_TABLE()
};

#endif // !slic3r_GUI_TabCtrl_hpp_
