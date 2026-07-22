#include "MsgDialog.hpp"

#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/statbmp.h>
#include <wx/scrolwin.h>
#include <wx/clipbrd.h>
#include <wx/checkbox.h>
#include <wx/html/htmlwin.h>
#include <wx/textctrl.h>

#include <boost/algorithm/string/replace.hpp>

#include "Widgets/Label.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "libslic3r/libslic3r.h"
#include "libslic3r/Utils.hpp"
#include "GUI.hpp"
#include "I18N.hpp"
//#include "ConfigWizard.hpp"
#include "wxExtensions.hpp"
#include "slic3r/GUI/MainFrame.hpp"
#include "GUI_App.hpp"
#define MSG_DLG_MAX_SIZE wxSize(-1, FromDIP(464))//notice:ban setting the maximum width value
namespace Slic3r {
namespace GUI {

// Preserve the legacy behaviour of parenting a message dialog to the main frame
// when no explicit parent is given (MainFrame is fully defined in this TU, so
// the upcast is valid here even though the shell primitive avoids the
// dependency).
static wxWindow *msg_parent(wxWindow *parent)
{
    return parent ? parent : dynamic_cast<wxWindow *>(wxGetApp().mainframe);
}

// Map the message style flags to the header status glyph (replaces the former
// left-hand raster logo). Non-status dialogs default to the info glyph.
static MaterialIcon::Glyph msg_glyph_for_style(long style)
{
    if (style & wxAPPLY)            return MaterialIcon::TaskAlt;
    if (style & wxICON_ERROR)       return MaterialIcon::Error;
    if (style & wxICON_WARNING)     return MaterialIcon::Warning;
    if (style & wxICON_INFORMATION) return MaterialIcon::Info;
    if (style & wxICON_QUESTION)    return MaterialIcon::Help;
    return MaterialIcon::Info;
}

MsgDialog::MsgDialog(wxWindow *parent, const wxString &title, const wxString &headline, long style, wxBitmap /*bitmap*/, const wxString &forward_str)
    : MD3Dialog(msg_parent(parent), title, headline, msg_glyph_for_style(style))
    , boldfont(wxGetApp().normal_font())
    , m_forward_str(forward_str)
{
    boldfont.SetWeight(wxFONTWEIGHT_BOLD);

    // The shell (MD3Dialog) owns the body + footer sizers; the message content
    // and action buttons are added onto them, preserving the legacy member names
    // so the ~12 subclasses keep using content_sizer / btn_sizer unchanged.
    content_sizer = GetContentSizer();
    btn_sizer     = GetFooterSizer();

    // "Don't show again" checkbox sits at the far left of the footer row (kit
    // footer: toggle/secondary left, actions right). It is inserted before the
    // shell's leading stretch so the action buttons stay right-aligned.
    m_dsa_sizer = new wxBoxSizer(wxHORIZONTAL);
    btn_sizer->Insert(0, m_dsa_sizer, 0, wxALIGN_CENTER_VERTICAL);

    // Error dialogs get the Error-toned icon tile; every other status keeps the
    // kit's PrimaryContainer tile, with the status glyph carrying the meaning.
    if (style & wxICON_ERROR)
        SetHeaderAccent(MD3::Role::ErrorContainer, MD3::Role::OnErrorContainer);

    apply_style(style);
    wxGetApp().UpdateDlgDarkUI(this);
}

 MsgDialog::~MsgDialog()
{
    for (auto mb : m_buttons) { delete mb.second->buttondata ; delete mb.second; }
}

void MsgDialog::show_dsa_button(wxString const &title)
{
    m_checkbox_dsa = new CheckBox(this);
    m_dsa_sizer->Add(m_checkbox_dsa, 0, wxALL | wxALIGN_CENTER, FromDIP(2));
    m_checkbox_dsa->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent& e) {
        auto event = wxCommandEvent(EVT_CHECKBOX_CHANGE);
        event.SetInt(m_checkbox_dsa->GetValue()?1:0);
        event.SetEventObject(this);
        wxPostEvent(this, event);
        e.Skip();
    });

    auto  m_text_dsa = new wxStaticText(this, wxID_ANY, title.IsEmpty() ? _L("Don't show again") : title, wxDefaultPosition, wxDefaultSize, 0);
    m_dsa_sizer->Add(m_text_dsa, 0, wxALL | wxALIGN_CENTER, FromDIP(2));
    m_text_dsa->SetFont(::Label::Body_13);
    m_text_dsa->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    btn_sizer->Layout();
    Fit();
}

bool MsgDialog::get_checkbox_state()
{
    if (m_checkbox_dsa) {
        return m_checkbox_dsa->GetValue();
    }
    return false;
}

void MsgDialog::on_dpi_changed(const wxRect &suggested_rect)
 {
     // Kit footer buttons self-manage their pill radius / height / padding, so a
     // DPI change is handled by re-running their MD3 style via Rescale().
     MsgButtonsHash::iterator i = m_buttons.begin();
     while (i != m_buttons.end()) {
         MsgButton *bd = i->second;
         if (bd && bd->buttondata && bd->buttondata->button)
             bd->buttondata->button->Rescale();
         ++i;
     }
     // Re-apply the rounded window region for the (possibly) rescaled size.
     UpdateShape();
 }

void MsgDialog::SetButtonLabel(wxWindowID btn_id, const wxString& label, bool set_focus/* = false*/)
{
    if (Button* btn = get_button(btn_id)) {
        btn->SetLabel(label);
        if (set_focus)
            btn->SetFocus();
    }
}

Button* MsgDialog::add_button(wxWindowID btn_id, bool set_focus /*= false*/, const wxString& label/* = wxString()*/)
{
    Button* btn = new Button(this, label, "", 0, 0, btn_id);

    // Kit footer geometry (containment/Dialog + actions/Button): the default /
    // focused action is a Filled primary pill; secondary actions are Text
    // buttons. The MD3 variant path applies the pill radius (height/2), kit
    // medium height (42) and the size-matched label font, replacing the old
    // fixed 58/76/90 x 24 sizing + first-focused-green heuristic.
    btn->SetVariant(set_focus ? Button::Variant::Filled : Button::Variant::Text);
    btn->SetButtonSize(Button::Size::Medium);

    if (set_focus)
        btn->SetFocus();
    AddFooterButton(btn);
    btn->Bind(wxEVT_BUTTON, [this, btn_id](wxCommandEvent&) { EndModal(btn_id); });

    MsgButton *mb = new MsgButton;
    ButtonData *bd = new ButtonData;

    bd->button = btn;
    bd->type   = ButtonSizeNormal;

    mb->id        = wxString::Format("%d", m_buttons.size());
    mb->buttondata = bd;
    m_buttons[ wxString::Format("%d", m_buttons.size())] = mb;
    return btn;
};

Button* MsgDialog::get_button(wxWindowID btn_id){
    return static_cast<Button*>(FindWindowById(btn_id, this));
}

void MsgDialog::apply_style(long style)
{
    bool focus = (style & wxNO_DEFAULT) == 0;
    if (style & wxFORWARD)
        add_button(wxFORWARD, true, _L("Go to") + " " + m_forward_str);
    if (style & wxOK) {
        if (style & wxFORWARD) { add_button(wxID_OK, false, _L("Later")); }
        else {
            add_button(wxID_OK, focus, _L("OK"));
        }
    }
    if (style & wxYES)      add_button(wxID_YES, focus, _L("Yes"));
    if (style & wxNO)       add_button(wxID_NO, false,_L("No"));
    if (style & wxCANCEL)   add_button(wxID_CANCEL, false, _L("Cancel"));

    // The dialog status glyph is now the header icon tile (derived from the same
    // style flags at construction via msg_glyph_for_style); the former left-hand
    // raster logo no longer exists.
}

void MsgDialog::finalize()
{
    Layout();
    Fit();
    CenterOnParent();
    // The shell is borderless + shaped; re-derive the rounded region for the
    // final fitted size (the wxEVT_SIZE hook covers later re-fits).
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
}


// Text shown as HTML, so that mouse selection and Ctrl-V to copy will work.
static void add_msg_content(wxWindow   *parent,
                            wxBoxSizer *content_sizer,
                            wxString    msg,
                            bool        monospaced_font = false,
                            bool        is_marked_msg   = false,
                            const wxString &link_text = "",
                            std::function<void(const wxString &)> link_callback = nullptr)
{
    wxHtmlWindow* html = new wxHtmlWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHW_SCROLLBAR_AUTO);
    html->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));

    // count lines in the message
    int msg_lines = 0;
    if (!monospaced_font) {
        int line_len = 55;// count of symbols in one line
        int start_line = 0;
        for (auto i = msg.begin(); i != msg.end(); ++i) {
            if (*i == '\n') {
                int cur_line_len = i - msg.begin() - start_line;
                start_line = i - msg.begin();
                if (cur_line_len == 0 || line_len > cur_line_len)
                    msg_lines++;
                else
                    msg_lines += std::lround((double)(cur_line_len) / line_len);
            }
        }
        msg_lines++;
    }

    wxFont      font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    wxFont      monospace = wxGetApp().code_font();
    wxColour    text_clr = wxGetApp().get_label_clr_default();
    wxColour    bgr_clr = parent->GetBackgroundColour(); //wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    auto        text_clr_str = wxString::Format(wxT("#%02X%02X%02X"), text_clr.Red(), text_clr.Green(), text_clr.Blue());
    auto        bgr_clr_str = wxString::Format(wxT("#%02X%02X%02X"), bgr_clr.Red(), bgr_clr.Green(), bgr_clr.Blue());
    const int   font_size = font.GetPointSize();
    int         size[] = { font_size, font_size, font_size, font_size, font_size, font_size, font_size };
    html->SetFonts(font.GetFaceName(), monospace.GetFaceName(), size);
    html->SetBorders(2);

    // calculate html page size from text
    wxSize page_size;
    int em = wxGetApp().em_unit();
    if (!wxGetApp().mainframe) {
        // If mainframe is nullptr, it means that GUI_App::on_init_inner() isn't completed
        // (We just show information dialog about configuration version now)
        // And as a result the em_unit value wasn't created yet
        // So, calculate it from the scale factor of Dialog
#if defined(__WXGTK__)
        // Linux specific issue : get_dpi_for_window(this) still doesn't responce to the Display's scale in new wxWidgets(3.1.3).
        // So, initialize default width_unit according to the width of the one symbol ("m") of the currently active font of this window.
        em = std::max<size_t>(10, parent->GetTextExtent("m").x - 1);
#else
        double scale_factor = (double)get_dpi_for_window(parent) / (double)DPI_DEFAULT;
        em = std::max<size_t>(10, 10.0f * scale_factor);
#endif // __WXGTK__
    }
    auto info_width = 68 * em;
    // if message containes the table
    if (msg.Contains("<tr>")) {
        int lines = msg.Freq('\n') + 1;
        int pos = 0;
        while (pos < (int)msg.Len() && pos != wxNOT_FOUND) {
            pos = msg.find("<tr>", pos + 1);
            lines += 2;
        }
        int page_height = std::min(int(font.GetPixelSize().y + 2) * lines, info_width);
        page_size       = wxSize(info_width, page_height);
    }
    else {
        wxClientDC dc(parent);
        wxSize     msg_sz = dc.GetMultiLineTextExtent(msg);

        page_size = wxSize(std::min(msg_sz.GetX(), info_width), std::min(msg_sz.GetY(), info_width));
        // Extra line breaks in message dialog
        if (link_text.IsEmpty() && !link_callback && is_marked_msg == false) {//for common text
            html->Destroy();
            if (msg_sz.GetX() < info_width) {//No need for line breaks
                info_width = msg_sz.GetX();
            }
            wxScrolledWindow *scrolledWindow = new wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
            scrolledWindow->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));
            scrolledWindow->SetScrollRate(0, 20);
            scrolledWindow->EnableScrolling(false, true);
            wxBoxSizer *sizer_scrolled = new wxBoxSizer(wxHORIZONTAL);
            Label *wrapped_text = new Label(scrolledWindow, font, msg, LB_AUTO_WRAP, wxSize(info_width, -1));
            wrapped_text->SetMinSize(wxSize(info_width, -1));
            wrapped_text->SetMaxSize(wxSize(info_width, -1));
            wrapped_text->Wrap(info_width);
            sizer_scrolled->Add(wrapped_text, wxALIGN_LEFT ,0);
            sizer_scrolled->AddSpacer(5);
            sizer_scrolled->AddStretchSpacer();
            scrolledWindow->SetSizer(sizer_scrolled);
            auto info_height = 48 * em;
            if (sizer_scrolled->GetMinSize().GetHeight() < info_height) {
                info_height = sizer_scrolled->GetMinSize().GetHeight();
            }
            scrolledWindow->SetMinSize(wxSize(info_width, info_height));
            scrolledWindow->SetMaxSize(wxSize(info_width, info_height));
            scrolledWindow->FitInside();
            content_sizer->Add(scrolledWindow, 1, wxEXPAND | wxRIGHT, 8);
            return;
        }
    }
    html->SetMinSize(page_size);

    std::string msg_escaped = xml_escape(msg.ToUTF8().data(), is_marked_msg);
    boost::replace_all(msg_escaped, "\r\n", "<br>");
    boost::replace_all(msg_escaped, "\n", "<br>");
    if (monospaced_font)
        // Code formatting will be preserved. This is useful for reporting errors from the placeholder parser.
        msg_escaped = std::string("<pre><code>") + msg_escaped + "</code></pre>";

    if (!link_text.IsEmpty() && link_callback) {
        const wxColour link_clr = StateColor::semantic(MD3::Role::Primary);
        const auto     link_clr_str = wxString::Format(wxT("#%02X%02X%02X"), link_clr.Red(), link_clr.Green(), link_clr.Blue());
        msg_escaped += "<span><a href=\"#\" style=\"color:" + std::string(link_clr_str.ToUTF8().data()) + "; text-decoration:underline;\">" + std::string(link_text.ToUTF8().data()) + "</a></span>";
    }

    html->SetPage("<html><body bgcolor=\"" + bgr_clr_str + "\"><font color=\"" + text_clr_str + "\">" + wxString::FromUTF8(msg_escaped.data()) + "</font></body></html>");
    content_sizer->Add(html, 1, wxEXPAND|wxRIGHT, 8);
    wxGetApp().UpdateDarkUIWin(html);

    html->Bind(wxEVT_HTML_LINK_CLICKED, [=](wxHtmlLinkEvent& event) {
        if (link_callback)
            link_callback(event.GetLinkInfo().GetHref());
    });
}

// ErrorDialog

ErrorDialog::ErrorDialog(wxWindow *parent, const wxString &temp_msg, bool monospaced_font)
    : MsgDialog(parent, wxString::Format(_(L("%s error")), SLIC3R_APP_FULL_NAME),
                        wxString::Format(_(L("%s has encountered an error")), SLIC3R_APP_FULL_NAME), wxOK | wxICON_ERROR)
    , msg(temp_msg)
{
    add_msg_content(this, content_sizer, msg, monospaced_font);

    // The error status is carried by the header icon tile (Error glyph + Error
    // accent, applied by the base from the wxICON_ERROR style).

    SetMaxSize(MSG_DLG_MAX_SIZE);

    finalize();
}

// WarningDialog

WarningDialog::WarningDialog(wxWindow *parent,
                             const wxString& message,
                             const wxString& caption/* = wxEmptyString*/,
                             long style/* = wxOK*/)
    : MsgDialog(parent, caption.IsEmpty() ? wxString::Format(_L("%s warning"), SLIC3R_APP_FULL_NAME) : caption,
                        wxString::Format(_L("%s has a warning")+":", SLIC3R_APP_FULL_NAME), style)
{
    add_msg_content(this, content_sizer, message);
    finalize();
}

PostProcessScriptDialog::PostProcessScriptDialog(wxWindow* parent, const wxString& message, const wxString& script_content)
    : MsgDialog(parent,
        wxString::Format(_L("%s warning"), SLIC3R_APP_FULL_NAME),
        wxString::Format(_L("%s has a warning") + ":", SLIC3R_APP_FULL_NAME),
        wxICON_WARNING)
{
    const int content_width = FromDIP(500);
    wxFont msg_font = wxGetApp().normal_font();
    msg_font.SetPointSize(wxGetApp().code_font().GetPointSize());
    auto* msg = new Label(this, msg_font, message, LB_AUTO_WRAP, wxSize(content_width, -1));
    msg->SetMinSize(wxSize(content_width, -1));
    msg->SetForegroundColour(wxGetApp().get_label_clr_default());
    msg->Wrap(content_width);
    content_sizer->Add(msg, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    m_script_text = new wxTextCtrl(this, wxID_ANY, script_content, wxDefaultPosition,
        wxSize(content_width, FromDIP(140)), wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);
    m_script_text->SetFont(wxGetApp().code_font());
    m_details_expanded = true;
    content_sizer->Add(m_script_text, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    m_toggle_details = new Button(this, _L("Collapse") + " \u2227", "", 0, 0, wxID_ANY);
    m_toggle_details->SetMinSize(wxSize(FromDIP(120), FromDIP(24)));
    m_toggle_details->SetCornerRadius(FromDIP(12));
    m_toggle_details->SetFont(Label::Body_12);
    StateColor btn_bg_white(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainer), StateColor::Pressed),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Hovered),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Surface), StateColor::Normal)
    );
    StateColor btn_bd_white(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Outline), StateColor::Pressed),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Outline), StateColor::Hovered),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::Outline), StateColor::Normal)
    );
    StateColor btn_text_white(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OnSurfaceVariant), StateColor::Pressed),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OnSurfaceVariant), StateColor::Hovered),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::OnSurfaceVariant), StateColor::Normal)
    );
    m_toggle_details->SetBackgroundColor(btn_bg_white);
    m_toggle_details->SetBorderColor(btn_bd_white);
    m_toggle_details->SetTextColor(btn_text_white);
    m_toggle_details->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        m_details_expanded = !m_details_expanded;
        m_script_text->Show(m_details_expanded);
        m_toggle_details->SetLabel(m_details_expanded ? (_L("Collapse") + " \u2227") : (_L("View details") + " \u2228"));
        Layout();
        Fit();
    });
    content_sizer->Add(m_toggle_details, 0, wxBOTTOM, FromDIP(4));

    show_dsa_button();
    add_button(wxID_YES, false, _L("Execute"));
    add_button(wxID_NO, true, _L("Do not execute"));
    if (Button* execute_btn = get_button(wxID_YES)) {
        execute_btn->SetBorderColor(StateColor(StateColor::semantic(MD3::Role::Outline)));
        execute_btn->SetTextColor(StateColor(StateColor::semantic(MD3::Role::OnSurfaceVariant)));
    }
    SetMaxSize(MSG_DLG_MAX_SIZE);
    finalize();
    CallAfter([this]() {
        Layout();
        Fit();
        CenterOnParent();
    });
}

#if 1
// MessageDialog

MessageDialog::MessageDialog(wxWindow* parent,
    const wxString& message,
    const wxString& caption/* = wxEmptyString*/,
    long style /* = wxOK*/,
    const wxString &forward_str /* = wxEmptyString*/,
    const wxString &link_text   /* = wxEmptyString*/,
    std::function<void(const wxString &)> link_callback /* = nullptr*/)
    : MessageDialog(parent, message, caption, style, forward_str, link_text, link_callback, false)
{
}

MessageDialog::MessageDialog(wxWindow* parent,
    const wxString& message,
    const wxString& caption,
    long style,
    const wxString &forward_str,
    const wxString &link_text,
    std::function<void(const wxString &)> link_callback,
    bool is_marked_msg)
    : MsgDialog(parent, caption.IsEmpty() ? wxString::Format(_L("%s info"), SLIC3R_APP_FULL_NAME) : caption, wxEmptyString, style, wxBitmap(),forward_str)
{
    add_msg_content(this, content_sizer, message, false, is_marked_msg, link_text, link_callback);
    SetMaxSize(MSG_DLG_MAX_SIZE);
    finalize();
}


// RichMessageDialog

RichMessageDialog::RichMessageDialog(wxWindow* parent,
    const wxString& message,
    const wxString& caption/* = wxEmptyString*/,
    long style/* = wxOK*/)
    : MsgDialog(parent, caption.IsEmpty() ? wxString::Format(_L("%s info"), SLIC3R_APP_FULL_NAME) : caption, wxEmptyString, style)
{
    add_msg_content(this, content_sizer, message);


    finalize();
}

int RichMessageDialog::ShowModal()
{
    if (!m_checkBoxText.IsEmpty()) {
        show_dsa_button(m_checkBoxText);
        m_checkbox_dsa->SetValue(m_checkBoxValue);
    }
    Layout();

    return wxDialog::ShowModal();
}

bool RichMessageDialog::IsCheckBoxChecked() const
{
    if (m_checkbox_dsa)
        return m_checkbox_dsa->GetValue();

    return m_checkBoxValue;
}
#endif

// InfoDialog
InfoDialog::InfoDialog(wxWindow* parent, const wxString &title, const wxString& msg, bool is_marked_msg/* = false*/, long style/* = wxOK | wxICON_INFORMATION*/)
    : MsgDialog(parent, wxString::Format(_L("%s information"), SLIC3R_APP_FULL_NAME), title, style)
    , msg(msg)
{
    add_msg_content(this, content_sizer, msg, false, is_marked_msg);
    finalize();
}

// InfoDialog
DownloadDialog::DownloadDialog(wxWindow *parent, const wxString &msg, const wxString &title, bool is_marked_msg /* = false*/, long style /* = wxOK | wxICON_INFORMATION*/)
    : MsgDialog(parent, title, msg, style), msg(msg)
{
    add_button(wxID_YES, true, _L("Download"));
    add_button(wxID_CANCEL, true, _L("Skip"));

    finalize();
}


void DownloadDialog::SetExtendedMessage(const wxString &extendedMessage)
{
    add_msg_content(this, content_sizer, msg + "\n" + extendedMessage, false, false);
    Layout();
    Fit();
}

DeleteConfirmDialog::DeleteConfirmDialog(wxWindow *parent, const wxString &title, const wxString &msg)
    : MD3Dialog(parent, title, wxEmptyString, MaterialIcon::Delete)
{
    // Destructive-confirm: Error-toned icon tile; the m_line_top divider is gone
    // (the shell owns the footer's 1px OutlineVariant top border).
    SetHeaderAccent(MD3::Role::ErrorContainer, MD3::Role::OnErrorContainer);

    m_msg_text = new wxStaticText(this, wxID_ANY, msg);
    m_msg_text->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_msg_text->SetFont(::Label::Body_14);
    GetContentSizer()->Add(m_msg_text, 0, wxEXPAND);

    // Footer: Text cancel + Danger (destructive) delete, kit pill + medium height.
    m_cancel_btn = new Button(this, _L("Cancel"));
    m_cancel_btn->SetVariant(Button::Variant::Text);
    m_cancel_btn->SetButtonSize(Button::Size::Medium);
    m_cancel_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &e) { EndModal(wxID_CANCEL); });
    AddFooterButton(m_cancel_btn);

    m_del_btn = new Button(this, _L("Delete"));
    m_del_btn->SetVariant(Button::Variant::Danger);
    m_del_btn->SetButtonSize(Button::Size::Medium);
    m_del_btn->Bind(wxEVT_BUTTON, [this](wxCommandEvent &e) { EndModal(wxID_OK); });
    AddFooterButton(m_del_btn);

    SetMinSize(wxSize(FromDIP(450), -1));
    Layout();
    Fit();
    CenterOnParent();
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
}

DeleteConfirmDialog::~DeleteConfirmDialog() {}


void DeleteConfirmDialog::on_dpi_changed(const wxRect &suggested_rect) { UpdateShape(); }


Newer3mfVersionDialog::Newer3mfVersionDialog(wxWindow *parent, const Semver *file_version, const Semver *cloud_version, wxString new_keys)
    : MD3Dialog(parent, wxString(SLIC3R_APP_FULL_NAME " - ") + _L("Newer 3mf version"), wxEmptyString, MaterialIcon::Info)
    , m_file_version(file_version)
    , m_cloud_version(cloud_version)
    , m_new_keys(new_keys)
{
    // The former left info bitmap + m_line_top divider are replaced by the
    // shell's header icon tile and footer border.
    GetContentSizer()->Add(get_msg_sizer(), 0, wxEXPAND);
    GetFooterSizer()->Add(get_btn_sizer(), 1, wxEXPAND);

    Layout();
    Fit();
    CenterOnParent();
    UpdateShape();
    wxGetApp().UpdateDlgDarkUI(this);
}

wxBoxSizer *Newer3mfVersionDialog::get_msg_sizer()
{
    wxBoxSizer *vertical_sizer     = new wxBoxSizer(wxVERTICAL);
    bool        file_version_newer = (*m_file_version) > (*m_cloud_version);
    wxStaticText *text1;
    wxBoxSizer *     horizontal_sizer = new wxBoxSizer(wxHORIZONTAL);
    wxString    msg_str;
    if (file_version_newer) {
        text1 = new wxStaticText(this, wxID_ANY, _L("The 3mf file version is in Beta and it is newer than the current Bambu Studio version."));
        wxStaticText *   text2       = new wxStaticText(this, wxID_ANY, _L("If you would like to try Bambu Studio Beta, you may click to"));
        wxHyperlinkCtrl *github_link = new wxHyperlinkCtrl(this, wxID_ANY, _L("Download Beta Version"), "https://github.com/bambulab/BambuStudio/releases");
        horizontal_sizer->Add(text2, 0, wxEXPAND, 0);
        horizontal_sizer->Add(github_link, 0, wxEXPAND | wxLEFT, 5);

    } else {
        text1 = new wxStaticText(this, wxID_ANY, _L("The 3mf file version is newer than the current Bambu Studio version."));
        wxStaticText *text2 = new wxStaticText(this, wxID_ANY, _L("Update your Bambu Studio could enable all functionality in the 3mf file."));
        horizontal_sizer->Add(text2, 0, wxEXPAND, 0);
    }
    Semver        app_version = *(Semver::parse(SLIC3R_VERSION));
    wxStaticText *cur_version = new wxStaticText(this, wxID_ANY, _L("Current Version: ") + app_version.to_string());

    vertical_sizer->Add(text1, 0, wxEXPAND | wxTOP, FromDIP(5));
    vertical_sizer->Add(horizontal_sizer, 0, wxEXPAND | wxTOP, FromDIP(5));
    vertical_sizer->Add(cur_version, 0, wxEXPAND | wxTOP, FromDIP(5));
    if (!file_version_newer) {
        wxStaticText *latest_version = new wxStaticText(this, wxID_ANY, _L("Latest Version: ") + m_cloud_version->to_string());
        vertical_sizer->Add(latest_version, 0, wxEXPAND | wxTOP, FromDIP(5));
    }

    wxStaticText *unrecognized_keys = new wxStaticText(this, wxID_ANY, m_new_keys);
    vertical_sizer->Add(unrecognized_keys, 0, wxEXPAND | wxTOP, FromDIP(10));

    return vertical_sizer;
}

wxBoxSizer *Newer3mfVersionDialog::get_btn_sizer()
{
    // Right-aligned kit footer row (this sizer is nested into the shell footer).
    wxBoxSizer *horizontal_sizer = new wxBoxSizer(wxHORIZONTAL);
    horizontal_sizer->Add(0, 0, 1, wxEXPAND, 0);
    bool       file_version_newer = (*m_file_version) > (*m_cloud_version);
    if (!file_version_newer) {
        m_update_btn = new Button(this, _CTX(L_CONTEXT("Update", "Software"), "Software"));
        m_update_btn->SetVariant(Button::Variant::Filled);
        m_update_btn->SetButtonSize(Button::Size::Medium);
        horizontal_sizer->Add(m_update_btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));

        m_update_btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
            EndModal(wxID_OK);
            if (wxGetApp().app_config->has("app", "cloud_software_url")) {
                std::string download_url = wxGetApp().app_config->get("app", "cloud_software_url");
                wxLaunchDefaultBrowser(download_url);
            } else {
                BOOST_LOG_TRIVIAL(info) << __FUNCTION__ << "Bambu Studio conf has no cloud_software_url and file_version: " << m_file_version->to_string()
                                        << " and cloud_version: " << m_cloud_version->to_string();
            }
        });
    }

    if (!file_version_newer) {
        m_later_btn = new Button(this, _L("Not for now"));
        m_later_btn->SetVariant(Button::Variant::Text);
    } else {
        m_later_btn = new Button(this, _L("OK"));
        m_later_btn->SetVariant(Button::Variant::Filled);
    }
    m_later_btn->SetButtonSize(Button::Size::Medium);
    horizontal_sizer->Add(m_later_btn, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(10));
    m_later_btn->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        EndModal(wxID_OK);
    });
    return horizontal_sizer;
}

NetworkErrorDialog::NetworkErrorDialog(wxWindow* parent)
    : MD3Dialog(parent, _L("Server Exception"), wxEmptyString, MaterialIcon::Error)
{
    // Error-toned header tile; the m_line_top divider is gone (the shell owns
    // the footer's 1px OutlineVariant top border).
    SetHeaderAccent(MD3::Role::ErrorContainer, MD3::Role::OnErrorContainer);

    wxBoxSizer* body = GetContentSizer();

    m_text_basic = new Label(this, _L("The server is unable to respond. Please click the link below to check the server status."));
    m_text_basic->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_text_basic->SetMinSize(wxSize(FromDIP(470), -1));
    m_text_basic->SetMaxSize(wxSize(FromDIP(470), -1));
    m_text_basic->Wrap(FromDIP(470));
    m_text_basic->SetFont(::Label::Body_14);
    body->Add(m_text_basic, 0, wxEXPAND);

    m_link_server_state = new wxHyperlinkCtrl(this, wxID_ANY, _L("Check the status of current system services"), "");
    m_link_server_state->SetFont(::Label::Body_13);
    m_link_server_state->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {wxGetApp().link_to_network_check(); });
    m_link_server_state->Bind(wxEVT_ENTER_WINDOW, [this](auto& e) {SetCursor(wxCURSOR_HAND); });
    m_link_server_state->Bind(wxEVT_LEAVE_WINDOW, [this](auto& e) {SetCursor(wxCURSOR_ARROW); });
    body->Add(m_link_server_state, 0, wxTOP, FromDIP(6));

    m_text_proposal = new Label(this, _L("If the server is in a fault state, you can temporarily use offline printing or local network printing."));
    m_text_proposal->SetMinSize(wxSize(FromDIP(470), -1));
    m_text_proposal->SetMaxSize(wxSize(FromDIP(470), -1));
    m_text_proposal->Wrap(FromDIP(470));
    m_text_proposal->SetFont(::Label::Body_14);
    m_text_proposal->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    body->Add(m_text_proposal, 0, wxEXPAND | wxTOP, FromDIP(16));

    m_text_wiki = new wxHyperlinkCtrl(this, wxID_ANY, _L("How to use LAN only mode"), "");
    m_text_wiki->SetFont(::Label::Body_13);
    m_text_wiki->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {wxGetApp().link_to_lan_only_wiki(); });
    m_text_wiki->Bind(wxEVT_ENTER_WINDOW, [this](auto& e) {SetCursor(wxCURSOR_HAND); });
    m_text_wiki->Bind(wxEVT_LEAVE_WINDOW, [this](auto& e) {SetCursor(wxCURSOR_ARROW); });
    body->Add(m_text_wiki, 0, wxTOP, FromDIP(4));

    /*dont show again — sits at the far left of the footer row*/
    auto checkbox = new ::CheckBox(this);
    checkbox->SetValue(false);

    auto checkbox_title = new Label(this, _L("Don't show this dialog again"));
    checkbox_title->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    checkbox_title->SetFont(::Label::Body_14);
    checkbox_title->Wrap(-1);

    checkbox->Bind(wxEVT_TOGGLEBUTTON, [this, checkbox](wxCommandEvent &e) {
        m_show_again = checkbox->GetValue();
        e.Skip();
    });

    auto* dsa = new wxBoxSizer(wxHORIZONTAL);
    dsa->Add(checkbox, 0, wxALIGN_CENTER_VERTICAL);
    dsa->Add(checkbox_title, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(6));
    GetFooterSizer()->Insert(0, dsa, 0, wxALIGN_CENTER_VERTICAL);

    m_button_confirm = new Button(this, _L("Confirm"));
    m_button_confirm->SetVariant(Button::Variant::Filled);
    m_button_confirm->SetButtonSize(Button::Size::Medium);
    m_button_confirm->Bind(wxEVT_LEFT_DOWN, [this](auto& e) {EndModal(wxCLOSE);});
    AddFooterButton(m_button_confirm);

    Layout();
    Fit();
    UpdateShape();
    Centre(wxBOTH);
    wxGetApp().UpdateDlgDarkUI(this);
}


FilamentWarningDialog::FilamentWarningDialog(wxWindow *parent, const wxString &title, std::vector<FilamentWarningInfo> infos)
    : MsgDialog(parent, title.IsEmpty() ? wxString::Format(_L("%s warning"), SLIC3R_APP_FULL_NAME) : title, wxEmptyString, wxOK | wxICON_WARNING), m_messages(infos)
{
    BuildContent();
    finalize();
}



void FilamentWarningDialog::BuildContent()
{
    wxBoxSizer *messages_sizer = new wxBoxSizer(wxVERTICAL);

    int message_count = 0;
    for (int i = 0; i < m_messages.size(); i++)
    {
        const wxString &message  = m_messages[i].info_msg;
        const wxString &wiki_url = m_messages[i].wiki_url;
        if (message_count > 0) { messages_sizer->AddSpacer(FromDIP(10)); }

        if (wiki_url.IsEmpty()) {
            // No wiki link - just display as regular text
            Label *text = new Label(this, message);
            text->SetFont(::Label::Body_12);
            text->Wrap(FromDIP(400));
            messages_sizer->Add(text, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(5));
        } else {
            Label *link = new Label(this, message + " " + _L("Please refer to Wiki before use->"));
            link->SetForegroundColour(StateColor::semantic(MD3::Role::Primary));
            link->SetFont(::Label::Body_12);
            link->Wrap(FromDIP(400));
            link->Bind(wxEVT_ENTER_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_HAND); });
            link->Bind(wxEVT_LEAVE_WINDOW, [this](auto &e) { SetCursor(wxCURSOR_ARROW); });
            link->Bind(wxEVT_LEFT_DOWN, [wiki_url](auto &event) { wxLaunchDefaultBrowser(wiki_url); });
            messages_sizer->Add(link, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(5));
        }
        message_count++;
    }

    content_sizer->Add(messages_sizer, 1, wxEXPAND | wxALL, FromDIP(5));
}


} // namespace GUI

} // namespace Slic3r
