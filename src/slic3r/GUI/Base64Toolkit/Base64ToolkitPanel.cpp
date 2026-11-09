#include "Base64ToolkitPanel.hpp"
#include "../Widgets/MD3Menu.hpp"

#include "../FeatureFinder/LocalToolsActivity.hpp"
#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "../MsgDialog.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3DataView.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/MD3Tokens.hpp"
#include "../Widgets/MaterialIcon.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/StateColor.hpp"
#include "../Widgets/StaticBox.hpp"
#include "../Widgets/SwitchButton.hpp"
#include "../Widgets/TextArea.hpp"
#include "../Widgets/TextInput.hpp"
#include "../Widgets/TextTabbar.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Utils.hpp"

#include <wx/clipbrd.h>
#include <wx/datetime.h>
#include <wx/dcclient.h>
#include <wx/dnd.h>
#include <wx/filedlg.h>
#include <wx/image.h>
#include <wx/log.h>
#include <wx/mediactrl.h>
#include <wx/stdpaths.h>
#include <wx/mstream.h>
#include <wx/numformatter.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/wrapsizer.h>

#include <algorithm>
#include <ctime>
#include <fstream>
#include <sstream>

namespace Slic3r { namespace GUI {

namespace B64 = Base64Toolkit;
namespace fs  = std::filesystem;

namespace {

constexpr const char *kOptionsKey  = "base64_toolkit_options";
constexpr const char *kPayloadsKey = "base64_keep_payloads";
constexpr std::size_t kDisplayLimit   = std::size_t(1) << 20;  // characters shown in a text box
constexpr std::size_t kFileLimit      = std::size_t(16) << 20; // files read into the Text and data: URL tabs
constexpr std::size_t kTextPreview    = std::size_t(2) << 20;
constexpr std::size_t kImagePreview   = std::size_t(64) << 20;

enum Glyphs : std::uint32_t {
    GlyphPaste = 0xE14F, GlyphFileOpen = 0xEAF3, GlyphUpload = 0xE9FC, GlyphShield = 0xE9E0, GlyphImage = 0xE3F4,
    GlyphLink = 0xE157, GlyphError = 0xE000, GlyphCheck = 0xE86C, GlyphCopy = 0xE14D, GlyphSave = 0xE161,
    GlyphSwap = 0xE8D4, GlyphPause = 0xE034, GlyphPlay = 0xE037, GlyphFolder = 0xE2C8, GlyphDelete = 0xE872,
    GlyphUndo = 0xE166, GlyphRedo = 0xE15A, GlyphCode = 0xE86F, GlyphCancel = 0xE5C9, GlyphClearAll = 0xE0B8,
    GlyphShare = 0xE6B8, GlyphBackspace = 0xE14A, GlyphFix = 0xE663
};

wxColour role(MD3::Role r) { return StateColor::semantic(r); }

wxString number(std::uint64_t value)
{
    return wxNumberFormatter::ToString(static_cast<long long>(value), wxNumberFormatter::Style_WithThousandsSep);
}

wxString size_text(std::uint64_t bytes)
{
    if (bytes == 1) return _L("1 byte");
    if (bytes < 1024) return wxString::Format(_L("%s bytes"), number(bytes));
    return wxString::FromUTF8(B64::human_size(bytes));
}

wxString characters_text(std::size_t count)
{
    return count == 1 ? _L("1 character") : wxString::Format(_L("%s characters"), number(count));
}

wxString encoding_name(B64::TextEncoding e)
{
    switch (e) {
    case B64::TextEncoding::Utf16LE: return "UTF-16 LE";
    case B64::TextEncoding::Utf16BE: return "UTF-16 BE";
    case B64::TextEncoding::Latin1: return "Latin-1";
    case B64::TextEncoding::Utf8: break;
    }
    return "UTF-8";
}

std::string charset_for(B64::TextEncoding e)
{
    switch (e) {
    case B64::TextEncoding::Utf16LE: return "utf-16le";
    case B64::TextEncoding::Utf16BE: return "utf-16be";
    case B64::TextEncoding::Latin1: return "iso-8859-1";
    case B64::TextEncoding::Utf8: break;
    }
    return "utf-8";
}

// The character itself, or U+XXXX for invisible ones.
wxString character_text(std::uint32_t c)
{
    if (c == 0) return {};
    if (c < 0x20 || c == 0x7F || (c >= 0x80 && c < 0xA0) || c == 0xFEFF) return wxString::Format("U+%04X", unsigned(c));
    wxString s;
    if (c > 0xFFFF && sizeof(wchar_t) == 2) {
        const std::uint32_t v = c - 0x10000;
        s << wchar_t(0xD800 + (v >> 10)) << wchar_t(0xDC00 + (v & 0x3FF));
    } else {
        s << wchar_t(c);
    }
    return s;
}

wxString error_title(B64::DecodeError e)
{
    switch (e) {
    case B64::DecodeError::InvalidCharacter: return _L("Invalid character");
    case B64::DecodeError::WrongPadding: return _L("Wrong padding");
    case B64::DecodeError::TrailingData: return _L("Data after padding");
    case B64::DecodeError::TruncatedInput: return _L("Cut short");
    case B64::DecodeError::Empty: return _L("Nothing to decode");
    case B64::DecodeError::None: break;
    }
    return {};
}

wxString error_explanation(B64::DecodeError e)
{
    switch (e) {
    case B64::DecodeError::InvalidCharacter: return _L("Not in the chosen alphabet; the offset counts characters, not bytes.");
    case B64::DecodeError::WrongPadding: return _L("'=' in the wrong place, missing in strict mode, or non-zero leftover bits.");
    case B64::DecodeError::TrailingData: return _L("Something follows the closing '='; strict and lenient both stop.");
    case B64::DecodeError::TruncatedInput: return _L("One lone character in the last group can never decode.");
    case B64::DecodeError::Empty: return _L("There is no Base64 here, only spaces or line breaks.");
    case B64::DecodeError::None: break;
    }
    return {};
}

wxString file_problem(const B64::FileResult &r)
{
    if (r.decode_error != B64::DecodeError::None) {
        switch (r.decode_error) {
        case B64::DecodeError::InvalidCharacter:
            return wxString::Format(_L("Bad character %s at %s"), character_text(r.error_character), number(r.error_offset));
        case B64::DecodeError::WrongPadding: return wxString::Format(_L("Wrong padding at %s"), number(r.error_offset));
        case B64::DecodeError::TrailingData: return wxString::Format(_L("Data after padding at %s"), number(r.error_offset));
        case B64::DecodeError::TruncatedInput: return _L("Cut short at the end");
        case B64::DecodeError::Empty: return _L("Nothing to decode");
        case B64::DecodeError::None: break;
        }
    }
    const std::string &p = r.problem;
    if (p == "unreadable") return _L("Could not read the file");
    if (p == "empty") return _L("The file is empty");
    if (p == "no-free-name") return _L("No free name in that folder");
    if (p == "unwritable") return _L("Could not write in that folder");
    if (p == "write-failed") return _L("Writing failed; is the disk full?");
    if (p == "verification-failed") return _L("Verification failed, so nothing was kept");
    if (p == "rename-failed") return _L("Could not name the finished file");
    if (p == "bad-data-url") return _L("Not a valid data: URL");
    if (p == "data-url-not-base64") return _L("This data: URL is not Base64");
    if (p == "cancelled") return _L("Cancelled, nothing written");
    return wxString::FromUTF8(p);
}

bool copy_to_clipboard(const wxString &text)
{
    if (!wxTheClipboard->Open()) return false;
    const bool ok = wxTheClipboard->SetData(new wxTextDataObject(text));
    wxTheClipboard->Close();
    return ok;
}

wxString clipboard_text()
{
    wxString text;
    if (!wxTheClipboard->Open()) return text;
    if (wxTheClipboard->IsSupported(wxDF_UNICODETEXT) || wxTheClipboard->IsSupported(wxDF_TEXT)) {
        wxTextDataObject data;
        if (wxTheClipboard->GetData(data)) text = data.GetText();
    }
    wxTheClipboard->Close();
    return text;
}

fs::path to_path(const wxString &path) { return fs::path(path.ToStdWstring()); }

bool read_limited(const fs::path &path, std::size_t limit, std::string &bytes, bool &too_large)
{
    too_large = false;
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) return false;
    if (size > limit) {
        too_large = true;
        return false;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream text;
    text << in.rdbuf();
    bytes = text.str();
    return true;
}

bool write_all(const fs::path &path, const void *data, std::size_t size)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(static_cast<const char *>(data), std::streamsize(size));
    return bool(out);
}

std::int64_t now_seconds() { return std::int64_t(std::time(nullptr)); }

class PathDropTarget final : public wxFileDropTarget
{
public:
    explicit PathDropTarget(std::function<void(std::vector<fs::path>)> accept) : m_accept(std::move(accept)) {}
    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString &files) override
    {
        if (files.empty() || files.size() > 1000) return false;
        std::vector<fs::path> paths;
        for (const wxString &file : files) paths.push_back(to_path(file));
        m_accept(std::move(paths));
        return true;
    }

private:
    std::function<void(std::vector<fs::path>)> m_accept;
};

// A Material Symbols glyph in its own small window.
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

} // namespace

// ------------------------------------------------------------------- files queue

struct Base64ToolkitPanel::FileJob
{
    enum class Status { Queued, Running, Done, Failed, Cancelled };
    int                                   id = 0;
    fs::path                              source;
    bool                                  encode = true;
    B64::EncodeOptions                    encode_options;
    B64::DecodeOptions                    decode_options;
    Status                                status = Status::Queued;
    std::uint64_t                         done = 0, total = 0;
    std::chrono::steady_clock::time_point started;
    double                                paused_seconds = 0;
    B64::FileResult                       result;
    bool                                  cancel   = false;
    bool                                  recorded = false;
    // This file alone, on top of the queue's pause: it waits where it is
    // (queued or mid-file) while the other files go on.
    bool                                  paused   = false;
    std::chrono::steady_clock::time_point paused_at;
};

struct Base64ToolkitPanel::FileQueue
{
    std::mutex                            mutex;
    std::condition_variable               wake;
    std::vector<FileJob>                  jobs;
    bool                                  paused = false;
    bool                                  stop   = false;
    int                                   next_id = 1;
    std::chrono::steady_clock::time_point paused_at;
};

void Base64ToolkitPanel::worker_loop(std::shared_ptr<FileQueue> q)
{
    const auto find = [&q](int id) -> FileJob * {
        for (FileJob &job : q->jobs)
            if (job.id == id) return &job;
        return nullptr;
    };
    for (;;) {
        int                id     = 0;
        bool               encode = true;
        fs::path           source;
        B64::EncodeOptions eo;
        B64::DecodeOptions dopt;
        {
            std::unique_lock<std::mutex> lock(q->mutex);
            q->wake.wait(lock, [&] {
                if (q->stop) return true;
                if (q->paused) return false;
                return std::any_of(q->jobs.begin(), q->jobs.end(), [](const FileJob &j) { return j.status == FileJob::Status::Queued && !j.paused; });
            });
            if (q->stop) return;
            for (FileJob &job : q->jobs) {
                if (job.status != FileJob::Status::Queued || job.paused) continue;
                job.status  = FileJob::Status::Running;
                job.started = std::chrono::steady_clock::now();
                id          = job.id;
                encode      = job.encode;
                source      = job.source;
                eo          = job.encode_options;
                dopt        = job.decode_options;
                break;
            }
        }
        const auto progress = [&](std::uint64_t done, std::uint64_t total) {
            std::unique_lock<std::mutex> lock(q->mutex);
            if (FileJob *job = find(id)) {
                job->done  = done;
                job->total = total;
            }
            q->wake.wait(lock, [&] {
                const FileJob *job = find(id);
                return q->stop || job == nullptr || job->cancel || (!q->paused && !job->paused);
            });
            const FileJob *job = find(id);
            return !(q->stop || job == nullptr || job->cancel);
        };
        B64::FileResult result = encode ? B64::encode_file(source, eo, progress) : B64::decode_file(source, dopt, progress);
        std::lock_guard<std::mutex> lock(q->mutex);
        if (FileJob *job = find(id)) {
            job->status = result.ok ? FileJob::Status::Done : result.cancelled ? FileJob::Status::Cancelled : FileJob::Status::Failed;
            if (result.ok) job->done = job->total;
            job->result = std::move(result);
        }
    }
}

// ------------------------------------------------------------------- construction

Base64ToolkitPanel::Base64ToolkitPanel(wxWindow *parent)
    : wxPanel(parent, wxID_ANY)
    , m_convert_timer(this, wxWindow::NewControlId())
    , m_job_timer(this, wxWindow::NewControlId())
    , m_media_timer(this, wxWindow::NewControlId())
    , m_url_timer(this, wxWindow::NewControlId())
    , m_history(fs::u8path(data_dir()) / "base64-toolkit-history.jsonl")
{
    SetName("base64-toolkit");
    SetBackgroundColour(role(MD3::Role::Surface));
    const int pad = FromDIP(MD3::Metrics::active().padding);

    if (AppConfig *cfg = wxGetApp().app_config) {
        B64::TextOptions saved;
        if (B64::parse_options_json(cfg->get(kOptionsKey), saved)) m_options = saved;
    }
    m_history.load();

    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *title = make_label(this, Label::Head_20, _L("Base64 toolkit"));
    root->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, pad);
    auto *subtitle = make_label(this, Label::Body_13,
        _L("Encode and decode text and files on this computer only. Nothing is uploaded and nothing decoded is ever run or opened by itself."),
        true, true);
    root->Add(subtitle, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad / 2);

    m_tabs = new TextTabbar(this, TextTabbar::Align::Left, 28);
    root->Add(m_tabs, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad);
    m_book = new wxSimplebook(this, wxID_ANY);
    m_book->SetBackgroundColour(GetBackgroundColour());
    m_book->AddPage(build_text_page(m_book), "text");
    m_book->AddPage(build_files_page(m_book), "files");
    m_book->AddPage(build_url_page(m_book), "url");
    m_book->AddPage(build_history_page(m_book), "history");
    root->Add(m_book, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, pad / 2);

    m_status = make_label(this, Label::Body_12, wxString(), true, true);
    m_status->SetName("base64-status");
    root->Add(m_status, 0, wxEXPAND | wxALL, pad / 2);
    SetSizer(root);

    m_tabs->Bind(wxEVT_CHOICE, [this](wxCommandEvent &event) { select_tab(event.GetInt()); });
    Bind(wxEVT_TIMER, [this](wxTimerEvent &) { convert_now(); }, m_convert_timer.GetId());
    Bind(wxEVT_TIMER, [this](wxTimerEvent &) { poll_jobs(); }, m_job_timer.GetId());
    Bind(wxEVT_TIMER, [this](wxTimerEvent &) { url_read(); }, m_url_timer.GetId());

    refresh_tab_labels();
    select_tab(TabText);
    apply_options_to_controls();
    convert_now();
    refresh_history();
}

Base64ToolkitPanel::~Base64ToolkitPanel()
{
    m_convert_timer.Stop();
    m_job_timer.Stop();
    m_url_timer.Stop();
    m_media_timer.Stop();
    media_unload();
    stop_worker();
}

Label *Base64ToolkitPanel::make_label(wxWindow *parent, const wxFont &font, const wxString &text, bool muted, bool wrap)
{
    auto *label = new Label(parent, font, text, wrap ? (LB_AUTO_WRAP | wxST_NO_AUTORESIZE) : 0);
    if (wrap) label->SetMinSize(wxSize(FromDIP(120), -1));
    label->SetBackgroundColour(parent->GetBackgroundColour());
    label->SetForegroundColour(role(muted ? MD3::Role::OnSurfaceVariant : MD3::Role::OnSurface));
    return label;
}

Button *Base64ToolkitPanel::make_button(wxWindow *parent, const wxString &label, std::uint32_t glyph, bool tonal)
{
    auto *button = new Button(parent, label);
    button->SetVariant(tonal ? Button::Variant::Tonal : Button::Variant::Outlined);
    button->SetButtonSize(Button::Size::Small);
    if (glyph) button->SetGlyph(glyph);
    return button;
}

Button *Base64ToolkitPanel::make_icon_button(wxWindow *parent, std::uint32_t glyph, const wxString &tip)
{
    auto *button = new Button(parent, wxString());
    button->SetIconButton(Button::IconShape::Square, FromDIP(32));
    button->SetGlyph(glyph);
    button->SetToolTip(tip);
    button->SetName(tip);
    return button;
}

void Base64ToolkitPanel::add_choice(wxWindow *parent, wxSizer *row, Choice &choice, const wxString &caption,
                                    const std::vector<std::pair<int, wxString>> &items, std::function<void(int)> changed)
{
    auto *group = new wxBoxSizer(wxVERTICAL);
    group->Add(make_label(parent, Label::Body_12, caption, true), 0, wxBOTTOM, FromDIP(4));
    auto *buttons = new wxBoxSizer(wxHORIZONTAL);
    for (const auto &[value, text] : items) {
        auto *button = new Button(parent, text);
        button->SetVariant(Button::Variant::Outlined);
        button->SetButtonSize(Button::Size::Small);
        const int v = value;
        Choice   *c = &choice;
        button->Bind(wxEVT_BUTTON, [this, c, v, changed](wxCommandEvent &) {
            set_choice(*c, v);
            changed(v);
        });
        choice.buttons.emplace_back(value, button);
        buttons->Add(button, 0, wxRIGHT, FromDIP(4));
    }
    group->Add(buttons, 0);
    row->Add(group, 0, wxRIGHT | wxBOTTOM, FromDIP(16));
}

void Base64ToolkitPanel::set_choice(Choice &choice, int value)
{
    choice.value = value;
    for (auto &[v, button] : choice.buttons) {
        const bool on = v == value;
        button->SetVariant(on ? Button::Variant::Tonal : Button::Variant::Outlined);
        button->SetButtonSize(Button::Size::Small);
        button->SetName(button->GetLabel() + (on ? wxString(" (") + _L("selected") + ")" : wxString()));
        button->Refresh();
    }
}

void Base64ToolkitPanel::set_status(const wxString &message)
{
    if (!m_status) return;
    m_status->SetLabel(message);
    m_status->Wrap(std::max(FromDIP(200), GetClientSize().x - FromDIP(32)));
    Layout();
}

void Base64ToolkitPanel::refresh_tab_labels()
{
    std::size_t jobs = 0;
    if (m_queue) {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        jobs = m_queue->jobs.size();
    }
    const wxString files   = jobs ? wxString::Format(_L("Files %s"), number(jobs)) : _L("Files");
    const wxString history = m_history.entries().empty() ? _L("History") : wxString::Format(_L("History %s"), number(m_history.entries().size()));
    const wxString key     = files + "\n" + history;
    if (key == m_tab_labels) return;
    m_tab_labels = key;
    m_tabs->ClearTabs();
    m_tabs->AddTab(_L("Text"));
    m_tabs->AddTab(files);
    m_tabs->AddTab(_L("data: URL"));
    m_tabs->AddTab(history);
    m_tabs->SetSelection(m_tab);
    Layout();
}

void Base64ToolkitPanel::select_tab(int tab)
{
    if (tab < TabText || tab > TabHistory) return;
    m_tab = tab;
    m_tabs->SetSelection(tab);
    m_book->ChangeSelection(std::size_t(tab));
    if (tab == TabHistory) refresh_history();
    Layout();
}

// ------------------------------------------------------------------- Text tab

wxWindow *Base64ToolkitPanel::build_text_page(wxWindow *parent)
{
    auto *page = new MD3ScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    page->SetBackgroundColour(GetBackgroundColour());
    page->SetScrollRate(0, FromDIP(24));
    m_text_page = page;
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    const int gap = FromDIP(12);

    auto *options = new wxWrapSizer(wxHORIZONTAL);
    add_choice(page, options, m_direction, _L("Direction"), {{1, _L("Encode")}, {0, _L("Decode")}}, [this](int v) {
        m_options.encode = v == 1;
        schedule_convert();
    });
    add_choice(page, options, m_alphabet, _L("Alphabet"), {{0, _L("Standard §4")}, {1, _L("URL-safe §5")}}, [this](int v) {
        m_options.alphabet = v == 1 ? B64::Alphabet::UrlSafe : B64::Alphabet::Standard;
        schedule_convert();
    });
    add_choice(page, options, m_lines, _L("Lines"),
               {{0, _L("One line")}, {1, _L("MIME 76, CRLF")}, {2, _L("Custom width")}}, [this](int v) {
                   m_options.lines = v == 1 ? B64::LineMode::Mime : v == 2 ? B64::LineMode::Custom : B64::LineMode::OneLine;
                   m_width->Show(v == 2);
                   m_text_page->Layout();
                   schedule_convert();
               });
    m_width = new SpinInput(page, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(110), -1), 0, 4, 1000,
                            B64::clamp_width(m_options.custom_width));
    m_width->SetToolTip(_L("Characters per line, 4 to 1000"));
    m_width->SetName(_L("Characters per line"));
    m_width->Bind(wxEVT_SPINCTRL, [this](wxCommandEvent &) {
        m_options.custom_width = B64::clamp_width(m_width->GetValue());
        schedule_convert();
    });
    options->Add(m_width, 0, wxALIGN_BOTTOM | wxRIGHT | wxBOTTOM, FromDIP(16));
    add_choice(page, options, m_padding, _L("Padding"), {{1, _L("On")}, {0, _L("Off")}}, [this](int v) {
        m_options.padding = v == 1;
        schedule_convert();
    });
    add_choice(page, options, m_decoder, _L("Decoder"), {{1, _L("Strict")}, {0, _L("Lenient")}}, [this](int v) {
        m_options.strict = v == 1;
        schedule_convert();
    });
    add_choice(page, options, m_encoding, _L("Text as"),
               {{0, "UTF-8"}, {1, "UTF-16 LE"}, {2, "UTF-16 BE"}, {3, "Latin-1"}}, [this](int v) {
                   static const B64::TextEncoding all[] = {B64::TextEncoding::Utf8, B64::TextEncoding::Utf16LE,
                                                           B64::TextEncoding::Utf16BE, B64::TextEncoding::Latin1};
                   m_options.encoding = all[std::clamp(v, 0, 3)];
                   schedule_convert();
               });
    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    auto *swap = make_button(page, _L("Swap sides"), GlyphSwap);
    swap->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { swap_sides(); });
    auto *clear = make_button(page, _L("Clear"), GlyphClearAll);
    clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { clear_text(); });
    actions->Add(swap, 0, wxRIGHT, FromDIP(4));
    actions->Add(clear, 0);
    options->Add(actions, 0, wxALIGN_BOTTOM | wxBOTTOM, FromDIP(16));
    sizer->Add(options, 0, wxEXPAND | wxTOP, gap);

    // Two columns: what you type, and what it becomes.
    auto *columns = new wxBoxSizer(wxHORIZONTAL);
    const auto column = [&](Label *&title, Label *&counts, TextArea *&area, bool input) {
        auto *col  = new wxBoxSizer(wxVERTICAL);
        auto *head = new wxBoxSizer(wxHORIZONTAL);
        title  = make_label(page, Label::Head_14, wxString());
        counts = make_label(page, Label::Body_12, wxString(), true);
        head->Add(title, 0, wxALIGN_CENTER_VERTICAL);
        head->Add(counts, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
        if (input) {
            auto *paste = make_icon_button(page, GlyphPaste, _L("Paste"));
            paste->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { paste_input(); });
            auto *open = make_icon_button(page, GlyphFileOpen, _L("Open a text file"));
            open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { open_input_file(); });
            head->Add(paste, 0, wxALIGN_CENTER_VERTICAL);
            head->Add(open, 0, wxALIGN_CENTER_VERTICAL);
        } else {
            auto *copy = make_icon_button(page, GlyphCopy, _L("Copy"));
            copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { copy_output(); });
            auto *save = make_icon_button(page, GlyphSave, _L("Save as a file"));
            save->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { save_output(); });
            head->Add(copy, 0, wxALIGN_CENTER_VERTICAL);
            head->Add(save, 0, wxALIGN_CENTER_VERTICAL);
            m_output_actions.push_back(copy);
            m_output_actions.push_back(save);
        }
        col->Add(head, 0, wxEXPAND | wxBOTTOM, FromDIP(6));
        area = new TextArea(page, wxString(), wxSize(-1, FromDIP(220)), input ? 0 : wxTE_READONLY);
        area->SetMinLines(8);
        area->SetMonospace(!input);
        if (!input) area->SetReadOnly(true);
        col->Add(area, 1, wxEXPAND);
        return col;
    };
    columns->Add(column(m_in_title, m_in_counts, m_input, true), 1, wxEXPAND | wxRIGHT, gap / 2);
    columns->Add(column(m_out_title, m_out_counts, m_output, false), 1, wxEXPAND | wxLEFT, gap / 2);
    sizer->Add(columns, 0, wxEXPAND);
    m_output_note = make_label(page, Label::Body_12, wxString(), true, true);
    sizer->Add(m_output_note, 0, wxEXPAND | wxTOP, FromDIP(4));

    m_input->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { schedule_convert(); });
    m_input->SetName(_L("Base64 toolkit input"));
    m_output->SetName(_L("Base64 toolkit result"));

    // The bytes, and the proof the result is right.
    auto *bytes = new wxBoxSizer(wxHORIZONTAL);
    m_bytes_title = make_label(page, Label::Body_12, wxString(), true);
    m_bytes_hex   = make_label(page, Label::Mono_12, wxString(), false, true);
    bytes->Add(m_bytes_title, 0, wxRIGHT, FromDIP(8));
    bytes->Add(m_bytes_hex, 1);
    sizer->Add(bytes, 0, wxEXPAND | wxTOP, gap);

    auto *check = new wxBoxSizer(wxHORIZONTAL);
    m_check_icon = glyph_window(page, GlyphCheck, MD3::Role::Primary);
    m_check_text = make_label(page, Label::Body_13, wxString());
    m_check_hash = make_label(page, Label::Mono_12, wxString(), true);
    check->Add(m_check_icon, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    check->Add(m_check_text, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    check->Add(m_check_hash, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(check, 0, wxTOP, FromDIP(8));

    auto *more = new wxWrapSizer(wxHORIZONTAL);
    auto *as_url = make_button(page, _L("Copy as data: URL"), GlyphLink);
    as_url->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { copy_as_data_url(); });
    auto *record = make_button(page, _L("Export record (JSON)"), GlyphCode);
    record->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { export_record(); });
    auto *send = make_button(page, _L("Use in a data: URL"), GlyphShare);
    send->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { send_to_url_tab(); });
    m_save_bytes = make_button(page, _L("Save bytes as a file"), GlyphSave);
    m_save_bytes->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { save_bytes(); });
    for (Button *b : {as_url, record, send, m_save_bytes}) {
        more->Add(b, 0, wxRIGHT | wxTOP, FromDIP(6));
        m_output_actions.push_back(b);
    }
    sizer->Add(more, 0, wxEXPAND | wxTOP, FromDIP(4));

    // The problem card: exactly where decoding stopped, and what to do.
    m_problem_card = card(page, MD3::Role::ErrorContainer);
    auto *pc       = new wxBoxSizer(wxVERTICAL);
    auto *ph       = new wxBoxSizer(wxHORIZONTAL);
    ph->Add(glyph_window(m_problem_card, GlyphError, MD3::Role::Error), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    m_problem_title = make_label(m_problem_card, Label::Head_14, wxString());
    m_problem_title->SetForegroundColour(role(MD3::Role::OnErrorContainer));
    ph->Add(m_problem_title, 1, wxALIGN_CENTER_VERTICAL);
    m_problem_where = make_label(m_problem_card, Label::Body_13, wxString());
    m_problem_where->SetForegroundColour(role(MD3::Role::OnErrorContainer));
    ph->Add(m_problem_where, 0, wxALIGN_CENTER_VERTICAL);
    pc->Add(ph, 0, wxEXPAND);
    auto *ex = new wxBoxSizer(wxHORIZONTAL);
    m_problem_before = make_label(m_problem_card, Label::Mono_13, wxString());
    m_problem_at     = make_label(m_problem_card, Label::Mono_13, wxString());
    m_problem_after  = make_label(m_problem_card, Label::Mono_13, wxString());
    m_problem_before->SetForegroundColour(role(MD3::Role::OnErrorContainer));
    m_problem_after->SetForegroundColour(role(MD3::Role::OnErrorContainer));
    m_problem_at->SetBackgroundColour(role(MD3::Role::Error));
    m_problem_at->SetForegroundColour(role(MD3::Role::OnError));
    ex->Add(m_problem_before, 0);
    ex->Add(m_problem_at, 0);
    ex->Add(m_problem_after, 0);
    pc->Add(ex, 0, wxTOP, FromDIP(8));
    m_problem_kind = make_label(m_problem_card, Label::Head_13, wxString());
    m_problem_kind->SetForegroundColour(role(MD3::Role::OnErrorContainer));
    pc->Add(m_problem_kind, 0, wxTOP, FromDIP(8));
    m_problem_explain = make_label(m_problem_card, Label::Body_13, wxString(), false, true);
    m_problem_explain->SetForegroundColour(role(MD3::Role::OnErrorContainer));
    pc->Add(m_problem_explain, 0, wxEXPAND | wxTOP, FromDIP(2));
    auto *fixes = new wxWrapSizer(wxHORIZONTAL);
    m_try_lenient = make_button(m_problem_card, _L("Try lenient"), GlyphFix, true);
    m_try_lenient->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { try_lenient(); });
    m_remove_bad = make_button(m_problem_card, _L("Remove the bad character"), GlyphBackspace, true);
    m_remove_bad->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { remove_bad_character(); });
    m_use_utf8 = make_button(m_problem_card, _L("Use UTF-8"), GlyphFix, true);
    m_use_utf8->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        m_options.encoding = B64::TextEncoding::Utf8;
        set_choice(m_encoding, 0);
        convert_now();
    });
    for (Button *b : {m_try_lenient, m_remove_bad, m_use_utf8}) fixes->Add(b, 0, wxRIGHT | wxTOP, FromDIP(6));
    pc->Add(fixes, 0, wxEXPAND);
    auto *never = make_label(m_problem_card, Label::Body_12, _L("Nothing partial is ever shown as complete."), false, true);
    never->SetForegroundColour(role(MD3::Role::OnErrorContainer));
    pc->Add(never, 0, wxEXPAND | wxTOP, FromDIP(8));
    auto *pcp = new wxBoxSizer(wxVERTICAL);
    pcp->Add(pc, 1, wxEXPAND | wxALL, FromDIP(16));
    m_problem_card->SetSizer(pcp);
    sizer->Add(m_problem_card, 0, wxEXPAND | wxTOP, gap);
    m_problem_card->Hide();

    auto *outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(sizer, 1, wxEXPAND | wxBOTTOM, gap);
    page->SetSizer(outer);
    return page;
}

void Base64ToolkitPanel::apply_options_to_controls()
{
    set_choice(m_direction, m_options.encode ? 1 : 0);
    set_choice(m_alphabet, m_options.alphabet == B64::Alphabet::UrlSafe ? 1 : 0);
    set_choice(m_lines, m_options.lines == B64::LineMode::Mime ? 1 : m_options.lines == B64::LineMode::Custom ? 2 : 0);
    m_width->SetValue(B64::clamp_width(m_options.custom_width));
    m_width->Show(m_options.lines == B64::LineMode::Custom);
    set_choice(m_padding, m_options.padding ? 1 : 0);
    set_choice(m_decoder, m_options.strict ? 1 : 0);
    int encoding = 0;
    switch (m_options.encoding) {
    case B64::TextEncoding::Utf16LE: encoding = 1; break;
    case B64::TextEncoding::Utf16BE: encoding = 2; break;
    case B64::TextEncoding::Latin1: encoding = 3; break;
    case B64::TextEncoding::Utf8: break;
    }
    set_choice(m_encoding, encoding);
    if (m_text_page) m_text_page->Layout();
}

void Base64ToolkitPanel::save_options() const
{
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(kOptionsKey, B64::options_json(m_options));
}

void Base64ToolkitPanel::schedule_convert()
{
    if (m_applying_state) return;
    m_convert_timer.StartOnce(120);
}

void Base64ToolkitPanel::convert_now()
{
    m_convert_timer.Stop();
    m_input_u8 = into_u8(m_input->GetValue());
    m_outcome  = B64::run_text(m_input_u8, m_options);
    save_options();
    if (!m_applying_state) m_undo.commit(B64::TextState{m_input_u8, m_options});
    refresh_undo_buttons();
    show_outcome();
}

void Base64ToolkitPanel::show_outcome()
{
    const B64::TextOutcome &o      = m_outcome;
    const bool              encode = m_options.encode;
    const wxString          plain_title = _L("Text");
    const wxString          coded_title = "Base64";
    m_in_title->SetLabel(encode ? plain_title : coded_title);
    m_out_title->SetLabel(encode ? coded_title : plain_title);

    const wxString plain_counts = wxString::Format(_L("%s · %s"), characters_text(encode ? o.input_characters : o.output_characters),
                                                   size_text(o.bytes.size()));
    if (encode) {
        m_in_counts->SetLabel(o.problem == B64::TextProblem::Empty ? wxString() : plain_counts);
        m_out_counts->SetLabel(o.problem == B64::TextProblem::None ? characters_text(o.output_characters) : wxString());
    } else {
        m_in_counts->SetLabel(o.problem == B64::TextProblem::Empty ? wxString() : characters_text(o.input_characters));
        m_out_counts->SetLabel(o.problem == B64::TextProblem::None ? plain_counts :
                               o.problem == B64::TextProblem::OutputText ? size_text(o.bytes.size()) : wxString());
    }

    // The result box shows at most kDisplayLimit characters; Copy and Save use all of it.
    wxString shown;
    bool     clipped = false;
    if (o.problem == B64::TextProblem::None) {
        if (o.output.size() > kDisplayLimit) {
            std::size_t cut = kDisplayLimit;
            while (cut > 0 && (static_cast<unsigned char>(o.output[cut]) & 0xC0) == 0x80) --cut;
            shown   = wxString::FromUTF8(o.output.substr(0, cut));
            clipped = true;
        } else {
            shown = wxString::FromUTF8(o.output);
        }
    }
    m_output->SetValue(shown);
    wxString note;
    if (clipped) note = _L("Showing the first 1 MB of the result. Copy and Save use all of it.");
    else if (o.problem == B64::TextProblem::TooLarge) note = _L("This is more than 16 MB of text. Use the Files tab, which streams files of any size.");
    m_output_note->SetLabel(note);
    m_output_note->Show(!note.IsEmpty());

    const bool has_bytes = !o.bytes.empty();
    m_bytes_title->SetLabel(wxString::Format(_L("Bytes as %s"), encoding_name(m_options.encoding)));
    m_bytes_hex->SetLabel(has_bytes ? wxString::FromUTF8(B64::hex_preview(o.bytes, 48)) : _L("none"));

    const bool ok = o.problem == B64::TextProblem::None;
    m_check_icon->Show(ok);
    if (ok && encode) m_check_text->SetLabel(o.verified ? wxString::Format(_L("Decodes back to the same %s"), size_text(o.bytes.size()))
                                                        : _L("Did not decode back to the same bytes"));
    else if (ok) m_check_text->SetLabel(o.canonical ? wxString::Format(_L("Decoded %s; re-encodes to exactly this text"), size_text(o.bytes.size()))
                                                    : wxString::Format(_L("Decoded %s; the exact form for these options differs"), size_text(o.bytes.size())));
    else if (o.problem == B64::TextProblem::Empty) m_check_text->SetLabel(encode ? _L("Type or paste text to encode.") : _L("Paste Base64 to decode."));
    else m_check_text->SetLabel(wxString());
    m_check_hash->SetLabel(ok || o.problem == B64::TextProblem::OutputText ? "sha256 " + wxString::FromUTF8(B64::short_hash(o.sha256)) : wxString());
    m_check_hash->SetToolTip(wxString::FromUTF8(o.sha256));
    for (Button *b : m_output_actions) b->Enable(ok);
    m_save_bytes->Enable(has_bytes);
    m_save_bytes->Show(!encode);

    // The problem card.
    const bool problem = o.problem == B64::TextProblem::Decode || o.problem == B64::TextProblem::InputEncoding ||
                         o.problem == B64::TextProblem::OutputText;
    m_problem_card->Show(problem);
    m_try_lenient->Hide();
    m_remove_bad->Hide();
    m_use_utf8->Hide();
    if (problem) {
        const auto excerpt = [this](std::size_t offset) {
            const B64::Excerpt e = B64::excerpt_around(m_input_u8, offset, 16);
            m_problem_before->SetLabel(wxString(e.clipped_before ? L"…" : L"") + wxString::FromUTF8(e.before));
            m_problem_at->SetLabel(e.at.empty() ? wxString(L"␠") : wxString::FromUTF8(e.at));
            m_problem_after->SetLabel(wxString::FromUTF8(e.after) + wxString(e.clipped_after ? L"…" : L""));
        };
        if (o.problem == B64::TextProblem::Decode) {
            m_problem_title->SetLabel(m_options.strict ? _L("Strict decode stops at the first problem")
                                                       : _L("Lenient decode stops at this problem"));
            m_problem_where->SetLabel(o.character ? wxString::Format(_L("'%s' at offset %s"), character_text(o.character), number(o.offset))
                                                  : wxString::Format(_L("At the end, offset %s"), number(o.offset)));
            excerpt(o.offset);
            m_problem_kind->SetLabel(error_title(o.decode_error));
            m_problem_explain->SetLabel(error_explanation(o.decode_error));
            m_try_lenient->Show(m_options.strict);
            m_remove_bad->Show(o.decode_error == B64::DecodeError::InvalidCharacter && o.character != 0);
        } else if (o.problem == B64::TextProblem::InputEncoding) {
            m_problem_title->SetLabel(wxString::Format(_L("This text cannot be written as %s"), encoding_name(m_options.encoding)));
            m_problem_where->SetLabel(wxString::Format(_L("'%s' at offset %s"), character_text(o.character), number(o.offset)));
            excerpt(o.offset);
            m_problem_kind->SetLabel(_L("Character outside the text encoding"));
            m_problem_explain->SetLabel(_L("Choose UTF-8 or UTF-16 under Text as, or remove the character."));
            m_use_utf8->Show(m_options.encoding != B64::TextEncoding::Utf8);
            m_remove_bad->Show(true);
        } else {
            m_problem_title->SetLabel(wxString::Format(_L("The decoded bytes are not %s text"), encoding_name(m_options.encoding)));
            m_problem_where->SetLabel(wxString::Format(_L("First problem at byte %s"), number(o.offset)));
            m_problem_before->SetLabel(wxString());
            m_problem_at->SetLabel(wxString());
            m_problem_after->SetLabel(wxString());
            m_problem_kind->SetLabel(_L("Bytes, not text"));
            m_problem_explain->SetLabel(_L("The bytes are kept: the hex above shows them, and Save bytes as a file writes them exactly. Try another Text as choice if this is text."));
        }
        m_problem_explain->Wrap(std::max(FromDIP(240), m_problem_card->GetClientSize().x - FromDIP(32)));
    }
    m_text_page->Layout();
    m_text_page->FitInside();
}

void Base64ToolkitPanel::set_input(const wxString &text)
{
    m_input->SetValue(text);
    convert_now();
}

void Base64ToolkitPanel::swap_sides()
{
    if (m_outcome.problem != B64::TextProblem::None) {
        set_status(_L("Swap sides needs a result first."));
        return;
    }
    const std::string result = m_outcome.output;
    m_options.encode         = !m_options.encode;
    set_choice(m_direction, m_options.encode ? 1 : 0);
    set_input(wxString::FromUTF8(result));
}

void Base64ToolkitPanel::clear_text()
{
    set_input(wxString());
    set_status(_L("Cleared. Undo on the History tab brings it back."));
}

void Base64ToolkitPanel::paste_input()
{
    const wxString text = clipboard_text();
    if (text.IsEmpty()) {
        set_status(_L("The clipboard has no text."));
        return;
    }
    set_input(text);
}

void Base64ToolkitPanel::open_input_file()
{
    wxFileDialog dialog(this, m_options.encode ? _L("Open a text file to encode") : _L("Open a Base64 file to decode"), wxEmptyString,
                        wxEmptyString, m_options.encode ? _L("All files (*.*)|*.*") : _L("Base64 text (*.txt;*.b64;*.base64)|*.txt;*.b64;*.base64|All files (*.*)|*.*"),
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    load_input_file(to_path(dialog.GetPath()));
}

void Base64ToolkitPanel::load_input_file(const fs::path &path)
{
    std::string bytes;
    bool        too_large = false;
    if (!read_limited(path, kFileLimit, bytes, too_large)) {
        set_status(too_large ? _L("That file is over 16 MB. The Files tab encodes and decodes files of any size beside the originals.")
                             : _L("Could not read that file."));
        return;
    }
    std::string utf8;
    if (m_options.encode) {
        std::size_t bad = 0;
        if (!B64::bytes_to_text(B64::Bytes(bytes.begin(), bytes.end()), m_options.encoding, utf8, bad)) {
            set_status(wxString::Format(_L("That file is not %s text (byte %s). The Files tab encodes any file exactly."),
                                        encoding_name(m_options.encoding), number(bad)));
            return;
        }
    } else {
        utf8 = bytes.compare(0, 3, "\xEF\xBB\xBF") == 0 ? bytes.substr(3) : bytes;
    }
    set_input(wxString::FromUTF8(utf8));
    set_status(wxString::Format(_L("Opened %s."), from_u8(path.filename().u8string())));
}

void Base64ToolkitPanel::copy_output()
{
    if (m_outcome.problem != B64::TextProblem::None) return;
    if (!copy_to_clipboard(wxString::FromUTF8(m_outcome.output))) {
        set_status(_L("The clipboard is busy. Try again."));
        return;
    }
    set_status(wxString::Format(_L("Copied %s."), characters_text(m_outcome.output_characters)));
    record_text(into_u8(_L("Copied")));
}

void Base64ToolkitPanel::save_output()
{
    if (m_outcome.problem != B64::TextProblem::None) return;
    wxFileDialog dialog(this, _L("Save the result"), wxEmptyString, m_options.encode ? "encoded.base64.txt" : "decoded.txt",
                        _L("Text files (*.txt)|*.txt|All files (*.*)|*.*"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const fs::path path = to_path(dialog.GetPath());
    if (!write_all(path, m_outcome.output.data(), m_outcome.output.size())) {
        set_status(_L("Could not save there."));
        return;
    }
    set_status(wxString::Format(_L("Saved %s."), from_u8(path.filename().u8string())));
    record_text(path.filename().u8string());
}

void Base64ToolkitPanel::save_bytes()
{
    if (m_outcome.bytes.empty()) return;
    const B64::DetectedType type = B64::detect_type(m_outcome.bytes);
    wxFileDialog dialog(this, _L("Save the decoded bytes"), wxEmptyString, "decoded" + wxString::FromUTF8(type.extension),
                        _L("All files (*.*)|*.*"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const fs::path path = to_path(dialog.GetPath());
    if (!write_all(path, m_outcome.bytes.data(), m_outcome.bytes.size())) {
        set_status(_L("Could not save there."));
        return;
    }
    set_status(wxString::Format(_L("Saved %s, %s. It was not opened."), from_u8(path.filename().u8string()), size_text(m_outcome.bytes.size())));
    record_text(path.filename().u8string());
}

void Base64ToolkitPanel::copy_as_data_url()
{
    if (m_outcome.problem != B64::TextProblem::None) return;
    const std::string url = B64::build_data_url("text/plain", m_outcome.bytes, charset_for(m_options.encoding));
    if (!copy_to_clipboard(wxString::FromUTF8(url))) {
        set_status(_L("The clipboard is busy. Try again."));
        return;
    }
    set_status(wxString::Format(_L("Copied a data: URL, %s."), characters_text(url.size())));
    record_text("data: URL");
}

void Base64ToolkitPanel::export_record()
{
    if (m_outcome.problem != B64::TextProblem::None) return;
    wxFileDialog dialog(this, _L("Export record (JSON)"), wxEmptyString, "base64-record.json", _L("JSON files (*.json)|*.json"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const bool        keep   = m_keep_payloads && m_keep_payloads->GetValue();
    const std::string record = B64::record_json(m_options, m_outcome, now_seconds(), keep ? &m_input_u8 : nullptr);
    const fs::path    path   = to_path(dialog.GetPath());
    if (!write_all(path, record.data(), record.size())) {
        set_status(_L("Could not save there."));
        return;
    }
    set_status(keep ? _L("Exported the record with the text, because Keep payloads is on.")
                    : _L("Exported the record: options, sizes and hashes. The text itself is not in it while Keep payloads is off."));
    record_text(path.filename().u8string());
}

void Base64ToolkitPanel::send_to_url_tab()
{
    if (m_outcome.problem != B64::TextProblem::None) return;
    url_from_text();
    select_tab(TabDataUrl);
}

void Base64ToolkitPanel::try_lenient()
{
    m_options.strict = false;
    set_choice(m_decoder, 0);
    convert_now();
}

void Base64ToolkitPanel::remove_bad_character()
{
    if (m_outcome.problem != B64::TextProblem::Decode && m_outcome.problem != B64::TextProblem::InputEncoding) return;
    set_input(wxString::FromUTF8(B64::remove_at(m_input_u8, m_outcome.offset)));
}

void Base64ToolkitPanel::record_text(const std::string &name)
{
    B64::HistoryEntry e;
    e.time        = now_seconds();
    e.action      = m_options.encode ? "encode-text" : "decode-text";
    e.name        = name;
    e.bytes_in    = m_input_u8.size();
    e.bytes_out   = m_outcome.output.size();
    e.sha256      = m_outcome.sha256;
    e.has_options = true;
    e.options     = m_options;
    e.has_payload = true;
    e.payload     = m_input_u8;
    append_history(std::move(e));
}

void Base64ToolkitPanel::undo()
{
    if (auto state = m_undo.undo()) apply_state(*state);
    else set_status(_L("Nothing to undo on the Text tab."));
}

void Base64ToolkitPanel::redo()
{
    if (auto state = m_undo.redo()) apply_state(*state);
    else set_status(_L("Nothing to redo on the Text tab."));
}

void Base64ToolkitPanel::apply_state(const B64::TextState &state)
{
    m_applying_state = true;
    m_options        = state.options;
    apply_options_to_controls();
    m_input->SetValue(wxString::FromUTF8(state.input));
    convert_now();
    m_applying_state = false;
    m_convert_timer.Stop();
    select_tab(TabText);
}

void Base64ToolkitPanel::refresh_undo_buttons()
{
    if (m_undo_button) m_undo_button->Enable(m_undo.can_undo());
    if (m_redo_button) m_redo_button->Enable(m_undo.can_redo());
}

// ------------------------------------------------------------------- Files tab

wxWindow *Base64ToolkitPanel::build_files_page(wxWindow *parent)
{
    auto *page = new wxPanel(parent, wxID_ANY);
    page->SetBackgroundColour(GetBackgroundColour());
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    const int gap = FromDIP(12);

    auto *head = new wxBoxSizer(wxHORIZONTAL);
    head->Add(make_label(page, Label::Head_16, _L("Files, beside the originals")), 1, wxALIGN_CENTER_VERTICAL);
    m_pause = make_button(page, _L("Pause all"), GlyphPause);
    m_pause->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        bool paused = false;
        if (m_queue) {
            std::lock_guard<std::mutex> lock(m_queue->mutex);
            paused = m_queue->paused;
        }
        pause_all(!paused);
    });
    m_cancel = make_button(page, _L("Cancel all"), GlyphCancel);
    m_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { cancel_all(); });
    head->Add(m_pause, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    head->Add(m_cancel, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(head, 0, wxEXPAND | wxTOP, gap);

    auto *drops = new wxBoxSizer(wxHORIZONTAL);
    const auto drop_card = [&](bool encode) {
        StaticBox *box = card(page, MD3::Role::SurfaceContainerLow);
        auto      *s   = new wxBoxSizer(wxHORIZONTAL);
        s->Add(glyph_window(box, encode ? GlyphUpload : GlyphFileOpen, MD3::Role::Primary, 28), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
        auto *text = new wxBoxSizer(wxVERTICAL);
        auto *button = make_button(box, encode ? _L("Encode files") : _L("Decode files"), 0, true);
        button->Bind(wxEVT_BUTTON, [this, encode](wxCommandEvent &) { choose_files(encode); });
        text->Add(button, 0);
        text->Add(make_label(box, Label::Body_12, encode ? _L("report.pdf becomes report.pdf.base64.txt in the same folder. Or drop files here.")
                                                         : _L("report.pdf.base64.txt or .b64 restores report.pdf beside it. Or drop files here."),
                             true, true),
                  0, wxEXPAND | wxTOP, FromDIP(6));
        s->Add(text, 1, wxALIGN_CENTER_VERTICAL);
        auto *p = new wxBoxSizer(wxVERTICAL);
        p->Add(s, 1, wxEXPAND | wxALL, FromDIP(16));
        box->SetSizer(p);
        const auto accept = [this, encode](std::vector<fs::path> paths) { queue_files(paths, encode); };
        box->SetDropTarget(new PathDropTarget(accept));
        button->SetDropTarget(new PathDropTarget(accept));
        return box;
    };
    drops->Add(drop_card(true), 1, wxEXPAND | wxRIGHT, gap / 2);
    drops->Add(drop_card(false), 1, wxEXPAND | wxLEFT, gap / 2);
    sizer->Add(drops, 0, wxEXPAND | wxTOP, gap);

    m_files_rules = make_label(page, Label::Body_12, wxString(), true, true);
    sizer->Add(m_files_rules, 0, wxEXPAND | wxTOP, gap);

    m_jobs = new MD3DataViewListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(240)), wxDV_ROW_LINES | wxDV_SINGLE);
    m_jobs->AppendTextColumn(_L("Source"), wxDATAVIEW_CELL_INERT, FromDIP(320), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_jobs->AppendTextColumn(_L("Writes"), wxDATAVIEW_CELL_INERT, FromDIP(220), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_jobs->AppendTextColumn(_L("Size"), wxDATAVIEW_CELL_INERT, FromDIP(90), wxALIGN_RIGHT, wxDATAVIEW_COL_RESIZABLE);
    m_jobs->AppendTextColumn(_L("Progress"), wxDATAVIEW_CELL_INERT, FromDIP(150), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_jobs->AppendTextColumn(_L("State"), wxDATAVIEW_CELL_INERT, FromDIP(220), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_jobs->SetName(_L("Base64 file jobs"));
    m_jobs->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent &) { show_selected_in_folder(); });
    sizer->Add(m_jobs, 1, wxEXPAND | wxTOP, gap);
    m_files_empty = make_label(page, Label::Body_13, _L("No files yet. Pick or drop files above; each one is written beside its original."), true, true);
    sizer->Add(m_files_empty, 0, wxEXPAND | wxTOP, FromDIP(6));

    m_jobs->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &) { refresh_selection_buttons(); });
    m_jobs->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU, [this](wxDataViewEvent &) { show_jobs_menu(wxGetMousePosition()); });
    m_jobs->Bind(wxEVT_CONTEXT_MENU, [this](wxContextMenuEvent &e) {
        // Shift+F10 and the Menu key arrive with no position; open the menu by the list.
        const wxPoint at = e.GetPosition() == wxDefaultPosition ? m_jobs->ClientToScreen(wxPoint(FromDIP(24), FromDIP(24))) : e.GetPosition();
        show_jobs_menu(at);
    });

    auto *rows = new wxBoxSizer(wxHORIZONTAL);
    auto *folder = make_button(page, _L("Show in folder"), GlyphFolder);
    folder->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { show_selected_in_folder(); });
    auto *clear = make_button(page, _L("Clear finished"), GlyphClearAll);
    clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { clear_finished(); });
    // One file alone, the selected row; the row's right-click menu offers the same.
    m_pause_one = make_button(page, _L("Pause this file"), GlyphPause);
    m_pause_one->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        bool paused = false;
        if (m_queue) {
            std::lock_guard<std::mutex> lock(m_queue->mutex);
            const int row = m_jobs->GetSelectedRow();
            if (row != wxNOT_FOUND && std::size_t(row) < m_queue->jobs.size()) paused = m_queue->jobs[std::size_t(row)].paused;
        }
        pause_selected(!paused);
    });
    m_cancel_one = make_button(page, _L("Cancel this file"), GlyphCancel);
    m_cancel_one->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { cancel_selected(); });
    rows->Add(folder, 0, wxRIGHT, FromDIP(6));
    rows->Add(clear, 0, wxRIGHT, FromDIP(6));
    rows->Add(m_pause_one, 0, wxRIGHT, FromDIP(6));
    rows->Add(m_cancel_one, 0);
    sizer->Add(rows, 0, wxTOP, FromDIP(8));

    sizer->Add(make_label(page, Label::Body_12,
                          _L("Large files stream in 1 MB chunks with pause, resume and cancel. Each written file is checked before it counts as done: an encoded file is decoded again and its SHA-256 compared with the source. A clash gets a numbered name, so nothing is ever overwritten, and a failed or cancelled file leaves nothing behind."),
                          true, true),
               0, wxEXPAND | wxTOP | wxBOTTOM, gap);
    page->SetSizer(sizer);
    m_pause->Enable(false);
    m_cancel->Enable(false);
    return page;
}

void Base64ToolkitPanel::choose_files(bool encode)
{
    wxFileDialog dialog(this, encode ? _L("Choose files to encode") : _L("Choose Base64 files to decode"), wxEmptyString, wxEmptyString,
                        encode ? _L("All files (*.*)|*.*") : _L("Base64 text (*.txt;*.b64;*.base64)|*.txt;*.b64;*.base64|All files (*.*)|*.*"),
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
    if (dialog.ShowModal() != wxID_OK) return;
    wxArrayString chosen;
    dialog.GetPaths(chosen);
    std::vector<fs::path> paths;
    for (const wxString &p : chosen) paths.push_back(to_path(p));
    queue_files(paths, encode);
}

void Base64ToolkitPanel::queue_files(const std::vector<fs::path> &paths, bool encode)
{
    if (paths.empty()) return;
    ensure_worker();
    std::size_t added = 0, skipped = 0;
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        for (const fs::path &p : paths) {
            std::error_code ec;
            if (!fs::is_regular_file(p, ec)) {
                ++skipped;
                continue;
            }
            FileJob job;
            job.id             = m_queue->next_id++;
            job.source         = p;
            job.encode         = encode;
            job.encode_options = B64::encode_options(m_options);
            // Files are decoded leniently: line breaks and missing padding are fine.
            job.decode_options          = B64::decode_options(m_options);
            job.decode_options.strict   = false;
            job.total                   = fs::file_size(p, ec);
            m_queue->jobs.push_back(std::move(job));
            ++added;
        }
    }
    m_queue->wake.notify_all();
    if (skipped) set_status(wxString::Format(_L("Added %s files. %s were folders or missing and were skipped."), number(added), number(skipped)));
    else set_status(wxString::Format(encode ? _L("Encoding %s files beside the originals.") : _L("Decoding %s files beside the originals."), number(added)));
    select_tab(TabFiles);
    poll_jobs();
    m_job_timer.Start(250);
}

void Base64ToolkitPanel::ensure_worker()
{
    if (!m_queue) m_queue = std::make_shared<FileQueue>();
    if (!m_worker.joinable()) m_worker = std::thread(&Base64ToolkitPanel::worker_loop, m_queue);
}

void Base64ToolkitPanel::stop_worker()
{
    if (!m_queue) return;
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        m_queue->stop = true;
    }
    m_queue->wake.notify_all();
    if (m_worker.joinable()) m_worker.join();
}

void Base64ToolkitPanel::poll_jobs()
{
    if (!m_queue) return;
    std::vector<FileJob> snapshot;
    bool                 paused = false;
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        snapshot = m_queue->jobs;
        paused   = m_queue->paused;
        for (FileJob &job : m_queue->jobs)
            if (job.status == FileJob::Status::Done || job.status == FileJob::Status::Failed || job.status == FileJob::Status::Cancelled)
                job.recorded = true;
    }
    const auto now    = std::chrono::steady_clock::now();
    bool       active = false;
    if (std::size_t(m_jobs->GetItemCount()) != snapshot.size()) {
        m_jobs->DeleteAllItems();
        for (std::size_t i = 0; i < snapshot.size(); ++i) {
            wxVector<wxVariant> row;
            for (int c = 0; c < 5; ++c) row.push_back(wxVariant(wxString()));
            m_jobs->AppendItem(row);
        }
    }
    for (std::size_t i = 0; i < snapshot.size(); ++i) {
        const FileJob &job = snapshot[i];
        wxString       writes, progress, state;
        switch (job.status) {
        case FileJob::Status::Queued:
            active   = true;
            writes   = job.encode ? from_u8(B64::encoded_path_for(job.source).filename().u8string()) : _L("Named when done");
            progress = paused ? _L("Paused") : job.paused ? _L("Paused, this file") : _L("Waiting");
            state    = job.encode ? _L("Encode") : _L("Decode");
            break;
        case FileJob::Status::Running: {
            active   = true;
            writes   = job.encode ? from_u8(B64::encoded_path_for(job.source).filename().u8string()) : _L("Named when done");
            const int pct = B64::percent(job.done, job.total);
            if (paused || job.paused) {
                progress = wxString::Format(_L("Paused at %d %%"), pct);
                state    = paused ? _L("Paused") : _L("Paused, this file");
            } else {
                const double elapsed = std::chrono::duration<double>(now - job.started).count() - job.paused_seconds;
                const auto   left    = B64::seconds_left(job.done, job.total, elapsed);
                progress = left ? wxString::Format(_L("%d %% · %s s left"), pct, number(std::uint64_t(*left))) : wxString::Format("%d %%", pct);
                state    = job.encode ? _L("Encoding now") : _L("Decoding now");
            }
            break;
        }
        case FileJob::Status::Done:
            writes   = from_u8(job.result.output.filename().u8string());
            progress = "100 %";
            state    = job.result.renamed ? _L("Verified, renamed") : _L("Verified");
            break;
        case FileJob::Status::Failed:
            writes   = wxString();
            progress = wxString();
            state    = file_problem(job.result);
            break;
        case FileJob::Status::Cancelled:
            writes   = wxString();
            progress = wxString();
            state    = _L("Cancelled, nothing written");
            break;
        }
        const unsigned row = unsigned(i);
        const wxString cells[5] = {from_u8(job.source.u8string()), writes, size_text(job.total), progress, state};
        for (unsigned c = 0; c < 5; ++c)
            if (m_jobs->GetTextValue(row, c) != cells[c]) m_jobs->SetTextValue(cells[c], row, c);

        if (!job.recorded && job.status == FileJob::Status::Done) {
            B64::HistoryEntry e;
            e.time      = now_seconds();
            e.action    = job.encode ? "encode-file" : "decode-file";
            e.name      = job.source.filename().u8string();
            e.bytes_in  = job.result.bytes_in;
            e.bytes_out = job.result.bytes_out;
            e.sha256    = job.result.sha256;
            append_history(std::move(e));
        }
    }
    m_files_empty->Show(snapshot.empty());
    m_pause->Enable(active);
    m_cancel->Enable(active);
    m_pause->SetLabel(paused ? _L("Resume all") : _L("Pause all"));
    m_pause->SetGlyph(paused ? GlyphPlay : GlyphPause);
    refresh_selection_buttons();
    m_files_rules->SetLabel(wxString::Format(
        _L("Files use the Text tab's alphabet (%s), lines (%s) and padding (%s). Files are always decoded leniently: line breaks and missing padding are accepted, any other character stops that file."),
        m_options.alphabet == B64::Alphabet::UrlSafe ? _L("URL-safe") : _L("Standard"),
        m_options.lines == B64::LineMode::Mime ? _L("MIME 76, CRLF") : m_options.lines == B64::LineMode::Custom ? wxString::Format(_L("%d per line"), B64::clamp_width(m_options.custom_width)) : _L("One line"),
        m_options.padding ? _L("On") : _L("Off")));
    if (!active) m_job_timer.Stop();
    refresh_tab_labels();
    if (auto *page = page_at(TabFiles)) page->Layout();
}

void Base64ToolkitPanel::pause_all(bool pause)
{
    if (!m_queue) return;
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        if (m_queue->paused == pause) return;
        const auto now = std::chrono::steady_clock::now();
        if (pause) m_queue->paused_at = now;
        else
            for (FileJob &job : m_queue->jobs)
                if (job.status == FileJob::Status::Running) job.paused_seconds += std::chrono::duration<double>(now - m_queue->paused_at).count();
        m_queue->paused = pause;
    }
    m_queue->wake.notify_all();
    set_status(pause ? _L("Paused. The current file stops at the next 1 MB chunk.") : _L("Resumed."));
    poll_jobs();
}

void Base64ToolkitPanel::cancel_all()
{
    if (!m_queue) return;
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        for (FileJob &job : m_queue->jobs) {
            if (job.status == FileJob::Status::Queued) {
                job.status          = FileJob::Status::Cancelled;
                job.result.problem  = "cancelled";
                job.result.cancelled = true;
            } else if (job.status == FileJob::Status::Running) {
                job.cancel = true;
            }
        }
    }
    m_queue->wake.notify_all();
    set_status(_L("Cancelled. Partly written files are removed."));
    poll_jobs();
    m_job_timer.Start(250);
}

void Base64ToolkitPanel::refresh_selection_buttons()
{
    if (!m_pause_one || !m_cancel_one || !m_queue) return;
    bool live = false, paused = false;
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        const int row = m_jobs->GetSelectedRow();
        if (row != wxNOT_FOUND && std::size_t(row) < m_queue->jobs.size()) {
            const FileJob &job = m_queue->jobs[std::size_t(row)];
            live   = job.status == FileJob::Status::Queued || job.status == FileJob::Status::Running;
            paused = job.paused;
        }
    }
    m_pause_one->Enable(live);
    m_cancel_one->Enable(live);
    m_pause_one->SetLabel(paused ? _L("Resume this file") : _L("Pause this file"));
    m_pause_one->SetGlyph(paused ? GlyphPlay : GlyphPause);
}

void Base64ToolkitPanel::pause_selected(bool pause)
{
    if (!m_queue) return;
    const int row = m_jobs->GetSelectedRow();
    if (row == wxNOT_FOUND) {
        set_status(_L("Select a file in the list first."));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        if (std::size_t(row) >= m_queue->jobs.size()) return;
        FileJob &job = m_queue->jobs[std::size_t(row)];
        if (job.status != FileJob::Status::Queued && job.status != FileJob::Status::Running) return;
        if (job.paused == pause) return;
        const auto now = std::chrono::steady_clock::now();
        if (pause) job.paused_at = now;
        else if (job.status == FileJob::Status::Running && !m_queue->paused)
            job.paused_seconds += std::chrono::duration<double>(now - job.paused_at).count();
        job.paused = pause;
    }
    m_queue->wake.notify_all();
    set_status(pause ? _L("This file waits where it is; the others go on.") : _L("This file goes on."));
    poll_jobs();
    m_job_timer.Start(250);
}

void Base64ToolkitPanel::cancel_selected()
{
    if (!m_queue) return;
    const int row = m_jobs->GetSelectedRow();
    if (row == wxNOT_FOUND) {
        set_status(_L("Select a file in the list first."));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        if (std::size_t(row) >= m_queue->jobs.size()) return;
        FileJob &job = m_queue->jobs[std::size_t(row)];
        if (job.status == FileJob::Status::Queued) {
            job.status           = FileJob::Status::Cancelled;
            job.result.problem   = "cancelled";
            job.result.cancelled = true;
        } else if (job.status == FileJob::Status::Running) {
            job.cancel = true;
            job.paused = false; // a paused file must wake to notice the cancel
        } else {
            return;
        }
    }
    m_queue->wake.notify_all();
    set_status(_L("This file is cancelled; anything partly written is removed. The others go on."));
    poll_jobs();
    m_job_timer.Start(250);
}

void Base64ToolkitPanel::show_jobs_menu(const wxPoint &screen)
{
    if (!m_queue) return;
    bool live = false, paused = false, done = false;
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        const int row = m_jobs->GetSelectedRow();
        if (row != wxNOT_FOUND && std::size_t(row) < m_queue->jobs.size()) {
            const FileJob &job = m_queue->jobs[std::size_t(row)];
            live   = job.status == FileJob::Status::Queued || job.status == FileJob::Status::Running;
            paused = job.paused;
            done   = job.status == FileJob::Status::Done;
        }
    }
    wxMenu    menu;
    const int pause_id  = wxWindow::NewControlId();
    const int cancel_id = wxWindow::NewControlId();
    const int folder_id = wxWindow::NewControlId();
    const int clear_id  = wxWindow::NewControlId();
    menu.Append(pause_id, paused ? _L("Resume this file") : _L("Pause this file"))->Enable(live);
    menu.Append(cancel_id, _L("Cancel this file"))->Enable(live);
    menu.AppendSeparator();
    menu.Append(folder_id, done ? _L("Show the written file in its folder") : _L("Show in folder"));
    menu.Append(clear_id, _L("Clear finished"));
    const int chosen = MD3::PopupMenuSelection(this, menu, screen);
    if (chosen == pause_id) pause_selected(!paused);
    else if (chosen == cancel_id) cancel_selected();
    else if (chosen == folder_id) show_selected_in_folder();
    else if (chosen == clear_id) clear_finished();
}

void Base64ToolkitPanel::clear_finished()
{
    if (!m_queue) return;
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        auto &jobs = m_queue->jobs;
        jobs.erase(std::remove_if(jobs.begin(), jobs.end(), [](const FileJob &j) {
                       return j.status == FileJob::Status::Done || j.status == FileJob::Status::Failed || j.status == FileJob::Status::Cancelled;
                   }),
                   jobs.end());
    }
    poll_jobs();
}

void Base64ToolkitPanel::show_selected_in_folder()
{
    if (!m_queue) return;
    const int row = m_jobs->GetSelectedRow();
    if (row == wxNOT_FOUND) {
        set_status(_L("Select a file in the list first."));
        return;
    }
    fs::path target;
    {
        std::lock_guard<std::mutex> lock(m_queue->mutex);
        if (std::size_t(row) >= m_queue->jobs.size()) return;
        const FileJob &job = m_queue->jobs[std::size_t(row)];
        target = job.status == FileJob::Status::Done ? job.result.output : job.source;
    }
    desktop_open_any_folder(target.u8string());
}

// ------------------------------------------------------------------- data: URL tab

wxWindow *Base64ToolkitPanel::build_url_page(wxWindow *parent)
{
    auto *page = new MD3ScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxBORDER_NONE);
    page->SetBackgroundColour(GetBackgroundColour());
    page->SetScrollRate(0, FromDIP(24));
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    const int gap = FromDIP(12);

    auto *modes = new wxBoxSizer(wxHORIZONTAL);
    add_choice(page, modes, m_url_mode, _L("data: URL"), {{0, _L("Build a URL")}, {1, _L("Read a URL")}}, [this](int v) { url_set_mode(v); });
    sizer->Add(modes, 0, wxTOP, gap);

    m_url_book = new wxSimplebook(page, wxID_ANY);
    m_url_book->SetBackgroundColour(GetBackgroundColour());

    // Build
    auto *build = new wxPanel(m_url_book, wxID_ANY);
    build->SetBackgroundColour(GetBackgroundColour());
    auto *bs   = new wxBoxSizer(wxVERTICAL);
    auto *from = new wxBoxSizer(wxHORIZONTAL);
    auto *from_file = make_button(build, _L("From a file"), GlyphFileOpen, true);
    from_file->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { url_from_file(); });
    auto *from_text = make_button(build, _L("From the Text tab"), GlyphShare);
    from_text->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { url_from_text(); });
    from->Add(from_file, 0, wxRIGHT, FromDIP(6));
    from->Add(from_text, 0, wxRIGHT, FromDIP(12));
    m_url_source = make_label(build, Label::Body_13, _L("Nothing chosen yet."), true);
    from->Add(m_url_source, 1, wxALIGN_CENTER_VERTICAL);
    bs->Add(from, 0, wxEXPAND);
    auto *fields = new wxBoxSizer(wxHORIZONTAL);
    m_url_mime    = new TextInput(build, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(260), -1));
    m_url_charset = new TextInput(build, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(160), -1));
    m_url_mime->SetName(_L("MIME type"));
    m_url_charset->SetName(_L("Charset"));
    auto *mime_col = new wxBoxSizer(wxVERTICAL);
    mime_col->Add(make_label(build, Label::Body_12, _L("MIME type"), true), 0, wxBOTTOM, FromDIP(4));
    mime_col->Add(m_url_mime, 0);
    auto *charset_col = new wxBoxSizer(wxVERTICAL);
    charset_col->Add(make_label(build, Label::Body_12, _L("Charset (text only)"), true), 0, wxBOTTOM, FromDIP(4));
    charset_col->Add(m_url_charset, 0);
    fields->Add(mime_col, 0, wxRIGHT, gap);
    fields->Add(charset_col, 0);
    bs->Add(fields, 0, wxTOP, gap);
    m_url_mime->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { url_rebuild(); });
    m_url_charset->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { url_rebuild(); });
    m_url_built = new TextArea(build, wxString(), wxSize(-1, FromDIP(160)), wxTE_READONLY);
    m_url_built->SetMonospace(true);
    m_url_built->SetReadOnly(true);
    m_url_built->SetMinLines(6);
    m_url_built->SetName(_L("Built data: URL"));
    bs->Add(m_url_built, 0, wxEXPAND | wxTOP, gap);
    m_url_built_info = make_label(build, Label::Body_12, wxString(), true, true);
    bs->Add(m_url_built_info, 0, wxEXPAND | wxTOP, FromDIP(4));
    auto *built_actions = new wxBoxSizer(wxHORIZONTAL);
    auto *copy = make_button(build, _L("Copy"), GlyphCopy);
    copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { url_copy(); });
    auto *save = make_button(build, _L("Save as text"), GlyphSave);
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { url_save_text(); });
    built_actions->Add(copy, 0, wxRIGHT, FromDIP(6));
    built_actions->Add(save, 0);
    bs->Add(built_actions, 0, wxTOP, FromDIP(8));
    build->SetSizer(bs);
    build->SetDropTarget(new PathDropTarget([this](std::vector<fs::path> paths) {
        if (paths.empty()) return;
        std::string bytes;
        bool        too_large = false;
        if (!read_limited(paths.front(), kFileLimit, bytes, too_large)) {
            set_status(too_large ? _L("A data: URL is built from 16 MB at most.") : _L("Could not read that file."));
            return;
        }
        m_url_source_bytes.assign(bytes.begin(), bytes.end());
        m_url_source_name = paths.front().filename().u8string();
        const B64::DetectedType type = B64::detect_type(m_url_source_bytes);
        m_url_mime->GetTextCtrl()->SetValue(wxString::FromUTF8(type.mime));
        m_url_charset->GetTextCtrl()->SetValue(type.preview == B64::DetectedType::Preview::Text ? "utf-8" : "");
        url_rebuild();
    }));

    // Read
    auto *read = new wxPanel(m_url_book, wxID_ANY);
    read->SetBackgroundColour(GetBackgroundColour());
    auto *rs   = new wxBoxSizer(wxVERTICAL);
    auto *rh   = new wxBoxSizer(wxHORIZONTAL);
    rh->Add(make_label(read, Label::Body_13, _L("Paste a data: URL to read it. Nothing in it is opened or run."), true, true), 1,
            wxALIGN_CENTER_VERTICAL);
    auto *paste = make_icon_button(read, GlyphPaste, _L("Paste"));
    paste->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        const wxString text = clipboard_text();
        if (text.IsEmpty()) set_status(_L("The clipboard has no text."));
        else m_url_input->SetValue(text);
    });
    rh->Add(paste, 0, wxALIGN_CENTER_VERTICAL);
    rs->Add(rh, 0, wxEXPAND);
    m_url_input = new TextArea(read, wxString(), wxSize(-1, FromDIP(120)));
    m_url_input->SetMonospace(true);
    m_url_input->SetMinLines(4);
    m_url_input->SetName(_L("data: URL to read"));
    m_url_input->GetTextCtrl()->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { m_url_timer.StartOnce(150); });
    rs->Add(m_url_input, 0, wxEXPAND | wxTOP, FromDIP(6));
    m_url_details = make_label(read, Label::Body_13, wxString(), false, true);
    rs->Add(m_url_details, 0, wxEXPAND | wxTOP, gap);
    m_url_signature = make_label(read, Label::Body_13, wxString(), true, true);
    rs->Add(m_url_signature, 0, wxEXPAND | wxTOP, FromDIP(4));
    auto *pr = new wxBoxSizer(wxHORIZONTAL);
    add_choice(read, pr, m_url_preview, _L("Preview"), {{0, _L("Image")}, {1, _L("Text")}, {2, _L("Hex")}, {3, _L("Audio")}, {4, _L("Video")}},
               [this](int v) { url_show_preview(v); });
    rs->Add(pr, 0, wxTOP, gap);
    m_url_image = new wxStaticBitmap(read, wxID_ANY, wxNullBitmap);
    rs->Add(m_url_image, 0, wxTOP, FromDIP(4));
#if wxUSE_MEDIACTRL
    // The player is created once, hidden, and given a temporary file to play
    // when an audio or video preview is chosen. It never loads the URL itself.
    auto *media = new wxMediaCtrl();
    if (media->Create(read, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(FromDIP(320), FromDIP(180)), 0)) {
        m_url_media = media;
        media->Bind(wxEVT_MEDIA_LOADED, [this](wxMediaEvent &) { media_refresh(); });
        media->Bind(wxEVT_MEDIA_FINISHED, [this](wxMediaEvent &) { media_refresh(); });
        media->Bind(wxEVT_MEDIA_STATECHANGED, [this](wxMediaEvent &) { media_refresh(); });
        media->Hide();
        rs->Add(media, 0, wxTOP, FromDIP(4));
    } else {
        media->Destroy();
    }
#endif
    auto *media_row = new wxPanel(read, wxID_ANY);
    media_row->SetBackgroundColour(read->GetBackgroundColour());
    auto *mr = new wxBoxSizer(wxHORIZONTAL);
    m_media_play = make_button(media_row, _L("Play"), GlyphPlay, true);
    m_media_play->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { media_toggle(); });
    m_media_stop = make_button(media_row, _L("Stop"), GlyphCancel);
    m_media_stop->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
#if wxUSE_MEDIACTRL
        if (auto *mc = dynamic_cast<wxMediaCtrl *>(m_url_media)) mc->Stop();
#endif
        media_refresh();
    });
    m_media_info = make_label(media_row, Label::Body_12, wxString(), true, true);
    mr->Add(m_media_play, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    mr->Add(m_media_stop, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    mr->Add(m_media_info, 1, wxALIGN_CENTER_VERTICAL);
    media_row->SetSizer(mr);
    m_url_media_row = media_row;
    media_row->Hide();
    rs->Add(media_row, 0, wxEXPAND | wxTOP, FromDIP(4));
    m_media_timer.Bind(wxEVT_TIMER, [this](wxTimerEvent &) { media_refresh(); });
    m_url_image_info = make_label(read, Label::Body_12, wxString(), true, true);
    rs->Add(m_url_image_info, 0, wxEXPAND | wxTOP, FromDIP(4));
    m_url_preview_text = new TextArea(read, wxString(), wxSize(-1, FromDIP(200)), wxTE_READONLY);
    m_url_preview_text->SetMonospace(true);
    m_url_preview_text->SetReadOnly(true);
    m_url_preview_text->SetMinLines(8);
    rs->Add(m_url_preview_text, 0, wxEXPAND | wxTOP, FromDIP(4));
    m_url_preview_note = make_label(read, Label::Body_12, wxString(), true, true);
    rs->Add(m_url_preview_note, 0, wxEXPAND | wxTOP, FromDIP(4));
    m_url_save = make_button(read, _L("Save bytes as a file"), GlyphSave, true);
    m_url_save->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { url_save_bytes(); });
    rs->Add(m_url_save, 0, wxTOP, FromDIP(8));
    rs->Add(make_label(read, Label::Body_12,
                       _L("Previews never run anything: images are drawn from their pixels, text is shown as plain text, and audio or video is played from a temporary file by the system player, which is removed when the preview changes. Text over 2 MB and images, audio or video over 64 MB show a size notice instead of a partial preview."),
                       true, true),
            0, wxEXPAND | wxTOP | wxBOTTOM, gap);
    read->SetSizer(rs);

    m_url_book->AddPage(build, "build");
    m_url_book->AddPage(read, "read");
    sizer->Add(m_url_book, 1, wxEXPAND | wxTOP, FromDIP(4));
    page->SetSizer(sizer);
    set_choice(m_url_mode, 0);
    url_read();
    return page;
}

wxWindow *Base64ToolkitPanel::page_at(int tab) const
{
    return m_book && m_book->GetPageCount() > std::size_t(tab) ? m_book->GetPage(std::size_t(tab)) : nullptr;
}

void Base64ToolkitPanel::url_set_mode(int mode)
{
    m_url_book->ChangeSelection(mode == 1 ? 1 : 0);
    if (auto *page = page_at(TabDataUrl)) page->Layout();
}

void Base64ToolkitPanel::url_from_file()
{
    wxFileDialog dialog(this, _L("Choose a file for the data: URL"), wxEmptyString, wxEmptyString, _L("All files (*.*)|*.*"),
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    std::string bytes;
    bool        too_large = false;
    const fs::path path   = to_path(dialog.GetPath());
    if (!read_limited(path, kFileLimit, bytes, too_large)) {
        set_status(too_large ? _L("A data: URL is built from 16 MB at most.") : _L("Could not read that file."));
        return;
    }
    m_url_source_bytes.assign(bytes.begin(), bytes.end());
    m_url_source_name = path.filename().u8string();
    const B64::DetectedType type = B64::detect_type(m_url_source_bytes);
    m_url_mime->GetTextCtrl()->SetValue(wxString::FromUTF8(type.mime));
    m_url_charset->GetTextCtrl()->SetValue(type.preview == B64::DetectedType::Preview::Text ? "utf-8" : "");
    url_rebuild();
}

void Base64ToolkitPanel::url_from_text()
{
    if (m_outcome.problem != B64::TextProblem::None && m_outcome.problem != B64::TextProblem::OutputText) {
        set_status(_L("The Text tab has no result to use yet."));
        return;
    }
    m_url_source_bytes = m_outcome.bytes;
    m_url_source_name  = into_u8(_L("Text tab"));
    const bool text    = m_outcome.problem == B64::TextProblem::None;
    m_url_mime->GetTextCtrl()->SetValue(text ? wxString("text/plain") : wxString::FromUTF8(B64::detect_type(m_url_source_bytes).mime));
    m_url_charset->GetTextCtrl()->SetValue(text ? wxString::FromUTF8(charset_for(m_options.encoding)) : wxString());
    set_choice(m_url_mode, 0);
    url_set_mode(0);
    url_rebuild();
}

void Base64ToolkitPanel::url_rebuild()
{
    if (!m_url_built) return;
    if (m_url_source_name.empty()) {
        m_url_source->SetLabel(_L("Nothing chosen yet."));
        m_url_built->SetValue(wxString());
        m_url_built_info->SetLabel(_L("Choose a file or use the Text tab. The MIME type is detected from the bytes and can be changed."));
        return;
    }
    const std::string       mime    = into_u8(m_url_mime->GetTextCtrl()->GetValue().Strip(wxString::both));
    const std::string       charset = into_u8(m_url_charset->GetTextCtrl()->GetValue().Strip(wxString::both));
    const B64::DetectedType type    = B64::detect_type(m_url_source_bytes);
    const std::string       url     = B64::build_data_url(mime.empty() ? type.mime : mime, m_url_source_bytes, charset);
    m_url_source->SetLabel(wxString::Format(_L("%s, %s, detected as %s"), from_u8(m_url_source_name), size_text(m_url_source_bytes.size()),
                                            wxString::FromUTF8(type.label)));
    m_url_built->SetValue(url.size() > kDisplayLimit ? wxString::FromUTF8(url.substr(0, kDisplayLimit)) + L"…" : wxString::FromUTF8(url));
    m_url_built_info->SetLabel(wxString::Format(_L("%s. Copy and Save use the whole URL."), characters_text(url.size())));
}

void Base64ToolkitPanel::url_copy()
{
    if (m_url_source_name.empty()) return;
    const std::string mime = into_u8(m_url_mime->GetTextCtrl()->GetValue().Strip(wxString::both));
    const std::string url  = B64::build_data_url(mime.empty() ? B64::detect_type(m_url_source_bytes).mime : mime, m_url_source_bytes,
                                                 into_u8(m_url_charset->GetTextCtrl()->GetValue().Strip(wxString::both)));
    if (!copy_to_clipboard(wxString::FromUTF8(url))) {
        set_status(_L("The clipboard is busy. Try again."));
        return;
    }
    set_status(wxString::Format(_L("Copied a data: URL, %s."), characters_text(url.size())));
    record_built_url(mime.empty() ? B64::detect_type(m_url_source_bytes).mime : mime, url.size());
}

void Base64ToolkitPanel::record_built_url(const std::string &mime, std::size_t url_size)
{
    B64::HistoryEntry e;
    e.time      = now_seconds();
    e.action    = "build-url";
    e.name      = mime;
    e.bytes_in  = m_url_source_bytes.size();
    e.bytes_out = url_size;
    e.sha256    = B64::sha256_hex(m_url_source_bytes.data(), m_url_source_bytes.size());
    append_history(std::move(e));
}

void Base64ToolkitPanel::url_save_text()
{
    if (m_url_source_name.empty()) return;
    wxFileDialog dialog(this, _L("Save the data: URL"), wxEmptyString, "data-url.txt", _L("Text files (*.txt)|*.txt"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const std::string mime = into_u8(m_url_mime->GetTextCtrl()->GetValue().Strip(wxString::both));
    const std::string url  = B64::build_data_url(mime.empty() ? B64::detect_type(m_url_source_bytes).mime : mime, m_url_source_bytes,
                                                 into_u8(m_url_charset->GetTextCtrl()->GetValue().Strip(wxString::both)));
    if (!write_all(to_path(dialog.GetPath()), url.data(), url.size())) {
        set_status(_L("Could not save there."));
        return;
    }
    set_status(_L("Saved the data: URL."));
    record_built_url(mime.empty() ? B64::detect_type(m_url_source_bytes).mime : mime, url.size());
}

void Base64ToolkitPanel::url_read()
{
    m_url_timer.Stop();
    if (!m_url_input) return;
    const std::string text = into_u8(m_url_input->GetValue().Strip(wxString::both));
    m_url_parsed_ok        = false;
    m_url_parsed           = B64::DataUrl{};
    std::string error;
    if (text.empty()) {
        m_url_details->SetLabel(_L("Type, size, parameters and a preview appear here."));
        m_url_signature->SetLabel(wxString());
    } else if (!B64::parse_data_url(text, m_url_parsed, error)) {
        m_url_details->SetLabel(wxString::Format(_L("Not a data: URL the toolkit can read: %s"), wxString::FromUTF8(error)));
        m_url_signature->SetLabel(wxString());
    } else {
        m_url_parsed_ok = true;
        const B64::DetectedType type = B64::detect_type(m_url_parsed.bytes);
        wxString params;
        for (const auto &[k, v] : m_url_parsed.parameters) params << (params.IsEmpty() ? "" : ", ") << wxString::FromUTF8(k) << "=" << wxString::FromUTF8(v);
        m_url_details->SetLabel(wxString::Format(_L("Type %s · MIME %s · %s · Parameters: %s · %s"),
                                                 wxString::FromUTF8(type.label), wxString::FromUTF8(m_url_parsed.mime),
                                                 size_text(m_url_parsed.bytes.size()), params.IsEmpty() ? _L("none") : params,
                                                 m_url_parsed.base64 ? _L("Base64") : _L("percent-encoded")));
        const std::string declared = m_url_parsed.mime.substr(0, m_url_parsed.mime.find(';'));
        if (type.mime == declared) m_url_signature->SetLabel(wxString::Format(_L("Signature: %s, matches the type."), wxString::FromUTF8(type.label)));
        else if (type.mime == "application/octet-stream")
            m_url_signature->SetLabel(_L("Signature: no known signature in the bytes, so the type cannot be confirmed."));
        else
            m_url_signature->SetLabel(wxString::Format(_L("Signature: the bytes look like %s, not %s. Treat it with care."),
                                                       wxString::FromUTF8(type.label), wxString::FromUTF8(declared)));
        // One history row per distinct URL read, not one per keystroke.
        const std::string digest = B64::sha256_hex(m_url_parsed.bytes.data(), m_url_parsed.bytes.size());
        if (digest + declared != m_url_last_read) {
            m_url_last_read = digest + declared;
            B64::HistoryEntry e;
            e.time      = now_seconds();
            e.action    = "read-url";
            e.name      = declared;
            e.bytes_in  = text.size();
            e.bytes_out = m_url_parsed.bytes.size();
            e.sha256    = digest;
            append_history(std::move(e));
        }
    }
    const B64::DetectedType type = B64::detect_type(m_url_parsed.bytes);
    const bool image = m_url_parsed_ok && type.preview == B64::DetectedType::Preview::Image;
    const bool textual = m_url_parsed_ok && (type.preview == B64::DetectedType::Preview::Text || m_url_parsed.mime.rfind("text/", 0) == 0);
    const bool player = m_url_media != nullptr;
    const bool audio = m_url_parsed_ok && player && (type.preview == B64::DetectedType::Preview::Audio || m_url_parsed.mime.rfind("audio/", 0) == 0);
    const bool video = m_url_parsed_ok && player && (type.preview == B64::DetectedType::Preview::Video || m_url_parsed.mime.rfind("video/", 0) == 0);
    for (auto &[v, button] : m_url_preview.buttons)
        button->Enable(m_url_parsed_ok && (v == 2 || (v == 0 && image) || (v == 1 && textual) || (v == 3 && audio) || (v == 4 && video)));
    const int kind = image ? 0 : textual ? 1 : audio ? 3 : video ? 4 : 2;
    set_choice(m_url_preview, kind);
    url_show_preview(m_url_parsed_ok ? kind : -1);
    m_url_save->Enable(m_url_parsed_ok);
}

void Base64ToolkitPanel::url_show_preview(int kind)
{
    const B64::Bytes &bytes = m_url_parsed.bytes;
    m_url_image->Hide();
    m_url_image_info->Hide();
    m_url_preview_text->Hide();
    m_url_preview_note->SetLabel(wxString());
    media_unload();
    if (kind == 3 || kind == 4) {
        if (bytes.size() > kImagePreview) {
            m_url_preview_note->SetLabel(wxString::Format(_L("This %s is %s, over the 64 MB preview limit. Save it to play it."),
                                                          kind == 3 ? _L("audio") : _L("video"), size_text(bytes.size())));
        } else {
            std::string ext = B64::extension_for_mime(m_url_parsed.mime);
            if (ext.empty()) ext = B64::detect_type(bytes).extension;
            media_load(bytes, ext.empty() ? ".bin" : ext);
        }
    } else if (kind == 0) {
        if (bytes.size() > kImagePreview) {
            m_url_preview_note->SetLabel(wxString::Format(_L("This image is %s, over the 64 MB preview limit. Save it to view it."), size_text(bytes.size())));
        } else {
            wxLogNull            quiet;
            wxMemoryInputStream  stream(bytes.data(), bytes.size());
            wxImage              img;
            if (img.LoadFile(stream, wxBITMAP_TYPE_ANY) && img.IsOk()) {
                const int    w = img.GetWidth(), h = img.GetHeight();
                const int    box = FromDIP(240);
                const double scale = std::min(1.0, std::min(double(box) / std::max(1, w), double(box) / std::max(1, h)));
                wxImage      shown = scale < 1.0 ? img.Scale(std::max(1, int(w * scale)), std::max(1, int(h * scale)), wxIMAGE_QUALITY_HIGH) : img;
                m_url_image->SetBitmap(wxBitmap(shown));
                m_url_image->Show();
                m_url_image_info->SetLabel(wxString::Format(_L("Size %d × %d px · Colour %s · File %s bytes"), w, h,
                                                            img.HasAlpha() ? _L("RGBA, 8 bits") : _L("RGB, 8 bits"), number(bytes.size())));
                m_url_image_info->Show();
            } else {
                m_url_preview_note->SetLabel(_L("This image format cannot be drawn here. Save the bytes to open it in your own viewer."));
            }
        }
    } else if (kind == 1) {
        if (bytes.size() > kTextPreview) {
            m_url_preview_note->SetLabel(wxString::Format(_L("This text is %s, over the 2 MB preview limit. Save it to read it."), size_text(bytes.size())));
        } else {
            std::string charset;
            for (const auto &[k, v] : m_url_parsed.parameters)
                if (k == "charset") charset = v;
            std::transform(charset.begin(), charset.end(), charset.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            const B64::TextEncoding encoding = charset == "utf-16le" ? B64::TextEncoding::Utf16LE :
                                               charset == "utf-16be" ? B64::TextEncoding::Utf16BE :
                                               (charset == "iso-8859-1" || charset == "latin1" || charset == "latin-1") ? B64::TextEncoding::Latin1 :
                                                                                                                        B64::TextEncoding::Utf8;
            std::string utf8;
            std::size_t bad = 0;
            if (B64::bytes_to_text(bytes, encoding, utf8, bad)) {
                m_url_preview_text->SetValue(wxString::FromUTF8(utf8));
                m_url_preview_text->Show();
            } else {
                m_url_preview_note->SetLabel(wxString::Format(_L("These bytes are not %s text (byte %s). The hex view shows them."), encoding_name(encoding), number(bad)));
            }
        }
    } else if (kind == 2) {
        m_url_preview_text->SetValue(wxString::FromUTF8(B64::hex_dump(bytes, 4096)));
        m_url_preview_text->Show();
        if (bytes.size() > 4096) m_url_preview_note->SetLabel(wxString::Format(_L("Showing the first 4 KB of %s."), size_text(bytes.size())));
    }
    m_url_preview_note->Show(!m_url_preview_note->GetLabel().IsEmpty());
    if (auto *page = page_at(TabDataUrl)) {
        page->Layout();
        if (auto *scrolled = dynamic_cast<wxScrolledWindow *>(page)) scrolled->FitInside();
    }
}

void Base64ToolkitPanel::media_load(const B64::Bytes &bytes, const std::string &extension)
{
#if wxUSE_MEDIACTRL
    auto *mc = dynamic_cast<wxMediaCtrl *>(m_url_media);
    if (!mc) return;
    // The file is named by its content, in the temporary folder, and removed
    // when the preview changes or the panel closes.
    const std::string digest = B64::sha256_hex(bytes.data(), bytes.size());
    const fs::path    path   = to_path(wxStandardPaths::Get().GetTempDir()) / ("bambu-base64-" + digest.substr(0, 16) + extension);
    if (!write_all(path, bytes.data(), bytes.size())) {
        m_url_preview_note->SetLabel(_L("The temporary file for the player could not be written. Save the bytes to play them in your own player."));
        return;
    }
    m_media_temp = path;
    if (!mc->Load(wxString::FromUTF8(path.u8string()))) {
        m_url_preview_note->SetLabel(_L("The system player cannot open this format. Save the bytes to play them in your own player."));
        media_unload();
        return;
    }
    mc->Show();
    if (m_url_media_row) m_url_media_row->Show();
    m_media_info->SetLabel(_L("Loading"));
    m_media_timer.Start(500);
#else
    (void) bytes;
    (void) extension;
    m_url_preview_note->SetLabel(_L("This build has no media player. Save the bytes to play them in your own player."));
#endif
}

void Base64ToolkitPanel::media_unload()
{
    m_media_timer.Stop();
#if wxUSE_MEDIACTRL
    if (auto *mc = dynamic_cast<wxMediaCtrl *>(m_url_media)) {
        mc->Stop();
        mc->Hide();
    }
#endif
    if (m_url_media_row) m_url_media_row->Hide();
    if (!m_media_temp.empty()) {
        std::error_code ec;
        fs::remove(m_media_temp, ec);
        m_media_temp.clear();
    }
}

void Base64ToolkitPanel::media_toggle()
{
#if wxUSE_MEDIACTRL
    auto *mc = dynamic_cast<wxMediaCtrl *>(m_url_media);
    if (!mc) return;
    if (mc->GetState() == wxMEDIASTATE_PLAYING) mc->Pause();
    else mc->Play();
#endif
    media_refresh();
}

void Base64ToolkitPanel::media_refresh()
{
#if wxUSE_MEDIACTRL
    auto *mc = dynamic_cast<wxMediaCtrl *>(m_url_media);
    if (!mc || !m_media_info) return;
    const bool playing = mc->GetState() == wxMEDIASTATE_PLAYING;
    m_media_play->SetLabel(playing ? _L("Pause") : _L("Play"));
    m_media_play->SetGlyph(playing ? GlyphPause : GlyphPlay);
    const auto seconds = [](wxFileOffset ms) {
        const long s = long(ms / 1000);
        return wxString::Format("%ld:%02ld", s / 60, s % 60);
    };
    const wxFileOffset length = mc->Length(), at = mc->Tell();
    if (length > 0) m_media_info->SetLabel(wxString::Format(_L("%s of %s"), seconds(at), seconds(length)));
    else m_media_info->SetLabel(playing ? _L("Playing") : _L("Ready"));
    if (m_url_media_row) m_url_media_row->Layout();
#endif
}

void Base64ToolkitPanel::url_save_bytes()
{
    if (!m_url_parsed_ok) return;
    std::string ext = B64::extension_for_mime(m_url_parsed.mime);
    if (ext.empty()) ext = B64::detect_type(m_url_parsed.bytes).extension;
    wxFileDialog dialog(this, _L("Save the bytes"), wxEmptyString, "data" + wxString::FromUTF8(ext), _L("All files (*.*)|*.*"),
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const fs::path path = to_path(dialog.GetPath());
    if (!write_all(path, m_url_parsed.bytes.data(), m_url_parsed.bytes.size())) {
        set_status(_L("Could not save there."));
        return;
    }
    set_status(wxString::Format(_L("Saved %s, %s. It was not opened."), from_u8(path.filename().u8string()), size_text(m_url_parsed.bytes.size())));
}

// ------------------------------------------------------------------- History tab

wxWindow *Base64ToolkitPanel::build_history_page(wxWindow *parent)
{
    auto *page = new wxPanel(parent, wxID_ANY);
    page->SetBackgroundColour(GetBackgroundColour());
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    const int gap = FromDIP(12);

    auto *bar = new wxBoxSizer(wxHORIZONTAL);
    bar->Add(make_label(page, Label::Head_16, _L("History")), 1, wxALIGN_CENTER_VERTICAL);
    m_undo_button = make_icon_button(page, GlyphUndo, _L("Undo the last change on the Text tab"));
    m_undo_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { undo(); });
    m_redo_button = make_icon_button(page, GlyphRedo, _L("Redo on the Text tab"));
    m_redo_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { redo(); });
    auto *export_button = make_button(page, _L("Export history"), GlyphSave);
    export_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { export_history(); });
    auto *clear = make_button(page, _L("Clear history"), GlyphDelete);
    clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { clear_history(); });
    bar->Add(m_undo_button, 0, wxALIGN_CENTER_VERTICAL);
    bar->Add(m_redo_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    bar->Add(export_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    bar->Add(clear, 0, wxALIGN_CENTER_VERTICAL);
    sizer->Add(bar, 0, wxEXPAND | wxTOP, gap);

    m_history_list = new MD3DataViewListCtrl(page, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(300)), wxDV_ROW_LINES | wxDV_SINGLE);
    m_history_list->AppendTextColumn(_L("When"), wxDATAVIEW_CELL_INERT, FromDIP(150), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_history_list->AppendTextColumn(_L("Action"), wxDATAVIEW_CELL_INERT, FromDIP(360), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_history_list->AppendTextColumn(_L("Sizes"), wxDATAVIEW_CELL_INERT, FromDIP(180), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_history_list->AppendTextColumn("SHA-256", wxDATAVIEW_CELL_INERT, FromDIP(140), wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE);
    m_history_list->SetName(_L("Base64 history"));
    m_history_list->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent &) { restore_history_row(m_history_list->GetSelectedRow()); });
    sizer->Add(m_history_list, 1, wxEXPAND | wxTOP, gap);
    sizer->Add(make_label(page, Label::Body_12, _L("Double-click a text row to open it on the Text tab again: its options always, its text when it was kept."), true, true),
               0, wxEXPAND | wxTOP, FromDIP(6));

    StaticBox *privacy = card(page, MD3::Role::SurfaceContainerLow);
    auto      *ps      = new wxBoxSizer(wxHORIZONTAL);
    ps->Add(glyph_window(privacy, GlyphShield, MD3::Role::Primary), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    ps->Add(make_label(privacy, Label::Body_13,
                       _L("History keeps names, sizes and hashes only. Turn on Keep payloads to store the text itself, on this computer only."), false, true),
            1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    ps->Add(make_label(privacy, Label::Body_13, _L("Keep payloads")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    m_keep_payloads = new SwitchButton(privacy);
    m_keep_payloads->SetName(_L("Keep payloads"));
    bool keep = false;
    if (AppConfig *cfg = wxGetApp().app_config) keep = cfg->get(kPayloadsKey) == "1";
    m_keep_payloads->SetValue(keep);
    m_keep_payloads->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &) { set_keep_payloads(m_keep_payloads->GetValue()); });
    ps->Add(m_keep_payloads, 0, wxALIGN_CENTER_VERTICAL);
    auto *pp = new wxBoxSizer(wxVERTICAL);
    pp->Add(ps, 1, wxEXPAND | wxALL, FromDIP(14));
    privacy->SetSizer(pp);
    sizer->Add(privacy, 0, wxEXPAND | wxTOP | wxBOTTOM, gap);
    page->SetSizer(sizer);
    return page;
}

void Base64ToolkitPanel::append_history(B64::HistoryEntry entry)
{
    // Recent activity on the Local tools page: the name only, never the payload.
    static const std::pair<const char *, const char *> words[] = {
        {"encode-text", L("Encoded text")},    {"decode-text", L("Decoded text")},
        {"encode-file", L("Encoded")},         {"decode-file", L("Decoded")},
        {"build-url", L("Built a data: URL")}, {"read-url", L("Read a data: URL")},
    };
    for (const auto &[action, said] : words)
        if (entry.action == action) LocalTools::record_activity("base64", said, entry.name);
    const bool keep = m_keep_payloads && m_keep_payloads->GetValue();
    m_history.append(std::move(entry), keep);
    refresh_tab_labels();
    if (m_tab == TabHistory) refresh_history();
}

void Base64ToolkitPanel::refresh_history()
{
    if (!m_history_list) return;
    m_history_list->DeleteAllItems();
    m_history_rows.clear();
    const auto    &entries = m_history.entries();
    const wxString today   = wxDateTime::Now().FormatISODate();
    for (std::size_t n = entries.size(); n-- > 0;) {
        const B64::HistoryEntry &e = entries[n];
        const wxDateTime         when(static_cast<time_t>(e.time));
        const wxString           stamp = when.FormatISODate() == today ? when.Format("%H:%M:%S") : when.Format("%Y-%m-%d %H:%M");
        const wxString           name  = wxString::FromUTF8(e.name);
        wxString                 action;
        if (e.action == "encode-text") action = wxString::Format(_L("Encoded text, %s"), name);
        else if (e.action == "decode-text") action = wxString::Format(_L("Decoded text, %s"), name);
        else if (e.action == "encode-file") action = wxString::Format(_L("Encoded %s"), name);
        else if (e.action == "decode-file") action = wxString::Format(_L("Decoded %s"), name);
        else if (e.action == "build-url") action = wxString::Format(_L("Built a data: URL, %s"), name);
        else action = wxString::Format(_L("Read a data: URL, %s"), name);
        if (e.has_payload) action << " (" << _L("text kept") << ")";
        wxVector<wxVariant> row;
        row.push_back(wxVariant(stamp));
        row.push_back(wxVariant(action));
        row.push_back(wxVariant(wxString::Format(_L("%s to %s"), size_text(e.bytes_in), size_text(e.bytes_out))));
        row.push_back(wxVariant(wxString::FromUTF8(B64::short_hash(e.sha256))));
        m_history_list->AppendItem(row);
        m_history_rows.push_back(int(n));
    }
    refresh_undo_buttons();
}

void Base64ToolkitPanel::export_history()
{
    if (m_history.entries().empty()) {
        set_status(_L("The history is empty."));
        return;
    }
    wxFileDialog dialog(this, _L("Export history"), wxEmptyString, "base64-history.json", _L("JSON files (*.json)|*.json"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    const std::string json = m_history.export_json();
    if (!write_all(to_path(dialog.GetPath()), json.data(), json.size())) set_status(_L("Could not save there."));
    else set_status(wxString::Format(_L("Exported %s history rows."), number(m_history.entries().size())));
}

void Base64ToolkitPanel::clear_history()
{
    if (m_history.entries().empty()) return;
    MessageDialog dialog(this, _L("Clear every history row? Kept text is deleted too. This cannot be undone."), _L("Clear history"),
                         wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    if (dialog.ShowModal() != wxID_YES) return;
    m_history.clear();
    refresh_history();
    refresh_tab_labels();
    set_status(_L("History cleared."));
}

void Base64ToolkitPanel::set_keep_payloads(bool keep)
{
    if (AppConfig *cfg = wxGetApp().app_config) cfg->set(kPayloadsKey, keep ? "1" : "0");
    if (!keep) {
        m_history.forget_payloads();
        refresh_history();
        set_status(_L("Keep payloads is off. Text kept earlier was deleted from the history."));
    } else {
        set_status(_L("Keep payloads is on. From now on, text you copy, save or export is kept in the history, on this computer only."));
    }
}

void Base64ToolkitPanel::restore_history_row(int row)
{
    if (row < 0 || std::size_t(row) >= m_history_rows.size()) return;
    const B64::HistoryEntry &e = m_history.entries()[std::size_t(m_history_rows[std::size_t(row)])];
    if (!e.has_options) {
        set_status(_L("Only text rows can be opened again; files stay where they were written."));
        return;
    }
    B64::TextState state;
    state.options = e.options;
    state.input   = e.has_payload ? e.payload : m_input_u8;
    apply_state(state);
    set_status(e.has_payload ? _L("Opened the kept text and its options on the Text tab.")
                             : _L("Opened the options on the Text tab. The text was not kept, so your current text stays."));
}

}} // namespace Slic3r::GUI
