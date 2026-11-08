#include "LocalToolsHubPanel.hpp"
#include "FeatureCatalogue.hpp"
#include "FeatureCommands.hpp"
#include "FeatureFinderPanel.hpp"
#include "LocalToolsActivity.hpp"

#include "../ElementLocks/LockService.hpp"
#include "../Extensions/ExtensionRuntime.hpp"
#include "../GameServers/ServersPanel.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../MsgDialog.hpp"
#include "../SetupWizard/SetupModel.hpp"
#include "../VirtualDisks/DiskPlan.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/StaticBox.hpp"
#include "../Widgets/SuperConfirmGate.hpp"
#include "../Widgets/SwitchButton.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/libslic3r.h"
#include "libslic3r/Utils.hpp"

#include <wx/datetime.h>
#include <wx/dcclient.h>
#include <wx/dialog.h>
#include <wx/sizer.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>

namespace Slic3r::GUI {

namespace {

namespace fs = std::filesystem;
using LocalTools::Activity;
using LocalTools::Tool;

constexpr std::uint32_t kGlyphPin      = 0xF026; // keep
constexpr std::uint32_t kGlyphUnpin    = 0xE6F9; // keep_off
constexpr std::uint32_t kGlyphEarlier  = 0xE5CB; // chevron_left
constexpr std::uint32_t kGlyphLater    = 0xE5CC; // chevron_right
constexpr std::uint32_t kGlyphHistory  = 0xE28E; // history
constexpr std::uint32_t kGlyphRefresh  = 0xE5D5; // refresh
constexpr std::size_t   kRecentShown   = 8;
constexpr int           kActivityWidth = 340;

const FeatureFinder::Feature &feature_of(const Tool &tool)
{
    // tests/local_tools_hub_test.cpp proves every tool is a catalogue entry.
    return *FeatureFinder::find(tool.feature);
}

wxString time_text(std::int64_t time)
{
    const wxDateTime when(static_cast<time_t>(time));
    const wxDateTime today = wxDateTime::Today();
    const wxString   clock = when.Format("%H:%M:%S");
    if (when.IsSameDate(today)) return wxString::Format(_L("Today %s"), clock);
    if (when.IsSameDate(today - wxDateSpan::Day())) return wxString::Format(_L("Yesterday %s"), clock);
    return when.Format("%Y-%m-%d ") + clock;
}

wxString result_text(const std::string &result)
{
    if (result == "Failed") return _L("Failed");
    if (result == "Cancelled") return _L("Cancelled");
    if (result == "Incomplete") return _L("Incomplete");
    if (result == "Started") return _L("Started");
    return {};
}

// "Encoded report.pdf": the translated words, then the name.
wxString activity_text(const Activity &a)
{
    wxString text = _(wxString::FromUTF8(a.action));
    if (!a.subject.empty()) text << ' ' << wxString::FromUTF8(a.subject);
    return text;
}

Label *text_label(wxWindow *parent, const wxFont &font, const wxString &text, MD3::Role role, bool wrap = false)
{
    auto *label = new Label(parent, font, text, wrap ? long(LB_AUTO_WRAP | wxST_NO_AUTORESIZE) : 0L);
    if (wrap) label->SetMinSize(wxSize(0, -1));
    label->SetBackgroundColour(parent->GetBackgroundColour());
    label->SetForegroundColour(StateColor::semantic(role));
    return label;
}

wxPanel *glyph_panel(wxWindow *parent, std::uint32_t glyph, int box, int px, MD3::Role role)
{
    auto *icon = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(parent->FromDIP(box), parent->FromDIP(box)));
    icon->SetBackgroundColour(parent->GetBackgroundColour());
    const wxColour colour = StateColor::semantic(role);
    icon->Bind(wxEVT_PAINT, [icon, glyph, px, colour](wxPaintEvent &) {
        wxPaintDC dc(icon);
        if (!MaterialIcon::available()) return;
        const int    size = icon->FromDIP(px);
        const wxSize gs   = MaterialIcon::measure(dc, glyph, size);
        MaterialIcon::draw(dc, glyph, size, colour, wxPoint((icon->GetSize().x - gs.x) / 2, (icon->GetSize().y - gs.y) / 2));
    });
    return icon;
}

Button *small_button(wxWindow *parent, const wxString &label, Button::Variant variant, std::uint32_t glyph = 0)
{
    auto *button = new Button(parent, label);
    button->SetVariant(variant);
    button->SetButtonSize(Button::Size::Small);
    if (glyph != 0) button->SetGlyph(glyph);
    return button;
}

Button *icon_button(wxWindow *parent, std::uint32_t glyph, const wxString &name)
{
    auto *button = new Button(parent, wxString());
    button->SetIconButton(Button::IconShape::Circle, parent->FromDIP(32));
    button->SetGlyph(glyph);
    button->SetName(name);
    button->SetToolTip(name);
    return button;
}

std::string read_text(const fs::path &file)
{
    std::ifstream      in(file, std::ios::binary);
    std::ostringstream text;
    if (in) text << in.rdbuf();
    return text.str();
}

} // namespace

// ---------------------------------------------------------------- status lines

wxString LocalToolsHubPanel::status_of(const std::string &feature)
{
    const fs::path data = fs::u8path(data_dir());
    if (feature == "onboarding") {
        Setup::Progress progress;
        AppConfig      *cfg = wxGetApp().app_config;
        if (cfg == nullptr || !Setup::parse(cfg->get(Setup::kProgressKey), progress) || !Setup::has_saved_work(progress))
            return progress.finished ? _L("Finished") : _L("Not started");
        if (progress.finished) return _L("Finished");
        // The step it continues from, counted among the steps in this build.
        const int resume = Setup::resume_index(progress);
        int       total = 0, at = 1;
        for (int i = 0; i < Setup::kStepCount; ++i) {
            if (!Setup::info(i).available) continue;
            ++total;
            if (i <= resume) at = total;
        }
        return wxString::Format(_L("Step %d of %d saved"), at, total);
    }
    if (feature == "base64") {
        std::ifstream in(data / "base64-toolkit-history.jsonl", std::ios::binary);
        int           entries = 0;
        for (std::string line; std::getline(in, line);)
            if (line.find_first_not_of(" \r") != std::string::npos) ++entries;
        return entries > 0 ? wxString::Format(_L("In the history: %d"), entries) : wxString();
    }
    if (feature == "virtual-disks") {
        VirtualDisks::Library library;
        VirtualDisks::from_json(read_text(data / "virtual-disks.json"), library);
        return library.disks.empty() ? wxString() : wxString::Format(_L("In the library: %d"), int(library.disks.size()));
    }
    if (feature == "minecraft") {
        const GameServers::Summary s = GameServers::summary();
        if (s.running > 0) return wxString::Format(_L("Running: %d of %d"), s.running, s.servers);
        return s.servers > 0 ? wxString::Format(_L("Servers: %d, none running"), s.servers) : wxString();
    }
    if (feature == "drawing") {
        static const std::regex plate_file(R"(plate-\d+\.json)");
        std::error_code         ec;
        int                     plates = 0;
        for (fs::directory_iterator it(data / "drawings", ec), end; !ec && it != end; it.increment(ec))
            if (std::regex_match(it->path().filename().u8string(), plate_file)) ++plates;
        return plates > 0 ? wxString::Format(_L("Plates drawn on: %d"), plates) : wxString();
    }
    if (feature == "extensions") {
        const Extensions::Runtime::Summary s = Extensions::Runtime::instance().summary();
        if (!s.known) return {};
        if (s.waiting > 0) return wxString::Format(_L("On: %d, waiting for your answer: %d"), s.on, s.waiting);
        return wxString::Format(_L("On: %d"), s.on);
    }
    if (feature == "element-locks") {
        const int locks = ElementLocks::lock_count();
        return locks > 0 ? wxString::Format(_L("Locked: %d"), locks) : wxString();
    }
    return {};
}

// ---------------------------------------------------------------- the page

void LocalToolsHubPanel::open()
{
    if (MainFrame *frame = wxGetApp().mainframe)
        frame->open_service("local-tools", _L("All tools"), [](wxWindow *parent) { return new LocalToolsHubPanel(parent); });
}

LocalToolsHubPanel::LocalToolsHubPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY)
{
    SetName("local-tools");
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    const int pad = FromDIP(MD3::Metrics::active().padding);

    if (AppConfig *cfg = wxGetApp().app_config) {
        m_pins   = LocalTools::parse_pins(cfg->get(LocalTools::kPinnedKey), cfg->has(LocalTools::kPinnedKey));
        m_filter = cfg->has(LocalTools::kFilterKey) ? LocalTools::parse_filter(cfg->get(LocalTools::kFilterKey)) : std::string(LocalTools::kPinnedFilter);
    } else {
        m_pins = LocalTools::default_pins();
    }
    if (m_filter == LocalTools::kPinnedFilter && m_pins.empty()) m_filter.clear();

    auto *root = new wxBoxSizer(wxVERTICAL);

    auto *header  = new wxBoxSizer(wxHORIZONTAL);
    auto *heading = new wxBoxSizer(wxVERTICAL);
    heading->Add(text_label(this, Label::Head_20, _L("Local tools"), MD3::Role::OnSurface), 0);
    heading->Add(text_label(this, Label::Body_13, _L("Everything here runs on this computer. Nothing is uploaded."), MD3::Role::OnSurfaceVariant), 0,
                 wxTOP, FromDIP(4));
    header->Add(heading, 1, wxALIGN_CENTER_VERTICAL);
    // TRN: Placeholder of the search field on the Local tools page.
    m_search = new SearchField(this, _L("Search tools"), wxDefaultPosition, wxSize(FromDIP(360), -1));
    header->Add(m_search, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(16));
    auto *refresh = icon_button(this, kGlyphRefresh, _L("Read every tool's state again"));
    refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { rebuild(); });
    header->Add(refresh, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    root->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    build_filters(root);

    m_body = new MD3ScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    m_body->SetBackgroundColour(GetBackgroundColour());
    m_body->SetScrollRate(0, FromDIP(24));
    m_body->SetSizer(new wxBoxSizer(wxVERTICAL));
    root->Add(m_body, 1, wxEXPAND | wxALL, pad);

    auto *footer = new wxBoxSizer(wxHORIZONTAL);
    footer->Add(text_label(this, Label::Body_12, wxString::Format(_L("Version %s"), wxString::FromUTF8(SLIC3R_VERSION)), MD3::Role::OnSurfaceVariant), 0,
                wxALIGN_CENTER_VERTICAL);
    auto *changed = small_button(this, _L("What changed"), Button::Variant::Text);
    changed->Bind(wxEVT_BUTTON, [](wxCommandEvent &) {
        wxGetApp().CallAfter([]() { FeatureFinder::Commands::instance().run("changelog"); });
    });
    footer->Add(changed, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    footer->AddStretchSpacer(1);
    auto *every = small_button(this, _L("Every feature"), Button::Variant::Text);
    every->Bind(wxEVT_BUTTON, [](wxCommandEvent &) {
        if (MainFrame *frame = wxGetApp().mainframe) frame->CallAfter([frame]() { frame->open_every_feature(); });
    });
    footer->Add(every, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(footer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, pad);

    SetSizer(root);

    m_search->SetOnQuery([this](const wxString &) { rebuild(); });
    m_search->SetOnRegexToggle([this](bool) { rebuild(); });
    m_body->Bind(wxEVT_SIZE, [this](wxSizeEvent &event) {
        event.Skip();
        const int  width = event.GetSize().GetWidth();
        const bool wide  = width >= FromDIP(900);
        if (wide != m_wide || columns_for(width - (wide ? FromDIP(kActivityWidth + 16) : 0)) != m_columns)
            CallAfter([this]() { rebuild(); });
    });
    // What a tool did may change its line too (a server started, a disk attached).
    m_activity_token = LocalTools::subscribe_activity([this]() { CallAfter([this]() { rebuild(); }); });
    Bind(wxEVT_SHOW, [this](wxShowEvent &event) {
        event.Skip();
        if (event.IsShown()) CallAfter([this]() { rebuild(); });
    });

    rebuild();
}

LocalToolsHubPanel::~LocalToolsHubPanel() { LocalTools::unsubscribe_activity(m_activity_token); }

int LocalToolsHubPanel::columns_for(int width) const
{
    if (width >= FromDIP(900)) return 3;
    if (width >= FromDIP(560)) return 2;
    return 1;
}

void LocalToolsHubPanel::build_filters(wxBoxSizer *root)
{
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    const auto add_filter = [&](const std::string &id) {
        auto *button = new Button(this, wxString());
        m_filter_buttons.emplace_back(id, button);
        button->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) { set_filter(id); });
        row->Add(button, 0, wxRIGHT | wxBOTTOM, FromDIP(8));
    };
    add_filter(LocalTools::kPinnedFilter);
    add_filter("");
    for (const LocalTools::Group &g : LocalTools::groups())
        if (LocalTools::count_in(g.id) > 0) add_filter(g.id);
    root->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(MD3::Metrics::active().padding));
    style_filters();
}

void LocalToolsHubPanel::style_filters()
{
    for (auto &[id, button] : m_filter_buttons) {
        wxString label;
        if (id == LocalTools::kPinnedFilter) label = wxString::Format(_L("Pinned %d"), int(m_pins.size()));
        else if (id.empty()) label = wxString::Format(_L("All tools %d"), int(LocalTools::tools().size()));
        else if (const LocalTools::Group *g = LocalTools::find_group(id)) label = wxString::Format("%s %d", _(g->title), int(LocalTools::count_in(id)));
        const bool on = id == m_filter;
        button->SetLabel(label);
        button->SetVariant(on ? Button::Variant::Tonal : Button::Variant::Outlined);
        button->SetButtonSize(Button::Size::Small);
        button->SetName(label + (on ? wxString(" (") + _L("selected") + ")" : wxString()));
        button->Refresh();
    }
    Layout();
}

void LocalToolsHubPanel::set_filter(const std::string &filter)
{
    m_filter = filter;
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(LocalTools::kFilterKey, filter);
    style_filters();
    rebuild();
}

void LocalToolsHubPanel::toggle_pin(const std::string &id)
{
    const bool was  = LocalTools::is_pinned(m_pins, id);
    const auto pins = LocalTools::toggle_pin(m_pins, id);
    if (pins == m_pins && !was) {
        // Nothing changed: the pin list is full.
        MessageDialog(this, wxString::Format(_L("At most %d tools can be pinned. Unpin one first."), int(LocalTools::kPinLimit)), _L("Local tools"),
                      wxOK | wxICON_INFORMATION)
            .ShowModal();
        return;
    }
    m_pins = pins;
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(LocalTools::kPinnedKey, LocalTools::serialize_pins(m_pins));
    style_filters();
    CallAfter([this]() { rebuild(); });
}

void LocalToolsHubPanel::move_pin(const std::string &id, int delta)
{
    m_pins = LocalTools::move_pin(m_pins, id, delta);
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(LocalTools::kPinnedKey, LocalTools::serialize_pins(m_pins));
    CallAfter([this]() { rebuild(); });
}

wxWindow *LocalToolsHubPanel::make_card(wxWindow *parent, const Tool &tool, const wxString &status)
{
    const FeatureFinder::Feature &feature = feature_of(tool);
    const wxString                reason  = FeatureFinderPanel::unavailable_reason(feature);
    const bool                    ready   = reason.IsEmpty();
    const bool                    pinned  = LocalTools::is_pinned(m_pins, tool.feature);
    const std::string             id      = tool.feature;

    auto *card = new StaticBox(parent);
    card->SetCornerRadius(FromDIP(20));
    card->SetBorderWidth(0);
    card->SetBackgroundColor(StateColor::semantic(MD3::Role::SurfaceContainer));
    card->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
    card->SetName(_(feature.title));
    auto *sizer = new wxBoxSizer(wxVERTICAL);

    auto *head = new wxBoxSizer(wxHORIZONTAL);
    head->Add(glyph_panel(card, feature.glyph, 40, 24, ready ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant), 0, wxALIGN_CENTER_VERTICAL);
    auto *titles = new wxBoxSizer(wxVERTICAL);
    titles->Add(text_label(card, Label::Head_14, _(feature.title), ready ? MD3::Role::OnSurface : MD3::Role::OnSurfaceVariant, true), 0, wxEXPAND);
    if (!status.IsEmpty()) titles->Add(text_label(card, Label::Body_12, status, MD3::Role::Primary, true), 0, wxEXPAND | wxTOP, FromDIP(2));
    head->Add(titles, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));
    if (feature.badge == FeatureFinder::Badge::New && ready)
        head->Add(text_label(card, Label::Body_12, _L("New"), MD3::Role::Primary), 0, wxALIGN_TOP | wxLEFT, FromDIP(8));
    sizer->Add(head, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(16));

    sizer->Add(text_label(card, Label::Body_13, _(tool.summary), MD3::Role::OnSurfaceVariant, true), 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
               FromDIP(10));
    sizer->Add(text_label(card, Label::Body_12, _(feature.where), MD3::Role::OnSurfaceVariant, true), 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
               FromDIP(6));

    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    if (ready) {
        auto *open_button = small_button(card, _L("Open"), Button::Variant::Tonal);
        open_button->SetName(wxString::Format(_L("Open %s"), _(feature.title)));
        open_button->Bind(wxEVT_BUTTON, [f = &feature](wxCommandEvent &) { FeatureFinderPanel::open(*f); });
        actions->Add(open_button, 0, wxALIGN_CENTER_VERTICAL);
        card->Bind(wxEVT_LEFT_DCLICK, [f = &feature](wxMouseEvent &) { FeatureFinderPanel::open(*f); });
    } else {
        actions->Add(text_label(card, Label::Body_12, reason, MD3::Role::OnSurfaceVariant), 0, wxALIGN_CENTER_VERTICAL);
    }
    actions->AddStretchSpacer(1);
    if (pinned && m_filter == LocalTools::kPinnedFilter && m_pins.size() > 1) {
        auto *earlier = icon_button(card, kGlyphEarlier, wxString::Format(_L("Move %s earlier"), _(feature.title)));
        earlier->Enable(m_pins.front() != id);
        earlier->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) { move_pin(id, -1); });
        actions->Add(earlier, 0, wxALIGN_CENTER_VERTICAL);
        auto *later = icon_button(card, kGlyphLater, wxString::Format(_L("Move %s later"), _(feature.title)));
        later->Enable(m_pins.back() != id);
        later->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) { move_pin(id, 1); });
        actions->Add(later, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    }
    auto *pin = small_button(card, pinned ? _L("Unpin") : _L("Pin"), Button::Variant::Text, pinned ? kGlyphUnpin : kGlyphPin);
    pin->SetName(wxString::Format(pinned ? _L("Unpin %s") : _L("Pin %s"), _(feature.title)));
    pin->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) { toggle_pin(id); });
    actions->Add(pin, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(actions, 0, wxEXPAND | wxALL, FromDIP(12));

    card->SetSizer(sizer);
    return card;
}

wxWindow *LocalToolsHubPanel::make_activity_card(wxWindow *parent)
{
    m_activity_card = new StaticBox(parent);
    m_activity_card->SetCornerRadius(FromDIP(20));
    m_activity_card->SetBorderWidth(0);
    m_activity_card->SetBackgroundColor(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    m_activity_card->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLow));
    m_activity_card->SetName(_L("Recent activity"));
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    auto *head  = new wxBoxSizer(wxHORIZONTAL);
    head->Add(text_label(m_activity_card, Label::Head_14, _L("Recent activity"), MD3::Role::OnSurface), 1, wxALIGN_CENTER_VERTICAL);
    auto *history = small_button(m_activity_card, _L("Open history"), Button::Variant::Text, kGlyphHistory);
    history->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { CallAfter([this]() { show_history(); }); });
    head->Add(history, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(head, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    m_activity_rows = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_activity_rows, 0, wxEXPAND | wxALL, FromDIP(12));
    m_activity_card->SetSizer(sizer);
    rebuild_activity();
    return m_activity_card;
}

void LocalToolsHubPanel::rebuild_activity()
{
    if (m_activity_card == nullptr || m_activity_rows == nullptr) return;
    m_activity_rows->Clear(true);
    wxWindow *card = m_activity_card;
    if (!LocalTools::activity_on()) {
        m_activity_rows->Add(text_label(card, Label::Body_13, _L("Recent activity is off. Nothing the tools do is written down."),
                                        MD3::Role::OnSurfaceVariant, true), 0, wxEXPAND | wxBOTTOM, FromDIP(8));
        auto *on = small_button(card, _L("Turn on"), Button::Variant::Outlined);
        on->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { LocalTools::set_activity_on(true); });
        m_activity_rows->Add(on, 0);
        return;
    }
    const std::vector<Activity> recent = LocalTools::newest_activity(kRecentShown);
    if (recent.empty()) {
        m_activity_rows->Add(text_label(card, Label::Body_13,
                                        _L("Nothing yet. What a tool finishes shows here: a file encoded, a disk attached, a server started."),
                                        MD3::Role::OnSurfaceVariant, true), 0, wxEXPAND);
        return;
    }
    for (const Activity &a : recent) {
        const LocalTools::Tool *tool = LocalTools::find_tool(a.tool);
        if (tool == nullptr) continue;
        const FeatureFinder::Feature &feature = feature_of(*tool);
        auto *row  = new wxBoxSizer(wxHORIZONTAL);
        row->Add(glyph_panel(card, feature.glyph, 28, 18, MD3::Role::OnSurfaceVariant), 0, wxALIGN_TOP | wxTOP, FromDIP(2));
        auto *text = new wxBoxSizer(wxVERTICAL);
        text->Add(text_label(card, Label::Body_13, activity_text(a), MD3::Role::OnSurface, true), 0, wxEXPAND);
        wxString meta = _(feature.title) + "  " + wxUniChar(0x00B7) + "  " + time_text(a.time);
        const wxString result = result_text(a.result);
        if (!result.IsEmpty()) meta << "  " << wxUniChar(0x00B7) << "  " << result;
        const bool bad = a.result == "Failed" || a.result == "Incomplete";
        text->Add(text_label(card, Label::Body_12, meta, bad ? MD3::Role::Error : MD3::Role::OnSurfaceVariant, true), 0, wxEXPAND | wxTOP, FromDIP(2));
        row->Add(text, 1, wxLEFT, FromDIP(8));
        m_activity_rows->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(10));
    }
}

void LocalToolsHubPanel::rebuild()
{
    const wxString query = m_search->GetValue();
    SearchField::MatchPass match(query, m_search->IsRegexEnabled(), m_search->IsCaseSensitive(), m_search->IsWholeWord(),
                                 m_search->IsMultiline());
    const auto                    &all = LocalTools::tools();
    const std::vector<std::size_t> order = LocalTools::visible(query.IsEmpty() ? m_filter : std::string(), m_pins);

    // Statuses are read once per rebuild; a search also finds them ("running").
    std::vector<std::pair<std::size_t, wxString>> shown;
    for (std::size_t i : order) {
        const FeatureFinder::Feature &f      = feature_of(all[i]);
        const wxString                status = status_of(all[i].feature);
        if (!query.IsEmpty()) {
            wxString haystack = _(f.title);
            haystack << ' ' << _(all[i].summary) << ' ' << _(f.where) << ' ' << wxString::FromUTF8(f.keywords) << ' ' << status;
            if (!match.matches(haystack)) continue;
        }
        shown.emplace_back(i, status);
    }

    const wxPoint scrolled = m_body->GetViewStart();
    m_body->Freeze();
    m_body->GetSizer()->Clear(true);
    m_activity_card = nullptr;
    m_activity_rows = nullptr;

    const int width = m_body->GetClientSize().GetWidth();
    m_wide          = width >= FromDIP(900);
    m_columns       = columns_for(width - (m_wide ? FromDIP(kActivityWidth + 16) : 0));
    const int gap   = FromDIP(12);

    auto *outer = new wxBoxSizer(m_wide ? wxHORIZONTAL : wxVERTICAL);
    auto *tools = new wxBoxSizer(wxVERTICAL);
    if (!query.IsEmpty())
        tools->Add(text_label(m_body, Label::Body_13, wxString::Format(_L("Tools found: %d"), int(shown.size())), MD3::Role::OnSurfaceVariant), 0,
                   wxBOTTOM, gap);
    if (shown.empty()) {
        const wxString empty = !query.IsEmpty() ? _L("No tool matches. Try fewer words, or turn off the regular expression.") :
                               m_filter == LocalTools::kPinnedFilter ? _L("Nothing is pinned. Pin a tool from All tools to keep it here.") :
                                                                       _L("No tool in this group.");
        tools->Add(text_label(m_body, Label::Body_14, empty, MD3::Role::OnSurfaceVariant, true), 0, wxEXPAND | wxALL, FromDIP(8));
    } else {
        auto *grid = new wxFlexGridSizer(m_columns, gap, gap);
        for (int c = 0; c < m_columns; ++c) grid->AddGrowableCol(c, 1);
        for (const auto &[index, status] : shown) grid->Add(make_card(m_body, all[index], status), 0, wxEXPAND);
        tools->Add(grid, 0, wxEXPAND);
    }
    outer->Add(tools, 1, wxEXPAND);
    wxWindow *activity = make_activity_card(m_body);
    if (m_wide) {
        activity->SetMinSize(wxSize(FromDIP(kActivityWidth), -1));
        outer->Add(activity, 0, wxLEFT | wxALIGN_TOP, FromDIP(16));
    } else {
        outer->Add(activity, 0, wxEXPAND | wxTOP, FromDIP(16));
    }
    m_body->GetSizer()->Add(outer, 0, wxEXPAND);

    m_body->FitInside();
    m_body->Layout();
    m_body->Scroll(scrolled);
    m_body->Thaw();
}

// ---------------------------------------------------------------- history

void LocalToolsHubPanel::show_history()
{
    wxDialog dialog(this, wxID_ANY, _L("Local tools activity"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
    dialog.SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerHigh));
    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(text_label(&dialog, Label::Head_16, _L("Local tools activity"), MD3::Role::OnSurface), 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(16));
    auto *note = text_label(&dialog, Label::Body_12,
                            _L("What the local tools finished, newest first, at most 200. It stays on this computer; what a file holds is never written down."),
                            MD3::Role::OnSurfaceVariant);
    note->Wrap(FromDIP(560));
    root->Add(note, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(8));
    auto *search = new SearchField(&dialog, _L("Search activity"), wxDefaultPosition, wxSize(FromDIP(560), -1));
    root->Add(search, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(12));

    auto *list = new MD3ScrolledWindow(&dialog, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(600), FromDIP(380)), wxVSCROLL | wxBORDER_NONE);
    list->SetBackgroundColour(dialog.GetBackgroundColour());
    list->SetScrollRate(0, FromDIP(20));
    list->SetSizer(new wxBoxSizer(wxVERTICAL));
    root->Add(list, 1, wxEXPAND | wxALL, FromDIP(12));

    const auto fill = [list, search, this]() {
        list->Freeze();
        list->GetSizer()->Clear(true);
        const wxString         query = search->GetValue();
        SearchField::MatchPass match(query, search->IsRegexEnabled(), search->IsCaseSensitive(), search->IsWholeWord(), search->IsMultiline());
        int                    shown = 0;
        for (const Activity &a : LocalTools::newest_activity(LocalTools::kActivityLimit)) {
            const LocalTools::Tool *tool = LocalTools::find_tool(a.tool);
            if (tool == nullptr) continue;
            const wxString title = _(feature_of(*tool).title);
            wxString       line  = time_text(a.time) + "   " + title + ": " + activity_text(a);
            const wxString result = result_text(a.result);
            if (!result.IsEmpty()) line << " (" << result << ")";
            if (!query.IsEmpty() && !match.matches(line)) continue;
            list->GetSizer()->Add(text_label(list, Label::Body_13, line, MD3::Role::OnSurface), 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
            ++shown;
        }
        if (shown == 0)
            list->GetSizer()->Add(text_label(list, Label::Body_13, query.IsEmpty() ? _L("Nothing recorded yet.") : _L("No activity matches."),
                                             MD3::Role::OnSurfaceVariant), 0, wxALL, FromDIP(8));
        list->FitInside();
        list->Layout();
        list->Thaw();
    };
    search->SetOnQuery([fill](const wxString &) { fill(); });
    search->SetOnRegexToggle([fill](bool) { fill(); });
    fill();

    auto *buttons = new wxBoxSizer(wxHORIZONTAL);
    auto *keep    = new SwitchButton(&dialog);
    keep->SetValue(LocalTools::activity_on());
    keep->SetName(_L("Keep recent activity"));
    keep->Bind(wxEVT_TOGGLEBUTTON, [keep](wxCommandEvent &) { LocalTools::set_activity_on(keep->GetValue()); });
    buttons->Add(keep, 0, wxALIGN_CENTER_VERTICAL);
    buttons->Add(text_label(&dialog, Label::Body_13, _L("Keep recent activity"), MD3::Role::OnSurface), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    buttons->AddStretchSpacer(1);
    auto *clear = small_button(&dialog, _L("Clear activity..."), Button::Variant::Danger);
    clear->Bind(wxEVT_BUTTON, [clear, fill](wxCommandEvent &) {
        const std::vector<Activity> all = LocalTools::newest_activity(LocalTools::kActivityLimit);
        if (all.empty()) return;
        SuperConfirmGate::Spec spec;
        spec.action          = _L("Clear activity");
        spec.consequence     = _L("Every entry in Recent activity is deleted from this computer. This cannot be undone. The tools and their own histories are not changed.");
        for (std::size_t i = 0; i < all.size() && i < 5; ++i) spec.affected.push_back(activity_text(all[i]));
        spec.affected_count  = int(all.size());
        spec.completed_label = _L("Activity cleared");
        if (!SuperConfirmGate::Run(clear, spec)) return;
        LocalTools::clear_activity();
        fill();
    });
    buttons->Add(clear, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    auto *close = small_button(&dialog, _L("Close"), Button::Variant::Filled);
    close->Bind(wxEVT_BUTTON, [&dialog](wxCommandEvent &) { dialog.EndModal(wxID_OK); });
    buttons->Add(close, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    dialog.SetSizerAndFit(root);
    MD3DialogCaption::Adopt(&dialog);
    dialog.CenterOnParent();
    dialog.ShowModal();
}

} // namespace Slic3r::GUI
