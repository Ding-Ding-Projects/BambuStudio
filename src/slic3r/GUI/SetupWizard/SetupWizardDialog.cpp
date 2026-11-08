#include "SetupWizardDialog.hpp"

#include "../ConfigProfilesDialog.hpp"
#include "../ConfigWizard.hpp"
#include "../DeviceManager.hpp"
#include "../Appearance/ThemeFollow.hpp"
#include "../Appearance/ThemeFollowWatcher.hpp"
#include "../DeviceCore/DevManager.h"
#include "../FeatureFinder/LocalToolsActivity.hpp"
#include "../Extensions/ExtensionRuntime.hpp"
#include "../Extensions/ExtensionsPanel.hpp"
#include "../FeatureServices/PresentationRoutes.hpp"
#include "../GLCanvas3D.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../LanguageMode.hpp"
#include "../MainFrame.hpp"
#include "../MsgDialog.hpp"
#include "../PersonalModes/SchoolMode.hpp"
#include "../PersonalVocabulary.hpp"
#include "../Plater.hpp"
#include "../UnsavedChangesDialog.hpp"
#include "../VocabularyDrop.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/ProgressBar.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/StaticBox.hpp"
#include "../Widgets/SwitchButton.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Format/bbs_3mf.hpp"
#include "libslic3r/PresetBundle.hpp"

#include <boost/algorithm/string/predicate.hpp>

#include <wx/datetime.h>
#include <wx/dcclient.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <ctime>
#include <functional>

namespace Slic3r { namespace GUI {

namespace {

using Setup::Step;

enum Glyph : std::uint32_t {
    GlyphDone = 0xE86C, GlyphCurrent = 0xE837, GlyphPending = 0xE836, GlyphSkipped = 0xE044, GlyphBlocked = 0xE14B,
    GlyphBookmark = 0xE866, GlyphIdea = 0xE0F0, GlyphRule = 0xEF76, GlyphSave = 0xE161, GlyphAdd = 0xE145,
    GlyphSearch = 0xE2DB, GlyphUpload = 0xF09B, GlyphLan = 0xEB2F, GlyphTask = 0xE2E6, GlyphError = 0xE000,
    GlyphSync = 0xE627, GlyphBackup = 0xE864, GlyphTour = 0xEF75, GlyphPrint = 0xE555, GlyphVocabulary = 0xE8CE,
    GlyphBack = 0xE5C4, GlyphForward = 0xE5C8, GlyphSkipNext = 0xE044, GlyphRestart = 0xF053, GlyphInfo = 0xE88E
};

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

std::int64_t now_seconds() { return std::int64_t(std::time(nullptr)); }

// A Material Symbols glyph in its own small window; glyph() is read at every paint.
wxWindow *glyph_window(wxWindow *parent, std::function<std::uint32_t()> glyph, MD3::Role colour, int size_dip = 20)
{
    auto *w = new wxWindow(parent, wxID_ANY, wxDefaultPosition, parent->FromDIP(wxSize(size_dip + 4, size_dip + 4)));
    w->SetBackgroundColour(parent->GetBackgroundColour());
    w->Bind(wxEVT_PAINT, [w, glyph, colour, size_dip](wxPaintEvent &) {
        wxPaintDC dc(w);
        if (!MaterialIcon::available()) return;
        const std::uint32_t g  = glyph();
        const int           px = w->FromDIP(size_dip);
        const wxSize        gs = MaterialIcon::measure(dc, g, px);
        MaterialIcon::draw(dc, g, px, role(colour), wxPoint((w->GetSize().x - gs.x) / 2, (w->GetSize().y - gs.y) / 2));
    });
    return w;
}

wxWindow *fixed_glyph(wxWindow *parent, std::uint32_t glyph, MD3::Role colour, int size_dip = 20)
{
    return glyph_window(parent, [glyph]() { return glyph; }, colour, size_dip);
}

Label *make_label(wxWindow *parent, const wxFont &font, const wxString &text, MD3::Role colour = MD3::Role::OnSurface, bool wrap = false)
{
    auto *label = new Label(parent, font, text, wrap ? (LB_AUTO_WRAP | wxST_NO_AUTORESIZE) : 0);
    if (wrap) label->SetMinSize(wxSize(parent->FromDIP(160), -1));
    label->SetBackgroundColour(parent->GetBackgroundColour());
    label->SetForegroundColour(role(colour));
    return label;
}

Button *make_button(wxWindow *parent, const wxString &text, std::uint32_t glyph, Button::Variant variant = Button::Variant::Outlined)
{
    auto *b = new Button(parent, text);
    b->SetVariant(variant);
    b->SetButtonSize(Button::Size::Small);
    if (glyph) b->SetGlyph(glyph);
    return b;
}

StaticBox *card(wxWindow *parent, MD3::Role background)
{
    auto *box = new StaticBox(parent);
    box->SetCornerRadius(parent->FromDIP(16));
    box->SetBorderWidth(0);
    box->SetBackgroundColor(role(background));
    box->SetBackgroundColour(role(background));
    return box;
}

MultiSwitchButton *segmented(wxWindow *parent, const std::vector<wxString> &options, int selected, const wxString &name)
{
    auto *s = new MultiSwitchButton(parent);
    s->SetOptions(options);
    s->SetFitOptionsToText(true);
    s->SetName(name);
    if (selected >= 0) s->SetSelection(selected); // before Bind: the initial choice fires nothing
    return s;
}

// Visible printer presets that are not the built-in defaults, the same test
// the first-run check uses.
std::vector<std::string> your_printers()
{
    std::vector<std::string> names;
    if (auto *bundle = wxGetApp().preset_bundle)
        for (const Preset &p : bundle->printers.get_presets())
            if (p.is_visible && !p.is_default && !boost::starts_with(p.name, "Default")) names.push_back(p.name);
    return names;
}

int visible_filaments()
{
    int count = 0;
    if (auto *bundle = wxGetApp().preset_bundle)
        for (const Preset &p : bundle->filaments.get_presets())
            if (p.is_visible && !p.is_default && !boost::starts_with(p.name, "Default")) ++count;
    return count;
}

wxString join_names(const std::vector<std::string> &names, std::size_t limit)
{
    wxString text;
    for (std::size_t i = 0; i < names.size() && i < limit; ++i) text << (i ? "\n" : "") << wxString::FromUTF8(names[i]);
    if (names.size() > limit) text << "\n" << wxString::Format(_L("and %d more"), int(names.size() - limit));
    return text;
}

int language_choice()
{
    const std::string mode = I18N::normalize_language_mode_id(wxGetApp().app_config->get("language"));
    if (mode == I18N::LANGUAGE_MODE_CANTONESE_HONG_KONG) return 1;
    if (mode == I18N::LANGUAGE_MODE_ENGLISH_CANTONESE_HK) return 2;
    if (mode.rfind("en", 0) == 0 || mode.empty()) return 0;
    return -1; // another language: none of the three is selected
}

void apply_dark_mode(wxDialog *dialog, bool dark)
{
    wxGetApp().app_config->set("dark_color_mode", dark ? "1" : "0");
    wxGetApp().app_config->save();
    wxGetApp().Update_dark_mode_flag();
#ifdef _MSW_DARK_MODE
    wxGetApp().force_colors_update();
    wxGetApp().update_ui_from_settings();
#endif
    if (wxGetApp().plater()) {
        SimpleEvent evt = SimpleEvent(EVT_GLCANVAS_COLOR_MODE_CHANGED);
        wxPostEvent(wxGetApp().plater(), evt);
    }
    wxGetApp().UpdateDlgDarkUI(dialog);
    dialog->Refresh();
}

void refresh_appearance(wxDialog *dialog)
{
#ifdef _MSW_DARK_MODE
    wxGetApp().force_colors_update();
    wxGetApp().update_ui_from_settings();
#endif
    if (wxGetApp().plater()) {
        SimpleEvent evt = SimpleEvent(EVT_GLCANVAS_COLOR_MODE_CHANGED);
        wxPostEvent(wxGetApp().plater(), evt);
    }
    if (wxGetApp().mainframe) wxGetApp().mainframe->Refresh();
    dialog->Refresh();
    dialog->Layout();
}

wxString step_title(int index) { return _(Setup::info(index).title); }

} // namespace

// ------------------------------------------------------------------- opening

void SetupWizardDialog::open(wxWindow *parent, bool first_run)
{
    std::string language;
    {
        SetupWizardDialog dialog(parent, first_run);
        dialog.ShowModal();
        language = dialog.m_pending_language;
    }
    if (language.empty()) return;
    // The wizard is closed; rebuild the window in the new language from the
    // event loop, outside any handler of the window being replaced.
    wxGetApp().CallAfter([]() {
        Label::initSysFont(I18N::language_mode_profile().font_language);
        wxGetApp().recreate_GUI(_L("Changing application language"));
    });
}

void SetupWizardDialog::after_gui_rebuild()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr || cfg->get(Setup::kReopenKey) != "1") return;
    cfg->set(Setup::kReopenKey, "0");
    cfg->save();
    wxGetApp().CallAfter([]() {
        if (wxGetApp().mainframe) open(wxGetApp().mainframe, false);
    });
}

void SetupWizardDialog::open_for_first_run()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr || wxGetApp().mainframe == nullptr) return;
    Setup::Progress saved;
    if (Setup::parse(cfg->get(Setup::kProgressKey), saved) && saved.finished) return;
    open(wxGetApp().mainframe, true);
}

// ------------------------------------------------------------------- construction

SetupWizardDialog::SetupWizardDialog(wxWindow *parent, bool first_run)
    : DPIDialog(parent, wxID_ANY, _L("Set up Bambu Studio"), wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxBORDER_NONE)
    , m_first_run(first_run)
{
    SetName("setup-wizard");
    SetBackgroundColour(role(MD3::Role::Surface));
    AppConfig *cfg = wxGetApp().app_config;
    if (!Setup::parse(cfg->get(Setup::kProgressKey), m_progress) || m_progress.finished)
        m_progress = Setup::start_over(first_run, 0);
    if (first_run) m_progress.first_run = true;
    m_index = Setup::resume_index(m_progress);

    build();
    wxGetApp().UpdateDlgDarkUI(this);

    // Alt+N next, Alt+B back, Alt+S skip; Esc saves and closes.
    const int id_next = NewControlId(), id_back = NewControlId(), id_skip = NewControlId();
    wxAcceleratorEntry entries[3];
    entries[0].Set(wxACCEL_ALT, int('N'), id_next);
    entries[1].Set(wxACCEL_ALT, int('B'), id_back);
    entries[2].Set(wxACCEL_ALT, int('S'), id_skip);
    SetAcceleratorTable(wxAcceleratorTable(3, entries));
    Bind(wxEVT_MENU, [this](wxCommandEvent &) { on_next(); }, id_next);
    Bind(wxEVT_MENU, [this](wxCommandEvent &) { on_back(); }, id_back);
    Bind(wxEVT_MENU, [this](wxCommandEvent &) { on_skip(); }, id_skip);
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_ESCAPE) save_and_close();
        else e.Skip();
    });
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &) { save_and_close(); });

    const wxSize need = GetSizer()->CalcMin() + (GetSize() - GetClientSize());
    SetMinSize(wxSize(std::max(FromDIP(860), need.GetWidth()), std::max(FromDIP(640), need.GetHeight())));
    SetSize(wxSize(std::max(FromDIP(980), need.GetWidth()), std::max(FromDIP(720), need.GetHeight())));
    CenterOnParent();
    show_step(m_index);
    MD3DialogCaption::FinishChrome(this);
}

SetupWizardDialog::~SetupWizardDialog() = default;

void SetupWizardDialog::on_dpi_changed(const wxRect &)
{
    Layout();
    Refresh();
}

void SetupWizardDialog::build()
{
    const int pad = FromDIP(24);
    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(new MD3DialogCaption(this, _L("Set up Bambu Studio")), 0, wxEXPAND);

    // Welcome back: where setup stopped, and what is kept.
    StaticBox *banner = card(this, MD3::Role::SecondaryContainer);
    m_banner = banner;
    auto *bs = new wxBoxSizer(wxHORIZONTAL);
    bs->Add(fixed_glyph(banner, GlyphBookmark, MD3::Role::OnSecondaryContainer), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_banner_text = make_label(banner, Label::Body_13, wxString(), MD3::Role::OnSecondaryContainer, true);
    bs->Add(m_banner_text, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    auto *again = make_button(banner, _L("Start over"), GlyphRestart);
    again->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { start_over(); });
    auto *carry_on = make_button(banner, _L("Continue"), GlyphForward, Button::Variant::Filled);
    carry_on->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        m_banner->Hide();
        Layout();
    });
    bs->Add(again, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    bs->Add(carry_on, 0, wxALIGN_CENTER_VERTICAL);
    auto *bp = new wxBoxSizer(wxVERTICAL);
    bp->Add(bs, 1, wxEXPAND | wxALL, FromDIP(12));
    banner->SetSizer(bp);
    root->Add(banner, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);
    if (Setup::has_saved_work(m_progress)) {
        const wxDateTime when(static_cast<time_t>(m_progress.updated));
        const int        kept = Setup::done_count(m_progress);
        m_banner_text->SetLabel(wxString::Format(
            _L("Welcome back. Setup stopped at step %d of %d on %s at %s; %s kept."), m_index + 1, Setup::kStepCount,
            when.FormatISODate(), when.FormatISOTime(),
            kept == 1 ? _L("your first step is") : wxString::Format(_L("%d finished steps are"), kept)));
    } else {
        banner->Hide();
    }

    auto *body = new wxBoxSizer(wxHORIZONTAL);

    // The step rail.
    StaticBox *rail = card(this, MD3::Role::SurfaceContainerLow);
    auto *rs = new wxBoxSizer(wxVERTICAL);
    rs->Add(make_label(rail, Label::Head_16, _L("Set up Bambu Studio")), 0, wxBOTTOM, FromDIP(2));
    rs->Add(make_label(rail, Label::Body_12, _L("About five minutes. Every step saves as you go."), MD3::Role::OnSurfaceVariant, true), 0,
            wxEXPAND | wxBOTTOM, FromDIP(12));
    m_rail.resize(Setup::kStepCount);
    for (int i = 0; i < Setup::kStepCount; ++i) {
        RailItem &item = m_rail[std::size_t(i)];
        item.glyph     = GlyphPending;
        auto *row      = new wxBoxSizer(wxHORIZONTAL);
        item.icon      = glyph_window(rail, [this, i]() { return m_rail[std::size_t(i)].glyph; }, MD3::Role::Primary);
        row->Add(item.icon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        auto *text = new wxBoxSizer(wxVERTICAL);
        item.title = make_label(rail, Label::Body_14, step_title(i));
        item.state = make_label(rail, Label::Body_12, wxString(), MD3::Role::OnSurfaceVariant);
        text->Add(item.title, 0);
        text->Add(item.state, 0);
        row->Add(text, 1, wxALIGN_CENTER_VERTICAL);
        rs->Add(row, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(6));
        const int index = i;
        for (wxWindow *w : {static_cast<wxWindow *>(item.icon), static_cast<wxWindow *>(item.title), static_cast<wxWindow *>(item.state)})
            w->Bind(wxEVT_LEFT_UP, [this, index](wxMouseEvent &) {
                if (Setup::info(index).available) show_step(index);
            });
    }
    rs->AddStretchSpacer(1);
    rs->Add(make_label(rail, Label::Body_12, _L("Keyboard: Alt+N next, Alt+B back, Alt+S skip an optional step, Esc saves and closes."),
                       MD3::Role::OnSurfaceVariant, true),
            0, wxEXPAND | wxTOP, FromDIP(12));
    auto *rp = new wxBoxSizer(wxVERTICAL);
    rp->Add(rs, 1, wxEXPAND | wxALL, FromDIP(16));
    rail->SetSizer(rp);
    rail->SetMinSize(wxSize(FromDIP(240), -1));
    body->Add(rail, 0, wxEXPAND | wxRIGHT, FromDIP(16));

    // The step itself.
    auto *main = new wxBoxSizer(wxVERTICAL);
    auto *head = new wxBoxSizer(wxHORIZONTAL);
    m_step_of       = make_label(this, Label::Body_13, wxString(), MD3::Role::OnSurfaceVariant);
    m_required_left = make_label(this, Label::Body_13, wxString(), MD3::Role::Primary);
    head->Add(m_step_of, 1, wxALIGN_CENTER_VERTICAL);
    head->Add(m_required_left, 0, wxALIGN_CENTER_VERTICAL);
    main->Add(head, 0, wxEXPAND);
    m_progress_bar = new ProgressBar(this, wxID_ANY, 100);
    m_progress_bar->SetHeight(FromDIP(6));
    m_progress_bar->SetName(_L("Setup progress"));
    main->Add(m_progress_bar, 0, wxEXPAND | wxTOP, FromDIP(8));
    m_title = make_label(this, Label::Head_24, wxString());
    main->Add(m_title, 0, wxTOP, FromDIP(4));
    m_lead = make_label(this, Label::Body_14, wxString(), MD3::Role::OnSurfaceVariant, true);
    main->Add(m_lead, 0, wxEXPAND | wxTOP, FromDIP(4));

    m_book = new wxSimplebook(this, wxID_ANY);
    m_book->SetBackgroundColour(GetBackgroundColour());
    m_pages = {build_language(m_book), build_look(m_book), build_printers(m_book), build_inks(m_book),
               build_backups(m_book), build_extensions(m_book), build_finish(m_book)};
    for (int i = 0; i < Setup::kStepCount; ++i) m_book->AddPage(m_pages[std::size_t(i)], Setup::info(i).id);
    main->Add(m_book, 1, wxEXPAND | wxTOP, FromDIP(12));

    // What is missing (shown after Next on an unsatisfied required step).
    m_missing_card = card(this, MD3::Role::ErrorContainer);
    auto *ms = new wxBoxSizer(wxHORIZONTAL);
    ms->Add(fixed_glyph(m_missing_card, GlyphError, MD3::Role::Error), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_missing_text = make_label(m_missing_card, Label::Body_13, wxString(), MD3::Role::OnErrorContainer, true);
    ms->Add(m_missing_text, 1, wxALIGN_CENTER_VERTICAL);
    auto *mp = new wxBoxSizer(wxVERTICAL);
    mp->Add(ms, 1, wxEXPAND | wxALL, FromDIP(12));
    m_missing_card->SetSizer(mp);
    m_missing_card->Hide();
    main->Add(m_missing_card, 0, wxEXPAND | wxTOP, FromDIP(8));

    auto *notes = new wxBoxSizer(wxHORIZONTAL);
    StaticBox *why = card(this, MD3::Role::SurfaceContainer);
    auto *ws = new wxBoxSizer(wxHORIZONTAL);
    ws->Add(fixed_glyph(why, GlyphIdea, MD3::Role::Primary), 0, wxRIGHT, FromDIP(8));
    auto *wt = new wxBoxSizer(wxVERTICAL);
    wt->Add(make_label(why, Label::Head_13, _L("Why this step matters")), 0);
    m_why_text = make_label(why, Label::Body_13, wxString(), MD3::Role::OnSurfaceVariant, true);
    wt->Add(m_why_text, 0, wxEXPAND | wxTOP, FromDIP(2));
    ws->Add(wt, 1);
    auto *wp = new wxBoxSizer(wxVERTICAL);
    wp->Add(ws, 1, wxEXPAND | wxALL, FromDIP(12));
    why->SetSizer(wp);
    notes->Add(why, 1, wxEXPAND | wxRIGHT, FromDIP(8));
    m_rule_card = card(this, MD3::Role::SurfaceContainer);
    auto *qs = new wxBoxSizer(wxHORIZONTAL);
    qs->Add(fixed_glyph(m_rule_card, GlyphRule, MD3::Role::Primary), 0, wxRIGHT, FromDIP(8));
    auto *qt = new wxBoxSizer(wxVERTICAL);
    qt->Add(make_label(m_rule_card, Label::Head_13, _L("Required to finish")), 0);
    m_rule_text = make_label(m_rule_card, Label::Body_13, wxString(), MD3::Role::OnSurfaceVariant, true);
    qt->Add(m_rule_text, 0, wxEXPAND | wxTOP, FromDIP(2));
    qs->Add(qt, 1);
    auto *qp = new wxBoxSizer(wxVERTICAL);
    qp->Add(qs, 1, wxEXPAND | wxALL, FromDIP(12));
    m_rule_card->SetSizer(qp);
    notes->Add(m_rule_card, 1, wxEXPAND);
    main->Add(notes, 0, wxEXPAND | wxTOP, FromDIP(12));

    auto *footer = new wxBoxSizer(wxHORIZONTAL);
    footer->Add(fixed_glyph(this, GlyphSave, MD3::Role::OnSurfaceVariant, 18), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    m_saved = make_label(this, Label::Body_12, _L("Saved. Close any time and continue from Help, then Set up Bambu Studio."),
                         MD3::Role::OnSurfaceVariant, true);
    footer->Add(m_saved, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    m_skip = make_button(this, _L("Skip this step"), GlyphSkipNext, Button::Variant::Text);
    m_skip->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_skip(); });
    m_back = make_button(this, _L("Back"), GlyphBack);
    m_back->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_back(); });
    m_next = make_button(this, _L("Next"), GlyphForward, Button::Variant::Filled);
    m_next->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { on_next(); });
    footer->Add(m_skip, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    footer->Add(m_back, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    footer->Add(m_next, 0, wxALIGN_CENTER_VERTICAL);
    main->Add(footer, 0, wxEXPAND | wxTOP, FromDIP(16));

    body->Add(main, 1, wxEXPAND);
    root->Add(body, 1, wxEXPAND | wxALL, pad);
    SetSizer(root);
}

wxBoxSizer *SetupWizardDialog::results_box(wxWindow *page, int step)
{
    auto *box = new wxBoxSizer(wxVERTICAL);
    m_results[std::size_t(step)] = box;
    (void) page;
    return box;
}

void SetupWizardDialog::add_result(int step, std::uint32_t glyph, bool ok, const wxString &title, const wxString &detail,
                                   const wxString &action, std::function<void()> on_action)
{
    wxWindow *page = m_pages[std::size_t(step)];
    wxBoxSizer *box = m_results[std::size_t(step)];
    if (page == nullptr || box == nullptr) return;
    StaticBox *row = card(page, ok ? MD3::Role::SurfaceContainerLow : MD3::Role::ErrorContainer);
    auto *s = new wxBoxSizer(wxHORIZONTAL);
    s->Add(fixed_glyph(row, glyph, ok ? MD3::Role::Primary : MD3::Role::Error), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    auto *t = new wxBoxSizer(wxVERTICAL);
    t->Add(make_label(row, Label::Head_13, title, ok ? MD3::Role::OnSurface : MD3::Role::OnErrorContainer, true), 0, wxEXPAND);
    if (!detail.IsEmpty())
        t->Add(make_label(row, Label::Body_12, detail, ok ? MD3::Role::OnSurfaceVariant : MD3::Role::OnErrorContainer, true), 0, wxEXPAND | wxTOP,
               FromDIP(2));
    s->Add(t, 1, wxALIGN_CENTER_VERTICAL);
    if (!action.IsEmpty() && on_action) {
        auto *b = make_button(row, action, 0);
        b->Bind(wxEVT_BUTTON, [on_action](wxCommandEvent &) { on_action(); });
        s->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    }
    auto *p = new wxBoxSizer(wxVERTICAL);
    p->Add(s, 1, wxEXPAND | wxALL, FromDIP(10));
    row->SetSizer(p);
    box->Insert(0, row, 0, wxEXPAND | wxBOTTOM, FromDIP(6));
    // Keep the newest five results.
    while (box->GetItemCount() > 5) {
        wxSizerItem *last = box->GetItem(box->GetItemCount() - 1);
        wxWindow    *w    = last ? last->GetWindow() : nullptr;
        box->Remove(int(box->GetItemCount() - 1));
        if (w) w->Destroy();
    }
    page->Layout();
    if (auto *scrolled = dynamic_cast<wxScrolledWindow *>(page)) scrolled->FitInside();
    save();
}

// ------------------------------------------------------------------- steps

static MD3ScrolledWindow *step_page(wxWindow *parent)
{
    auto *page = new MD3ScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    page->SetBackgroundColour(parent->GetBackgroundColour());
    page->SetScrollRate(0, parent->FromDIP(24));
    return page;
}

wxWindow *SetupWizardDialog::build_language(wxWindow *parent)
{
    auto *page = step_page(parent);
    auto *s    = new wxBoxSizer(wxVERTICAL);
    const bool school = PersonalModes::school_presentation_suppressed.load();

    s->Add(make_label(page, Label::Head_14, _L("Language")), 0, wxBOTTOM, FromDIP(6));
    m_language = segmented(page, {"English", wxString::FromUTF8("\xE5\xBB\xA3\xE6\x9D\xB1\xE8\xA9\xB1"), _L("Both")}, language_choice(), _L("Language"));
    m_language->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [this](wxCommandEvent &e) {
        static const char *modes[] = {I18N::LANGUAGE_MODE_ENGLISH, I18N::LANGUAGE_MODE_CANTONESE_HONG_KONG, I18N::LANGUAGE_MODE_ENGLISH_CANTONESE_HK};
        const int choice = e.GetInt();
        if (choice >= 0 && choice < 3 && choice != language_choice()) request_language(modes[choice]);
        e.Skip();
    });
    s->Add(m_language, 0, wxBOTTOM, FromDIP(4));
    s->Add(make_label(page, Label::Body_12, _L("Changing the language reloads the window; setup opens again where you are."), MD3::Role::OnSurfaceVariant, true),
           0, wxEXPAND | wxBOTTOM, FromDIP(16));

    if (school) {
        s->Add(make_label(page, Label::Body_13, _L("School mode is on, so humour, emoji and personal wording settings are hidden here and in Preferences."),
                          MD3::Role::OnSurfaceVariant, true),
               0, wxEXPAND);
    } else {
        AppConfig *cfg = wxGetApp().app_config;
        const auto funny_row = [&](const char *key, I18N::FunnyLanguage language, const wxString &caption) {
            s->Add(make_label(page, Label::Head_14, caption), 0, wxBOTTOM, FromDIP(6));
            const int level = I18N::parse_funny_level(cfg->get(key));
            auto *levels = segmented(page, {"1", "2", "3", "4", "5"}, level - I18N::FUNNY_LEVEL_MIN, caption);
            const std::string k = key;
            levels->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [this, k, language](wxCommandEvent &e) {
                const int value = I18N::clamp_funny_level(e.GetInt() + I18N::FUNNY_LEVEL_MIN);
                wxGetApp().app_config->set(k, std::to_string(value));
                wxGetApp().app_config->save();
                I18N::language_mode_service().set_funny_level(language, value);
                save();
                e.Skip();
            });
            s->Add(levels, 0, wxBOTTOM, FromDIP(12));
        };
        funny_row(I18N::FUNNY_LEVEL_ENGLISH_KEY, I18N::FunnyLanguage::English, _L("English funny level"));
        funny_row(I18N::FUNNY_LEVEL_CANTONESE_KEY, I18N::FunnyLanguage::Cantonese, _L("Cantonese funny level"));
        s->Add(make_label(page, Label::Body_12, _L("1 is plain, 5 is the most playful. Humour changes the tone, never the facts."),
                          MD3::Role::OnSurfaceVariant, true),
               0, wxEXPAND | wxBOTTOM, FromDIP(12));

        auto *emoji_row = new wxBoxSizer(wxHORIZONTAL);
        emoji_row->Add(make_label(page, Label::Head_14, _L("Emoji in dialogs")), 1, wxALIGN_CENTER_VERTICAL);
        auto *emoji = new SwitchButton(page);
        emoji->SetName(_L("Emoji in dialogs"));
        emoji->SetValue(I18N::parse_dialog_emojis(cfg->get(I18N::DIALOG_EMOJIS_KEY)));
        emoji->Bind(wxEVT_TOGGLEBUTTON, [this, emoji](wxCommandEvent &e) {
            wxGetApp().app_config->set(I18N::DIALOG_EMOJIS_KEY, emoji->GetValue() ? "1" : "0");
            wxGetApp().app_config->save();
            I18N::language_mode_service().set_dialog_emojis(emoji->GetValue());
            save();
            e.Skip();
        });
        emoji_row->Add(emoji, 0, wxALIGN_CENTER_VERTICAL);
        s->Add(emoji_row, 0, wxEXPAND | wxBOTTOM, FromDIP(16));

        auto *wording_row = new wxBoxSizer(wxHORIZONTAL);
        auto *wording_text = new wxBoxSizer(wxVERTICAL);
        wording_text->Add(make_label(page, Label::Head_14, _L("Personal wording file")), 0);
        wording_text->Add(make_label(page, Label::Body_12, _L("A .json file of your own words for things on screen. You can also drop it on the window later."),
                                     MD3::Role::OnSurfaceVariant, true),
                          0, wxEXPAND | wxTOP, FromDIP(2));
        wording_row->Add(wording_text, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
        auto *choose = make_button(page, _L("Choose .json"), GlyphVocabulary);
        choose->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            wxFileDialog picker(this, _L("Load personal vocabulary"), wxEmptyString, wxEmptyString, _L("JSON files (*.json)|*.json"),
                                wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (picker.ShowModal() != wxID_OK) return;
            auto       reason  = PersonalVocabulary::Rejection::None;
            const bool applied = PersonalVocabulary::load(std::filesystem::path(picker.GetPath().ToStdWstring()), &reason);
            if (applied)
                add_result(Setup::index_of(Step::Language), GlyphDone, true, _L("Personal wording applied"), wxFileName(picker.GetPath()).GetFullName());
            else
                add_result(Setup::index_of(Step::Language), GlyphError, false,
                           _L("The vocabulary file was not applied, and the wording on screen has not changed."),
                           VocabularyDrop::rejection_text(reason));
        });
        wording_row->Add(choose, 0, wxALIGN_CENTER_VERTICAL);
        s->Add(wording_row, 0, wxEXPAND | wxBOTTOM, FromDIP(16));
    }
    s->Add(results_box(page, Setup::index_of(Step::Language)), 0, wxEXPAND);
    page->SetSizer(s);
    return page;
}

wxWindow *SetupWizardDialog::build_look(wxWindow *parent)
{
    auto *page = step_page(parent);
    auto *s    = new wxBoxSizer(wxVERTICAL);
    AppConfig *cfg = wxGetApp().app_config;

    s->Add(make_label(page, Label::Head_14, _L("Theme")), 0, wxBOTTOM, FromDIP(6));
    const ThemeFollow::Choice theme_now = ThemeFollow::choice_from(cfg->get("dark_color_mode"), cfg->get(ThemeFollow::kFollowKey));
    auto *theme = segmented(page, {_L("Light"), _L("Dark"), _L("Follow Windows")},
                            theme_now == ThemeFollow::Choice::FollowWindows ? 2 : theme_now == ThemeFollow::Choice::Dark ? 1 : 0, _L("Theme"));
    theme->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [this](wxCommandEvent &e) {
        const ThemeFollow::Choice choice = e.GetInt() == 2 ? ThemeFollow::Choice::FollowWindows :
                                           e.GetInt() == 1 ? ThemeFollow::Choice::Dark : ThemeFollow::Choice::Light;
        AppConfig                *config = wxGetApp().app_config;
        const ThemeFollow::Stored stored = ThemeFollow::store(choice, ThemeFollow::windows_prefers_dark(), config->get("dark_color_mode"));
        config->set(ThemeFollow::kFollowKey, stored.follow);
        apply_dark_mode(this, stored.dark_color_mode == "1");
        save();
        e.Skip();
    });
    s->Add(theme, 0, wxBOTTOM, FromDIP(4));
    s->Add(make_label(page, Label::Body_12, _L("Follow Windows switches between light and dark when Windows does, also while Bambu Studio is open."),
                      MD3::Role::OnSurfaceVariant, true),
           0, wxEXPAND | wxBOTTOM, FromDIP(16));

    s->Add(make_label(page, Label::Head_14, _L("Density")), 0, wxBOTTOM, FromDIP(6));
    auto *density = segmented(page, {_L("Comfortable"), _L("Compact")}, cfg->get("ui_density") == "compact" ? 1 : 0, _L("Density"));
    density->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [this](wxCommandEvent &e) {
        const bool compact = e.GetInt() == 1;
        wxGetApp().app_config->set("ui_density", compact ? "compact" : "comfortable");
        wxGetApp().app_config->save();
        MD3::Metrics::setDensity(compact ? MD3::Metrics::Density::Compact : MD3::Metrics::Density::Comfortable);
        refresh_appearance(this);
        save();
        e.Skip();
    });
    s->Add(density, 0, wxBOTTOM, FromDIP(4));
    s->Add(make_label(page, Label::Body_12, _L("Windows opened from now on use the new density; open windows keep theirs until they are opened again."),
                      MD3::Role::OnSurfaceVariant, true),
           0, wxEXPAND | wxBOTTOM, FromDIP(16));

    s->Add(make_label(page, Label::Head_14, _L("Interface motion")), 0, wxBOTTOM, FromDIP(6));
    auto *motion = segmented(page, {_L("System"), _L("Reduce motion")}, cfg->get("motion_preference") == "reduced" ? 1 : 0, _L("Interface motion"));
    motion->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [this](wxCommandEvent &e) {
        wxGetApp().app_config->set("motion_preference", e.GetInt() == 1 ? "reduced" : "system");
        wxGetApp().app_config->save();
        save();
        e.Skip();
    });
    s->Add(motion, 0, wxBOTTOM, FromDIP(4));
    s->Add(make_label(page, Label::Body_12, _L("Reduce motion settles supported transitions immediately. System follows your operating system preference."),
                      MD3::Role::OnSurfaceVariant, true),
           0, wxEXPAND | wxBOTTOM, FromDIP(16));
    s->Add(results_box(page, Setup::index_of(Step::Look)), 0, wxEXPAND);
    page->SetSizer(s);
    return page;
}

wxWindow *SetupWizardDialog::build_printers(wxWindow *parent)
{
    auto *page = step_page(parent);
    auto *s    = new wxBoxSizer(wxVERTICAL);
    const int step = Setup::index_of(Step::Printers);

    auto *actions = new wxWrapSizer(wxHORIZONTAL);
    auto *choose = make_button(page, _L("Choose printers"), GlyphAdd, Button::Variant::Tonal);
    choose->Bind(wxEVT_BUTTON, [this, step](wxCommandEvent &) {
        AppConfig *cfg = wxGetApp().app_config;
        const std::vector<std::string> before = your_printers();
        // What the printer guide can change, kept so Undo can put it back.
        PrinterGuideState state;
        state.vendors   = cfg->vendors();
        state.filaments = cfg->has_section(AppConfig::SECTION_FILAMENTS) ? cfg->get_section(AppConfig::SECTION_FILAMENTS)
                                                                        : std::map<std::string, std::string>();
        const bool                     ok    = wxGetApp().run_wizard(ConfigWizard::RR_USER, ConfigWizard::SP_PRINTERS);
        const std::vector<std::string> after = your_printers();
        for (const std::string &name : after)
            if (std::find(before.begin(), before.end(), name) == before.end()) state.added.push_back(name);
        refresh_printers();
        if (ok && !state.added.empty()) {
            auto undone = std::make_shared<bool>(false);
            add_result(step, GlyphDone, true, wxString::Format(_L("Added %d printers"), int(state.added.size())),
                       join_names(state.added, 4) + ". " + _L("Their plates, nozzles and process presets are now in Prepare."),
                       _L("Undo"), [this, step, state, undone]() { undo_added_printers(step, state, undone); });
        } else if (ok)
            add_result(step, GlyphInfo, true, _L("Printer choices saved"), wxString::Format(_L("You have %d printers set up."), int(after.size())));
        else
            add_result(step, GlyphInfo, true, _L("The printer guide closed without changes"), wxString());
    });
    auto *search = make_button(page, _L("Search my network"), GlyphSearch);
    search->Bind(wxEVT_BUTTON, [this, step](wxCommandEvent &) {
        DeviceManager *devices = wxGetApp().getDeviceManager();
        if (devices == nullptr) {
            add_result(step, GlyphError, false, _L("Network search is not available"),
                       _L("The network plug-in is not installed, so printers on your network cannot be listed here."));
            return;
        }
        const auto found = devices->get_local_machinelist();
        if (found.empty()) {
            add_result(step, GlyphLan, true, _L("No printers answered on your network yet"),
                       _L("Printers announce themselves every few seconds. Make sure the printer is on and on the same network, then search again."));
            return;
        }
        int shown = 0;
        for (const auto &[id, machine] : found) {
            if (machine == nullptr || shown == 3) continue;
            ++shown;
            const std::string dev_id = id;
            add_result(step, GlyphLan, true,
                       wxString::Format(_L("Found on your network: %s"), wxString::FromUTF8(machine->get_dev_name())) + " (" +
                           machine->get_printer_type_display_str() + ")",
                       _L("Connecting needs its access code, which you enter on the Device tab."), _L("Connect"),
                       [this, dev_id]() { connect_on_device_tab(dev_id); });
        }
        if (int(found.size()) > shown)
            add_result(step, GlyphLan, true, wxString::Format(_L("%d more printers answered"), int(found.size()) - shown),
                       _L("The Device tab lists every printer on your network."));
    });
    auto *import = make_button(page, _L("Import a printer profile"), GlyphUpload);
    import->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { import_printer_profile(); });
    for (Button *b : {choose, search, import}) actions->Add(b, 0, wxRIGHT | wxBOTTOM, FromDIP(8));
    s->Add(actions, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    s->Add(make_label(page, Label::Head_14, _L("Your printers")), 0, wxBOTTOM, FromDIP(4));
    m_printers_list = make_label(page, Label::Body_13, wxString(), MD3::Role::OnSurfaceVariant, true);
    s->Add(m_printers_list, 0, wxEXPAND | wxBOTTOM, FromDIP(16));
    s->Add(make_label(page, Label::Head_14, _L("Result of each action")), 0, wxBOTTOM, FromDIP(6));
    s->Add(results_box(page, step), 0, wxEXPAND);
    page->SetSizer(s);
    refresh_printers();
    return page;
}

void SetupWizardDialog::undo_added_printers(int step, const PrinterGuideState &state, const std::shared_ptr<bool> &undone)
{
    if (*undone) {
        add_result(step, GlyphInfo, true, _L("Already undone"), _L("The printers added there were removed before."));
        return;
    }
    const wxString question = wxString::Format(_L("Remove the printers added here again? %s"), join_names(state.added, 6)) + "\n\n" +
                              _L("The printer guide's choices go back to how they were before it opened. Nothing you saved is deleted, and the printer guide adds them again any time.");
    if (MessageDialog(this, question, _L("Undo"), wxOK | wxCANCEL | wxICON_QUESTION).ShowModal() != wxID_OK) return;
    // Unsaved changes to the printer in use are saved or dropped first, as the printer guide asks.
    if (!wxGetApp().check_and_keep_current_preset_changes(_L("Undo"), _L("The printers added in setup are removed again."),
                                                          UnsavedChangesDialog::ActionButtons::SAVE))
        return;
    AppConfig    *cfg    = wxGetApp().app_config;
    PresetBundle *bundle = wxGetApp().preset_bundle;
    if (cfg == nullptr || bundle == nullptr) return;
    cfg->set_section(AppConfig::SECTION_FILAMENTS, state.filaments);
    cfg->set_vendors(state.vendors);
    const auto loaded = bundle->load_presets(*cfg, ForwardCompatibilitySubstitutionRule::EnableSilentDisableSystem);
    if (!loaded.second.empty()) show_error(this, loaded.second);
    bundle->export_selections(*cfg);
    wxGetApp().load_current_presets();
    cfg->save();
    *undone = true;
    refresh_printers();
    const std::vector<std::string> now = your_printers();
    const int gone = int(std::count_if(state.added.begin(), state.added.end(),
                                       [&now](const std::string &name) { return std::find(now.begin(), now.end(), name) == now.end(); }));
    add_result(step, GlyphDone, true, wxString::Format(_L("Removed %d of the printers added here"), gone),
               gone == int(state.added.size()) ? _L("Their presets are hidden again; nothing you saved was deleted.") :
                                                 _L("The others stay: a preset of yours still uses them."));
}

void SetupWizardDialog::import_printer_profile()
{
    if (wxGetApp().mainframe == nullptr) return;
    const int         step   = Setup::index_of(Step::Printers);
    const std::size_t before = your_printers().size();
    wxGetApp().mainframe->load_config_file();
    const std::size_t after = your_printers().size();
    refresh_printers();
    if (after > before)
        add_result(step, GlyphDone, true, wxString::Format(_L("Imported %d printers"), int(after - before)), wxString());
    else
        add_result(step, GlyphError, false, _L("No new printer came from that import"),
                   _L("A filament or process file goes to its own list; use the Inks step for filaments."), _L("Choose another file"),
                   [this]() { CallAfter([this]() { import_printer_profile(); }); });
}

void SetupWizardDialog::connect_on_device_tab(const std::string &dev_id)
{
    save();
    EndModal(wxID_OK);
    wxGetApp().CallAfter([dev_id]() {
        if (wxGetApp().mainframe) wxGetApp().mainframe->jump_to_monitor(dev_id);
    });
}

void SetupWizardDialog::refresh_printers()
{
    if (m_printers_list == nullptr) return;
    const auto names = your_printers();
    m_printers_list->SetLabel(names.empty() ? _L("None yet. Choose printers, or import a printer profile.") : join_names(names, 6));
    if (m_pages[std::size_t(Setup::index_of(Step::Printers))]) m_pages[std::size_t(Setup::index_of(Step::Printers))]->Layout();
    refresh_header();
}

wxWindow *SetupWizardDialog::build_inks(wxWindow *parent)
{
    auto *page = step_page(parent);
    auto *s    = new wxBoxSizer(wxVERTICAL);
    const int step = Setup::index_of(Step::Inks);

    auto *actions = new wxWrapSizer(wxHORIZONTAL);
    auto *sync = make_button(page, _L("Sync AMS"), GlyphSync, Button::Variant::Tonal);
    sync->SetToolTip(_L("Read the AMS now and fill the plate's filament list from the printer"));
    sync->Bind(wxEVT_BUTTON, [this, step](wxCommandEvent &) {
        if (wxGetApp().plater() == nullptr) return;
        wxGetApp().plater()->sidebar().sync_ams_list(true);
        refresh_inks();
        add_result(step, GlyphSync, true, _L("Asked the printer for its AMS"),
                   _L("The list below shows what Prepare has now. Without a connected printer, nothing changes."));
    });
    auto *pick = make_button(page, _L("Pick filaments"), GlyphAdd);
    pick->Bind(wxEVT_BUTTON, [this, step](wxCommandEvent &) {
        const int  before = visible_filaments();
        const bool ok     = wxGetApp().run_wizard(ConfigWizard::RR_USER, ConfigWizard::SP_FILAMENTS);
        const int  after  = visible_filaments();
        refresh_inks();
        if (ok && after != before)
            add_result(step, GlyphDone, true, wxString::Format(_L("%d filament presets available"), after), wxString());
        else
            add_result(step, GlyphInfo, true, _L("The filament guide closed without changes"), wxString());
    });
    actions->Add(sync, 0, wxRIGHT | wxBOTTOM, FromDIP(8));
    actions->Add(pick, 0, wxRIGHT | wxBOTTOM, FromDIP(8));
    s->Add(actions, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    s->Add(make_label(page, Label::Body_12, _L("Or pick filaments by brand, type and colour."), MD3::Role::OnSurfaceVariant), 0, wxBOTTOM, FromDIP(12));

    s->Add(make_label(page, Label::Head_14, _L("On the plate now")), 0, wxBOTTOM, FromDIP(4));
    m_inks_list = make_label(page, Label::Body_13, wxString(), MD3::Role::OnSurfaceVariant, true);
    s->Add(m_inks_list, 0, wxEXPAND | wxBOTTOM, FromDIP(16));
    s->Add(make_label(page, Label::Head_14, _L("Result of each action")), 0, wxBOTTOM, FromDIP(6));
    s->Add(results_box(page, step), 0, wxEXPAND);
    page->SetSizer(s);
    refresh_inks();
    return page;
}

void SetupWizardDialog::refresh_inks()
{
    if (m_inks_list == nullptr) return;
    std::vector<std::string> slots;
    if (auto *bundle = wxGetApp().preset_bundle) {
        const auto *colours = bundle->project_config.option<ConfigOptionStrings>("filament_colour");
        for (std::size_t i = 0; i < bundle->filament_presets.size(); ++i) {
            std::string line = bundle->filament_presets[i];
            if (colours && i < colours->values.size()) line += "  " + colours->values[i];
            slots.push_back(line);
        }
    }
    wxString text;
    for (std::size_t i = 0; i < slots.size() && i < 8; ++i)
        text << (i ? "\n" : "") << wxString::Format(_L("Slot %d"), int(i + 1)) << ": " << wxString::FromUTF8(slots[i]);
    if (slots.size() > 8) text << "\n" << wxString::Format(_L("and %d more"), int(slots.size() - 8));
    m_inks_list->SetLabel(text.IsEmpty() ? _L("No filaments on the plate yet.") : text);
    if (m_pages[std::size_t(Setup::index_of(Step::Inks))]) m_pages[std::size_t(Setup::index_of(Step::Inks))]->Layout();
    refresh_header();
}

wxWindow *SetupWizardDialog::build_backups(wxWindow *parent)
{
    auto *page = step_page(parent);
    auto *s    = new wxBoxSizer(wxVERTICAL);
    const int step = Setup::index_of(Step::Backups);
    AppConfig *cfg = wxGetApp().app_config;

    s->Add(make_label(page, Label::Head_14, _L("Back up the open project every")), 0, wxBOTTOM, FromDIP(6));
    const std::string interval = cfg->get("backup_switch") == "true" ? cfg->get("backup_interval") : "0";
    const int         selected = interval == "0" ? 0 : interval == "120" ? 1 : interval == "600" ? 2 : -1;
    m_backup_interval = segmented(page, {_L("Off"), _L("2 minutes"), _L("10 minutes")}, selected, _L("Auto-Backup"));
    m_backup_interval->Bind(wxCUSTOMEVT_MULTISWITCH_SELECTION, [this, step](wxCommandEvent &e) {
        static const char *values[] = {"0", "120", "600"};
        const int          choice   = std::clamp(e.GetInt(), 0, 2);
        wxGetApp().app_config->set("backup_switch", "true");
        wxGetApp().app_config->set("backup_interval", values[choice]);
        wxGetApp().app_config->save();
        Slic3r::set_backup_interval(std::stol(values[choice]));
        add_result(step, GlyphDone, true, choice == 0 ? _L("Auto-backup is off") : wxString::Format(_L("Auto-backup every %s"), m_backup_interval->GetSelectedText()),
                   _L("The same setting is in Preferences, Auto-Backup."));
        e.Skip();
    });
    s->Add(m_backup_interval, 0, wxBOTTOM, FromDIP(4));
    if (selected < 0)
        s->Add(make_label(page, Label::Body_12, wxString::Format(_L("Now every %s seconds, set in Preferences."), wxString::FromUTF8(interval)),
                          MD3::Role::OnSurfaceVariant),
               0, wxBOTTOM, FromDIP(4));
    s->AddSpacer(FromDIP(12));

    auto *backup_row = new wxBoxSizer(wxHORIZONTAL);
    auto *backup_text = new wxBoxSizer(wxVERTICAL);
    backup_text->Add(make_label(page, Label::Head_14, _L("Make a config backup now")), 0);
    backup_text->Add(make_label(page, Label::Body_12,
                                _L("Opens Config profiles & backup. Snapshot now keeps a complete copy of your settings on this PC; Export moves everything, secrets included, to another PC behind a confirmation."),
                                MD3::Role::OnSurfaceVariant, true),
                     0, wxEXPAND | wxTOP, FromDIP(2));
    backup_row->Add(backup_text, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    auto *backup = make_button(page, _L("Back up"), GlyphBackup);
    backup->Bind(wxEVT_BUTTON, [this, step](wxCommandEvent &) {
        ConfigProfilesDialog(this).ShowModal();
        add_result(step, GlyphBackup, true, _L("Opened Config profiles & backup"),
                   _L("Snapshots you recorded there are listed in its history."));
    });
    backup_row->Add(backup, 0, wxALIGN_CENTER_VERTICAL);
    s->Add(backup_row, 0, wxEXPAND | wxBOTTOM, FromDIP(16));
    s->Add(results_box(page, step), 0, wxEXPAND);
    page->SetSizer(s);
    return page;
}

wxWindow *SetupWizardDialog::build_extensions(wxWindow *parent)
{
    auto *page = step_page(parent);
    auto *s    = new wxBoxSizer(wxVERTICAL);
    s->Add(make_label(page, Label::Body_13,
                      _L("Bambu Studio comes with a few small extensions. Each one stays off until you allow exactly what it asks for, and none of them sends anything anywhere."),
                      MD3::Role::OnSurfaceVariant, true),
           0, wxEXPAND | wxBOTTOM, FromDIP(12));
    m_extension_list = new wxBoxSizer(wxVERTICAL);
    s->Add(m_extension_list, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    s->Add(make_label(page, Label::Body_12, _L("Your own extensions, their runs and the limits every run gets are in File, Extensions."),
                      MD3::Role::OnSurfaceVariant, true),
           0, wxEXPAND | wxBOTTOM, FromDIP(16));
    s->Add(results_box(page, Setup::index_of(Step::Extensions)), 0, wxEXPAND);
    page->SetSizer(s);
    refresh_extensions(page);
    return page;
}

void SetupWizardDialog::refresh_extensions(wxWindow *page)
{
    if (!m_extension_list || !page) return;
    m_extension_list->Clear(true);
    Extensions::Host &host  = Extensions::Runtime::instance().host();
    int               shown = 0;
    for (const Extensions::Installed &x : host.extensions()) {
        if (!x.builtin) continue;
        ++shown;
        const Extensions::State state = host.state(x);
        const bool              on    = state == Extensions::State::On;
        StaticBox              *row   = card(page, MD3::Role::SurfaceContainerLow);
        auto                   *rs    = new wxBoxSizer(wxHORIZONTAL);
        rs->Add(fixed_glyph(row, on ? GlyphDone : 0xE87B /* extension */, on ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant), 0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
        auto *text = new wxBoxSizer(wxVERTICAL);
        text->Add(make_label(row, Label::Head_14, wxString::FromUTF8(x.manifest.name + " " + x.manifest.version)), 0);
        if (!x.manifest.description.empty())
            text->Add(make_label(row, Label::Body_12, wxString::FromUTF8(x.manifest.description), MD3::Role::OnSurfaceVariant, true), 0,
                      wxEXPAND | wxTOP, FromDIP(2));
        text->Add(make_label(row, Label::Head_12, Extensions::state_text(state), on ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant), 0,
                  wxTOP, FromDIP(4));
        rs->Add(text, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
        auto *toggle = make_button(row, on ? _L("Turn off") : _L("Review and allow"), on ? 0 : GlyphRule,
                                   on ? Button::Variant::Outlined : Button::Variant::Tonal);
        toggle->Enable(state != Extensions::State::ManifestError && state != Extensions::State::AllOff);
        const std::string id = x.manifest.id;
        toggle->Bind(wxEVT_BUTTON, [this, page, id](wxCommandEvent &) { toggle_extension(page, id); });
        rs->Add(toggle, 0, wxALIGN_CENTER_VERTICAL);
        auto *pad = new wxBoxSizer(wxVERTICAL);
        pad->Add(rs, 1, wxEXPAND | wxALL, FromDIP(12));
        row->SetSizer(pad);
        m_extension_list->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    }
    if (!host.master_on())
        m_extension_list->Add(make_label(page, Label::Body_12, _L("Extensions are off for the whole app. Turn them on in File, Extensions."),
                                         MD3::Role::Error, true),
                              0, wxEXPAND | wxBOTTOM, FromDIP(8));
    if (shown == 0)
        m_extension_list->Add(make_label(page, Label::Body_13, _L("No built-in extension is installed: they were removed, or this build has none."),
                                         MD3::Role::OnSurfaceVariant, true),
                              0, wxEXPAND);
    page->Layout();
    if (auto *scrolled = dynamic_cast<wxScrolledWindow *>(page)) scrolled->FitInside();
}

void SetupWizardDialog::toggle_extension(wxWindow *page, const std::string &id)
{
    Extensions::Host            &host = Extensions::Runtime::instance().host();
    const Extensions::Installed *x    = host.find(id);
    if (!x) return;
    const int      step  = Setup::index_of(Step::Extensions);
    const wxString name  = wxString::FromUTF8(x->manifest.name);
    const auto     state = host.state(*x);
    if (state == Extensions::State::On) {
        host.set_enabled(id, false);
        add_result(step, GlyphDone, true, wxString::Format(_L("%s is off"), name), _L("Turn it on again here or in File, Extensions."));
    } else if ((state == Extensions::State::OffByUser || state == Extensions::State::OffAfterFailures) && host.set_enabled(id, true)) {
        add_result(step, GlyphDone, true, wxString::Format(_L("%s is on"), name), _L("It keeps what you allowed before."));
    } else {
        const Extensions::Consent *before = host.consent(id);
        Extensions::Consent        previous;
        if (before) previous = *before;
        const Extensions::ConsentAnswer answer = Extensions::ask_consent(this, *x, before ? &previous : nullptr);
        if (answer == Extensions::ConsentAnswer::Allow) {
            host.allow(id);
            add_result(step, GlyphDone, true, wxString::Format(_L("%s is on"), name), _L("It runs from now on; its runs are listed in File, Extensions."));
        } else if (answer == Extensions::ConsentAnswer::KeepOff) {
            host.keep_off(id);
            add_result(step, GlyphInfo, true, wxString::Format(_L("%s stays off"), name), _L("Nothing was allowed."));
        }
    }
    Extensions::Runtime::instance().notify_changed();
    // The button that called this lives in the rows being rebuilt.
    CallAfter([this, page]() { refresh_extensions(page); });
}

wxWindow *SetupWizardDialog::build_finish(wxWindow *parent)
{
    auto *page = step_page(parent);
    auto *s    = new wxBoxSizer(wxVERTICAL);
    auto *head = new wxBoxSizer(wxHORIZONTAL);
    head->Add(fixed_glyph(page, GlyphTask, MD3::Role::Primary, 32), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    m_finish_title = make_label(page, Label::Head_20, wxString());
    head->Add(m_finish_title, 1, wxALIGN_CENTER_VERTICAL);
    s->Add(head, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    m_finish_summary = make_label(page, Label::Body_14, wxString(), MD3::Role::OnSurfaceVariant, true);
    s->Add(m_finish_summary, 0, wxEXPAND | wxBOTTOM, FromDIP(16));
    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    auto *prepare = make_button(page, _L("Open Prepare"), GlyphPrint, Button::Variant::Filled);
    prepare->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        const Setup::Facts f = facts();
        if (!Setup::can_finish(f)) {
            on_next();
            return;
        }
        m_progress = Setup::finish(m_progress, now_seconds());
        save();
        EndModal(wxID_OK);
        if (wxGetApp().mainframe) wxGetApp().mainframe->select_tab(size_t(MainFrame::tp3DEditor));
    });
    auto *tour = make_button(page, _L("Take the feature tour"), GlyphTour);
    tour->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        const Setup::Facts f = facts();
        if (Setup::can_finish(f)) m_progress = Setup::finish(m_progress, now_seconds());
        save();
        EndModal(wxID_OK);
        if (wxGetApp().mainframe) wxGetApp().mainframe->open_every_feature();
    });
    actions->Add(prepare, 0, wxRIGHT, FromDIP(8));
    actions->Add(tour, 0);
    s->Add(actions, 0, wxBOTTOM, FromDIP(12));
    s->Add(make_label(page, Label::Body_12, _L("Setup stays in Help for later changes."), MD3::Role::OnSurfaceVariant), 0, wxBOTTOM, FromDIP(12));
    s->Add(results_box(page, Setup::index_of(Step::Finish)), 0, wxEXPAND);
    page->SetSizer(s);
    return page;
}

void SetupWizardDialog::refresh_finish()
{
    if (m_finish_summary == nullptr) return;
    const Setup::Facts f = facts();
    AppConfig *cfg = wxGetApp().app_config;
    const std::string interval = cfg->get("backup_switch") == "true" ? cfg->get("backup_interval") : "0";
    wxString backups;
    if (interval == "0" || interval.empty()) backups = _L("auto-backup off");
    else if (interval == "120") backups = _L("auto-backup every 2 minutes");
    else if (interval == "600") backups = _L("auto-backup every 10 minutes");
    else backups = wxString::Format(_L("auto-backup every %s seconds"), wxString::FromUTF8(interval));
    std::size_t plate = wxGetApp().preset_bundle ? wxGetApp().preset_bundle->filament_presets.size() : 0;
    m_finish_title->SetLabel(Setup::can_finish(f) ? _L("Ready to print") : _L("Almost ready"));
    wxString summary = wxString::Format(_L("%d printers, %d filaments on the plate, %s."), f.printers, int(plate), backups);
    if (!Setup::can_finish(f)) {
        summary << "\n";
        if (Setup::missing_for(Step::Printers, f) != Setup::Missing::None) summary << "\n" << _L("Still needed: at least one printer (Printers step).");
        if (Setup::missing_for(Step::Inks, f) != Setup::Missing::None) summary << "\n" << _L("Still needed: at least one filament preset (Inks step).");
    }
    m_finish_summary->SetLabel(summary);
    if (m_pages[std::size_t(Setup::index_of(Step::Finish))]) m_pages[std::size_t(Setup::index_of(Step::Finish))]->Layout();
}

// ------------------------------------------------------------------- navigation

Setup::Facts SetupWizardDialog::facts() const
{
    Setup::Facts f;
    f.printers  = int(your_printers().size());
    f.filaments = visible_filaments();
    return f;
}

void SetupWizardDialog::show_step(int index)
{
    index = std::clamp(index, 0, Setup::kStepCount - 1);
    if (!Setup::info(index).available) index = Setup::next_index(index);
    m_index       = index;
    m_next_anyway = false;
    m_progress    = Setup::go_to(m_progress, index, now_seconds());
    m_book->ChangeSelection(std::size_t(index));
    m_missing_card->Hide();

    const Step step = Setup::info(index).step;
    switch (step) {
    case Step::Language:
        m_title->SetLabel(_L("Language and humour"));
        m_lead->SetLabel(_L("Pick how Bambu Studio talks to you. Humour changes the tone, never the facts."));
        m_why_text->SetLabel(_L("The language and humour you pick show up in every dialog. Change them later in Preferences."));
        break;
    case Step::Look:
        m_title->SetLabel(_L("Look and motion"));
        m_lead->SetLabel(_L("Choose light or dark, how roomy controls are, and how much things move."));
        m_why_text->SetLabel(_L("These apply right away and stay in Preferences, Appearance."));
        break;
    case Step::Printers:
        m_title->SetLabel(_L("Add your printers"));
        m_lead->SetLabel(_L("Pick the printers and nozzles you own. Slicing presets, plates and process profiles come from these choices."));
        m_why_text->SetLabel(_L("Printers decide which plates, nozzles and process presets Prepare offers. Add more later from Prepare, then Printer."));
        m_rule_text->SetLabel(_L("At least one printer. Next stays available; it explains what is missing instead of greying out."));
        refresh_printers();
        break;
    case Step::Inks:
        m_title->SetLabel(_L("Inks and filaments"));
        m_lead->SetLabel(_L("Tell Bambu Studio what is loaded. Reading the AMS fills the plate's filament list from the printer."));
        m_why_text->SetLabel(_L("Filament presets set temperatures, cooling and flow for every slice."));
        m_rule_text->SetLabel(_L("At least one filament preset. Printers you add bring their own."));
        refresh_inks();
        break;
    case Step::Backups:
        m_title->SetLabel(_L("Backups"));
        m_lead->SetLabel(_L("Keep your work safe while you print. This step is optional."));
        m_why_text->SetLabel(_L("Auto-backup keeps a recovery copy of the open project; a config snapshot keeps your settings."));
        break;
    case Step::Extensions:
        m_title->SetLabel(_L("Extensions"));
        m_lead->SetLabel(_L("Turn on the built-in extensions you want. This step is optional."));
        m_why_text->SetLabel(_L("Extensions add notes after slicing and comment lines to exported G-code without changing how anything prints."));
        refresh_extensions(m_pages[std::size_t(index)]);
        break;
    case Step::Finish:
        refresh_finish();
        m_title->SetLabel(_L("Finish"));
        m_lead->SetLabel(_L("Here is what setup did. Open Prepare to start, or take the feature tour."));
        m_why_text->SetLabel(_L("Every choice here is in Preferences or Help afterwards."));
        break;
    }
    m_rule_card->Show(Setup::info(index).required);
    m_lead->Wrap(std::max(FromDIP(300), m_book->GetClientSize().x));
    refresh_rail();
    refresh_header();
    save();
    Layout();
}

void SetupWizardDialog::refresh_rail()
{
    for (int i = 0; i < Setup::kStepCount; ++i) {
        RailItem &item = m_rail[std::size_t(i)];
        const Setup::StepInfo &si = Setup::info(i);
        const Setup::State     st = m_progress.states[std::size_t(i)];
        wxString state;
        if (!si.available) {
            item.glyph = GlyphBlocked;
            state      = _L("Not built yet");
        } else if (i == m_index) {
            item.glyph = GlyphCurrent;
            state      = si.required ? _L("Required") : si.optional ? _L("Optional") : _L("Now");
        } else if (st == Setup::State::Done) {
            item.glyph = GlyphDone;
            state      = _L("Done");
        } else if (st == Setup::State::Skipped) {
            item.glyph = GlyphSkipped;
            state      = _L("Skipped");
        } else {
            item.glyph = GlyphPending;
            state      = si.required ? _L("Required") : si.optional ? _L("Optional") : wxString();
        }
        item.state->SetLabel(state);
        item.title->SetFont(i == m_index ? Label::Head_14 : Label::Body_14);
        item.title->SetForegroundColour(role(si.available ? MD3::Role::OnSurface : MD3::Role::OnSurfaceVariant));
        item.title->SetName(step_title(i) + ", " + state);
        item.icon->Refresh();
    }
}

void SetupWizardDialog::refresh_header()
{
    // The step pages refresh while the dialog is still being built.
    if (m_step_of == nullptr || m_next == nullptr) return;
    const Setup::Facts f = facts();
    m_step_of->SetLabel(wxString::Format(_L("Step %d of %d"), m_index + 1, Setup::kStepCount));
    int available = 0, settled = 0;
    for (int i = 0; i < Setup::kStepCount; ++i) {
        if (!Setup::info(i).available) continue;
        ++available;
        if (m_progress.states[std::size_t(i)] != Setup::State::Pending) ++settled;
    }
    if (m_progress_bar) m_progress_bar->SetValue(available ? settled * 100 / available : 0);
    const int left = Setup::required_left(f);
    m_required_left->SetLabel(left == 0 ? _L("Every required step is done") :
                              left == 1 ? _L("1 required step left") : wxString::Format(_L("%d required steps left"), left));
    const bool last = m_index == Setup::kStepCount - 1;
    m_next->SetLabel(last ? _L("Finish") : m_next_anyway ? _L("Next anyway") : _L("Next"));
    m_back->Enable(m_index > 0);
    m_skip->Show(Setup::info(m_index).optional);
    Layout();
}

void SetupWizardDialog::on_next()
{
    const Setup::Facts   f       = facts();
    const Setup::Missing missing = Setup::missing_for(Setup::info(m_index).step, f);
    if (m_index == Setup::kStepCount - 1) {
        if (!Setup::can_finish(f)) {
            m_missing_text->SetLabel(_L("Setup can finish once every required step is done. The summary above says what is still needed; click a step on the left to do it."));
            m_missing_card->Show();
            Layout();
            return;
        }
        m_progress = Setup::finish(m_progress, now_seconds());
        save();
        EndModal(wxID_OK);
        return;
    }
    if (missing != Setup::Missing::None && !m_next_anyway) {
        m_missing_text->SetLabel(missing == Setup::Missing::Printer
                                     ? _L("No printer is set up yet. Choose printers or import a printer profile first. You can also go on and come back; setup cannot finish without one.")
                                     : _L("No filament preset is available yet. Pick filaments first, or go on and come back; setup cannot finish without one."));
        m_missing_card->Show();
        m_next_anyway = true;
        refresh_header();
        return;
    }
    if (missing == Setup::Missing::None) m_progress = Setup::complete(m_progress, m_index, now_seconds());
    show_step(Setup::next_index(m_index));
}

void SetupWizardDialog::on_back()
{
    if (m_index > 0) show_step(Setup::previous_index(m_index));
}

void SetupWizardDialog::on_skip()
{
    if (!Setup::info(m_index).optional) return;
    m_progress = Setup::skip(m_progress, m_index, now_seconds());
    show_step(m_progress.current);
}

void SetupWizardDialog::save()
{
    AppConfig *cfg = wxGetApp().app_config;
    if (cfg == nullptr) return;
    if (m_progress.updated == 0) m_progress.updated = now_seconds();
    Setup::Progress before;
    const bool      was_finished = Setup::parse(cfg->get(Setup::kProgressKey), before) && before.finished;
    cfg->set(Setup::kProgressKey, Setup::serialize(m_progress));
    cfg->save();
    if (m_progress.finished && !was_finished) LocalTools::record_activity("onboarding", L("Finished setup"), std::string());
    if (m_saved)
        m_saved->SetLabel(wxString::Format(_L("Saved at %s. Close any time and continue from Help, then Set up Bambu Studio."),
                                           wxDateTime::Now().FormatISOTime()));
}

void SetupWizardDialog::save_and_close()
{
    save();
    if (IsModal()) EndModal(wxID_CANCEL);
    else Destroy();
}

void SetupWizardDialog::start_over()
{
    m_progress = Setup::start_over(m_first_run, now_seconds());
    m_banner->Hide();
    show_step(0);
}

void SetupWizardDialog::request_language(const std::string &mode)
{
    if (wxGetApp().plater() && wxGetApp().plater()->is_project_dirty()) {
        const int result = MessageDialog(this, _L("The current project has unsaved changes, save it before continuing?"),
                                         wxString(SLIC3R_APP_FULL_NAME) + " - " + _L("Save"), wxYES_NO | wxCANCEL | wxYES_DEFAULT | wxCENTRE)
                               .ShowModal();
        if (result == wxID_CANCEL) {
            m_language->SetSelection(language_choice());
            return;
        }
        if (result == wxID_YES) wxGetApp().plater()->save_project();
    }
    MessageDialog confirm(this, _L("Switching the language requires application restart.\n") + "\n" + _L("Do you want to continue?"),
                          _L("Language selection"), wxICON_QUESTION | wxOK | wxCANCEL);
    if (confirm.ShowModal() == wxID_CANCEL) {
        m_language->SetSelection(language_choice());
        return;
    }
    int action_buttons = UnsavedChangesDialog::ActionButtons::SAVE;
    if (!wxGetApp().check_and_keep_current_preset_changes(_L("Switching application language"),
                                                         _L("Switching application language while some presets are modified."), action_buttons)) {
        m_language->SetSelection(language_choice());
        return;
    }
    AppConfig        *cfg      = wxGetApp().app_config;
    const std::string previous = cfg->get("language");
    cfg->set("language", mode);
    cfg->save();
    if (!wxGetApp().load_language(from_u8(mode), false)) {
        cfg->set("language", previous);
        cfg->save();
        m_language->SetSelection(language_choice());
        add_result(Setup::index_of(Step::Language), GlyphError, false, _L("That language could not be loaded"), wxString());
        return;
    }
    // Keep the place, mark the step done and open again after the rebuild.
    m_progress = Setup::complete(m_progress, Setup::index_of(Step::Language), now_seconds());
    save();
    cfg->set(Setup::kReopenKey, "1");
    cfg->save();
    m_pending_language = mode;
    EndModal(wxID_OK);
}

}} // namespace Slic3r::GUI
