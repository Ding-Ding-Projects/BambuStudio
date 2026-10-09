#include "LocalConverterPanel.hpp"
#include "LocalConverterCopy.hpp"
#include "libslic3r/LocalConverter/Worker.hpp"
#include "slic3r/GUI/GUI_App.hpp"
#include "slic3r/GUI/I18N.hpp"
#include "slic3r/GUI/LanguageMode.hpp"
#include "slic3r/GUI/wxExtensions.hpp"
#include "slic3r/GUI/Widgets/Button.hpp"
#include "slic3r/GUI/Widgets/Label.hpp"
#include "slic3r/GUI/Widgets/LabeledCheckBox.hpp"
#include "slic3r/GUI/Widgets/MD3DataView.hpp"
#include "slic3r/GUI/Widgets/MD3ScrolledWindow.hpp"
#include "slic3r/GUI/Widgets/MD3Tokens.hpp"
#include "slic3r/GUI/Widgets/SearchField.hpp"
#include "slic3r/GUI/Widgets/StateColor.hpp"
#include "slic3r/GUI/Widgets/TabStrip.hpp"
#include "slic3r/GUI/Widgets/TextInput.hpp"
#include <fstream>
#include <algorithm>
#include <functional>
#include <initializer_list>
#include <sstream>
#include <nlohmann/json.hpp>
#include <wx/dataview.h>
#include <wx/dirdlg.h>
#include <wx/filedlg.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>

namespace Slic3r::GUI {
namespace {
namespace LC = LocalConverter;
namespace Copy = LocalConverterCopy;
namespace fs = std::filesystem;
using Text = I18N::LocalizedText;
// Stable tab ids, in the order of LocalConverter::Category. They key the
// strip's persisted layout and never change with the language.
constexpr std::array<const char *,8> kCategoryIds{"documents","images","audio","video","archives","structured","text","binary"};
constexpr std::array<int,4> kRotations{0,90,180,270};
wxString utf(const std::string &s) { return wxString::FromUTF8(s); }
wxString category_label(unsigned i)
{
    switch (i) {
    case 0:return _L("Documents/PDF"); case 1:return _L("Images"); case 2:return _L("Audio"); case 3:return _L("Video");
    case 4:return _L("Archives"); case 5:return _L("Structured Data/Spreadsheets"); case 6:return _L("Code/Text"); default:return _L("Binary Encodings");
    }
}

// Factual catalogue text (registry names, disclosures, reasons, states, result
// messages and safety statements) in the active language mode. It never takes
// a funny-level voice. In bilingual mode it carries the Cantonese as secondary.
Text fact(const char *source) { return I18N::language_mode_service().factual(wxString::FromUTF8(source)); }
Text fact(const std::string &source) { return fact(source.c_str()); }
// Text that is the same in every language: a code, a number, a file name.
Text plain(const wxString &value) { return Text{value, wxString()}; }
Text number(std::uint64_t value) { return plain(wxString::Format("%llu", static_cast<unsigned long long>(value))); }
// Non-factual copy: English and Cantonese each take the variant for their own
// funny level, and both state the same facts.
Text voice(Copy::Line line)
{
    const auto &service = I18N::language_mode_service();
    const Text english = service.factual(wxString::FromUTF8(Copy::source(line, service.funny_level(I18N::FunnyLanguage::English))));
    const Text cantonese = service.factual(wxString::FromUTF8(Copy::source(line, service.funny_level(I18N::FunnyLanguage::Cantonese))));
    switch (service.profile().kind) {
    case I18N::LanguageModeKind::CantoneseHongKong: return Text{cantonese.primary, wxString()};
    case I18N::LanguageModeKind::BilingualEnglishCantoneseHongKong: return Text{english.primary, cantonese.secondary};
    default: return Text{english.primary, wxString()};
    }
}
const wxString &language_text(const Text &text, bool secondary) { return secondary && !text.secondary.empty() ? text.secondary : text.primary; }
// Formats each language with the same arguments (counts and degrees).
template <class... Args> Text format(const Text &text, Args... args)
{
    Text out{wxString::Format(text.primary, args...), wxString()};
    if (!text.secondary.empty()) out.secondary = wxString::Format(text.secondary, args...);
    return out;
}
// Fills %s placeholders with text that has its own language variants, so the
// Cantonese line holds the Cantonese argument.
template <class... Parts> Text fill(const Text &text, const Parts &...parts)
{
    const bool bilingual = !text.secondary.empty() || (... || !parts.secondary.empty());
    Text out{wxString::Format(text.primary, parts.primary...), wxString()};
    if (bilingual) out.secondary = wxString::Format(language_text(text, true), language_text(parts, true)...);
    return out;
}
// Joins parts per language: in bilingual mode the stacked label reads the
// English block, then the Cantonese block.
Text join(std::initializer_list<Text> parts, const wxString &separator)
{
    Text out; bool bilingual = false;
    for (const Text &part : parts) {
        if (part.primary.empty()) continue;
        if (!out.primary.empty()) { out.primary += separator; out.secondary += separator; }
        out.primary += part.primary; out.secondary += language_text(part, true);
        bilingual = bilingual || !part.secondary.empty();
    }
    if (!bilingual) out.secondary.clear();
    return out;
}
// One table cell: "English · 廣東話" in bilingual mode, otherwise the language shown.
wxString cell(const Text &text) { return text.secondary.empty() ? text.primary : text.primary + wxString::FromUTF8(" \xC2\xB7 ") + text.secondary; }
wxString searchable(const std::string &english, const Text &shown) { return utf(english) + " " + shown.primary + " " + shown.secondary; }
// A stable result code with its translated sentence; the code stays visible so
// the exact boundary is never lost in translation.
Text result_text(const std::string &code)
{
    const char *message = LC::result_message(code);
    return fill(fact(L("%s (code %s)")), message != nullptr ? fact(message) : fact(L("No description is available for this result code.")), plain(utf(code)));
}
Text availability(const LC::Adapter &a)
{
    if (a.enabled) return fact(L("Bundled worker verified"));
    if (a.detail.empty()) return fact(a.reason);
    return fill(fact(L("%s (code %s)")), fact(a.reason), plain(utf(a.detail)));
}
void show(Label *label, const Text &text)
{
    I18N::LocalizedTextRenderOptions options;
    options.presentation = I18N::LocalizedTextPresentation::Stacked;
    I18N::apply_localized_text(*label, text.finalize_without_arguments(), options);
}
Button *action(wxWindow *parent, wxSizer *row, const wxString &name, std::function<void()> fn, Button::Variant variant = Button::Variant::Outlined)
{
    auto *button = new Button(parent,name); button->SetName(name); button->SetVariant(variant); button->SetButtonSize(Button::Size::Large);
    button->Bind(wxEVT_BUTTON,[fn = std::move(fn)](wxCommandEvent &){ fn(); }); row->Add(button,0,wxALL,parent->FromDIP(4)); return button;
}
Label *text(wxWindow *parent, const wxString &value)
{
    return new Label(parent,value,LB_AUTO_WRAP);
}
MD3DataViewListCtrl *table(wxWindow *parent, long style, int height)
{
    auto *list = new MD3DataViewListCtrl(parent,wxID_ANY,wxDefaultPosition,parent->FromDIP(wxSize(-1,height)),style);
    wxGetApp().UpdateDVCDarkUI(list); md3_style_data_view(list); list->SetMinSize(parent->FromDIP(wxSize(-1,height))); return list;
}
void add_row(MD3DataViewListCtrl *list, std::initializer_list<wxString> cells)
{
    wxVector<wxVariant> row; for (const auto &value : cells) row.push_back(wxVariant(value)); list->AppendItem(row);
}
SearchField::MatchPass matcher(SearchField *field)
{ return SearchField::MatchPass(field->GetValue(),field->IsRegexEnabled(),field->IsCaseSensitive(),field->IsWholeWord(),field->IsMultiline()); }
wxString field_value(TextInput *field) { return field->GetTextCtrl()->GetValue(); }
}
LocalConverterPanel::LocalConverterPanel(wxWindow *parent, LC::PackageProof proof, fs::path queue_directory)
    : wxPanel(parent),m_proof(std::move(proof)),m_adapters(LC::catalog({})),m_timer(this)
{
    SetName(_L("Local file converter"));
    SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    const int gap = FromDIP(8);
    auto *outer = new wxBoxSizer(wxVERTICAL);
    auto *body = new MD3ScrolledWindow(this,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxVSCROLL);
    body->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    body->SetScrollRate(0,FromDIP(16)); auto *layout = new wxBoxSizer(wxVERTICAL);
    auto *title = new Label(body,::Label::Head_20,_L("Local file converter")); title->SetName(_L("Local file converter"));
    layout->Add(title,0,wxEXPAND|wxALL,gap);
    auto *intro = text(body,wxEmptyString); show(intro,voice(Copy::Line::Intro));
    layout->Add(intro,0,wxEXPAND|wxALL,gap);

    // Categories: a persisted kit TabStrip over one page per category.
    TabStrip::Options options;
    options.surface_key = "local_converter_categories";
    options.surface_name = _L("Local file converter");
    options.strip_name = _L("Converter categories");
    options.default_edge = MD3::Tabs::DockEdge::Top;
    options.close_mode = TabStrip::CloseMode::Hide;
    options.allow_close = false;
    m_categories = new TabStrip(body, options); m_categories->SetName(_L("Converter categories"));
    m_category_pages = new wxSimplebook(body,wxID_ANY);
    m_category_pages->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
    for (unsigned i = 0; i != kCategoryIds.size(); ++i) {
        auto *page = new wxPanel(m_category_pages); page->SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
        auto *sizer = new wxBoxSizer(wxVERTICAL);
        m_search[i] = new SearchField(page,_L("Search adapters in this category")); m_search[i]->SetName(category_label(i)+" "+_L("adapter search"));
        m_search[i]->SetOnQuery([this](const wxString &){ refresh_catalog(); });
        m_search[i]->SetOnRegexToggle([this](bool){ refresh_catalog(); });
        sizer->Add(m_search[i],0,wxEXPAND|wxALL,FromDIP(4));
        auto *list = table(page,wxDV_SINGLE|wxDV_ROW_LINES,175);
        list->SetName(category_label(i)+" "+_L("available and unavailable adapters"));
        list->AppendTextColumn(_L("Adapter"),wxDATAVIEW_CELL_INERT,FromDIP(220),wxALIGN_LEFT,wxDATAVIEW_COL_RESIZABLE);
        list->AppendTextColumn(_L("Availability"),wxDATAVIEW_CELL_INERT,FromDIP(420),wxALIGN_LEFT,wxDATAVIEW_COL_RESIZABLE);
        list->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,[this,i](wxDataViewEvent &){ select_adapter(i); });
        m_catalogs[i] = list; sizer->Add(list,1,wxEXPAND|wxALL,FromDIP(4));
        m_empty[i] = text(page,wxEmptyString); show(m_empty[i],voice(Copy::Line::NoMatches)); m_empty[i]->Hide(); sizer->Add(m_empty[i],0,wxEXPAND|wxALL,FromDIP(4));
        page->SetSizer(sizer); m_category_pages->AddPage(page,category_label(i));
        m_categories->AddTab(kCategoryIds[i], category_label(i));
    }
    m_categories->Bind(EVT_TABSTRIP_ACTIVATE,[this](wxCommandEvent &event){ show_category(event.GetString().ToStdString()); });
    m_categories->Bind(EVT_TABSTRIP_DOCK_CHANGED,[this](wxCommandEvent &){ place_categories(); });
    m_category_layout = new wxBoxSizer(wxVERTICAL);
    layout->Add(m_category_layout,0,wxEXPAND|wxALL,gap);
    m_categories->LoadLayout();
    place_categories();
    show_category(m_categories->ActiveId().empty() ? std::string(kCategoryIds[0]) : m_categories->ActiveId());

    m_details = text(body,wxEmptyString); show(m_details,voice(Copy::Line::NoAdapterSelected));
    m_details->SetName(_L("Selected adapter details")); layout->Add(m_details,0,wxEXPAND|wxALL,gap);
    // Explicit consent for a lossy or metadata/encoding-changing adapter. It is
    // bound to the exact disclosure shown above and lasts for this session.
    m_acknowledge = new LabeledCheckBox(body,_L("I reviewed what this conversion changes or leaves out, and I want to convert with these changes."));
    m_acknowledge->SetName(_L("Accept the data changes of the selected adapter"));
    m_acknowledge->GetCheckBox()->SetName(_L("Accept the data changes of the selected adapter"));
    m_acknowledge->Bind(wxEVT_CHECKBOX,[this](wxCommandEvent &){
        if (m_selected>=m_adapters.size()) return;
        const std::string token=LC::acknowledgement_token(m_adapters[m_selected]);
        if (m_acknowledge->GetValue()) { m_acknowledged.insert(token); update_status(fact(L("Changes confirmed for this adapter. You can now add files."))); }
        else { m_acknowledged.erase(token); update_status(fact(L("Confirmation withdrawn. Files cannot be added with this adapter until you confirm again."))); }
    });
    m_acknowledge->Hide(); layout->Add(m_acknowledge,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,gap);

    // PDF operation settings, each row discoverable through its own search.
    auto *pdf_search=new SearchField(body,_L("Search PDF operation settings")); pdf_search->SetName(_L("PDF operation settings search"));
    layout->Add(pdf_search,0,wxEXPAND|wxALL,gap);
    auto *pdf_settings=new wxBoxSizer(wxVERTICAL);
    std::vector<std::pair<wxSizer*,wxString>> pdf_rows;
    auto *pages_row=new wxBoxSizer(wxHORIZONTAL);
    const auto pages_label=_L("Page order (one-based, comma separated). Default: 1. Blank rotates all pages.");
    pages_row->Add(text(body,pages_label),1,wxALL|wxALIGN_CENTER_VERTICAL,FromDIP(4));
    m_pdf_pages=new TextInput(body,"1",_L("PDF page order")); m_pdf_pages->SetName(_L("PDF page order")); m_pdf_pages->GetTextCtrl()->SetMaxLength(5000);
    pages_row->Add(m_pdf_pages,1,wxALL|wxALIGN_CENTER_VERTICAL,FromDIP(4));
    pdf_settings->Add(pages_row,0,wxEXPAND);pdf_rows.emplace_back(pages_row,pages_label);
    auto *rotation_row=new wxWrapSizer(wxHORIZONTAL);
    m_rotation_label=text(body,_L("Absolute page rotation. Default: 90 degrees.")); m_rotation_label->SetName(_L("PDF rotation degrees"));
    rotation_row->Add(m_rotation_label,0,wxALL|wxALIGN_CENTER_VERTICAL,FromDIP(4));
    for(std::size_t r=0;r!=kRotations.size();++r){const int degrees=kRotations[r];m_rotation[r]=action(body,rotation_row,wxString::Format(_L("%d degrees"),degrees),[this,degrees]{set_rotation(degrees);});}
    pdf_settings->Add(rotation_row,0,wxEXPAND);pdf_rows.emplace_back(rotation_row,_L("PDF rotation degrees"));
    auto *title_row=new wxBoxSizer(wxHORIZONTAL);
    const auto title_label=_L("PDF title for Metadata. Default: empty, which clears the title. Other supported metadata is preserved.");
    title_row->Add(text(body,title_label),1,wxALL|wxALIGN_CENTER_VERTICAL,FromDIP(4));
    m_pdf_title=new TextInput(body,wxEmptyString,_L("PDF title metadata")); m_pdf_title->SetName(_L("PDF title metadata")); m_pdf_title->GetTextCtrl()->SetMaxLength(1000);
    title_row->Add(m_pdf_title,1,wxALL|wxALIGN_CENTER_VERTICAL,FromDIP(4));
    pdf_settings->Add(title_row,0,wxEXPAND);pdf_rows.emplace_back(title_row,title_label);
    layout->Add(pdf_settings,0,wxEXPAND|wxALL,FromDIP(4));
    set_rotation(m_pdf_rotation);
    auto filter_pdf=[this,pdf_search,pdf_settings,pdf_rows]{auto pass=matcher(pdf_search);for(const auto &row:pdf_rows)pdf_settings->Show(row.first,pass.matches(row.second),true);Layout();};
    pdf_search->SetOnQuery([filter_pdf](const wxString&){filter_pdf();});pdf_search->SetOnRegexToggle([filter_pdf](bool){filter_pdf();});

    // Output folder and source admission.
    layout->Add(text(body,_L("Output folder (sources keep their names with the chosen extension)")),0,wxEXPAND|wxLEFT|wxRIGHT|wxTOP,gap);
    auto *destination_row = new wxBoxSizer(wxHORIZONTAL);
    m_destination = new TextInput(body,wxEmptyString,_L("Conversion output folder")); m_destination->SetName(_L("Conversion output folder"));
    destination_row->Add(m_destination,1,wxALIGN_CENTER_VERTICAL|wxALL,FromDIP(4));
    action(body,destination_row,_L("Browse output folder"),[this]{ wxDirDialog dialog(this,_L("Choose output folder")); if (dialog.ShowModal() == wxID_OK) m_destination->GetTextCtrl()->SetValue(dialog.GetPath()); });
    layout->Add(destination_row,0,wxEXPAND|wxALL,FromDIP(4));
    auto *admission = new wxWrapSizer(wxHORIZONTAL);
    action(body,admission,_L("Add source file"),[this]{ choose_source(false); },Button::Variant::Filled);
    action(body,admission,_L("Add source folder"),[this]{ choose_source(true); },Button::Variant::Tonal);
    layout->Add(admission,0,wxEXPAND|wxLEFT|wxRIGHT,FromDIP(4));

    // Queue controls, search, the paged result table and its bulk actions.
    auto *controls = new wxWrapSizer(wxHORIZONTAL);
    action(body,controls,_L("Resume queue"),[this]{ run(); },Button::Variant::Filled);
    action(body,controls,_L("Pause after current file"),[this]{ if (m_queue) { m_queue->pause(true); update_status(fact(L("Paused. The current file may finish; pending files stay saved."))); } });
    action(body,controls,_L("Cancel active and pending"),[this]{ m_cancel = true; if (m_queue) m_queue->cancel_pending(); update_status(fact(L("Cancellation requested. Completed outputs are preserved."))); });
    layout->Add(controls,0,wxEXPAND|wxLEFT|wxRIGHT,FromDIP(4));
    m_queue_search = new SearchField(body,_L("Search this queue page")); m_queue_search->SetName(_L("Converter queue page search"));
    m_queue_search->SetOnQuery([this](const wxString &){ refresh_queue(); }); m_queue_search->SetOnRegexToggle([this](bool){ refresh_queue(); });
    layout->Add(m_queue_search,0,wxEXPAND|wxALL,gap);
    m_jobs = new MD3DataViewListCtrl(body,wxID_ANY,wxDefaultPosition,FromDIP(wxSize(-1,210)),wxDV_MULTIPLE|wxDV_ROW_LINES);
    wxGetApp().UpdateDVCDarkUI(m_jobs); md3_style_data_view(m_jobs); m_jobs->SetMinSize(FromDIP(wxSize(-1,210)));
    m_jobs->SetName(_L("Conversion queue and result history, current page"));
    m_jobs->AppendTextColumn(_L("Number"),wxDATAVIEW_CELL_INERT,FromDIP(70),wxALIGN_LEFT,wxDATAVIEW_COL_RESIZABLE);
    m_jobs->AppendTextColumn(_L("Source name"),wxDATAVIEW_CELL_INERT,FromDIP(190),wxALIGN_LEFT,wxDATAVIEW_COL_RESIZABLE);
    m_jobs->AppendTextColumn(_L("State"),wxDATAVIEW_CELL_INERT,FromDIP(150),wxALIGN_LEFT,wxDATAVIEW_COL_RESIZABLE);
    m_jobs->AppendTextColumn(_L("Result"),wxDATAVIEW_CELL_INERT,FromDIP(280),wxALIGN_LEFT,wxDATAVIEW_COL_RESIZABLE);
    layout->Add(m_jobs,0,wxEXPAND|wxALL,gap);
    auto *bulk = new wxWrapSizer(wxHORIZONTAL);
    action(body,bulk,_L("Select this page"),[this]{ m_jobs->SelectAll(); });
    action(body,bulk,_L("Invert selection"),[this]{
        for (unsigned row=0;row<m_jobs->GetItemCount();++row) { if (m_jobs->IsRowSelected(row)) m_jobs->UnselectRow(row); else m_jobs->SelectRow(row); }
    });
    action(body,bulk,_L("Retry selected"),[this]{
        if (!m_queue || m_running) { update_status(fact(L("Pause and wait for the active file before retrying."))); return; }
        std::size_t accepted=0,rejected=0;
        for (unsigned row=0;row<m_jobs->GetItemCount();++row) {
            if (!m_jobs->IsRowSelected(row) || row>=m_visible_jobs.size()) continue;
            try { m_queue->retry(m_page[m_visible_jobs[row]].id); ++accepted; } catch (...) { ++rejected; }
        }
        update_status(format(fact(L("Retried: %llu. Not eligible or destination already exists: %llu.")),static_cast<unsigned long long>(accepted),static_cast<unsigned long long>(rejected))); refresh_queue();
    });
    action(body,bulk,_L("Export visible results"),[this]{ export_page(); });
    layout->Add(bulk,0,wxEXPAND|wxLEFT|wxRIGHT,FromDIP(4));
    auto *paging = new wxBoxSizer(wxHORIZONTAL);
    action(body,paging,_L("Previous page"),[this]{ m_after = m_after >= LC::Limits::page_size ? m_after-LC::Limits::page_size : 0; refresh_queue(); },Button::Variant::Text);
    action(body,paging,_L("Next page"),[this]{ if (m_queue && m_after + LC::Limits::page_size < m_queue->count()) m_after += LC::Limits::page_size; refresh_queue(); },Button::Variant::Text);
    m_page_label = text(body,wxEmptyString); m_page_label->SetName(_L("Queue page summary")); paging->Add(m_page_label,1,wxALIGN_CENTER_VERTICAL|wxALL,FromDIP(4)); layout->Add(paging,0,wxEXPAND|wxALL,FromDIP(4));
    m_status = text(body,wxEmptyString); show(m_status,voice(Copy::Line::EmptyQueue)); m_status->SetName(_L("Converter status"));
    layout->Add(m_status,0,wxEXPAND|wxALL,gap); body->SetSizer(layout); outer->Add(body,1,wxEXPAND); SetSizer(outer);
    try { m_queue = std::make_unique<LC::Queue>(std::move(queue_directory)); }
    catch (...) { update_status(fact(L("The private queue cannot be opened or another converter owns it. Close the other converter and reopen this destination."))); }
    refresh_catalog(); refresh_queue();
    Bind(wxEVT_TIMER,[this](wxTimerEvent &){
        if (!m_running && m_work.joinable()) {
            m_work.join();
            if(m_catalog_pending.exchange(false)){
                {std::lock_guard<std::mutex> lock(m_catalog_mutex);m_adapters=std::move(m_next_adapters);}
                m_selected=static_cast<std::size_t>(-1);
                show(m_details,voice(Copy::Line::NoAdapterSelected)); update_acknowledgement();
                refresh_catalog();update_status(voice(Copy::Line::ChecksFinished));return;
            }
            Text summary=format(voice(Copy::Line::Stopped),static_cast<unsigned long long>(m_admitted.load()),static_cast<unsigned long long>(m_rejected.load()));
            if(m_preview_kind.load()>=0)
                summary=join({summary,fill(fact(L("Last source preview: %s, %s bytes.")),fact(LC::kind_name(static_cast<LC::Kind>(m_preview_kind.load()))),number(m_preview_bytes.load()))},wxString(" "));
            update_status(summary);
            refresh_queue();
        }
        if (m_running) refresh_queue();
    }); m_timer.Start(500);
    m_running=true;update_status(voice(Copy::Line::Checking));
    m_work=std::thread([this]{auto ready=LC::catalog(m_proof);{std::lock_guard<std::mutex> lock(m_catalog_mutex);m_next_adapters=std::move(ready);}m_catalog_pending=true;m_running=false;});
}
LocalConverterPanel::~LocalConverterPanel() { m_timer.Stop(); stop(); }
void LocalConverterPanel::place_categories()
{
    // The strip may be docked on any edge from its own context menu; the
    // pages follow it, as the other strip hosts do.
    m_category_layout->Detach(m_categories); m_category_layout->Detach(m_category_pages);
    const auto edge = m_categories->GetDockEdge();
    m_category_layout->SetOrientation(m_categories->IsVertical() ? wxHORIZONTAL : wxVERTICAL);
    const bool first = edge == MD3::Tabs::DockEdge::Left || edge == MD3::Tabs::DockEdge::Top;
    if (first) m_category_layout->Add(m_categories,0,wxEXPAND);
    m_category_layout->Add(m_category_pages,1,wxEXPAND);
    if (!first) m_category_layout->Add(m_categories,0,wxEXPAND);
    Layout();
}
void LocalConverterPanel::show_category(const std::string &id)
{
    for (std::size_t i = 0; i != kCategoryIds.size(); ++i)
        if (id == kCategoryIds[i]) { m_category_pages->SetSelection(i); m_categories->Activate(id,false); break; }
}
void LocalConverterPanel::select_adapter(unsigned category)
{
    const int row = m_catalogs[category]->GetSelectedRow();
    if (row == wxNOT_FOUND || static_cast<std::size_t>(row) >= m_visible[category].size()) return;
    m_selected = m_visible[category][row]; const auto &a = m_adapters[m_selected];
    const Text name = fact(a.name);
    const Text disclosure = a.disclosure.empty() ? Text{} : fact(a.disclosure);
    if (a.enabled)
        show(m_details,join({name,disclosure,fill(fact(L("Validation: %s")),fact(a.validator)),
                              LC::requires_acknowledgement(a) ? fact(L("This conversion changes or leaves out data as described above. Confirm below before adding files.")) : Text{}},wxString("\n")));
    else
        show(m_details,join({name,disclosure,fill(fact(L("Unavailable: %s")),fact(a.reason)),
                              a.detail.empty() ? Text{} : fill(fact(L("Diagnostic: %s")),result_text(a.detail))},wxString("\n")));
    update_acknowledgement();
}
void LocalConverterPanel::update_acknowledgement()
{
    // Shown only for an enabled adapter that changes or leaves out data; its
    // state is whether this exact disclosure was accepted in this session.
    const bool needed = m_selected < m_adapters.size() && m_adapters[m_selected].enabled && LC::requires_acknowledgement(m_adapters[m_selected]);
    if (needed) m_acknowledge->SetValue(m_acknowledged.count(LC::acknowledgement_token(m_adapters[m_selected])) != 0);
    m_acknowledge->Show(needed);
    Layout();
}
void LocalConverterPanel::set_rotation(int degrees)
{
    m_pdf_rotation = degrees;
    for (std::size_t r = 0; r != kRotations.size(); ++r) { m_rotation[r]->SetVariant(kRotations[r] == degrees ? Button::Variant::Filled : Button::Variant::Outlined); m_rotation[r]->Refresh(); }
    m_rotation_label->SetLabel(wxString::Format(_L("Absolute page rotation: %d degrees."),degrees));
}
void LocalConverterPanel::update_status(const Text &message) { show(m_status,message); Layout(); }
void LocalConverterPanel::refresh_catalog()
{
    for (unsigned c=0;c!=kCategoryIds.size();++c) {
        m_catalogs[c]->DeleteAllItems(); m_visible[c].clear(); auto pass = matcher(m_search[c]);
        for (std::size_t i=0;i<m_adapters.size();++i) {
            const auto &a=m_adapters[i]; if (static_cast<unsigned>(a.category)!=c) continue;
            const Text name=fact(a.name), state=availability(a);
            const wxString haystack=searchable(a.name,name)+" "+searchable(a.reason+" "+a.disclosure+" "+a.detail,state)+" "+cell(a.disclosure.empty() ? Text{} : fact(a.disclosure));
            if (!pass.matches(haystack)) continue;
            add_row(m_catalogs[c],{cell(name),cell(state)}); m_visible[c].push_back(i);
        }
        m_empty[c]->Show(m_visible[c].empty());
    }
    Layout();
}
void LocalConverterPanel::refresh_queue()
{
    if (!m_queue) return;
    try {
        // Preserve selected stable IDs across the progress refresh.
        std::vector<std::uint64_t> selected;
        for(unsigned row=0;row<m_jobs->GetItemCount();++row)
            if(m_jobs->IsRowSelected(row)&&row<m_visible_jobs.size()) selected.push_back(m_page[m_visible_jobs[row]].id);
        m_page=m_queue->page(m_after); m_jobs->DeleteAllItems(); m_visible_jobs.clear(); auto pass=matcher(m_queue_search);
        for(std::size_t i=0;i<m_page.size();++i) {
            const auto &j=m_page[i]; const auto name=j.source.filename().u8string();
            const Text state=fact(LC::state_name(j.state)), result=result_text(j.code);
            if(!pass.matches(utf(name)+" "+searchable(LC::state_name(j.state),state)+" "+searchable(j.code,result))) continue;
            add_row(m_jobs,{wxString::Format("%llu",static_cast<unsigned long long>(j.id)),utf(name),cell(state),cell(result)});
            m_visible_jobs.push_back(i);
            if(std::find(selected.begin(),selected.end(),j.id)!=selected.end()) m_jobs->SelectRow(static_cast<unsigned>(m_visible_jobs.size()-1));
        }
        show(m_page_label,format(fact(L("%llu saved files. Showing %llu matches from at most 100 records.")),static_cast<unsigned long long>(m_queue->count()),static_cast<unsigned long long>(m_visible_jobs.size())));
    } catch (...) { update_status(fact(L("A saved queue record could not be read. Records and outputs have been retained for recovery."))); }
}
void LocalConverterPanel::choose_source(bool folder)
{
    if (!m_queue || m_running) { update_status(fact(L("Wait for the current operation before adding sources."))); return; }
    if (m_selected>=m_adapters.size() || !m_adapters[m_selected].enabled) { update_status(fact(L("Select an enabled adapter first. Unavailable rows explain their missing packaged adapter."))); return; }
    // A lossy or metadata/encoding-changing conversion needs the user's explicit
    // acceptance of its disclosure before any file is admitted.
    const auto &chosen=m_adapters[m_selected];
    if (LC::requires_acknowledgement(chosen) && m_acknowledged.count(LC::acknowledgement_token(chosen))==0) {
        update_status(fact(L("Confirm what this conversion changes before adding files. Its disclosure lists what changes or is left out.")));
        m_acknowledge->GetCheckBox()->SetFocus(); return;
    }
    const bool merge=chosen.id=="pdf.merge";
    if(folder && merge){update_status(fact(L("PDF merge needs an explicit ordered selection. Use Add source file and select at least two PDFs.")));return;}
    if(folder) { wxDirDialog dialog(this,_L("Choose source folder, discovered one file at a time")); if(dialog.ShowModal()==wxID_OK) admit(fs::path(dialog.GetPath().ToStdWstring()),true); }
    else { wxFileDialog dialog(this,merge?_L("Choose PDFs to merge, in picker order"):_L("Choose source file"),wxEmptyString,wxEmptyString,_L("All files (*.*)|*.*"),wxFD_OPEN|wxFD_FILE_MUST_EXIST|(merge?wxFD_MULTIPLE:0));
        if(dialog.ShowModal()==wxID_OK){
            if(merge){wxArrayString paths;dialog.GetPaths(paths);if(paths.size()<2||paths.size()>1000){update_status(fact(L("Select between 2 and 1000 PDFs within the aggregate 16 MiB limit.")));return;}
                std::vector<fs::path> additional;for(std::size_t i=1;i<paths.size();++i)additional.emplace_back(paths[i].ToStdWstring());admit(fs::path(paths[0].ToStdWstring()),false,std::move(additional));}
            else admit(fs::path(dialog.GetPath().ToStdWstring()),false);
        }
    }
}
void LocalConverterPanel::admit(fs::path source,bool folder,std::vector<fs::path> additional)
{
    fs::path destination(field_value(m_destination).ToStdWstring());
    std::error_code ec; if(!fs::is_directory(destination,ec)) { update_status(fact(L("Choose an existing output folder."))); return; }
    const auto adapter=m_adapters[m_selected]; std::string options;
    if(adapter.id.rfind("pdf.",0)==0){
        nlohmann::json data=nlohmann::json::object();
        if(adapter.id=="pdf.extract"||adapter.id=="pdf.reorder"||adapter.id=="pdf.rotate"){
            data["pages"]=nlohmann::json::array();std::stringstream list(field_value(m_pdf_pages).ToStdString());std::string item;
            while(std::getline(list,item,',')){std::stringstream field(item);unsigned n=0;field>>n;field>>std::ws;if(!field.eof()||!n||n>1000){update_status(fact(L("Enter page numbers from 1 to 1000, separated by commas.")));return;}data["pages"].push_back(n);}
        }
        if(adapter.id=="pdf.rotate")data["rotation"]=m_pdf_rotation;
        if(adapter.id=="pdf.metadata")data["metadata"]={{"Title",field_value(m_pdf_title).ToUTF8().data()}};
        options=data.dump();
    }
    // The accepted disclosure travels with every admitted record; the queue
    // refuses the admission without it.
    const std::string acknowledgement=LC::requires_acknowledgement(adapter) && m_acknowledged.count(LC::acknowledgement_token(adapter)) ? LC::acknowledgement_token(adapter) : std::string();
    if(m_work.joinable()) m_work.join(); m_cancel=false; m_running=true; m_admitted=0; m_rejected=0;
    update_status(fact(L("Discovering sources with bounded memory. Cancel stops discovery; admitted records stay saved.")));
    m_work=std::thread([this,source=std::move(source),destination=std::move(destination),adapter,folder,options=std::move(options),additional=std::move(additional),acknowledgement]{
        auto add=[&](const fs::path &path){
            if(m_cancel.load()) return;
            fs::path target=destination/path.filename();target.replace_extension("."+adapter.extension);
            auto rejected=[&](const char *code){++m_rejected;try{m_queue->record_rejected(path,target,adapter.id,code);}catch(...){}};
            try {
                if(!fs::is_regular_file(fs::symlink_status(path))) { rejected("source_not_regular"); return; }
                const auto size=fs::file_size(path); if(size>LC::Limits::input_bytes) {rejected("input_limit");return;}
                std::ifstream in(path,std::ios::binary); LC::Bytes data(static_cast<std::size_t>(size));
                in.read(reinterpret_cast<char *>(data.data()),static_cast<std::streamsize>(size));
                if(!in || in.peek()!=std::char_traits<char>::eof()) {rejected("source_unreadable_or_changed");return;}
                const auto kind=LC::detect(data); m_preview_kind=static_cast<int>(kind);m_preview_bytes=size;
                if(std::find(adapter.sources.begin(),adapter.sources.end(),kind)==adapter.sources.end()) {rejected("incompatible_source_signature");return;}
                m_queue->enqueue(path,target,adapter.id,options,additional,acknowledgement); ++m_admitted;
            } catch (const std::runtime_error &e) {
                // The queue refuses with a stable code (for example
                // disclosure_not_acknowledged); keep it when it is one.
                const std::string code=e.what();
                rejected(LC::result_message(code)!=nullptr ? code.c_str() : "admission_preflight_failed");
            } catch (...) { rejected("admission_preflight_failed"); }
        };
        try { if(folder) { for(const auto &entry:fs::recursive_directory_iterator(source,fs::directory_options::none)) { if(m_cancel.load()) break; if(entry.is_regular_file()) add(entry.path()); } } else add(source); }
        catch (...) { ++m_rejected;try{m_queue->record_rejected(source,destination,adapter.id,"directory_discovery_incomplete");}catch(...){} }
        m_running=false;
    });
}
void LocalConverterPanel::run()
{
    if(!m_queue || m_running) return;
    std::string reason; if(!LC::verify_package(m_proof,reason)) { update_status(fact(reason)); return; }
    if(m_work.joinable()) m_work.join(); m_cancel=false; m_running=true; m_queue->pause(false);
    update_status(voice(Copy::Line::Converting));
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
    // State and result stay stable identifiers for tools; the message column is
    // the result sentence in the language shown.
    std::string out="id,source_name,state,result,message\r\n";
    auto quote=[](std::string value){ std::string q="\""; for(char c:value){q+=c;if(c=='"')q+='"';} return q+'"'; };
    for(auto i:m_visible_jobs) {
        const auto &j=m_page[i];
        out+=std::to_string(j.id)+","+quote(j.source.filename().u8string())+","+quote(LC::state_name(j.state))+","+quote(j.code)+","+quote(result_text(j.code).primary.ToUTF8().data())+"\r\n";
    }
    std::string code; const bool ok=LC::atomic_create(fs::path(dialog.GetPath().ToStdWstring()),LC::Bytes(out.begin(),out.end()),code);
    update_status(ok ? voice(Copy::Line::Exported) : fact(L("Export was not written. Choose a new destination in a writable folder.")));
}
} // namespace Slic3r::GUI
