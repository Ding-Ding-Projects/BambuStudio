#include "OllamaSuiteDialog.hpp"
#include "OllamaClient.hpp"
#include "OllamaSuiteText.hpp"
#include "NativeLaunchAdapter.hpp"
#include "libslic3r/OllamaSuite/LaunchProfiles.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/ListBox.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/SuperConfirmGate.hpp"
#include "../Widgets/TextInput.hpp"
#include "../Widgets/TextArea.hpp"
#include "../Widgets/TabStrip.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include <wx/dialog.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include <wx/wrapsizer.h>
#include <wx/filedlg.h>
#include <wx/base64.h>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace Slic3r::GUI {
namespace {
using namespace Slic3r::OllamaSuite;
using Model = Slic3r::OllamaSuite::Model;
wxString u8(const std::string &s) { return wxString::FromUTF8(s.c_str()); }
std::string utf8(const wxString &s) { auto bytes=s.ToUTF8(); return bytes ? std::string(bytes.data(),bytes.length()) : std::string(); }
FitSettings saved_fit_settings(const std::filesystem::path &root) {
    try { if(std::filesystem::exists(root/"fit-settings.json")) return load_fit_settings(read_json(root/"fit-settings.json",4096)); } catch(...) {}
    return {};
}
class SuiteDialog final : public wxDialog {
    struct WorkerState {
        std::mutex mutex; std::atomic_bool cancel{false}; bool busy=false, changed=false;
        std::string status; std::vector<Model> installed; std::set<std::string> running;
        CatalogSnapshot catalog; std::optional<Model> selected; std::string response;
        Json messages=Json::array();
        std::optional<ValidatedLaunchPlan> launch_plan;
        std::string profile_preview;
        // Hardware evidence and the verdict for the selected model; both are recomputed, never cached across inputs.
        Hardware hardware; std::string runtime_version; FitSettings fit_settings; std::optional<FitResult> fit;
        bool details_ready=false, hardware_ready=false, measured_notice=false, inventory_known=false;
        // The latest refresh attempt; a failed one is reported beside, never instead of, the last verified catalog.
        std::optional<CatalogAttempt> catalog_attempt; bool catalog_ready=false;
    };
public:
    SuiteDialog(wxWindow *parent,const std::filesystem::path &root)
        :wxDialog(parent,wxID_ANY,_L("Local Ollama suite"),wxDefaultPosition,wxDefaultSize,wxRESIZE_BORDER|wxBORDER_NONE),
         m_root(root/"ollama-suite"),m_queue(m_root/"pulls"),m_history(m_root/"chats"),m_timer(this) {
        m_state.fit_settings=saved_fit_settings(m_root);
        SetName("ollama-suite"); SetBackgroundColour(StateColor::semantic(MD3::Role::Surface));
        SetSize(FromDIP(wxSize(960,760))); SetMinSize(FromDIP(wxSize(640,560)));
        auto *outer=new wxBoxSizer(wxVERTICAL); outer->Add(new MD3DialogCaption(this,_L("Local Ollama suite")),0,wxEXPAND);
        m_status=new Label(this,_L("Local data only. Refresh runtime to inspect Ollama on this computer."),LB_AUTO_WRAP);
        outer->Add(m_status,0,wxEXPAND|wxALL,FromDIP(12));
        m_body=new wxBoxSizer(wxHORIZONTAL);
        TabStrip::Options options; options.surface_key="ollama_suite"; options.surface_name=_L("Local Ollama suite"); options.strip_name=_L("Suite sections"); options.default_edge=MD3::Tabs::DockEdge::Left;
        m_tabs=new TabStrip(this,options); m_book=new wxSimplebook(this);
        m_body->Add(m_tabs,0,wxEXPAND); m_body->Add(m_book,1,wxEXPAND); outer->Add(m_body,1,wxEXPAND);
        build_store(); build_chat(); build_queue(); build_profiles(); build_help();
        m_tabs->Bind(EVT_TABSTRIP_ACTIVATE,[this](wxCommandEvent &e) { const auto id=e.GetString().ToStdString(); for(std::size_t i=0;i<m_sections.size();++i) if(m_sections[i]==id) m_book->SetSelection(i); });
        m_tabs->Bind(EVT_TABSTRIP_DOCK_CHANGED,[this](wxCommandEvent &) { layout_tabs(); });
        m_tabs->LoadLayout(); m_tabs->Activate("models",false); layout_tabs();
        auto *stop=new Button(this,_L("Stop current operation")); stop->SetVariant(Button::Variant::Outlined);
        stop->Bind(wxEVT_BUTTON,[this](wxCommandEvent &) { m_state.cancel=true; if(m_launcher)m_launcher->request_cancel(); }); outer->Add(stop,0,wxALL|wxALIGN_RIGHT,FromDIP(12));
        SetSizer(outer); MD3DialogCaption::FinishChrome(this); CentreOnParent();
        Bind(wxEVT_TIMER,[this](wxTimerEvent &) { poll(); if(++m_ticks%500==0) show_catalog_age(); }); m_timer.Start(120);
        try { m_queue.recover();
            if(std::filesystem::exists(m_root/"catalog-attempt.json")) m_state.catalog_attempt=load_attempt(read_json(m_root/"catalog-attempt.json",4096));
            if(std::filesystem::exists(m_root/"catalog.json")) {
                // A saved catalog that no longer re-traverses or matches its revision is reported, never shown.
                auto saved=load_catalog(read_json(m_root/"catalog.json"),content_identity);
                if(!catalog_verified(saved)) throw std::runtime_error("Saved catalog is not verified");
                m_state.catalog=std::move(saved); } }
        catch(...) { m_status->SetLabel(_L("Saved suite data is invalid or unavailable. Existing files were retained.")); }
        // Measuring is local and quick, so every catalog and installed entry opens with a current verdict.
        m_state.hardware=measure_hardware(); m_hardware->SetLabel(OllamaText::hardware_summary(m_state.hardware));
        render_models(); render_queue();
    }
    ~SuiteDialog() override { m_timer.Stop(); m_state.cancel=true; if(m_launcher)m_launcher->request_cancel(); if(m_worker.joinable()) m_worker.join(); }
private:
    std::filesystem::path m_root; PullQueue m_queue; ChatStore m_history; WorkerState m_state;
    std::thread m_worker; wxTimer m_timer; std::size_t m_ticks=0; Label *m_status=nullptr,*m_catalog_status=nullptr;
    TabStrip *m_tabs=nullptr; wxSimplebook *m_book=nullptr; wxBoxSizer *m_body=nullptr;
    std::vector<std::string> m_sections; SearchField *m_model_search=nullptr;
    ListBox *m_models=nullptr,*m_pulls=nullptr; TextArea *m_details=nullptr,*m_prompt=nullptr,*m_system=nullptr,*m_output=nullptr;
    std::vector<StoreEntry> m_store; std::vector<StoreRow> m_rows; std::size_t m_model_offset=0,m_queue_offset=0,m_history_offset=0; ChatSession m_session;
    SearchField *m_history_search=nullptr; ListBox *m_sessions=nullptr; TextInput *m_title=nullptr;
    Button *m_attach=nullptr;
    std::unique_ptr<NativeLaunchAdapter> m_launcher; std::vector<LaunchProfile> m_profiles;
    Button *m_launch=nullptr;
    TextArea *m_profile_preview=nullptr;
    TextInput *m_temperature=nullptr,*m_context=nullptr,*m_predict=nullptr;
    Label *m_hardware=nullptr; ComboBox *m_fit_context=nullptr,*m_fit_cache=nullptr;
    // Model Store pickers. Dynamic pickers keep the value behind each item: nullopt is "any", "" is "not verified".
    ComboBox *m_filter_state=nullptr,*m_filter_family=nullptr,*m_filter_variant=nullptr,*m_filter_capability=nullptr,*m_filter_quantization=nullptr;
    ComboBox *m_filter_size=nullptr,*m_filter_fit=nullptr,*m_store_group=nullptr,*m_store_sort=nullptr; Label *m_query_summary=nullptr;
    std::vector<std::optional<std::string>> m_family_values,m_variant_values,m_capability_values,m_quantization_values;
    std::vector<ChatSession> m_history_rows; std::string m_attachment;
    std::pair<wxWindow *,wxBoxSizer *> section(const std::string &id,const wxString &name) {
        auto *panel=new MD3ScrolledWindow(m_book,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxVSCROLL);
        panel->SetScrollRate(0,FromDIP(12)); auto *s=new wxBoxSizer(wxVERTICAL); panel->SetSizer(s);
        m_book->AddPage(panel,name); m_sections.push_back(id); m_tabs->AddTab(id,name); return {panel,s};
    }
    Button *action(wxWindow *p,wxSizer *s,const wxString &name,std::function<void()> callback) {
        auto *b=new Button(p,name); b->SetVariant(Button::Variant::Outlined); b->SetButtonSize(Button::Size::Large);
        b->Bind(wxEVT_BUTTON,[callback](wxCommandEvent &) { callback(); }); s->Add(b,0,wxALL,FromDIP(4)); return b;
    }
    void note(wxWindow *p,wxSizer *s,const wxString &message) { s->Add(new Label(p,message,LB_AUTO_WRAP),0,wxEXPAND|wxALL,FromDIP(10)); }
    void layout_tabs() {
        m_body->Detach(m_tabs); m_body->Detach(m_book); m_body->SetOrientation(m_tabs->IsVertical()?wxHORIZONTAL:wxVERTICAL);
        auto edge=m_tabs->GetDockEdge(); bool after=edge==MD3::Tabs::DockEdge::Right||edge==MD3::Tabs::DockEdge::Bottom;
        if(after) { m_body->Add(m_book,1,wxEXPAND); m_body->Add(m_tabs,0,wxEXPAND); }
        else { m_body->Add(m_tabs,0,wxEXPAND); m_body->Add(m_book,1,wxEXPAND); } Layout();
    }
    void start(std::function<void()> work) {
        { std::lock_guard<std::mutex> lock(m_state.mutex); if(m_state.busy) { m_status->SetLabel(_L("An operation is already running. Stop it or wait for its result.")); return; } m_state.busy=true; m_state.cancel=false; }
        if(m_worker.joinable()) m_worker.join();
        m_status->SetLabel(_L("Working locally. Stop is available; progress will appear here."));
        m_worker=std::thread([this,work=std::move(work)] {
            try { work(); } catch(...) { std::lock_guard<std::mutex> lock(m_state.mutex); m_state.status="Operation failed validation or could not persist local state. Open Troubleshooting."; }
            std::lock_guard<std::mutex> lock(m_state.mutex); m_state.busy=false; m_state.changed=true;
        });
    }
    void set_status(const std::string &s) { std::lock_guard<std::mutex> lock(m_state.mutex); m_state.status=s; m_state.changed=true; }
    void poll() {
        bool done=false; { std::lock_guard<std::mutex> lock(m_state.mutex); if(!m_state.changed) return; m_state.changed=false;
            m_status->SetLabel(u8(m_state.status.substr(0,m_state.status.find('\n')))); m_output->SetValue(u8(m_state.response)); done=!m_state.busy;
            if(!m_state.profile_preview.empty())m_profile_preview->SetValue(u8(m_state.profile_preview));
            if(done&&!m_state.messages.empty()) { m_session.messages=m_state.messages; m_state.messages=Json::array(); }
            if(m_state.hardware_ready) { m_hardware->SetLabel(OllamaText::hardware_summary(m_state.hardware)); m_state.hardware_ready=false; }
            if(done&&m_state.measured_notice) { m_status->SetLabel(OllamaText::ui(OllamaText::Ui::MeasuredAgain)); m_state.measured_notice=false; }
            if(done&&m_state.catalog_ready&&m_state.catalog_attempt) { m_status->SetLabel(OllamaText::catalog_outcome(*m_state.catalog_attempt)); m_state.catalog_ready=false; }
            if(done&&m_state.details_ready&&m_state.selected&&m_state.fit) {
                m_details->SetValue(OllamaText::model_explanation(*m_state.selected,m_state.fit->verdict)+"\n\n"+OllamaText::fit_report(*m_state.selected,*m_state.fit)); m_state.details_ready=false;
                m_status->SetLabel(OllamaText::ui(OllamaText::Ui::InspectionFinished)); }
            m_attach->Enable(done&&m_state.selected&&m_state.selected->capabilities_verified&&m_state.selected->local&&m_state.selected->capabilities.count("vision"));
            m_launch->Enable(done&&m_state.launch_plan.has_value()); }
        if(done) { render_models(); render_queue(); render_history(); } Layout();
    }
    void build_store() {
        auto [p,s]=section("models",_L("Models")); auto *buttons=new wxWrapSizer(wxHORIZONTAL);
        action(p,buttons,_L("Refresh runtime"),[this] { refresh_runtime(); });
        action(p,buttons,_L("Refresh official catalog"),[this] { start([this] {
            LocalClient c; auto snapshot=refresh_catalog([&](const std::string &path) { return c.catalog_page(path,m_state.cancel); },m_state.cancel);
            // Every verified traversal replaces the saved catalog with its revision; a failed one is only recorded.
            const bool verified=catalog_verified(snapshot);
            if(verified) { snapshot.revision=catalog_revision(snapshot,content_identity); atomic_json(m_root/"catalog.json",catalog_json(snapshot)); }
            const auto attempt=catalog_attempt(snapshot);
            try { atomic_json(m_root/"catalog-attempt.json",attempt_json(attempt)); } catch(...) {}
            std::lock_guard<std::mutex> lock(m_state.mutex); m_state.catalog_attempt=attempt; m_state.catalog_ready=true;
            if(verified) m_state.catalog=std::move(snapshot); else m_state.catalog.offline=true;
        }); }); s->Add(buttons,0,wxEXPAND);
        build_hardware(p,s);
        m_catalog_status=new Label(p,_L("Catalog has not been verified."),LB_AUTO_WRAP); s->Add(m_catalog_status,0,wxEXPAND|wxALL,FromDIP(8));
        m_model_search=new SearchField(p,_L("Search exact model, family, capability or quantization")); s->Add(m_model_search,0,wxEXPAND|wxALL,FromDIP(8));
        m_model_search->SetOnQuery([this](const wxString &) { m_model_offset=0; render_models(); });
        m_model_search->SetOnRegexToggle([this](bool) { m_model_offset=0; render_models(); });
        build_store_filters(p,s);
        m_models=new ListBox(p,wxID_ANY,FromDIP(wxSize(-1,220))); m_models->SetName("Ollama model inventory"); s->Add(m_models,0,wxEXPAND|wxALL,FromDIP(8));
        m_models->Bind(wxEVT_LISTBOX,[this](wxCommandEvent &) { inspect_selected(); });
        auto *pages=new wxWrapSizer(wxHORIZONTAL);
        action(p,pages,_L("Previous models"),[this] { if(m_model_offset>=100) m_model_offset-=100; render_models(); });
        action(p,pages,_L("Next models"),[this] { if(m_model_offset+100<m_rows.size()) m_model_offset+=100; render_models(); }); s->Add(pages,0,wxEXPAND);
        m_details=new TextArea(p,_L("Select an installed model to inspect verified capabilities and hardware evidence."),FromDIP(wxSize(-1,150))); m_details->SetReadOnly(true); s->Add(m_details,0,wxEXPAND|wxALL,FromDIP(8));
        auto *choose=new wxWrapSizer(wxHORIZONTAL);
        action(p,choose,_L("Use selected model for chat"),[this] { std::lock_guard<std::mutex> lock(m_state.mutex); if(m_state.busy||!m_state.selected||!m_state.selected->capabilities_verified||!m_state.selected->local) { m_status->SetLabel(_L("Finish the current operation and inspect a verified installed local model first.")); return; } m_session={unique_id(),"Local session",m_state.selected->name,Json::array()}; m_state.messages=Json::array(); m_state.response.clear(); m_output->Clear(); m_tabs->Activate("chat"); });
        action(p,choose,_L("Add selected tag to batch"),[this] { const auto model=selected_model(); if(!model) return; try { m_queue.add(model->name); render_queue(); m_tabs->Activate("pulls"); } catch(...) { m_status->SetLabel(_L("Could not persist the batch item.")); } }); s->Add(choose,0,wxEXPAND);
    }
    static constexpr StoreState store_states[]={StoreState::Running,StoreState::Installed,StoreState::Catalog};
    static constexpr SizeBand size_bands[]={SizeBand::Under2GiB,SizeBand::From2To8GiB,SizeBand::From8To32GiB,SizeBand::Over32GiB,SizeBand::Unknown};
    static constexpr OllamaSuite::Fit fit_verdicts[]={OllamaSuite::Fit::RunsWell,OllamaSuite::Fit::WithLimits,OllamaSuite::Fit::Unlikely,OllamaSuite::Fit::Unknown}; // wxWindow::Fit hides the type name here
    static constexpr StoreGroup store_groups[]={StoreGroup::None,StoreGroup::Family,StoreGroup::State,StoreGroup::Fit,StoreGroup::Quantization,StoreGroup::Size};
    static constexpr StoreSort store_sorts[]={StoreSort::Name,StoreSort::Family,StoreSort::SizeAscending,StoreSort::SizeDescending,StoreSort::Fit};
    ComboBox *store_picker(wxWindow *p,wxSizer *row,OllamaText::StoreUi name,int width) {
        auto *column=new wxBoxSizer(wxVERTICAL); column->Add(new Label(p,OllamaText::store_ui(name)),0,wxBOTTOM,FromDIP(2));
        auto *picker=new ComboBox(p,wxID_ANY,wxEmptyString,wxDefaultPosition,FromDIP(wxSize(width,-1)),0,nullptr,wxCB_READONLY); picker->SetName(OllamaText::store_ui(name));
        column->Add(picker,0); row->Add(column,0,wxALL,FromDIP(4));
        picker->Bind(wxEVT_COMBOBOX,[this](wxCommandEvent &) { m_model_offset=0; render_models(); });
        return picker;
    }
    void build_store_filters(wxWindow *p,wxSizer *s) {
        using OllamaText::StoreUi;
        auto *row=new wxWrapSizer(wxHORIZONTAL);
        m_filter_state=store_picker(p,row,StoreUi::StateFilter,150); m_filter_state->Append(OllamaText::store_ui(StoreUi::AnyState));
        for(auto state:store_states) m_filter_state->Append(OllamaText::state_text(state));
        m_filter_family=store_picker(p,row,StoreUi::FamilyFilter,200);
        m_filter_variant=store_picker(p,row,StoreUi::VariantFilter,200);
        m_filter_capability=store_picker(p,row,StoreUi::CapabilityFilter,180);
        m_filter_quantization=store_picker(p,row,StoreUi::QuantizationFilter,180);
        m_filter_size=store_picker(p,row,StoreUi::SizeFilter,170); m_filter_size->Append(OllamaText::store_ui(StoreUi::AnySize));
        for(auto band:size_bands) m_filter_size->Append(OllamaText::size_band_text(band));
        m_filter_fit=store_picker(p,row,StoreUi::FitFilter,170); m_filter_fit->Append(OllamaText::store_ui(StoreUi::AnyFit));
        for(auto verdict:fit_verdicts) m_filter_fit->Append(OllamaText::verdict_text(verdict));
        m_store_group=store_picker(p,row,StoreUi::Grouping,200); for(auto group:store_groups) m_store_group->Append(OllamaText::group_text(group));
        m_store_sort=store_picker(p,row,StoreUi::SortOrder,240); for(auto sort:store_sorts) m_store_sort->Append(OllamaText::sort_text(sort));
        for(auto *picker:{m_filter_state,m_filter_size,m_filter_fit,m_store_group,m_store_sort}) picker->SetSelection(0);
        s->Add(row,0,wxEXPAND);
        auto *actions=new wxWrapSizer(wxHORIZONTAL);
        action(p,actions,OllamaText::store_ui(StoreUi::ClearFilters),[this] {
            for(auto *picker:{m_filter_state,m_filter_family,m_filter_variant,m_filter_capability,m_filter_quantization,m_filter_size,m_filter_fit,m_store_group,m_store_sort}) if(picker->GetCount()>0) picker->SetSelection(0);
            m_model_offset=0; render_models(); });
        s->Add(actions,0,wxEXPAND);
        m_query_summary=new Label(p,wxString(),LB_AUTO_WRAP); s->Add(m_query_summary,0,wxEXPAND|wxALL,FromDIP(8));
    }
    static std::optional<std::string> picked(ComboBox *picker,const std::vector<std::optional<std::string>> &values) {
        const int i=picker->GetSelection(); return i>=0&&static_cast<std::size_t>(i)<values.size() ? values[static_cast<std::size_t>(i)] : std::nullopt;
    }
    // Repopulates a picker only when its values changed, keeping the chosen value when it still exists.
    static void set_options(ComboBox *picker,std::vector<std::optional<std::string>> &values,std::vector<std::optional<std::string>> next,const std::function<wxString(const std::optional<std::string> &)> &label) {
        if(next==values) return;
        const auto current=picked(picker,values); picker->Clear(); int keep=0;
        for(std::size_t i=0;i<next.size();++i) { picker->Append(label(next[i])); if(next[i]==current) keep=static_cast<int>(i); }
        picker->SetSelection(keep); values=std::move(next);
    }
    void update_store_options(const std::vector<StoreEntry> &entries) {
        using OllamaText::StoreUi;
        const auto facets=store_facets(entries);
        std::vector<std::optional<std::string>> families{std::nullopt},capabilities{std::nullopt},quantizations{std::nullopt},variants{std::nullopt};
        for(const auto &f:facets.families) families.push_back(f);
        for(const auto &c:facets.capabilities) capabilities.push_back(c);
        if(facets.unverified_capabilities) capabilities.push_back(std::string());
        for(const auto &q:facets.quantizations) quantizations.push_back(q);
        if(facets.unverified_quantization) quantizations.push_back(std::string());
        set_options(m_filter_family,m_family_values,families,[](const std::optional<std::string> &v) { return v ? u8(*v) : OllamaText::store_ui(StoreUi::AnyFamily); });
        set_options(m_filter_capability,m_capability_values,capabilities,[](const std::optional<std::string> &v) { return !v ? OllamaText::store_ui(StoreUi::AnyCapability) : v->empty() ? OllamaText::store_ui(StoreUi::CapabilitiesUnverified) : u8(*v); });
        set_options(m_filter_quantization,m_quantization_values,quantizations,[](const std::optional<std::string> &v) { return !v ? OllamaText::store_ui(StoreUi::AnyQuantization) : v->empty() ? OllamaText::store_ui(StoreUi::QuantizationUnverified) : u8(*v); });
        // Variants are listed for one family at a time; a family has tens of tags, the catalog thousands.
        const auto family=picked(m_filter_family,m_family_values);
        if(family) for(const auto &v:family_variants(entries,*family)) variants.push_back(v);
        set_options(m_filter_variant,m_variant_values,variants,[](const std::optional<std::string> &v) { return v ? u8(*v) : OllamaText::store_ui(StoreUi::AnyVariant); });
        m_filter_variant->Enable(family.has_value());
        m_filter_variant->SetToolTip(family ? wxString() : OllamaText::store_ui(StoreUi::ChooseFamilyFirst));
    }
    StoreQuery current_query() const {
        StoreQuery q; int i=0;
        if((i=m_filter_state->GetSelection())>0) q.state=store_states[i-1];
        q.family=picked(m_filter_family,m_family_values); q.variant=picked(m_filter_variant,m_variant_values);
        q.capability=picked(m_filter_capability,m_capability_values); q.quantization=picked(m_filter_quantization,m_quantization_values);
        if((i=m_filter_size->GetSelection())>0) q.size=size_bands[i-1];
        if((i=m_filter_fit->GetSelection())>0) q.fit=fit_verdicts[i-1];
        if((i=m_store_group->GetSelection())>0) q.group=store_groups[i];
        if((i=m_store_sort->GetSelection())>0) q.sort=store_sorts[i];
        return q;
    }
    const StoreRow *selected_row() const {
        const int i=m_models->GetSelection(); if(i<0) return nullptr;
        const auto row=m_model_offset+static_cast<std::size_t>(i); return row<m_rows.size() ? &m_rows[row] : nullptr;
    }
    std::optional<Model> selected_model() const { const auto *row=selected_row(); if(!row||row->header) return std::nullopt; return m_store[row->entry].model; }
    void build_hardware(wxWindow *p,wxSizer *s) {
        m_hardware=new Label(p,OllamaText::hardware_summary(Hardware{}),LB_AUTO_WRAP); m_hardware->SetName(OllamaText::ui(OllamaText::Ui::HardwareEvidence)); s->Add(m_hardware,0,wxEXPAND|wxALL,FromDIP(8));
        FitSettings settings; { std::lock_guard<std::mutex> lock(m_state.mutex); settings=m_state.fit_settings; }
        auto *row=new wxWrapSizer(wxHORIZONTAL);
        action(p,row,OllamaText::ui(OllamaText::Ui::MeasureAgain),[this] { recompute_fit(); });
        row->Add(new Label(p,OllamaText::ui(OllamaText::Ui::EstimateContext)),0,wxALIGN_CENTER_VERTICAL|wxALL,FromDIP(4));
        m_fit_context=new ComboBox(p,wxID_ANY,wxEmptyString,wxDefaultPosition,FromDIP(wxSize(220,-1)),0,nullptr,wxCB_READONLY); m_fit_context->SetName(OllamaText::ui(OllamaText::Ui::EstimateContext));
        const auto &contexts=fit_contexts();
        for(std::size_t i=0;i<contexts.size();++i) { m_fit_context->Append(OllamaText::context_choice(contexts[i])); if(contexts[i]==settings.context) m_fit_context->SetSelection(static_cast<int>(i)); }
        row->Add(m_fit_context,0,wxALL,FromDIP(4));
        row->Add(new Label(p,OllamaText::ui(OllamaText::Ui::CachePrecision)),0,wxALIGN_CENTER_VERTICAL|wxALL,FromDIP(4));
        m_fit_cache=new ComboBox(p,wxID_ANY,wxEmptyString,wxDefaultPosition,FromDIP(wxSize(320,-1)),0,nullptr,wxCB_READONLY); m_fit_cache->SetName(OllamaText::ui(OllamaText::Ui::CachePrecision));
        for(std::size_t i=0;i<std::size(fit_caches);++i) { m_fit_cache->Append(OllamaText::kv_cache(fit_caches[i])); if(fit_caches[i]==settings.cache) m_fit_cache->SetSelection(static_cast<int>(i)); }
        row->Add(m_fit_cache,0,wxALL,FromDIP(4)); s->Add(row,0,wxEXPAND);
        m_fit_context->Bind(wxEVT_COMBOBOX,[this](wxCommandEvent &) { change_fit_settings(); });
        m_fit_cache->Bind(wxEVT_COMBOBOX,[this](wxCommandEvent &) { change_fit_settings(); });
        note(p,s,OllamaText::fit_settings_help());
    }
    static constexpr KvCache fit_caches[]={KvCache::F16,KvCache::Q8_0,KvCache::Q4_0};
    void change_fit_settings() {
        const auto &contexts=fit_contexts(); const int context=m_fit_context->GetSelection(), cache=m_fit_cache->GetSelection();
        if(context<0||static_cast<std::size_t>(context)>=contexts.size()||cache<0||static_cast<std::size_t>(cache)>=std::size(fit_caches)) return;
        FitSettings settings; settings.context=contexts[static_cast<std::size_t>(context)]; settings.cache=fit_caches[static_cast<std::size_t>(cache)];
        { std::lock_guard<std::mutex> lock(m_state.mutex); m_state.fit_settings=settings; }
        try { atomic_json(m_root/"fit-settings.json",fit_settings_json(settings)); } catch(...) { m_status->SetLabel(OllamaText::ui(OllamaText::Ui::SettingsNotSaved)); }
        recompute_fit();
    }
    std::optional<BackendObservation> saved_backend() const {
        try { if(std::filesystem::exists(m_root/"backend.json")) return load_backend(read_json(m_root/"backend.json",64*1024)); } catch(...) {}
        return std::nullopt;
    }
    // Measures now; called on the worker thread without holding the state lock.
    Hardware measure_hardware() {
        std::vector<Model> installed; std::string version; bool known=false;
        { std::lock_guard<std::mutex> lock(m_state.mutex); installed=m_state.installed; version=m_state.runtime_version; known=m_state.inventory_known; }
        auto h=detect_hardware(installed); h.inventory_known=known; apply_backend(h,saved_backend(),version); return h;
    }
    void recompute_fit() { start([this] {
        auto h=measure_hardware(); std::lock_guard<std::mutex> lock(m_state.mutex); m_state.hardware=h; m_state.hardware_ready=true;
        if(m_state.selected) { m_state.fit=assess(*m_state.selected,h,m_state.fit_settings); m_state.details_ready=true; } else m_state.measured_notice=true;
    }); }
    void refresh_runtime() { start([this] {
        LocalClient c; auto health=c.execute(Operation::Version,Json::object(),m_state.cancel);
        if(health.state!=RuntimeState::Healthy) { set_status(health.diagnostic); return; }
        const auto version=health.value.at("version").get<std::string>();
        auto tags=c.execute(Operation::Installed,Json::object(),m_state.cancel); if(tags.state!=RuntimeState::Healthy) { set_status(tags.diagnostic); return; }
        auto models=installed_models(tags.value);
        // Verified /api/show metadata for each installed model lets the store filter by capability and fit
        // without guessing. A model whose metadata cannot be read stays unverified.
        for(std::size_t i=0;i<models.size()&&i<256&&!m_state.cancel.load();++i) {
            auto shown=c.execute(Operation::Show,{{"model",models[i].name}},m_state.cancel);
            if(shown.state==RuntimeState::Healthy) { try { apply_details(models[i],shown.value); } catch(...) { models[i].capabilities_verified=false; } }
        }
        auto running=c.execute(Operation::Running,Json::object(),m_state.cancel); std::set<std::string> names;
        std::vector<RuntimeMemory> loaded;
        if(running.state==RuntimeState::Healthy) { for(const auto &m:installed_models(running.value)) names.insert(m.name); loaded=runtime_memory(running.value); }
        { std::lock_guard<std::mutex> lock(m_state.mutex); m_state.installed=models; m_state.running=std::move(names); m_state.runtime_version=version; m_state.inventory_known=true; m_state.selected.reset(); m_state.fit.reset(); }
        // Measure after the inventory is current: its manifests prove the model folder. A loaded model is
        // the runtime's own evidence of which backend it uses, so it replaces any earlier observation.
        auto h=detect_hardware(models); h.inventory_known=true;
        auto seen=observe_backend(loaded,version,adapter_identity(h.gpus),h.measured_at);
        if(seen) { try { atomic_json(m_root/"backend.json",backend_json(*seen)); } catch(...) {} } else seen=saved_backend();
        apply_backend(h,seen,version);
        std::lock_guard<std::mutex> lock(m_state.mutex); m_state.hardware=std::move(h); m_state.hardware_ready=true; m_state.status="Local Ollama responded. Installed model inventory refreshed.";
    }); }
    // The catalog's age keeps moving while the dialog is open.
    void show_catalog_age() { std::lock_guard<std::mutex> lock(m_state.mutex); m_catalog_status->SetLabel(OllamaText::catalog_status(m_state.catalog,m_state.catalog_attempt,now_seconds())); }
    void render_models() {
        if(!m_models||!m_filter_state) return; // a search callback can arrive before the store is built
        const auto previous=selected_model(); const std::string selected=previous ? previous->name : std::string();
        // Every entry gets its verdict from the current hardware and estimate settings, so fit filters, groups
        // and sorting follow each re-measurement and settings change.
        std::vector<StoreEntry> entries;
        { std::lock_guard<std::mutex> lock(m_state.mutex); auto all=reconcile(m_state.catalog.models,m_state.installed,m_state.running); entries.reserve(all.size());
            for(auto &m:all) { const auto verdict=assess(m,m_state.hardware,m_state.fit_settings).verdict; entries.push_back({std::move(m),verdict}); }
            m_catalog_status->SetLabel(OllamaText::catalog_status(m_state.catalog,m_state.catalog_attempt,now_seconds())); }
        update_store_options(entries);
        const auto query=current_query();
        SearchField::MatchPass match(m_model_search->GetValue(),m_model_search->IsRegexEnabled(),m_model_search->IsCaseSensitive(),m_model_search->IsWholeWord(),m_model_search->IsMultiline());
        m_rows=query_store(entries,query,[&](const Model &m) { std::string hay=m.name+" "+m.family+" "+m.quantization; for(const auto &c:m.capabilities) hay+=" "+c; return match.matches(u8(hay)); });
        m_store=std::move(entries);
        if(m_model_offset>=m_rows.size()) m_model_offset=0;
        std::vector<wxString> labels;
        for(std::size_t r=m_model_offset;r<m_rows.size()&&labels.size()<100;++r) {
            const auto &row=m_rows[r]; const auto &entry=m_store[row.entry];
            labels.push_back(row.header ? OllamaText::group_heading(query.group,entry,row.members) : OllamaText::store_row(entry)); }
        m_models->Set(labels);
        for(std::size_t i=0;i<labels.size();++i) { const auto &row=m_rows[m_model_offset+i]; if(!row.header&&m_store[row.entry].model.name==selected) m_models->SetSelection(static_cast<int>(i)); }
        m_query_summary->SetLabel(OllamaText::query_summary(query,store_matches(m_rows),m_store.size()));
    }
    void inspect_selected() {
        const auto *row=selected_row(); if(!row) return;
        // Explain before anything is chosen: a heading explains its group, a model its family, variant,
        // state, capabilities, quantization, size, storage and current fit, before inspection adds evidence.
        if(row->header) { m_details->SetValue(OllamaText::group_explanation(current_query().group,m_store[row->entry],row->members)); return; }
        auto m=m_store[row->entry].model; m_details->SetValue(OllamaText::model_explanation(m,m_store[row->entry].fit));
        if(!m.installed) {
            // Registry metadata gives the exact download size; runtime metadata stays unknown until installation.
            start([this,m=std::move(m)]() mutable { LocalClient client; m=client.registry_metadata(m.name,m_state.cancel);
                auto h=measure_hardware(); std::lock_guard<std::mutex> lock(m_state.mutex);
                m_state.selected=m; m_state.hardware=h; m_state.hardware_ready=true; m_state.fit=assess(m,h,m_state.fit_settings); m_state.details_ready=true;
                for(auto &entry:m_state.catalog.models) if(entry.name==m.name) entry=m;
            }); return;
        }
        start([this,m=std::move(m)]() mutable {
            LocalClient c; auto details=c.execute(Operation::Show,{{"model",m.name}},m_state.cancel);
            if(details.state!=RuntimeState::Healthy) { set_status(details.diagnostic); return; } apply_details(m,details.value);
            auto h=measure_hardware(); std::lock_guard<std::mutex> lock(m_state.mutex);
            m_state.selected=m; m_state.hardware=h; m_state.hardware_ready=true; m_state.fit=assess(m,h,m_state.fit_settings); m_state.details_ready=true;
        });
    }
    void build_chat() {
        auto [p,s]=section("chat",_L("Chat")); note(p,s,_L("Choose an installed local completion model in Models. Messages remain on this computer. Ordinary exports omit message content and attachments."));
        auto *parameters=new wxWrapSizer(wxHORIZONTAL);
        m_temperature=new TextInput(p,"0.7",_L("Temperature, 0 to 2")); m_temperature->SetValCheckers({TextInputValChecker::CreateDoubleRangeChecker(0,2,false)});parameters->Add(m_temperature,0,wxALL,FromDIP(4));
        m_context=new TextInput(p,"2048",_L("Context, 512 to 32768"));m_context->SetValCheckers({TextInputValChecker::CreateIntRangeChecker(512,32768)});parameters->Add(m_context,0,wxALL,FromDIP(4));
        m_predict=new TextInput(p,"512",_L("Output limit, 1 to 4096"));m_predict->SetValCheckers({TextInputValChecker::CreateIntRangeChecker(1,4096)});parameters->Add(m_predict,0,wxALL,FromDIP(4));s->Add(parameters,0,wxEXPAND);
        auto *presets=new wxWrapSizer(wxHORIZONTAL);
        action(p,presets,_L("Recommended balanced settings"),[this]{m_temperature->GetTextCtrl()->SetValue("0.7");m_context->GetTextCtrl()->SetValue("2048");m_predict->GetTextCtrl()->SetValue("512");});
        action(p,presets,_L("Conservative short reply"),[this]{m_temperature->GetTextCtrl()->SetValue("0.2");m_context->GetTextCtrl()->SetValue("512");m_predict->GetTextCtrl()->SetValue("128");});s->Add(presets,0,wxEXPAND);
        note(p,s,_L("System prompt")); m_system=new TextArea(p,wxString(),FromDIP(wxSize(-1,80))); m_system->GetTextCtrl()->SetMaxLength(8192); s->Add(m_system,0,wxEXPAND|wxALL,FromDIP(8));
        m_output=new TextArea(p,wxString(),FromDIP(wxSize(-1,210))); m_output->SetReadOnly(true); m_output->SetName("Local response"); s->Add(m_output,0,wxEXPAND|wxALL,FromDIP(8));
        m_prompt=new TextArea(p,wxString(),FromDIP(wxSize(-1,100))); m_prompt->GetTextCtrl()->SetMaxLength(32768); m_prompt->SetName("Message for selected local model"); s->Add(m_prompt,0,wxEXPAND|wxALL,FromDIP(8));
        auto *buttons=new wxWrapSizer(wxHORIZONTAL); action(p,buttons,_L("Send locally"),[this] { send(); });
        m_attach=action(p,buttons,_L("Attach image"),[this] { attach_image(); }); m_attach->Enable(false); m_attach->SetToolTip(_L("Select a PNG or JPEG only after verifying vision capability in Models. Image bytes are sent locally and are omitted from saved history."));
        s->Add(buttons,0,wxEXPAND); note(p,s,_L("Default parameters: temperature 0.7, context 2048, output limit 512. These bounds are checked against the selected model. Stop cancels the active HTTP transfer."));
        note(p,s,_L("Saved sessions"));
        m_history_search=new SearchField(p,_L("Search saved sessions on this page")); s->Add(m_history_search,0,wxEXPAND|wxALL,FromDIP(8));
        m_history_search->SetOnQuery([this](const wxString &){render_history();}); m_history_search->SetOnRegexToggle([this](bool){render_history();});
        m_sessions=new ListBox(p,wxID_ANY,FromDIP(wxSize(-1,140))); s->Add(m_sessions,0,wxEXPAND|wxALL,FromDIP(8));
        m_sessions->Bind(wxEVT_LISTBOX,[this](wxCommandEvent &) { auto i=m_sessions->GetSelection(); if(i<0||static_cast<std::size_t>(i)>=m_history_rows.size()) return;
            std::lock_guard<std::mutex> lock(m_state.mutex); if(m_state.busy) return; m_session=m_history_rows[i]; m_title->GetTextCtrl()->SetValue(u8(m_session.title)); std::string visible;
            for(const auto &m:m_session.messages) visible+=m.value("role",std::string())+": "+m.value("content",std::string())+"\n\n"; m_state.response=visible; m_output->SetValue(u8(visible)); });
        m_title=new TextInput(p,wxString()); m_title->GetTextCtrl()->SetMaxLength(200); m_title->SetName("Session display name"); s->Add(m_title,0,wxEXPAND|wxALL,FromDIP(8));
        auto *history_actions=new wxWrapSizer(wxHORIZONTAL);
        action(p,history_actions,_L("Rename session"),[this] { try { std::lock_guard<std::mutex> lock(m_state.mutex); if(m_state.busy||m_session.id.empty()) return; m_session.title=utf8(m_title->GetTextCtrl()->GetValue()); m_history.save(m_session); } catch(...) { m_status->SetLabel(_L("Session rename could not be saved.")); } render_history(); });
        action(p,history_actions,_L("Delete session"),[this] { { std::lock_guard<std::mutex> lock(m_state.mutex); if(m_state.busy||m_session.id.empty()) return; }
            SuperConfirmGate::Spec spec; spec.action=_L("Delete session"); spec.consequence=_L("The selected local chat history will be permanently deleted. This cannot be undone."); spec.affected={u8(m_session.title)};
            if(SuperConfirmGate::Run(m_sessions,spec)) { try { m_history.remove(m_session.id,true); m_session={}; m_output->Clear(); render_history(); } catch(...) { m_status->SetLabel(_L("Session deletion failed. Existing data was retained.")); } } });
        action(p,history_actions,_L("Export redacted session"),[this] { if(m_session.id.empty()) return; wxFileDialog file(this,_L("Export redacted session"),wxString(),"session.json","JSON (*.json)|*.json",wxFD_SAVE|wxFD_OVERWRITE_PROMPT);
            if(file.ShowModal()!=wxID_OK) return; try { const std::filesystem::path destination(file.GetPath().ToStdWstring());
                if(std::filesystem::exists(destination)) { SuperConfirmGate::Spec spec; spec.action=_L("Replace export file"); spec.consequence=_L("The selected file will be replaced permanently."); spec.affected={file.GetFilename()}; if(!SuperConfirmGate::Run(m_sessions,spec)) return; }
                atomic_json(destination,m_history.redacted_export(m_session)); m_status->SetLabel(_L("Redacted export saved. Message text, titles and attachments were omitted.")); } catch(...) { m_status->SetLabel(_L("Export could not be saved.")); } });
        action(p,history_actions,_L("Previous sessions"),[this] { if(m_history_offset>=50) m_history_offset-=50; render_history(); });
        action(p,history_actions,_L("Next sessions"),[this] { m_history_offset+=50; render_history(); }); s->Add(history_actions,0,wxEXPAND); render_history();
    }
    void attach_image() {
        { std::lock_guard<std::mutex> lock(m_state.mutex); if(m_state.busy||!m_state.selected||!m_state.selected->capabilities_verified||!m_state.selected->capabilities.count("vision")) { m_status->SetLabel(_L("Image attachments need a selected model with verified vision capability. Open Models and inspect a vision model.")); return; } }
        wxFileDialog file(this,_L("Choose a local PNG or JPEG"),wxString(),wxString(),"PNG and JPEG (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg",wxFD_OPEN|wxFD_FILE_MUST_EXIST);
        if(file.ShowModal()!=wxID_OK) return;
        try { const std::filesystem::path path(file.GetPath().ToStdWstring()); const auto size=std::filesystem::file_size(path); if(size<8||size>768*1024) throw std::runtime_error("image bound");
            std::ifstream in(path,std::ios::binary); std::string bytes(static_cast<std::size_t>(size),'\0'); in.read(bytes.data(),bytes.size()); if(!in||in.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("image changed");
            const bool png=bytes.compare(0,8,std::string("\x89PNG\r\n\x1a\n",8))==0; const bool jpeg=static_cast<unsigned char>(bytes[0])==0xff&&static_cast<unsigned char>(bytes[1])==0xd8&&static_cast<unsigned char>(bytes[2])==0xff;
            if(!png&&!jpeg) throw std::runtime_error("image signature"); m_attachment=utf8(wxBase64Encode(bytes.data(),bytes.size())); m_status->SetLabel(_L("One image is attached for the next message only. Its filename is not sent or saved."));
        } catch(...) { m_attachment.clear(); m_status->SetLabel(_L("Attachment rejected: select an unchanged PNG or JPEG no larger than 768 KiB.")); }
    }
    void render_history() { if(!m_sessions) return; try {
        auto records=m_history.page(m_history_offset,50); std::vector<wxString> rows; m_history_rows.clear();
        SearchField::MatchPass match(m_history_search->GetValue(),m_history_search->IsRegexEnabled(),m_history_search->IsCaseSensitive(),m_history_search->IsWholeWord(),m_history_search->IsMultiline());
        for(auto &record:records) if(match.matches(u8(record.title+" "+record.model))) { rows.push_back(u8(record.title+" | "+record.model)); m_history_rows.push_back(std::move(record)); } m_sessions->Set(rows);
    } catch(...) { m_status->SetLabel(_L("Saved sessions could not be read. Existing files were retained.")); } }
    void send() {
        Model m; { std::lock_guard<std::mutex> lock(m_state.mutex); if(m_state.busy||!m_state.selected||m_state.selected->name!=m_session.model) { m_status->SetLabel(_L("Finish the current operation, then select and inspect a model and choose Use selected model for chat.")); return; } m=*m_state.selected; m_state.response.clear(); }
        const auto prompt=utf8(m_prompt->GetValue()); if(prompt.empty()) return;
        auto session=m_session; if(session.messages.empty()&&!m_system->GetValue().empty()) session.messages.push_back({{"role","system"},{"content",utf8(m_system->GetValue())}});
        session.messages.push_back({{"role","user"},{"content",prompt}});
        if(!m_attachment.empty()) session.messages.back()["images"]=Json::array({m_attachment});
        Json payload; try { double temperature=0;unsigned long context=0,predict=0;
            if(!m_temperature->GetTextCtrl()->GetValue().ToDouble(&temperature)||!m_context->GetTextCtrl()->GetValue().ToULong(&context)||!m_predict->GetTextCtrl()->GetValue().ToULong(&predict))throw std::runtime_error("invalid parameter");
            payload=chat_payload(m,session.messages,temperature,context,predict); } catch(...) { m_status->SetLabel(_L("Chat is unavailable: verify completion capability, context metadata, and message limits in Models.")); return; }
        m_session=session; m_prompt->Clear(); m_attachment.clear(); start([this,payload,session]() mutable {
            std::string answer; LocalClient c; auto result=c.execute(Operation::Chat,payload,m_state.cancel,[&](const Json &chunk) {
                if(chunk.contains("message")&&chunk.at("message").contains("content")) {
                    auto part=chunk.at("message").at("content").get<std::string>(); if(part.size()>max_chat_bytes-answer.size()) return false; answer+=part;
                    std::lock_guard<std::mutex> lock(m_state.mutex); m_state.response=answer; m_state.status="Receiving a local response."; m_state.changed=true;
                } return true;
            });
            for(auto &message:session.messages) message.erase("images");
            if(result.state==RuntimeState::Healthy) { session.messages.push_back({{"role","assistant"},{"content",answer}}); m_history.save(session); }
            std::lock_guard<std::mutex> lock(m_state.mutex); m_state.messages=session.messages; m_state.status=result.diagnostic;
        });
    }
    void build_queue() {
        auto [p,s]=section("pulls",_L("Batch pulls")); note(p,s,_L("A batch schedules model downloads only. Each exact tag is persisted independently; interrupted transfers require installed-state reconciliation before retry."));
        m_pulls=new ListBox(p,wxID_ANY,FromDIP(wxSize(-1,260))); s->Add(m_pulls,0,wxEXPAND|wxALL,FromDIP(8));
        auto *buttons=new wxWrapSizer(wxHORIZONTAL); action(p,buttons,_L("Previous items"),[this] { if(m_queue_offset>=100) m_queue_offset-=100; render_queue(); }); action(p,buttons,_L("Next items"),[this] { m_queue_offset+=100; render_queue(); });
        auto *pull=action(p,buttons,_L("Review and start batch"),[]{}); pull->Enable(false); pull->SetToolTip(_L("Transfer is unavailable until every selected tag has exact download metadata, destination disk preflight, and a reviewed transfer disclosure.")); s->Add(buttons,0,wxEXPAND);
        note(p,s,_L("Storage preflight is incomplete for uninstalled catalog entries. No model download will start from this surface until that evidence is available."));
    }
    void render_queue() { try { std::vector<wxString> rows; for(const auto &i:m_queue.page(m_queue_offset)) rows.push_back(u8(i.model)+" | "+u8(i.message.empty()?"queued":i.message)); m_pulls->Set(rows); } catch(...) { m_status->SetLabel(_L("Batch state could not be read. Existing files were retained.")); } }
    void build_profiles() {
        auto [p,s]=section("profiles",_L("Launch profiles")); note(p,s,_L("Launch profiles belong to this application. Ollama does not launch other programs. Profiles cannot accept shell commands, scripts, or arbitrary environment values."));
        NativeLaunchPolicy policy;
        policy.installed_model=[this](const std::string &tag) { LocalClient client; auto result=client.execute(Operation::Installed,Json::object(),m_state.cancel); if(result.state!=RuntimeState::Healthy)return false; auto installed=installed_models(result.value); return std::any_of(installed.begin(),installed.end(),[&](const Model &model){return model.name==tag;}); };
        policy.hardware_fit=[this](const LaunchProfile &profile) { std::optional<Model> model; FitSettings settings;
            { std::lock_guard<std::mutex> lock(m_state.mutex); if(!m_state.selected||m_state.selected->name!=profile.model_tag)return false; model=m_state.selected; settings=m_state.fit_settings; }
            settings.context=profile.context_length; // the profile's own context sizes its cache
            auto verdict=assess(*model,measure_hardware(),settings); return verdict.verdict==Fit::RunsWell||verdict.verdict==Fit::WithLimits; };
        m_launcher=std::make_unique<NativeLaunchAdapter>(std::move(policy)); m_profiles=LaunchProfileRegistry::prebuilt();
        if(!m_launcher->available()) note(p,s,u8(m_launcher->unavailable_reason()));
        for(std::size_t i=0;i<m_profiles.size();++i) {
            note(p,s,u8(m_profiles[i].name)); if(!NativeLaunchDetail::supported_kind(m_profiles[i].kind))note(p,s,u8(NativeLaunchDetail::profile_unavailable_reason(m_profiles[i].kind)));auto *row=new wxWrapSizer(wxHORIZONTAL);
            auto *executable=action(p,row,_L("Choose executable"),[this,i] { {std::lock_guard<std::mutex> lock(m_state.mutex);if(m_state.busy)return;m_state.launch_plan.reset();}
                auto result=m_launcher->pick_executable(m_profiles[i].kind,GetHandle()); if(result.evidence)m_profiles[i].executable=result.evidence; if(!result.cancelled)m_status->SetLabel(u8(result.diagnostic)); });
            executable->Enable(m_launcher->available()&&NativeLaunchDetail::supported_kind(m_profiles[i].kind));executable->SetToolTip(u8(NativeLaunchDetail::profile_unavailable_reason(m_profiles[i].kind)));
            action(p,row,_L("Choose working folder"),[this,i] { {std::lock_guard<std::mutex> lock(m_state.mutex);if(m_state.busy)return;m_state.launch_plan.reset();}
                auto result=m_launcher->pick_working_directory(GetHandle()); if(result.path)m_profiles[i].working_directory=*result.path; if(!result.cancelled)m_status->SetLabel(u8(result.diagnostic)); });
            if(m_profiles[i].kind==LaunchKind::LlamaServer) action(p,row,_L("Choose local model file"),[this,i] { {std::lock_guard<std::mutex> lock(m_state.mutex);if(m_state.busy)return;m_state.launch_plan.reset();}
                auto result=m_launcher->pick_model_file(GetHandle()); if(result.evidence)m_profiles[i].model_file=result.evidence; if(!result.cancelled)m_status->SetLabel(u8(result.diagnostic)); });
            action(p,row,_L("Review preflight"),[this,i] { {std::lock_guard<std::mutex> lock(m_state.mutex);if(m_state.busy)return;if(m_state.selected)m_profiles[i].model_tag=m_state.selected->name;m_state.launch_plan.reset();}
                auto profile=m_profiles[i]; start([this,profile] { auto result=LaunchProfileRegistry::prepare(profile,*m_launcher); std::string report;
                    if(result.plan) { const auto &preview=result.plan->preview(); report="Launch preflight passed. Review exact local execution:\n"+preview.executable+"\nWorking folder: "+preview.working_directory+"\nArguments:"; for(const auto &arg:preview.arguments)report+="\n"+arg; report+="\nEnvironment keys:";for(const auto &[key,value]:preview.environment)report+=" "+key; }
                    else {report="Launch blocked:\n";for(const auto &blocker:result.blockers)report+=blocker+"\n";}
                    std::lock_guard<std::mutex> lock(m_state.mutex);m_state.launch_plan=std::move(result.plan);m_state.profile_preview=report;m_state.status=std::move(report);
                }); }); s->Add(row,0,wxEXPAND);
        }
        m_profile_preview=new TextArea(p,_L("Choose the registered inputs, then review preflight. Exact launch details appear here before launch."),FromDIP(wxSize(-1,190)));m_profile_preview->SetReadOnly(true);s->Add(m_profile_preview,0,wxEXPAND|wxALL,FromDIP(8));
        m_launch=action(p,s,_L("Launch reviewed profile"),[this] { std::optional<ValidatedLaunchPlan> plan; {std::lock_guard<std::mutex> lock(m_state.mutex);if(m_state.busy||!m_state.launch_plan)return;plan=m_state.launch_plan;m_state.launch_plan.reset();}
            start([this,plan] {auto outcome=LaunchTransaction::run(*plan,*m_launcher,*m_launcher,30000);set_status(m_state.cancel.load()?"Launch cancelled. "+outcome.diagnostic:outcome.diagnostic);}); }); m_launch->Enable(false);
        m_launch->SetToolTip(_L("Complete executable, folder, model and hardware preflight first. Unknown hardware evidence prevents launch."));
        action(p,s,_L("Stop owned process and restore snapshot"),[this] {start([this] {set_status(m_launcher->recover_interrupted()?"Owned process stopped and saved profile state restored.":"No restorable snapshot was available, or recovery failed. Existing state was retained.");});});
        note(p,s,_L("Registration requires an exact approved executable identity. Launch uses an owned process job and explicit arguments, and snapshots before mutation. Unknown hardware evidence blocks launch. Runtime evidence remains pending for this packaged adapter."));
    }
    void build_help() {
        auto [p,s]=section("help",_L("Troubleshooting"));
        note(p,s,_L("No service response: Ollama may be absent or stopped. Those states cannot be distinguished from a refused loopback connection. This build does not contain a verified Ollama installer or service-start adapter, so installation and startup are unavailable here."));
        note(p,s,_L("If Ollama is already installed, open its own desktop application, then use Refresh runtime. The suite never runs an arbitrary shell command, searches PATH, or downloads an unverified installer."));
        note(p,s,_L("Unhealthy API: refresh runtime and inspect the selected model again. An interrupted stream is incomplete. Storage shortage: free space in the configured model destination before reviewing the batch again."));
        note(p,s,_L("Catalog offline: the last verified snapshot and installed models remain available. The official HTML catalog currently supplies no verified total-count contract, so discovered entries are never labelled exhaustive."));
        note(p,s,_L("Unknown fit: usable GPU memory, driver/backend support, or context-memory evidence is missing. A model name is not hardware evidence. No execution guarantee is made."));
        action(p,s,_L("Retry runtime and return to Models"),[this] { m_tabs->Activate("models"); refresh_runtime(); });
    }
};
}
void show_ollama_suite(wxWindow *parent,const std::filesystem::path &state_root) { SuiteDialog dialog(parent,state_root); dialog.ShowModal(); }
}
