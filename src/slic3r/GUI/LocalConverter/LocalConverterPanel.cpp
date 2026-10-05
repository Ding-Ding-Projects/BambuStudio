#include "LocalConverterPanel.hpp"
#include "libslic3r/LocalConverter/Worker.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/SearchField.hpp"
#include "slic3r/GUI/Widgets/MD3Tokens.hpp"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <nlohmann/json.hpp>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace Slic3r::GUI {
namespace {
namespace LC = LocalConverter;
namespace fs = std::filesystem;
wxString utf(const std::string &s) { return wxString::FromUTF8(s); }
wxString category_label(unsigned i)
{
    switch (i) {
    case 0:return _L("Documents/PDF"); case 1:return _L("Images"); case 2:return _L("Audio"); case 3:return _L("Video");
    case 4:return _L("Archives"); case 5:return _L("Structured Data/Spreadsheets"); case 6:return _L("Code/Text"); default:return _L("Binary Encodings");
    }
}
Button *action(wxWindow *parent, wxSizer *row, const wxString &name, std::function<void()> fn)
{
    auto *button = new Button(parent,name); button->SetName(name); button->SetMinSize(parent->FromDIP(wxSize(-1,44)));
    button->Bind(wxEVT_BUTTON,[fn = std::move(fn)](wxCommandEvent &){ fn(); }); row->Add(button,0,wxALL,4); return button;
}
SearchField::MatchPass matcher(SearchField *field)
{ return SearchField::MatchPass(field->GetValue(),field->IsRegexEnabled(),field->IsCaseSensitive(),field->IsWholeWord(),field->IsMultiline()); }
}
LocalConverterPanel::LocalConverterPanel(wxWindow *parent, LC::PackageProof proof, fs::path queue_directory)
    : wxPanel(parent),m_proof(std::move(proof)),m_adapters(LC::catalog({})),m_timer(this)
{
    SetName(_L("Local file converter"));
    auto *outer = new wxBoxSizer(wxVERTICAL);
    auto *body = new wxScrolledWindow(this,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxVSCROLL);
    body->SetScrollRate(0,FromDIP(16)); auto *layout = new wxBoxSizer(wxVERTICAL);
    auto *title = new wxStaticText(body,wxID_ANY,_L("Local file converter"));
    layout->Add(title,0,wxEXPAND|wxALL,8);
    auto *intro = new wxStaticText(body,wxID_ANY,_L("Choose a verified adapter, review its data changes, choose an output folder, then add files. Sources are preserved. Existing outputs are skipped. Processing stays offline."));
    intro->Wrap(FromDIP(700)); layout->Add(intro,0,wxEXPAND|wxALL,8);
    m_categories = new wxNotebook(body,wxID_ANY);
    for (unsigned i = 0; i != 8; ++i) {
        auto *panel = new wxPanel(m_categories); auto *sizer = new wxBoxSizer(wxVERTICAL);
        m_search[i] = new SearchField(panel,_L("Search adapters in this category")); m_search[i]->SetName(category_label(i)+" "+_L("adapter search"));
        m_search[i]->SetOnQuery([this](const wxString &){ refresh_catalog(); });
        m_search[i]->SetOnRegexToggle([this](bool){ refresh_catalog(); });
        sizer->Add(m_search[i],0,wxEXPAND|wxALL,4);
        auto *list = new wxListCtrl(panel,wxID_ANY,wxDefaultPosition,FromDIP(wxSize(-1,175)),wxLC_REPORT|wxLC_SINGLE_SEL);
        list->SetName(category_label(i)+" "+_L("available and unavailable adapters"));
        list->InsertColumn(0,_L("Adapter"),wxLIST_FORMAT_LEFT,FromDIP(220));
        list->InsertColumn(1,_L("Availability"),wxLIST_FORMAT_LEFT,FromDIP(420));
        list->Bind(wxEVT_LIST_ITEM_SELECTED,[this,i](wxListEvent &event){
            const auto row = static_cast<std::size_t>(event.GetIndex()); if (row >= m_visible[i].size()) return;
            m_selected = m_visible[i][row]; const auto &a = m_adapters[m_selected];
            m_details->SetLabel(utf(a.disclosure + (a.enabled ? "\nValidation: " + a.validator : "\n" + a.reason)));
            m_details->Wrap(FromDIP(700)); Layout();
        });
        m_catalogs[i] = list; sizer->Add(list,1,wxEXPAND|wxALL,4); panel->SetSizer(sizer); m_categories->AddPage(panel,category_label(i));
    }
    layout->Add(m_categories,0,wxEXPAND|wxALL,8);
    m_details = new wxStaticText(body,wxID_ANY,_L("No adapter selected. Unavailable formats remain listed with their exact reason."));
    m_details->Wrap(FromDIP(700)); layout->Add(m_details,0,wxEXPAND|wxALL,8);
    auto *pdf_search=new SearchField(body,_L("Search PDF operation settings"));
    layout->Add(pdf_search,0,wxEXPAND|wxALL,8);
    auto *pdf_settings=new wxBoxSizer(wxVERTICAL);
    std::vector<std::pair<wxSizer*,wxString>> pdf_rows;
    auto *pages_row=new wxBoxSizer(wxHORIZONTAL);
    const auto pages_label=_L("Page order (one-based, comma separated). Default: 1. Blank rotates all pages.");
    auto *pages_caption=new wxStaticText(body,wxID_ANY,pages_label);pages_caption->Wrap(FromDIP(320));pages_row->Add(pages_caption,1,wxALL|wxALIGN_CENTER_VERTICAL,4);
    m_pdf_pages=new wxTextCtrl(body,wxID_ANY,"1");m_pdf_pages->SetName(_L("PDF page order"));m_pdf_pages->SetMaxLength(5000);pages_row->Add(m_pdf_pages,1,wxALL|wxALIGN_CENTER_VERTICAL,4);
    pdf_settings->Add(pages_row,0,wxEXPAND);pdf_rows.emplace_back(pages_row,pages_label);
    auto *rotation_row=new wxBoxSizer(wxHORIZONTAL);
    auto *rotation_label=new wxStaticText(body,wxID_ANY,_L("Absolute page rotation. Default: 90 degrees."));rotation_row->Add(rotation_label,1,wxALL|wxALIGN_CENTER_VERTICAL,4);
    for(int degrees:{0,90,180,270})action(body,rotation_row,wxString::Format(_L("%d degrees"),degrees),[this,rotation_label,degrees]{m_pdf_rotation=degrees;rotation_label->SetLabel(wxString::Format(_L("Absolute page rotation: %d degrees."),degrees));});
    pdf_settings->Add(rotation_row,0,wxEXPAND);pdf_rows.emplace_back(rotation_row,_L("PDF rotation degrees"));
    auto *title_row=new wxBoxSizer(wxHORIZONTAL);
    const auto title_label=_L("PDF title for Metadata. Default: empty, which clears the title. Other supported metadata is preserved.");
    auto *title_caption=new wxStaticText(body,wxID_ANY,title_label);title_caption->Wrap(FromDIP(320));title_row->Add(title_caption,1,wxALL|wxALIGN_CENTER_VERTICAL,4);
    m_pdf_title=new wxTextCtrl(body,wxID_ANY);m_pdf_title->SetName(_L("PDF title metadata"));m_pdf_title->SetMaxLength(1000);title_row->Add(m_pdf_title,1,wxALL|wxALIGN_CENTER_VERTICAL,4);
    pdf_settings->Add(title_row,0,wxEXPAND);pdf_rows.emplace_back(title_row,title_label);
    layout->Add(pdf_settings,0,wxEXPAND|wxALL,4);
    auto filter_pdf=[this,pdf_search,pdf_settings,pdf_rows]{auto pass=matcher(pdf_search);for(const auto &row:pdf_rows)pdf_settings->Show(row.first,pass.matches(row.second),true);Layout();};
    pdf_search->SetOnQuery([filter_pdf](const wxString&){filter_pdf();});pdf_search->SetOnRegexToggle([filter_pdf](bool){filter_pdf();});
    auto *destination_label = new wxStaticText(body,wxID_ANY,_L("Output folder (sources keep their names with the chosen extension)"));
    layout->Add(destination_label,0,wxLEFT|wxRIGHT|wxTOP,8);
    auto *destination_row = new wxBoxSizer(wxHORIZONTAL);
    m_destination = new wxTextCtrl(body,wxID_ANY); m_destination->SetName(_L("Conversion output folder")); destination_row->Add(m_destination,1,wxALIGN_CENTER_VERTICAL|wxALL,4);
    action(body,destination_row,_L("Browse output folder"),[this]{ wxDirDialog dialog(this,_L("Choose output folder")); if (dialog.ShowModal() == wxID_OK) m_destination->SetValue(dialog.GetPath()); });
    layout->Add(destination_row,0,wxEXPAND|wxALL,4);
    auto *admission = new wxBoxSizer(wxHORIZONTAL);
    action(body,admission,_L("Add source file"),[this]{ choose_source(false); });
    action(body,admission,_L("Add source folder"),[this]{ choose_source(true); });
    layout->Add(admission,0,wxEXPAND|wxLEFT|wxRIGHT,4);
    auto *controls = new wxBoxSizer(wxHORIZONTAL);
    action(body,controls,_L("Resume queue"),[this]{ run(); });
    action(body,controls,_L("Pause after current file"),[this]{ if (m_queue) { m_queue->pause(true); update_status(_L("Paused. The current file may finish; pending files stay saved.")); } });
    action(body,controls,_L("Cancel active and pending"),[this]{ m_cancel = true; if (m_queue) m_queue->cancel_pending(); update_status(_L("Cancellation requested. Completed outputs are preserved.")); });
    layout->Add(controls,0,wxEXPAND|wxLEFT|wxRIGHT,4);
    m_queue_search = new SearchField(body,_L("Search this queue page")); m_queue_search->SetName(_L("Converter queue page search"));
    m_queue_search->SetOnQuery([this](const wxString &){ refresh_queue(); }); m_queue_search->SetOnRegexToggle([this](bool){ refresh_queue(); });
    layout->Add(m_queue_search,0,wxEXPAND|wxALL,8);
    m_jobs = new wxListCtrl(body,wxID_ANY,wxDefaultPosition,FromDIP(wxSize(-1,210)),wxLC_REPORT);
    m_jobs->SetName(_L("Conversion queue and result history, current page"));
    m_jobs->InsertColumn(0,_L("Number"),wxLIST_FORMAT_LEFT,FromDIP(70)); m_jobs->InsertColumn(1,_L("Source name"),wxLIST_FORMAT_LEFT,FromDIP(190));
    m_jobs->InsertColumn(2,_L("State"),wxLIST_FORMAT_LEFT,FromDIP(150)); m_jobs->InsertColumn(3,_L("Result"),wxLIST_FORMAT_LEFT,FromDIP(280));
    layout->Add(m_jobs,0,wxEXPAND|wxALL,8);
    auto *bulk = new wxBoxSizer(wxHORIZONTAL);
    action(body,bulk,_L("Select this page"),[this]{ for (long i=0;i<m_jobs->GetItemCount();++i) m_jobs->SetItemState(i,wxLIST_STATE_SELECTED,wxLIST_STATE_SELECTED); });
    action(body,bulk,_L("Invert selection"),[this]{ for (long i=0;i<m_jobs->GetItemCount();++i) m_jobs->SetItemState(i,m_jobs->GetItemState(i,wxLIST_STATE_SELECTED) ? 0 : wxLIST_STATE_SELECTED,wxLIST_STATE_SELECTED); });
    action(body,bulk,_L("Retry selected"),[this]{
        if (!m_queue || m_running) { update_status(_L("Pause and wait for the active file before retrying.")); return; }
        std::size_t accepted=0,rejected=0;
        for (long row=m_jobs->GetNextItem(-1,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED);row!=-1;row=m_jobs->GetNextItem(row,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED)) {
            try { m_queue->retry(m_page[m_visible_jobs[row]].id); ++accepted; } catch (...) { ++rejected; }
        }
        update_status(wxString::Format(_L("Retried: %llu. Not eligible or destination already exists: %llu."),static_cast<unsigned long long>(accepted),static_cast<unsigned long long>(rejected))); refresh_queue();
    });
    action(body,bulk,_L("Export visible results"),[this]{ export_page(); });
    layout->Add(bulk,0,wxEXPAND|wxLEFT|wxRIGHT,4);
    auto *paging = new wxBoxSizer(wxHORIZONTAL);
    action(body,paging,_L("Previous page"),[this]{ m_after = m_after >= LC::Limits::page_size ? m_after-LC::Limits::page_size : 0; refresh_queue(); });
    action(body,paging,_L("Next page"),[this]{ if (m_queue && m_after + LC::Limits::page_size < m_queue->count()) m_after += LC::Limits::page_size; refresh_queue(); });
    m_page_label = new wxStaticText(body,wxID_ANY); paging->Add(m_page_label,1,wxALIGN_CENTER_VERTICAL|wxALL,4); layout->Add(paging,0,wxEXPAND|wxALL,4);
    m_status = new wxStaticText(body,wxID_ANY,_L("Empty queue. No source files have been selected.")); m_status->SetName(_L("Converter status")); m_status->Wrap(FromDIP(700));
    layout->Add(m_status,0,wxEXPAND|wxALL,8); body->SetSizer(layout); outer->Add(body,1,wxEXPAND); SetSizer(outer);
    try { m_queue = std::make_unique<LC::Queue>(std::move(queue_directory)); }
    catch (...) { update_status(_L("The private queue cannot be opened or another converter owns it. Close the other converter and reopen this destination.")); }
    refresh_catalog(); refresh_queue();
    Bind(wxEVT_TIMER,[this](wxTimerEvent &){
        if (!m_running && m_work.joinable()) {
            m_work.join();
            if(m_catalog_pending.exchange(false)){
                {std::lock_guard<std::mutex> lock(m_catalog_mutex);m_adapters=std::move(m_next_adapters);}
                refresh_catalog();update_status(_L("Bundled worker checks finished. Each unavailable adapter shows its exact reason."));return;
            }
            wxString preview;
            if(m_preview_kind.load()>=0) preview=wxString::Format(_L(" Last source preview: %s, %llu bytes."),utf(LC::kind_name(static_cast<LC::Kind>(m_preview_kind.load()))),static_cast<unsigned long long>(m_preview_bytes.load()));
            update_status(wxString::Format(_L("Operation stopped. Added: %llu. Not admitted: %llu. Review each saved result below."),static_cast<unsigned long long>(m_admitted.load()),static_cast<unsigned long long>(m_rejected.load()))+preview);
            refresh_queue();
        }
        if (m_running) refresh_queue();
    }); m_timer.Start(500);
    m_running=true;update_status(_L("Checking the bundled worker and offline sandbox. No converter is enabled until its runtime check completes."));
    m_work=std::thread([this]{auto ready=LC::catalog(m_proof);{std::lock_guard<std::mutex> lock(m_catalog_mutex);m_next_adapters=std::move(ready);}m_catalog_pending=true;m_running=false;});
}
LocalConverterPanel::~LocalConverterPanel() { m_timer.Stop(); stop(); }
void LocalConverterPanel::update_status(const wxString &message) { m_status->SetLabel(message); m_status->Wrap(FromDIP(700)); Layout(); }
void LocalConverterPanel::refresh_catalog()
{
    for (unsigned c=0;c!=8;++c) {
        m_catalogs[c]->DeleteAllItems(); m_visible[c].clear(); auto pass = matcher(m_search[c]);
        for (std::size_t i=0;i<m_adapters.size();++i) {
            const auto &a=m_adapters[i]; if (static_cast<unsigned>(a.category)!=c || !pass.matches(utf(a.name+" "+a.reason+" "+a.disclosure))) continue;
            const long row=m_catalogs[c]->InsertItem(m_catalogs[c]->GetItemCount(),utf(a.name));
            m_catalogs[c]->SetItem(row,1,a.enabled ? _L("Bundled worker verified") : utf(a.reason)); m_visible[c].push_back(i);
        }
        if (m_visible[c].empty()) m_catalogs[c]->InsertItem(0,_L("No adapters match this search."));
    }
}
void LocalConverterPanel::refresh_queue()
{
    if (!m_queue) return;
    try {
        // Preserve selected stable IDs across the progress refresh.
        std::vector<std::uint64_t> selected;
        for(long row=m_jobs->GetNextItem(-1,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED);row!=-1;row=m_jobs->GetNextItem(row,wxLIST_NEXT_ALL,wxLIST_STATE_SELECTED))
            if(static_cast<std::size_t>(row)<m_visible_jobs.size()) selected.push_back(m_page[m_visible_jobs[row]].id);
        m_page=m_queue->page(m_after); m_jobs->DeleteAllItems(); m_visible_jobs.clear(); auto pass=matcher(m_queue_search);
        for(std::size_t i=0;i<m_page.size();++i) {
            const auto &j=m_page[i]; const auto name=j.source.filename().u8string();
            if(!pass.matches(utf(name+" "+LC::state_name(j.state)+" "+j.code))) continue;
            const auto row=m_jobs->InsertItem(m_jobs->GetItemCount(),wxString::Format("%llu",static_cast<unsigned long long>(j.id)));
            m_jobs->SetItem(row,1,utf(name)); m_jobs->SetItem(row,2,utf(LC::state_name(j.state))); m_jobs->SetItem(row,3,utf(j.code)); m_visible_jobs.push_back(i);
            if(std::find(selected.begin(),selected.end(),j.id)!=selected.end()) m_jobs->SetItemState(row,wxLIST_STATE_SELECTED,wxLIST_STATE_SELECTED);
        }
        m_page_label->SetLabel(wxString::Format(_L("%llu saved files. Showing %llu matches from at most 100 records."),static_cast<unsigned long long>(m_queue->count()),static_cast<unsigned long long>(m_visible_jobs.size())));
    } catch (...) { update_status(_L("A saved queue record could not be read. Records and outputs have been retained for recovery.")); }
}
void LocalConverterPanel::choose_source(bool folder)
{
    if (!m_queue || m_running) { update_status(_L("Wait for the current operation before adding sources.")); return; }
    if (m_selected>=m_adapters.size() || !m_adapters[m_selected].enabled) { update_status(_L("Select an enabled adapter first. Unavailable rows explain their missing packaged adapter.")); return; }
    const bool merge=m_adapters[m_selected].id=="pdf.merge";
    if(folder && merge){update_status(_L("PDF merge needs an explicit ordered selection. Use Add source file and select at least two PDFs."));return;}
    if(folder) { wxDirDialog dialog(this,_L("Choose source folder, discovered one file at a time")); if(dialog.ShowModal()==wxID_OK) admit(fs::path(dialog.GetPath().ToStdWstring()),true); }
    else { wxFileDialog dialog(this,merge?_L("Choose PDFs to merge, in picker order"):_L("Choose source file"),wxEmptyString,wxEmptyString,_L("All files (*.*)|*.*"),wxFD_OPEN|wxFD_FILE_MUST_EXIST|(merge?wxFD_MULTIPLE:0));
        if(dialog.ShowModal()==wxID_OK){
            if(merge){wxArrayString paths;dialog.GetPaths(paths);if(paths.size()<2||paths.size()>1000){update_status(_L("Select between 2 and 1000 PDFs within the aggregate 16 MiB limit."));return;}
                std::vector<fs::path> additional;for(std::size_t i=1;i<paths.size();++i)additional.emplace_back(paths[i].ToStdWstring());admit(fs::path(paths[0].ToStdWstring()),false,std::move(additional));}
            else admit(fs::path(dialog.GetPath().ToStdWstring()),false);
        }
    }
}
void LocalConverterPanel::admit(fs::path source,bool folder,std::vector<fs::path> additional)
{
    fs::path destination(m_destination->GetValue().ToStdWstring());
    std::error_code ec; if(!fs::is_directory(destination,ec)) { update_status(_L("Choose an existing output folder.")); return; }
    const auto adapter=m_adapters[m_selected]; std::string options;
    if(adapter.id.rfind("pdf.",0)==0){
        nlohmann::json data=nlohmann::json::object();
        if(adapter.id=="pdf.extract"||adapter.id=="pdf.reorder"||adapter.id=="pdf.rotate"){
            data["pages"]=nlohmann::json::array();std::stringstream list(m_pdf_pages->GetValue().ToStdString());std::string item;
            while(std::getline(list,item,',')){std::stringstream field(item);unsigned n=0;field>>n;field>>std::ws;if(!field.eof()||!n||n>1000){update_status(_L("Enter page numbers from 1 to 1000, separated by commas."));return;}data["pages"].push_back(n);}
        }
        if(adapter.id=="pdf.rotate")data["rotation"]=m_pdf_rotation;
        if(adapter.id=="pdf.metadata")data["metadata"]={{"Title",m_pdf_title->GetValue().ToUTF8().data()}};
        options=data.dump();
    }
    if(m_work.joinable()) m_work.join(); m_cancel=false; m_running=true; m_admitted=0; m_rejected=0;
    update_status(_L("Discovering sources with bounded memory. Cancel stops discovery; admitted records stay saved."));
    m_work=std::thread([this,source=std::move(source),destination=std::move(destination),adapter,folder,options=std::move(options),additional=std::move(additional)]{
        auto add=[&](const fs::path &path){
            if(m_cancel.load()) return;
            try {
                if(!fs::is_regular_file(fs::symlink_status(path))) { ++m_rejected; return; }
                const auto size=fs::file_size(path); if(size>LC::Limits::input_bytes) {++m_rejected;return;}
                std::ifstream in(path,std::ios::binary); LC::Bytes data(static_cast<std::size_t>(size));
                in.read(reinterpret_cast<char *>(data.data()),static_cast<std::streamsize>(size));
                if(!in || in.peek()!=std::char_traits<char>::eof()) {++m_rejected;return;}
                const auto kind=LC::detect(data); m_preview_kind=static_cast<int>(kind);m_preview_bytes=size;
                if(std::find(adapter.sources.begin(),adapter.sources.end(),kind)==adapter.sources.end()) {++m_rejected;return;}
                fs::path target=destination/path.filename(); target.replace_extension("."+adapter.extension);
                m_queue->enqueue(path,target,adapter.id,options,additional); ++m_admitted;
            } catch (...) { ++m_rejected; }
        };
        try { if(folder) { for(const auto &entry:fs::recursive_directory_iterator(source,fs::directory_options::none)) { if(m_cancel.load()) break; if(entry.is_regular_file()) add(entry.path()); } } else add(source); }
        catch (...) { ++m_rejected; }
        m_running=false;
    });
}
void LocalConverterPanel::run()
{
    if(!m_queue || m_running) return;
    std::string reason; if(!LC::verify_package(m_proof,reason)) { update_status(utf(reason)); return; }
    if(m_work.joinable()) m_work.join(); m_cancel=false; m_running=true; m_queue->pause(false);
    update_status(_L("Converting offline in the isolated worker. One file runs at a time; every result is saved."));
    m_work=std::thread([this]{
        try { while(m_queue->step([this](const std::string &id,const LC::Bytes &input,const std::atomic<bool> &cancel){return LC::isolated_transform(m_proof,id,input,cancel);},m_cancel)) {} }
        catch (...) { m_cancel=true; }
        m_running=false;
    });
}
void LocalConverterPanel::stop() { m_cancel=true; if(m_work.joinable()) m_work.join(); }
void LocalConverterPanel::export_page()
{
    wxFileDialog dialog(this,_L("Export visible queue results"),wxEmptyString,"conversion-results.csv",_L("CSV files (*.csv)|*.csv"),wxFD_SAVE);
    if(dialog.ShowModal()!=wxID_OK) return;
    std::string out="id,source_name,state,result\r\n";
    auto quote=[](std::string value){ std::string q="\""; for(char c:value){q+=c;if(c=='"')q+='"';} return q+'"'; };
    for(auto i:m_visible_jobs) { const auto &j=m_page[i]; out+=std::to_string(j.id)+","+quote(j.source.filename().u8string())+","+quote(LC::state_name(j.state))+","+quote(j.code)+"\r\n"; }
    std::string code; const bool ok=LC::atomic_create(fs::path(dialog.GetPath().ToStdWstring()),LC::Bytes(out.begin(),out.end()),code);
    update_status(ok ? _L("Visible result rows exported as UTF-8 CSV. Source paths were excluded.") : _L("Export was not written. Choose a new destination in a writable folder."));
}
} // namespace Slic3r::GUI
