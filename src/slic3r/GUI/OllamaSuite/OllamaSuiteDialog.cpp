#include "OllamaSuiteDialog.hpp"
#include "OllamaClient.hpp"
#include "libslic3r/OllamaSuite/LaunchProfiles.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/ListBox.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/TextArea.hpp"
#include "../Widgets/TabStrip.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include <wx/dialog.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/timer.h>
#include <wx/wrapsizer.h>
#include <mutex>
#include <thread>

namespace Slic3r::GUI {
namespace {
using namespace Slic3r::OllamaSuite;
wxString u8(const std::string &s) { return wxString::FromUTF8(s.c_str()); }
std::string utf8(const wxString &s) { auto bytes=s.ToUTF8(); return bytes ? std::string(bytes.data(),bytes.length()) : std::string(); }
class SuiteDialog final : public wxDialog {
    struct WorkerState {
        std::mutex mutex; std::atomic_bool cancel{false}; bool busy=false, changed=false;
        std::string status; std::vector<Model> installed; std::set<std::string> running;
        CatalogSnapshot catalog; std::optional<Model> selected; std::string response;
        Json messages=Json::array();
    };
public:
    SuiteDialog(wxWindow *parent,const std::filesystem::path &root)
        :wxDialog(parent,wxID_ANY,_L("Local Ollama suite"),wxDefaultPosition,wxDefaultSize,wxRESIZE_BORDER|wxBORDER_NONE),
         m_root(root/"ollama-suite"),m_queue(m_root/"pulls"),m_history(m_root/"chats"),m_timer(this) {
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
        stop->Bind(wxEVT_BUTTON,[this](wxCommandEvent &) { m_state.cancel=true; }); outer->Add(stop,0,wxALL|wxALIGN_RIGHT,FromDIP(12));
        SetSizer(outer); MD3DialogCaption::FinishChrome(this); CentreOnParent();
        Bind(wxEVT_TIMER,[this](wxTimerEvent &) { poll(); }); m_timer.Start(120);
        try { m_queue.recover(); if(std::filesystem::exists(m_root/"catalog.json")) m_state.catalog=load_catalog(read_json(m_root/"catalog.json")); }
        catch(...) { m_status->SetLabel(_L("Saved suite data is invalid or unavailable. Existing files were retained.")); }
        render_models(); render_queue();
    }
    ~SuiteDialog() override { m_timer.Stop(); m_state.cancel=true; if(m_worker.joinable()) m_worker.join(); }
private:
    std::filesystem::path m_root; PullQueue m_queue; ChatStore m_history; WorkerState m_state;
    std::thread m_worker; wxTimer m_timer; Label *m_status=nullptr,*m_catalog_status=nullptr;
    TabStrip *m_tabs=nullptr; wxSimplebook *m_book=nullptr; wxBoxSizer *m_body=nullptr;
    std::vector<std::string> m_sections; SearchField *m_model_search=nullptr;
    ListBox *m_models=nullptr,*m_pulls=nullptr; TextArea *m_details=nullptr,*m_prompt=nullptr,*m_system=nullptr,*m_output=nullptr;
    std::vector<Model> m_visible; std::size_t m_model_offset=0,m_queue_offset=0; ChatSession m_session;
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
            m_status->SetLabel(u8(m_state.status)); m_output->SetValue(u8(m_state.response)); done=!m_state.busy;
            if(done&&!m_state.messages.empty()) { m_session.messages=m_state.messages; m_state.messages=Json::array(); }
            if(done&&m_state.selected) m_details->SetValue(u8(m_state.status)); }
        if(done) { render_models(); render_queue(); } Layout();
    }
    void build_store() {
        auto [p,s]=section("models",_L("Models")); auto *buttons=new wxWrapSizer(wxHORIZONTAL);
        action(p,buttons,_L("Refresh runtime"),[this] { refresh_runtime(); });
        action(p,buttons,_L("Refresh official catalog"),[this] { start([this] {
            LocalClient c; auto snapshot=refresh_catalog([&](const std::string &path) { return c.catalog_page(path,m_state.cancel); },m_state.cancel);
            if(snapshot.complete) atomic_json(m_root/"catalog.json",catalog_json(snapshot));
            std::lock_guard<std::mutex> lock(m_state.mutex); m_state.status=snapshot.reason;
            if(snapshot.complete || m_state.catalog.models.empty()) m_state.catalog=std::move(snapshot); else m_state.catalog.offline=true;
        }); }); s->Add(buttons,0,wxEXPAND);
        m_catalog_status=new Label(p,_L("Catalog has not been verified."),LB_AUTO_WRAP); s->Add(m_catalog_status,0,wxEXPAND|wxALL,FromDIP(8));
        m_model_search=new SearchField(p,_L("Search exact model, family, capability or quantization")); s->Add(m_model_search,0,wxEXPAND|wxALL,FromDIP(8));
        m_model_search->SetOnQuery([this](const wxString &) { m_model_offset=0; render_models(); });
        m_model_search->SetOnRegexToggle([this](bool) { m_model_offset=0; render_models(); });
        m_models=new ListBox(p,wxID_ANY,FromDIP(wxSize(-1,220))); m_models->SetName("Ollama model inventory"); s->Add(m_models,0,wxEXPAND|wxALL,FromDIP(8));
        m_models->Bind(wxEVT_LISTBOX,[this](wxCommandEvent &) { inspect_selected(); });
        auto *pages=new wxWrapSizer(wxHORIZONTAL);
        action(p,pages,_L("Previous models"),[this] { if(m_model_offset>=100) m_model_offset-=100; render_models(); });
        action(p,pages,_L("Next models"),[this] { if(m_visible.size()==100) m_model_offset+=100; render_models(); }); s->Add(pages,0,wxEXPAND);
        m_details=new TextArea(p,_L("Select an installed model to inspect verified capabilities and hardware evidence."),FromDIP(wxSize(-1,150))); m_details->SetReadOnly(true); s->Add(m_details,0,wxEXPAND|wxALL,FromDIP(8));
        auto *choose=new wxWrapSizer(wxHORIZONTAL);
        action(p,choose,_L("Use selected model for chat"),[this] { std::lock_guard<std::mutex> lock(m_state.mutex); if(m_state.busy||!m_state.selected||!m_state.selected->capabilities_verified||!m_state.selected->local) { m_status->SetLabel(_L("Finish the current operation and inspect a verified installed local model first.")); return; } m_session={unique_id(),"Local session",m_state.selected->name,Json::array()}; m_state.messages=Json::array(); m_state.response.clear(); m_output->Clear(); m_tabs->Activate("chat"); });
        action(p,choose,_L("Add selected tag to batch"),[this] { auto i=m_models->GetSelection(); if(i<0||static_cast<std::size_t>(i)>=m_visible.size()) return; try { m_queue.add(m_visible[i].name); render_queue(); m_tabs->Activate("pulls"); } catch(...) { m_status->SetLabel(_L("Could not persist the batch item.")); } }); s->Add(choose,0,wxEXPAND);
    }
    void refresh_runtime() { start([this] {
        LocalClient c; auto health=c.execute(Operation::Version,Json::object(),m_state.cancel);
        if(health.state!=RuntimeState::Healthy) { set_status(health.diagnostic); return; }
        auto tags=c.execute(Operation::Installed,Json::object(),m_state.cancel); if(tags.state!=RuntimeState::Healthy) { set_status(tags.diagnostic); return; }
        auto models=installed_models(tags.value); auto running=c.execute(Operation::Running,Json::object(),m_state.cancel); std::set<std::string> names;
        if(running.state==RuntimeState::Healthy) for(const auto &m:installed_models(running.value)) names.insert(m.name);
        std::lock_guard<std::mutex> lock(m_state.mutex); m_state.installed=std::move(models); m_state.running=std::move(names); m_state.selected.reset(); m_state.status="Local Ollama responded. Installed model inventory refreshed.";
    }); }
    void render_models() {
        std::vector<Model> all; { std::lock_guard<std::mutex> lock(m_state.mutex); all=reconcile(m_state.catalog.models,m_state.installed,m_state.running);
            m_catalog_status->SetLabel(u8(m_state.catalog.reason.empty()?"No verified catalog is cached. Installed models remain available.":m_state.catalog.reason)+"\n"+u8(m_state.catalog.refreshed_at)); }
        SearchField::MatchPass match(m_model_search->GetValue(),m_model_search->IsRegexEnabled(),m_model_search->IsCaseSensitive(),m_model_search->IsWholeWord(),m_model_search->IsMultiline());
        std::vector<wxString> rows; m_visible.clear(); std::size_t offset=0;
        for(const auto &m:all) { std::string hay=m.name+" "+m.family+" "+m.quantization; for(const auto &c:m.capabilities) hay+=" "+c;
            if(!match.matches(u8(hay))) continue; if(offset++<m_model_offset) continue; if(rows.size()==100) break;
            rows.push_back(u8(m.name)+(m.installed?_L(" | installed"):_L(" | catalog, metadata unverified"))+(m.running?_L(" | running"):wxString())); m_visible.push_back(m); }
        m_models->Set(rows);
    }
    void inspect_selected() {
        auto i=m_models->GetSelection(); if(i<0||static_cast<std::size_t>(i)>=m_visible.size()) return; auto m=m_visible[i];
        if(!m.installed) { m_details->SetValue(_L("This exact catalog tag is not installed. Size and capability metadata are unverified; hardware fit is Unknown. Batch transfer remains unavailable until the full storage preflight can be proved.")); return; }
        start([this,m=std::move(m)]() mutable {
            LocalClient c; auto details=c.execute(Operation::Show,{{"model",m.name}},m_state.cancel);
            if(details.state!=RuntimeState::Healthy) { set_status(details.diagnostic); return; } apply_details(m,details.value);
            const auto h=detect_hardware(m_root); auto verdict=fit(m,h,2048,{});
            std::string summary=m.name+"\n"+fit_label(verdict.verdict)+"\n"; for(const auto &e:verdict.evidence) summary+=e+"\n";
            summary+="Capabilities:"; for(const auto &c:m.capabilities) summary+=" "+c;
            std::lock_guard<std::mutex> lock(m_state.mutex); m_state.selected=m; m_state.status=summary;
        });
    }
    void build_chat() {
        auto [p,s]=section("chat",_L("Chat")); note(p,s,_L("Choose an installed local completion model in Models. Messages remain on this computer. Ordinary exports omit message content and attachments."));
        note(p,s,_L("System prompt")); m_system=new TextArea(p,wxString(),FromDIP(wxSize(-1,80))); m_system->GetTextCtrl()->SetMaxLength(8192); s->Add(m_system,0,wxEXPAND|wxALL,FromDIP(8));
        m_output=new TextArea(p,wxString(),FromDIP(wxSize(-1,210))); m_output->SetReadOnly(true); m_output->SetName("Local response"); s->Add(m_output,0,wxEXPAND|wxALL,FromDIP(8));
        m_prompt=new TextArea(p,wxString(),FromDIP(wxSize(-1,100))); m_prompt->GetTextCtrl()->SetMaxLength(32768); m_prompt->SetName("Message for selected local model"); s->Add(m_prompt,0,wxEXPAND|wxALL,FromDIP(8));
        auto *buttons=new wxWrapSizer(wxHORIZONTAL); action(p,buttons,_L("Send locally"),[this] { send(); });
        auto *attach=action(p,buttons,_L("Attach image"),[]{}); attach->Enable(false); attach->SetToolTip(_L("Image transport validation exists, but a verified file-picker adapter is not yet connected. Use Models to inspect vision capability."));
        s->Add(buttons,0,wxEXPAND); note(p,s,_L("Default parameters: temperature 0.7, context 2048, output limit 512. These bounds are checked against the selected model. Stop cancels the active HTTP transfer."));
    }
    void send() {
        Model m; { std::lock_guard<std::mutex> lock(m_state.mutex); if(m_state.busy||!m_state.selected||m_state.selected->name!=m_session.model) { m_status->SetLabel(_L("Finish the current operation, then select and inspect a model and choose Use selected model for chat.")); return; } m=*m_state.selected; m_state.response.clear(); }
        const auto prompt=utf8(m_prompt->GetValue()); if(prompt.empty()) return;
        auto session=m_session; if(session.messages.empty()&&!m_system->GetValue().empty()) session.messages.push_back({{"role","system"},{"content",utf8(m_system->GetValue())}});
        session.messages.push_back({{"role","user"},{"content",prompt}});
        Json payload; try { payload=chat_payload(m,session.messages); } catch(...) { m_status->SetLabel(_L("Chat is unavailable: verify completion capability, context metadata, and message limits in Models.")); return; }
        m_session=session; m_prompt->Clear(); start([this,payload,session]() mutable {
            std::string answer; LocalClient c; auto result=c.execute(Operation::Chat,payload,m_state.cancel,[&](const Json &chunk) {
                if(chunk.contains("message")&&chunk.at("message").contains("content")) {
                    auto part=chunk.at("message").at("content").get<std::string>(); if(part.size()>max_chat_bytes-answer.size()) return false; answer+=part;
                    std::lock_guard<std::mutex> lock(m_state.mutex); m_state.response=answer; m_state.status="Receiving a local response."; m_state.changed=true;
                } return true;
            });
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
        for(const auto &profile:LaunchProfileRegistry::prebuilt()) { note(p,s,u8(profile.name)); auto *b=action(p,s,_L("Register verified executable"),[]{}); b->Enable(false); b->SetToolTip(_L("A privileged file-picker verifier and contained process executor are not yet bundled. Profile launch remains unavailable.")); }
        note(p,s,_L("The core validates executable identities and typed arguments, snapshots before mutation, and restores after failed launch or readiness. Actual launch requires the packaged executor and its runtime evidence."));
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
