#include "SecurityPanels.hpp"
#include "PairingQr.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/TextInput.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/SuperConfirmGate.hpp"
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/scrolwin.h>
#include <wx/radiobut.h>
#include <wx/checkbox.h>
#include <wx/spinctrl.h>
#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/display.h>
#include <wx/utils.h>
#include <wx/dcbuffer.h>
#include <wx/filedlg.h>
#include <chrono>
#include <algorithm>

namespace Slic3r::GUI::LocalSecurityUI {
using namespace Slic3r::LocalSecurity;
namespace {
void require_hooks(const Hooks& h){if(!h.text||!h.factual_text||!h.notify||!h.register_surface||!h.register_sensitive||!h.record_label||!h.record_factual_label||!h.record_tooltip||!h.record_name)throw Failure(Error::InvalidInput);}
std::uint64_t seconds(){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());}
Time now(){return std::chrono::steady_clock::now();}
wxString text(const Hooks& h,const char* value){return h.text(value);}
Label* source_label(wxWindow* parent,const Hooks& h,const char* value,long style=0){auto label=new Label(parent,text(h,value),style);h.record_label(label,value);return label;}
Label* factual_label(wxWindow* parent,const Hooks& h,const char* value){auto label=new Label(parent,h.factual_text(value),LB_AUTO_WRAP);h.record_factual_label(label,value);return label;}
std::string utf8(const wxString& value){auto b=value.ToUTF8();return std::string(b.data(),b.length());}
Secret secret(wxTextCtrl* control){auto b=control->GetValue().ToUTF8();Secret answer(std::string_view(b.data(),b.length()));control->ChangeValue({});return answer;}
wxTextCtrl* field(wxWindow* parent,wxSizer* sizer,const Hooks& h,const char* label,bool hidden=false){
    sizer->Add(source_label(parent,h,label),0,wxEXPAND|wxBOTTOM,parent->FromDIP(4));
    auto box=new TextInput(parent,{}, {},{},wxDefaultPosition,wxDefaultSize,hidden?(wxTE_PASSWORD|wxTE_PROCESS_ENTER):0);auto entry=box->GetTextCtrl();entry->SetName(text(h,label));h.record_name(entry,label);entry->SetMaxLength(hidden?8192:256);if(hidden){h.register_sensitive(box);h.register_sensitive(entry);}sizer->Add(box,0,wxEXPAND|wxBOTTOM,parent->FromDIP(8));return entry;
}
Button* action(wxWindow* parent,wxSizer* sizer,const Hooks& h,const char* title,std::function<void()> fn){
    auto button=new Button(parent,text(h,title));button->SetVariant(Button::Variant::Outlined);button->SetButtonSize(Button::Size::Large);button->SetName(text(h,title));
    h.record_label(button,title);h.record_name(button,title);
    button->Bind(wxEVT_BUTTON,[fn=std::move(fn)](wxCommandEvent&){fn();});sizer->Add(button,0,wxEXPAND|wxBOTTOM,parent->FromDIP(6));return button;
}
SearchField* search(wxWindow* parent,wxSizer* sizer,const Hooks& h,const char* title,std::function<void()> filter){
    auto result=new SearchField(parent,text(h,title));result->GetTextCtrl()->SetName(text(h,title));h.record_name(result->GetTextCtrl(),title);result->SetOnQuery([filter](const wxString&){filter();});result->SetOnRegexToggle([filter](bool){filter();});sizer->Add(result,0,wxEXPAND|wxBOTTOM,parent->FromDIP(8));return result;
}
void register_all(wxWindow* window,const Hooks& h,const std::string& root){
    h.register_surface(window,root);unsigned i=0;for(auto child:window->GetChildren())register_all(child,h,root+"."+std::to_string(i++));
}
void copy(const wxString& value){if(!wxTheClipboard->Open())throw Failure(Error::Unavailable);bool ok=wxTheClipboard->SetData(new wxTextDataObject(value));wxTheClipboard->Close();if(!ok)throw Failure(Error::Unavailable);}
wxString failure(const Hooks& h,const Failure& e){return text(h,e.what());}
class PairingQrView final:public wxPanel {
    std::optional<PairingQr> m_qr;
public:
    PairingQrView(wxWindow* parent,const Hooks& hooks):wxPanel(parent){
        SetBackgroundStyle(wxBG_STYLE_PAINT);SetMinSize(FromDIP(wxSize(300,300)));SetName(text(hooks,"Registration QR code. The manual setup key is available beside it."));hooks.register_sensitive(this);
        Bind(wxEVT_PAINT,[this](wxPaintEvent&){wxAutoBufferedPaintDC dc(this);dc.SetBackground(*wxWHITE_BRUSH);dc.Clear();if(!m_qr)return;auto size=GetClientSize();const int count=static_cast<int>(m_qr->modules+8);int scale=std::min(size.x,size.y)/count;if(scale<2)return;int ox=(size.x-count*scale)/2+4*scale,oy=(size.y-count*scale)/2+4*scale;dc.SetPen(*wxTRANSPARENT_PEN);dc.SetBrush(*wxBLACK_BRUSH);for(unsigned y=0;y<m_qr->modules;++y)for(unsigned x=0;x<m_qr->modules;++x)if(m_qr->module(x,y))dc.DrawRectangle(ox+static_cast<int>(x)*scale,oy+static_cast<int>(y)*scale,scale,scale);});
        Hide();
    }
    void Reveal(const Enrollment& e){m_qr=PairingQr::encode(e);Show();Refresh();}
    void Clear(){m_qr.reset();Hide();Refresh();}
};
}

AuthenticatorPanel::AuthenticatorPanel(wxWindow* parent,std::shared_ptr<AuthenticatorStore> store,Hooks hooks)
    :wxPanel(parent),m_store(std::move(store)),m_hooks(std::move(hooks)),m_timer(this){
    require_hooks(m_hooks);if(!m_store)throw Failure(Error::InvalidInput);
    auto root=new wxBoxSizer(wxVERTICAL);auto scroll=new wxScrolledWindow(this);scroll->SetScrollRate(0,FromDIP(12));auto body=new wxBoxSizer(wxVERTICAL);
    m_search=search(scroll,body,m_hooks,"Search authenticator entries",[this]{filter();});
    m_list=new wxListBox(scroll,wxID_ANY,wxDefaultPosition,FromDIP(wxSize(350,160)),0,nullptr,wxLB_EXTENDED);m_list->SetName(text(m_hooks,"Authenticator entries"));m_hooks.record_name(m_list,"Authenticator entries");body->Add(m_list,0,wxEXPAND|wxBOTTOM,FromDIP(8));
    m_code=new Label(scroll,text(m_hooks,"Select an entry to show its current code."));m_hooks.register_sensitive(m_code);auto font=m_code->GetFont();font.SetPointSize(26);m_code->SetFont(font);body->Add(m_code,0,wxEXPAND);
    m_countdown=new Label(scroll,{});body->Add(m_countdown,0,wxEXPAND|wxBOTTOM,FromDIP(8));
    action(scroll,body,m_hooks,"Copy current code",[this]{safely([this]{auto id=selected();copy(wxString::FromUTF8(m_store->code(id,seconds()).current));});});
    action(scroll,body,m_hooks,"Show next code",[this]{safely([this]{m_status->SetLabel(text(m_hooks,"Next code: ")+wxString::FromUTF8(m_store->code(selected(),seconds()).next));});});
    action(scroll,body,m_hooks,"Move selected entry up",[this]{safely([this]{m_store->move(selected(),-1);RefreshEntries();});});
    action(scroll,body,m_hooks,"Move selected entry down",[this]{safely([this]{m_store->move(selected(),1);RefreshEntries();});});
    auto issuer=field(scroll,body,m_hooks,"Issuer");auto account=field(scroll,body,m_hooks,"Account");auto group=field(scroll,body,m_hooks,"Group");
    action(scroll,body,m_hooks,"Save selected entry labels",[this,issuer,account,group]{safely([&]{m_store->rename(selected(),utf8(issuer->GetValue()),utf8(account->GetValue()),utf8(group->GetValue()));RefreshEntries();});});
    m_list->Bind(wxEVT_LISTBOX,[this,issuer,account,group](wxCommandEvent&){safely([&]{auto id=selected();for(const auto& entry:m_entries)if(entry.id==id){issuer->ChangeValue(wxString::FromUTF8(entry.issuer));account->ChangeValue(wxString::FromUTF8(entry.account));group->ChangeValue(wxString::FromUTF8(entry.group));}refresh_codes();});});
    m_uri=field(scroll,body,m_hooks,"Pairing URI (kept local)",true);m_uri->SetHint("otpauth://totp/...");
    m_confirmation=field(scroll,body,m_hooks,"Current code to confirm pairing",true);m_confirmation->SetMaxLength(8);
    action(scroll,body,m_hooks,"Confirm and add authenticator",[this]{safely([this]{auto uri=m_uri->GetValue().ToUTF8();auto enrollment=parse_otpauth(std::string_view(uri.data(),uri.length()));auto code=secret(m_confirmation);m_store->add(std::move(enrollment),std::string_view(reinterpret_cast<const char*>(code.data()),code.size()),seconds());m_uri->ChangeValue({});RefreshEntries();});});
    auto manual=field(scroll,body,m_hooks,"Manual base32 secret",true);manual->SetMaxLength(1024);
    action(scroll,body,m_hooks,"Add manual SHA-1 / 6-digit / 30-second entry",[this,manual,issuer,account]{safely([&]{auto value=manual->GetValue().ToUTF8();Enrollment e;e.secret=decode_base32(std::string_view(value.data(),value.length()));e.issuer=utf8(issuer->GetValue());e.account=utf8(account->GetValue());auto code=secret(m_confirmation);m_store->add(std::move(e),std::string_view(reinterpret_cast<const char*>(code.data()),code.size()),seconds());manual->ChangeValue({});RefreshEntries();});});
    auto accept_import=[this](std::optional<Secret> payload){if(!payload)return;auto enrollment=parse_otpauth(std::string_view(reinterpret_cast<const char*>(payload->data()),payload->size()));auto uri=pairing_uri(enrollment);m_uri->ChangeValue(wxString::FromUTF8(uri));m_status->SetLabel(text(m_hooks,"QR parameters loaded locally. Enter a current code to confirm before saving."));};
    auto file_import=action(scroll,body,m_hooks,"Import QR from image",[this,accept_import]{safely([&]{if(!m_hooks.decode_qr_file)throw Failure(Error::Unavailable);wxFileDialog picker(this,text(m_hooks,"Select a QR image"),{}, {},"PNG or JPEG (*.png;*.jpg;*.jpeg)|*.png;*.jpg;*.jpeg",wxFD_OPEN|wxFD_FILE_MUST_EXIST);if(picker.ShowModal()==wxID_OK)accept_import(m_hooks.decode_qr_file(std::filesystem::path(picker.GetPath().ToStdWstring())));});});
    if(!m_hooks.decode_qr_file){file_import->Enable(false);file_import->SetToolTip(text(m_hooks,"The bundled isolated QR image decoder is unavailable. Use a pairing URI or manual entry."));m_hooks.record_tooltip(file_import,"The bundled isolated QR image decoder is unavailable. Use a pairing URI or manual entry.");}
    auto clipboard_import=action(scroll,body,m_hooks,"Import QR from clipboard image",[this,accept_import]{safely([&]{if(!m_hooks.decode_qr_clipboard)throw Failure(Error::Unavailable);accept_import(m_hooks.decode_qr_clipboard());});});
    if(!m_hooks.decode_qr_clipboard){clipboard_import->Enable(false);clipboard_import->SetToolTip(text(m_hooks,"The bundled isolated clipboard-image decoder is unavailable."));m_hooks.record_tooltip(clipboard_import,"The bundled isolated clipboard-image decoder is unavailable.");}
    auto camera_import=action(scroll,body,m_hooks,"Scan QR with camera",[this]{if(!m_hooks.scan_qr_camera)return;wxWeakRef<AuthenticatorPanel> self(this);m_hooks.scan_qr_camera([self](std::optional<Secret> payload){if(!self||!payload)return;self->safely([&]{auto e=parse_otpauth(std::string_view(reinterpret_cast<const char*>(payload->data()),payload->size()));self->m_uri->ChangeValue(wxString::FromUTF8(pairing_uri(e)));self->m_status->SetLabel(text(self->m_hooks,"Camera parameters loaded locally. Confirm with a current code before saving."));});});});
    if(!m_hooks.scan_qr_camera){camera_import->Enable(false);camera_import->SetToolTip(text(m_hooks,"No verified local camera decoder is registered."));m_hooks.record_tooltip(camera_import,"No verified local camera decoder is registered.");}
    auto export_button=action(scroll,body,m_hooks,"Export redacted entries",[this]{safely([this]{if(!m_hooks.export_text)throw Failure(Error::Unavailable);m_hooks.export_text(text(m_hooks,"Authenticator entries"),m_store->export_redacted());});});
    if(!m_hooks.export_text){export_button->Enable(false);export_button->SetToolTip(text(m_hooks,"The shared export service is unavailable."));m_hooks.record_tooltip(export_button,"The shared export service is unavailable.");}
    action(scroll,body,m_hooks,"Remove selected entries",[this]{safely([this]{wxArrayInt selected_rows;m_list->GetSelections(selected_rows);if(selected_rows.empty())throw Failure(Error::InvalidInput);std::vector<std::string> ids;SuperConfirmGate::Spec spec;spec.action=text(m_hooks,"Remove authenticator entries");spec.consequence=text(m_hooks,"Selected authenticator entries will be removed from this device. Their encrypted history is retained.");for(auto row:selected_rows){ids.push_back(m_visible.at(row));spec.affected.push_back(m_list->GetString(row));}wxWeakRef<AuthenticatorPanel> self(this);SuperConfirmGate::Show(m_list,spec,[self,ids]{if(self)self->safely([&]{for(const auto& id:ids)self->m_store->remove(id);self->RefreshEntries();});});});});
    m_status=new Label(scroll,text(m_hooks,"Codes use this computer's clock. Clock accuracy is not verified; check the operating-system time if a code is rejected."),LB_AUTO_WRAP);m_hooks.register_sensitive(m_status);body->Add(m_status,0,wxEXPAND|wxTOP,FromDIP(8));
    scroll->SetSizer(body);root->Add(scroll,1,wxEXPAND|wxALL,FromDIP(12));SetSizer(root);register_all(this,m_hooks,"local-security.authenticator");
    Bind(wxEVT_TIMER,[this](wxTimerEvent&){refresh_codes();});m_timer.Start(1000);RefreshEntries();
}
AuthenticatorPanel::~AuthenticatorPanel(){m_timer.Stop();}
void AuthenticatorPanel::safely(const std::function<void()>& fn){try{fn();}catch(const Failure& e){m_status->SetLabel(failure(m_hooks,e));m_hooks.notify(failure(m_hooks,e));}catch(...){m_status->SetLabel(text(m_hooks,"The local operation did not complete. No success was recorded."));}}
void AuthenticatorPanel::RefreshEntries(){safely([this]{m_entries=m_store->list();filter();});}
void AuthenticatorPanel::filter(){m_list->Clear();m_visible.clear();SearchField::MatchPass match(m_search->GetValue(),m_search->IsRegexEnabled(),m_search->IsCaseSensitive(),m_search->IsWholeWord(),m_search->IsMultiline());for(const auto& e:m_entries){auto label=wxString::FromUTF8(e.issuer+" / "+e.account+" / "+e.group);if(match.matches(label)){m_list->Append(label);m_visible.push_back(e.id);}}m_code->SetLabel(text(m_hooks,m_visible.empty()?"No matching authenticator entries.":"Select an entry to show its current code."));m_countdown->SetLabel({});}
std::string AuthenticatorPanel::selected() const{wxArrayInt rows;m_list->GetSelections(rows);if(rows.size()!=1)throw Failure(Error::InvalidInput);return m_visible.at(rows[0]);}
void AuthenticatorPanel::refresh_codes(){if(!IsShownOnScreen()){m_code->SetLabel({});m_countdown->SetLabel({});m_status->SetLabel({});return;}try{auto c=m_store->code(selected(),seconds());auto grouped=c.current.substr(0,c.current.size()/2)+" "+c.current.substr(c.current.size()/2);auto value=wxString::FromUTF8(grouped);if(m_code->GetLabel()!=value)m_code->SetLabel(value);m_countdown->SetLabel(wxString::Format("%u ",c.seconds_remaining)+text(m_hooks,"seconds remaining"));}catch(const Failure&){m_code->SetLabel(text(m_hooks,"Select one available entry."));m_countdown->SetLabel({});}}

void LockWizard::Open(wxWindow* anchor,std::shared_ptr<ElementLock> lock,wxString target,wxString recovery,Hooks hooks){auto dialog=new LockWizard(anchor,std::move(lock),std::move(target),std::move(recovery),std::move(hooks));dialog->Show();}
LockWizard::LockWizard(wxWindow* anchor,std::shared_ptr<ElementLock> lock,wxString target,wxString recovery,Hooks hooks)
    :wxDialog(anchor,wxID_ANY,target,wxDefaultPosition,wxDefaultSize,wxBORDER_NONE|wxRESIZE_BORDER),m_lock(std::move(lock)),m_hooks(std::move(hooks)),m_anchor(anchor){
    require_hooks(m_hooks);if(!m_lock||!anchor||recovery.empty())throw Failure(Error::InvalidInput);
    auto root=new wxBoxSizer(wxVERTICAL);root->Add(new MD3DialogCaption(this,target),0,wxEXPAND);auto scroll=new wxScrolledWindow(this);scroll->SetScrollRate(0,FromDIP(12));auto body=new wxBoxSizer(wxVERTICAL);
    auto search_field=search(scroll,body,m_hooks,"Search lock settings",[]{});
    body->Add(factual_label(scroll,m_hooks,"This is a toy lock, not a security boundary. To reset, close the application and delete its local application-data folder yourself:"),0,wxEXPAND|wxBOTTOM,FromDIP(8));
    auto path=new TextInput(scroll,recovery,{}, {},wxDefaultPosition,wxDefaultSize,wxTE_READONLY);path->GetTextCtrl()->SetName(text(m_hooks,"Recovery folder"));m_hooks.record_name(path->GetTextCtrl(),"Recovery folder");body->Add(path,0,wxEXPAND|wxBOTTOM,FromDIP(8));
    m_status=new Label(scroll,{},LB_AUTO_WRAP);body->Add(m_status,0,wxEXPAND|wxBOTTOM,FromDIP(8));
    if(m_lock->configured()){
        m_answer=field(scroll,body,m_hooks,"Unlock answer",true);m_answer->SetMaxLength(1024);
        auto keypad=new wxGridSizer(3,FromDIP(4),FromDIP(4));
        for(unsigned digit=1;digit<=9;++digit){auto value=wxString::Format("%u",digit);auto b=new Button(scroll,value);b->SetButtonSize(Button::Size::Large);b->SetName(value);b->Bind(wxEVT_BUTTON,[this,value](wxCommandEvent&){if(m_lock->expected(now())==Factor::Pin&&m_answer->GetValue().size()<32)m_answer->AppendText(value);});keypad->Add(b,0,wxEXPAND);}body->Add(keypad,0,wxEXPAND|wxBOTTOM,FromDIP(8));
        action(scroll,body,m_hooks,"Zero",[this]{if(m_lock->expected(now())==Factor::Pin&&m_answer->GetValue().size()<32)m_answer->AppendText("0");});
        action(scroll,body,m_hooks,"Backspace",[this]{auto n=m_answer->GetLastPosition();if(n>0)m_answer->Remove(n-1,n);});
        action(scroll,body,m_hooks,"Clear answer",[this]{m_answer->ChangeValue({});});
        auto submit=[this]{safely([this]{auto answer=secret(m_answer);if(m_lock->submit(answer,now(),seconds())){m_hooks.notify(text(m_hooks,"Unlocked. Activate the target again to use it."));close();}else refresh_prompt();});};
        action(scroll,body,m_hooks,"Submit answer",submit);m_answer->Bind(wxEVT_TEXT_ENTER,[submit](wxCommandEvent&){submit();});refresh_prompt();
        action(scroll,body,m_hooks,"Lock again",[this]{m_lock->relock();refresh_prompt();});
    }else{
        const char* policies[]={"PIN","Password","PIN plus password","Password plus TOTP","PIN plus TOTP","Password plus PIN plus TOTP"};
        std::vector<wxRadioButton*> options;for(unsigned i=0;i<6;++i){auto b=new wxRadioButton(scroll,wxID_ANY,text(m_hooks,policies[i]),wxDefaultPosition,wxDefaultSize,i==0?wxRB_GROUP:0);m_hooks.record_label(b,policies[i]);body->Add(b,0,wxEXPAND|wxBOTTOM,FromDIP(6));options.push_back(b);}options[0]->SetValue(true);
        auto pin=field(scroll,body,m_hooks,"New PIN (4 to 32 digits)",true);auto password=field(scroll,body,m_hooks,"New password (8 to 1024 UTF-8 bytes)",true);auto otp=field(scroll,body,m_hooks,"TOTP pairing URI for this element",true);auto confirm=field(scroll,body,m_hooks,"Current TOTP code to confirm",true);
        auto generated=std::make_shared<std::optional<Enrollment>>();auto qr=new PairingQrView(scroll,m_hooks);body->Add(qr,0,wxEXPAND|wxBOTTOM,FromDIP(8));
        auto manual_key=field(scroll,body,m_hooks,"One-time manual setup key",false);manual_key->SetEditable(false);manual_key->Hide();m_hooks.register_sensitive(manual_key);
        body->Add(source_label(scroll,m_hooks,"Generated pairing uses SHA-1, six digits and a 30-second period. The QR and manual key are registration-only and are never saved as images.",LB_AUTO_WRAP),0,wxEXPAND|wxBOTTOM,FromDIP(8));
        action(scroll,body,m_hooks,"Generate a local pairing secret",[this,generated,otp,qr,manual_key,target]{safely([&]{Enrollment e;e.issuer="Bambu Studio";e.account=utf8(target);if(e.account.size()>256)e.account="Local element";e.secret=random_secret(20);*generated=std::move(e);otp->ChangeValue(wxString::FromUTF8(pairing_uri(**generated)));qr->Clear();manual_key->ChangeValue({});manual_key->Hide();m_status->SetLabel(text(m_hooks,"A new secret was generated locally. Reveal the QR or manual key, pair it, then type a current code before creating the lock."));Layout();});});
        action(scroll,body,m_hooks,"Reveal registration QR and manual key",[this,generated,qr,manual_key,scroll]{safely([&]{if(!*generated)throw Failure(Error::Missing);qr->Reveal(**generated);manual_key->ChangeValue(wxString::FromUTF8(encode_base32((**generated).secret)));manual_key->Show();scroll->Layout();scroll->FitInside();});});
        action(scroll,body,m_hooks,"Hide registration QR and manual key",[qr,manual_key,scroll]{qr->Clear();manual_key->ChangeValue({});manual_key->Hide();scroll->Layout();scroll->FitInside();});
        const char* durations[]={"This surface only","A set number of minutes","Until the application closes"};std::vector<wxRadioButton*> duration;for(unsigned i=0;i<3;++i){auto b=new wxRadioButton(scroll,wxID_ANY,text(m_hooks,durations[i]),wxDefaultPosition,wxDefaultSize,i==0?wxRB_GROUP:0);m_hooks.record_label(b,durations[i]);body->Add(b,0,wxEXPAND|wxBOTTOM,FromDIP(6));duration.push_back(b);}duration[0]->SetValue(true);
        auto minutes=new wxSpinCtrl(scroll,wxID_ANY,"5",wxDefaultPosition,wxDefaultSize,wxSP_ARROW_KEYS,1,1440,5);minutes->SetName(text(m_hooks,"Unlock minutes"));m_hooks.record_name(minutes,"Unlock minutes");body->Add(minutes,0,wxEXPAND|wxBOTTOM,FromDIP(8));
        auto disclosure=new wxCheckBox(scroll,wxID_ANY,m_hooks.factual_text("This is a toy lock, and the recovery folder is shown above."));m_hooks.record_factual_label(disclosure,"This is a toy lock, and the recovery folder is shown above.");body->Add(disclosure,0,wxEXPAND|wxBOTTOM,FromDIP(8));
        action(scroll,body,m_hooks,"Create this element's lock",[this,options,duration,pin,password,otp,confirm,minutes,disclosure]{safely([&]{if(!disclosure->GetValue())throw Failure(Error::InvalidInput);LockEnrollment e;for(unsigned i=0;i<options.size();++i)if(options[i]->GetValue())e.settings.policy=static_cast<Policy>(i);for(unsigned i=0;i<duration.size();++i)if(duration[i]->GetValue())e.settings.duration=static_cast<Duration>(i);e.settings.minutes=minutes->GetValue();e.pin=secret(pin);e.password=secret(password);if(!otp->IsEmpty()){auto raw=otp->GetValue().ToUTF8();e.otp=parse_otpauth(std::string_view(raw.data(),raw.length()));otp->ChangeValue({});}e.confirmation_code=utf8(confirm->GetValue());confirm->ChangeValue({});m_lock->create(std::move(e),seconds());m_hooks.notify(text(m_hooks,"The element is now locked."));close();});});
    }
    action(scroll,body,m_hooks,"Forgotten your password? Open Support Tickets",[this]{if(m_hooks.open_support){m_lock->cancel();m_hooks.open_support();close();}else m_status->SetLabel(text(m_hooks,"Support Tickets is not registered. The recovery folder above remains available."));});
    action(scroll,body,m_hooks,"Cancel",[this]{close();});
    // Search targets labels only, never entered secrets or generated codes.
    auto refilter=[this,scroll,search_field]{if(search_field->GetValue().empty())return;SearchField::MatchPass match(search_field->GetValue(),search_field->IsRegexEnabled(),search_field->IsCaseSensitive(),search_field->IsWholeWord(),search_field->IsMultiline());wxString matches;for(auto child:scroll->GetChildren()){if(child==search_field||child==m_status)continue;auto label=dynamic_cast<Label*>(child);if(label&&match.matches(label->GetUnwrappedLabel())){if(!matches.empty())matches+="\n";matches+=label->GetUnwrappedLabel();}}m_status->SetLabel(matches.empty()?text(m_hooks,"No matching settings."):text(m_hooks,"Matching settings: ")+matches);scroll->Layout();scroll->FitInside();};search_field->SetOnQuery([refilter](const wxString&){refilter();});search_field->SetOnRegexToggle([refilter](bool){refilter();});
    scroll->SetSizer(body);root->Add(scroll,1,wxEXPAND|wxALL,FromDIP(12));SetSizer(root);SetSize(FromDIP(wxSize(500,680)));MD3DialogCaption::FinishChrome(this);
    auto display=wxDisplay::GetFromWindow(anchor);wxRect bounds=display==wxNOT_FOUND?wxRect(wxPoint(0,0),wxGetDisplaySize()):wxDisplay(display).GetClientArea();auto desired=anchor->GetScreenRect().GetBottomRight();auto size=GetSize();size.x=std::min(size.x,bounds.width);size.y=std::min(size.y,bounds.height);SetSize(size);Move(std::max(bounds.x,std::min(desired.x,bounds.GetRight()-size.x)),std::max(bounds.y,std::min(desired.y,bounds.GetBottom()-size.y)));
    Bind(wxEVT_CLOSE_WINDOW,[this](wxCloseEvent&){close();});Bind(wxEVT_BUTTON,[this](wxCommandEvent&){close();},wxID_CANCEL);register_all(this,m_hooks,"local-security.lock-wizard");
}
void LockWizard::safely(const std::function<void()>& fn){try{fn();}catch(const Failure& e){m_status->SetLabel(failure(m_hooks,e));}catch(...){m_status->SetLabel(text(m_hooks,"The local operation did not complete. No success was recorded."));}}
void LockWizard::refresh_prompt(){auto state=m_lock->attempts(now());if(state.wait_seconds){m_status->SetLabel(wxString::Format("%u ",state.wait_seconds)+text(m_hooks,"seconds before another attempt. The recovery folder remains available."));return;}auto f=m_lock->expected(now());m_status->SetLabel(text(m_hooks,!f?"Already unlocked.":*f==Factor::Pin?"Enter this element's PIN.":*f==Factor::Password?"Enter this element's password.":"Enter this element's current TOTP code."));}
void LockWizard::close(){m_lock->cancel();if(m_answer)m_answer->ChangeValue({});if(m_anchor)m_anchor->SetFocus();Destroy();}

SupportTicketsPanel::SupportTicketsPanel(wxWindow* parent,std::shared_ptr<SupportTickets> store,std::filesystem::path folder,Hooks hooks)
    :wxPanel(parent),m_store(std::move(store)),m_folder(std::move(folder)),m_hooks(std::move(hooks)){
    require_hooks(m_hooks);if(!m_store||!m_folder.is_absolute())throw Failure(Error::InvalidInput);auto root=new wxBoxSizer(wxVERTICAL);auto scroll=new wxScrolledWindow(this);scroll->SetScrollRate(0,FromDIP(12));auto body=new wxBoxSizer(wxVERTICAL);
    // This disclosure is factual, outside the comedic-response formatter.
    body->Add(factual_label(scroll,m_hooks,"Nothing is sent anywhere. No ticket exists outside this machine. No network request is made, no data is collected, and nobody is reading it."),0,wxEXPAND|wxBOTTOM,FromDIP(8));
    m_search=search(scroll,body,m_hooks,"Search local support tickets",[this]{refresh();});m_list=new wxListBox(scroll,wxID_ANY,wxDefaultPosition,FromDIP(wxSize(350,180)),0,nullptr,wxLB_EXTENDED);m_list->SetName(text(m_hooks,"Local support tickets"));m_hooks.record_name(m_list,"Local support tickets");body->Add(m_list,0,wxEXPAND|wxBOTTOM,FromDIP(8));
    const char* categories[]={"Forgotten answer","Missing authenticator","Local data reset"};std::vector<wxRadioButton*> category;for(unsigned i=0;i<3;++i){auto b=new wxRadioButton(scroll,wxID_ANY,text(m_hooks,categories[i]),wxDefaultPosition,wxDefaultSize,i==0?wxRB_GROUP:0);m_hooks.record_label(b,categories[i]);category.push_back(b);body->Add(b,0,wxEXPAND|wxBOTTOM,FromDIP(6));}category[0]->SetValue(true);
    auto description=field(scroll,body,m_hooks,"Description (do not include credentials)");description->SetMaxLength(160);auto severity=new wxSpinCtrl(scroll,wxID_ANY,"1",wxDefaultPosition,wxDefaultSize,wxSP_ARROW_KEYS,1,5,1);severity->SetName(text(m_hooks,"Fictional severity"));m_hooks.record_name(severity,"Fictional severity");body->Add(severity,0,wxEXPAND|wxBOTTOM,FromDIP(8));
    action(scroll,body,m_hooks,"Create local ticket",[this,category,description,severity]{safely([&]{unsigned c=0;for(unsigned i=0;i<category.size();++i)if(category[i]->GetValue())c=i;m_store->create(static_cast<TicketCategory>(c),severity->GetValue(),utf8(description->GetValue()));description->ChangeValue({});refresh();});});
    action(scroll,body,m_hooks,"Advance selected tickets",[this]{safely([this]{wxArrayInt rows;m_list->GetSelections(rows);if(rows.empty())throw Failure(Error::InvalidInput);for(auto row:rows)m_store->advance(m_visible.at(row));refresh();m_status->SetLabel(text(m_hooks,"The local desk reviewed the manual. Resolution: open the application-data folder and delete it yourself to reset."));});});
    action(scroll,body,m_hooks,"Remove selected tickets",[this]{safely([this]{wxArrayInt rows;m_list->GetSelections(rows);if(rows.empty())throw Failure(Error::InvalidInput);std::vector<std::string> ids;SuperConfirmGate::Spec spec;spec.action=text(m_hooks,"Remove local tickets");spec.consequence=text(m_hooks,"Selected local ticket records will be removed. Application data will not be deleted.");for(auto row:rows){ids.push_back(m_visible.at(row));spec.affected.push_back(m_list->GetString(row));}wxWeakRef<SupportTicketsPanel> self(this);SuperConfirmGate::Show(m_list,spec,[self,ids]{if(self)self->safely([&]{for(const auto& id:ids)self->m_store->remove(id);self->refresh();});});});});
    auto path=new TextInput(scroll,wxString(m_folder.wstring()),{}, {},wxDefaultPosition,wxDefaultSize,wxTE_READONLY);path->GetTextCtrl()->SetName(text(m_hooks,"Application-data folder"));m_hooks.record_name(path->GetTextCtrl(),"Application-data folder");body->Add(path,0,wxEXPAND|wxBOTTOM,FromDIP(8));
    action(scroll,body,m_hooks,"Copy recovery folder",[this]{safely([this]{copy(wxString(m_folder.wstring()));});});
    action(scroll,body,m_hooks,"Open application-data folder",[this]{safely([this]{if(!std::filesystem::is_directory(m_folder)||!wxLaunchDefaultApplication(wxString(m_folder.wstring())))throw Failure(Error::Unavailable);m_status->SetLabel(text(m_hooks,"The folder was opened. This application did not delete anything."));});});
    auto export_button=action(scroll,body,m_hooks,"Export local tickets",[this]{safely([this]{if(!m_hooks.export_text)throw Failure(Error::Unavailable);m_hooks.export_text(text(m_hooks,"Local support tickets"),m_store->export_text());});});if(!m_hooks.export_text){export_button->Enable(false);export_button->SetToolTip(text(m_hooks,"The shared export service is unavailable."));m_hooks.record_tooltip(export_button,"The shared export service is unavailable.");}
    m_status=new Label(scroll,text(m_hooks,"Up to eight local tickets are retained. Descriptions are limited to 160 UTF-8 bytes."),LB_AUTO_WRAP);body->Add(m_status,0,wxEXPAND);scroll->SetSizer(body);root->Add(scroll,1,wxEXPAND|wxALL,FromDIP(12));SetSizer(root);register_all(this,m_hooks,"local-security.support-tickets");refresh();
}
void SupportTicketsPanel::safely(const std::function<void()>& fn){try{fn();}catch(const Failure& e){m_status->SetLabel(failure(m_hooks,e));m_hooks.notify(failure(m_hooks,e));}catch(...){m_status->SetLabel(text(m_hooks,"The local operation did not complete. No success was recorded."));}}
void SupportTicketsPanel::refresh(){safely([this]{m_list->Clear();m_visible.clear();SearchField::MatchPass match(m_search->GetValue(),m_search->IsRegexEnabled(),m_search->IsCaseSensitive(),m_search->IsWholeWord(),m_search->IsMultiline());for(const auto& t:m_store->list()){auto label=wxString::FromUTF8(t.id+" | "+t.description+" | "+std::to_string(static_cast<unsigned>(t.stage)));if(match.matches(label)){m_list->Append(label);m_visible.push_back(t.id);}}if(m_visible.empty())m_status->SetLabel(text(m_hooks,"No matching local tickets."));});}
}
