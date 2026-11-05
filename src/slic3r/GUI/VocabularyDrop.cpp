#include "VocabularyDrop.hpp"

#include "I18N.hpp"
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

#include <wx/control.h>
#include <wx/dcbuffer.h>
#include <wx/dnd.h>
#include <wx/graphics.h>
#include <wx/popupwin.h>
#include <wx/timer.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <shellapi.h>
#include <UIAutomation.h>
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "uiautomationcore.lib")
#endif

namespace Slic3r::GUI::VocabularyDrop {

namespace {

constexpr int ANIMATE_IN_MS  = 320;
constexpr int SUCCESS_HOLD_MS = 3000;
constexpr int REFUSAL_HOLD_MS = 5000;
constexpr int ANIMATE_OUT_MS = 320;
constexpr int TICK_MS        = 16;
constexpr double PI         = 3.14159265358979323846;

bool os_quiet_mode()
{
#ifdef _WIN32
    QUERY_USER_NOTIFICATION_STATE state {};
    if (SUCCEEDED(::SHQueryUserNotificationState(&state))) {
        switch (state) {
        case QUNS_BUSY:
        case QUNS_RUNNING_D3D_FULL_SCREEN:
        case QUNS_PRESENTATION_MODE:
        case QUNS_QUIET_TIME:
        case QUNS_APP: return true;
        default: break;
        }
    }
#endif
    return false;
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

// One cue per outcome; the system volume and mute apply as for any sound.
void play_outcome_sound(bool applied)
{
#ifdef _WIN32
    if (os_quiet_mode()) return;
    if (applied) {
        wchar_t windows_dir[MAX_PATH] {};
        const UINT length = ::GetWindowsDirectoryW(windows_dir, MAX_PATH);
        if (length == 0 || length >= MAX_PATH) return;
        const std::wstring tada = std::wstring(windows_dir, length) + L"\\Media\\tada.wav";
        ::PlaySoundW(tada.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    } else {
        ::PlaySoundW(L"SystemHand", nullptr, SND_ALIAS | SND_ASYNC | SND_NODEFAULT);
    }
#else
    (void) applied;
#endif
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
        ::UiaRaiseNotificationEvent(provider, NotificationKind_ActionCompleted, NotificationProcessing_All,
                                    message, activity);
    ::SysFreeString(message);
    ::SysFreeString(activity);
    provider->Release();
#else
    (void) frame;
    (void) text;
#endif
}

// The wording the loaded file gives for "private vocabulary", if any.
wxString celebration_headline()
{
    for (const wxString source : {wxString("private vocabulary"), wxString("Private vocabulary")}) {
        const wxString shown = PersonalVocabulary::display(PersonalVocabulary::remember(source));
        if (shown != source) return shown;
    }
    return _L("Personal wording applied");
}

double ease_out(double t) { return 1.0 - std::pow(1.0 - std::clamp(t, 0.0, 1.0), 3.0); }

// Click-through overlay over the main window. It never takes focus or input,
// so the workspace stays usable while it is shown.
class OutcomeOverlay : public wxPopupWindow
{
public:
    OutcomeOverlay(wxWindow *frame, bool applied, wxString headline, wxString detail)
        : wxPopupWindow(frame, wxBORDER_NONE)
        , m_frame(frame)
        , m_applied(applied)
        , m_headline(std::move(headline))
        , m_detail(std::move(detail))
        , m_animate(!os_reduced_motion() && CanSetTransparent())
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetName(m_detail.empty() ? m_headline : m_headline + "\n" + m_detail);
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

    void place()
    {
        const wxRect client(m_frame->ClientToScreen(wxPoint(0, 0)), m_frame->GetClientSize());
        if (m_applied) {
            SetSize(client);
            return;
        }
        // The refusal is a notice, not a celebration: a card near the bottom.
        const int width  = std::min(FromDIP(560), client.width - FromDIP(48));
        const int height = FromDIP(112);
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

    void paint_celebration(wxDC &dc, wxGraphicsContext *gc, const wxRect &rect)
    {
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::PrimaryContainer)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(rect);

        const double grow = m_animate ? ease_out(m_entrance) : 1.0;
        const wxPoint centre(rect.width / 2, rect.height / 2);
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            // Rays and confetti, laid out deterministically around the headline.
            const wxColour ray = StateColor::semantic(MD3::Role::Primary);
            const double reach = std::hypot(rect.width, rect.height) * 0.5 * (0.55 + 0.45 * grow);
            gc->SetPen(wxPen(wxColour(ray.Red(), ray.Green(), ray.Blue(), 40), FromDIP(18)));
            for (int i = 0; i < 16; ++i) {
                const double angle = i * PI / 8.0;
                gc->StrokeLine(centre.x + std::cos(angle) * FromDIP(150), centre.y + std::sin(angle) * FromDIP(150),
                               centre.x + std::cos(angle) * reach, centre.y + std::sin(angle) * reach);
            }
            const wxColour pieces[] = {StateColor::semantic(MD3::Role::Primary),
                                       StateColor::semantic(MD3::Role::SecondaryContainer),
                                       StateColor::semantic(MD3::Role::InversePrimary)};
            gc->SetPen(*wxTRANSPARENT_PEN);
            for (int i = 0; i < 28; ++i) {
                const double angle  = i * 2.399963; // golden angle spreads the pieces evenly
                const double radius = (FromDIP(190) + (i * 37 % 160) * FromDIP(1)) * grow;
                const double x = centre.x + std::cos(angle) * radius * 1.6;
                const double y = centre.y + std::sin(angle) * radius;
                gc->SetBrush(wxBrush(pieces[i % 3]));
                gc->DrawRoundedRectangle(x, y, FromDIP(10), FromDIP(18), FromDIP(3));
            }
        }

        const wxColour on_container = StateColor::semantic(MD3::Role::OnPrimaryContainer);
        wxFont headline_font = Label::Head_48;
        headline_font.SetFractionalPointSize(headline_font.GetFractionalPointSize() * (0.7 + 0.5 * grow));
        dc.SetFont(headline_font);
        dc.SetTextForeground(on_container);
        const wxString headline = wxControl::Ellipsize(m_headline, dc, wxELLIPSIZE_END, rect.width - FromDIP(64));
        const wxSize headline_size = dc.GetTextExtent(headline);
        dc.DrawText(headline, centre.x - headline_size.x / 2, centre.y - headline_size.y / 2);

        const wxString subtitle = _L("Personal wording applied");
        if (subtitle != m_headline) {
            dc.SetFont(Label::Head_20);
            const wxSize subtitle_size = dc.GetTextExtent(subtitle);
            dc.DrawText(subtitle, centre.x - subtitle_size.x / 2, centre.y + headline_size.y / 2 + FromDIP(16));
        }
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
        const int pad = FromDIP(20);
        dc.SetTextForeground(StateColor::semantic(MD3::Role::OnErrorContainer));
        dc.SetFont(Label::Head_14);
        dc.DrawText(wxControl::Ellipsize(m_headline, dc, wxELLIPSIZE_END, rect.width - 2 * pad), pad, pad);
        dc.SetFont(Label::Body_14);
        dc.DrawText(wxControl::Ellipsize(m_detail, dc, wxELLIPSIZE_END, rect.width - 2 * pad), pad, pad + FromDIP(36));
    }

    wxWindow *m_frame;
    bool      m_applied;
    wxString  m_headline;
    wxString  m_detail;
    bool      m_animate;
    wxTimer   m_timer;
    int       m_elapsed  = 0;
    double    m_entrance = 0.0;
    bool      m_closing  = false;
};

OutcomeOverlay *OutcomeOverlay::s_current = nullptr;

void show_outcome(wxWindow *frame, bool applied, const wxString &headline, const wxString &detail)
{
    if (OutcomeOverlay::s_current) OutcomeOverlay::s_current->dismiss();
    auto *overlay = new OutcomeOverlay(frame, applied, headline, detail);
    OutcomeOverlay::s_current = overlay;
    overlay->present();
}

class VocabularyDropTarget : public wxFileDropTarget
{
public:
    explicit VocabularyDropTarget(wxWindow *frame) : m_frame(frame) { SetDefaultAction(wxDragCopy); }

    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString &files) override
    {
        if (!is_vocabulary_file(files)) return false;
        const wxString file = files[0];
        wxWindow *frame = m_frame;
        // Leave the OLE drop callback before validating and painting.
        frame->CallAfter([frame, file] { apply_dropped(frame, file); });
        return true;
    }

private:
    wxWindow *m_frame;
};

} // namespace

bool is_vocabulary_file(const wxArrayString &files)
{
    return files.size() == 1 && files[0].Lower().EndsWith(".json");
}

wxString rejection_text(PersonalVocabulary::Rejection reason)
{
    using PersonalVocabulary::Rejection;
    switch (reason) {
    case Rejection::Unreadable: return _L("The file could not be read.");
    case Rejection::TooLarge:
        return wxString::Format(_L("The file is larger than %d KiB."), int(PersonalVocabulary::max_file_bytes / 1024));
    case Rejection::Malformed: return _L("The file is not valid JSON saved as UTF-8.");
    case Rejection::TooDeep:
        return wxString::Format(_L("The file nests objects or lists more than %d levels deep."), int(PersonalVocabulary::max_depth));
    case Rejection::DuplicateKey: return _L("The file repeats a key inside one object.");
    case Rejection::UnsupportedVersion: return _L("The file must use schemaVersion 1.");
    case Rejection::UnexpectedField: return _L("The file must contain only schemaVersion and an entries object.");
    case Rejection::NonStringValue: return _L("Every entry value must be text.");
    case Rejection::KeyTooLong:
        return wxString::Format(_L("An entry key is longer than %d characters."), int(PersonalVocabulary::max_key_code_points));
    case Rejection::ValueTooLong:
        return wxString::Format(_L("An entry value is longer than %d characters."), int(PersonalVocabulary::max_value_code_points));
    case Rejection::UnsafeText: return _L("An entry is blank, contains control or text-direction characters, or uses a reserved name.");
    case Rejection::CacheUnavailable: return _L("The file is valid, but this device could not save its private copy.");
    case Rejection::None: break;
    }
    return wxString();
}

void apply_dropped(wxWindow *frame, const wxString &file)
{
    if (frame == nullptr) return;
    const wxString refused = _L("The vocabulary file was not applied, and the wording on screen has not changed.");
    if (PersonalModes::school_presentation_suppressed.load()) {
        const wxString detail = _L("Personal wording cannot be changed while school presentation is on.");
        play_outcome_sound(false);
        show_outcome(frame, false, refused, detail);
        announce(frame, refused + " " + detail);
        return;
    }
    auto reason = PersonalVocabulary::Rejection::None;
    const bool applied = PersonalVocabulary::load(std::filesystem::path(file.ToStdWstring()), &reason);
    // Diagnostics name the outcome only, never the file, its keys or its values.
    BOOST_LOG_TRIVIAL(info) << "Personal vocabulary drop: " << (applied ? "applied" : "refused") << ", rule " << int(reason);
    play_outcome_sound(applied);
    if (applied) {
        const wxString headline = celebration_headline();
        show_outcome(frame, true, headline, wxString());
        announce(frame, headline);
    } else {
        const wxString detail = rejection_text(reason);
        show_outcome(frame, false, refused, detail);
        announce(frame, refused + " " + detail);
    }
}

wxFileDropTarget *create_drop_target(wxWindow *frame) { return new VocabularyDropTarget(frame); }

} // namespace Slic3r::GUI::VocabularyDrop
