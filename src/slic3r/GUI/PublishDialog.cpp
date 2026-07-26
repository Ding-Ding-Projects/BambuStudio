#include "PublishDialog.hpp"
#include "GUI_App.hpp"

#include <wx/wx.h> 
#include <wx/sizer.h>
#include <wx/statbox.h>
#include "wx/evtloop.h"

#include "libslic3r/Model.hpp"
#include "libslic3r/Polygon.hpp"
#include "MainFrame.hpp"
#include "GUI_App.hpp"


namespace Slic3r {
namespace GUI {

static wxString PUBLISH_STEP_STRING[STEP_COUNT] = {
    _L("Slice all plate to obtain time and filament estimation"),
    _L("Packing project data into 3mf file"),
    _L("Uploading 3mf"),
    _L("Jump to model publish web page")
};

static wxString NOTE_STRING = _L("Note: The preparation may take several minutes. Please be patient.");

PublishDialog::PublishDialog(Plater *plater)
    : MD3Dialog(static_cast<wxWindow *>(wxGetApp().mainframe), _L("Publish"), wxEmptyString, MaterialIcon::Publish)
    , m_plater(plater)
{
    wxBoxSizer *content = GetContentSizer();

    m_step_panel = new wxPanel(this, wxID_ANY);
    wxBoxSizer *step_sizer = create_publish_step_sizer();
    m_step_panel->SetSizer(step_sizer);
    m_step_panel->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainer));

    content->Add(m_step_panel, 1, wxEXPAND, 0);

    content->Add(0, FromDIP(20), 0, wxEXPAND, 0);

    m_text_note = new wxStaticText(this, wxID_ANY, NOTE_STRING, wxDefaultPosition, wxDefaultSize, 0);
    m_text_note->SetFont(Label::Body_14);
    m_text_note->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    m_text_note->Wrap(-1);
    content->Add(m_text_note, 0, wxEXPAND, 0);
    content->Add(0, FromDIP(10), 0, wxEXPAND, 0);

    wxWindow *m_staticline = new wxWindow(this, wxID_ANY, wxDefaultPosition, wxSize(-1, FromDIP(1)));
    m_staticline->SetBackgroundColour(StateColor::semantic(MD3::Role::OutlineVariant));
    content->Add(m_staticline, 0, wxEXPAND, FromDIP(0));
    content->Add(0, FromDIP(10), 0, wxEXPAND, 0);

    m_text_progress = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0);
    m_text_progress->Wrap(-1);
    m_text_progress->SetFont(Label::Body_12);
    m_text_progress->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    content->Add(m_text_progress, 0, wxEXPAND, 0);
    content->Add(0, FromDIP(6), 0, wxEXPAND, 0);

    m_progress = new ProgressBar(this, wxID_ANY, 100, wxDefaultPosition, wxDefaultSize);
    m_progress->SetHeight(FromDIP(8));
    m_progress->SetFont(Label::Head_10);
    content->Add(m_progress, 0, wxEXPAND, 0);
    content->Add(0, FromDIP(8), 0, wxEXPAND, 0);

    m_text_errors = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0);
    m_text_errors->Wrap(-1);
    m_text_errors->SetFont(Label::Body_12);
    m_text_errors->SetForegroundColour(StateColor::semantic(MD3::Role::Error));
    content->Add(m_text_errors, 0, wxEXPAND, 0);

    // Footer: kit outlined pill Cancel button.
    m_btn_cancel = new Button(this, _L("Cancel"));
    m_btn_cancel->SetVariant(Button::Variant::Outlined);
    m_btn_cancel->SetButtonSize(Button::Size::Medium);
    AddFooterButton(m_btn_cancel);

    this->Layout();
    GetSizer()->SetSizeHints(this);
    SetMinSize(wxSize(FromDIP(540), -1));
    SetSize(wxSize(FromDIP(540), FromDIP(400)));
    UpdateShape();
    this->Centre(wxBOTH);

    m_btn_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            this->cancel();
        });

    Bind(wxEVT_CLOSE_WINDOW, &PublishDialog::on_close, this);
    wxGetApp().UpdateDlgDarkUI(this);
}

void PublishDialog::cancel()
{
    m_was_cancelled = true;
    m_btn_cancel->Enable(false);
    m_text_progress->SetLabelText(_L("Publish was cancelled"));
    wxCloseEvent evt;
    this->on_close(evt);
}

void PublishDialog::start_slicing()
{
    SetPublishStep(PublishStep::STEP_SLICING);

    wxCommandEvent *evt = new wxCommandEvent(EVT_PUBLISH);
    evt->SetInt(EVT_PUBLISHING_START);
    wxQueueEvent(m_plater, evt);
}

bool PublishDialog::UpdateStatus(wxString &msg, int percent, bool yield)
{
    if (m_was_cancelled) return false;

    if (percent >= 0)
        m_progress->SetValue(percent);
    m_text_progress->SetLabelText(msg);

    if (yield)
        wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
    return true;
}

void PublishDialog::Pulse(wxString &msg, bool &skip)
{
    if (!msg.IsEmpty())
        m_text_progress->SetLabelText(msg);
    wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
    skip = m_was_cancelled;
}

void PublishDialog::SetPublishStep(PublishStep step, bool yield, int percent)
{
    m_publish_steps->SelectItem((int)step);
    if (step == PublishStep::STEP_SLICING) {
        m_text_progress->SetLabelText(_L("Slicing Plate 1"));
        if (percent > 0)
            m_progress->SetValue(percent);
        else
            m_progress->SetValue(0);
    } else if (step == PublishStep::STEP_PACKING) {
        m_text_progress->SetLabelText(_L("Packing data to 3mf"));
        if (percent > 0)
            m_progress->SetValue(percent);
        else
            m_progress->SetValue(70);
    } else if (step == PublishStep::STEP_UPLOADING) {
        m_text_progress->SetLabelText(_L("Packing data to 3mf"));
        if (percent > 0)
            m_progress->SetValue(percent);
        else
            m_progress->SetValue(85);
    } else if (step == PublishStep::STEP_FILL_INFO) {
        m_text_progress->SetLabelText(_L("Jump to webpage"));
        m_progress->SetValue(100);
    }

    if (yield)
        wxEventLoopBase::GetActive()->YieldFor(wxEVT_CATEGORY_UI | wxEVT_CATEGORY_USER_INPUT);
}

wxBoxSizer *PublishDialog::create_publish_step_sizer()
{
    auto sizer = new wxBoxSizer(wxHORIZONTAL);

    sizer->Add(FromDIP(30), 0, 0, wxEXPAND, 0);

    auto middle_sizer = new wxBoxSizer(wxVERTICAL);

    m_publish_steps = new StepIndicator(m_step_panel, wxID_ANY);
    StateColor bg_color(std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainer), StateColor::Normal));
    m_publish_steps->SetBackgroundColor(bg_color);
    m_publish_steps->SetFont(Label::Body_14);

    for (int i = 0; i < (int) PublishStep::STEP_PUBLISH_COUNT; i++) {
        m_publish_steps->AppendItem(PUBLISH_STEP_STRING[i]);
    }

    middle_sizer->Add(0, FromDIP(30), 0, wxEXPAND, 0);
    middle_sizer->Add(m_publish_steps, 1, wxALL | wxEXPAND, 0);
    middle_sizer->Add(0, FromDIP(30), 0, wxEXPAND, 0);

    sizer->Add(middle_sizer, 1, wxALL | wxEXPAND, 0);

    sizer->Add(FromDIP(30), 0, 0, wxEXPAND, 0);

    return sizer;
}

void PublishDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    Fit();
    UpdateShape(); // keep the borderless rounded silhouette in sync after re-fit
    Refresh();
}

void PublishDialog::on_close(wxCloseEvent &event)
{
    wxCommandEvent *evt = new wxCommandEvent(EVT_PUBLISH);
    evt->SetInt(EVT_PUBLISHING_STOP);
    wxQueueEvent(m_plater, evt);
}

void PublishDialog::OnHeaderClose()
{
    // Route the MD3Dialog circular header [x] through the same path the legacy
    // native title-bar close used (wxEVT_CLOSE_WINDOW -> on_close): post
    // EVT_PUBLISHING_STOP so the plater stops publishing and ends the modal via
    // show_publish_dlg(false). The base OnHeaderClose() would EndModal directly
    // and leave publishing running in the plater (this dialog is a retained
    // member, so its destructor does not run on close either).
    wxCloseEvent e;
    on_close(e);
}

void PublishDialog::reset()
{
    m_btn_cancel->Enable();
    m_was_cancelled = false;
    SetPublishStep(PublishStep::STEP_SLICING);
    m_text_errors->Hide();
}


} // GUI
} // Slic3r
