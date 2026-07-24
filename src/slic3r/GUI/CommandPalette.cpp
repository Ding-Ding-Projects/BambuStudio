#include "CommandPalette.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "Plater.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/SwitchButton.hpp"
#include "Widgets/StateColor.hpp"

#include <wx/dcbuffer.h>
#include <wx/menu.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>

#include "libslic3r/AppConfig.hpp"

namespace Slic3r::GUI {

namespace {

constexpr int kWidth      = 640;
constexpr int kListHeight = 420;
constexpr int kRowHeight  = 52;

std::uint32_t glyph_for_menu(const wxString &top)
{
    if (top.Contains(_L("File")))        return MaterialIcon::FolderOpen;
    if (top.Contains(_L("Edit")))        return MaterialIcon::Edit;
    if (top.Contains(_L("View")))        return MaterialIcon::Visibility;
    if (top.Contains(_L("Objects")))     return MaterialIcon::DeployedCode;
    if (top.Contains(_L("Calibration"))) return MaterialIcon::Build;
    if (top.Contains(_L("Help")))        return MaterialIcon::Help;
    return MaterialIcon::ChevronRight;
}

} // namespace

CommandPalette::CommandPalette(MainFrame *frame)
    : wxDialog(frame, wxID_ANY, _L("Command palette"), wxDefaultPosition, wxDefaultSize,
               wxBORDER_SIMPLE)
    , m_frame(frame)
{
    SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    auto *root = new wxBoxSizer(wxVERTICAL);

    // TRN: Placeholder of the command palette's search field.
    m_search = new SearchField(this, _L("Search commands, settings and pages"));
    root->Add(m_search, 0, wxEXPAND | wxALL, FromDIP(12));

    m_list = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition,
                                  wxSize(FromDIP(kWidth), FromDIP(kListHeight)),
                                  wxVSCROLL | wxBORDER_NONE);
    m_list->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    m_list->SetScrollRate(0, FromDIP(kRowHeight));
    m_list->SetSizer(new wxBoxSizer(wxVERTICAL));
    root->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    SetSizerAndFit(root);
    SetClientSize(FromDIP(kWidth), FromDIP(kListHeight) + m_search->GetSize().GetHeight() + FromDIP(36));

    collect_entries();
    rebuild_rows();

    m_search->SetOnQuery([this](const wxString &) { rebuild_rows(); });
    m_search->SetOnRegexToggle([this](bool) { rebuild_rows(); });

    // Keyboard driving: the search entry keeps focus; arrows/Enter/Esc are
    // intercepted before they reach the text control.
    auto on_key = [this](wxKeyEvent &e) {
        switch (e.GetKeyCode()) {
        case WXK_DOWN:   select_row(m_selected + 1); return;
        case WXK_UP:     select_row(m_selected - 1); return;
        case WXK_RETURN: run_selected(); return;
        case WXK_ESCAPE: EndModal(wxID_CANCEL); return;
        default: e.Skip();
        }
    };
    m_search->GetTextCtrl()->Bind(wxEVT_KEY_DOWN, on_key);
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_ESCAPE) { EndModal(wxID_CANCEL); return; }
        e.Skip();
    });
}

void CommandPalette::ShowPalette(MainFrame *frame)
{
    CommandPalette palette(frame);
    palette.CenterOnParent();
    palette.ShowModal();
}

void CommandPalette::collect_entries()
{
    m_entries.clear();

    // --- Rich quick-settings rows (always near the top) ---------------------
    m_entries.push_back({MaterialIcon::Palette, _L("Theme"),
                         _L("Switch between the light and dark appearance"),
                         nullptr, Rich::Theme});
    m_entries.push_back({MaterialIcon::Tune, _L("Density"),
                         _L("Comfortable or compact control spacing"),
                         nullptr, Rich::Density});
    m_entries.push_back({MaterialIcon::Palette, _L("Accent color"),
                         _L("Pick the accent seed the interface is tinted with"),
                         nullptr, Rich::Accent});

    // --- Navigation ---------------------------------------------------------
    struct Nav { std::uint32_t glyph; wxString title; wxString desc; size_t tab; };
    const std::vector<Nav> navs = {
        {MaterialIcon::Home,       _L("Go to Home"),        _L("Start page with recent projects"), 0},
        {MaterialIcon::ViewInAr,   _L("Go to Prepare"),     _L("Arrange models and set up the print"), 1},
        {MaterialIcon::Layers,     _L("Go to Preview"),     _L("Sliced result, layers and toolpaths"), 2},
        {MaterialIcon::Cast,       _L("Go to Device"),      _L("Printer status and control"), 3},
        {MaterialIcon::FolderOpen, _L("Go to Project"),     _L("Project files and metadata"), 4},
    };
    for (const Nav &n : navs) {
        MainFrame *frame = m_frame;
        m_entries.push_back({n.glyph, n.title, n.desc,
                             [frame, tab = n.tab]() { frame->select_tab(tab); }});
    }

    // --- Feature landmarks ---------------------------------------------------
    m_entries.push_back({MaterialIcon::Settings, _L("Open Preferences"),
                         _L("General, appearance, 3D and developer settings — with live search"),
                         [this]() { wxGetApp().open_preferences(); }});
    m_entries.push_back({MaterialIcon::Search, _L("Search in settings"),
                         _L("Find any print / filament / printer parameter"),
                         [this]() { if (auto *p = wxGetApp().plater()) p->search(); }});

    // --- Every enabled menubar command ---------------------------------------
    wxMenuBar *bar = m_frame->GetMenuBar();
    if (bar != nullptr) {
        for (size_t m = 0; m < bar->GetMenuCount(); ++m) {
            const wxString top = wxMenuItem::GetLabelText(bar->GetMenuLabel(m));
            std::function<void(wxMenu *, const wxString &)> walk =
                [&](wxMenu *menu, const wxString &path) {
                    for (wxMenuItem *item : menu->GetMenuItems()) {
                        if (item->IsSeparator())
                            continue;
                        const wxString label = wxMenuItem::GetLabelText(item->GetItemLabel());
                        if (item->GetSubMenu() != nullptr) {
                            walk(item->GetSubMenu(), path + " / " + label);
                            continue;
                        }
                        if (!item->IsEnabled())
                            continue;
                        MainFrame *frame = m_frame;
                        const int  id    = item->GetId();
                        m_entries.push_back({glyph_for_menu(top), path + " / " + label,
                                             item->GetHelp(),
                                             [frame, id]() {
                                                 wxCommandEvent evt(wxEVT_MENU, id);
                                                 frame->GetEventHandler()->AddPendingEvent(evt);
                                             }});
                    }
                };
            walk(bar->GetMenu(m), top);
        }
    }
}

wxPanel *CommandPalette::make_row(const Entry &entry, int index)
{
    const wxColour on     = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour on_var = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    const wxColour base   = StateColor::semantic(MD3::Role::SurfaceContainerLow);

    auto *row = new wxPanel(m_list, wxID_ANY);
    row->SetBackgroundColour(base);
    row->SetMinSize(wxSize(-1, FromDIP(kRowHeight)));
    auto *sizer = new wxBoxSizer(wxHORIZONTAL);

    auto *icon = new wxPanel(row, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(36), FromDIP(36)));
    icon->SetBackgroundColour(base);
    const std::uint32_t glyph = entry.glyph;
    icon->Bind(wxEVT_PAINT, [icon, glyph](wxPaintEvent &) {
        wxPaintDC dc(icon);
        if (MaterialIcon::available()) {
            const int px = icon->FromDIP(22);
            const wxSize gs = MaterialIcon::measure(dc, glyph, px);
            MaterialIcon::draw(dc, glyph, px, StateColor::semantic(MD3::Role::OnSurfaceVariant),
                               wxPoint((icon->GetSize().x - gs.x) / 2, (icon->GetSize().y - gs.y) / 2));
        }
    });
    sizer->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));

    auto *text_col = new wxBoxSizer(wxVERTICAL);
    auto *title = new Label(row, Label::Body_14, entry.title);
    title->SetBackgroundColour(base);
    title->SetForegroundColour(on);
    text_col->Add(title, 0);
    if (!entry.desc.IsEmpty()) {
        auto *desc = new Label(row, Label::Body_12, entry.desc);
        desc->SetBackgroundColour(base);
        desc->SetForegroundColour(on_var);
        text_col->Add(desc, 0, wxTOP, FromDIP(1));
    }
    sizer->Add(text_col, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));

    if (entry.rich != Rich::None)
        add_rich_controls(row, sizer, entry.rich);

    row->SetSizer(sizer);

    auto activate = [this, index]() { select_row(index); run_selected(); };
    for (wxWindow *w : {static_cast<wxWindow *>(row), static_cast<wxWindow *>(icon),
                        static_cast<wxWindow *>(title)})
        w->Bind(wxEVT_LEFT_DOWN, [activate, entry](wxMouseEvent &e) {
            if (entry.rich == Rich::None)
                activate();
            e.Skip();
        });
    return row;
}

void CommandPalette::add_rich_controls(wxPanel *row, wxBoxSizer *sizer, Rich rich)
{
    AppConfig *cfg = wxGetApp().app_config;
    if (rich == Rich::Theme || rich == Rich::Density) {
        auto *seg = new MultiSwitchButton(row);
        if (rich == Rich::Theme) {
            seg->SetOptions({_L("Light"), _L("Dark")});
            seg->SetSelection(cfg->get("dark_color_mode") == "1" ? 1 : 0);
            seg->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [cfg](wxCommandEvent &e) {
                cfg->set("dark_color_mode", e.GetInt() == 1 ? "1" : "0");
                cfg->save();
                wxGetApp().Update_dark_mode_flag();
#ifdef _MSW_DARK_MODE
                wxGetApp().force_colors_update();
                wxGetApp().update_ui_from_settings();
#endif
                e.Skip();
            });
        } else {
            seg->SetOptions({_L("Comfortable"), _L("Compact")});
            seg->SetSelection(cfg->get("ui_density") == "compact" ? 1 : 0);
            seg->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [cfg](wxCommandEvent &e) {
                const bool compact = e.GetInt() == 1;
                cfg->set("ui_density", compact ? "compact" : "comfortable");
                cfg->save();
                MD3::Metrics::setDensity(compact ? MD3::Metrics::Density::Compact
                                                 : MD3::Metrics::Density::Comfortable);
                e.Skip();
            });
        }
        seg->SetMinSize(wxSize(FromDIP(190), FromDIP(28)));
        sizer->Add(seg, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
        return;
    }
    // Accent: six mini seed swatches, same seeds as Preferences ▸ Appearance.
    const std::vector<wxString> seeds = {"#146c2e", "#7c5cff", "#14b8a6",
                                         "#2563eb", "#d81b60", "#ea580c"};
    for (const wxString &hex : seeds) {
        auto *sw = new wxPanel(row, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(22), FromDIP(22)));
        sw->SetBackgroundColour(wxColour(hex));
        sw->Bind(wxEVT_LEFT_DOWN, [hex](wxMouseEvent &) {
            AppConfig *c = wxGetApp().app_config;
            c->set("ui_accent_seed", hex.ToStdString());
            c->save();
            MD3::setAccentSeed(wxColour(hex));
            wxGetApp().update_ui_from_settings();
        });
        sizer->Add(sw, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    }
    sizer->AddSpacer(FromDIP(4));
}

void CommandPalette::rebuild_rows()
{
    const wxString query = m_search->GetValue();
    const bool regex      = m_search->IsRegexEnabled();
    const bool case_sense = m_search->IsCaseSensitive();
    const bool whole_word = m_search->IsWholeWord();

    m_list->Freeze();
    m_list->GetSizer()->Clear(true);
    m_rows.clear();
    m_visible.clear();
    m_selected = -1;

    const wxColour outline = StateColor::semantic(MD3::Role::OutlineVariant);
    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
        const Entry &entry = m_entries[i];
        if (!query.IsEmpty() &&
            !SearchField::textMatches(query, entry.title + " " + entry.desc, regex, case_sense, whole_word))
            continue;
        wxPanel *row = make_row(entry, static_cast<int>(m_visible.size()));
        m_visible.push_back(i);
        m_rows.push_back(row);
        m_list->GetSizer()->Add(row, 0, wxEXPAND);
        auto *divider = new wxPanel(m_list, wxID_ANY, wxDefaultPosition, wxSize(-1, 1));
        divider->SetBackgroundColour(outline);
        m_list->GetSizer()->Add(divider, 0, wxEXPAND | wxLEFT, FromDIP(56));
        if (m_rows.size() >= 120)
            break; // keep the palette instant; refine the query for more
    }
    m_list->FitInside();
    m_list->Thaw();
    if (!m_rows.empty())
        select_row(0);
}

void CommandPalette::select_row(int index)
{
    if (m_rows.empty())
        return;
    index = std::max(0, std::min(index, static_cast<int>(m_rows.size()) - 1));
    const wxColour base = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour sel  = StateColor::semantic(MD3::Role::SurfaceContainerHighest);
    if (m_selected >= 0 && m_selected < static_cast<int>(m_rows.size()))
        m_rows[m_selected]->SetBackgroundColour(base), m_rows[m_selected]->Refresh();
    m_selected = index;
    m_rows[m_selected]->SetBackgroundColour(sel);
    m_rows[m_selected]->Refresh();
    // keep the selection visible
    int sy = 0;
    m_list->GetViewStart(nullptr, &sy);
    const int row_units = m_selected; // one scroll unit per row (incl. divider rounding)
    if (row_units < sy)
        m_list->Scroll(0, row_units);
    else {
        const int visible = m_list->GetClientSize().GetHeight() / FromDIP(kRowHeight);
        if (row_units >= sy + visible)
            m_list->Scroll(0, row_units - visible + 1);
    }
}

void CommandPalette::run_selected()
{
    if (m_selected < 0 || m_selected >= static_cast<int>(m_visible.size()))
        return;
    const Entry &entry = m_entries[m_visible[m_selected]];
    if (entry.rich != Rich::None)
        return; // rich rows act through their inline controls
    if (entry.run) {
        EndModal(wxID_OK);
        entry.run();
    }
}

} // namespace Slic3r::GUI
