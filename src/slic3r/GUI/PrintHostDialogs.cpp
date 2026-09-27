#include "PrintHostDialogs.hpp"

#include <algorithm>
#include <iomanip>

#include <wx/frame.h>
#include <wx/progdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/wupdlock.h>
#include <wx/debug.h>
#include <wx/msgdlg.h>

#include <boost/log/trivial.hpp>
#include <boost/filesystem.hpp>
#include <boost/nowide/convert.hpp>

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "MsgDialog.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "libslic3r/AppConfig.hpp"
#include "NotificationManager.hpp"
#include "ExtraRenderers.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/MD3DialogChrome.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/ComboBox.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/TextInput.hpp"

namespace fs = boost::filesystem;

namespace Slic3r {
namespace GUI {

static const char *CONFIG_KEY_PATH  = "printhost_path";
static const char *CONFIG_KEY_GROUP = "printhost_group";

PrintHostSendDialog::PrintHostSendDialog(const fs::path &path, PrintHostPostUploadActions post_actions, const wxArrayString &groups)
    : MsgDialog(static_cast<wxWindow*>(wxGetApp().mainframe), _L("Send to print"), _L("Upload to Printer Host with the following filename:"),0)
    , txt_filename(nullptr)
    , combo_groups(nullptr)
    , post_upload_action(PrintHostPostUploadAction::None)
{
    // Kit fields in place of the stock wx controls this dialog used to drop
    // straight into the MD3 body: TextInput is the r10 SurfaceContainerHighest
    // ValueField and ComboBox the SelectField with the Material Symbols
    // chevron, so the only input of the upload dialog stops being a sunken
    // Win32 edit box under kit chrome. Field height follows the active
    // Appearance > Density row height rather than a pinned literal.
    const int field_h = FromDIP(MD3::Metrics::active().row_height);
    txt_filename = new ::TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxSize(-1, field_h));
    txt_filename->SetMinSize(wxSize(-1, field_h));
    // MSW infers an edit box's accessible name from the static text before it;
    // the entry is now nested inside a custom container, so name it from the
    // dialog headline explicitly rather than lose the name to the reskin.
    txt_filename->SetName(_L("Upload to Printer Host with the following filename:"));
    txt_filename->GetTextCtrl()->SetName(txt_filename->GetName());

#ifdef __APPLE__
    txt_filename->GetTextCtrl()->OSXDisableAllSmartSubstitutions();
#endif
    const AppConfig *app_config = wxGetApp().app_config;

    auto *label_dir_hint = new ::Label(this, ::Label::Body_13, _L("Use forward slashes ( / ) as a directory separator if needed."));
    label_dir_hint->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    label_dir_hint->Wrap(CONTENT_WIDTH * wxGetApp().em_unit());

    content_sizer->Add(txt_filename, 0, wxEXPAND | wxALL, FromDIP(10));
    content_sizer->Add(FromDIP(10), FromDIP(10), 0, 0);
    content_sizer->Add(label_dir_hint, 0, 0, FromDIP(10));
    content_sizer->AddSpacer(VERT_SPACING);
    
    if (! groups.IsEmpty()) {
        // Repetier specific: Show a selection of file groups.
        combo_groups = new ::ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxSize(-1, field_h), 0, nullptr, wxCB_READONLY);
        combo_groups->SetMinSize(wxSize(-1, field_h));
        combo_groups->SetName(_L("Group"));
        for (size_t i = 0; i < groups.GetCount(); ++i)
            combo_groups->Append(groups[i]);

        auto *label_group = new ::Label(this, ::Label::Body_13, _L("Group"));
        label_group->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        content_sizer->Add(label_group);
        // The SelectField draws its own value text and does not measure itself
        // from the item strings, so give it the body width the filename field
        // already has instead of letting it collapse to a default box.
        content_sizer->Add(combo_groups, 0, wxEXPAND | wxBOTTOM, 2*VERT_SPACING);
        wxString recent_group = from_u8(app_config->get("recent", CONFIG_KEY_GROUP));
        if (! recent_group.empty()) {
            // The read-only wxComboBox silently ignored a value that was not in
            // the list; ComboBox::SetValue() would instead display it verbatim
            // and hand it back from group(). Resolve the stored name to an index
            // so a group the host no longer offers leaves the field empty.
            const int recent_idx = combo_groups->FindString(recent_group);
            if (recent_idx != wxNOT_FOUND)
                combo_groups->SetSelection(recent_idx);
        }
    }

    wxString recent_path = from_u8(app_config->get("recent", CONFIG_KEY_PATH));
    if (recent_path.Length() > 0 && recent_path[recent_path.Length() - 1] != '/') {
        recent_path += '/';
    }
    const auto recent_path_len = recent_path.Length();
    recent_path += path.filename().wstring();
    wxString stem(path.stem().wstring());
    const auto stem_len = stem.Length();

    txt_filename->GetTextCtrl()->SetValue(recent_path);
    txt_filename->GetTextCtrl()->SetFocus();

    m_valid_suffix = recent_path.substr(recent_path.find_last_of('.'));
    // .gcode suffix control
    auto validate_path = [this](const wxString &path) -> bool {
        if (! path.Lower().EndsWith(m_valid_suffix.Lower())) {
            MessageDialog msg_wingow(this, wxString::Format(_L("Upload filename doesn't end with \"%s\". Do you wish to continue?"), m_valid_suffix), wxString(SLIC3R_APP_NAME), wxYES | wxNO);
            if (msg_wingow.ShowModal() == wxID_NO)
                return false;
        }
        return true;
    };

    auto* btn_upload = add_button(wxID_YES, false, _L("Upload"));
    btn_upload->Bind(wxEVT_BUTTON, [this, validate_path](wxCommandEvent&) {
        if (validate_path(txt_filename->GetTextCtrl()->GetValue())) {
            post_upload_action = PrintHostPostUploadAction::None;
            EndDialog(wxID_OK);
        }
    });

    if (post_actions.has(PrintHostPostUploadAction::StartPrint)) {
        auto* btn_print = add_button(wxID_YES, false, _L("Print"));
        btn_print->Bind(wxEVT_BUTTON, [this, validate_path](wxCommandEvent&) {
            if (validate_path(txt_filename->GetTextCtrl()->GetValue())) {
                post_upload_action = PrintHostPostUploadAction::StartPrint;
                EndDialog(wxID_OK);
            }
        });
    }

    if (post_actions.has(PrintHostPostUploadAction::StartSimulation)) {
        // Using wxID_MORE as a button identifier to be different from the other buttons, wxID_MORE has no other meaning here.
        auto* btn_simulate = add_button(wxID_MORE, false, _L("Simulate"));
        btn_simulate->Bind(wxEVT_BUTTON, [this, validate_path](wxCommandEvent&) {
            if (validate_path(txt_filename->GetTextCtrl()->GetValue())) {
                post_upload_action = PrintHostPostUploadAction::StartSimulation;
                EndDialog(wxID_OK);
            }        
        });
    }

    add_button(wxID_CANCEL,false, _L("Cancel"));
    finalize();

#ifdef __linux__
    // On Linux with GTK2 when text control lose the focus then selection (colored background) disappears but text color stay white
    // and as a result the text is invisible with light mode
    // see https://github.com/prusa3d/PrusaSlicer/issues/4532
    // Workaround: Unselect text selection explicitly on kill focus
    txt_filename->GetTextCtrl()->Bind(wxEVT_KILL_FOCUS, [this](wxEvent& e) {
        e.Skip();
        txt_filename->GetTextCtrl()->SetInsertionPoint(txt_filename->GetTextCtrl()->GetLastPosition());
    });
#endif /* __linux__ */

    Bind(wxEVT_SHOW, [=](const wxShowEvent &) {
        // Another similar case where the function only works with EVT_SHOW + CallAfter,
        // this time on Mac.
        CallAfter([=]() {
            txt_filename->GetTextCtrl()->SetSelection(recent_path_len, recent_path_len + stem_len);
        });
    });
}

fs::path PrintHostSendDialog::filename() const
{
    return into_path(txt_filename->GetTextCtrl()->GetValue());
}

PrintHostPostUploadAction PrintHostSendDialog::post_action() const
{
    return post_upload_action;
}

std::string PrintHostSendDialog::group() const
{
     if (combo_groups == nullptr) {
         return "";
     } else {
         wxString group = combo_groups->GetValue();
         return into_u8(group);
    }
}

void PrintHostSendDialog::EndModal(int ret)
{
    if (ret == wxID_OK) {
        // Persist path and print settings
        wxString path = txt_filename->GetTextCtrl()->GetValue();
        int last_slash = path.Find('/', true);
		if (last_slash == wxNOT_FOUND)
			path.clear();
		else
            path = path.SubString(0, last_slash);
                
		AppConfig *app_config = wxGetApp().app_config;
		app_config->set("recent", CONFIG_KEY_PATH, into_u8(path));

        if (combo_groups != nullptr) {
            wxString group = combo_groups->GetValue();
            app_config->set("recent", CONFIG_KEY_GROUP, into_u8(group));
        }
    }

    MsgDialog::EndModal(ret);
}



wxDEFINE_EVENT(EVT_PRINTHOST_PROGRESS, PrintHostQueueDialog::Event);
wxDEFINE_EVENT(EVT_PRINTHOST_ERROR,    PrintHostQueueDialog::Event);
wxDEFINE_EVENT(EVT_PRINTHOST_CANCEL,   PrintHostQueueDialog::Event);

PrintHostQueueDialog::Event::Event(wxEventType eventType, int winid, size_t job_id)
    : wxEvent(winid, eventType)
    , job_id(job_id)
{}

PrintHostQueueDialog::Event::Event(wxEventType eventType, int winid, size_t job_id, int progress)
    : wxEvent(winid, eventType)
    , job_id(job_id)
    , progress(progress)
{}

PrintHostQueueDialog::Event::Event(wxEventType eventType, int winid, size_t job_id, wxString error)
    : wxEvent(winid, eventType)
    , job_id(job_id)
    , error(std::move(error))
{}

wxEvent *PrintHostQueueDialog::Event::Clone() const
{
    return new Event(*this);
}

PrintHostQueueDialog::PrintHostQueueDialog(wxWindow *parent)
    // MD3 caption strip instead of the native title bar (see MD3DialogChrome).
    : DPIDialog(parent, wxID_ANY, _L("Print host upload queue"), wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxBORDER_NONE)
    , on_progress_evt(this, EVT_PRINTHOST_PROGRESS, &PrintHostQueueDialog::on_progress, this)
    , on_error_evt(this, EVT_PRINTHOST_ERROR, &PrintHostQueueDialog::on_error, this)
    , on_cancel_evt(this, EVT_PRINTHOST_CANCEL, &PrintHostQueueDialog::on_cancel, this)
{
    const auto em = GetTextExtent("m").x;

    // Own the dialog face before any child is built: the kit Buttons below take
    // their resting fill from the parent background at construction, so leaving
    // this on the system dialog colour would bake a system-grey plate into the
    // Outlined / Text pills.
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));

    auto *topsizer = new wxBoxSizer(wxVERTICAL);
    topsizer->Add(new MD3DialogCaption(this, _L("Print host upload queue")), 0, wxEXPAND);

    std::vector<int> widths;
    widths.reserve(6);
    if (!load_user_data(UDT_COLS, widths)) {
        widths.clear();
        for (size_t i = 0; i < 6; i++)
            widths.push_back(-1);
    }

    job_list = new wxDataViewListCtrl(this, wxID_ANY);

    // MSW DarkMode: workaround for the selected item in the list
    auto append_text_column = [this](const wxString& label, int width, wxAlignment align = wxALIGN_LEFT,
                                     int flags = wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE) {
#ifdef _WIN32
            job_list->AppendColumn(new wxDataViewColumn(label, new TextRenderer(), job_list->GetColumnCount(), width, align, flags));
#else
            job_list->AppendTextColumn(label, wxDATAVIEW_CELL_INERT, width, align, flags);
#endif
    };

    // Note: Keep these in sync with Column
    append_text_column("ID", widths[0]);
    job_list->AppendProgressColumn(_L("Progress"),      wxDATAVIEW_CELL_INERT, widths[1], wxALIGN_LEFT, wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    append_text_column(_L("Status"),widths[2]);
    append_text_column(_L("Host"),  widths[3]);
    append_text_column(_CTX_utf8(L_CONTEXT("Size", "OfFile"), "OfFile"), widths[4]);
    append_text_column(_L("Filename"),      widths[5]);
    append_text_column(_L("Error Message"), -1, wxALIGN_CENTER, wxDATAVIEW_COL_HIDDEN);
 
    // Kit action row instead of three stock Windows push buttons sitting under
    // the MD3 SearchField: Outlined / Text secondaries and a Filled primary at
    // the kit medium height. The wxID_DELETE / wxID_CANCEL ids are kept — the
    // dialog's own EVT_BUTTON(wxID_CANCEL) close routing and on_dpi_changed()
    // both key off them.
    auto *btnsizer = new wxBoxSizer(wxHORIZONTAL);
    btn_cancel = new ::Button(this, _L("Cancel selected"), "", 0, 0, wxID_DELETE);
    btn_cancel->SetButtonSize(::Button::Size::Medium);
    btn_cancel->SetVariant(::Button::Variant::Outlined);
    btn_cancel->Disable();
    btn_error = new ::Button(this, _L("Show error message"));
    btn_error->SetButtonSize(::Button::Size::Medium);
    btn_error->SetVariant(::Button::Variant::Text);
    btn_error->Disable();
    // Note: The label needs to be present, otherwise we get accelerator bugs on Mac
    btn_close = new ::Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);
    btn_close->SetButtonSize(::Button::Size::Medium);
    btn_close->SetVariant(::Button::Variant::Filled);
    btnsizer->Add(btn_cancel, 0, wxRIGHT, SPACING);
    btnsizer->Add(btn_error, 0);
    btnsizer->AddStretchSpacer();
    btnsizer->Add(btn_close);

    // wxDialogBase routes Escape through EmulateButtonClickIfPresent(), which
    // wxDynamicCasts the wxID_CANCEL window to wxButton — a kit Button is not
    // one, so the native Esc-closes behaviour has to be restored explicitly.
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_ESCAPE)
            EndDialog(wxID_CANCEL);
        else
            e.Skip();
    });

    // Find-in-queue bar. Upload job ids are row indices, so rows are never
    // hidden: the search selects the first matching row and reports the match
    // count, and the regex builder rides along via the shared SearchField.
    auto *searchsizer = new wxBoxSizer(wxHORIZONTAL);
    // TRN: Placeholder of the search field in the print host upload queue.
    search_field = new SearchField(this, _L("Search uploads"));
    search_status = new ::Label(this, ::Label::Body_13, wxEmptyString);
    search_status->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
    search_field->SetOnQuery([this](const wxString &) { run_queue_search(); });
    search_field->SetOnRegexToggle([this](bool) { run_queue_search(); });
    searchsizer->Add(search_field, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, SPACING);
    searchsizer->Add(search_status, 0, wxALIGN_CENTER_VERTICAL);
    topsizer->Add(searchsizer, 0, wxEXPAND | wxBOTTOM, SPACING);

    topsizer->Add(job_list, 1, wxEXPAND | wxBOTTOM, SPACING);
    topsizer->Add(btnsizer, 0, wxEXPAND);
    SetSizer(topsizer);

    wxGetApp().UpdateDlgDarkUI(this);
    wxGetApp().UpdateDVCDarkUI(job_list);

    std::vector<int> size;
    SetSize(load_user_data(UDT_SIZE, size) ? wxSize(size[0] * em, size[1] * em) : wxSize(HEIGHT * em, WIDTH * em));

    Bind(wxEVT_SIZE, [this](wxSizeEvent& evt) {
        OnSize(evt); 
        save_user_data(UDT_SIZE | UDT_POSITION | UDT_COLS);
     });
    
    std::vector<int> pos;
    if (load_user_data(UDT_POSITION, pos))
        SetPosition(wxPoint(pos[0], pos[1]));

    Bind(wxEVT_MOVE, [this](wxMoveEvent& evt) {
        save_user_data(UDT_SIZE | UDT_POSITION | UDT_COLS);
    });

    job_list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent&) { on_list_select(); });
    MD3DialogCaption::FinishChrome(this);

    btn_cancel->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        int selected = job_list->GetSelectedRow();
        if (selected == wxNOT_FOUND) { return; }

        const JobState state = get_state(selected);
        if (state < ST_ERROR) {
            GUI::wxGetApp().printhost_job_queue().cancel(selected);
        }
    });

    btn_error->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        int selected = job_list->GetSelectedRow();
        if (selected == wxNOT_FOUND) { return; }
        GUI::show_error(nullptr, job_list->GetTextValue(selected, COL_ERRORMSG));
    });

    // The legacy dark walk above still exists for the native wxDataViewListCtrl,
    // and it re-touches every child's window colours on the way. Re-derive the
    // kit pills afterwards so their surfaces come from the MD3 roles, not from
    // whatever the walk last wrote.
    wxGetApp().UpdateDlgDarkUI(this);
    btn_cancel->Rescale();
    btn_error->Rescale();
    btn_close->Rescale();
}

void PrintHostQueueDialog::append_job(const PrintHostJob &job)
{
    wxCHECK_RET(!job.empty(), "PrintHostQueueDialog: Attempt to append an empty job");

    wxVector<wxVariant> fields;
    fields.push_back(wxVariant(wxString::Format("%d", job_list->GetItemCount() + 1)));
    fields.push_back(wxVariant(0));
    fields.push_back(wxVariant(_L("Enqueued")));
    fields.push_back(wxVariant(job.printhost->get_host()));
    boost::system::error_code ec;
    boost::uintmax_t size_i = boost::filesystem::file_size(job.upload_data.source_path, ec);
    std::stringstream stream;
    if (ec) {
        stream << "unknown";
        size_i = 0;
        BOOST_LOG_TRIVIAL(error) << ec.message();
    } else 
        stream << std::fixed << std::setprecision(2) << ((float)size_i / 1024 / 1024) << "MB";
    fields.push_back(wxVariant(stream.str()));
    fields.push_back(wxVariant(job.upload_data.upload_path.string()));
    fields.push_back(wxVariant(""));
    job_list->AppendItem(fields, static_cast<wxUIntPtr>(ST_NEW));
    // Both strings are UTF-8 encoded.
    upload_names.emplace_back(job.printhost->get_host(), job.upload_data.upload_path.string());

    wxGetApp().notification_manager()->push_upload_job_notification(job_list->GetItemCount(), (float)size_i / 1024 / 1024, job.upload_data.upload_path.string(), job.printhost->get_host());
}

void PrintHostQueueDialog::on_dpi_changed(const wxRect &suggested_rect)
{
    const int& em = em_unit();

    // Not msw_buttons_rescale(): it pins every listed button to 2.5em of height,
    // which would overwrite the kit pill height and radius the variant owns.
    // Button::Rescale() re-derives both from the new DPI instead.
    btn_cancel->Rescale();
    btn_error->Rescale();
    btn_close->Rescale();

    SetMinSize(wxSize(HEIGHT * em, WIDTH * em));

    Fit();
    Refresh();

    save_user_data(UDT_SIZE | UDT_POSITION | UDT_COLS);
}

void PrintHostQueueDialog::on_sys_color_changed()
{
#ifdef _WIN32
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    wxGetApp().UpdateDlgDarkUI(this);
    wxGetApp().UpdateDVCDarkUI(job_list);
    // Same reason as in the ctor: re-resolve the kit pills for the new theme
    // after the legacy walk has been over them.
    btn_cancel->Rescale();
    btn_error->Rescale();
    btn_close->Rescale();
    if (search_status)
        search_status->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
#endif
}

PrintHostQueueDialog::JobState PrintHostQueueDialog::get_state(int idx)
{
    wxCHECK_MSG(idx >= 0 && idx < job_list->GetItemCount(), ST_ERROR, "Out of bounds access to job list");
    return static_cast<JobState>(job_list->GetItemData(job_list->RowToItem(idx)));
}

void PrintHostQueueDialog::set_state(int idx, JobState state)
{
    wxCHECK_RET(idx >= 0 && idx < job_list->GetItemCount(), "Out of bounds access to job list");
    job_list->SetItemData(job_list->RowToItem(idx), static_cast<wxUIntPtr>(state));

    switch (state) {
        case ST_NEW:        job_list->SetValue(_L("Enqueued"), idx, COL_STATUS); break;
        case ST_PROGRESS:   job_list->SetValue(_L("Uploading"), idx, COL_STATUS); break;
        case ST_ERROR:      job_list->SetValue(_L("Error"), idx, COL_STATUS); break;
        case ST_CANCELLING: job_list->SetValue(_L("Cancelling"), idx, COL_STATUS); break;
        case ST_CANCELLED:  job_list->SetValue(_L("Cancelled"), idx, COL_STATUS); break;
        case ST_COMPLETED:  job_list->SetValue(_L("Completed"), idx, COL_STATUS); break;
    }
    // This might be ambigous call, but user data needs to be saved time to time
    save_user_data(UDT_SIZE | UDT_POSITION | UDT_COLS);
}

void PrintHostQueueDialog::run_queue_search()
{
    if (search_field == nullptr || search_status == nullptr)
        return;
    const wxString query = search_field->GetValue();
    if (query.IsEmpty()) {
        search_status->SetLabel(wxEmptyString);
        Layout();
        return;
    }
    const bool regex      = search_field->IsRegexEnabled();
    const bool case_sense = search_field->IsCaseSensitive();
    const bool whole_word = search_field->IsWholeWord();
    const bool multiline  = search_field->IsMultiline();
    int matches = 0;
    int first_match = wxNOT_FOUND;
    const int rows = job_list->GetItemCount();
    SearchField::MatchPass match_pass(query, regex, case_sense, whole_word, multiline);
    for (int row = 0; row < rows; ++row) {
        wxVariant id, status, host, filename;
        job_list->GetValue(id, row, COL_ID);
        job_list->GetValue(status, row, COL_STATUS);
        job_list->GetValue(host, row, COL_HOST);
        job_list->GetValue(filename, row, COL_FILENAME);
        const wxString haystack = id.GetString() + " " + status.GetString() + " " +
                                  host.GetString() + " " + filename.GetString();
        if (match_pass.matches(haystack)) {
            ++matches;
            if (first_match == wxNOT_FOUND)
                first_match = row;
        }
    }
    if (first_match != wxNOT_FOUND) {
        job_list->SelectRow(first_match);
        job_list->EnsureVisible(job_list->RowToItem(first_match));
        on_list_select();
    }
    // TRN: %1$d is how many upload jobs match the search; %2$d is the total.
    search_status->SetLabel(wxString::Format(_L("%d of %d match"), matches, rows));
    Layout();
}

void PrintHostQueueDialog::on_list_select()
{
    int selected = job_list->GetSelectedRow();
    if (selected != wxNOT_FOUND) {
        const JobState state = get_state(selected);
        btn_cancel->Enable(state < ST_ERROR);
        btn_error->Enable(state == ST_ERROR);
        Layout();
    } else {
        btn_cancel->Disable();
    }
}

void PrintHostQueueDialog::on_progress(Event &evt)
{
    wxCHECK_RET(evt.job_id < (size_t)job_list->GetItemCount(), "Out of bounds access to job list");

    if (evt.progress < 100) {
        set_state(evt.job_id, ST_PROGRESS);
        job_list->SetValue(wxVariant(evt.progress), evt.job_id, COL_PROGRESS);
    } else {
        set_state(evt.job_id, ST_COMPLETED);
        job_list->SetValue(wxVariant(100), evt.job_id, COL_PROGRESS);
    }

    on_list_select();

    if (evt.progress > 0)
    {
        wxVariant nm, hst;
        job_list->GetValue(nm, evt.job_id, COL_FILENAME);
        job_list->GetValue(hst, evt.job_id, COL_HOST);
        wxGetApp().notification_manager()->set_upload_job_notification_percentage(evt.job_id + 1, boost::nowide::narrow(nm.GetString()), boost::nowide::narrow(hst.GetString()), evt.progress / 100.f);
    }
}

void PrintHostQueueDialog::on_error(Event &evt)
{
    wxCHECK_RET(evt.job_id < (size_t)job_list->GetItemCount(), "Out of bounds access to job list");

    set_state(evt.job_id, ST_ERROR);

    auto errormsg = from_u8((boost::format("%1%\n%2%") % _utf8(L("Error uploading to print host:")) % std::string(evt.error.ToUTF8())).str());
    job_list->SetValue(wxVariant(0), evt.job_id, COL_PROGRESS);
    job_list->SetValue(wxVariant(errormsg), evt.job_id, COL_ERRORMSG);    // Stashes the error message into a hidden column for later

    on_list_select();

    GUI::show_error(nullptr, errormsg);

    wxVariant nm, hst;
    job_list->GetValue(nm, evt.job_id, COL_FILENAME);
    job_list->GetValue(hst, evt.job_id, COL_HOST);
    wxGetApp().notification_manager()->upload_job_notification_show_error(evt.job_id + 1, boost::nowide::narrow(nm.GetString()), boost::nowide::narrow(hst.GetString()));
}

void PrintHostQueueDialog::on_cancel(Event &evt)
{
    wxCHECK_RET(evt.job_id < (size_t)job_list->GetItemCount(), "Out of bounds access to job list");

    set_state(evt.job_id, ST_CANCELLED);
    job_list->SetValue(wxVariant(0), evt.job_id, COL_PROGRESS);

    on_list_select();

    wxVariant nm, hst;
    job_list->GetValue(nm, evt.job_id, COL_FILENAME);
    job_list->GetValue(hst, evt.job_id, COL_HOST);
    wxGetApp().notification_manager()->upload_job_notification_show_canceled(evt.job_id + 1, boost::nowide::narrow(nm.GetString()), boost::nowide::narrow(hst.GetString()));
}

void PrintHostQueueDialog::get_active_jobs(std::vector<std::pair<std::string, std::string>>& ret)
{
    int ic = job_list->GetItemCount();
    for (int i = 0; i < ic; i++)
    {
        auto item = job_list->RowToItem(i);
        auto data = job_list->GetItemData(item);
        JobState st = static_cast<JobState>(data);
        if(st == JobState::ST_NEW || st == JobState::ST_PROGRESS)
            ret.emplace_back(upload_names[i]);       
    }
    //job_list->data
}
void PrintHostQueueDialog::save_user_data(int udt)
{
    const auto em = GetTextExtent("m").x;
    auto *app_config = wxGetApp().app_config;
    if (udt & UserDataType::UDT_SIZE) {
        
        app_config->set("print_host_queue_dialog_height", std::to_string(this->GetSize().x / em));
        app_config->set("print_host_queue_dialog_width", std::to_string(this->GetSize().y / em));
    }
    if (udt & UserDataType::UDT_POSITION)
    {
        app_config->set("print_host_queue_dialog_x", std::to_string(this->GetPosition().x));
        app_config->set("print_host_queue_dialog_y", std::to_string(this->GetPosition().y));
    }
    if (udt & UserDataType::UDT_COLS)
    {
        for (size_t i = 0; i < job_list->GetColumnCount() - 1; i++)
        {
            app_config->set("print_host_queue_dialog_column_" + std::to_string(i), std::to_string(job_list->GetColumn(i)->GetWidth()));
        }
    }    
}
bool PrintHostQueueDialog::load_user_data(int udt, std::vector<int>& vector)
{
    auto* app_config = wxGetApp().app_config;
    auto hasget = [app_config](const std::string& name, std::vector<int>& vector)->bool {
        if (app_config->has(name)) {
            vector.push_back(std::stoi(app_config->get(name)));
            return true;
        }
        return false;
    };
    if (udt & UserDataType::UDT_SIZE) {
        if (!hasget("print_host_queue_dialog_height",vector))
            return false;
        if (!hasget("print_host_queue_dialog_width", vector))
            return false;
    }
    if (udt & UserDataType::UDT_POSITION)
    {
        if (!hasget("print_host_queue_dialog_x", vector))
            return false;
        if (!hasget("print_host_queue_dialog_y", vector))
            return false;
    }
    if (udt & UserDataType::UDT_COLS)
    {
        for (size_t i = 0; i < 6; i++)
        {
            if (!hasget("print_host_queue_dialog_column_" + std::to_string(i), vector))
                return false;
        }
    }
    return true;
}
}}
