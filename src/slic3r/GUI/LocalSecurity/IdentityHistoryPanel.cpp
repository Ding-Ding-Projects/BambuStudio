#include "IdentityHistoryPanel.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/CollapsibleFilterBar.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/TextInput.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/SuperConfirmGate.hpp"
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/scrolwin.h>
#include <wx/checkbox.h>
#include <wx/datetime.h>
#include <openssl/crypto.h>

namespace Slic3r::GUI::LocalSecurityUI {
using namespace LocalSecurity;
namespace {
wxString t(const Hooks& h,const char* key){return h.text(key);}
const char* action_name(HistoryAction a){switch(a){case HistoryAction::Created:return "Created";case HistoryAction::Renamed:return "Renamed";case HistoryAction::CredentialChanged:return "Credential changed";case HistoryAction::Removed:return "Removed";case HistoryAction::Restored:return "Restored";case HistoryAction::Labelled:return "Labelled";case HistoryAction::Pruned:return "Payload access removed";}return "Unknown";}
wxTextCtrl* entry(wxWindow* parent,wxSizer* sizer,const Hooks& hooks,const char* title,bool sensitive=false){auto label=new Label(parent,t(hooks,title));hooks.record_label(label,title);sizer->Add(label,0,wxEXPAND|wxBOTTOM,parent->FromDIP(4));auto box=new TextInput(parent,{}, {},{},wxDefaultPosition,wxDefaultSize,sensitive?wxTE_PASSWORD:0);auto control=box->GetTextCtrl();control->SetName(t(hooks,title));hooks.record_name(control,title);control->SetMaxLength(sensitive?1024:256);if(sensitive){hooks.register_sensitive(box);hooks.register_sensitive(control);}sizer->Add(box,0,wxEXPAND|wxBOTTOM,parent->FromDIP(8));return control;}
Button* button(wxWindow* parent,wxSizer* sizer,const Hooks& hooks,const char* title,std::function<void()> action){auto result=new Button(parent,t(hooks,title));result->SetVariant(Button::Variant::Outlined);result->SetButtonSize(Button::Size::Large);result->SetName(t(hooks,title));hooks.record_label(result,title);hooks.record_name(result,title);result->Bind(wxEVT_BUTTON,[action=std::move(action)](wxCommandEvent&){action();});sizer->Add(result,0,wxEXPAND|wxBOTTOM,parent->FromDIP(6));return result;}
Secret take(wxTextCtrl* control){auto text=control->GetValue().ToUTF8();Secret answer(std::string_view(text.data(),text.length()));control->ChangeValue({});return answer;}
void register_controls(wxWindow* parent,const Hooks& hooks,const std::string& id){hooks.register_surface(parent,id);unsigned index=0;for(auto child:parent->GetChildren())register_controls(child,hooks,id+"."+std::to_string(index++));}
std::optional<wxDateTime> date(const wxString& input){if(input.empty())return {};wxDateTime value;if(!value.ParseISODate(input))throw Failure(Error::InvalidInput);return value;}
}
IdentityHistoryPanel::IdentityHistoryPanel(wxWindow* parent,std::shared_ptr<IdentityHistory> history,Hooks hooks,RestoreIdentity restore)
    :wxPanel(parent),m_history(std::move(history)),m_hooks(std::move(hooks)),m_restore(std::move(restore)),m_actions(7,true),m_timer(this){
    if(!m_history||!m_hooks.text||!m_hooks.factual_text||!m_hooks.notify||!m_hooks.register_surface||!m_hooks.register_sensitive||!m_hooks.record_label||!m_hooks.record_factual_label||!m_hooks.record_tooltip||!m_hooks.record_name)throw Failure(Error::InvalidInput);
    auto root=new wxBoxSizer(wxVERTICAL);auto scroll=new wxScrolledWindow(this);scroll->SetScrollRate(0,FromDIP(12));auto body=new wxBoxSizer(wxVERTICAL);
    const char* disclosure="History has its own credential. Snapshots remain encrypted; this view never reveals authenticator secrets, passwords, PINs or codes.";auto notice=new Label(scroll,m_hooks.factual_text(disclosure),LB_AUTO_WRAP);m_hooks.record_factual_label(notice,disclosure);body->Add(notice,0,wxEXPAND|wxBOTTOM,FromDIP(8));
    auto password=entry(scroll,body,m_hooks,"History password",true);auto confirmation=entry(scroll,body,m_hooks,"Confirm new history password",true);
    button(scroll,body,m_hooks,"Create protected history",[this,password,confirmation]{safely([&]{auto answer=take(password),repeat=take(confirmation);if(answer.size()!=repeat.size()||CRYPTO_memcmp(answer.data(),repeat.data(),answer.size())!=0)throw Failure(Error::Authentication);m_history->initialize(CredentialKind::Password,answer);m_answer=std::move(answer);m_expires=std::chrono::steady_clock::now()+std::chrono::minutes(5);read_page();m_status->SetLabel(t(m_hooks,"Protected history was created. Existing histories are never overwritten by this action."));});});
    button(scroll,body,m_hooks,"Unlock history for five minutes",[this,password]{safely([&]{auto answer=take(password);m_rows=m_history->read_metadata(answer,0,10);m_answer=std::move(answer);m_offset=0;m_expires=std::chrono::steady_clock::now()+std::chrono::minutes(5);filter();});});
    button(scroll,body,m_hooks,"Lock history now",[this]{LockHistory();});
    // Search, date range and action filters sit in the shared collapsible filter bar; collapsed, it still names every active filter.
    m_filters=new CollapsibleFilterBar(scroll,"identity_history",t(m_hooks,"Search and filters"));auto filter_body=m_filters->GetBody();auto filter_sizer=m_filters->GetBodySizer();
    m_search=new SearchField(m_filters->GetBody(),t(m_hooks,"Search redacted history metadata"));m_search->GetTextCtrl()->SetName(t(m_hooks,"Search redacted history metadata"));m_hooks.record_name(m_search->GetTextCtrl(),"Search redacted history metadata");m_search->SetOnQuery([this](const wxString&){update_active_filters();safely([this]{filter();});});m_search->SetOnRegexToggle([this](bool){safely([this]{filter();});});filter_sizer->Add(m_search,0,wxEXPAND|wxBOTTOM,FromDIP(8));
    m_from=entry(filter_body,filter_sizer,m_hooks,"From date (YYYY-MM-DD, local time)");m_to=entry(filter_body,filter_sizer,m_hooks,"Through date (YYYY-MM-DD, local time)");m_from->SetMaxLength(10);m_to->SetMaxLength(10);
    for(auto date_field:{m_from,m_to})date_field->Bind(wxEVT_TEXT,[this](wxCommandEvent& event){update_active_filters();event.Skip();});
    button(filter_body,filter_sizer,m_hooks,"Apply date filters",[this]{safely([this]{filter();});});
    for(unsigned i=0;i<7;++i){auto choice=new wxCheckBox(filter_body,wxID_ANY,t(m_hooks,action_name(static_cast<HistoryAction>(i))));m_hooks.record_label(choice,action_name(static_cast<HistoryAction>(i)));choice->SetValue(true);choice->Bind(wxEVT_CHECKBOX,[this,i](wxCommandEvent& event){m_actions[i]=event.IsChecked();update_active_filters();safely([this]{filter();});});filter_sizer->Add(choice,0,wxEXPAND|wxBOTTOM,FromDIP(4));}
    body->Add(m_filters->GetSectionSizer(),0,wxEXPAND|wxBOTTOM,FromDIP(8));
    m_list=new wxListBox(scroll,wxID_ANY,wxDefaultPosition,FromDIP(wxSize(420,200)),0,nullptr,wxLB_EXTENDED);m_list->SetName(t(m_hooks,"Redacted history revisions"));m_hooks.record_name(m_list,"Redacted history revisions");body->Add(m_list,0,wxEXPAND|wxBOTTOM,FromDIP(8));
    button(scroll,body,m_hooks,"Previous history page",[this]{safely([this]{require_session();m_offset=m_offset>=10?m_offset-10:0;read_page();});});
    button(scroll,body,m_hooks,"Next history page",[this]{safely([this]{require_session();if(m_offset>=10000||m_rows.size()<10)throw Failure(Error::InvalidInput);m_offset+=10;read_page();});});
    button(scroll,body,m_hooks,"Compare two selected revisions",[this]{safely([this]{require_session();wxArrayInt selection;m_list->GetSelections(selection);if(selection.size()!=2)throw Failure(Error::InvalidInput);auto diff=IdentityHistory::redacted_diff(m_rows.at(m_visible.at(selection[0])),m_rows.at(m_visible.at(selection[1])));m_status->SetLabel(t(m_hooks,diff.same_identity?"Both revisions belong to the same identity.":"These revisions belong to different identities.")+" "+t(m_hooks,diff.action_changed?"Their recorded actions differ. Snapshot contents are omitted.":"Their recorded actions match. Snapshot contents are omitted."));});});
    auto label=entry(scroll,body,m_hooks,"History label (256 UTF-8 bytes maximum)");
    button(scroll,body,m_hooks,"Label selected identity",[this,label]{safely([&]{require_session();wxArrayInt selected;m_list->GetSelections(selected);if(selected.size()!=1)throw Failure(Error::InvalidInput);const auto row=m_rows.at(m_visible.at(selected[0]));auto value=take(label);m_history->append_label(*m_answer,row.identity,value);read_page();});});
    button(scroll,body,m_hooks,"Read selected label",[this]{safely([this]{require_session();wxArrayInt selected;m_list->GetSelections(selected);if(selected.size()!=1)throw Failure(Error::InvalidInput);auto value=m_history->read_label(*m_answer,m_rows.at(m_visible.at(selected[0])).revision);m_status->SetLabel(wxString::FromUTF8(reinterpret_cast<const char*>(value.data()),value.size()));});});
    auto restore_button=button(scroll,body,m_hooks,"Restore selected revision",[this]{safely([this]{require_session();wxArrayInt selected;m_list->GetSelections(selected);if(selected.size()!=1||!m_restore)throw Failure(Error::Unavailable);auto row=m_rows.at(m_visible.at(selected[0]));if(row.action==HistoryAction::Labelled||row.action==HistoryAction::Pruned||row.pruned)throw Failure(Error::InvalidInput);SuperConfirmGate::Spec spec;spec.action=t(m_hooks,"Restore local identity");spec.consequence=t(m_hooks,"The selected identity will be restored through its live owner and recorded as a new revision. Existing history is retained.");spec.affected={wxString::FromUTF8(row.identity)};wxWeakRef<IdentityHistoryPanel> self(this);SuperConfirmGate::Show(m_list,spec,[self,row]{if(self)self->safely([&]{self->require_session();auto e=self->m_history->read_revision(*self->m_answer,row.revision);self->m_restore({e.revision,e.identity,e.action,std::move(e.snapshot)});self->read_page();self->m_status->SetLabel(t(self->m_hooks,"The live owner completed and recorded the restore."));});});});});
    if(!m_restore){restore_button->Enable(false);restore_button->SetToolTip(t(m_hooks,"No live identity restore dispatcher is registered. Encrypted history remains intact."));m_hooks.record_tooltip(restore_button,"No live identity restore dispatcher is registered. Encrypted history remains intact.");}
    auto export_button=button(scroll,body,m_hooks,"Export this redacted history page",[this]{safely([this]{require_session();if(!m_hooks.export_text)throw Failure(Error::Unavailable);m_hooks.export_text(t(m_hooks,"Redacted identity history"),m_history->export_redacted(*m_answer,m_offset,10));});});if(!m_hooks.export_text){export_button->Enable(false);export_button->SetToolTip(t(m_hooks,"The shared export service is unavailable."));m_hooks.record_tooltip(export_button,"The shared export service is unavailable.");}
    auto retention=button(scroll,body,m_hooks,"Remove access to selected payloads",[this]{safely([this]{
        require_session();if(!m_hooks.confirm_retention)throw Failure(Error::Unavailable);
        wxArrayInt selected;m_list->GetSelections(selected);std::vector<std::string> revisions;
        for(auto index:selected)revisions.push_back(m_rows.at(m_visible.at(index)).revision);
        auto preview=m_history->preview_prune(*m_answer,revisions);
        std::vector<wxString> affected;for(const auto& row:preview.rows())affected.push_back(wxString::FromUTF8(row.revision));
        wxWeakRef<IdentityHistoryPanel> self(this);
        m_hooks.confirm_retention(m_list,m_hooks.factual_text("Remove active decryption access to the selected payloads. Their history metadata remains. Backups, vault copies and storage journals may retain recoverable material; this does not guarantee forensic erasure."),affected,
            [self,preview](SuperConfirm::State confirmation){if(self)self->safely([&]{self->require_session();self->m_history->prune(*self->m_answer,preview,confirmation);self->read_page();self->m_status->SetLabel(t(self->m_hooks,"Active payload decryption access was removed. Metadata remains; external copies are outside this operation."));});});
    });});
    if(!m_hooks.confirm_retention){retention->Enable(false);const char* reason="The native two-key confirmation service is unavailable. No payload access will be removed.";retention->SetToolTip(t(m_hooks,reason));m_hooks.record_tooltip(retention,reason);}
    button(scroll,body,m_hooks,"Show protected history capacity",[this]{safely([this]{require_session();auto c=m_history->capacity(*m_answer);m_status->SetLabel(wxString::Format("%u / %u ",c.active_keys,c.maximum_active_keys)+t(m_hooks,"active event keys")+"; "+wxString::Format("%u / %u ",c.tombstones,c.maximum_tombstones)+t(m_hooks,"retained removal records"));});});
    m_status=new Label(scroll,t(m_hooks,"History is locked. Enter its independent password, or create it explicitly if no history exists."),LB_AUTO_WRAP);m_hooks.register_sensitive(m_status);body->Add(m_status,0,wxEXPAND|wxTOP,FromDIP(8));scroll->SetSizer(body);root->Add(scroll,1,wxEXPAND|wxALL,FromDIP(12));SetSizer(root);register_controls(this,m_hooks,"local-security.identity-history");
    Bind(wxEVT_TIMER,[this](wxTimerEvent&){if(m_answer&&(!IsShownOnScreen()||std::chrono::steady_clock::now()>=m_expires))LockHistory();});m_timer.Start(1000);
}
IdentityHistoryPanel::~IdentityHistoryPanel(){m_timer.Stop();m_answer.reset();}
void IdentityHistoryPanel::LockHistory(){m_answer.reset();m_rows.clear();m_visible.clear();m_list->Clear();m_status->SetLabel(t(m_hooks,"History is locked."));}
void IdentityHistoryPanel::require_session(){if(!m_answer||std::chrono::steady_clock::now()>=m_expires){LockHistory();throw Failure(Error::Authentication);}}
void IdentityHistoryPanel::safely(const std::function<void()>& action){try{action();}catch(const Failure& e){m_status->SetLabel(t(m_hooks,e.what()));m_hooks.notify(t(m_hooks,e.what()));}catch(...){m_status->SetLabel(t(m_hooks,"History operation did not complete. Existing data is retained."));}}
void IdentityHistoryPanel::read_page(){require_session();m_rows=m_history->read_metadata(*m_answer,m_offset,10);filter();}
void IdentityHistoryPanel::update_active_filters(){
    if(!m_filters)return;
    std::vector<wxString> active;
    const wxString search=CollapsibleFilterBar::SearchFilterLabel(m_search->GetValue());if(!search.empty())active.push_back(search);
    // Dates apply when the history is filtered; a typed date is reported as entered.
    if(!m_from->GetValue().empty())active.push_back(CollapsibleFilterBar::FilterLabel(t(m_hooks,"From"),m_from->GetValue()));
    if(!m_to->GetValue().empty())active.push_back(CollapsibleFilterBar::FilterLabel(t(m_hooks,"To"),m_to->GetValue()));
    for(std::size_t i=0;i<m_actions.size();++i)if(!m_actions[i])active.push_back(CollapsibleFilterBar::ExcludedFilterLabel(t(m_hooks,action_name(static_cast<HistoryAction>(i)))));
    m_filters->SetActiveFilters(active);
}
void IdentityHistoryPanel::filter(){require_session();auto from=date(m_from->GetValue()),to=date(m_to->GetValue());if(from&&to&&*from>*to)throw Failure(Error::InvalidInput);if(to)*to+=wxDateSpan::Days(1);m_list->Clear();m_visible.clear();SearchField::MatchPass match(m_search->GetValue(),m_search->IsRegexEnabled(),m_search->IsCaseSensitive(),m_search->IsWholeWord(),m_search->IsMultiline());for(std::size_t i=0;i<m_rows.size();++i){const auto& row=m_rows[i];auto action=static_cast<std::size_t>(row.action);if(action>=m_actions.size()||!m_actions[action])continue;wxDateTime timestamp(static_cast<time_t>(row.committed_at_utc_seconds));if((from&&timestamp<*from)||(to&&timestamp>=*to))continue;auto label=timestamp.FormatISOCombined(' ')+" | "+t(m_hooks,action_name(row.action))+" | "+wxString::FromUTF8(row.identity)+" | "+wxString::FromUTF8(row.revision);if(row.pruned)label+=" | "+t(m_hooks,"Payload access removed");if(match.matches(label)){m_visible.push_back(i);m_list->Append(label);}}m_status->SetLabel(wxString::Format("%zu / %zu ",m_visible.size(),m_rows.size())+t(m_hooks,"revisions shown on this page. Labels and snapshot contents are omitted."));}
}
