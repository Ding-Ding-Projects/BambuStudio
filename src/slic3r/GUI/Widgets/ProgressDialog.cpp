#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/utils.h"
#include "wx/sizer.h"
#include "wx/event.h"
#include "wx/gauge.h"
#include "wx/settings.h"
#include "wx/app.h"
#endif

#include "../GUI.hpp"
#include "../GUI_App.hpp"
#include "../I18N.hpp"
#include "ProgressDialog.hpp"
#include "ProgressBar.hpp"
#include "StateColor.hpp"
#include "wx/evtloop.h"
#include "Label.hpp"

// ----------------------------------------------------------------------------
// constants
// ----------------------------------------------------------------------------

#define LAYOUT_MARGIN 12

static const int wxID_SKIP = 32000;

namespace Slic3r { namespace GUI {
void ProgressDialog::Init()
{
    // we may disappear at any moment, let the others know about it
    SetExtraStyle(GetExtraStyle() | wxWS_EX_TRANSIENT);

    // Initialize all our members that we always use (even when we don't
    // create a valid window in this class).

    m_pdStyle   = 0;
    m_parentTop = NULL;

    m_progress_bar   = NULL;
    m_progress_value = 0;
    m_msg     = NULL;
    m_elapsed = m_estimated = m_remaining = NULL;

    m_state   = Uncancelable;
    m_maximum = 0;

    m_timeStart = wxGetCurrentTime();
    m_timeStop  = (unsigned long) -1;
    m_break     = 0;

    m_skip = false;

    m_btnAbort = NULL;
    m_btnSkip  = NULL;

    m_display_estimated = m_last_timeupdate = m_ctdelay = 0;

    m_delay = 3;

    m_winDisabler   = NULL;
    m_tempEventLoop = NULL;
    // Window style is owned by the MD3Dialog shell (borderless, shaped); the
    // legacy SetWindowStyle(wxDEFAULT_DIALOG_STYLE) is intentionally gone.
}

// Both ctors delegate to MD3Dialog's protected two-phase default ctor, which
// creates the bare classic shaped window; the shell chrome + content are laid
// out later from Create() via MD3Dialog::CreateShell().
ProgressDialog::ProgressDialog() : MD3Dialog() { Init(); }

ProgressDialog::ProgressDialog(const wxString &title, const wxString &message, int maximum, wxWindow *parent, int style, bool adaptive) : MD3Dialog()
{
    m_adaptive = adaptive;
    Init();
    Create(title, message, maximum, parent, style);
    Bind(wxEVT_CLOSE_WINDOW, &ProgressDialog::OnClose, this);
}

void ProgressDialog::SetTopParent(wxWindow *parent)
{
    m_parent    = parent;
    m_parentTop = parent;
}

wxString ProgressDialog::FormatString(wxString title)
{
    if (!m_adaptive) {
        auto current_width = 0;
        m_mode             = 0;
        for (int i = 0; i < title.length(); i++) {
            current_width = m_msg->GetTextExtent(title.SubString(0, i)).x;
            if (current_width > PROGRESSDIALOG_GAUGE_SIZE.x) {
                m_mode = 1;
                // title.insert(i - 1, "\n");
                break;
            }
        }
        // set mode
        if (m_mode == 0) {
            m_simplebook->SetSelection(0);
            m_msg->SetLabel(title);
        } else {
            wxSize content_size = m_msg->GetTextExtent(title);
            int resized_height = (int(content_size.x / PROGRESSDIALOG_GAUGE_SIZE.x) + 1) * content_size.y;
            set_panel_height(resized_height);
            m_simplebook->SetSelection(1);
            m_msg_2line->SetLabel(title);
        }
    } else {
        m_msg->SetLabel(title);
        m_msg->SetMaxSize(wxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x + 5, -1));
        m_msg->SetMinSize(wxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x  + 5, -1));
        m_msg->Wrap(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x  + 5);
        m_msg->Layout();
        m_msg->Fit();

        m_msg_scrolledWindow->SetSize(wxSize(FromDIP(m_msg->GetSize().x + 5), FromDIP(150)));
        m_msg_scrolledWindow->SetMinSize(wxSize(FromDIP(m_msg->GetSize().x + 5), FromDIP(150)));
        m_msg_scrolledWindow->SetMaxSize(wxSize(FromDIP(m_msg->GetSize().x + 5), FromDIP(150)));
       // m_msg_scrolledWindow->Layout();
        m_msg_scrolledWindow->Fit();
    }

    
    //Fit();
    return title;
}

bool ProgressDialog::Create(const wxString &title, const wxString &message, int maximum, wxWindow *parent, int style)
{
    SetTopParent(parent);
    m_pdStyle = style;

    // Build the MD3 shell chrome onto the two-phase window created by the default
    // ctor: header icon tile + the operation title + circular close, a footer
    // divider + Cancel row, and the SurfaceContainer background. Replaces the
    // legacy wxDialog::Create() native-window creation (and the former Grey450
    // top line — the kit header stands in for it).
    if (!CreateShell(m_parentTop, title, wxEmptyString, MaterialIcon::Sync)) return false;

    SetMaximum(maximum);
    EnsureActiveEventLoopExists();

    m_state = HasPDFlag(wxPD_CAN_ABORT) ? Continue : Uncancelable;

    // Body: the status message (simplebook 1-line / 2-line, or a scrolled area in
    // the adaptive mode) and the MD3 ProgressBar, added to the shell body sizer
    // (already padded 0/24 by the shell).
    wxBoxSizer *body = GetContentSizer();

    if (!m_adaptive) {
        m_simplebook = new wxSimplebook(this, wxID_ANY, wxDefaultPosition, PROGRESSDIALOG_SIMPLEBOOK_SIZE, 0);
        m_simplebook->SetBackgroundColour(PROGRESSDIALOG_DEF_BK);

        m_panel_1line = new wxPanel(m_simplebook, wxID_ANY, wxDefaultPosition, PROGRESSDIALOG_SIMPLEBOOK_SIZE, 0);
        m_panel_1line->SetMaxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE);
        m_panel_1line->SetBackgroundColour(PROGRESSDIALOG_DEF_BK);
        m_simplebook->AddPage(m_panel_1line, wxEmptyString, false);

        m_panel_2line = new wxPanel(m_simplebook, wxID_ANY, wxDefaultPosition, PROGRESSDIALOG_SIMPLEBOOK_SIZE, 0);
        m_panel_2line->SetMaxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE);
        m_panel_2line->SetBackgroundColour(PROGRESSDIALOG_DEF_BK);
        m_simplebook->AddPage(m_panel_2line, wxEmptyString, false);

        wxBoxSizer *sizer_1line = new wxBoxSizer(wxHORIZONTAL);
        m_msg                   = new wxStaticText(m_panel_1line, wxID_ANY, wxEmptyString, wxDefaultPosition, PROGRESSDIALOG_SIMPLEBOOK_SIZE, 0);
        m_msg->Wrap(-1);
        m_msg->SetFont(::Label::Body_13);
        m_msg->SetForegroundColour(PROGRESSDIALOG_GREY_700);
        sizer_1line->Add(m_msg, 0, wxALIGN_CENTER, 0);
        m_panel_1line->SetSizer(sizer_1line);
        m_panel_1line->Layout();
        sizer_1line->Fit(m_panel_1line);

        wxBoxSizer *sizer_2line = new wxBoxSizer(wxVERTICAL);
        m_msg_2line             = new wxStaticText(m_panel_2line, wxID_ANY, wxEmptyString, wxDefaultPosition, PROGRESSDIALOG_SIMPLEBOOK_SIZE, 0);
        m_msg_2line->Wrap(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x);
        m_msg_2line->SetFont(::Label::Body_13);
        m_msg_2line->SetForegroundColour(PROGRESSDIALOG_GREY_700);
        m_msg_2line->SetMaxSize(wxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x, -1));
        sizer_2line->Add(m_msg_2line, 1, wxALL, 0);
        m_panel_2line->SetSizer(sizer_2line);
        m_panel_2line->Layout();
        sizer_2line->Fit(m_panel_2line);

        body->Add(m_simplebook, 0, wxEXPAND);
    } else {
        m_msg_scrolledWindow = new wxScrolledWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL );
        m_msg_scrolledWindow->SetBackgroundColour(PROGRESSDIALOG_DEF_BK);
        m_msg_scrolledWindow->SetScrollRate(0,5);
        wxBoxSizer* m_msg_sizer= new wxBoxSizer(wxVERTICAL);

        m_msg = new wxStaticText(m_msg_scrolledWindow, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x, -1), 0);
        m_msg->Wrap(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x);
        m_msg->SetFont(::Label::Body_13);
        m_msg->SetForegroundColour(PROGRESSDIALOG_GREY_700);

        m_msg_sizer->Add(m_msg, 0, wxEXPAND | wxALL, 0);
        m_msg_scrolledWindow->SetSizer(m_msg_sizer);
        m_msg_scrolledWindow->Layout();
        m_msg_sizer->Fit(m_msg_scrolledWindow);
        body->Add(m_msg_scrolledWindow, 0, wxEXPAND);
    }

#ifdef __WXMSW__
    maximum /= m_factor;
#endif

    if (!HasPDFlag(wxPD_NO_PROGRESS)) {
        // Migrated MD3 ProgressBar: Primary fill on a SurfaceContainerHighest
        // track (its defaults), 8px/r6 geometry, replacing the raw wxGauge.
        m_progress_bar = new ProgressBar(this, wxID_ANY, maximum, wxDefaultPosition, PROGRESSDIALOG_GAUGE_SIZE);
        // The bar paints its own rounded track; match its window fill to the
        // dialog so the rounded corners don't reveal a stale white backing.
        m_progress_bar->SetBackgroundColour(PROGRESSDIALOG_DEF_BK);
        m_progress_bar->SetProgress(0);
        body->AddSpacer(FromDIP(12));
        body->Add(m_progress_bar, 0, wxEXPAND);
    }

    // Footer: pill Cancel (kit Text variant) added to the shell footer row; the
    // existing wxEVT_LEFT_DOWN cancel handler (state + timers) is unchanged. When
    // the operation can't be aborted, hide the whole footer (no empty divider)
    // and the header close so it can't be dismissed from the chrome.
    if (HasPDFlag(wxPD_CAN_ABORT)) {
        m_button_cancel = new Button(this, _L("Cancel"));
        m_button_cancel->SetVariant(Button::Variant::Text);
        m_button_cancel->SetButtonSize(Button::Size::Medium);
        m_button_cancel->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) {
            if (m_state == Finished) {
                event.Skip();
            } else {
                m_state = Canceled;
                DisableAbort();
                m_button_cancel->Enable(false);
                DisableSkip();
                OnCancel();
                m_timeStop = wxGetCurrentTime();
            }
        });
        AddFooterButton(m_button_cancel);
    } else {
        ShowHeaderClose(false);
        ShowFooter(false);
    }

    wxGetApp().UpdateDlgDarkUI(this);

    Layout();
    Fit();
    Centre(wxCENTER_FRAME | wxBOTH);
    DisableOtherWindows();
    FormatString(message);

    Show();
    Enable();
    if (m_elapsed) { SetTimeLabel(0, m_elapsed); }

    Update();
    return true;
}

void ProgressDialog::UpdateTimeEstimates(int value, unsigned long &elapsedTime, unsigned long &estimatedTime, unsigned long &remainingTime)
{
    unsigned long elapsed = wxGetCurrentTime() - m_timeStart;
    if (value != 0 && (m_last_timeupdate < elapsed || value == m_maximum)) {
        m_last_timeupdate       = elapsed;
        unsigned long estimated = m_break + (unsigned long) (((double) (elapsed - m_break) * m_maximum) / ((double) value));
        if (estimated > m_display_estimated && m_ctdelay >= 0) {
            ++m_ctdelay;
        } else if (estimated < m_display_estimated && m_ctdelay <= 0) {
            --m_ctdelay;
        } else {
            m_ctdelay = 0;
        }
        if (m_ctdelay >= m_delay             // enough confirmations for a higher value
            || m_ctdelay <= (m_delay * -1)   // enough confirmations for a lower value
            || value == m_maximum            // to stay consistent
            || elapsed > m_display_estimated // to stay consistent
            || (elapsed > 0 && elapsed < 4)  // additional updates in the beginning
        ) {
            m_display_estimated = estimated;
            m_ctdelay           = 0;
        }
    }

    if (value != 0) {
        long display_remaining = m_display_estimated - elapsed;
        if (display_remaining < 0) { display_remaining = 0; }

        estimatedTime = m_display_estimated;
        remainingTime = display_remaining;
    }

    elapsedTime = elapsed;
}

// static
wxString ProgressDialog::GetFormattedTime(unsigned long timeInSec)
{
    wxString timeAsHMS;

    if (timeInSec == (unsigned long) -1) {
        timeAsHMS = wxGetTranslation("Unknown");
    } else {
        unsigned hours   = timeInSec / 3600;
        unsigned minutes = (timeInSec % 3600) / 60;
        unsigned seconds = timeInSec % 60;
        timeAsHMS.Printf("%u:%02u:%02u", hours, minutes, seconds);
    }

    return timeAsHMS;
}

void ProgressDialog::EnsureActiveEventLoopExists()
{
    if (!wxEventLoopBase::GetActive()) {
        m_tempEventLoop = new wxEventLoop;
        wxEventLoop::SetActive(m_tempEventLoop);
    }
}

wxStaticText *ProgressDialog::CreateLabel(const wxString &text, wxSizer *sizer)
{
    wxStaticText *label = new wxStaticText(this, wxID_ANY, text);
    wxStaticText *value = new wxStaticText(this, wxID_ANY, wxGetTranslation("unknown"));

    // select placement most native or nice on target GUI
#if defined(__WXMSW__) || defined(__WXMAC__) || defined(__WXGTK20__)
    // value and time centered in one row
    sizer->Add(label, 1, wxALIGN_RIGHT | wxTOP | wxRIGHT, LAYOUT_MARGIN);
    sizer->Add(value, 1, wxALIGN_LEFT | wxTOP, LAYOUT_MARGIN);
#else
    // value and time to the right in one row
    sizer->Add(label);
    sizer->Add(value, 0, wxLEFT, LAYOUT_MARGIN);
#endif

    return value;
}

// ----------------------------------------------------------------------------
// ProgressDialog operations
// ----------------------------------------------------------------------------

bool ProgressDialog::Update(int value, const wxString &newmsg, bool *skip)
{
    if (!DoBeforeUpdate(skip)) return false;

    wxCHECK_MSG(m_msg || m_progress_bar, false, "dialog should be fully created");

#ifdef __WXMSW__
    value /= m_factor;
#endif // __WXMSW__

    wxASSERT_MSG(value <= m_maximum, wxT("invalid progress value"));
    if (m_progress_bar) {
        m_progress_bar->SetProgress(value);
        m_progress_value = value;
    }

    UpdateMessage(newmsg);

    if ((m_elapsed || m_remaining || m_estimated) && (value != 0)) {
        unsigned long elapsed;
        unsigned long display_remaining;

        UpdateTimeEstimates(value, elapsed, m_display_estimated, display_remaining);

        SetTimeLabel(elapsed, m_elapsed);
        SetTimeLabel(m_display_estimated, m_estimated);
        SetTimeLabel(display_remaining, m_remaining);
    }

    if (value == m_maximum) {
        if (m_state == Finished) {
            // ignore multiple calls to Update(m_maximum): it may sometimes be
            // troublesome to ensure that Update() is not called twice with the
            // same value (e.g. because of the rounding errors) and if we don't
            // return now we're going to generate asserts below
            return true;
        }

        // so that we return true below and that out [Cancel] handler knew what
        // to do
        m_state = Finished;
        if (!HasPDFlag(wxPD_AUTO_HIDE)) {
            EnableClose();
            DisableSkip();
            // The count-down is finished and we're about to show modally: make
            // the header circular close available again so the user can dismiss
            // it (the MD3 stand-in for the native EnableCloseButton()); it was
            // hidden earlier for an uncancelable dialog.
            ShowHeaderClose(true);

            if (newmsg.empty()) {
                // also provide the finishing message if the application didn't
                m_msg->SetLabel(wxGetTranslation("Done."));
            }

            // allow the window to repaint:
            // NOTE: since we yield only for UI events with this call, there
            //       should be no side-effects
            Yield();

            // NOTE: this call results in a new event loop being created
            //       and to a call to ProcessPendingEvents() (which may generate
            //       unwanted re-entrancies).
            (void) ShowModal();
        } else // auto hide
        {
            // reenable other windows before hiding this one because otherwise
            // Windows wouldn't give the focus back to the window which had
            // been previously focused because it would still be disabled
            ReenableOtherWindows();

            Hide();
        }
    } else // not at maximum yet
    {
        DoAfterUpdate();
    }

    // update the display in case yielding above didn't do it
    Update();

    return m_state != Canceled;
}

bool ProgressDialog::Pulse(const wxString &newmsg, bool *skip)
{
    if (!DoBeforeUpdate(skip)) return false;

    wxCHECK_MSG(m_msg || m_progress_bar, false, "dialog should be fully created");

    // The kit ProgressBar is determinate only; there is no indeterminate pulse
    // to render. Callers that pulse before a real value (e.g. app restart) are
    // unaffected — the next Update() sets a concrete value.

    UpdateMessage(newmsg);

    if (m_elapsed || m_remaining || m_estimated) {
        unsigned long elapsed = wxGetCurrentTime() - m_timeStart;

        SetTimeLabel(elapsed, m_elapsed);
        SetTimeLabel((unsigned long) -1, m_estimated);
        SetTimeLabel((unsigned long) -1, m_remaining);
    }

    DoAfterUpdate();

    return m_state != Canceled;
}

bool ProgressDialog::WasCanceled() const { return m_state == Canceled; }

bool ProgressDialog::DoBeforeUpdate(bool *skip)
{
    // we have to yield because not only we want to update the display but
    // also to process the clicks on the cancel and skip buttons
    // NOTE: using YieldFor() this call shouldn't give re-entrancy problems
    //       for event handlers not interested to UI/user-input events.
    Yield();

    Update();

    if (m_skip && skip && !*skip) {
        *skip  = true;
        m_skip = false;
        EnableSkip();
    }

    return m_state != Canceled;
}

void ProgressDialog::DoAfterUpdate()
{
    // allow the window to repaint:
    // NOTE: since we yield only for UI events with this call, there
    //       should be no side-effects
    Yield();
}

void ProgressDialog::Resume()
{
    m_state   = Continue;
    m_ctdelay = m_delay; // force an update of the elapsed/estimated/remaining time
    m_break += wxGetCurrentTime() - m_timeStop;

    EnableAbort();
    m_button_cancel->Enable(true);
    EnableSkip();
    m_skip = false;
}

bool ProgressDialog::Show(bool show)
{
    // reenable other windows before hiding this one because otherwise
    // Windows wouldn't give the focus back to the window which had
    // been previously focused because it would still be disabled
    if (!show) ReenableOtherWindows();
    return wxDialog::Show(show);
}

int ProgressDialog::GetValue() const
{
    wxCHECK_MSG(m_progress_bar, -1, "dialog should be fully created");

    return m_progress_value;
}

int ProgressDialog::GetRange() const { return m_maximum; }

wxString ProgressDialog::GetMessage() const { return m_msg->GetLabel(); }

void ProgressDialog::SetRange(int maximum)
{
    wxCHECK_RET(m_progress_bar, "dialog should be fully created");

    wxCHECK_RET(maximum > 0, "Invalid range");

    SetMaximum(maximum);

    // Mirror the (MSW-scaled) maximum onto the bar so the fill proportion stays
    // correct; the bar exposes m_max directly (no SetRange getter/setter).
    m_progress_bar->m_max = m_maximum;
    m_progress_bar->Refresh();
}

void ProgressDialog::set_panel_height(int height) {
    m_simplebook->SetSize(wxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x, height));
    m_simplebook->SetMinSize(wxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x, height));
    m_panel_2line->SetSize(wxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x, height));
    m_panel_2line->SetMinSize(wxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x, height));
    m_msg_2line->SetSize(wxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x, height));
    m_msg_2line->SetMinSize(wxSize(PROGRESSDIALOG_SIMPLEBOOK_SIZE.x, height));
}

void ProgressDialog::SetMaximum(int maximum)
{
    m_maximum = maximum;

#if defined(__WXMSW__)
    // we can't have values > 65,536 in the progress control under Windows, so
    // scale everything down
    m_factor = m_maximum / 65536 + 1;
    m_maximum /= m_factor;
#endif // __WXMSW__
}

bool ProgressDialog::WasCancelled() const { return HasPDFlag(wxPD_CAN_ABORT) && m_state == Canceled; }

bool ProgressDialog::WasSkipped() const { return HasPDFlag(wxPD_CAN_SKIP) && m_skip; }

// static
void ProgressDialog::SetTimeLabel(unsigned long val, wxStaticText *label)
{
    if (label) {
        wxString s;

        if (val != (unsigned long) -1) {
            s = GetFormattedTime(val);
        } else {
            s = wxGetTranslation("Unknown");
        }

        if (s != label->GetLabel()) label->SetLabel(s);
    }
}

// ----------------------------------------------------------------------------
// event handlers
// ----------------------------------------------------------------------------

void ProgressDialog::OnCancel(wxCommandEvent &event)
{
    if (m_state == Finished) {
        // this means that the count down is already finished and we're being
        // shown as a modal dialog - so just let the default handler do the job
        event.Skip();
    } else {
        // request to cancel was received, the next time Update() is called we
        // will handle it
        m_state = Canceled;

        // update the buttons state immediately so that the user knows that the
        // request has been noticed
        DisableAbort();
        DisableSkip();

        OnCancel();

        // save the time when the dialog was stopped
        m_timeStop = wxGetCurrentTime();
    }
}

void ProgressDialog::OnSkip(wxCommandEvent &WXUNUSED(event))
{
    DisableSkip();
    m_skip = true;
}

void ProgressDialog::OnClose(wxCloseEvent &event)
{
    if (m_state == Uncancelable) {
        // can't close this dialog
        event.Veto();
    } else if (m_state == Finished) {
        // let the default handler close the window as we already terminated
        event.Skip();
    } else {
        // next Update() will notice it
        m_state = Canceled;
        DisableAbort();
        DisableSkip();
        OnCancel();

        m_timeStop = wxGetCurrentTime();
    }
}

void ProgressDialog::OnHeaderClose()
{
    // The MD3 header circular close replaces the native window [x]; mirror
    // OnClose()'s semantics rather than the base EndModal(wxID_CANCEL).
    if (m_state == Uncancelable) {
        // can't close this dialog (the affordance is normally hidden in this state)
        return;
    } else if (m_state == Finished) {
        // count-down finished; dismiss the (possibly modal) finished dialog like
        // the native [x] would.
        if (IsModal())
            EndModal(wxID_OK);
        else
            Hide();
    } else {
        // request to cancel; the next Update() notices m_state == Canceled.
        m_state = Canceled;
        DisableAbort();
        if (m_button_cancel)
            m_button_cancel->Enable(false);
        DisableSkip();
        OnCancel();
        m_timeStop = wxGetCurrentTime();
    }
}

// ----------------------------------------------------------------------------
// destruction
// ----------------------------------------------------------------------------

ProgressDialog::~ProgressDialog()
{
    // normally this should have been already done, but just in case
    ReenableOtherWindows();

    if (m_tempEventLoop) {
        // If another event loop has been installed as active during the life
        // time of this object, we shouldn't deactivate it, but we also can't
        // delete our m_tempEventLoop in this case because it risks leaving the
        // new event loop with a dangling pointer, which it will set back as
        // the active loop when it exits, resulting in a crash. So we have no
        // choice but to just leak this pointer then, which is, of course, bad
        // and usually easily avoidable by just destroying the progress dialog
        // sooner, so warn the programmer about it.
        wxCHECK_RET(wxEventLoopBase::GetActive() == m_tempEventLoop, "current event loop must not be changed during "
                                                                     "ProgressDialog lifetime");

        wxEventLoopBase::SetActive(NULL);
        delete m_tempEventLoop;
    }
}

void ProgressDialog::DisableOtherWindows()
{
    if (HasPDFlag(wxPD_APP_MODAL)) {
#if defined(__WXOSX__)
        if (m_parentTop) m_parentTop->Disable();
        m_winDisabler = NULL;
#else
        m_winDisabler = new wxWindowDisabler(this);
#endif
    } else {
        if (m_parentTop) m_parentTop->Disable();
        m_winDisabler = NULL;
    }

}

void ProgressDialog::ReenableOtherWindows()
{
    if (HasPDFlag(wxPD_APP_MODAL)) {
#if defined(__WXOSX__)
        if (m_parentTop) m_parentTop->Enable();
#else
        wxDELETE(m_winDisabler);
#endif
    } else {
        if (m_parentTop) m_parentTop->Enable();
    }
}

// ----------------------------------------------------------------------------
// private functions
// ----------------------------------------------------------------------------

void ProgressDialog::EnableSkip(bool enable)
{
    if (HasPDFlag(wxPD_CAN_SKIP)) {
        if (m_btnSkip) m_btnSkip->Enable(enable);
    }
}

void ProgressDialog::EnableAbort(bool enable)
{
    if (HasPDFlag(wxPD_CAN_ABORT)) {
        if (m_btnAbort) m_btnAbort->Enable(enable);
    }
}

void ProgressDialog::EnableClose()
{
    if (HasPDFlag(wxPD_CAN_ABORT)) {
        if (m_btnAbort) {
            m_btnAbort->Enable();
            m_btnAbort->SetLabel(wxGetTranslation("Close"));
        }
    }
}

void ProgressDialog::UpdateMessage(const wxString &newmsg)
{
    if (!newmsg.empty() && newmsg != m_msg->GetLabel()) {
        const wxSize sizeOld = m_msg->GetSize();

        FormatString(newmsg);
        Fit();
        // m_msg->SetLabel(newmsg);

        if (m_msg->GetSize().x > sizeOld.x) {
            // Resize the dialog to fit its new, longer contents instead of
            // just truncating it.
            Fit();
        }

        // allow the window to repaint:
        // NOTE: since we yield only for UI events with this call, there
        //       should be no side-effects
        Yield();
    }
}

void ProgressDialog::Yield()
{
    if (!m_need_yield) return;
    wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
}

}} // namespace Slic3r::GUI
