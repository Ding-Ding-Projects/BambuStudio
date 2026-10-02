#include "../PersonalVocabulary.hpp"
#include "DropDown.hpp"
#include "Label.hpp"
#include "StateColor.hpp"
#include "MaterialIcon.hpp"
#include "SearchField.hpp"
#include "MD3MenuModel.hpp"
#include "../I18N.hpp"

#include <cstdio>
#include <algorithm>
#include <wx/display.h>
#include <wx/dcbuffer.h>
#include <wx/dcgraph.h>

#ifdef __WXGTK__
#include <gtk/gtk.h>
#endif

#include <set>

wxDEFINE_EVENT(EVT_DISMISS, wxCommandEvent);

BEGIN_EVENT_TABLE(DropDown, PopupWindow)

EVT_LEFT_DOWN(DropDown::mouseDown)
EVT_LEFT_UP(DropDown::mouseReleased)
EVT_MOUSE_CAPTURE_LOST(DropDown::mouseCaptureLost)
EVT_MOTION(DropDown::mouseMove)
EVT_MOUSEWHEEL(DropDown::mouseWheelMoved)

// catch paint events
EVT_PAINT(DropDown::paintEvent)

END_EVENT_TABLE()

/*
 * Called by the system of by wxWidgets when the panel needs
 * to be redrawn. You can also trigger this call by
 * calling Refresh()/Update().
 */

DropDown::DropDown(std::vector<Item> &items)
    : items(items)
    , state_handler(this)
    // 1px popup border in OutlineVariant (its light value #c5c6d0 is a gDarkColors
    // key -> #4a4c54, so it live-remaps on a dark-mode toggle).
    , border_color(MD3::Light::outlineVariant)
    , text_color(std::make_pair(ThemeColor::TextDisabled, (int) StateColor::Disabled),
        std::make_pair(ThemeColor::TextPrimary, (int) StateColor::Normal))
    , selector_border_color(std::make_pair(ThemeColor::BrandGreen, (int) StateColor::Hovered),
        std::make_pair(ThemeColor::White, (int) StateColor::Normal))
    // Selected-row highlight: SecondaryContainer, the MD3 selected/active tonal
    // (replacing the raw 0xEDFAF2). Its light value #d7e8d9 is a gDarkColors key
    // (-> #2b3a2f), so colorForStates() re-adapts it live on a dark-mode toggle,
    // unlike a semantic() snapshot that would freeze at the construction theme.
    , selector_background_color(std::make_pair(MD3::Light::secondaryContainer, (int) StateColor::Checked),
        std::make_pair(MD3::Light::secondaryContainer, (int) StateColor::Normal))
{
}

DropDown::DropDown(wxWindow *parent, std::vector<Item> &items, long style)
    : DropDown(items)
{
    Create(parent, style);
}

void DropDown::Create(wxWindow *parent, long style)
{
    PopupWindow::Create(parent, wxPU_CONTAINS_CONTROLS);
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    // Floating surface fill: SurfaceContainer. Stored as the raw light role value
    // (#eeedf3, a gDarkColors key), so render()'s darkModeColorFor() maps it to the
    // dark tone (#25262b) every paint and it follows a runtime dark-mode toggle.
    SetBackgroundColour(MD3::Light::sc);
    // Rounded floating card. FromDIP so the radius tracks the monitor DPI; kept in
    // sync on DPI change via Rescale().
    radius = FromDIP(18);
    state_handler.attach({&border_color, &text_color, &selector_border_color, &selector_background_color});
    state_handler.update_binds();
    if ((style & DD_NO_CHECK_ICON) == 0)
        check_bitmap = ScalableBitmap(this, "checked", 16);
    arrow_bitmap = ScalableBitmap(this, "hms_arrow", 16);
    text_off = style & DD_NO_TEXT;

    // BBS set default font
    SetFont(Label::Body_14);
    m_search = new SearchField(this, _L("Search menu"));
    m_search->SetOnQuery([this](const wxString &) { applyFilter(); });
    m_search->SetOnRegexToggle([this](bool) { applyFilter(); });
    Bind(wxEVT_CHAR_HOOK, &DropDown::onCharHook, this);
#ifdef __WXOSX__
    // PopupWindow releases mouse on idle, which may cause various problems,
    //  such as losting mouse move, and dismissing soon on first LEFT_DOWN event.
    Bind(wxEVT_IDLE, [] (wxIdleEvent & evt) {});
#endif
}

void DropDown::Invalidate(bool clear)
{
    if (clear) {
        selection = hover_item = -1;
        offset = wxPoint();
    }
    assert(selection < (int) items.size());
    need_sync = true;
}

void DropDown::SetSelection(int n)
{
    if (n >= (int) items.size())
        n = -1;
    if (selection == n) return;
    selection = n;
    if (need_sync) { // for icon Size
        messureSize();
        need_sync = true;
    }
    if (subDropDown)
        subDropDown->SetSelection(n);
    paintNow();
}

wxString DropDown::GetValue() const
{
    return selection >= 0 ? items[selection].text : wxString();
}

void DropDown::SetValue(const wxString &value)
{
    auto i    = std::find_if(items.begin(), items.end(), [&value](Item & item) { return item.text == value; });
    selection = i == items.end() ? -1 : std::distance(items.begin(), i);
}

void DropDown::SetCornerRadius(double radius)
{
    this->radius = radius;
    paintNow();
}

void DropDown::SetBorderColor(StateColor const &color)
{
    border_color = color;
    state_handler.update_binds();
    paintNow();
}

void DropDown::SetSelectorBorderColor(StateColor const &color)
{
    selector_border_color = color;
    state_handler.update_binds();
    paintNow();
}

void DropDown::SetTextColor(StateColor const &color)
{
    text_color = color;
    state_handler.update_binds();
    paintNow();
}

void DropDown::SetSelectorBackgroundColor(StateColor const &color)
{
    selector_background_color = color;
    state_handler.update_binds();
    paintNow();
}

void DropDown::SetColorScheme(MD3::ColorScheme scheme)
{
    m_scheme = scheme;
    paintNow();
}

void DropDown::SetUseContentWidth(bool use, bool limit_max_content_width)
{
    if (use_content_width == use)
        return;
    use_content_width = use;
    this->limit_max_content_width = limit_max_content_width;
    need_sync = true;
    messureSize();
}

void DropDown::SetAlignIcon(bool align) { align_icon = align; }

void DropDown::Rescale()
{
    radius    = FromDIP(18);
    if (m_search) m_search->Rescale();
    need_sync = true;
}

int DropDown::headerHeight() const { return FromDIP(60); }
int DropDown::viewportHeight() const { return std::max(0, GetClientSize().y - headerHeight()); }

void DropDown::Popup(wxWindow *)
{
    need_sync = true;
    autoPosition();
    PopupWindow::Popup(m_search->GetTextCtrl());
    m_search->GetTextCtrl()->SetFocus();
}

void DropDown::rebuildRows()
{
    m_visible.clear();
    SearchField::MatchPass match(m_search->GetValue(), m_search->IsRegexEnabled(),
        m_search->IsCaseSensitive(), m_search->IsWholeWord(), m_search->IsMultiline());
    std::set<wxString> groups;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        const auto &item = items[i];
        if (!group.IsEmpty() && item.group != group) continue;
        if (!m_search->GetValue().IsEmpty() &&
            !match.matches(item.text + " " + item.group + " " + item.alias + " " + item.tip)) continue;
        if (group.IsEmpty() && !item.group.IsEmpty()) {
            if (groups.insert(item.group).second) m_visible.push_back(-i - 2);
        } else {
            m_visible.push_back(i);
        }
    }
    count = m_visible.size();
}

void DropDown::applyFilter()
{
    if (subDropDown && subDropDown->IsShown()) {
        subDropDown->Hide();
        subDropDown->group.clear();
    }
    offset = wxPoint();
    hover_item = -1;
    need_sync = true;
    autoPosition();
    SetName(count == 0 ? _L("No matches.") : wxString::Format(_L("%d results"), static_cast<int>(count)));
    Refresh();
}

void DropDown::activateHover()
{
    const int index = hoverIndex();
    if (index < -1 && subDropDown) {
        subDropDown->group = items[-index - 2].group;
        subDropDown->Popup();
    } else if (index >= 0 && !(items[index].style & (DD_ITEM_STYLE_DISABLED | DD_ITEM_STYLE_SPLIT_ITEM))) {
        sendDropDownEvent();
        if (mainDropDown) {
            Hide();
            mainDropDown->DismissAndNotify();
        } else {
            DismissAndNotify();
        }
    }
}

void DropDown::onCharHook(wxKeyEvent &event)
{
    if (m_search->IsBuilderShown()) { event.Skip(); return; }
    if (event.GetKeyCode() == WXK_ESCAPE) {
        if (!m_search->GetValue().IsEmpty()) m_search->Clear();
        else if (mainDropDown) { Hide(); mainDropDown->m_search->GetTextCtrl()->SetFocus(); }
        else DismissAndNotify();
        return;
    }
    if (event.GetKeyCode() == WXK_TAB) {
        std::vector<wxWindow *> stops{m_search->GetTextCtrl()};
        for (wxWindow *child : m_search->GetChildren())
            if (child != m_search->GetTextCtrl() && child->IsShown() && child->IsEnabled() && child->AcceptsFocusFromKeyboard())
                stops.push_back(child);
        const auto current = std::find(stops.begin(), stops.end(), wxWindow::FindFocus());
        const size_t index = current == stops.end() ? stops.size() - 1 : static_cast<size_t>(current - stops.begin());
        stops[event.ShiftDown() ? (index + stops.size() - 1) % stops.size() : (index + 1) % stops.size()]->SetFocus();
        return;
    }
    if (event.GetKeyCode() == WXK_UP || event.GetKeyCode() == WXK_DOWN) {
        const int delta = event.GetKeyCode() == WXK_UP ? -1 : 1;
        for (size_t n = 0; n < count; ++n) {
            hover_item = hover_item < 0 ? (delta > 0 ? 0 : static_cast<int>(count) - 1)
                : (hover_item + delta + static_cast<int>(count)) % static_cast<int>(count);
            const int index = hoverIndex();
            if (index < -1 || (index >= 0 && !(items[index].style & (DD_ITEM_STYLE_DISABLED | DD_ITEM_STYLE_SPLIT_ITEM)))) break;
        }
        if (hover_item >= 0) {
            offset.y = std::min(offset.y, -hover_item * rowSize.y);
            offset.y = std::max(offset.y, viewportHeight() - (hover_item + 1) * rowSize.y);
            offset.y = std::clamp(offset.y, std::min(0, viewportHeight() - static_cast<int>(count) * rowSize.y), 0);
        }
        Refresh();
        return;
    }
    wxWindow *focus = wxWindow::FindFocus();
    if ((event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER) &&
        (focus == this || focus == m_search->GetTextCtrl())) {
        activateHover();
        return;
    }
    event.Skip();
}

bool DropDown::HasDismissLongTime()
{
    auto now = boost::posix_time::microsec_clock::universal_time();
    return !IsShown() &&
        (now - dismissTime).total_milliseconds() >= 20;
}

void DropDown::paintEvent(wxPaintEvent& evt)
{
    // depending on your system you may need to look at double-buffered dcs
    wxBufferedPaintDC dc(this);
    render(dc);
}

/*
 * Alternatively, you can use a clientDC to paint on the panel
 * at any time. Using this generally does not free you from
 * catching paint events, since it is possible that e.g. the window
 * manager throws away your drawing when the window comes to the
 * background, and expects you will redraw it when the window comes
 * back (by sending a paint event).
 */
void DropDown::paintNow()
{
    // depending on your system you may need to look at double-buffered dcs
    //wxClientDC dc(this);
    //render(dc);
    Refresh();
}

static wxSize GetBmpSize(wxBitmap & bmp)
{
    if (!bmp.IsOk())
        return wxSize(0, 0);
#ifdef __APPLE__
    return bmp.GetScaledSize();
#else
    return bmp.GetSize();
#endif
}

static void _DrawSplitItem(const wxWindow* w, wxDC& dc, wxString split_text, wxPoint start_pt, int item_width, int item_height)
{
    // save dc
    auto pre_clr = dc.GetTextForeground();
    auto pre_pen = dc.GetPen();
    dc.SetTextForeground(StateColor::darkModeColorFor(ThemeColor::Grey450));
    dc.SetPen(StateColor::darkModeColorFor(ThemeColor::Grey450));
    // miner font
    auto font = w->GetFont();
    font.SetPointSize(font.GetPointSize() - 3);
    dc.SetFont(font);

    int spacing = w->FromDIP(8);

    if (!split_text.empty())// Paiting: text + spacing + line + spacing
    {
        int max_content_width = item_width - start_pt.x - 2 * spacing;
        wxSize tSize = dc.GetMultiLineTextExtent(split_text);
        if (tSize.x > max_content_width)
        {
            split_text = wxControl::Ellipsize(split_text, dc, wxELLIPSIZE_END, max_content_width);
            tSize = dc.GetMultiLineTextExtent(split_text);
        }

        dc.SetFont(font);
        dc.DrawText(split_text, start_pt);

        int line_width = item_width - start_pt.x - tSize.x - 2 * spacing;
        int line_y = start_pt.y + (tSize.GetHeight() / 2);
        dc.DrawLine(start_pt.x + tSize.x + spacing, line_y, start_pt.x + tSize.x + line_width + spacing, line_y);// draw right line
    }
    else// Paiting: line + spacing
    {
        int line_y = start_pt.y + (item_height / 2);
        int line_width = item_width - start_pt.x - spacing;
        dc.DrawLine(start_pt.x, line_y, start_pt.x + line_width, line_y);// draw line
    }

    // restore dc
    dc.SetTextForeground(pre_clr);
    dc.SetPen(pre_pen);
    dc.SetFont(w->GetFont());
}

/*
 * Here we do the actual rendering. I put it in a separate
 * method so that it can work no matter what type of DC
 * (e.g. wxPaintDC or wxClientDC) is used.
 */
void DropDown::render(wxDC &dc)
{
    // Clear the buffer to the themed surface colour first so the rounded-corner
    // triangles left outside DrawRoundedRectangle are clean, not uninitialised.
    dc.SetBackground(wxBrush(StateColor::darkModeColorFor(GetBackgroundColour())));
    dc.Clear();
    int states = state_handler.states();
    if (subDropDown)
        states |= subDropDown->state_handler.states();
    dc.SetPen(wxPen(border_color.colorForStates(states)));
    dc.SetBrush(wxBrush(StateColor::darkModeColorFor(GetBackgroundColour())));
    // if (GetWindowStyle() & wxBORDER_NONE)
    //    dc.SetPen(wxNullPen);

    // draw background
    wxSize size = GetSize();
    if (radius == 0)
        dc.DrawRectangle(0, 0, size.x, size.y);
    else
        dc.DrawRoundedRectangle(0, 0, size.x, size.y, radius);

    int selected_item = selectedItem();
    int hover_index   = hoverIndex();

    // Row highlights are rounded fills inset from the row edges: a
    // SurfaceContainerHigh hover pane and a SecondaryContainer selected pane (the
    // MD3 selected/active tonal), replacing the legacy hard 1px border rects.
    const int row_radius = FromDIP(8);
    const int inset_x    = FromDIP(4);
    const int inset_y    = FromDIP(1);
    const int header = headerHeight();
    wxDCClipper content_clip(dc, wxRect(0, header, size.x, std::max(0, size.y - header)));
    if (m_visible.empty()) {
        dc.SetFont(GetFont());
        dc.SetTextForeground(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        dc.DrawLabel(_L("No matches."), wxRect(0, header, size.x, viewportHeight()), wxALIGN_CENTER);
        return;
    }
    wxRect rcContent = {{0, offset.y + header}, rowSize};

    // hover pane (skip the selected row -- its own fill takes over below)
    if (hover_item >= 0 && hover_item != selected_item && ((states & StateColor::Hovered) || wxWindow::FindFocus() == m_search->GetTextCtrl()) &&
        (hover_index < 0 || !(items[hover_index].style & (DD_ITEM_STYLE_SPLIT_ITEM | DD_ITEM_STYLE_DISABLED)))) {
        wxRect rc = rcContent;
        rc.y += rowSize.y * hover_item;
        if (rc.GetBottom() > 0 && rc.y < size.y) {
            rc.Deflate(inset_x, inset_y);
            dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHigh)));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRoundedRectangle(rc, row_radius);
        }
    }
    // selected pane
    if (selected_item >= 0) {
        wxRect rc = rcContent;
        rc.y += rowSize.y * selected_item;
        if (rc.GetBottom() > 0 && rc.y < size.y) {
            rc.Deflate(inset_x, inset_y);
            // Non-Brand schemes (Preview/Device) resolve live per paint so the
            // selected row tints with the owning workspace's accent instead of
            // always Brand-green; Brand keeps the existing gDarkColors-mapped field.
            const wxColour fill = m_scheme == MD3::ColorScheme::Brand
                ? selector_background_color.colorForStates(states | StateColor::Checked)
                : StateColor::semantic(MD3::Role::SecondaryContainer, m_scheme);
            dc.SetBrush(wxBrush(fill));
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRoundedRectangle(rc, row_radius);
        }
    }
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    {
        wxSize offset = (rowSize - textSize) / 2;
        rcContent.Deflate(0, offset.y);
    }

    // draw position bar -- rounded OutlineVariant thumb (drops the Grey300 fill)
    if (rowSize.y * count > static_cast<size_t>(viewportHeight())) {
        const int height = rowSize.y * static_cast<int>(count);
        const int viewport = viewportHeight();
        wxRect rect = {size.x - FromDIP(6), header - offset.y * viewport / height, FromDIP(4),
                       std::max(FromDIP(12), viewport * viewport / height)};
        wxColour thumb = StateColor::semantic(MD3::Role::OutlineVariant);
        dc.SetPen(wxPen(thumb));
        dc.SetBrush(wxBrush(thumb));
        dc.DrawRoundedRectangle(rect, 2);
        rcContent.width -= 6;
    }

    // draw check icon -- Material Symbols Check glyph on the selected row (the
    // raster stays as the graceful fallback when the icon font is unavailable)
    rcContent.x += 5;
    rcContent.width -= 5;
    if (check_bitmap.bmp().IsOk()) {
        auto szBmp = check_bitmap.GetBmpSize();
        if (selected_item >= 0) {
            wxPoint pt = rcContent.GetLeftTop();
            pt.y += (rcContent.height - szBmp.y) / 2;
            pt.y += rowSize.y * selected_item;
            if (pt.y + szBmp.y > 0 && pt.y < size.y) {
                if (MaterialIcon::available())
                    MaterialIcon::drawCentered(dc, MaterialIcon::Check, 16,
                        StateColor::semantic(MD3::Role::OnSecondaryContainer), wxRect(pt, szBmp));
                else
                    dc.DrawBitmap(check_bitmap.bmp(), pt);
            }
        }
        rcContent.x += szBmp.x + 5;
        rcContent.width -= szBmp.x + 5;
    }

    // Every visible row retains its original item identity.
    int index = 0;
    for (int row : m_visible) {
        const int i = row < -1 ? -row - 2 : row;
        auto &item = items[i];
        int states2 = states;
        bool is_dimmed = (item.style & DD_ITEM_STYLE_DIMMED) != 0;
        if ((item.style & DD_ITEM_STYLE_DISABLED) != 0)
            states2 &= ~StateColor::Enabled;
        if (row < -1) states2 |= StateColor::Enabled;
        bool is_hover = index == hover_item;
        ++index;
        if (rcContent.GetBottom() < 0) {
            rcContent.y += rowSize.y;
            continue;
        }
        if (rcContent.y > size.y) break;
        wxPoint pt   = rcContent.GetLeftTop();

        if (item.style & DD_ITEM_STYLE_SPLIT_ITEM) {
            _DrawSplitItem(this, dc, item.text, pt, rowSize.GetWidth(), rowSize.GetHeight());
            rcContent.y += rowSize.GetHeight();
            continue;
        }

        const bool is_top_level_group = group.IsEmpty() && !item.group.IsEmpty();
        auto &  icon  = item.icon;
        auto size2 = GetBmpSize(icon);
        if (iconSize.x > 0) {
            if (!is_top_level_group && icon.IsOk()) {
                pt.y += (rcContent.height - size2.y) / 2;
                dc.DrawBitmap(icon, pt);
            }
            pt.x += iconSize.x + 5;
            pt.y = rcContent.y;
        } else if (!is_top_level_group && icon.IsOk()) {
            pt.y += (rcContent.height - size2.y) / 2;
            dc.DrawBitmap(icon, pt);
            pt.x += size2.x + 5;
            pt.y = rcContent.y;
        }
        // When the icon is a full-row pre-rendered bitmap (height >> text height),
        // it already contains all text; skip drawing text on top to avoid duplicates.
        const bool icon_fills_row = !is_top_level_group && icon.IsOk()
                                    && size2.y > textSize.y * 2;
        auto strip_brand_prefix = [](const wxString &text, const wxString &grp) -> wxString {
            if (grp.EndsWith(' ')) return text;
            wxString prefix = grp.BeforeFirst(' ');
            if (prefix.IsEmpty()) prefix = grp;
            if (text.StartsWith(prefix))
                return text.substr(prefix.size()).Trim(false);
            return text;
        };
        auto text = group.IsEmpty()
                        ? (item.group.IsEmpty() ? item.text : item.group)
                        : strip_brand_prefix(item.text, group);
        text = Slic3r::GUI::PersonalVocabulary::display(text);
        if (!text_off && !text.IsEmpty() && !icon_fills_row) {
            wxSize tSize = dc.GetMultiLineTextExtent(text);
            if (pt.x + tSize.x > rcContent.GetRight()) {
                if (is_hover && item.tip.IsEmpty())
                    SetToolTip(text);
                text = wxControl::Ellipsize(text, dc, wxELLIPSIZE_END,
                                            std::max(1, rcContent.GetRight() - pt.x));
            }
            pt.y += (rcContent.height - textSize.y) / 2;
            dc.SetFont(GetFont());
            dc.SetTextForeground(is_dimmed ? ThemeColor::Grey400 : text_color.colorForStates(states2));
            dc.DrawText(text, pt);
            if (group.IsEmpty() && !item.group.IsEmpty()) {
                auto szBmp = arrow_bitmap.GetBmpSize();
                pt.x = rcContent.GetRight() - szBmp.x - 5;
                pt.y = rcContent.y + (rcContent.height - szBmp.y) / 2;
                // submenu affordance -- Material Symbols ChevronRight glyph
                if (MaterialIcon::available())
                    MaterialIcon::drawCentered(dc, MaterialIcon::ChevronRight, 16,
                        StateColor::semantic(MD3::Role::OnSurfaceVariant), wxRect(pt, szBmp));
                else
                    dc.DrawBitmap(arrow_bitmap.bmp(), pt);
            }
        }
        rcContent.y += rowSize.y;
    }
}

int DropDown::hoverIndex()
{
    return hover_item >= 0 && hover_item < static_cast<int>(m_visible.size()) ? m_visible[hover_item] : -1;
}

int DropDown::selectedItem()
{
    const auto it = std::find(m_visible.begin(), m_visible.end(), selection);
    return selection >= 0 && it != m_visible.end() ? static_cast<int>(it - m_visible.begin()) : -1;
}

void DropDown::messureSize()
{
    if (!need_sync) return;
    rebuildRows();
    textSize = wxSize();
    iconSize = wxSize();
    wxClientDC dc(GetParent() ? GetParent() : this);
    dc.SetFont(GetFont());
    std::set<wxString> groups;
    for (int row : m_visible) {
        const int i = row < -1 ? -row - 2 : row;
        auto &item = items[i];
        if (row < -1) groups.insert(item.group);
        wxSize size1;
        if (!text_off) {
            auto strip_brand_prefix = [](const wxString &text, const wxString &grp) -> wxString {
                if (grp.EndsWith(' ')) return text;
                wxString prefix = grp.BeforeFirst(' ');
                if (prefix.IsEmpty()) prefix = grp;
                if (text.StartsWith(prefix))
                    return text.substr(prefix.size()).Trim(false);
                return text;
            };
            auto text = group.IsEmpty()
                        ? (item.group.IsEmpty() ? item.text : item.group)
                        : strip_brand_prefix(item.text, group);
        text = Slic3r::GUI::PersonalVocabulary::display(text);
            size1 = dc.GetMultiLineTextExtent(text);
            if (group.IsEmpty() && !item.group.IsEmpty())
                size1.x += 5 + arrow_bitmap.GetBmpWidth();
        }
        const bool is_top_level_group = group.IsEmpty() && !item.group.IsEmpty();
        if (!is_top_level_group && item.icon.IsOk()) {
            wxSize size2 = GetBmpSize(item.icon);
            if (size2.x > iconSize.x)
                iconSize = size2;
            if (!align_icon) {
                // Full-row bitmap (icon height >> text height): width = bitmap width only.
                if (size2.y > size1.y * 2)
                    size1.x = size2.x;
                else
                    size1.x += size2.x + (text_off ? 0 : 5);
            }
        }
        textSize.x = std::max(textSize.x, size1.x);
        textSize.y = std::max(textSize.y, size1.y);
    }
    if (!align_icon) iconSize.x = 0;
    wxSize szContent = textSize;
    if (szContent.x < FromDIP(120))
        szContent.x = FromDIP(120);
    szContent.x += 10;
    if (check_bitmap.bmp().IsOk()) {
        auto szBmp = check_bitmap.GetBmpSize();
        szContent.x += szBmp.x + 5;
    }
    if (iconSize.x > 0) szContent.x += iconSize.x + (text_off ? 0 : 5);
    if (iconSize.y > szContent.y) szContent.y = iconSize.y;
    szContent.y = std::max(szContent.y + 10, FromDIP(40));
    if (count > (size_t)max_visible_rows) szContent.x += 6;
    if (GetParent() && group.IsEmpty()) {
        auto x = GetParent()->GetSize().x;
        if (x > 0 && (!use_content_width || x > szContent.x))
            szContent.x = x;
    }
    szContent.x = std::max(szContent.x, FromDIP(320));
    rowSize = szContent;
    if (limit_max_content_width) {
        wxSize parent_size = GetParent()->GetSize();
        const int max_w = std::max(FromDIP(320), std::min(
            static_cast<int>(parent_size.x * 1.5), GetParent()->FromDIP(400)));
        if (rowSize.x > max_w) {
            rowSize.x = max_w;
            szContent  = rowSize;
        }
    }
    szContent.y *= std::min((size_t)max_visible_rows, std::max(count, (size_t) 1));
    szContent.y += count > (size_t)max_visible_rows ? rowSize.y / 2 : 0;
    szContent.y += headerHeight();
    wxWindow::SetSize(szContent);
#ifdef __WXGTK__
    // Gtk has a wrapper window for popup widget
    gtk_window_resize (GTK_WINDOW (m_widget), szContent.x, szContent.y);
#endif
    if (!groups.empty() && subDropDown == nullptr) {
        subDropDown = new DropDown(items);
        subDropDown->mainDropDown = this;
        subDropDown->check_bitmap      = check_bitmap;
        subDropDown->text_off          = text_off;
        subDropDown->use_content_width       = true;
        subDropDown->limit_max_content_width = true;
        subDropDown->max_visible_rows        = 8;
        subDropDown->Create(GetParent());
        subDropDown->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &e) {
            e.SetEventObject(this);
            e.SetId(GetId());
            GetEventHandler()->ProcessEvent(e);
        });
#ifdef __WXGTK__
        subDropDown->Bind(wxEVT_IDLE, [this](wxIdleEvent &evt) {
            if (!subDropDown || !subDropDown->IsShown())
                return;
            wxPoint mouse_pos = wxGetMousePosition();
            wxRect sub_rect = subDropDown->GetScreenRect();
            if (!sub_rect.Contains(mouse_pos)) {
                wxPoint local_pt = ScreenToClient(mouse_pos);
                wxMouseEvent mouse_evt(wxEVT_MOTION);
                mouse_evt.SetX(local_pt.x);
                mouse_evt.SetY(local_pt.y);
                wxPostEvent(this, mouse_evt);
                evt.RequestMore();
            }
        });
#endif
    }
    need_sync = false;
}

void DropDown::autoPosition()
{
    messureSize();
    int display_index = wxDisplay::GetFromWindow(GetParent());
    if (display_index == wxNOT_FOUND) display_index = 0;
    const wxRect display = wxDisplay(static_cast<unsigned>(display_index)).GetClientArea();
    MD3::Menu::Placement placement;
    if (mainDropDown) {
        const auto at = mainDropDown->ClientToScreen(wxPoint(0, mainDropDown->headerHeight() +
            mainDropDown->hover_item * mainDropDown->rowSize.y + mainDropDown->offset.y));
        placement = MD3::Menu::place_submenu(wxRect(at, mainDropDown->rowSize), GetSize(), display);
    } else {
        placement = MD3::Menu::place_root(GetParent()->GetScreenRect(), GetSize(), display);
    }
    SetSize(placement.rect);
    rowSize.x = placement.rect.width;
    m_search->SetSize(FromDIP(8), FromDIP(8), std::max(1, placement.rect.width - FromDIP(16)), FromDIP(44));
    offset.y = std::clamp(offset.y, std::min(0, viewportHeight() - static_cast<int>(count) * rowSize.y), 0);
}

void DropDown::mouseDown(wxMouseEvent& event)
{
    // Receivce unexcepted LEFT_DOWN on Mac after OnDismiss
    if (!IsShown())
        return;
    if (event.GetY() < headerHeight()) return;
    // force calc hover item again
    mouseMove(event);
    pressedDown = true;
    CaptureMouse();
    dragStart   = event.GetPosition();
}

void DropDown::mouseReleased(wxMouseEvent& event)
{
    if (pressedDown) {
        dragStart = wxPoint();
        pressedDown = false;
        if (HasCapture())
            ReleaseMouse();
        if (hover_item < 0)
            return;

        // If the clicked row is a top-level group header, open (or focus) the drill-down
        // submenu instead of dismissing the dropdown. Previously this path only fired via
        // hover in mouseMove — which is flaky for narrow (1-row) groups and unreachable
        // via keyboard/tap. Clicking on the header is the natural UX for a two-level menu.
        int idx = hoverIndex();
        if (idx < -1 && subDropDown) {
            const wxString &target_group = items[-idx - 2].group;
            auto &drop = *subDropDown;
            if (drop.group != target_group) {
                drop.group     = target_group;
                drop.need_sync = true;
                drop.messureSize();
                drop.autoPosition();
                drop.paintNow();
            }
            if (!drop.IsShown())
                drop.Popup(&drop);
            return;
        }

        if (hover_item >= 0 && (subDropDown == nullptr || subDropDown->group.empty())) { // not moved
            sendDropDownEvent();
            if (mainDropDown)
                mainDropDown->hover_item = -1; // To Dismiss mainDropDown
            DismissAndNotify();
        } else if (subDropDown)
            subDropDown->Popup(subDropDown);
    }
}

void DropDown::mouseCaptureLost(wxMouseCaptureLostEvent &event)
{
    wxMouseEvent evt;
    mouseReleased(evt);
}

void DropDown::mouseMove(wxMouseEvent &event)
{
    wxPoint pt  = event.GetPosition();
#ifdef __WXOSX__
    if (mainDropDown) {
        auto size = GetSize();
        if (pt.x < 0 || pt.y < 0 || pt.x >= size.x || pt.y >= size.y) {
            auto diff = GetPosition() - mainDropDown->GetPosition();
            event.SetX(pt.x + diff.x);
            event.SetY(pt.y + diff.y);
            mainDropDown->mouseMove(event);
            return;
        }
    }
#endif
    if (pressedDown) {
        wxPoint pt2 = offset + pt - dragStart;
        wxSize size(GetSize().x, viewportHeight());
        dragStart    = pt;
        if (pt2.y > 0)
            pt2.y = 0;
        else if (pt2.y + rowSize.y * int(count) < size.y)
            pt2.y = size.y - rowSize.y * int(count);
        pt2.y = std::clamp(pt2.y, std::min(0, size.y - rowSize.y * static_cast<int>(count)), 0);
    if (pt2.y != offset.y) {
            offset = pt2;
            hover_item = -1; // moved
        } else {
            return;
        }
    }
    if (rowSize.y > 0 && (!pressedDown || hover_item >= 0)) {
        int hover = pt.y < headerHeight() ? -1 : (pt.y - headerHeight() - offset.y) / rowSize.y;
        if (hover >= (int) count) hover = -1;
        if (hover == hover_item) return;
        hover_item = hover;
        int index  = hoverIndex();
        if (index < -1) {
            auto & drop = *subDropDown;
            drop.group  = items[-index - 2].group;
            drop.need_sync = true;
            drop.messureSize();
            drop.autoPosition();
            drop.paintNow();
            if (!drop.IsShown())
                drop.Popup(&drop);
        } else if (index >= 0) {
            if (subDropDown) {
                subDropDown->group.clear();
                if (subDropDown->IsShown())
                    subDropDown->Dismiss();
            }
            SetToolTip(items[index].tip);
        }
    }
    paintNow();
}

void DropDown::mouseWheelMoved(wxMouseEvent &event)
{
    auto delta = event.GetWheelRotation();
    wxSize size(GetSize().x, viewportHeight());
    wxPoint pt2   = offset + wxPoint{0, delta};
    if (pt2.y > 0)
        pt2.y = 0;
    else if (pt2.y + rowSize.y * int(count) < size.y)
        pt2.y = size.y - rowSize.y * int(count);
    pt2.y = std::clamp(pt2.y, std::min(0, size.y - rowSize.y * static_cast<int>(count)), 0);
    if (pt2.y != offset.y) {
        offset = pt2;
    } else {
        return;
    }
    int hover = event.GetY() < headerHeight() ? -1 : (event.GetY() - headerHeight() - offset.y) / rowSize.y;
    if (hover >= (int) count) hover = -1;
    if (hover != hover_item) {
        hover_item = hover;
        if (auto index = hoverIndex(); index >= 0)
            SetToolTip(items[index].tip);
    }
    paintNow();
}

// currently unused events
void DropDown::sendDropDownEvent()
{
    int index = hoverIndex();
    if (index < 0 || (items[index].style & (DD_ITEM_STYLE_DISABLED | DD_ITEM_STYLE_SPLIT_ITEM)))
        return;
    wxCommandEvent event(wxEVT_COMBOBOX, GetId());
    event.SetEventObject(this);
    event.SetInt(index);
    event.SetString(items[index].text);
    GetEventHandler()->ProcessEvent(event);
}

void DropDown::Dismiss()
{
    if (m_search && m_search->IsBuilderShown()) return;
    if (subDropDown && subDropDown->IsShown())
        return;
    PopupWindow::Dismiss();
}

void DropDown::OnDismiss()
{
    if (m_search && m_search->IsBuilderShown()) return;
    if (mainDropDown) {

        const wxPoint& mouse_pos = wxGetMousePosition();
        if (!mainDropDown->GetScreenRect().Contains(mouse_pos))
            mainDropDown->DismissAndNotify();
        else
#ifdef __WIN32__
            SetActiveWindow(mainDropDown->GetHandle());
#else
            ;
#endif
        return;
    }
    if (subDropDown && subDropDown->IsShown())
        return;
    dismissTime = boost::posix_time::microsec_clock::universal_time();
    hover_item  = -1;
    if (GetParent() && GetParent()->IsShown() && GetParent()->IsEnabled()) GetParent()->SetFocus();
    wxCommandEvent e(EVT_DISMISS);
    GetEventHandler()->ProcessEvent(e);
}
