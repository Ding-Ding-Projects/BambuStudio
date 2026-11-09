#include "ExtensionsPanel.hpp"
#include "ExtensionRuntime.hpp"

#include "../DocsBrowserDialog.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../GUI_Utils.hpp"
#include "../I18N.hpp"
#include "../MainFrame.hpp"
#include "../MsgDialog.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3DataView.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/MD3Menu.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/StaticBox.hpp"
#include "../Widgets/SwitchButton.hpp"
#include "../Widgets/TextArea.hpp"
#include "../Widgets/TextInput.hpp"
#include "../Widgets/TextTabbar.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Utils.hpp"

#include <wx/datetime.h>
#include <wx/dcclient.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/menu.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>

namespace Slic3r { namespace GUI { namespace Extensions {

namespace fs = std::filesystem;

namespace {

enum Glyphs : std::uint32_t {
    GlyphExtension = 0xE87B, GlyphAddFolder = 0xE2CC, GlyphFolder = 0xE2C8, GlyphRefresh = 0xE5D5, GlyphGuide = 0xEA19,
    GlyphChecklist = 0xE6B1, GlyphSchedule = 0xE8B5, GlyphSummarize = 0xF071, GlyphShield = 0xE9E0, GlyphError = 0xE000,
    GlyphCheck = 0xE86C, GlyphPlay = 0xE037, GlyphStop = 0xE047, GlyphDelete = 0xE872, GlyphSample = 0xEA4B,
    GlyphCode = 0xE86F, GlyphSave = 0xE161, GlyphInfo = 0xE88E, GlyphBlock = 0xE14B, GlyphLock = 0xE897,
    GlyphSwap = 0xE8D4, GlyphHistory = 0xE889, GlyphBolt = 0xEA0B, GlyphTerminal = 0xEB8E, GlyphWarning = 0xE002,
    GlyphChart = 0xE26B, GlyphPrint = 0xE8AD, GlyphNote = 0xE24D, GlyphDescription = 0xE873, GlyphVisibility = 0xE8F4,
    GlyphUndo = 0xE166
};

constexpr const char *kGuide = "docs/features/windows/extensions.md";

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

// " · " built from its code point: a narrow literal would be read in the ANSI code page.
wxString dot() { return wxString(" ") + wxUniChar(0x00B7) + " "; }

wxWindow *glyph_window(wxWindow *parent, std::uint32_t glyph, MD3::Role colour, int size_dip = 20)
{
    auto *w = new wxWindow(parent, wxID_ANY, wxDefaultPosition, parent->FromDIP(wxSize(size_dip + 4, size_dip + 4)));
    w->SetBackgroundColour(parent->GetBackgroundColour());
    w->Bind(wxEVT_PAINT, [w, glyph, colour, size_dip](wxPaintEvent &) {
        wxPaintDC dc(w);
        if (!MaterialIcon::available()) return;
        const int    px = w->FromDIP(size_dip);
        const wxSize gs = MaterialIcon::measure(dc, glyph, px);
        MaterialIcon::draw(dc, glyph, px, role(colour), wxPoint((w->GetSize().x - gs.x) / 2, (w->GetSize().y - gs.y) / 2));
    });
    return w;
}

StaticBox *card(wxWindow *parent, MD3::Role background = MD3::Role::SurfaceContainer)
{
    auto *box = new StaticBox(parent);
    box->SetCornerRadius(parent->FromDIP(16));
    box->SetBorderWidth(0);
    box->SetBackgroundColor(role(background));
    box->SetBackgroundColour(role(background));
    return box;
}

Label *label(wxWindow *parent, const wxFont &font, const wxString &text, bool muted = false, bool wrap = false)
{
    auto *l = new Label(parent, font, text, wrap ? (LB_AUTO_WRAP | wxST_NO_AUTORESIZE) : 0);
    if (wrap) l->SetMinSize(wxSize(parent->FromDIP(120), -1));
    l->SetBackgroundColour(parent->GetBackgroundColour());
    l->SetForegroundColour(role(muted ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface));
    return l;
}

Button *button(wxWindow *parent, const wxString &text, std::uint32_t glyph, Button::Variant variant = Button::Variant::Outlined)
{
    auto *b = new Button(parent, text);
    b->SetVariant(variant);
    b->SetButtonSize(Button::Size::Small);
    if (glyph) b->SetGlyph(glyph);
    return b;
}

std::uint32_t glyph_for(const Manifest &m)
{
    if (m.hooks.empty()) return GlyphExtension;
    switch (m.hooks.front()) {
    case Hook::PlateSliced: return GlyphSummarize;
    case Hook::PrintBeforeSend: return GlyphChecklist;
    case Hook::GcodeExported: return GlyphSchedule;
    case Hook::PrintFinished: return GlyphPrint;
    case Hook::ModelImported: return GlyphDescription;
    case Hook::Command: return GlyphBolt;
    }
    return GlyphExtension;
}

std::uint32_t glyph_for(Permission p)
{
    switch (p) {
    case Permission::SliceRead: return GlyphChart;
    case Permission::ModelRead: return GlyphDescription;
    case Permission::PrinterRead: return GlyphPrint;
    case Permission::NotesShow: return GlyphNote;
    case Permission::SendWarn: return GlyphWarning;
    case Permission::SendBlock: return GlyphBlock;
    case Permission::GcodeComment: return GlyphCode;
    case Permission::FileSave: return GlyphSave;
    }
    return GlyphShield;
}

wxString hook_title(Hook hook)
{
    const HookInfo &h = info(hook);
    wxString        t = _(h.title);
    if (!h.built) t << " (" << _L("not run by this build yet") << ")";
    return t;
}

wxString when_text(std::int64_t time)
{
    if (time <= 0) return _L("Never");
    const wxDateTime when(static_cast<time_t>(time));
    const wxDateTime now = wxDateTime::Now();
    if (when.FormatISODate() == now.FormatISODate()) return when.Format("%H:%M:%S");
    if (when.FormatISODate() == (now - wxDateSpan::Day()).FormatISODate()) return _L("Yesterday") + " " + when.Format("%H:%M:%S");
    return when.Format("%Y-%m-%d %H:%M");
}

wxString bytes_text(std::size_t bytes)
{
    if (bytes < 1024) return wxString::Format(_L("%d B"), int(bytes));
    if (bytes < 1024 * 1024) return wxString::Format(_L("%.1f KB"), bytes / 1024.0);
    return wxString::Format(_L("%.1f MB"), bytes / (1024.0 * 1024.0));
}

wxString hook_text(const RunRecord &r, const Manifest *m)
{
    if (r.hook.rfind("command:", 0) == 0) {
        const std::string action = r.hook.substr(8);
        if (m)
            for (const Action &a : m->actions)
                if (a.id == action) return wxString::Format(_L("Command: %s"), wxString::FromUTF8(a.title));
        return wxString::Format(_L("Command: %s"), wxString::FromUTF8(action));
    }
    if (const HookInfo *h = find_hook(r.hook)) return _(h->title);
    return wxString::FromUTF8(r.hook);
}

wxString manifest_error_text(const ManifestError &e)
{
    const wxString field  = wxString::FromUTF8(e.field);
    const wxString detail = wxString::FromUTF8(e.detail);
    if (e.code == "not-json") return _L("manifest.json is not valid JSON");
    if (e.code == "too-large") return _L("manifest.json is larger than 64 KB");
    if (e.code == "missing") return field.empty() ? _L("There is no manifest.json in this folder") : wxString::Format(_L("\"%s\" is missing"), field);
    if (e.code == "wrong-type") return wxString::Format(_L("\"%s\" has the wrong type"), field);
    if (e.code == "bad-id") return _L("The id must be lower-case letters, digits, dots and dashes, like com.example.name");
    if (e.code == "bad-version") return _L("The version must be MAJOR.MINOR.PATCH, like 1.0.0");
    if (e.code == "api-newer")
        return wxString::Format(_L("apiVersion %s is newer than this app supports (%d)"), detail, kApiVersion);
    if (e.code == "api-older") return wxString::Format(_L("apiVersion %s is older than this app supports (%d)"), detail, kApiVersion);
    if (e.code == "unknown-hook") return wxString::Format(_L("Unknown hook \"%s\""), detail);
    if (e.code == "unknown-permission") return wxString::Format(_L("Unknown permission \"%s\""), detail);
    if (e.code == "unknown-field") return wxString::Format(_L("Unknown field \"%s\""), field.empty() ? detail : field);
    if (e.code == "too-long") return wxString::Format(_L("\"%s\" is too long"), field);
    if (e.code == "empty-run") return _L("\"run.windows\" is empty");
    if (e.code == "duplicate") return wxString::Format(_L("Another folder already has the id %s"), detail);
    return wxString::FromUTF8(e.code);
}

std::string read_text(const fs::path &path, std::size_t limit)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::string text(limit, '\0');
    in.read(&text[0], std::streamsize(limit));
    text.resize(std::size_t(in.gcount()));
    return text;
}

wxString display_name(const Installed &x)
{
    if (!x.manifest.name.empty()) return wxString::FromUTF8(x.manifest.name);
    return wxString::FromUTF8(x.folder.filename().u8string());
}

// ---------------------------------------------------------------- the two prompts

class PromptDialog : public DPIDialog
{
public:
    PromptDialog(wxWindow *parent, const wxString &title) : DPIDialog(parent, wxID_ANY, title, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE)
    {
        SetBackgroundColour(role(MD3::Role::Surface));
        m_root = new wxBoxSizer(wxVERTICAL);
        m_root->Add(new MD3DialogCaption(this, title), 0, wxEXPAND);
        m_body = new wxBoxSizer(wxVERTICAL);
        m_root->Add(m_body, 1, wxEXPAND | wxALL, FromDIP(20));
        m_buttons = new wxBoxSizer(wxHORIZONTAL);
        m_buttons->AddStretchSpacer(1);
        m_root->Add(m_buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
            if (e.GetKeyCode() == WXK_ESCAPE) EndModal(wxID_CANCEL);
            else e.Skip();
        });
    }
    wxBoxSizer *body() { return m_body; }
    void        add_button(const wxString &text, int id, bool primary)
    {
        auto *b = button(this, text, 0, primary ? Button::Variant::Filled : Button::Variant::Outlined);
        b->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) { EndModal(id); });
        m_buttons->Add(b, 0, wxLEFT, FromDIP(8));
        if (primary) b->SetFocus();
    }
    int run()
    {
        SetSizer(m_root);
        SetMinSize(wxSize(FromDIP(460), -1));
        SetMaxSize(wxSize(FromDIP(560), -1));
        Fit();
        MD3DialogCaption::FinishChrome(this);
        CenterOnParent();
        wxGetApp().UpdateDlgDarkUI(this);
        return ShowModal();
    }

protected:
    void on_dpi_changed(const wxRect &) override { Layout(); }

private:
    wxBoxSizer *m_root    = nullptr;
    wxBoxSizer *m_body    = nullptr;
    wxBoxSizer *m_buttons = nullptr;
};

} // namespace

wxString state_text(State state)
{
    switch (state) {
    case State::On: return _L("On");
    case State::OffByUser: return _L("Off by you");
    case State::WaitingNew: return _L("Waiting for you");
    case State::WaitingChanged: return _L("Changed, waiting for you");
    case State::OffAfterFailures: return _L("Off after failed runs");
    case State::ManifestError: return _L("Manifest error");
    case State::AllOff: return _L("All extensions off");
    }
    return wxString();
}

ConsentAnswer ask_consent(wxWindow *parent, const Installed &x, const Consent *previous)
{
    const Manifest &m     = x.manifest;
    const wxString  title = wxString::Format(_L("Allow %s %s?"), wxString::FromUTF8(m.name), wxString::FromUTF8(m.version));
    PromptDialog    dialog(parent, title);
    wxWindow       *page  = &dialog;
    wxBoxSizer     *s     = dialog.body();

    auto *head = new wxBoxSizer(wxHORIZONTAL);
    head->Add(glyph_window(page, GlyphShield, MD3::Role::Primary, 28), 0, wxALIGN_TOP | wxRIGHT, page->FromDIP(10));
    auto *lead = new wxBoxSizer(wxVERTICAL);
    lead->Add(label(page, Label::Head_16, title), 0, wxBOTTOM, page->FromDIP(4));
    const bool answered_before = previous && !previous->version.empty();
    const bool new_version     = answered_before && previous->version != m.version;
    const bool asks_more       = answered_before && !added_permissions(m, *previous).empty();
    wxString   why;
    if (!answered_before) why = _L("It stays off until you allow exactly this list. You can turn it off again at any time.");
    else if (asks_more) why = _L("This version asks for something new. It stays off until you allow exactly this list.");
    else if (new_version) why = _L("This is a new version. It stays off until you allow it again.");
    else why = _L("You kept it off before. It stays off until you allow exactly this list.");
    lead->Add(label(page, Label::Body_13, why, true, true), 0, wxEXPAND);
    const bool changed = answered_before && !previous->allowed.empty();
    head->Add(lead, 1, wxEXPAND);
    s->Add(head, 0, wxEXPAND | wxBOTTOM, page->FromDIP(14));

    StaticBox *list = card(page, MD3::Role::SurfaceContainerLow);
    auto      *ls   = new wxBoxSizer(wxVERTICAL);
    if (m.permissions.empty())
        ls->Add(label(list, Label::Body_13, _L("It asks for nothing: it only runs when you start one of its commands, and its answers are ignored."), false, true),
                0, wxEXPAND | wxALL, page->FromDIP(12));
    for (Permission p : m.permissions) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(glyph_window(list, glyph_for(p), MD3::Role::OnSurfaceVariant), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, page->FromDIP(8));
        row->Add(label(list, Label::Body_13, _(info(p).title), false, true), 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, page->FromDIP(8));
        wxString tag;
        if (changed) tag = previous && allowed(*previous, p) ? _L("Allowed before") : _L("New");
        if (!tag.IsEmpty()) {
            auto *t = label(list, Label::Head_12, tag);
            t->SetForegroundColour(role(tag == _L("New") ? MD3::Role::Primary : MD3::Role::OnSurfaceVariant));
            row->Add(t, 0, wxALIGN_CENTER_VERTICAL);
        }
        ls->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, page->FromDIP(10));
    }
    ls->AddSpacer(page->FromDIP(10));
    list->SetSizer(ls);
    s->Add(list, 0, wxEXPAND | wxBOTTOM, page->FromDIP(12));

    wxString hooks;
    for (Hook h : m.hooks) {
        if (!hooks.IsEmpty()) hooks << ", ";
        hooks << hook_title(h);
    }
    if (!hooks.IsEmpty())
        s->Add(label(page, Label::Body_12, wxString::Format(_L("It runs: %s."), hooks), true, true), 0, wxEXPAND | wxBOTTOM, page->FromDIP(8));
    s->Add(label(page, Label::Body_12,
                 _L("It never receives passwords, access codes or account tokens. An extension is still a program running as you, so Bambu Studio cannot stop it from reading your own files."),
                 true, true),
           0, wxEXPAND);

    dialog.add_button(_L("Keep off"), wxID_NO, false);
    dialog.add_button(_L("Allow exactly this"), wxID_YES, true);
    const int answer = dialog.run();
    return answer == wxID_YES ? ConsentAnswer::Allow : answer == wxID_NO ? ConsentAnswer::KeepOff : ConsentAnswer::Undecided;
}

bool ask_replace(wxWindow *parent, const Manifest &installed, const Manifest &incoming, Host::Inspection kind)
{
    const wxString name  = wxString::FromUTF8(installed.name);
    const wxString title = kind == Host::Inspection::SameVersion
                               ? wxString::Format(_L("Replace the files of %s %s?"), name, wxString::FromUTF8(installed.version))
                               : wxString::Format(_L("Replace %s %s with %s?"), name, wxString::FromUTF8(installed.version),
                                                  wxString::FromUTF8(incoming.version));
    PromptDialog dialog(parent, title);
    wxWindow    *page = &dialog;
    wxBoxSizer  *s    = dialog.body();
    auto        *head = new wxBoxSizer(wxHORIZONTAL);
    head->Add(glyph_window(page, GlyphSwap, MD3::Role::Primary, 28), 0, wxALIGN_TOP | wxRIGHT, page->FromDIP(10));
    auto *lead = new wxBoxSizer(wxVERTICAL);
    lead->Add(label(page, Label::Head_16, title), 0, wxBOTTOM, page->FromDIP(4));
    wxString why = kind == Host::Inspection::OlderVersion
                       ? _L("The folder you picked holds an older version of an extension you already have.")
                       : kind == Host::Inspection::SameVersion ? _L("The folder you picked holds the version you already have, perhaps with changed files.")
                                                               : _L("The folder you picked holds a newer version of an extension you already have.");
    why << " " << _L("Its runs stay; it asks for your permission again if its list changed.");
    lead->Add(label(page, Label::Body_13, why, true, true), 0, wxEXPAND);
    head->Add(lead, 1, wxEXPAND);
    s->Add(head, 0, wxEXPAND | wxBOTTOM, page->FromDIP(14));

    auto *cards = new wxBoxSizer(wxHORIZONTAL);
    const auto add_card = [&](const wxString &caption, const Manifest &m, bool highlight) {
        StaticBox *c  = card(page, highlight ? MD3::Role::SecondaryContainer : MD3::Role::SurfaceContainerLow);
        auto      *cs = new wxBoxSizer(wxVERTICAL);
        cs->Add(label(c, Label::Body_12, caption, true), 0, wxBOTTOM, page->FromDIP(4));
        cs->Add(label(c, Label::Head_14,
                      wxString::Format(_L("%s · %d permissions"), wxString::FromUTF8(m.version), int(m.permissions.size()))),
                0);
        auto *pad = new wxBoxSizer(wxVERTICAL);
        pad->Add(cs, 1, wxEXPAND | wxALL, page->FromDIP(12));
        c->SetSizer(pad);
        cards->Add(c, 1, wxEXPAND | wxRIGHT, page->FromDIP(8));
    };
    add_card(_L("Installed"), installed, false);
    add_card(_L("From the folder"), incoming, true);
    s->Add(cards, 0, wxEXPAND);

    dialog.add_button(wxString::Format(_L("Keep %s"), wxString::FromUTF8(installed.version)), wxID_NO, false);
    dialog.add_button(_L("Replace"), wxID_YES, true);
    return dialog.run() == wxID_YES;
}

// ---------------------------------------------------------------- the panel

ExtensionsPanel::ExtensionsPanel(wxWindow *parent) : wxPanel(parent, wxID_ANY)
{
    SetName("extensions");
    SetBackgroundColour(role(MD3::Role::Surface));
    const int pad = FromDIP(MD3::Metrics::active().padding);
    Runtime::instance().host();

    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *top  = new wxBoxSizer(wxHORIZONTAL);
    auto *titles = new wxBoxSizer(wxVERTICAL);
    titles->Add(make_label(this, Label::Head_20, _L("Extensions")), 0);
    titles->Add(make_label(this, Label::Body_13,
                           _L("Add steps after slicing, comment lines in exported G-code and commands of your own. Each extension is a folder with a "
                              "manifest and a program in any language, run one request at a time outside Bambu Studio."),
                           true, true),
                0, wxEXPAND | wxTOP, FromDIP(4));
    top->Add(titles, 1, wxEXPAND | wxRIGHT, pad);
    m_master_label = make_label(this, Label::Body_13, _L("Extensions on"));
    top->Add(m_master_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_master = new SwitchButton(this);
    m_master->SetName(_L("Extensions on"));
    m_master->SetToolTip(_L("Off: no extension runs, whatever its own switch says"));
    m_master->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &) { set_master(m_master->GetValue()); });
    top->Add(m_master, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(top, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *bar = new wxBoxSizer(wxHORIZONTAL);
    auto *add = make_button(this, _L("Add from folder"), GlyphAddFolder, true);
    add->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { add_from_folder(); });
    auto *open = make_button(this, _L("Open folder"), GlyphFolder);
    open->Bind(wxEVT_BUTTON, [](wxCommandEvent &) {
        std::error_code ec;
        fs::create_directories(Runtime::instance().folder(), ec);
        desktop_open_any_folder(Runtime::instance().folder().u8string());
    });
    auto *reload = make_button(this, _L("Reload"), GlyphRefresh);
    reload->SetToolTip(_L("Read every extension folder again"));
    reload->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { Runtime::instance().reload(); });
    auto *guide = make_button(this, _L("Author guide"), GlyphGuide);
    guide->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { DocsBrowserDialog::ShowArticle(wxGetApp().mainframe, kGuide); });
    for (Button *b : {add, open, reload, guide}) bar->Add(b, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    bar->AddStretchSpacer(1);
    m_activity = make_label(this, Label::Body_12, wxString(), true);
    m_activity->SetName("extensions-activity");
    bar->Add(m_activity, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_stop = make_button(this, _L("Stop"), GlyphStop);
    m_stop->SetToolTip(_L("Stop the run in progress and drop the queued ones"));
    m_stop->Bind(wxEVT_BUTTON, [](wxCommandEvent &) { Runtime::instance().stop_all(); });
    bar->Add(m_stop, 0, wxALIGN_CENTER_VERTICAL);
    root->Add(bar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);

    auto *columns = new wxBoxSizer(wxHORIZONTAL);
    columns->Add(build_list_side(this), 0, wxEXPAND | wxRIGHT, pad);
    columns->Add(build_detail_side(this), 1, wxEXPAND);
    root->Add(columns, 1, wxEXPAND | wxALL, pad);
    SetSizer(root);

    m_token = Runtime::instance().subscribe([this]() { CallAfter([this]() { refresh_all(); }); });
    refresh_all();
}

ExtensionsPanel::~ExtensionsPanel() { Runtime::instance().unsubscribe(m_token); }

Label *ExtensionsPanel::make_label(wxWindow *parent, const wxFont &font, const wxString &text, bool muted, bool wrap)
{
    return label(parent, font, text, muted, wrap);
}

Button *ExtensionsPanel::make_button(wxWindow *parent, const wxString &text, std::uint32_t glyph, bool tonal)
{
    return button(parent, text, glyph, tonal ? Button::Variant::Tonal : Button::Variant::Outlined);
}

StaticBox *ExtensionsPanel::make_card(wxWindow *parent, int background_role) { return card(parent, MD3::Role(background_role)); }

wxWindow *ExtensionsPanel::build_list_side(wxWindow *parent)
{
    auto *side = new MD3ScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(FromDIP(400), -1), wxVSCROLL | wxBORDER_NONE);
    side->SetBackgroundColour(GetBackgroundColour());
    side->SetScrollRate(0, FromDIP(24));
    side->SetMinSize(wxSize(FromDIP(400), -1));
    auto     *s   = new wxBoxSizer(wxVERTICAL);
    const int gap = FromDIP(10);

    m_search = new SearchField(side, _L("Search name, hook, action or permission"));
    m_search->SetOnQuery([this](const wxString &) { refresh_list(); });
    m_search->SetOnRegexToggle([this](bool) { refresh_list(); });
    s->Add(m_search, 0, wxEXPAND | wxBOTTOM, gap);

    auto *chips = new wxWrapSizer(wxHORIZONTAL);
    for (int f = 0; f < FilterCount; ++f) {
        auto *chip = button(side, wxString(), 0, Button::Variant::Outlined);
        chip->Bind(wxEVT_BUTTON, [this, f](wxCommandEvent &) { set_filter(f); });
        m_filter_buttons.emplace_back(f, chip);
        chips->Add(chip, 0, wxRIGHT | wxBOTTOM, FromDIP(4));
    }
    s->Add(chips, 0, wxEXPAND | wxBOTTOM, FromDIP(6));

    m_list = new MD3DataViewListCtrl(side, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(260)), wxDV_ROW_LINES | wxDV_SINGLE);
    m_list->AppendTextColumn(_L("Extension"), wxDATAVIEW_CELL_INERT, FromDIP(170), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(_L("State"), wxDATAVIEW_CELL_INERT, FromDIP(120), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->AppendTextColumn(_L("Last run"), wxDATAVIEW_CELL_INERT, FromDIP(90), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_list->SetName(_L("Extensions"));
    m_list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &) {
        if (m_refreshing) return;
        const int row = m_list->GetSelectedRow();
        if (row >= 0 && row < int(m_rows.size())) m_selected = m_rows[std::size_t(row)];
        refresh_detail();
    });
    s->Add(m_list, 1, wxEXPAND | wxBOTTOM, gap);

    m_folder = make_label(side, Label::Body_12, wxString(), true, true);
    s->Add(m_folder, 0, wxEXPAND | wxBOTTOM, gap);

    StaticBox *limits = card(side, MD3::Role::SurfaceContainerLow);
    auto      *ls     = new wxBoxSizer(wxVERTICAL);
    ls->Add(label(limits, Label::Head_14, _L("Limits for every run")), 0, wxBOTTOM, FromDIP(8));
    const auto spin_row = [&](const wxString &caption, const wxString &unit, int min, int max, SpinInput *&out, const wxString &tip) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(label(limits, Label::Body_13, caption), 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        out = new SpinInput(limits, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(96), -1), 0, min, max, min);
        out->SetName(caption);
        out->SetToolTip(tip);
        out->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &) { save_limits(); });
        row->Add(out, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
        row->Add(label(limits, Label::Body_12, unit, true), 0, wxALIGN_CENTER_VERTICAL);
        row->SetMinSize(wxSize(-1, FromDIP(36)));
        ls->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(4));
    };
    spin_row(_L("Time limit"), _L("s"), 1, 60, m_timeout, _L("A run that takes longer is stopped, 1 to 60 seconds"));
    spin_row(_L("Largest answer"), _L("KB"), 1, 1024, m_max_answer, _L("A longer answer is thrown away, 1 to 1024 KB"));
    spin_row(_L("Turn off after"), _L("failed runs in a row"), 0, 20, m_off_after, _L("0 never turns an extension off"));
    auto *lp = new wxBoxSizer(wxVERTICAL);
    lp->Add(ls, 1, wxEXPAND | wxALL, FromDIP(14));
    limits->SetSizer(lp);
    s->Add(limits, 0, wxEXPAND | wxBOTTOM, gap);

    StaticBox *week = card(side, MD3::Role::SurfaceContainerLow);
    auto      *ws   = new wxBoxSizer(wxVERTICAL);
    auto      *wh   = new wxBoxSizer(wxHORIZONTAL);
    wh->Add(glyph_window(week, GlyphHistory, MD3::Role::Primary), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    wh->Add(label(week, Label::Head_14, _L("The last 7 days")), 1, wxALIGN_CENTER_VERTICAL);
    ws->Add(wh, 0, wxEXPAND | wxBOTTOM, FromDIP(6));
    m_week = label(week, Label::Body_13, wxString(), true, true);
    ws->Add(m_week, 0, wxEXPAND);
    auto *wp = new wxBoxSizer(wxVERTICAL);
    wp->Add(ws, 1, wxEXPAND | wxALL, FromDIP(14));
    week->SetSizer(wp);
    s->Add(week, 0, wxEXPAND);

    side->SetSizer(s);
    side->FitInside();
    return side;
}

wxWindow *ExtensionsPanel::build_detail_side(wxWindow *parent)
{
    StaticBox *detail = card(parent, MD3::Role::SurfaceContainerLowest);
    m_detail          = detail;
    auto *outer       = new wxBoxSizer(wxVERTICAL);

    m_detail_empty = new wxPanel(detail);
    m_detail_empty->SetBackgroundColour(detail->GetBackgroundColour());
    auto *es = new wxBoxSizer(wxVERTICAL);
    es->AddStretchSpacer(1);
    es->Add(glyph_window(m_detail_empty, GlyphExtension, MD3::Role::OnSurfaceVariant, 40), 0, wxALIGN_CENTER_HORIZONTAL | wxBOTTOM, FromDIP(8));
    es->Add(label(m_detail_empty, Label::Body_14, _L("Pick an extension to see what it asks for and what it did."), true), 0,
            wxALIGN_CENTER_HORIZONTAL);
    es->AddStretchSpacer(1);
    m_detail_empty->SetSizer(es);
    outer->Add(m_detail_empty, 1, wxEXPAND);

    m_detail_body = new wxPanel(detail);
    m_detail_body->SetBackgroundColour(detail->GetBackgroundColour());
    wxWindow *body = m_detail_body;
    auto     *s    = new wxBoxSizer(wxVERTICAL);
    auto     *head = new wxBoxSizer(wxHORIZONTAL);
    auto     *names = new wxBoxSizer(wxVERTICAL);
    m_name = label(body, Label::Head_20, wxString());
    names->Add(m_name, 0);
    m_description = label(body, Label::Body_13, wxString(), true, true);
    names->Add(m_description, 0, wxEXPAND | wxTOP, FromDIP(4));
    m_id = label(body, Label::Body_12, wxString(), true);
    names->Add(m_id, 0, wxTOP, FromDIP(2));
    head->Add(names, 1, wxEXPAND | wxRIGHT, FromDIP(12));
    auto *switches = new wxBoxSizer(wxVERTICAL);
    m_state = label(body, Label::Head_13, wxString());
    switches->Add(m_state, 0, wxALIGN_RIGHT | wxBOTTOM, FromDIP(6));
    m_on = new SwitchButton(body);
    m_on->SetName(_L("Extension on"));
    m_on->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &) { toggle_selected(m_on->GetValue()); });
    switches->Add(m_on, 0, wxALIGN_RIGHT);
    head->Add(switches, 0);
    s->Add(head, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

    m_banner      = card(body, MD3::Role::SecondaryContainer);
    auto *bs      = new wxBoxSizer(wxHORIZONTAL);
    bs->Add(glyph_window(m_banner, GlyphShield, MD3::Role::OnSecondaryContainer), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_banner_text = label(m_banner, Label::Body_13, wxString(), false, true);
    m_banner_text->SetForegroundColour(role(MD3::Role::OnSecondaryContainer));
    bs->Add(m_banner_text, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_review = button(m_banner, _L("Review and allow"), GlyphShield, Button::Variant::Filled);
    m_review->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { review_selected(); });
    bs->Add(m_review, 0, wxALIGN_CENTER_VERTICAL);
    auto *bp = new wxBoxSizer(wxVERTICAL);
    bp->Add(bs, 1, wxEXPAND | wxALL, FromDIP(12));
    m_banner->SetSizer(bp);
    s->Add(m_banner, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

    // Wraps instead of cutting the last button when the detail pane is narrow.
    auto *actions = new wxWrapSizer(wxHORIZONTAL);
    m_sample = make_button(body, _L("Try with a sample"), GlyphSample, true);
    m_sample->SetToolTip(_L("Run it once with a request made of invented values marked as samples"));
    m_sample->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { try_sample(); });
    m_open_folder = make_button(body, _L("Open its folder"), GlyphFolder);
    m_open_folder->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { open_selected_folder(); });
    m_remove = make_button(body, _L("Remove"), GlyphDelete);
    m_remove->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { remove_selected(); });
    actions->Add(m_sample, 0, wxRIGHT | wxBOTTOM, FromDIP(6));
    actions->Add(m_open_folder, 0, wxRIGHT | wxBOTTOM, FromDIP(6));
    actions->Add(m_remove, 0, wxBOTTOM, FromDIP(6));
    s->Add(actions, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

    // A tighter gap keeps all five tabs inside the narrow detail pane.
    m_tabs = new TextTabbar(body, TextTabbar::Align::Left, 12);
    m_tabs->Bind(wxEVT_CHOICE, [this](wxCommandEvent &e) { select_tab(e.GetInt()); });
    s->Add(m_tabs, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    m_book = new wxSimplebook(body, wxID_ANY);
    m_book->SetBackgroundColour(body->GetBackgroundColour());

    const auto scrolled_page = [this]() {
        auto *page = new MD3ScrolledWindow(m_book, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
        page->SetBackgroundColour(m_book->GetBackgroundColour());
        page->SetScrollRate(0, FromDIP(24));
        page->SetSizer(new wxBoxSizer(wxVERTICAL));
        return page;
    };
    m_overview    = scrolled_page();
    m_permissions = scrolled_page();
    m_settings    = scrolled_page();

    m_runs_page = new wxPanel(m_book);
    m_runs_page->SetBackgroundColour(m_book->GetBackgroundColour());
    auto *rs   = new wxBoxSizer(wxVERTICAL);
    m_runs     = new MD3DataViewListCtrl(m_runs_page, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(220)), wxDV_ROW_LINES | wxDV_SINGLE);
    m_runs->AppendTextColumn(_L("When"), wxDATAVIEW_CELL_INERT, FromDIP(150), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_runs->AppendTextColumn(_L("Hook or command"), wxDATAVIEW_CELL_INERT, FromDIP(220), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_runs->AppendTextColumn(_L("Time"), wxDATAVIEW_CELL_INERT, FromDIP(70), wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
    m_runs->AppendTextColumn(_L("Answer"), wxDATAVIEW_CELL_INERT, FromDIP(80), wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
    m_runs->AppendTextColumn(_L("Result"), wxDATAVIEW_CELL_INERT, FromDIP(240), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_runs->SetName(_L("Runs"));
    rs->Add(m_runs, 1, wxEXPAND | wxBOTTOM, FromDIP(8));
    auto *rb = new wxBoxSizer(wxHORIZONTAL);
    m_runs_note = label(m_runs_page, Label::Body_12, wxString(), true, true);
    rb->Add(m_runs_note, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    auto *export_button = make_button(m_runs_page, _L("Export runs"), GlyphSave);
    export_button->SetToolTip(_L("Save every run of every extension as JSON: times, hooks, sizes and results, never what was sent"));
    export_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { export_runs(); });
    rb->Add(export_button, 0, wxALIGN_CENTER_VERTICAL);
    rs->Add(rb, 0, wxEXPAND);
    m_runs_page->SetSizer(rs);

    m_manifest_page = new wxPanel(m_book);
    m_manifest_page->SetBackgroundColour(m_book->GetBackgroundColour());
    auto *ms        = new wxBoxSizer(wxVERTICAL);
    m_manifest_state = label(m_manifest_page, Label::Head_13, wxString(), false, true);
    ms->Add(m_manifest_state, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    m_manifest = new TextArea(m_manifest_page, wxString(), wxSize(-1, FromDIP(240)), wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
    m_manifest->SetReadOnly(true);
    m_manifest->SetMonospace(true);
    m_manifest->SetName(_L("manifest.json"));
    ms->Add(m_manifest, 1, wxEXPAND | wxBOTTOM, FromDIP(8));
    ms->Add(label(m_manifest_page, Label::Body_12,
                  _L("One JSON request goes in on standard input, one JSON answer comes back on standard output. A broken extension cannot take Bambu Studio down."),
                  true, true),
            0, wxEXPAND);
    m_manifest_page->SetSizer(ms);

    m_book->AddPage(m_overview, "overview");
    m_book->AddPage(m_permissions, "permissions");
    m_book->AddPage(m_settings, "settings");
    m_book->AddPage(m_runs_page, "runs");
    m_book->AddPage(m_manifest_page, "manifest");
    s->Add(m_book, 1, wxEXPAND);
    m_detail_body->SetSizer(s);
    outer->Add(m_detail_body, 1, wxEXPAND | wxALL, FromDIP(18));
    detail->SetSizer(outer);
    return detail;
}

// ---------------------------------------------------------------- refreshing

void ExtensionsPanel::refresh_all()
{
    if (m_refreshing) return;
    Freeze();
    const bool master = Runtime::instance().host().master_on();
    m_master->SetValue(master);
    refresh_filters();
    refresh_list();
    refresh_limits();
    refresh_week();
    refresh_activity();
    refresh_detail();
    Layout();
    Thaw();
}

void ExtensionsPanel::refresh_activity()
{
    const Runtime::Activity a = Runtime::instance().activity();
    wxString                text;
    if (!a.running.empty()) {
        const Installed *x    = Runtime::instance().host().find(a.running);
        const wxString   name = x ? display_name(*x) : wxString::FromUTF8(a.running);
        RunRecord        r;
        r.hook = a.hook;
        text   = wxString::Format(_L("Running %s: %s"), name, hook_text(r, x ? &x->manifest : nullptr));
        if (a.queued > 0) text << dot() << wxString::Format(_L("%d waiting"), a.queued);
    }
    m_activity->SetLabel(text);
    m_activity->Show(!text.IsEmpty());
    m_stop->Show(!text.IsEmpty());
}

void ExtensionsPanel::refresh_filters()
{
    Host &h = Runtime::instance().host();
    int   counts[FilterCount] = {0};
    for (const Installed &x : h.extensions())
        for (int f = 0; f < FilterCount; ++f)
            if (in_filter(x, f)) ++counts[f];
    const wxString names[FilterCount] = {_L("All"), _L("On"), _L("Off"), _L("Waiting for you"), _L("Manifest errors"), _L("Built in")};
    const std::uint32_t glyphs[FilterCount] = {0, GlyphCheck, 0, GlyphShield, GlyphError, GlyphExtension};
    for (auto &[f, chip] : m_filter_buttons) {
        chip->SetLabel(wxString::Format("%s %d", names[f], counts[f]));
        if (glyphs[f]) chip->SetGlyph(glyphs[f]);
        chip->SetVariant(f == m_filter ? Button::Variant::Tonal : Button::Variant::Outlined);
        chip->SetName(names[f]);
        chip->Refresh();
    }
}

bool ExtensionsPanel::in_filter(const Installed &x, int filter) const
{
    const State s = Runtime::instance().host().state(x);
    switch (filter) {
    case FilterOn: return s == State::On;
    case FilterOff: return s == State::OffByUser || s == State::OffAfterFailures || s == State::AllOff;
    case FilterWaiting: return s == State::WaitingNew || s == State::WaitingChanged;
    case FilterErrors: return s == State::ManifestError;
    case FilterBuiltin: return x.builtin;
    default: return true;
    }
}

bool ExtensionsPanel::matches(const Installed &x) const
{
    const wxString query = m_search ? m_search->GetValue() : wxString();
    if (query.IsEmpty()) return true;
    wxString hay = display_name(x) + "\n" + wxString::FromUTF8(x.manifest.id) + "\n" + wxString::FromUTF8(x.manifest.description);
    for (Hook h : x.manifest.hooks) hay << "\n" << info(h).id << "\n" << _(info(h).title);
    for (const Action &a : x.manifest.actions) hay << "\n" << wxString::FromUTF8(a.title) << "\n" << wxString::FromUTF8(a.id);
    for (Permission p : x.manifest.permissions) hay << "\n" << info(p).id << "\n" << _(info(p).title);
    return SearchField::textMatches(query, hay, m_search->IsRegexEnabled(), m_search->IsCaseSensitive(), m_search->IsWholeWord(),
                                    m_search->IsMultiline());
}

wxString ExtensionsPanel::last_run_text(const std::string &id) const
{
    const auto runs = Runtime::instance().host().runs().for_extension(id);
    return runs.empty() ? _L("Never") : when_text(runs.back().time);
}

void ExtensionsPanel::refresh_list()
{
    m_refreshing = true;
    Host &h      = Runtime::instance().host();
    m_list->DeleteAllItems();
    m_rows.clear();
    int select_row = -1;
    for (const Installed &x : h.extensions()) {
        if (!in_filter(x, m_filter) || !matches(x)) continue;
        wxString name = display_name(x);
        if (!x.manifest.version.empty()) name << " " << wxString::FromUTF8(x.manifest.version);
        if (x.builtin) name << dot() << _L("built in");
        wxVector<wxVariant> row;
        row.push_back(wxVariant(name));
        const State state = h.state(x);
        wxString    state_label = state_text(state);
        if (state == State::ManifestError) state_label = manifest_error_text(x.error);
        row.push_back(wxVariant(state_label));
        row.push_back(wxVariant(x.manifest.id.empty() ? _L("Never") : last_run_text(x.manifest.id)));
        m_list->AppendItem(row);
        const std::string key = x.manifest.id.empty() ? x.folder.u8string() : x.manifest.id;
        if (key == m_selected) select_row = int(m_rows.size());
        m_rows.push_back(key);
    }
    if (select_row < 0 && !m_rows.empty() && m_selected.empty()) {
        select_row = 0;
        m_selected = m_rows.front();
    }
    if (select_row >= 0) {
        m_list->SelectRow(unsigned(select_row));
        m_list->EnsureVisible(m_list->RowToItem(select_row));
    }
    m_refreshing = false;

    std::size_t folders = h.extensions().size();
    m_folder->SetLabel(wxString::Format(_L("%s · %d folders"), wxString::FromUTF8(h.root().u8string()), int(folders)));
    if (m_rows.empty() && !h.extensions().empty())
        m_folder->SetLabel(m_folder->GetLabel() + "\n" + _L("Nothing matches the search and the filter."));
    // A wrapped label gets taller after new text; without a relayout its row
    // keeps the old height and the second line is cut.
    m_folder->InvalidateBestSize();
    if (auto *side = dynamic_cast<wxScrolledWindow *>(m_folder->GetParent())) {
        side->Layout();
        side->FitInside();
    }
}

void ExtensionsPanel::refresh_limits()
{
    const Limits l = Runtime::instance().host().limits();
    m_timeout->SetValue(l.timeout_ms / 1000);
    m_max_answer->SetValue(int(l.max_answer / 1024));
    m_off_after->SetValue(l.off_after_failures);
}

void ExtensionsPanel::refresh_week()
{
    const std::int64_t since = std::int64_t(std::time(nullptr)) - 7 * 24 * 3600;
    int                total = 0, by[8] = {0};
    for (const RunRecord &r : Runtime::instance().host().runs().records()) {
        if (r.time < since) continue;
        ++total;
        ++by[std::clamp(int(r.outcome), 0, 7)];
    }
    if (total == 0) {
        m_week->SetLabel(_L("No runs yet. Every run is listed under Runs with its hook, time, answer size and result."));
        return;
    }
    wxString parts;
    const auto add = [&parts](int n, const wxString &what) {
        if (n <= 0) return;
        if (!parts.IsEmpty()) parts << ", ";
        parts << wxString::Format("%d %s", n, what);
    };
    add(by[int(Outcome::Answered)], _L("answered"));
    add(by[int(Outcome::TimedOut)], _L("timed out"));
    add(by[int(Outcome::TooLarge)], _L("too large"));
    add(by[int(Outcome::NotJson)], _L("not valid JSON"));
    add(by[int(Outcome::Invalid)], _L("broke a rule"));
    add(by[int(Outcome::Failed)], _L("failed"));
    add(by[int(Outcome::NotStarted)], _L("not started"));
    add(by[int(Outcome::Stopped)], _L("stopped by you"));
    m_week->SetLabel(wxString::Format(_L("%d runs: %s. Every run is listed under Runs with its hook, time, answer size and result."), total, parts));
}

const Installed *ExtensionsPanel::selected() const
{
    if (m_selected.empty()) return nullptr;
    Host &h = Runtime::instance().host();
    if (const Installed *x = h.find(m_selected)) return x;
    for (const Installed &x : h.extensions())
        if (x.folder.u8string() == m_selected) return &x;
    return nullptr;
}

void ExtensionsPanel::refresh_tabs()
{
    const Installed *x    = selected();
    const auto       runs = x && !x->manifest.id.empty() ? Runtime::instance().host().runs().for_extension(x->manifest.id).size() : 0;
    m_tabs->ClearTabs();
    m_tabs->AddTab(_L("Overview"));
    m_tabs->AddTab(_L("Permissions"));
    const int settings = x ? int(x->manifest.settings.size()) : 0;
    m_tabs->AddTab(settings ? wxString::Format(_L("Settings %d"), settings) : _L("Settings"));
    m_tabs->AddTab(runs ? wxString::Format(_L("Runs %d"), int(runs)) : _L("Runs"));
    m_tabs->AddTab(_L("Manifest"));
    m_tabs->SetSelection(m_tab);
}

void ExtensionsPanel::select_tab(int tab)
{
    m_tab = std::clamp(tab, int(TabOverview), int(TabManifest));
    m_book->SetSelection(std::size_t(m_tab));
    m_tabs->SetSelection(m_tab);
}

void ExtensionsPanel::refresh_detail()
{
    const Installed *x = selected();
    m_detail_empty->Show(x == nullptr);
    m_detail_body->Show(x != nullptr);
    if (!x) {
        m_detail->Layout();
        return;
    }
    Host       &h     = Runtime::instance().host();
    const State state = h.state(*x);
    wxString    name  = display_name(*x);
    if (!x->manifest.version.empty()) name << " " << wxString::FromUTF8(x->manifest.version);
    m_name->SetLabel(name);
    wxString description = wxString::FromUTF8(x->manifest.description);
    if (x->builtin) {
        if (!description.IsEmpty()) description << " ";
        description << (x->modified ? _L("Built in, and changed by you: updates from Bambu Studio are paused so your changes stay.")
                                    : _L("Built in: Bambu Studio updates it while you have not changed its files."));
    }
    m_description->SetLabel(description);
    m_description->Show(!description.IsEmpty());
    m_id->SetLabel(x->manifest.id.empty() ? wxString::FromUTF8(x->folder.u8string()) : wxString::FromUTF8(x->manifest.id));
    m_state->SetLabel(state == State::ManifestError ? manifest_error_text(x->error) : state_text(state));
    m_state->SetForegroundColour(role(state == State::On ? MD3::Role::Primary
                                      : state == State::ManifestError || state == State::OffAfterFailures ? MD3::Role::Error
                                                                                                          : MD3::Role::OnSurfaceVariant));

    const bool waiting = state == State::WaitingNew || state == State::WaitingChanged;
    wxString   banner;
    if (waiting) {
        const Consent *c = h.consent(x->manifest.id);
        const auto     added = c ? added_permissions(x->manifest, *c) : std::vector<Permission>{};
        banner = state == State::WaitingNew ? _L("It stays off until you allow exactly what it asks for.")
                                            : added.empty() ? _L("This version changed. It stays off until you allow it again.")
                                                            : wxString::Format(_L("This version asks for %d new permission(s). It stays off until you allow exactly this list."),
                                                                               int(added.size()));
    } else if (state == State::OffAfterFailures) {
        banner = wxString::Format(_L("It was turned off after %d failed runs in a row. Turn it on to try again; Runs says what went wrong."),
                                  h.limits().off_after_failures);
    } else if (state == State::AllOff) {
        banner = _L("Extensions are off for the whole app. Turn on Extensions on, top right, to run any of them.");
    } else if (state == State::ManifestError) {
        banner = manifest_error_text(x->error) + ". " + _L("Fix manifest.json in its folder, then Reload.");
    }
    m_banner_text->SetLabel(banner);
    m_banner->Show(!banner.IsEmpty());
    m_review->Show(waiting);

    m_on->SetValue(state == State::On);
    m_on->Enable(state != State::ManifestError && state != State::AllOff);
    m_sample->Enable(state == State::On);
    m_open_folder->Enable(true);
    m_remove->Enable(!x->manifest.id.empty() || state == State::ManifestError);

    refresh_tabs();
    fill_overview(*x);
    fill_permissions(*x);
    fill_settings(*x);
    fill_runs(*x);
    fill_manifest(*x);
    select_tab(m_tab);
    m_detail_body->Layout();
    m_detail->Layout();
}

void ExtensionsPanel::fill_overview(const Installed &x)
{
    wxWindow *page = m_overview;
    page->DestroyChildren();
    wxSizer *s = page->GetSizer();
    s->Clear(false);
    const auto heading = [&](const wxString &text) { s->Add(label(page, Label::Head_14, text), 0, wxTOP | wxBOTTOM, FromDIP(6)); };
    const auto line    = [&](std::uint32_t glyph, const wxString &text, bool muted = false) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(glyph_window(page, glyph, MD3::Role::OnSurfaceVariant, 18), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        row->Add(label(page, Label::Body_13, text, muted, true), 1, wxALIGN_CENTER_VERTICAL);
        s->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(4));
    };

    heading(_L("Hooks"));
    if (x.manifest.hooks.empty()) line(GlyphInfo, _L("None: it only runs when you start one of its commands."), true);
    for (Hook h : x.manifest.hooks) {
        if (h == Hook::Command) continue;
        line(glyph_for(x.manifest), hook_title(h));
    }

    const bool on = Runtime::instance().host().state(x) == State::On;
    heading(_L("Commands"));
    if (x.manifest.actions.empty()) line(GlyphInfo, _L("No commands of its own."), true);
    for (const Action &a : x.manifest.actions) {
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(glyph_window(page, GlyphBolt, MD3::Role::OnSurfaceVariant, 18), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        row->Add(label(page, Label::Body_13, wxString::FromUTF8(a.title)), 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
        auto *run = button(page, _L("Run"), GlyphPlay, Button::Variant::Tonal);
        run->Enable(on);
        run->SetToolTip(on ? _L("Run it now with the plate and selection you have") : _L("Turn the extension on first"));
        const std::string id = a.id;
        run->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) { run_action(id); });
        row->Add(run, 0, wxALIGN_CENTER_VERTICAL);
        s->Add(row, 0, wxEXPAND | wxBOTTOM, FromDIP(4));
    }
    if (!x.manifest.actions.empty()) line(GlyphInfo, _L("Its commands are also in the command palette (Ctrl+Shift+F) while it is on."), true);

    heading(_L("Program"));
    line(GlyphTerminal, x.manifest.run.empty() ? _L("Not set") : wxString::FromUTF8(x.manifest.run));
    line(GlyphCode, wxString::Format(_L("API version %d · runs in %s"), x.manifest.api_version, wxString::FromUTF8(x.folder.u8string())), true);

    heading(_L("What extensions can hook"));
    auto *table = new wxFlexGridSizer(3, FromDIP(4), FromDIP(16));
    table->AddGrowableCol(1, 1);
    table->AddGrowableCol(2, 1);
    for (const wxString &t : {_L("Hook"), _L("Receives"), _L("May answer")}) table->Add(label(page, Label::Head_12, t, true));
    for (const HookInfo &h : hooks()) {
        wxString id = h.id;
        if (!h.built) id << "\n(" << _L("not run yet") << ")";
        table->Add(label(page, Label::Body_12, id));
        table->Add(label(page, Label::Body_12, _(h.receives), true, true), 1, wxEXPAND);
        table->Add(label(page, Label::Body_12, _(h.answers), true, true), 1, wxEXPAND);
    }
    s->Add(table, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
    page->Layout();
    static_cast<MD3ScrolledWindow *>(page)->FitInside();
}

void ExtensionsPanel::fill_permissions(const Installed &x)
{
    wxWindow *page = m_permissions;
    page->DestroyChildren();
    wxSizer *s = page->GetSizer();
    s->Clear(false);
    const Consent *c = x.manifest.id.empty() ? nullptr : Runtime::instance().host().consent(x.manifest.id);

    auto *table = new wxFlexGridSizer(3, FromDIP(8), FromDIP(12));
    table->AddGrowableCol(1, 1);
    table->Add(new wxPanel(page, wxID_ANY, wxDefaultPosition, wxSize(1, 1)));
    table->Add(label(page, Label::Head_12, _L("What it may receive or do"), true));
    table->Add(label(page, Label::Head_12, _L("State"), true));
    for (const PermissionInfo &p : permissions()) {
        const bool asked    = std::find(x.manifest.permissions.begin(), x.manifest.permissions.end(), p.permission) != x.manifest.permissions.end();
        const bool answered = c && c->version == x.manifest.version;
        const bool yes      = c && allowed(*c, p.permission);
        wxString   state;
        MD3::Role  colour = MD3::Role::OnSurfaceVariant;
        if (!asked) state = _L("Not asked for");
        else if (answered && yes && !(c->off_by_user && c->allowed != x.manifest.permissions)) {
            state  = _L("Allowed");
            colour = MD3::Role::Primary;
        } else if (answered && c->off_by_user) state = _L("Not allowed: kept off");
        else if (yes) state = wxString::Format(_L("Allowed for %s, asking again"), wxString::FromUTF8(c->version));
        else {
            state  = _L("Asking now");
            colour = MD3::Role::Error;
        }
        table->Add(glyph_window(page, glyph_for(p.permission), asked ? MD3::Role::OnSurface : MD3::Role::OutlineVariant, 18), 0, wxALIGN_CENTER_VERTICAL);
        wxString text = _(p.title);
        text << "\n" << p.id;
        table->Add(label(page, Label::Body_13, text, !asked, true), 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);
        auto *st = label(page, Label::Head_12, state);
        st->SetForegroundColour(role(colour));
        table->Add(st, 0, wxALIGN_CENTER_VERTICAL);
    }
    table->Add(glyph_window(page, GlyphLock, MD3::Role::Primary, 18), 0, wxALIGN_CENTER_VERTICAL);
    table->Add(label(page, Label::Body_13, _L("Passwords, access codes, account tokens, the printer's serial number"), false, true), 1,
               wxEXPAND | wxALIGN_CENTER_VERTICAL);
    auto *never = label(page, Label::Head_12, _L("Never sent"));
    never->SetForegroundColour(role(MD3::Role::Primary));
    table->Add(never, 0, wxALIGN_CENTER_VERTICAL);
    s->Add(table, 0, wxEXPAND | wxBOTTOM, FromDIP(12));
    s->Add(label(page, Label::Body_12,
                 _L("Every request is filtered here before it leaves: a section the extension may not receive is left out, and any value whose name "
                    "looks like a secret is removed at every depth. An answer that uses something not allowed is thrown away and recorded under Runs. "
                    "An extension is still a program running as you, so Bambu Studio cannot stop it from reading your own files."),
                 true, true),
           0, wxEXPAND);
    page->Layout();
    static_cast<MD3ScrolledWindow *>(page)->FitInside();
}

bool ExtensionsPanel::save_setting(const std::string &id, const std::string &setting, const nlohmann::json &value)
{
    std::string problem;
    if (Runtime::instance().host().set_setting(id, setting, value, problem)) return true;
    wxString why;
    if (problem == "below-minimum") why = _L("It is below the smallest value allowed.");
    else if (problem == "above-maximum") why = _L("It is above the largest value allowed.");
    else if (problem == "not-whole") why = _L("It must be a whole number.");
    else if (problem == "too-long") why = _L("It is longer than allowed.");
    else if (problem == "has-control") why = _L("It holds a line break or another control character.");
    else why = _L("It is not a value this setting takes.");
    MessageDialog(this, _L("The setting was not saved.") + "\n\n" + why, _L("Extensions"), wxOK | wxICON_WARNING).ShowModal();
    return false;
}

void ExtensionsPanel::fill_settings(const Installed &x)
{
    const std::string shown = x.manifest.id + "@" + x.manifest.version;
    if (shown == m_settings_for) return;
    m_settings_for = shown;
    wxWindow *page = m_settings;
    page->DestroyChildren();
    wxSizer *s = page->GetSizer();
    s->Clear(false);
    const std::string id = x.manifest.id;
    if (id.empty() || x.manifest.settings.empty()) {
        s->Add(label(page, Label::Body_13, _L("This extension has no settings."), true, true), 0, wxEXPAND);
        page->Layout();
        static_cast<MD3ScrolledWindow *>(page)->FitInside();
        return;
    }
    const nlohmann::json values = Runtime::instance().host().setting_values(id);
    auto *table = new wxFlexGridSizer(2, FromDIP(10), FromDIP(16));
    table->AddGrowableCol(0, 1);
    for (const SettingSpec &spec : x.manifest.settings) {
        const std::string     setting = spec.id;
        const nlohmann::json &value   = values.contains(setting) ? values[setting] : spec.default_value;
        auto *names = new wxBoxSizer(wxVERTICAL);
        wxString title = wxString::FromUTF8(spec.title);
        if (!spec.unit.empty()) title << " (" << wxString::FromUTF8(spec.unit) << ")";
        names->Add(label(page, Label::Head_13, title), 0);
        wxString note = wxString::FromUTF8(spec.description);
        if (spec.type == SettingType::Number && (spec.has_minimum || spec.has_maximum)) {
            if (!note.IsEmpty()) note << " ";
            const auto number = [](double v) { return wxString::FromCDouble(v, v == double(static_cast<long long>(v)) ? 0 : 2); };
            if (spec.has_minimum && spec.has_maximum)
                note << wxString::Format(_L("From %s to %s."), number(spec.minimum), number(spec.maximum));
            else if (spec.has_minimum) note << wxString::Format(_L("At least %s."), number(spec.minimum));
            else note << wxString::Format(_L("At most %s."), number(spec.maximum));
        }
        if (!note.IsEmpty()) names->Add(label(page, Label::Body_12, note, true, true), 0, wxEXPAND | wxTOP, FromDIP(2));
        names->Add(label(page, Label::Body_12, wxString::FromUTF8(setting), true), 0, wxTOP, FromDIP(2));
        table->Add(names, 1, wxEXPAND | wxALIGN_CENTER_VERTICAL);

        wxWindow *control = nullptr;
        switch (spec.type) {
        case SettingType::Switch: {
            auto *sw = new SwitchButton(page);
            sw->SetValue(value.is_boolean() && value.get<bool>());
            sw->Bind(wxEVT_TOGGLEBUTTON, [this, id, setting, sw](wxCommandEvent &) {
                if (!save_setting(id, setting, sw->GetValue())) sw->SetValue(!sw->GetValue());
            });
            control = sw;
            break;
        }
        case SettingType::Choice: {
            auto *combo = new ComboBox(page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(200), -1), 0, nullptr, wxCB_READONLY);
            std::vector<std::string> ids;
            for (const SettingChoice &c : spec.choices) {
                combo->Append(wxString::FromUTF8(c.title));
                ids.push_back(c.id);
                if (value.is_string() && value.get<std::string>() == c.id) combo->SetSelection(int(ids.size() - 1));
            }
            combo->Bind(wxEVT_COMBOBOX, [this, id, setting, combo, ids](wxCommandEvent &) {
                const int at = combo->GetSelection();
                if (at >= 0 && at < int(ids.size())) save_setting(id, setting, ids[std::size_t(at)]);
            });
            control = combo;
            break;
        }
        case SettingType::Text:
        case SettingType::Number: {
            const bool number = spec.type == SettingType::Number;
            wxString   shown_value;
            if (number && value.is_number_integer()) shown_value = wxString::Format("%lld", value.get<long long>());
            else if (number && value.is_number()) shown_value = wxString::FromCDouble(value.get<double>());
            else if (value.is_string()) shown_value = wxString::FromUTF8(value.get<std::string>());
            auto *input = new TextInput(page, shown_value, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(number ? 120 : 240), -1),
                                        wxTE_PROCESS_ENTER);
            auto saved = std::make_shared<wxString>(shown_value);
            const auto commit = [this, id, setting, input, number, saved]() {
                const wxString text = input->GetTextCtrl()->GetValue();
                if (text == *saved) return;
                nlohmann::json next;
                if (number) {
                    double parsed = 0;
                    long long whole = 0;
                    if (text.ToLongLong(&whole)) next = whole;
                    else if (text.ToCDouble(&parsed)) next = parsed;
                    else {
                        MessageDialog(this, _L("The setting was not saved.") + "\n\n" + _L("It is not a number."), _L("Extensions"), wxOK | wxICON_WARNING).ShowModal();
                        input->GetTextCtrl()->ChangeValue(*saved);
                        return;
                    }
                } else {
                    next = into_u8(text);
                }
                if (save_setting(id, setting, next)) *saved = text;
                else input->GetTextCtrl()->ChangeValue(*saved);
            };
            input->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, [commit](wxCommandEvent &e) {
                commit();
                e.Skip();
            });
            input->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [commit](wxFocusEvent &e) {
                commit();
                e.Skip();
            });
            control = input;
            break;
        }
        }
        control->SetName(wxString::FromUTF8(spec.title));
        table->Add(control, 0, wxALIGN_CENTER_VERTICAL);
    }
    s->Add(table, 0, wxEXPAND | wxBOTTOM, FromDIP(12));
    auto *reset = make_button(page, _L("Back to the defaults"), GlyphUndo);
    reset->Bind(wxEVT_BUTTON, [this, id](wxCommandEvent &) {
        Runtime::instance().host().reset_settings(id);
        m_settings_for.clear(); // the page shows the defaults again
        CallAfter([this]() {
            if (const Installed *now = selected()) fill_settings(*now);
        });
    });
    s->Add(reset, 0, wxBOTTOM, FromDIP(12));
    s->Add(label(page, Label::Body_12,
                 _L("Saved as you change them, on this computer, and sent to this extension with every request. A setting that looks like a password, "
                    "token or key is refused when the extension is added, so nothing here is ever a secret."),
                 true, true),
           0, wxEXPAND);
    page->Layout();
    static_cast<MD3ScrolledWindow *>(page)->FitInside();
}

void ExtensionsPanel::fill_runs(const Installed &x)
{
    m_runs->DeleteAllItems();
    if (x.manifest.id.empty()) {
        m_runs_note->SetLabel(_L("An extension with a broken manifest never runs."));
        return;
    }
    const Host  &h    = Runtime::instance().host();
    const auto   runs = h.runs().for_extension(x.manifest.id);
    for (std::size_t n = runs.size(); n-- > 0;) {
        const RunRecord    &r = runs[n];
        wxVector<wxVariant> row;
        row.push_back(wxVariant(when_text(r.time)));
        wxString hook = hook_text(r, &x.manifest);
        if (!r.subject.empty()) hook << dot() << wxString::FromUTF8(r.subject);
        row.push_back(wxVariant(hook));
        row.push_back(wxVariant(wxString::Format("%.2f s", r.seconds)));
        row.push_back(wxVariant(bytes_text(r.answer_bytes)));
        row.push_back(wxVariant(outcome_text(r.outcome, r.problem, h.limits())));
        m_runs->AppendItem(row);
    }
    m_runs_note->SetLabel(runs.empty() ? _L("No runs yet. Try with a sample to see what it answers.")
                                       : wxString::Format(_L("%d runs, newest first. The last %d runs of all extensions are kept."), int(runs.size()),
                                                          int(RunLog::kLimit)));
}

void ExtensionsPanel::fill_manifest(const Installed &x)
{
    const std::string text = read_text(x.folder / "manifest.json", kManifestLimit);
    m_manifest->SetValue(text.empty() ? _L("(no manifest.json)") : wxString::FromUTF8(text));
    if (x.error.ok()) {
        m_manifest_state->SetLabel(_L("Valid"));
        m_manifest_state->SetForegroundColour(role(MD3::Role::Primary));
    } else {
        m_manifest_state->SetLabel(manifest_error_text(x.error));
        m_manifest_state->SetForegroundColour(role(MD3::Role::Error));
    }
}

void ExtensionsPanel::set_filter(int filter)
{
    m_filter = filter;
    refresh_filters();
    refresh_list();
    refresh_detail();
}

void ExtensionsPanel::select(const std::string &id)
{
    m_selected = id;
    m_filter   = FilterAll;
    if (m_search) m_search->SetValue(wxString());
    refresh_all();
}

// ---------------------------------------------------------------- actions

void ExtensionsPanel::set_master(bool on)
{
    Runtime::instance().host().set_master(on);
    if (!on) Runtime::instance().stop_all();
    Runtime::instance().notify_changed();
}

bool ExtensionsPanel::review_selected()
{
    const Installed *x = selected();
    if (!x || x->manifest.id.empty()) return false;
    Host             &h      = Runtime::instance().host();
    const std::string id     = x->manifest.id;
    const Consent    *before = h.consent(id);
    Consent           previous;
    if (before) previous = *before;
    const ConsentAnswer answer = ask_consent(this, *x, before ? &previous : nullptr);
    if (answer == ConsentAnswer::Allow) h.allow(id);
    else if (answer == ConsentAnswer::KeepOff) h.keep_off(id);
    Runtime::instance().notify_changed();
    return answer == ConsentAnswer::Allow;
}

void ExtensionsPanel::toggle_selected(bool on)
{
    const Installed *x = selected();
    if (!x || x->manifest.id.empty()) return;
    Host       &h     = Runtime::instance().host();
    const State state = h.state(*x);
    if (on && (state == State::WaitingNew || state == State::WaitingChanged)) {
        review_selected();
        return;
    }
    if (!h.set_enabled(x->manifest.id, on)) {
        // The version or the list changed: the prompt decides.
        review_selected();
        return;
    }
    Runtime::instance().notify_changed();
}

void ExtensionsPanel::add_from_folder()
{
    wxDirDialog dialog(this, _L("Pick an extension folder (it holds manifest.json)"), wxString(), wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    const fs::path from = fs::u8path(into_u8(dialog.GetPath()));
    Host          &h    = Runtime::instance().host();
    Manifest       incoming;
    ManifestError  error;
    const Host::Inspection kind = h.inspect(from, incoming, error);
    if (kind == Host::Inspection::Invalid) {
        MessageDialog(this, wxString::Format(_L("This folder cannot be added: %s."), manifest_error_text(error)), _L("Extensions"), wxOK | wxICON_WARNING)
            .ShowModal();
        return;
    }
    if (kind != Host::Inspection::New) {
        const Installed *installed = h.find(incoming.id);
        if (installed && !ask_replace(this, installed->manifest, incoming, kind)) return;
    }
    std::string problem;
    if (!h.install(from, problem)) {
        MessageDialog(this, wxString::Format(_L("The folder could not be copied: %s."), wxString::FromUTF8(problem)), _L("Extensions"),
                      wxOK | wxICON_WARNING)
            .ShowModal();
        return;
    }
    m_selected = incoming.id;
    m_filter   = FilterAll;
    Runtime::instance().notify_changed();
    if (const Installed *x = h.find(incoming.id)) {
        const State s = h.state(*x);
        if (s == State::WaitingNew || s == State::WaitingChanged) review_selected();
    }
}

void ExtensionsPanel::remove_selected()
{
    const Installed *x = selected();
    if (!x) return;
    const wxString name = display_name(*x);
    wxString       text = wxString::Format(_L("Remove %s? Its folder is deleted from this computer."), name);
    if (x->builtin) text << " " << _L("A built-in extension stays removed until you add its folder again.");
    MessageDialog confirm(this, text, _L("Remove extension"), wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION);
    if (confirm.ShowModal() != wxID_YES) return;
    std::string problem;
    if (!x->manifest.id.empty()) {
        if (!Runtime::instance().host().remove(x->manifest.id, problem)) {
            MessageDialog(this, wxString::Format(_L("%s could not be removed: %s."), name, wxString::FromUTF8(problem)), _L("Extensions"),
                          wxOK | wxICON_WARNING)
                .ShowModal();
        }
    } else {
        std::error_code ec;
        fs::remove_all(x->folder, ec);
        Runtime::instance().host().load();
    }
    m_selected.clear();
    Runtime::instance().notify_changed();
}

void ExtensionsPanel::open_selected_folder()
{
    if (const Installed *x = selected()) desktop_open_any_folder(x->folder.u8string());
}

void ExtensionsPanel::try_sample()
{
    const Installed *x = selected();
    if (!x || x->manifest.id.empty()) return;
    struct Choice
    {
        Hook        hook;
        std::string action;
        wxString    title;
    };
    std::vector<Choice> choices;
    for (Hook h : x->manifest.hooks)
        if (h != Hook::Command) choices.push_back({h, {}, hook_title(h)});
    for (const Action &a : x->manifest.actions) choices.push_back({Hook::Command, a.id, wxString::Format(_L("Command: %s"), wxString::FromUTF8(a.title))});
    if (choices.empty()) return;
    std::size_t pick = 0;
    if (choices.size() > 1) {
        wxMenu menu;
        for (std::size_t i = 0; i < choices.size(); ++i) menu.Append(int(wxID_HIGHEST + 1 + i), choices[i].title);
        const int chosen = MD3::PopupMenuSelection(this, menu, m_sample->ClientToScreen(wxPoint(0, m_sample->GetSize().y)));
        if (chosen == wxID_NONE) return;
        pick = std::size_t(chosen - wxID_HIGHEST - 1);
        if (pick >= choices.size()) return;
    }
    Runtime::instance().try_sample(x->manifest.id, choices[pick].hook, choices[pick].action);
}

void ExtensionsPanel::run_action(const std::string &action)
{
    const Installed *x = selected();
    if (!x) return;
    if (!Runtime::instance().run_action(x->manifest.id, action))
        MessageDialog(this, _L("Turn the extension on first."), _L("Extensions"), wxOK | wxICON_INFORMATION).ShowModal();
}

void ExtensionsPanel::export_runs()
{
    wxFileDialog dialog(this, _L("Export runs"), wxString(), "extension-runs.json", "JSON (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    std::ofstream out(fs::u8path(into_u8(dialog.GetPath())), std::ios::binary | std::ios::trunc);
    out << Runtime::instance().host().runs().export_json();
    if (!out)
        MessageDialog(this, _L("The runs could not be saved there."), _L("Extensions"), wxOK | wxICON_WARNING).ShowModal();
}

void ExtensionsPanel::save_limits()
{
    Limits l;
    l.timeout_ms         = m_timeout->GetValue() * 1000;
    l.max_answer         = std::size_t(std::max(1, m_max_answer->GetValue())) * 1024;
    l.off_after_failures = m_off_after->GetValue();
    Runtime::instance().host().set_limits(clamp_limits(l));
    refresh_week();
}

}}} // namespace Slic3r::GUI::Extensions
