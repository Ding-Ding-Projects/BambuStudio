#include "VocabularyDrop.hpp"
#include "ActionSounds.hpp"

#include "I18N.hpp"
#include "LanguageMode.hpp"
#include "PersonalVocabulary.hpp"
#include "PersonalModes/SchoolMode.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/StateColor.hpp"

#include <boost/log/trivial.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <memory>
#include <vector>

#include <wx/control.h>
#include <wx/dcbuffer.h>
#include <wx/dcscreen.h>
#include <wx/dnd.h>
#include <wx/graphics.h>
#include <wx/popupwin.h>
#include <wx/timer.h>

#ifdef _WIN32
#include <windows.h>
#include <UIAutomation.h>
#pragma comment(lib, "uiautomationcore.lib")
#endif

namespace Slic3r::GUI::VocabularyDrop {

namespace {

constexpr int    ANIMATE_IN_MS   = 320;
constexpr int    SUCCESS_HOLD_MS = 3000;
constexpr int    REFUSAL_HOLD_MS = 5000;
constexpr int    ANIMATE_OUT_MS  = 320;
constexpr int    TICK_MS         = 16;
constexpr double PI              = 3.14159265358979323846;

// Text in the current language mode: primary as _L() shows it, secondary set
// only in the bilingual mode. Painted text gets no bilingual decoration from
// the widget registry, so the overlay draws both lines itself.
struct Copy
{
    wxString primary;
    wxString secondary;

    wxString joined(const wxString &separator) const { return secondary.empty() ? primary : primary + separator + secondary; }
};

Copy localize(const char *source)
{
    const I18N::LocalizedText text = I18N::language_mode_service().translate(wxString::FromUTF8(source), wxEmptyString);
    return {text.primary, text.secondary};
}

Copy localize_with(const char *source, int value)
{
    Copy copy = localize(source);
    copy.primary = wxString::Format(copy.primary, value);
    if (!copy.secondary.empty()) copy.secondary = wxString::Format(copy.secondary, value);
    return copy;
}

Copy rejection_copy(PersonalVocabulary::Rejection reason)
{
    using PersonalVocabulary::Rejection;
    switch (reason) {
    case Rejection::Unreadable: return localize(L("The file could not be read."));
    case Rejection::TooLarge: return localize_with(L("The file is larger than %d KiB."), int(PersonalVocabulary::max_file_bytes / 1024));
    case Rejection::Malformed: return localize(L("The file is not valid JSON saved as UTF-8."));
    case Rejection::TooDeep:
        return localize_with(L("The file nests objects or lists more than %d levels deep."), int(PersonalVocabulary::max_depth));
    case Rejection::DuplicateKey: return localize(L("The file repeats a key inside one object."));
    case Rejection::UnsupportedVersion: return localize(L("The file must use schemaVersion 1."));
    case Rejection::UnexpectedField: return localize(L("The file must contain only schemaVersion and an entries object."));
    case Rejection::NonStringValue: return localize(L("Every entry value must be text."));
    case Rejection::KeyTooLong:
        return localize_with(L("An entry key is longer than %d characters."), int(PersonalVocabulary::max_key_code_points));
    case Rejection::ValueTooLong:
        return localize_with(L("An entry value is longer than %d characters."), int(PersonalVocabulary::max_value_code_points));
    case Rejection::UnsafeText: return localize(L("An entry is blank, contains control or text-direction characters, or uses a reserved name."));
    case Rejection::CacheUnavailable: return localize(L("The file is valid, but this device could not save its private copy."));
    case Rejection::None: break;
    }
    return {};
}

bool os_reduced_motion()
{
#ifdef _WIN32
    BOOL animations = TRUE;
    if (::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
        return animations == FALSE;
#endif
    return false;
}

// Polite screen reader announcement through the window's UI Automation provider.
void announce(wxWindow *frame, const wxString &text)
{
#ifdef _WIN32
    if (frame == nullptr || text.empty()) return;
    IRawElementProviderSimple *provider = nullptr;
    if (FAILED(::UiaHostProviderFromHwnd(frame->GetHWND(), &provider)) || provider == nullptr) return;
    BSTR message  = ::SysAllocString(text.wc_str());
    BSTR activity = ::SysAllocString(L"personal-vocabulary-drop");
    if (message != nullptr && activity != nullptr)
        ::UiaRaiseNotificationEvent(provider, NotificationKind_ActionCompleted, NotificationProcessing_All, message, activity);
    ::SysFreeString(message);
    ::SysFreeString(activity);
    provider->Release();
#else
    (void) frame;
    (void) text;
#endif
}

// The wording the loaded file gives for "private vocabulary", if any.
wxString headline_from_file()
{
    for (const wxString source : {wxString("private vocabulary"), wxString("Private vocabulary")}) {
        const wxString shown = PersonalVocabulary::display(PersonalVocabulary::remember(source));
        if (shown != source) return shown;
    }
    return wxString();
}

double ease_out(double t) { return 1.0 - std::pow(1.0 - std::clamp(t, 0.0, 1.0), 3.0); }

// Word-wraps text to max_width with dc's font; runs without spaces (CJK) break per character.
std::vector<wxString> wrap(wxDC &dc, const wxString &text, int max_width)
{
    std::vector<wxString> lines;
    wxString line;
    wxString word;
    auto fits = [&](const wxString &candidate) { return dc.GetTextExtent(candidate).x <= max_width; };
    auto push_word = [&]() {
        if (word.empty()) return;
        const wxString candidate = line.empty() ? word : line + " " + word;
        if (fits(candidate)) {
            line = candidate;
        } else {
            if (!line.empty()) lines.push_back(line);
            line.clear();
            for (const wxUniChar c : word) {
                if (!line.empty() && !fits(line + c)) {
                    lines.push_back(line);
                    line.clear();
                }
                line += c;
            }
        }
        word.clear();
    };
    for (const wxUniChar c : text) {
        if (c == ' ')
            push_word();
        else
            word += c;
    }
    push_word();
    if (!line.empty()) lines.push_back(line);
    return lines;
}

struct TextBlock
{
    wxString text;
    wxFont   font;
    int      gap_before = 0;
};

// Click-through overlay over the main window. It never takes focus or input,
// so the workspace stays usable while it is shown.
class OutcomeOverlay : public wxPopupWindow
{
public:
    // A celebration shows headline and subtitle; a refusal shows the reason.
    OutcomeOverlay(wxWindow *frame, bool applied, std::vector<TextBlock> blocks, const wxString &accessible_name)
        : wxPopupWindow(frame, wxBORDER_NONE)
        , m_frame(frame)
        , m_applied(applied)
        , m_blocks(std::move(blocks))
        , m_animate(!os_reduced_motion() && CanSetTransparent())
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetName(accessible_name);
#ifdef _WIN32
        HWND hwnd = GetHWND();
        ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE,
                            ::GetWindowLongPtrW(hwnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE);
#endif
        Bind(wxEVT_PAINT, &OutcomeOverlay::on_paint, this);
        m_timer.Bind(wxEVT_TIMER, &OutcomeOverlay::on_tick, this);
        m_frame->Bind(wxEVT_CLOSE_WINDOW, &OutcomeOverlay::on_frame_close, this);
    }

    void present()
    {
        place();
        m_elapsed = 0;
        SetTransparent(m_animate ? 0 : peak_alpha());
        Show(true);
        m_timer.Start(TICK_MS);
    }

    void dismiss()
    {
        if (m_closing) return;
        m_closing = true;
        m_timer.Stop();
        m_frame->Unbind(wxEVT_CLOSE_WINDOW, &OutcomeOverlay::on_frame_close, this);
        Hide();
        if (s_current == this) s_current = nullptr;
        CallAfter([this] { Destroy(); });
    }

    static OutcomeOverlay *s_current;

private:
    int hold_ms() const { return m_applied ? SUCCESS_HOLD_MS : REFUSAL_HOLD_MS; }
    int peak_alpha() const { return m_applied ? 236 : 255; }
    int pad() const { return FromDIP(m_applied ? 32 : 20); }

    // Wraps every block to width and returns the total text height.
    int layout(int width)
    {
        wxScreenDC dc;
        m_lines.clear();
        int height = 0;
        for (const TextBlock &block : m_blocks) {
            dc.SetFont(block.font);
            height += block.gap_before;
            bool first = true;
            for (const wxString &line : wrap(dc, block.text, width)) {
                const int line_height = dc.GetTextExtent(line.empty() ? wxString("Ag") : line).y;
                m_lines.push_back({line, block.font, first ? block.gap_before : 0, line_height});
                height += line_height;
                first = false;
            }
        }
        return height;
    }

    void place()
    {
        const wxRect client(m_frame->ClientToScreen(wxPoint(0, 0)), m_frame->GetClientSize());
        if (m_applied) {
            m_text_height = layout(client.width - 2 * pad());
            SetSize(client);
            return;
        }
        // The refusal is a notice, not a celebration: a card near the bottom.
        const int width = std::min(FromDIP(640), client.width - FromDIP(48));
        m_text_height   = layout(width - 2 * pad());
        const int height = m_text_height + 2 * pad();
        SetSize(client.x + (client.width - width) / 2, client.y + client.height - height - FromDIP(96), width, height);
    }

    void on_frame_close(wxCloseEvent &event)
    {
        event.Skip();
        dismiss();
    }

    void on_tick(wxTimerEvent &)
    {
        m_elapsed += TICK_MS;
        const int total = (m_animate ? ANIMATE_IN_MS + ANIMATE_OUT_MS : 0) + hold_ms();
        if (m_elapsed >= total) {
            dismiss();
            return;
        }
        if (!m_animate) return;
        double level = 1.0;
        if (m_elapsed < ANIMATE_IN_MS)
            level = ease_out(double(m_elapsed) / ANIMATE_IN_MS);
        else if (m_elapsed > ANIMATE_IN_MS + hold_ms())
            level = 1.0 - ease_out(double(m_elapsed - ANIMATE_IN_MS - hold_ms()) / ANIMATE_OUT_MS);
        SetTransparent(wxByte(std::lround(level * peak_alpha())));
        m_entrance = std::min(1.0, double(m_elapsed) / ANIMATE_IN_MS);
        Refresh(false);
    }

    void on_paint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        const wxRect rect = GetClientRect();
        if (m_applied)
            paint_celebration(dc, gc.get(), rect);
        else
            paint_refusal(dc, gc.get(), rect);
    }

    // Lines are centred for the celebration and left aligned for the notice.
    void paint_lines(wxDC &dc, int top, const wxRect &rect, bool centred, const wxColour &colour)
    {
        dc.SetTextForeground(colour);
        int y = top;
        for (const Line &line : m_lines) {
            y += line.gap_before;
            dc.SetFont(line.font);
            const int x = centred ? (rect.width - dc.GetTextExtent(line.text).x) / 2 : pad();
            dc.DrawText(line.text, x, y);
            y += line.height;
        }
    }

    void paint_celebration(wxDC &dc, wxGraphicsContext *gc, const wxRect &rect)
    {
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::PrimaryContainer)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(rect);

        const double  grow = m_animate ? ease_out(m_entrance) : 1.0;
        const wxPoint centre(rect.width / 2, rect.height / 2);
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            // Rays and confetti, laid out deterministically around the text.
            const wxColour ray   = StateColor::semantic(MD3::Role::Primary);
            const double   reach = std::hypot(rect.width, rect.height) * 0.5 * (0.55 + 0.45 * grow);
            const double   inner = std::max<double>(FromDIP(150), m_text_height * 0.8);
            gc->SetPen(wxPen(wxColour(ray.Red(), ray.Green(), ray.Blue(), 40), FromDIP(18)));
            for (int i = 0; i < 16; ++i) {
                const double angle = i * PI / 8.0;
                gc->StrokeLine(centre.x + std::cos(angle) * inner, centre.y + std::sin(angle) * inner,
                               centre.x + std::cos(angle) * reach, centre.y + std::sin(angle) * reach);
            }
            const wxColour pieces[] = {StateColor::semantic(MD3::Role::Primary), StateColor::semantic(MD3::Role::SecondaryContainer),
                                       StateColor::semantic(MD3::Role::InversePrimary)};
            gc->SetPen(*wxTRANSPARENT_PEN);
            for (int i = 0; i < 28; ++i) {
                const double angle  = i * 2.399963; // golden angle spreads the pieces evenly
                const double radius = (inner + FromDIP(40) + (i * 37 % 160) * FromDIP(1)) * grow;
                gc->SetBrush(wxBrush(pieces[i % 3]));
                gc->DrawRoundedRectangle(centre.x + std::cos(angle) * radius * 1.6, centre.y + std::sin(angle) * radius,
                                         FromDIP(10), FromDIP(18), FromDIP(3));
            }
        }
        // The text rises into place while the overlay fades in.
        const int rise = m_animate ? int(std::lround((1.0 - grow) * FromDIP(40))) : 0;
        paint_lines(dc, centre.y - m_text_height / 2 + rise, rect, true, StateColor::semantic(MD3::Role::OnPrimaryContainer));
    }

    void paint_refusal(wxDC &dc, wxGraphicsContext *gc, const wxRect &rect)
    {
        // Popup windows are rectangular; paint the corners in the surface tone.
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Surface)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(rect);
        const wxColour container = StateColor::semantic(MD3::Role::ErrorContainer);
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            gc->SetBrush(wxBrush(container));
            gc->SetPen(wxPen(StateColor::semantic(MD3::Role::Error), 1));
            gc->DrawRoundedRectangle(rect.x + 0.5, rect.y + 0.5, rect.width - 1.0, rect.height - 1.0, FromDIP(16));
        } else {
            dc.SetBrush(wxBrush(container));
            dc.DrawRectangle(rect);
        }
        paint_lines(dc, pad(), rect, false, StateColor::semantic(MD3::Role::OnErrorContainer));
    }

    struct Line
    {
        wxString text;
        wxFont   font;
        int      gap_before;
        int      height;
    };

    wxWindow              *m_frame;
    bool                   m_applied;
    std::vector<TextBlock> m_blocks;
    std::vector<Line>      m_lines;
    int                    m_text_height = 0;
    bool                   m_animate;
    wxTimer                m_timer;
    int                    m_elapsed  = 0;
    double                 m_entrance = 0.0;
    bool                   m_closing  = false;
};

OutcomeOverlay *OutcomeOverlay::s_current = nullptr;

void show_outcome(wxWindow *frame, bool applied, std::vector<TextBlock> blocks, const wxString &spoken)
{
    if (OutcomeOverlay::s_current) OutcomeOverlay::s_current->dismiss();
    auto *overlay = new OutcomeOverlay(frame, applied, std::move(blocks), spoken);
    OutcomeOverlay::s_current = overlay;
    overlay->present();
    announce(frame, spoken);
}

void show_refusal(wxWindow *frame, const Copy &reason)
{
    const Copy refused = localize(L("The vocabulary file was not applied, and the wording on screen has not changed."));
    std::vector<TextBlock> blocks{{refused.primary, Label::Head_14, 0}};
    if (!refused.secondary.empty()) blocks.push_back({refused.secondary, Label::Body_14, wxWindow::FromDIP(4, frame)});
    blocks.push_back({reason.primary, Label::Body_14, wxWindow::FromDIP(12, frame)});
    if (!reason.secondary.empty()) blocks.push_back({reason.secondary, Label::Body_14, wxWindow::FromDIP(4, frame)});
    show_outcome(frame, false, std::move(blocks), refused.joined(" ") + " " + reason.joined(" "));
}

void show_celebration(wxWindow *frame)
{
    const Copy     applied  = localize(L("Personal wording applied"));
    const wxString from_file = headline_from_file();
    wxFont headline_font = Label::Head_48;
    headline_font.SetFractionalPointSize(headline_font.GetFractionalPointSize() * 1.2);
    std::vector<TextBlock> blocks;
    if (!from_file.empty()) {
        blocks.push_back({from_file, headline_font, 0});
        blocks.push_back({applied.primary, Label::Head_20, wxWindow::FromDIP(16, frame)});
    } else {
        blocks.push_back({applied.primary, headline_font, 0});
    }
    if (!applied.secondary.empty()) blocks.push_back({applied.secondary, Label::Head_20, wxWindow::FromDIP(8, frame)});
    show_outcome(frame, true, std::move(blocks), from_file.empty() ? applied.joined(" ") : from_file + " " + applied.joined(" "));
}

class VocabularyDropTarget : public wxFileDropTarget
{
public:
    explicit VocabularyDropTarget(wxWindow *frame) : m_frame(frame) { SetDefaultAction(wxDragCopy); }

    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString &files) override
    {
        if (!is_vocabulary_file(files)) return false;
        const wxString file  = files[0];
        wxWindow      *frame = m_frame;
        // Leave the OLE drop callback before validating and painting.
        frame->CallAfter([frame, file] { apply_dropped(frame, file); });
        return true;
    }

private:
    wxWindow *m_frame;
};

} // namespace

bool is_vocabulary_file(const wxArrayString &files) { return files.size() == 1 && files[0].Lower().EndsWith(".json"); }

wxString rejection_text(PersonalVocabulary::Rejection reason) { return rejection_copy(reason).primary; }

void apply_dropped(wxWindow *frame, const wxString &file)
{
    if (frame == nullptr) return;
    if (PersonalModes::school_presentation_suppressed.load()) {
        show_refusal(frame, localize(L("Personal wording cannot be changed while school presentation is on.")));
        return;
    }
    auto       reason  = PersonalVocabulary::Rejection::None;
    const bool applied = PersonalVocabulary::load(std::filesystem::path(file.ToStdWstring()), &reason);
    ActionSounds::play(applied ? ActionSounds::Cue::VocabularyApplied : ActionSounds::Cue::Rejected);
    // Diagnostics name the outcome only, never the file, its keys or its values.
    BOOST_LOG_TRIVIAL(info) << "Personal vocabulary drop: " << (applied ? "applied" : "refused") << ", rule " << int(reason);
    if (applied)
        show_celebration(frame);
    else
        show_refusal(frame, rejection_copy(reason));
}

wxFileDropTarget *create_drop_target(wxWindow *frame) { return new VocabularyDropTarget(frame); }

} // namespace Slic3r::GUI::VocabularyDrop
