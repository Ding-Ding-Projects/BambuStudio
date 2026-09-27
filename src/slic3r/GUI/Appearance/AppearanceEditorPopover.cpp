#include "AppearanceEditorPopover.hpp"

#include <algorithm>
#include <set>

#include <wx/dcbuffer.h>
#include <wx/display.h>
#include <wx/filedlg.h>
#include <wx/fontenum.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/textdlg.h>

#include "ElementStyle.hpp"
#include "slic3r/GUI/GUI.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/CheckBox.hpp"
#include "slic3r/GUI/Widgets/ComboBox.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/ListBox.hpp"
#include "slic3r/GUI/Widgets/MD3ColorPicker.hpp"
#include "slic3r/GUI/Widgets/MD3Menu.hpp"
#include "slic3r/GUI/Widgets/MD3MenuModel.hpp"
#include "slic3r/GUI/Widgets/MD3Motion.hpp"
#include "slic3r/GUI/Widgets/MD3Tokens.hpp"
#include "slic3r/GUI/Widgets/MaterialIcon.hpp"
#include "slic3r/GUI/Widgets/SearchField.hpp"
#include "slic3r/GUI/Widgets/SpinInput.hpp"
#include "slic3r/GUI/Widgets/StateColor.hpp"

namespace Slic3r { namespace GUI {

namespace {

constexpr int kCardWidth   = 380; // DIP
constexpr int kFrame       = 1;   // px outline
constexpr int kPad         = 16;  // DIP card padding
constexpr int kRowGap      = 8;
constexpr int kListHeight  = 132;
constexpr int kTickMs      = 120;
constexpr int kMenuItemId  = wxID_HIGHEST + 9601;

// Bundled faces registered by Label::initSysFont from resources/fonts. The
// enumerator does not always list private (FR_PRIVATE) faces, so they are
// added explicitly and tagged so a user can tell them from installed ones.
const std::vector<const char *> &bundled_faces()
{
    static const std::vector<const char *> faces = {
        "Roboto", "Roboto Mono", "HarmonyOS Sans SC", "NanumGothic", "Source Han Sans JP", "Symbola",
    };
    return faces;
}

struct WeightChoice { int value; const char *label; };
const std::vector<WeightChoice> &weight_choices()
{
    static const std::vector<WeightChoice> w = {
        {100, "Thin (100)"},     {200, "Extra light (200)"}, {300, "Light (300)"},
        {400, "Regular (400)"},  {500, "Medium (500)"},      {600, "Semibold (600)"},
        {700, "Bold (700)"},     {800, "Extra bold (800)"},  {900, "Black (900)"},
    };
    return w;
}

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

// Per-menu binding state for append_edit_appearance_item (see the header).
struct MenuBinding
{
    std::string         element_id;
    wxWeakRef<wxWindow> anchor;
};
std::map<wxMenu *, MenuBinding> &menu_bindings()
{
    static auto *m = new std::map<wxMenu *, MenuBinding>();
    return *m;
}

} // namespace

AppearanceEditorPopover *AppearanceEditorPopover::s_current = nullptr;

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
AppearanceEditorPopover *AppearanceEditorPopover::open_for(wxWindow *anchor, const std::string &element_id)
{
    if (!anchor || element_id.empty())
        return nullptr;
    if (s_current) {
        s_current->retarget(anchor, element_id);
        s_current->Raise();
        return s_current;
    }
    s_current = new AppearanceEditorPopover(anchor, element_id);
    s_current->Show();
    MD3::Motion::FadeIn(s_current, MD3::Motion::short2);
    if (!s_current->m_section_buttons.empty())
        s_current->m_section_buttons[0]->SetFocus();
    return s_current;
}

AppearanceEditorPopover *AppearanceEditorPopover::current() { return s_current; }

void AppearanceEditorPopover::close_current()
{
    if (s_current)
        s_current->close_and_return_focus();
}

AppearanceEditorPopover::AppearanceEditorPopover(wxWindow *anchor, const std::string &element_id)
    : wxFrame(wxGetTopLevelParent(anchor), wxID_ANY, _L("Edit appearance"), wxDefaultPosition, wxDefaultSize,
              wxFRAME_TOOL_WINDOW | wxFRAME_NO_TASKBAR | wxFRAME_FLOAT_ON_PARENT | wxBORDER_NONE | wxCLIP_CHILDREN)
    , m_id(element_id)
    , m_anchor(anchor)
{
    SetName(_L("Edit appearance"));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(role(MD3::Role::SurfaceContainer));
    Bind(wxEVT_PAINT, &AppearanceEditorPopover::paint, this);
    Bind(wxEVT_ERASE_BACKGROUND, [](wxEraseEvent &) {});
    Bind(wxEVT_CHAR_HOOK, &AppearanceEditorPopover::on_char_hook, this);
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &) { close_and_return_focus(); });
    m_tick.Bind(wxEVT_TIMER, &AppearanceEditorPopover::on_tick, this);

    m_registry_token = ElementStyle::registry().subscribe([this](const std::string &changed) {
        if (m_loading)
            return;
        if (changed == StyleRegistry::kEveryElement || changed == m_id ||
            (m_id.size() > changed.size() && m_id.compare(0, changed.size(), changed) == 0 && m_id[changed.size()] == '/'))
            CallAfter([this]() { refresh_from_registry(); });
    });

    build();
    refresh_from_registry();
    place();
    m_tick.Start(kTickMs);
}

AppearanceEditorPopover::~AppearanceEditorPopover()
{
    m_tick.Stop();
    ElementStyle::registry().unsubscribe(m_registry_token);
    if (s_current == this)
        s_current = nullptr;
}

void AppearanceEditorPopover::retarget(wxWindow *anchor, const std::string &element_id)
{
    m_anchor = anchor;
    m_id     = element_id;
    m_last_anchor_rect = wxRect();
    refresh_from_registry();
    place();
}

void AppearanceEditorPopover::close_and_return_focus()
{
    m_tick.Stop();
    wxWindow *back = m_anchor.get();
    Hide();
    if (back && back->IsShownOnScreen() && back->IsEnabled())
        back->SetFocus();
    Destroy();
}

// ---------------------------------------------------------------------------
// Placement / anchor tracking
// ---------------------------------------------------------------------------
void AppearanceEditorPopover::place()
{
    wxWindow *anchor = m_anchor.get();
    if (!anchor)
        return;
    const wxRect anchor_rect = anchor->GetScreenRect();
    m_last_anchor_rect       = anchor_rect;
    int idx = wxDisplay::GetFromWindow(anchor);
    if (idx == wxNOT_FOUND)
        idx = 0;
    const wxRect display = wxDisplay(static_cast<unsigned>(idx)).GetClientArea();
    const wxSize wanted  = GetBestSize();
    const MD3::Menu::Placement p = MD3::Menu::place_root(anchor_rect, wanted, display);
    SetSize(p.rect);
    Layout();
}

void AppearanceEditorPopover::on_tick(wxTimerEvent &)
{
    wxWindow *anchor = m_anchor.get();
    if (!anchor) {
        // The element went away under us: close, nothing to return focus to.
        m_tick.Stop();
        Destroy();
        return;
    }
    if (!anchor->IsShownOnScreen()) {
        if (IsShown())
            Hide();
        return;
    }
    if (!IsShown())
        Show();
    if (anchor->GetScreenRect() != m_last_anchor_rect)
        place();
}

void AppearanceEditorPopover::paint(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(this);
    const wxRect r = GetClientRect();
    dc.SetBackground(wxBrush(StaticBox::GetParentBackgroundColor(GetParent())));
    dc.Clear();
    dc.SetPen(wxPen(role(MD3::Role::OutlineVariant), kFrame));
    dc.SetBrush(wxBrush(role(MD3::Role::SurfaceContainer)));
    dc.DrawRoundedRectangle(r, FromDIP(MD3::Metrics::radius_rail));
}

void AppearanceEditorPopover::on_char_hook(wxKeyEvent &e)
{
    if (e.GetKeyCode() == WXK_ESCAPE) {
        close_and_return_focus();
        return;
    }
    // Ctrl+PageUp / Ctrl+PageDown move between the sections.
    if (e.ControlDown() && (e.GetKeyCode() == WXK_PAGEDOWN || e.GetKeyCode() == WXK_PAGEUP)) {
        const int n = static_cast<int>(m_section_buttons.size());
        if (n > 0)
            show_section(((m_section + (e.GetKeyCode() == WXK_PAGEDOWN ? 1 : n - 1)) % n));
        return;
    }
    e.Skip();
}

// ---------------------------------------------------------------------------
// Building
// ---------------------------------------------------------------------------
Button *AppearanceEditorPopover::make_reset(wxWindow *parent, const char *key, const wxString &what)
{
    auto *b = new Button(parent, "", "", 0, 0, wxID_ANY);
    if (MaterialIcon::available()) {
        b->SetIconButton(Button::IconShape::Circle, 28);
        b->SetGlyph(MaterialIcon::Undo, 16);
    } else {
        b->SetLabel(wxString(wxUniChar(0x21BA)));
        b->SetVariant(Button::Variant::Text);
        b->SetButtonSize(Button::Size::Small);
    }
    b->SetName(wxString::Format(_L("Reset %s"), what));
    b->SetToolTip(wxString::Format(_L("Reset %s to the preset / default value"), what));
    const std::string k = key;
    b->Bind(wxEVT_BUTTON, [this, k](wxCommandEvent &) { reset_property(k.c_str()); });
    m_reset_buttons[key] = b;
    return b;
}

Button *AppearanceEditorPopover::make_swatch(wxWindow *parent, const char *key, const wxString &what)
{
    auto *b = new Button(parent, wxString::FromUTF8("\xE2\x80\x94"));
    b->SetVariant(Button::Variant::Outlined);
    b->SetButtonSize(Button::Size::Small);
    b->SetMinSize(wxSize(FromDIP(120), FromDIP(32)));
    b->SetName(what);
    b->SetToolTip(wxString::Format(_L("Choose the %s colour"), what));
    const std::string k = key;
    b->Bind(wxEVT_BUTTON, [this, k, b](wxCommandEvent &) {
        wxColour initial = style_colour_from_json(ElementStyle::registry().resolve(m_id, k), wxNullColour);
        if (!initial.IsOk()) {
            wxWindow *a = m_anchor.get();
            if (k == StyleProp::background && a)
                initial = a->GetBackgroundColour();
            else if (k == StyleProp::foreground && a)
                initial = a->GetForegroundColour();
            else if (k == StyleProp::highlight)
                initial = role(MD3::Role::PrimaryContainer);
            else
                initial = role(MD3::Role::Outline);
        }
        MD3ColorPickerDialog dlg(this, initial);
        if (dlg.ShowModal() == wxID_OK)
            write_string(k.c_str(), style_colour_to_string(dlg.GetColour()));
        b->SetFocus();
    });
    m_swatches[key] = b;
    return b;
}

void AppearanceEditorPopover::build()
{
    auto *root = new wxBoxSizer(wxVERTICAL);
    const int pad = FromDIP(kPad);

    // Header: title (display name) + caption (stable id) + close.
    auto *head = new wxBoxSizer(wxHORIZONTAL);
    auto *titles = new wxBoxSizer(wxVERTICAL);
    m_title = new Label(this, Label::Head_16, _L("Edit appearance"), wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    m_subtitle = new Label(this, Label::Body_12, wxEmptyString, wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    m_subtitle->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    titles->Add(m_title, 0, wxEXPAND);
    titles->Add(m_subtitle, 0, wxEXPAND | wxTOP, FromDIP(2));
    head->Add(titles, 1, wxALIGN_CENTER_VERTICAL);
    auto *close = new Button(this, "", "", 0, 0, wxID_ANY);
    if (MaterialIcon::available()) {
        close->SetIconButton(Button::IconShape::Circle, 32);
        close->SetGlyph(MaterialIcon::Close, 18);
    } else {
        close->SetLabel(wxString(wxUniChar(0x2715)));
        close->SetVariant(Button::Variant::Text);
    }
    close->SetName(_L("Close appearance editor"));
    close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { close_and_return_focus(); });
    head->Add(close, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    root->Add(head, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    // Section tabs.
    auto *tabs = new wxBoxSizer(wxHORIZONTAL);
    const wxString names[] = {_L("Typography"), _L("Colours"), _L("Shape & spacing"), _L("Presets")};
    for (int i = 0; i < 4; ++i) {
        auto *b = new Button(this, names[i]);
        b->SetVariant(i == 0 ? Button::Variant::Tonal : Button::Variant::Text);
        b->SetButtonSize(Button::Size::Small);
        b->SetName(wxString::Format(_L("%s section"), names[i]));
        b->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent &) { show_section(i); });
        tabs->Add(b, 0, wxRIGHT, FromDIP(4));
        m_section_buttons.push_back(b);
        ElementStyle::apply(b, "appearance-editor.tab", _L("Appearance editor tab"));
    }
    root->Add(tabs, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

    m_book = new wxSimplebook(this, wxID_ANY);
    m_book->SetBackgroundColour(role(MD3::Role::SurfaceContainer));
    auto add_page = [this](const wxString &name, void (AppearanceEditorPopover::*builder)(wxWindow *)) {
        auto *page = new wxPanel(m_book, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL);
        page->SetBackgroundColour(role(MD3::Role::SurfaceContainer));
        page->SetName(name);
        (this->*builder)(page);
        m_book->AddPage(page, name);
    };
    add_page(_L("Typography"), &AppearanceEditorPopover::build_typography);
    add_page(_L("Colours"), &AppearanceEditorPopover::build_colours);
    add_page(_L("Shape & spacing"), &AppearanceEditorPopover::build_shape);
    add_page(_L("Presets"), &AppearanceEditorPopover::build_presets);
    root->Add(m_book, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(10));

    // Footer: element reset, global reset.
    auto *foot = new wxBoxSizer(wxHORIZONTAL);
    auto *reset_el = new Button(this, _L("Reset element"));
    reset_el->SetVariant(Button::Variant::Outlined);
    reset_el->SetButtonSize(Button::Size::Small);
    reset_el->SetToolTip(_L("Drop every override of this element; the active preset still applies"));
    reset_el->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        ElementStyle::registry().reset_element(m_id);
        persist();
    });
    auto *reset_all = new Button(this, _L("Reset all"));
    reset_all->SetVariant(Button::Variant::Text);
    reset_all->SetButtonSize(Button::Size::Small);
    reset_all->SetToolTip(_L("Drop every element override and return to the Material default preset (saved presets are kept)"));
    reset_all->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        wxMessageDialog ask(this,
                            _L("Reset the appearance of every element and return to the Material default preset? "
                               "Your saved presets are kept."),
                            _L("Reset all appearance overrides"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
        if (ask.ShowModal() != wxID_YES)
            return;
        ElementStyle::registry().reset_all();
        persist();
    });
    foot->Add(reset_el, 0);
    foot->Add(reset_all, 0, wxLEFT, FromDIP(8));
    foot->AddStretchSpacer(1);
    auto *shortcut = new Label(this, Label::Body_11, AppearanceEditor::shortcut_text());
    shortcut->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    shortcut->SetToolTip(_L("Keyboard shortcut that opens this editor for the focused control"));
    foot->Add(shortcut, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(foot, 0, wxEXPAND | wxALL, pad);

    SetSizer(root);
    SetMinSize(wxSize(FromDIP(kCardWidth), -1));
    root->SetSizeHints(this);

    // The editor obeys its own customization: its title and body copy are
    // adopted like any other element.
    ElementStyle::apply(m_title, "appearance-editor.title", _L("Appearance editor title"));
    ElementStyle::apply(m_subtitle, "appearance-editor.caption", _L("Appearance editor caption"));
}

void AppearanceEditorPopover::build_typography(wxWindow *page)
{
    auto *s = new wxBoxSizer(wxVERTICAL);
    auto row = [&](const wxString &label, wxWindow *control, const char *key, const wxString &what) {
        auto *r = new wxBoxSizer(wxHORIZONTAL);
        auto *l = new Label(page, Label::Body_13, label, 0, wxSize(FromDIP(110), -1));
        ElementStyle::apply(l, "appearance-editor.label", _L("Appearance editor label"));
        r->Add(l, 0, wxALIGN_CENTER_VERTICAL);
        r->Add(control, 1, wxALIGN_CENTER_VERTICAL);
        if (key)
            r->Add(make_reset(page, key, what), 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
        s->Add(r, 0, wxEXPAND | wxBOTTOM, FromDIP(kRowGap));
    };

    // Font family: searchable list (installed + bundled) with a live preview.
    m_font_search = new SearchField(page, _L("Search fonts"));
    m_font_search->SetOnQuery([this](const wxString &) { refresh_font_list(); });
    m_font_search->SetOnRegexToggle([this](bool) { refresh_font_list(); });
    s->Add(m_font_search, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    auto *family_row = new wxBoxSizer(wxHORIZONTAL);
    m_font_list = new ListBox(page, wxID_ANY, wxSize(-1, FromDIP(kListHeight)));
    m_font_list->SetName(_L("Font family"));
    m_font_list->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &) {
        if (m_loading)
            return;
        const int sel = m_font_list->GetSelection();
        if (sel < 0 || sel >= static_cast<int>(m_font_visible.size()))
            return;
        const wxString face = m_font_faces[m_font_visible[sel]];
        write_string(StyleProp::font_family, std::string(face.ToUTF8().data()));
    });
    family_row->Add(m_font_list, 1, wxEXPAND);
    family_row->Add(make_reset(page, StyleProp::font_family, _L("font family")), 0, wxALIGN_TOP | wxLEFT, FromDIP(4));
    s->Add(family_row, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    m_font_preview = new Label(page, Label::Body_14, _L("The quick brown fox 0123 Aa Bb"), wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    m_font_preview->SetName(_L("Font preview"));
    m_font_preview->SetMinSize(wxSize(-1, FromDIP(28)));
    s->Add(m_font_preview, 0, wxEXPAND | wxBOTTOM, FromDIP(kRowGap));

    m_size = new wxSpinCtrlDouble(page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(96), -1),
                                  wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 4.0, 96.0, 13.0, 0.5);
    m_size->SetName(_L("Font size in points"));
    m_size->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent &e) { if (!m_loading) write_number(StyleProp::font_size, e.GetValue()); });
    m_size->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &) { if (!m_loading) write_number(StyleProp::font_size, m_size->GetValue()); });
    row(_L("Size (pt)"), m_size, StyleProp::font_size, _L("font size"));

    m_weight = new ComboBox(page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(170), -1), 0, nullptr, wxCB_READONLY);
    for (const WeightChoice &w : weight_choices())
        m_weight->Append(_L(w.label));
    m_weight->SetName(_L("Font weight"));
    m_weight->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) {
        const int sel = m_weight->GetSelection();
        if (!m_loading && sel >= 0 && sel < static_cast<int>(weight_choices().size()))
            write_number(StyleProp::font_weight, weight_choices()[sel].value);
    });
    row(_L("Weight"), m_weight, StyleProp::font_weight, _L("font weight"));

    auto *deco = new wxBoxSizer(wxHORIZONTAL);
    auto add_check = [&](CheckBox *&slot, const wxString &label, const char *key) {
        slot = new CheckBox(page);
        slot->SetName(label);
        slot->SetToolTip(label);
        const std::string k = key;
        slot->Bind(wxEVT_TOGGLEBUTTON, [this, k, &slot](wxCommandEvent &e) {
            e.Skip();
            if (m_loading)
                return;
            if (k == StyleProp::font_style)
                write_string(k.c_str(), slot->GetValue() ? "italic" : "normal");
            else
                write_bool(k.c_str(), slot->GetValue());
        });
        deco->Add(slot, 0, wxALIGN_CENTER_VERTICAL);
        auto *l = new Label(page, Label::Body_13, label);
        ElementStyle::apply(l, "appearance-editor.label", _L("Appearance editor label"));
        deco->Add(l, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
        deco->Add(make_reset(page, key, label.Lower()), 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(4));
    };
    add_check(m_italic, _L("Italic"), StyleProp::font_style);
    add_check(m_underline, _L("Underline"), StyleProp::underline);
    add_check(m_strike, _L("Strikethrough"), StyleProp::strikethrough);
    s->Add(deco, 0, wxEXPAND | wxBOTTOM, FromDIP(kRowGap));

    m_letter_spacing = new wxSpinCtrlDouble(page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(96), -1),
                                            wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, -4.0, 20.0, 0.0, 0.1);
    m_letter_spacing->SetName(_L("Letter spacing in pixels"));
    m_letter_spacing->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent &e) { if (!m_loading) write_number(StyleProp::letter_spacing, e.GetValue()); });
    row(_L("Letter spacing"), m_letter_spacing, StyleProp::letter_spacing, _L("letter spacing"));

    m_line_height = new wxSpinCtrlDouble(page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(96), -1),
                                         wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0.8, 3.0, 1.0, 0.05);
    m_line_height->SetName(_L("Line height multiplier"));
    m_line_height->Bind(wxEVT_SPINCTRLDOUBLE, [this](wxSpinDoubleEvent &e) { if (!m_loading) write_number(StyleProp::line_height, e.GetValue()); });
    row(_L("Line height"), m_line_height, StyleProp::line_height, _L("line height"));

    auto *note = new Label(page, Label::Body_11,
                           _L("Letter spacing and line height are stored for widgets that measure their own text; native labels ignore them."),
                           LB_AUTO_WRAP);
    note->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    note->Wrap(FromDIP(kCardWidth - 2 * kPad));
    s->Add(note, 0, wxEXPAND);

    page->SetSizer(s);
}

void AppearanceEditorPopover::build_colours(wxWindow *page)
{
    auto *s = new wxBoxSizer(wxVERTICAL);
    struct Row { const char *key; wxString label; };
    const Row rows[] = {
        {StyleProp::foreground, _L("Text")},
        {StyleProp::background, _L("Background")},
        {StyleProp::highlight, _L("Highlight")},
        {StyleProp::border_color, _L("Border")},
    };
    for (const Row &r : rows) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        auto *l = new Label(page, Label::Body_13, r.label, 0, wxSize(FromDIP(110), -1));
        ElementStyle::apply(l, "appearance-editor.label", _L("Appearance editor label"));
        row->Add(l, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(make_swatch(page, r.key, r.label), 0, wxALIGN_CENTER_VERTICAL);
        row->AddStretchSpacer(1);
        row->Add(make_reset(page, r.key, r.label.Lower() + " " + _L("colour")), 0, wxALIGN_CENTER_VERTICAL);
        s->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(kRowGap));
    }
    auto *note = new Label(page, Label::Body_11,
                           _L("Each swatch opens the Material colour picker with its colour translator. An unset colour keeps the theme's token."),
                           LB_AUTO_WRAP);
    note->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    note->Wrap(FromDIP(kCardWidth - 2 * kPad));
    s->Add(note, 0, wxEXPAND);
    page->SetSizer(s);
}

void AppearanceEditorPopover::build_shape(wxWindow *page)
{
    auto *s = new wxBoxSizer(wxVERTICAL);
    auto spin_row = [&](SpinInput *&slot, const wxString &label, const char *key, int max) {
        slot = new SpinInput(page, "0", "", wxDefaultPosition, wxSize(FromDIP(110), -1), wxTE_PROCESS_ENTER, 0, max, 0);
        slot->SetName(label);
        const std::string k = key;
        slot->Bind(wxEVT_SPINCTRL, [this, k, &slot](wxCommandEvent &) { if (!m_loading) write_number(k.c_str(), slot->GetValue()); });
        slot->Bind(wxEVT_TEXT_ENTER, [this, k, &slot](wxCommandEvent &) { if (!m_loading) write_number(k.c_str(), slot->GetValue()); });
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        auto *l = new Label(page, Label::Body_13, label, 0, wxSize(FromDIP(110), -1));
        ElementStyle::apply(l, "appearance-editor.label", _L("Appearance editor label"));
        row->Add(l, 0, wxALIGN_CENTER_VERTICAL);
        row->Add(slot, 0, wxALIGN_CENTER_VERTICAL);
        row->AddStretchSpacer(1);
        row->Add(make_reset(page, key, label.Lower()), 0, wxALIGN_CENTER_VERTICAL);
        s->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(kRowGap));
    };
    spin_row(m_border_width, _L("Border width (px)"), StyleProp::border_width, 12);
    spin_row(m_radius, _L("Corner radius (px)"), StyleProp::radius, 64);
    spin_row(m_padding, _L("Padding (px)"), StyleProp::padding, 64);
    spin_row(m_margin, _L("Margin (px)"), StyleProp::margin, 64);
    auto *note = new Label(page, Label::Body_11,
                           _L("Shape values are read by the Material widgets that paint their own frame (buttons, tabs, menus). Native controls keep the platform shape."),
                           LB_AUTO_WRAP);
    note->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    note->Wrap(FromDIP(kCardWidth - 2 * kPad));
    s->Add(note, 0, wxEXPAND);
    page->SetSizer(s);
}

void AppearanceEditorPopover::build_presets(wxWindow *page)
{
    auto *s = new wxBoxSizer(wxVERTICAL);
    m_preset_active = new Label(page, Label::Body_12, wxEmptyString, wxST_ELLIPSIZE_END | wxST_NO_AUTORESIZE);
    m_preset_active->SetForegroundColour(role(MD3::Role::OnSurfaceVariant));
    s->Add(m_preset_active, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    m_preset_search = new SearchField(page, _L("Search presets"));
    m_preset_search->SetOnQuery([this](const wxString &) { refresh_preset_list(); });
    m_preset_search->SetOnRegexToggle([this](bool) { refresh_preset_list(); });
    s->Add(m_preset_search, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    m_preset_list = new ListBox(page, wxID_ANY, wxSize(-1, FromDIP(kListHeight)));
    m_preset_list->SetName(_L("Appearance presets"));
    m_preset_list->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &) {
        const int sel = m_preset_list->GetSelection();
        const bool ok = sel >= 0 && sel < static_cast<int>(m_preset_visible.size());
        if (m_preset_apply)
            m_preset_apply->Enable(ok);
        if (m_preset_delete)
            m_preset_delete->Enable(ok && !ElementStyle::registry().is_shipped_preset(m_preset_visible[sel]));
    });
    m_preset_list->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent &) {
        const int sel = m_preset_list->GetSelection();
        if (sel >= 0 && sel < static_cast<int>(m_preset_visible.size())) {
            ElementStyle::registry().set_active_preset(m_preset_visible[sel]);
            persist();
        }
    });
    s->Add(m_preset_list, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    m_preset_apply = new Button(page, _L("Apply"));
    m_preset_apply->SetVariant(Button::Variant::Filled);
    m_preset_apply->SetButtonSize(Button::Size::Small);
    m_preset_apply->SetToolTip(_L("Make the selected preset the active one; your element overrides stay on top of it"));
    m_preset_apply->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        const int sel = m_preset_list->GetSelection();
        if (sel >= 0 && sel < static_cast<int>(m_preset_visible.size())) {
            ElementStyle::registry().set_active_preset(m_preset_visible[sel]);
            persist();
        }
    });
    auto *save_as = new Button(page, _L("Save as preset..."));
    save_as->SetVariant(Button::Variant::Tonal);
    save_as->SetButtonSize(Button::Size::Small);
    save_as->SetToolTip(_L("Snapshot the current look of every styled element as a named preset"));
    save_as->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        wxTextEntryDialog ask(this, _L("Name for the new appearance preset"), _L("Save as preset"));
        if (ask.ShowModal() != wxID_OK)
            return;
        wxString name = ask.GetValue();
        name.Trim(true).Trim(false);
        if (name.IsEmpty())
            return;
        StyleRegistry &reg = ElementStyle::registry();
        const std::string n(name.ToUTF8().data());
        if (reg.is_shipped_preset(n)) {
            wxMessageBox(wxString::Format(_L("\"%s\" is a shipped preset and cannot be overwritten. Choose another name."), name),
                         _L("Save as preset"), wxOK | wxICON_INFORMATION, this);
            return;
        }
        reg.save_preset(n);
        reg.set_active_preset(n);
        persist();
    });
    m_preset_delete = new Button(page, _L("Delete"));
    m_preset_delete->SetVariant(Button::Variant::Text);
    m_preset_delete->SetButtonSize(Button::Size::Small);
    m_preset_delete->SetToolTip(_L("Delete the selected user preset (shipped presets cannot be deleted)"));
    m_preset_delete->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        const int sel = m_preset_list->GetSelection();
        if (sel < 0 || sel >= static_cast<int>(m_preset_visible.size()))
            return;
        const std::string name = m_preset_visible[sel];
        wxMessageDialog ask(this, wxString::Format(_L("Delete the appearance preset \"%s\"? This cannot be undone."), wxString::FromUTF8(name)),
                            _L("Delete preset"), wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
        if (ask.ShowModal() != wxID_YES)
            return;
        ElementStyle::registry().delete_preset(name);
        persist();
    });
    actions->Add(m_preset_apply, 0);
    actions->Add(save_as, 0, wxLEFT, FromDIP(6));
    actions->Add(m_preset_delete, 0, wxLEFT, FromDIP(6));
    s->Add(actions, 0, wxBOTTOM, FromDIP(8));

    auto *io = new wxBoxSizer(wxHORIZONTAL);
    auto *export_btn = new Button(page, _L("Export theme..."));
    export_btn->SetVariant(Button::Variant::Outlined);
    export_btn->SetButtonSize(Button::Size::Small);
    export_btn->SetToolTip(_L("Write every preset and override to a JSON file you can keep or share"));
    export_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        wxFileDialog dlg(this, _L("Export appearance theme"), wxEmptyString, "appearance-theme.json",
                         "JSON (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK)
            return;
        std::string err;
        if (!ElementStyle::registry().export_theme(std::string(dlg.GetPath().ToUTF8().data()), &err))
            wxMessageBox(wxString::Format(_L("The theme could not be exported: %s"), wxString::FromUTF8(err)),
                         _L("Export appearance theme"), wxOK | wxICON_ERROR, this);
    });
    auto *import_btn = new Button(page, _L("Import theme..."));
    import_btn->SetVariant(Button::Variant::Outlined);
    import_btn->SetButtonSize(Button::Size::Small);
    import_btn->SetToolTip(_L("Load presets and overrides from a JSON theme file; unknown properties are kept and listed"));
    import_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        wxFileDialog dlg(this, _L("Import appearance theme"), wxEmptyString, wxEmptyString,
                         "JSON (*.json)|*.json", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK)
            return;
        const StyleLoadReport r = ElementStyle::registry().import_theme(std::string(dlg.GetPath().ToUTF8().data()));
        if (!r.ok) {
            wxMessageBox(wxString::Format(_L("The theme could not be imported: %s"), wxString::FromUTF8(r.error)),
                         _L("Import appearance theme"), wxOK | wxICON_ERROR, this);
            return;
        }
        persist();
        if (!r.unknown_properties.empty() || !r.unknown_top_level.empty()) {
            wxString list;
            for (const std::string &k : r.unknown_top_level)
                list << wxString::FromUTF8(k) << "\n";
            for (const std::string &k : r.unknown_properties)
                list << wxString::FromUTF8(k) << "\n";
            wxMessageBox(wxString::Format(_L("Imported. These entries were kept but are not understood by this version:\n%s"), list),
                         _L("Import appearance theme"), wxOK | wxICON_INFORMATION, this);
        }
    });
    io->Add(export_btn, 0);
    io->Add(import_btn, 0, wxLEFT, FromDIP(6));
    s->Add(io, 0);
    page->SetSizer(s);
}

void AppearanceEditorPopover::show_section(int index)
{
    if (!m_book || index < 0 || index >= static_cast<int>(m_book->GetPageCount()))
        return;
    m_section = index;
    m_book->ChangeSelection(static_cast<size_t>(index));
    for (int i = 0; i < static_cast<int>(m_section_buttons.size()); ++i)
        m_section_buttons[i]->SetVariant(i == index ? Button::Variant::Tonal : Button::Variant::Text);
    Layout();
    place();
    if (index < static_cast<int>(m_section_buttons.size()))
        m_section_buttons[index]->SetFocus();
}

// ---------------------------------------------------------------------------
// Registry <-> controls
// ---------------------------------------------------------------------------
void AppearanceEditorPopover::refresh_font_list()
{
    if (!m_font_list)
        return;
    if (m_all_fonts.empty()) {
        std::set<wxString> seen;
        for (const char *face : bundled_faces()) {
            const wxString f = wxString::FromUTF8(face);
            m_font_faces.push_back(f);
            m_all_fonts.push_back(f + "  " + _L("(bundled)"));
            seen.insert(f);
        }
        wxArrayString installed = wxFontEnumerator::GetFacenames(wxFONTENCODING_SYSTEM, false);
        installed.Sort();
        for (const wxString &f : installed) {
            if (f.IsEmpty() || f[0] == '@' || seen.count(f))
                continue;
            seen.insert(f);
            m_font_faces.push_back(f);
            m_all_fonts.push_back(f);
        }
    }
    const wxString query = m_font_search ? m_font_search->GetValue() : wxString();
    SearchField::MatchPass pass(query, m_font_search && m_font_search->IsRegexEnabled(),
                                m_font_search && m_font_search->IsCaseSensitive(),
                                m_font_search && m_font_search->IsWholeWord(),
                                m_font_search && m_font_search->IsMultiline());
    m_font_visible.clear();
    std::vector<wxString> rows;
    for (int i = 0; i < static_cast<int>(m_all_fonts.size()); ++i) {
        if (!pass.matches(m_all_fonts[i]))
            continue;
        m_font_visible.push_back(i);
        rows.push_back(m_all_fonts[i]);
    }
    const bool was_loading = m_loading;
    m_loading = true;
    m_font_list->Set(rows);
    const wxString current = wxString::FromUTF8(
        ElementStyle::registry().resolve(m_id, StyleProp::font_family).is_string()
            ? ElementStyle::registry().resolve(m_id, StyleProp::font_family).get<std::string>()
            : std::string());
    for (int v = 0; v < static_cast<int>(m_font_visible.size()); ++v)
        if (m_font_faces[m_font_visible[v]] == current) {
            m_font_list->SetSelection(v);
            break;
        }
    m_loading = was_loading;
}

void AppearanceEditorPopover::refresh_preset_list()
{
    if (!m_preset_list)
        return;
    StyleRegistry &reg = ElementStyle::registry();
    const wxString query = m_preset_search ? m_preset_search->GetValue() : wxString();
    SearchField::MatchPass pass(query, m_preset_search && m_preset_search->IsRegexEnabled(),
                                m_preset_search && m_preset_search->IsCaseSensitive(),
                                m_preset_search && m_preset_search->IsWholeWord(),
                                m_preset_search && m_preset_search->IsMultiline());
    m_preset_visible.clear();
    std::vector<wxString> rows;
    for (const std::string &name : reg.preset_names()) {
        wxString label = wxString::FromUTF8(name);
        if (reg.is_shipped_preset(name))
            label << "  " << _L("(shipped)");
        if (name == reg.active_preset())
            label << "  " << wxString::FromUTF8("\xE2\x80\xA2 ") << _L("active");
        if (!pass.matches(label))
            continue;
        m_preset_visible.push_back(name);
        rows.push_back(label);
    }
    m_preset_list->Set(rows);
    for (int i = 0; i < static_cast<int>(m_preset_visible.size()); ++i)
        if (m_preset_visible[i] == reg.active_preset()) {
            m_preset_list->SetSelection(i);
            break;
        }
    if (m_preset_active)
        m_preset_active->SetLabel(wxString::Format(_L("Active preset: %s"), wxString::FromUTF8(reg.active_preset())));
    const int sel = m_preset_list->GetSelection();
    const bool ok = sel >= 0 && sel < static_cast<int>(m_preset_visible.size());
    if (m_preset_apply)
        m_preset_apply->Enable(ok);
    if (m_preset_delete)
        m_preset_delete->Enable(ok && !reg.is_shipped_preset(m_preset_visible[sel]));
}

void AppearanceEditorPopover::refresh_reset_buttons()
{
    const StyleRegistry &reg = ElementStyle::registry();
    for (auto &kv : m_reset_buttons)
        kv.second->Enable(reg.has_override(m_id, kv.first));
}

void AppearanceEditorPopover::refresh_from_registry()
{
    m_loading = true;
    StyleRegistry &reg = ElementStyle::registry();
    const StyleBag bag = reg.resolved_bag(m_id);

    if (m_title)
        m_title->SetLabel(wxString::Format(_L("Edit appearance: %s"), ElementStyle::display_name_of(m_id)));
    if (m_subtitle)
        m_subtitle->SetLabel(wxString::FromUTF8(m_id));

    auto number = [&](const char *key, double fallback) {
        auto it = bag.find(key);
        return it != bag.end() && it->is_number() ? it->get<double>() : fallback;
    };
    auto boolean = [&](const char *key) {
        auto it = bag.find(key);
        return it != bag.end() && it->is_boolean() && it->get<bool>();
    };

    wxWindow *anchor = m_anchor.get();
    const wxFont anchor_font = anchor ? anchor->GetFont() : wxFont();
    if (m_size)
        m_size->SetValue(number(StyleProp::font_size, anchor_font.IsOk() ? anchor_font.GetFractionalPointSize() : 13.0));
    if (m_weight) {
        const int w = static_cast<int>(number(StyleProp::font_weight, anchor_font.IsOk() ? anchor_font.GetNumericWeight() : 400));
        int best = 3, best_d = 10000;
        for (int i = 0; i < static_cast<int>(weight_choices().size()); ++i) {
            const int d = std::abs(weight_choices()[i].value - w);
            if (d < best_d) { best_d = d; best = i; }
        }
        m_weight->SetSelection(best);
    }
    if (m_italic) {
        auto it = bag.find(StyleProp::font_style);
        m_italic->SetValue(it != bag.end() && it->is_string() && it->get<std::string>() == "italic");
    }
    if (m_underline)
        m_underline->SetValue(boolean(StyleProp::underline));
    if (m_strike)
        m_strike->SetValue(boolean(StyleProp::strikethrough));
    if (m_letter_spacing)
        m_letter_spacing->SetValue(number(StyleProp::letter_spacing, 0.0));
    if (m_line_height)
        m_line_height->SetValue(number(StyleProp::line_height, 1.0));
    if (m_font_preview) {
        const wxFont preview = ElementStyle::font_for(m_id, anchor_font.IsOk() ? anchor_font : Label::Body_14);
        m_font_preview->SetFont(preview);
        m_font_preview->SetForegroundColour(ElementStyle::colour_for(m_id, StyleProp::foreground, role(MD3::Role::OnSurface)));
        m_font_preview->Refresh();
    }
    refresh_font_list();

    for (auto &kv : m_swatches) {
        const wxColour c = style_colour_from_json(bag.contains(kv.first) ? bag[kv.first] : nlohmann::json(), wxNullColour);
        if (c.IsOk()) {
            kv.second->SetLabel(wxString::FromUTF8(style_colour_to_string(c)));
            kv.second->SetBackgroundColorNormal(c);
            // Legible label on any swatch: white on dark, near-black on light.
            const int luma = (c.Red() * 299 + c.Green() * 587 + c.Blue() * 114) / 1000;
            kv.second->SetTextColorNormal(luma < 140 ? wxColour(255, 255, 255) : wxColour(26, 27, 31));
        } else {
            kv.second->SetLabel(_L("Theme default"));
            kv.second->SetBackgroundColorNormal(role(MD3::Role::SurfaceContainerHighest));
            kv.second->SetTextColorNormal(role(MD3::Role::OnSurface));
        }
        kv.second->Refresh();
    }

    if (m_border_width) m_border_width->SetValue(static_cast<int>(number(StyleProp::border_width, 0)));
    if (m_radius)       m_radius->SetValue(static_cast<int>(number(StyleProp::radius, 0)));
    if (m_padding)      m_padding->SetValue(static_cast<int>(number(StyleProp::padding, 0)));
    if (m_margin)       m_margin->SetValue(static_cast<int>(number(StyleProp::margin, 0)));

    refresh_preset_list();
    refresh_reset_buttons();
    m_loading = false;
    Layout();
}

void AppearanceEditorPopover::write_number(const char *key, double value)
{
    ElementStyle::registry().set(m_id, key, value);
    persist();
}

void AppearanceEditorPopover::write_bool(const char *key, bool value)
{
    ElementStyle::registry().set(m_id, key, value);
    persist();
}

void AppearanceEditorPopover::write_string(const char *key, const std::string &value)
{
    ElementStyle::registry().set(m_id, key, value);
    persist();
}

void AppearanceEditorPopover::reset_property(const char *key)
{
    ElementStyle::registry().reset_property(m_id, key);
    persist();
}

void AppearanceEditorPopover::persist()
{
    // Coalesce a burst of spin/drag edits into one write per event-loop turn.
    if (m_save_pending)
        return;
    m_save_pending = true;
    CallAfter([this]() {
        m_save_pending = false;
        ElementStyle::save();
        refresh_from_registry();
    });
}

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------
namespace AppearanceEditor {

void init(const std::string &storage_dir)
{
    ElementStyle::set_attach_hook([](wxWindow *w, const std::string &id) { attach(w, id); });
    ElementStyle::set_storage_dir(storage_dir);
}

wxString shortcut_text() { return wxString::FromUTF8(shortkey_ctrl_prefix()) + "Shift+E"; }

void open_for(wxWindow *anchor, const std::string &element_id)
{
    AppearanceEditorPopover::open_for(anchor, element_id);
}

bool open_for_focused()
{
    wxWindow *focused = wxWindow::FindFocus();
    if (!focused)
        return false;
    // Never edit the editor's own chrome through the shortcut: it would
    // re-target the card to itself.
    for (wxWindow *w = focused; w; w = w->GetParent())
        if (dynamic_cast<AppearanceEditorPopover *>(w))
            return false;
    std::string id = ElementStyle::element_id_of(focused);
    wxWindow *  anchor = focused;
    if (id.empty()) {
        // Any focused control becomes editable: adopt it under a generic id
        // derived from its accessible name (or class) so the base is
        // remembered and live restyle works.
        wxString name = focused->GetName();
        if (name.IsEmpty() || name == "panel" || name == "wxWindow" || name == "scrolledWindow")
            name = focused->GetClassInfo() ? focused->GetClassInfo()->GetClassName() : wxString("control");
        std::string slug;
        for (wxUniChar ch : name) {
            const int c = static_cast<int>(ch.GetValue());
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
                slug += static_cast<char>(c);
            else if (c >= 'A' && c <= 'Z')
                slug += static_cast<char>(c - 'A' + 'a');
            else if (c == ' ' || c == '-' || c == '_' || c == '.')
                slug += '-';
        }
        while (!slug.empty() && slug.back() == '-')
            slug.pop_back();
        if (slug.empty())
            slug = "control";
        id = "focused/" + slug;
        ElementStyle::apply(focused, id, name);
    }
    AppearanceEditorPopover::open_for(anchor, id);
    return true;
}

int edit_appearance_item_id() { return kMenuItemId; }

wxMenuItem *append_edit_appearance_item(wxMenu &menu, const std::string &element_id, wxWindow *anchor, const wxString &label)
{
    MenuBinding &binding = menu_bindings()[&menu];
    binding.element_id  = element_id;
    binding.anchor      = anchor;

    wxMenuItem *existing = menu.FindItem(kMenuItemId);
    if (existing)
        return existing;

    if (menu.GetMenuItemCount() > 0)
        menu.AppendSeparator();
    const wxString text = (label.IsEmpty() ? _L("Edit appearance...") : label) + "\t" + shortcut_text();
    wxMenuItem *item = menu.Append(kMenuItemId, text, _L("Open the appearance editor beside this element"));
    menu.Bind(wxEVT_MENU, [menu_ptr = &menu](wxCommandEvent &) {
        auto it = menu_bindings().find(menu_ptr);
        if (it == menu_bindings().end())
            return;
        const std::string id = it->second.element_id;
        wxWindow *a = it->second.anchor.get();
        if (!a)
            a = menu_ptr->GetInvokingWindow();
        if (!a)
            a = wxWindow::FindFocus();
        if (!a || id.empty())
            return;
        // The Material menu delivers wxEVT_MENU before it has fully closed;
        // open on the next loop turn so focus return and the new card do not
        // fight over activation.
        a->CallAfter([a, id]() { AppearanceEditorPopover::open_for(a, id); });
    }, kMenuItemId);
    return item;
}

void remove_edit_appearance_item(wxMenu &menu)
{
    wxMenuItem *item = menu.FindItem(kMenuItemId);
    if (!item)
        return;
    const size_t count = menu.GetMenuItemCount();
    // Drop the separator this helper added in front of the item.
    if (count >= 2) {
        wxMenuItem *prev = menu.FindItemByPosition(count - 2);
        if (prev && prev->IsSeparator() && menu.FindItemByPosition(count - 1) == item)
            menu.Destroy(prev);
    }
    menu.Destroy(item);
    menu_bindings().erase(&menu);
}

void attach(wxWindow *window, const std::string &element_id)
{
    if (!window || element_id.empty())
        return;
    // Right-click (or the keyboard Menu key / Shift+F10) that the widget's own
    // handlers left unhandled: a Material context menu carrying the item.
    // Shift held: skip the menu and open the editor directly.
    window->Bind(wxEVT_CONTEXT_MENU, [window](wxContextMenuEvent &e) {
        const std::string id = ElementStyle::element_id_of(window);
        if (id.empty()) {
            e.Skip();
            return;
        }
        if (wxGetKeyState(WXK_SHIFT)) {
            AppearanceEditorPopover::open_for(window, id);
            return;
        }
        wxMenu menu;
        append_edit_appearance_item(menu, id, window);
        wxPoint at = e.GetPosition();
        if (at == wxDefaultPosition)
            at = window->ClientToScreen(wxPoint(0, window->GetSize().y));
        MD3::PopupMenu(window, &menu, at);
        remove_edit_appearance_item(menu);
    });
    window->Bind(wxEVT_CHAR_HOOK, [window](wxKeyEvent &e) {
        if (e.ControlDown() && e.ShiftDown() && !e.AltDown() && e.GetKeyCode() == 'E') {
            const std::string id = ElementStyle::element_id_of(window);
            if (!id.empty()) {
                AppearanceEditorPopover::open_for(window, id);
                return;
            }
        }
        e.Skip();
    });
}

} // namespace AppearanceEditor

}} // namespace Slic3r::GUI
