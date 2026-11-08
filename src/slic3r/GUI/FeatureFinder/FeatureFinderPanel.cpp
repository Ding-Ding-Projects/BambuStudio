#include "FeatureFinderPanel.hpp"
#include "FeatureCommands.hpp"

#include "../CommandPalette.hpp"
#include "../Documentation/OfflineDocumentation.hpp"
#include "../FeatureServices/PresentationRoutes.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../NotificationManager.hpp"
#include "../PersonalModes/SchoolMode.hpp"
#include "../Plater.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/StaticBox.hpp"

#include "libslic3r/AppConfig.hpp"

#include <wx/dcclient.h>
#include <wx/sizer.h>

#include <algorithm>
#include <map>

namespace Slic3r::GUI {

namespace {

using FeatureFinder::Badge;
using FeatureFinder::Feature;
using FeatureFinder::SortOrder;
using FeatureFinder::State;

constexpr int kRowHeight = 56;

// Card layout per column count, from the FeatureFinder board.
const std::vector<std::vector<const char *>> &layout_for(int columns)
{
    static const std::vector<std::vector<const char *>> three = {
        {"language-voice", "look"}, {"find", "records"}, {"tools", "safety"}};
    static const std::vector<std::vector<const char *>> two = {
        {"language-voice", "look", "find"}, {"records", "tools", "safety"}};
    static const std::vector<std::vector<const char *>> one = {
        {"language-voice", "look", "find", "records", "tools", "safety"}};
    return columns >= 3 ? three : columns == 2 ? two : one;
}

// Documentation the School presentation hides (the palette applies the same rule).
bool docs_hidden_by_school(const std::string &path)
{
    return path.find("dim-sum") != std::string::npos || path.find("personal-vocabulary") != std::string::npos ||
           path.find("language") != std::string::npos || path.find("funny") != std::string::npos;
}

wxString search_haystack(const Feature &f)
{
    wxString text = _(f.title);
    text << ' ' << _(f.where) << ' ' << wxString::FromUTF8(f.shortcut) << ' ' << wxString::FromUTF8(f.keywords);
    return text;
}

void notify(const wxString &message)
{
    if (auto *plater = wxGetApp().plater(); plater && plater->get_notification_manager())
        plater->get_notification_manager()->push_notification(into_u8(message));
}

} // namespace

FeatureFinderPanel::FeatureFinderPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY)
{
    SetName("every-feature");
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    const int pad = FromDIP(MD3::Metrics::active().padding);

    if (AppConfig *cfg = wxGetApp().app_config) {
        m_sort   = FeatureFinder::parse_sort(cfg->get(FeatureFinder::kSortKey));
        m_recent = FeatureFinder::parse_recent(cfg->get(FeatureFinder::kRecentKey));
    }

    auto *root = new wxBoxSizer(wxVERTICAL);

    // Header: title and count on the left, the search pill on the right.
    auto *header = new wxBoxSizer(wxHORIZONTAL);
    auto *heading = new wxBoxSizer(wxVERTICAL);
    auto *title = new Label(this, Label::Head_20, _L("Every feature"));
    title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    heading->Add(title, 0);
    auto *subtitle = new Label(this, Label::Body_13,
        wxString::Format(_L("%d features in this version. Each says where it lives and opens in one click."),
                         int(FeatureFinder::features().size())));
    subtitle->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    heading->Add(subtitle, 0, wxTOP, FromDIP(4));
    header->Add(heading, 1, wxALIGN_CENTER_VERTICAL);
    // TRN: Placeholder of the search field on the Every feature page.
    m_search = new SearchField(this, _L("Search features, places and shortcuts"), wxDefaultPosition, wxSize(FromDIP(420), -1));
    header->Add(m_search, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(16));
    root->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    build_filters(root);

    // Recently opened: one tonal button per feature, hidden while empty.
    m_recent_panel = new wxPanel(this, wxID_ANY);
    m_recent_panel->SetBackgroundColour(GetBackgroundColour());
    m_recent_sizer = new wxBoxSizer(wxHORIZONTAL);
    m_recent_panel->SetSizer(m_recent_sizer);
    root->Add(m_recent_panel, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    m_list = new MD3ScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    m_list->SetBackgroundColour(GetBackgroundColour());
    m_list->SetScrollRate(0, FromDIP(24));
    m_list->SetSizer(new wxBoxSizer(wxVERTICAL));
    root->Add(m_list, 1, wxEXPAND | wxALL, pad);

    auto *footer = new Label(this, Label::Body_12,
        wxString::Format(_L("The command palette (%s) finds every feature too. A feature marked Not built yet is designed and planned; its Open button appears the day it ships."),
                         wxString::FromUTF8(PaletteIndex::palette_shortcut_label())));
    footer->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    root->Add(footer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, pad);

    SetSizer(root);

    m_search->SetOnQuery([this](const wxString &) { rebuild(); });
    m_search->SetOnRegexToggle([this](bool) { rebuild(); });
    m_list->Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
        event.Skip();
        const int columns = columns_for(event.GetSize().GetWidth());
        if (columns != m_columns) CallAfter([this]() { rebuild(); });
    });

    rebuild_recent();
    rebuild();
}

int FeatureFinderPanel::columns_for(int width) const
{
    if (width >= FromDIP(1180)) return 3;
    if (width >= FromDIP(760)) return 2;
    return 1;
}

void FeatureFinderPanel::style_toggle(Button *button, bool on)
{
    button->SetVariant(on ? Button::Variant::Tonal : Button::Variant::Outlined);
    button->SetButtonSize(Button::Size::Small);
    button->SetName(button->GetLabel() + (on ? wxString(" (") + _L("selected") + ")" : wxString()));
    button->Refresh();
}

void FeatureFinderPanel::build_filters(wxBoxSizer *root)
{
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    const auto add_filter = [&](const std::string &id, const wxString &label) {
        auto *button = new Button(this, label);
        m_filter_buttons.emplace_back(id, button);
        button->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) { set_filter(id); });
        row->Add(button, 0, wxRIGHT, FromDIP(8));
    };
    add_filter("", wxString::Format(_L("All %d"), int(FeatureFinder::features().size())));
    for (const FeatureFinder::Category &c : FeatureFinder::categories())
        add_filter(c.id, wxString::Format("%s %d", _(c.title), int(FeatureFinder::count_in(c.id))));
    // Only while something is still designed but not built: an empty chip says nothing.
    if (FeatureFinder::count_state(State::NotBuilt) > 0)
        add_filter(FeatureFinder::kNotBuiltFilter,
                   wxString::Format(_L("Not built yet %d"), int(FeatureFinder::count_state(State::NotBuilt))));
    row->AddStretchSpacer(1);

    auto *sort_label = new Label(this, Label::Body_13, _L("Sort"));
    sort_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    row->Add(sort_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    const std::pair<SortOrder, wxString> sorts[] = {
        {SortOrder::Group, _L("Group")}, {SortOrder::AToZ, _L("A to Z")}, {SortOrder::Recent, _L("Recently used")}};
    for (const auto &[order, label] : sorts) {
        auto *button = new Button(this, label);
        m_sort_buttons.emplace_back(order, button);
        const SortOrder value = order;
        button->Bind(wxEVT_BUTTON, [this, value](wxCommandEvent &) { set_sort(value); });
        row->Add(button, 0, wxLEFT, FromDIP(4));
    }
    root->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(MD3::Metrics::active().padding));

    for (auto &[id, button] : m_filter_buttons) style_toggle(button, id == m_filter);
    for (auto &[order, button] : m_sort_buttons) style_toggle(button, order == m_sort);
}

void FeatureFinderPanel::set_filter(const std::string &filter)
{
    m_filter = filter;
    for (auto &[id, button] : m_filter_buttons) style_toggle(button, id == m_filter);
    rebuild();
}

void FeatureFinderPanel::set_sort(SortOrder sort)
{
    m_sort = sort;
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(FeatureFinder::kSortKey, FeatureFinder::sort_value(sort));
    for (auto &[order, button] : m_sort_buttons) style_toggle(button, order == m_sort);
    rebuild();
}

wxString FeatureFinderPanel::unavailable_reason(const Feature &feature)
{
    if (feature.state != State::Ready) return _L("Not built yet");
    const bool school = PersonalModes::school_presentation_suppressed.load();
    const FeatureFinder::Action action = FeatureFinder::parse_action(feature.action);
    switch (action.kind) {
    case FeatureFinder::ActionKind::Preference:
        if (!FeatureServices::presentation_setting_available(action.target, school)) return _L("Hidden in School mode");
        return {};
    case FeatureFinder::ActionKind::Command:
        if (!FeatureFinder::Commands::instance().has(action.target)) return _L("Not available on this system");
        return {};
    case FeatureFinder::ActionKind::Documentation:
        if (school && docs_hidden_by_school(action.target)) return _L("Hidden in School mode");
        return {};
    case FeatureFinder::ActionKind::Palette: return {};
    case FeatureFinder::ActionKind::None: break;
    }
    return _L("Not available on this system");
}

bool FeatureFinderPanel::open(const Feature &feature)
{
    if (!unavailable_reason(feature).IsEmpty()) return false;
    const FeatureFinder::Action action = FeatureFinder::parse_action(feature.action);
    MainFrame *frame = wxGetApp().mainframe;
    switch (action.kind) {
    case FeatureFinder::ActionKind::Preference:
        wxGetApp().CallAfter([key = action.target]() { wxGetApp().open_preferences(key); });
        return true;
    case FeatureFinder::ActionKind::Command:
        wxGetApp().CallAfter([id = action.target]() {
            if (!FeatureFinder::Commands::instance().run(id)) notify(_L("This feature is not available on this system."));
        });
        return true;
    case FeatureFinder::ActionKind::Palette:
        if (frame == nullptr) return false;
        frame->CallAfter([frame]() { CommandPalette::ShowPalette(frame); });
        return true;
    case FeatureFinder::ActionKind::Documentation:
        if (frame == nullptr) return false;
        frame->CallAfter([frame, path = action.target]() {
            if (!Documentation::ShowOfflineDocumentation(frame, wxString::FromUTF8(path)))
                notify(_L("This article is not present in the installed documentation bundle."));
        });
        return true;
    case FeatureFinder::ActionKind::None: break;
    }
    return false;
}

void FeatureFinderPanel::open_and_remember(const Feature &feature)
{
    if (!open(feature)) return;
    m_recent = FeatureFinder::touch_recent(m_recent, feature.id);
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(FeatureFinder::kRecentKey, FeatureFinder::serialize_recent(m_recent));
    rebuild_recent();
    if (m_sort == SortOrder::Recent) CallAfter([this]() { rebuild(); });
}

void FeatureFinderPanel::rebuild_recent()
{
    m_recent_panel->Freeze();
    m_recent_sizer->Clear(true);
    if (!m_recent.empty()) {
        auto *label = new Label(m_recent_panel, Label::Body_13, _L("Recently opened"));
        label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        m_recent_sizer->Add(label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
        for (const std::string &id : m_recent) {
            const Feature *f = FeatureFinder::find(id);
            if (f == nullptr) continue;
            auto *button = new Button(m_recent_panel, _(f->title));
            button->SetVariant(Button::Variant::Tonal);
            button->SetButtonSize(Button::Size::Small);
            button->Bind(wxEVT_BUTTON, [this, f](wxCommandEvent &) { open_and_remember(*f); });
            m_recent_sizer->Add(button, 0, wxRIGHT, FromDIP(8));
        }
    }
    m_recent_panel->Show(!m_recent.empty());
    m_recent_panel->Thaw();
    Layout();
}

wxPanel *FeatureFinderPanel::make_row(wxWindow *parent, const Feature &feature)
{
    const wxColour base   = StateColor::semantic(MD3::Role::SurfaceContainer);
    const wxString reason = unavailable_reason(feature);
    const bool     ready  = reason.IsEmpty();

    auto *row = new wxPanel(parent, wxID_ANY);
    row->SetName(wxString::FromUTF8(feature.id));
    row->SetBackgroundColour(base);
    row->SetMinSize(wxSize(-1, FromDIP(kRowHeight)));
    auto *sizer = new wxBoxSizer(wxHORIZONTAL);

    const wxColour glyph_colour = StateColor::semantic(ready ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant);
    auto *icon = new wxPanel(row, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(32), FromDIP(32)));
    icon->SetBackgroundColour(base);
    const std::uint32_t glyph = feature.glyph;
    icon->Bind(wxEVT_PAINT, [icon, glyph, glyph_colour](wxPaintEvent &) {
        wxPaintDC dc(icon);
        if (!MaterialIcon::available()) return;
        const int    px = icon->FromDIP(22);
        const wxSize gs = MaterialIcon::measure(dc, glyph, px);
        MaterialIcon::draw(dc, glyph, px, glyph_colour,
                           wxPoint((icon->GetSize().x - gs.x) / 2, (icon->GetSize().y - gs.y) / 2));
    });
    sizer->Add(icon, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));

    auto *text = new wxBoxSizer(wxVERTICAL);
    auto *title = new Label(row, Label::Head_14, _(feature.title), LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    title->SetMinSize(wxSize(0, -1));
    title->SetBackgroundColour(base);
    title->SetForegroundColour(StateColor::semantic(ready ? MD3::Role::OnSurface : MD3::Role::OnSurfaceVariant));
    text->Add(title, 0, wxEXPAND);
    auto *where = new Label(row, Label::Body_12, _(feature.where), LB_AUTO_WRAP | wxST_NO_AUTORESIZE);
    where->SetMinSize(wxSize(0, -1));
    where->SetBackgroundColour(base);
    where->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    text->Add(where, 0, wxEXPAND | wxTOP, FromDIP(2));
    sizer->Add(text, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxTOP | wxBOTTOM, FromDIP(8));

    // Badge or real shortcut, then Open (or the honest reason there is none).
    wxString side;
    if (feature.badge == Badge::Fixed) side = _L("Fixed in this version");
    else if (feature.badge == Badge::New && ready) side = _L("New in this version");
    else if (*feature.shortcut != '\0') side = wxString::FromUTF8(feature.shortcut);
    if (!side.IsEmpty() && ready) {
        auto *chip = new Label(row, Label::Body_12, side);
        chip->SetBackgroundColour(base);
        chip->SetForegroundColour(StateColor::semantic(feature.badge == Badge::None ? MD3::Role::OnSurfaceVariant : MD3::Role::Primary));
        sizer->Add(chip, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    }
    if (ready) {
        auto *open_button = new Button(row, _L("Open"));
        open_button->SetVariant(Button::Variant::Outlined);
        open_button->SetButtonSize(Button::Size::Small);
        open_button->SetName(wxString::Format(_L("Open %s"), _(feature.title)));
        open_button->Bind(wxEVT_BUTTON, [this, f = &feature](wxCommandEvent &) { open_and_remember(*f); });
        sizer->Add(open_button, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(8));
        row->Bind(wxEVT_LEFT_DCLICK, [this, f = &feature](wxMouseEvent &) { open_and_remember(*f); });
    } else {
        auto *why = new Label(row, Label::Body_12, reason);
        why->SetBackgroundColour(base);
        why->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        sizer->Add(why, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(12));
        row->SetToolTip(feature.state == State::NotBuilt ?
            _L("Designed and planned for this app. It is not in this build yet.") : reason);
    }
    row->SetSizer(sizer);
    return row;
}

wxWindow *FeatureFinderPanel::make_card(wxWindow *parent, const wxString &title, const std::vector<std::size_t> &indices)
{
    auto *card = new StaticBox(parent);
    card->SetCornerRadius(FromDIP(20));
    card->SetBorderWidth(0);
    card->SetBackgroundColor(StateColor::semantic(MD3::Role::SurfaceContainer));
    card->SetName(title);
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    auto *head = new wxBoxSizer(wxHORIZONTAL);
    auto *label = new Label(card, Label::Head_12, title.Upper());
    label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    head->Add(label, 1, wxALIGN_CENTER_VERTICAL);
    auto *count = new Label(card, Label::Body_12, wxString::Format("%d", int(indices.size())));
    count->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    head->Add(count, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(head, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    const auto &all = FeatureFinder::features();
    for (std::size_t i : indices) sizer->Add(make_row(card, all[i]), 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(8));
    sizer->AddSpacer(FromDIP(12));
    card->SetSizer(sizer);
    return card;
}

void FeatureFinderPanel::rebuild()
{
    const wxString query = m_search->GetValue();
    SearchField::MatchPass match(query, m_search->IsRegexEnabled(), m_search->IsCaseSensitive(),
                                 m_search->IsWholeWord(), m_search->IsMultiline());
    const auto &all = FeatureFinder::features();
    const std::vector<std::size_t> ordered = FeatureFinder::order(m_sort, m_recent,
        [](const Feature &f) { return into_u8(_(f.title)); });

    std::vector<std::size_t> visible;
    for (std::size_t i : ordered) {
        if (!FeatureFinder::passes_filter(all[i], m_filter)) continue;
        if (!query.IsEmpty() && !match.matches(search_haystack(all[i]))) continue;
        visible.push_back(i);
    }

    m_list->Freeze();
    m_list->GetSizer()->Clear(true);
    m_columns = columns_for(m_list->GetClientSize().GetWidth());

    if (visible.empty()) {
        auto *empty = new Label(m_list, Label::Body_14, _L("No feature matches. Try fewer words, or turn off the regular expression."));
        empty->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        m_list->GetSizer()->Add(empty, 0, wxALL, FromDIP(16));
    } else if (m_sort == SortOrder::Group && query.IsEmpty() && m_filter.empty()) {
        // Category cards in balanced columns, as on the board.
        auto *columns = new wxBoxSizer(wxHORIZONTAL);
        for (const auto &column : layout_for(m_columns)) {
            auto *col = new wxBoxSizer(wxVERTICAL);
            for (const char *category : column) {
                const FeatureFinder::Category *c = FeatureFinder::find_category(category);
                if (c == nullptr) continue;
                std::vector<std::size_t> indices;
                for (std::size_t i : visible)
                    if (std::string_view(all[i].category) == category) indices.push_back(i);
                if (!indices.empty()) col->Add(make_card(m_list, _(c->title), indices), 0, wxEXPAND | wxBOTTOM, FromDIP(12));
            }
            columns->Add(col, 1, wxRIGHT, FromDIP(12));
        }
        m_list->GetSizer()->Add(columns, 0, wxEXPAND);
    } else {
        wxString title;
        if (!query.IsEmpty()) title = wxString::Format(_L("%d results"), int(visible.size()));
        else if (m_filter == FeatureFinder::kNotBuiltFilter) title = _L("Not built yet");
        else if (const FeatureFinder::Category *c = FeatureFinder::find_category(m_filter)) title = _(c->title);
        else title = m_sort == SortOrder::AToZ ? _L("A to Z") : _L("Recently used first");
        m_list->GetSizer()->Add(make_card(m_list, title, visible), 0, wxEXPAND);
    }

    m_list->FitInside();
    m_list->Layout();
    m_list->Thaw();
}

} // namespace Slic3r::GUI
