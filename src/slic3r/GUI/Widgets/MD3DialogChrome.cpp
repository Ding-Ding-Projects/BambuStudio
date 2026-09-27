#include "MD3DialogChrome.hpp"

#include "Label.hpp"
#include "MD3Motion.hpp"

#include "slic3r/GUI/I18N.hpp"
#include "MD3Tokens.hpp"
#include "MaterialIcon.hpp"
#include "StateColor.hpp"

#include <wx/dcbuffer.h>
#include <wx/dialog.h>
#include <wx/sizer.h>

#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace {
constexpr int kHeight    = 44;
constexpr int kCloseHit  = 44; // a11y minimum touch target
constexpr int kGlyphPx   = 20;
} // namespace

MD3DialogCaption::MD3DialogCaption(wxDialog *dialog, const wxString &title)
    : wxPanel(dialog, wxID_ANY)
    , m_dialog(dialog)
{
    const wxColour bg = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    SetBackgroundColour(bg);
    SetMinSize(wxSize(-1, FromDIP(kHeight)));
    SetName(title); // screen readers announce the dialog purpose

    auto *sizer = new wxBoxSizer(wxHORIZONTAL);
    m_title = new Label(this, Label::Head_14, wxEmptyString);
    m_title->SetLabelText(title); // '&' renders literally, never as a mnemonic
    m_title->SetBackgroundColour(bg);
    m_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    sizer->Add(m_title, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(20));

    m_close = new wxPanel(this, wxID_ANY, wxDefaultPosition,
                          wxSize(FromDIP(kCloseHit), FromDIP(kCloseHit)));
    m_close->SetBackgroundStyle(wxBG_STYLE_PAINT);
    m_close->SetName(_L("Close"));
    m_close->SetToolTip(_L("Close"));
    m_close->Bind(wxEVT_PAINT, &MD3DialogCaption::OnPaintClose, this);
    m_close->Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &) { m_close_hover = true;  m_close->Refresh(); });
    m_close->Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &) { m_close_hover = false; m_close->Refresh(); });
    m_close->Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) {
        // Same routing as the native X: EndModal for modal dialogs, Close otherwise.
        if (m_dialog->IsModal()) m_dialog->EndModal(wxID_CANCEL);
        else                     m_dialog->Close();
    });
    m_close->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_RETURN || e.GetKeyCode() == WXK_SPACE) {
            if (m_dialog->IsModal()) m_dialog->EndModal(wxID_CANCEL);
            else                     m_dialog->Close();
            return;
        }
        e.Skip();
    });
    sizer->Add(m_close, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    SetSizer(sizer);

    // The strip (and the title label over it) is the drag region.
    auto begin_drag = [this](wxMouseEvent &e) {
#ifdef _WIN32
        ::ReleaseCapture();
        ::SendMessageW((HWND) m_dialog->GetHWND(), WM_NCLBUTTONDOWN, HTCAPTION, 0);
#else
        e.Skip();
#endif
    };
    Bind(wxEVT_LEFT_DOWN, begin_drag);
    m_title->Bind(wxEVT_LEFT_DOWN, begin_drag);
}

void MD3DialogCaption::OnPaintClose(wxPaintEvent &)
{
    wxAutoBufferedPaintDC dc(m_close);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    const wxSize sz = m_close->GetClientSize();
    if (m_close_hover) {
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::SurfaceContainerHighest)));
        const int d = std::min(sz.x, sz.y) - FromDIP(8);
        dc.DrawEllipse((sz.x - d) / 2, (sz.y - d) / 2, d, d);
    }
    const wxColour tint = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    if (MaterialIcon::available()) {
        const int px = FromDIP(kGlyphPx);
        const wxSize gs = MaterialIcon::measure(dc, MaterialIcon::Close, px);
        MaterialIcon::draw(dc, MaterialIcon::Close, px, tint,
                           wxPoint((sz.x - gs.x) / 2, (sz.y - gs.y) / 2));
    } else {
        dc.SetTextForeground(tint);
        const wxSize te = dc.GetTextExtent("X");
        dc.DrawText("X", (sz.x - te.x) / 2, (sz.y - te.y) / 2);
    }
}

void MD3DialogCaption::Adopt(wxDialog *dialog, const wxString &title)
{
    if (dialog == nullptr)
        return;
    // Content height before the frame changes: restored below so the body
    // keeps exactly the space the ctor laid out for it.
    const wxSize client = dialog->GetClientSize();

    long style = dialog->GetWindowStyleFlag();
    style &= ~(wxCAPTION | wxCLOSE_BOX | wxSYSTEM_MENU | wxMAXIMIZE_BOX | wxMINIMIZE_BOX);
    style |= wxBORDER_NONE;
    dialog->SetWindowStyleFlag(style); // wxMSW applies SWP_FRAMECHANGED itself

    const wxString caption_title = title.empty() ? dialog->GetTitle() : title;
    auto *caption = new MD3DialogCaption(dialog, caption_title);
    wxSizer *old = dialog->GetSizer();
    auto *outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(caption, 0, wxEXPAND);
    if (old != nullptr) {
        dialog->SetSizer(nullptr, false); // detach without deleting
        outer->Add(old, 1, wxEXPAND);
    }
    dialog->SetSizer(outer);
    dialog->SetClientSize(client.x, client.y + caption->GetMinSize().y);
    dialog->Layout();
    FinishChrome(dialog);
}

int MD3DialogCaption::Height(wxWindow *ref)
{
    return ref != nullptr ? ref->FromDIP(kHeight) : kHeight;
}

void MD3DialogCaption::FinishChrome(wxDialog *dialog)
{
    if (dialog == nullptr)
        return;
#ifdef _WIN32
    // DWM rounded corners for the borderless frame (no-op before Win11).
    const int DWMWA_WINDOW_CORNER_PREFERENCE_ = 33; // DWMWA_WINDOW_CORNER_PREFERENCE
    const int DWMWCP_ROUND_ = 2;                    // DWMWCP_ROUND
    int pref = DWMWCP_ROUND_;
    ::DwmSetWindowAttribute((HWND) dialog->GetHWND(), DWMWA_WINDOW_CORNER_PREFERENCE_,
                            &pref, sizeof(pref));
#endif
    MD3::Motion::FadeIn(dialog, MD3::Motion::medium1);
}
