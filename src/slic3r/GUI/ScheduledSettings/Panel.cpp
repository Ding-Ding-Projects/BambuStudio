#include "Panel.hpp"
#include "../I18N.hpp"
#include "../Presets/StartFromPicker.hpp"
#include "../Schedule/ScheduleRulePresets.hpp"
#include "../Field.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/TabCtrl.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/LabeledCheckBox.hpp"
#include "../Widgets/ListBox.hpp"
#include "../Widgets/MD3ColorPicker.hpp"
#include "../Widgets/MD3ScrolledWindow.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/TextInput.hpp"
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#include <type_traits>
#include <wx/textctrl.h>
#include <wx/datectrl.h>
#include <wx/dateevt.h>
#include <wx/timectrl.h>
#include <atomic>
#include <algorithm>
#include <cmath>
namespace Slic3r::GUI::ScheduledSettingsUI {
namespace SS=ScheduledSettings;
namespace {
std::string u8(const wxString& s){return std::string(s.utf8_string());}
wxString wxs(const std::string& s){return wxString::FromUTF8(s.c_str());}
SS::Date date(const wxDateTime& d){return {d.GetYear(),int(d.GetMonth())+1,int(d.GetDay())};}
wxDateTime date(const SS::Date& d){return wxDateTime(wxDateTime::wxDateTime_t(d.day),wxDateTime::Month(d.month-1),d.year);}
std::string new_id(){static std::atomic_uint serial{0};return "rule-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+"-"+std::to_string(++serial);}
// The accessible name goes on the kit field and on the native editor inside it,
// which is the window that takes the focus.
void name_editor(wxWindow* control,const wxString& name){control->SetName(name);if(auto* input=dynamic_cast<TextInput*>(control))input->GetTextCtrl()->SetName(name);else if(auto* spin=dynamic_cast<SpinInput*>(control))spin->GetTextCtrl()->SetName(name);}
// A decimal setting on the kit TextInput. Up and Down step by one inside the
// range, as the native spin control it replaces did; leaving the field or Enter
// keeps a typed number (a comma counts as the decimal point, out-of-range values
// are clamped) and restores the last number when the text is not one.
class DecimalField final:public TextInput{
public:
    DecimalField(wxWindow* parent,double minimum,double maximum,double initial):TextInput(parent,wxEmptyString,wxEmptyString,wxEmptyString,wxDefaultPosition,wxDefaultSize,wxTE_PROCESS_ENTER),m_min(minimum),m_max(maximum),m_value(std::clamp(initial,minimum,maximum)){
        show();
        GetTextCtrl()->Bind(wxEVT_KEY_DOWN,[this](wxKeyEvent& event){const int key=event.GetKeyCode();if(key!=WXK_UP&&key!=WXK_DOWN){event.Skip();return;}m_value=std::clamp(value()+(key==WXK_UP?1.0:-1.0),m_min,m_max);show();});
        GetTextCtrl()->Bind(wxEVT_TEXT_ENTER,[this](wxCommandEvent&){commit();});
        GetTextCtrl()->Bind(wxEVT_KILL_FOCUS,[this](wxFocusEvent& event){commit();event.Skip();});
    }
    // The number in the field, clamped to the range; the last number when the text is not one.
    double value() const{wxString text=GetTextCtrl()->GetValue();text.Replace(",",".");text.Trim().Trim(false);double number=m_value;if(!text.ToCDouble(&number)||!std::isfinite(number))return m_value;return std::clamp(number,m_min,m_max);}
private:
    void commit(){m_value=value();show();}
    void show(){GetTextCtrl()->ChangeValue(wxString::FromCDouble(m_value));GetTextCtrl()->SetInsertionPointEnd();}
    double m_min,m_max,m_value;
};
}
wxString Panel::tr(const char* en,const char* zh)const{auto text=m_translate?m_translate(en,zh):wxString::FromUTF8(en);m_copy[text]={en,zh};return text;}
Panel::Panel(wxWindow* parent,SS::Service& service,Translate translate,Visible visible):wxPanel(parent),m_service(service),m_translate(std::move(translate)),m_visible(std::move(visible)),m_draft(service.schedule()),m_live_timer(this){
    SetName(tr("Scheduled settings","排程設定"));
    auto* root=new wxBoxSizer(wxVERTICAL);SetSizer(root);
    auto* scroll=new MD3ScrolledWindow(this,wxID_ANY);scroll->SetScrollRate(0,FromDIP(12));root->Add(scroll,1,wxEXPAND);
    auto* content=new wxBoxSizer(wxVERTICAL);scroll->SetSizer(content);
    auto caption=[&](wxWindow* p,wxSizer* s,const char* en,const char* zh){auto* t=new Label(p,tr(en,zh));t->Wrap(FromDIP(540));s->Add(t,0,wxEXPAND|wxALL,FromDIP(6));};
    caption(scroll,content,"Schedules temporarily override your saved preferences. Higher priority wins; the later rule wins ties. Equal times mean all day. Overnight rules belong to the day they start.","排程只會暫時覆蓋已儲存偏好。優先次序較高嘅先用；同級用較後嘅規則。開始同結束時間一樣即全日。跨夜時段計開始嗰日。");
    m_search=new SearchField(scroll,tr("Search rules","搜尋規則"));content->Add(m_search,0,wxEXPAND|wxALL,FromDIP(6));
    m_rules=new ListBox(scroll,wxID_ANY,FromDIP(wxSize(300,110)),wxLB_MULTIPLE);m_rules->SetMinSize(FromDIP(wxSize(-1,110)));m_rules->SetName(tr("Scheduled rules, select several for bulk actions","排程規則，可多選批量操作"));content->Add(m_rules,0,wxEXPAND|wxALL,FromDIP(6));
    auto* actions=new wxWrapSizer(wxHORIZONTAL);content->Add(actions,0,wxEXPAND);
    auto button=[&](wxWindow* p,wxSizer* s,const char* en,const char* zh,std::function<void()> action){auto* b=new Button(p,tr(en,zh));b->SetVariant(Button::Variant::Outlined);b->SetMinSize(FromDIP(wxSize(-1,44)));b->Bind(wxEVT_BUTTON,[action](wxCommandEvent&){action();});s->Add(b,0,wxALL,FromDIP(4));return b;};
    button(scroll,actions,"Add rule","新增規則",[this]{if(!retain_editor())return;add_rule();});
    button(scroll,actions,"Duplicate","複製",[this]{if(!retain_editor())return;auto it=std::find_if(m_draft.rules.begin(),m_draft.rules.end(),[&](const auto& r){return r.id==m_selected;});if(it==m_draft.rules.end())return;auto r=*it;r.id=new_id();r.label+=" (2)";m_selected=r.id;m_draft.rules.push_back(r);rebuild_rules();load_editor();});
    auto bulk=[this](int mode){if(!retain_editor())return;wxArrayInt rows;m_rules->GetSelections(rows);std::vector<std::string> ids;for(int row:rows)if(size_t(row)<m_rule_ids.size())ids.push_back(m_rule_ids[size_t(row)]);for(auto& r:m_draft.rules)if(std::find(ids.begin(),ids.end(),r.id)!=ids.end())r.enabled=mode==1;if(mode==2)m_draft.rules.erase(std::remove_if(m_draft.rules.begin(),m_draft.rules.end(),[&](const auto& r){return std::find(ids.begin(),ids.end(),r.id)!=ids.end();}),m_draft.rules.end());rebuild_rules();load_editor();};
    button(scroll,actions,"Enable selected","啟用已選",[bulk]{bulk(1);});button(scroll,actions,"Disable selected","停用已選",[bulk]{bulk(0);});button(scroll,actions,"Remove selected","移除已選",[bulk]{bulk(2);});
    auto* tabs=new TabCtrl(scroll,wxID_ANY);tabs->SetNavItemStyle(true);content->Add(tabs,0,wxEXPAND|wxALL,FromDIP(6));
    const char* tab_en[]={"Time and dates","Settings","Source","Timezone"};const char* tab_zh[]={"時間同日期","設定","來源","時區"};
    std::vector<wxPanel*> pages;
    for(int i=0;i<4;++i){tabs->AppendItem(tr(tab_en[i],tab_zh[i]));auto* p=new wxPanel(scroll);p->SetSizer(new wxBoxSizer(wxVERTICAL));content->Add(p,0,wxEXPAND);p->Show(i==0);pages.push_back(p);}
    tabs->Bind(wxEVT_TAB_SEL_CHANGED,[pages,content,scroll](wxCommandEvent& e){int selected=e.GetInt();for(size_t i=0;i<pages.size();++i)pages[i]->Show(int(i)==selected);content->Layout();scroll->FitInside();});tabs->SelectItem(0);
    auto field=[&](wxWindow* p,wxSizer* s,wxWindow* control,const char* en,const char* zh){caption(p,s,en,zh);name_editor(control,tr(en,zh));control->SetMinSize(wxSize(-1,FromDIP(40)));s->Add(control,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,FromDIP(6));};
    // A kit TextInput field; the rule reads and writes the native editor inside it.
    auto text_field=[&](wxWindow* p,wxSizer* s,long style,const char* en,const char* zh){auto* box=new TextInput(p,wxEmptyString,wxEmptyString,wxEmptyString,wxDefaultPosition,wxDefaultSize,style);field(p,s,box,en,zh);return box->GetTextCtrl();};
    auto* p=pages[0];auto* ps=p->GetSizer();m_label=text_field(p,ps,0,"Rule name (stored locally)","規則名稱（只儲存喺本機）");m_label->SetMaxLength(160);
    m_enabled=new LabeledCheckBox(p,tr("Enabled","啟用"));ps->Add(m_enabled,0,wxALL,FromDIP(6));
    m_priority=new SpinInput(p,wxEmptyString,wxEmptyString,wxDefaultPosition,wxDefaultSize,0,0,10000,0);field(p,ps,m_priority,"Priority, 0 to 10000; default 0","優先次序，0 至 10000；預設 0");
    m_first=new wxDatePickerCtrl(p,wxID_ANY,wxDefaultDateTime,wxDefaultPosition,wxDefaultSize,wxDP_DEFAULT|wxDP_ALLOWNONE);field(p,ps,m_first,"First start date, blank means no lower bound","首個開始日期，留空即不限開始日");
    m_last=new wxDatePickerCtrl(p,wxID_ANY,wxDefaultDateTime,wxDefaultPosition,wxDefaultSize,wxDP_DEFAULT|wxDP_ALLOWNONE);field(p,ps,m_last,"Last start date, blank means no upper bound","最後開始日期，留空即不限結束日");
    m_start=new wxTimePickerCtrl(p,wxID_ANY);field(p,ps,m_start,"Start time, inclusive","開始時間，包含此刻");m_end=new wxTimePickerCtrl(p,wxID_ANY);field(p,ps,m_end,"End time, exclusive","結束時間，不包含此刻");
    m_every_day=new LabeledCheckBox(p,tr("Every day","每日"));ps->Add(m_every_day,0,wxALL,FromDIP(6));auto* days=new wxWrapSizer(wxHORIZONTAL);ps->Add(days,0,wxEXPAND);
    const char* en[]={"Monday","Tuesday","Wednesday","Thursday","Friday","Saturday","Sunday"};const char* zh[]={"星期一","星期二","星期三","星期四","星期五","星期六","星期日"};for(int i=0;i<7;++i){m_days[i]=new LabeledCheckBox(p,tr(en[i],zh[i]));days->Add(m_days[i],0,wxALL,FromDIP(6));}
    m_every_day->Bind(wxEVT_CHECKBOX,[this](wxCommandEvent&){for(auto* d:m_days){d->Enable(!m_every_day->GetValue());d->SetToolTip(m_every_day->GetValue()?tr("Every day already includes this day","每日已包括呢日"):wxString());}});
    p=pages[1];ps=p->GetSizer();caption(p,ps,"Select a setting, edit its value, then add it to the rule. Values apply only during this rule; saved base preferences are preserved.","揀設定、改數值，再加入規則。只會喺規則生效期間套用，原有偏好會保留。");
    m_setting_search=new SearchField(p,tr("Search available settings","搜尋可用設定"));ps->Add(m_setting_search,0,wxEXPAND|wxALL,FromDIP(6));m_settings=new ListBox(p,wxID_ANY);field(p,ps,m_settings,"Available settings","可用設定");m_settings->SetMinSize(FromDIP(wxSize(-1,160)));
    m_value_host=new wxPanel(p);m_value_host->SetSizer(new wxBoxSizer(wxVERTICAL));ps->Add(m_value_host,0,wxEXPAND);
    button(p,ps,"Use value in rule","將數值加入規則",[this]{auto row=m_settings->GetSelection();if(row==wxNOT_FOUND||size_t(row)>=m_setting_keys.size()||!m_value)return;auto it=std::find_if(m_draft.rules.begin(),m_draft.rules.end(),[&](const auto& r){return r.id==m_selected;});if(it==m_draft.rules.end())return;const auto key=m_setting_keys[size_t(row)];const auto& d=m_service.registry().entries().at(key);SS::Value value;
        if(d.kind==SS::Kind::Boolean)value=static_cast<LabeledCheckBox*>(m_value)->GetValue();else if(d.kind==SS::Kind::Integer)value=int64_t(static_cast<SpinInput*>(m_value)->GetValue());else if(d.kind==SS::Kind::Number)value=static_cast<DecimalField*>(m_value)->value();else if(d.kind==SS::Kind::Color)value=u8(static_cast<ColourSwatchButton*>(m_value)->GetColour().GetAsString(wxC2S_HTML_SYNTAX));else if(d.kind==SS::Kind::Choice){int n=static_cast<ListBox*>(m_value)->GetSelection();if(n==wxNOT_FOUND||size_t(n)>=m_choice_rows.size())return;value=d.choices[m_choice_rows[size_t(n)]];}else value=u8(static_cast<TextInput*>(m_value)->GetTextCtrl()->GetValue());std::string error;if(!m_service.registry().validate({{key,value}},error)){show_status(tr("Value is outside the allowed range.","數值超出容許範圍。"));return;}it->values[key]=std::move(value);show_status(tr("Value added to the draft. Save schedules to apply.","數值已加入草稿。儲存排程後先套用。"));});
    button(p,ps,"Remove setting from rule","從規則移除設定",[this]{auto row=m_settings->GetSelection();if(row==wxNOT_FOUND||size_t(row)>=m_setting_keys.size())return;for(auto& r:m_draft.rules)if(r.id==m_selected)r.values.erase(m_setting_keys[size_t(row)]);setting_editor();});
    // The value source is a kit radio group: a caption, then one row per SourceKind in order.
    p=pages[2];ps=p->GetSizer();caption(p,ps,"Value source","數值來源");const char* source_en[]={"Local values","Versioned HTTPS API","Home Assistant boolean entity"};const char* source_zh[]={"本機數值","有版本嘅 HTTPS API","Home Assistant 布林實體"};
    for(int i=0;i<3;++i){auto* row=new LabeledRadioButton(p,tr(source_en[i],source_zh[i]));m_source.Add(row);ps->Add(row,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,FromDIP(6));}m_source.SetSelection(0);
    m_url=text_field(p,ps,0,"HTTPS endpoint; Home Assistant uses its base URL","HTTPS 端點；Home Assistant 請用基礎網址");m_entity=text_field(p,ps,0,"Boolean entity, binary_sensor.name or input_boolean.name","布林實體，binary_sensor.name 或 input_boolean.name");
    m_consent=new LabeledCheckBox(p,tr("Allow requests to this exact endpoint. No settings are uploaded.","容許連線到呢個指定端點，唔會上傳設定。"));ps->Add(m_consent,0,wxALL,FromDIP(6));m_private=new LabeledCheckBox(p,tr("Allow private network Home Assistant only","只容許私人網絡嘅 Home Assistant"));ps->Add(m_private,0,wxALL,FromDIP(6));m_loopback=new LabeledCheckBox(p,tr("Allow loopback development HTTP only","只容許本機開發用 HTTP"));ps->Add(m_loopback,0,wxALL,FromDIP(6));
    m_interval=new SpinInput(p,wxEmptyString,wxEmptyString,wxDefaultPosition,wxDefaultSize,0,30,86400,30);field(p,ps,m_interval,"Refresh seconds, 30 to 86400; default 300","更新秒數，30 至 86400；預設 300");
    caption(p,ps,"Off, offline, expired, invalid and unauthorized sources fall back to saved local preferences or another matching rule. Requests stop after ten seconds, reject redirects, and never send your settings.","來源關閉、離線、過期、無效或未授權時，會用返本機偏好或另一條適用規則。請求十秒後停止，唔跟重新導向，亦唔會傳送偏好。");
    m_secret=text_field(p,ps,wxTE_PASSWORD,"Home Assistant access token, stored only in the credential vault","Home Assistant 存取憑證，只儲存喺憑證保管庫");m_secret->SetMaxLength(2048);
    button(p,ps,"Store credential","儲存憑證",[this]{auto secret=m_secret->GetValue().ToStdWstring();const bool ok=!m_selected.empty()&&SS::store_home_credential(m_selected,secret);std::fill(secret.begin(),secret.end(),wchar_t(0));m_secret->ChangeValue(wxEmptyString);show_status(ok?tr("Credential stored locally.","憑證已儲存喺本機。"):tr("Credential could not be stored.","未能儲存憑證。"));});
    button(p,ps,"Clear credential","清除憑證",[this]{const bool ok=!m_selected.empty()&&SS::remove_home_credential(m_selected);m_secret->ChangeValue(wxEmptyString);m_service.refresh();show_status(ok?tr("Credential cleared.","憑證已清除。"):tr("Credential could not be cleared.","未能清除憑證。"));});
    p=pages[3];ps=p->GetSizer();caption(p,ps,"Times use the selected timezone. Missing daylight-saving minutes never run; repeated minutes match both occurrences. Changing the system timezone is picked up at the next evaluation.","時間按所選時區計算。夏令時間跳過嘅分鐘唔會執行；重複嘅分鐘兩次都適用。系統時區變更會喺下次評估生效。");m_zone_search=new SearchField(p,tr("Search timezones","搜尋時區"));ps->Add(m_zone_search,0,wxEXPAND|wxALL,FromDIP(6));m_zones=new ListBox(p,wxID_ANY,FromDIP(wxSize(300,180)));field(p,ps,m_zones,"Configured local timezone","設定嘅本地時區");m_zones->SetMinSize(FromDIP(wxSize(-1,180)));m_zone_values=SS::timezones();zones_filter();m_zones->Bind(wxEVT_LISTBOX,[this](wxCommandEvent&){int n=m_zones->GetSelection();if(n!=wxNOT_FOUND&&size_t(n)<m_zone_indices.size())m_draft.timezone=m_zone_values[m_zone_indices[size_t(n)]].id;});
    m_live=new Label(scroll,wxEmptyString);m_live->SetName(tr("Effective schedule source state","目前排程來源狀態"));content->Add(m_live,0,wxEXPAND|wxALL,FromDIP(6));
    Bind(wxEVT_TIMER,[this](wxTimerEvent&){
        const auto it=m_service.effective().sources.find(m_selected);const auto state=it==m_service.effective().sources.end()?SS::State::Inactive:it->second;
        wxString status;
        switch(state){
        case SS::State::Local:status=tr("Local rule active","本機規則生效中");break;
        case SS::State::Waiting:status=tr("Waiting for external source; local preferences remain active","等緊外部來源，本機偏好繼續生效");break;
        case SS::State::Active:status=tr("External source validated and active","外部來源已驗證並生效");break;
        case SS::State::Inactive:status=tr("Rule is inactive; other rules or base preferences apply","規則未生效，會用其他規則或原有偏好");break;
        case SS::State::Offline:status=tr("Source is offline. Retry external sources when connected.","來源離線，連線後可重試外部來源。");break;
        case SS::State::Invalid:status=tr("Source returned invalid settings. Correct the source and retry.","來源傳回無效設定，請修正來源再試。");break;
        case SS::State::Unauthorized:status=tr("Source needs consent or a valid credential.","來源需要同意或有效憑證。");break;
        case SS::State::RateLimited:status=tr("Source is rate limited. The bounded retry interval still applies.","來源限制請求次數，會按設定間隔重試。");break;
        case SS::State::Stale:status=tr("Source response expired; local preferences restored.","來源回覆已過期，已恢復本機偏好。");break;
        default:status=tr("External transport is unavailable in this build.","呢個版本未能使用外部連線。");break;
        }
        const auto zone=m_service.schedule().timezone;status+=tr("\nActive timezone: ","\n目前時區：")+(zone=="system"?tr("System local time","系統本地時間"):wxs(zone));
        if(m_live->GetUnwrappedLabel()!=status){m_live->SetLabel(status);m_live->Wrap(FromDIP(540));Layout();}
    },m_live_timer.GetId());m_live_timer.Start(1000);
    m_url->Bind(wxEVT_TEXT,[this](wxCommandEvent& event){m_consent->SetValue(false);event.Skip();});m_entity->Bind(wxEVT_TEXT,[this](wxCommandEvent& event){m_consent->SetValue(false);event.Skip();});for(int i=0;i<m_source.GetCount();++i)m_source.Get(i)->Bind(wxEVT_RADIOBUTTON,[this](wxCommandEvent& event){m_consent->SetValue(false);event.Skip();});
    m_status=new Label(scroll,wxEmptyString);m_status->SetName(tr("Schedule status","排程狀態"));content->Add(m_status,0,wxEXPAND|wxALL,FromDIP(6));auto* bottom=new wxWrapSizer(wxHORIZONTAL);content->Add(bottom,0,wxEXPAND);button(scroll,bottom,"Save schedules","儲存排程",[this]{save();});button(scroll,bottom,"Reload saved schedules","重新載入已儲存排程",[this]{m_draft=m_service.schedule();m_selected.clear();rebuild_rules();load_editor();zones_filter();});button(scroll,bottom,"Retry external sources","重試外部來源",[this]{m_service.refresh();show_status(tr("Refresh queued for the next evaluation.","已排隊，下次評估會更新。"));});
    m_rules->Bind(wxEVT_LISTBOX,[this](wxCommandEvent&){select_rule();});m_settings->Bind(wxEVT_LISTBOX,[this](wxCommandEvent&){setting_editor();});
    m_search->SetOnQuery([this](const wxString&){rebuild_rules();});m_search->SetOnRegexToggle([this](bool){rebuild_rules();});m_setting_search->SetOnQuery([this](const wxString&){settings_filter();});m_setting_search->SetOnRegexToggle([this](bool){settings_filter();});m_zone_search->SetOnQuery([this](const wxString&){zones_filter();});m_zone_search->SetOnRegexToggle([this](bool){zones_filter();});
    rebuild_rules();load_editor();settings_filter();scroll->FitInside();
}
void Panel::show_status(const wxString& text){m_status->SetLabel(text);m_status->Wrap(FromDIP(540));Layout();}
void Panel::rebuild_rules(){m_rules->Clear();m_rule_ids.clear();SearchField::MatchPass pass(m_search->GetValue(),m_search->IsRegexEnabled(),m_search->IsCaseSensitive(),m_search->IsWholeWord(),m_search->IsMultiline());for(const auto& r:m_draft.rules){auto label=wxs(r.label)+(r.enabled?tr(" [enabled]"," [啟用]"):tr(" [disabled]"," [停用]"));if(pass.matches(label)){m_rule_ids.push_back(r.id);m_rules->Append(label);if(r.id==m_selected)m_rules->SetSelection(int(m_rule_ids.size()-1));}}}
void Panel::select_rule(){wxArrayInt selection;m_rules->GetSelections(selection);if(selection.empty())return;const auto next=m_rule_ids[size_t(selection[0])];if(next==m_selected)return;for(auto& r:m_draft.rules)if(r.id==m_selected){auto draft=r;if(!read_editor(draft))return;r=std::move(draft);}m_selected=next;load_editor();}
void Panel::add_rule(){
    // A new rule starts from the shipped defaults, the saved base settings the
    // service evaluates, or nothing (Presets/StartFromPicker): never from a
    // value invented here.
    namespace RP=ScheduleRulePresets;
    const auto spec=RP::service_rule_spec(m_service.registry(),m_visible);
    StartFromPicker picker(this,_L(RP::picker_title()),_L(RP::picker_subtitle()),BlankEditorPresets::start_presets(spec,RP::encode_values(m_service.base())));
    if(picker.ShowModal()!=wxID_OK||!picker.chosen())return;
    const BlankEditorPresets::Preset preset=*picker.chosen();
    SS::Rule r;r.id=new_id();r.label=u8(tr("New schedule","新排程"));
    if(!RP::decode_values(m_service.registry(),preset.values(),r.values)){show_status(tr("Value is outside the allowed range.","數值超出容許範圍。"));return;}
    m_selected=r.id;m_draft.rules.push_back(r);m_preset_rules[r.id]=preset.title;rebuild_rules();load_editor();
    // State exactly what the preset set, right after applying it.
    show_status(preset_applied_note(preset)+"\n"+tr("Editing a local draft. Save schedules to apply.","編輯緊本機草稿。儲存排程後先套用。"));
}
void Panel::load_editor(){auto it=std::find_if(m_draft.rules.begin(),m_draft.rules.end(),[&](const auto& r){return r.id==m_selected;});if(it==m_draft.rules.end()){m_selected.clear();m_label->ChangeValue(wxEmptyString);show_status(tr("Select or add a rule. Empty schedules preserve saved preferences.","請選取或新增規則。空排程會保留已儲存偏好。"));return;}const auto& r=*it;m_label->ChangeValue(wxs(r.label));m_enabled->SetValue(r.enabled);m_priority->SetValue(r.priority);m_first->SetValue(r.window.first?date(*r.window.first):wxInvalidDateTime);m_last->SetValue(r.window.last?date(*r.window.last):wxInvalidDateTime);auto t=wxDateTime::Today();t.SetHour(r.window.start_minute/60);t.SetMinute(r.window.start_minute%60);t.SetSecond(0);m_start->SetValue(t);t.SetHour(r.window.end_minute/60);t.SetMinute(r.window.end_minute%60);m_end->SetValue(t);m_every_day->SetValue(r.window.weekdays==127);for(int i=0;i<7;++i){m_days[i]->SetValue((r.window.weekdays&(1u<<i))!=0);m_days[i]->Enable(r.window.weekdays!=127);}m_source.SetSelection(int(r.source.kind));m_url->ChangeValue(wxs(r.source.url));m_entity->ChangeValue(wxs(r.source.entity));m_consent->SetValue(r.source.consent);m_private->SetValue(r.source.allow_private_network);m_loopback->SetValue(r.source.loopback_development);m_interval->SetValue(r.source.refresh_seconds);m_secret->ChangeValue(wxEmptyString);setting_editor();show_status(tr("Editing a local draft. Save schedules to apply.","編輯緊本機草稿。儲存排程後先套用。"));}
bool Panel::read_editor(SS::Rule& r){r.label=u8(m_label->GetValue());r.enabled=m_enabled->GetValue();r.priority=m_priority->GetValue();const auto first=m_first->GetValue(),last=m_last->GetValue(),start=m_start->GetValue(),end=m_end->GetValue();if(!start.IsValid()||!end.IsValid()){show_status(tr("Enter complete start and end times.","請輸入完整開始同結束時間。"));return false;}r.window.first=first.IsValid()?std::optional<SS::Date>(date(first)):std::nullopt;r.window.last=last.IsValid()?std::optional<SS::Date>(date(last)):std::nullopt;r.window.start_minute=int(start.GetHour())*60+start.GetMinute();r.window.end_minute=int(end.GetHour())*60+end.GetMinute();r.window.weekdays=0;if(m_every_day->GetValue())r.window.weekdays=127;else for(int i=0;i<7;++i)if(m_days[i]->GetValue())r.window.weekdays|=1u<<i;r.source.kind=SS::SourceKind(std::max(0,m_source.GetSelection()));if(r.source.kind==SS::SourceKind::Local)r.source=SS::Source{};else r.source={r.source.kind,u8(m_url->GetValue()),u8(m_entity->GetValue()),m_consent->GetValue(),m_private->GetValue(),m_loopback->GetValue(),m_interval->GetValue()};SS::Schedule test;test.rules={r};std::string error;if(!SS::validate(test,m_service.registry(),error)){show_status(tr("Check the rule name, weekdays, dates, settings and source. Invalid input has been kept for correction.","請檢查名稱、星期、日期、設定同來源。無效輸入會保留，方便修正。"));return false;}return true;}
bool Panel::retain_editor(){for(auto& r:m_draft.rules)if(r.id==m_selected){auto draft=r;if(!read_editor(draft))return false;r=std::move(draft);}return true;}
void Panel::save(){auto draft=m_draft;for(auto& r:draft.rules)if(r.id==m_selected&&!read_editor(r))return;std::string error;std::string label;for(const auto& r:draft.rules){const auto it=m_preset_rules.find(r.id);if(it!=m_preset_rules.end()){label=BlankEditorPresets::history_label(ScheduleRulePresets::history_action(),it->second);break;}}m_service.label_next_change(label);if(!m_service.replace(draft,error)){m_service.take_change_label();show_status(tr("Schedules were not saved. Check the input or storage and retry.","排程未儲存。請檢查輸入或儲存空間再試。"));return;}m_preset_rules.clear();m_draft=std::move(draft);SS::LocalTime now;if(SS::local_time(m_draft.timezone,now))m_service.tick(now,std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());rebuild_rules();show_status(tr("Schedules saved. Base preferences remain unchanged.","排程已儲存。原有偏好保持不變。"));}
void Panel::settings_filter(){m_settings->Clear();m_setting_keys.clear();SearchField::MatchPass pass(m_setting_search->GetValue(),m_setting_search->IsRegexEnabled(),m_setting_search->IsCaseSensitive(),m_setting_search->IsWholeWord(),m_setting_search->IsMultiline());for(const auto& p:m_service.registry().entries()){if(m_visible&&!m_visible(p.first))continue;auto label=tr(p.second.label_en.c_str(),p.second.label_zh.c_str());if(pass.matches(label+" "+wxs(p.first))){m_setting_keys.push_back(p.first);m_settings->Append(label);}}if(!m_setting_keys.empty())m_settings->SetSelection(0);setting_editor();}
void Panel::setting_editor(){m_value_host->GetSizer()->Clear(true);m_value=nullptr;m_choice_rows.clear();int row=m_settings->GetSelection();if(row==wxNOT_FOUND||size_t(row)>=m_setting_keys.size())return;const auto key=m_setting_keys[size_t(row)];const auto& d=m_service.registry().entries().at(key);SS::Value value=std::string();bool found=false;for(const auto& r:m_draft.rules)if(r.id==m_selected){auto it=r.values.find(key);if(it!=r.values.end()){value=it->second;found=true;}}auto* s=m_value_host->GetSizer();auto label=tr(d.label_en.c_str(),d.label_zh.c_str());if(d.kind==SS::Kind::Boolean){auto* c=new LabeledCheckBox(m_value_host,label);c->SetValue(found&&std::get<bool>(value));m_value=c;}else if(d.kind==SS::Kind::Integer){m_value=new SpinInput(m_value_host,wxEmptyString,wxEmptyString,wxDefaultPosition,wxDefaultSize,0,int(d.minimum),int(d.maximum),found?int(std::get<int64_t>(value)):int(d.minimum));}else if(d.kind==SS::Kind::Number){m_value=new DecimalField(m_value_host,d.minimum,d.maximum,found?std::visit([](const auto& x)->double{using T=std::decay_t<decltype(x)>;if constexpr(std::is_arithmetic_v<T>)return double(x);else return 0;},value):d.minimum);}else if(d.kind==SS::Kind::Color){
        // The kit swatch opens the Material colour picker; the setting stores an opaque colour.
        auto* c=new ColourSwatchButton(m_value_host,found?wxColour(wxs(std::get<std::string>(value))):wxColour("#6750A4"),wxSize(FromDIP(96),FromDIP(44)));c->Bind(wxEVT_BUTTON,[c,label](wxCommandEvent&){MD3ColorPickerDialog::Options options;options.opacity=false;options.title=label;MD3ColorPickerDialog dialog(wxGetTopLevelParent(c),c->GetColour(),options);if(dialog.ShowModal()!=wxID_OK)return;const wxColour picked=dialog.GetColour();if(picked.IsOk())c->SetColour(picked);});m_value=c;
    }else if(d.kind==SS::Kind::Choice){auto* search=new SearchField(m_value_host,tr("Search setting choices","搜尋設定選項"));s->Add(search,0,wxEXPAND|wxALL,FromDIP(6));auto* c=new ListBox(m_value_host,wxID_ANY);auto filter=[this,c,search,choices=d.choices](const wxString&){const int selected=c->GetSelection();const size_t previous=selected==wxNOT_FOUND||size_t(selected)>=m_choice_rows.size()?0:m_choice_rows[size_t(selected)];c->Clear();m_choice_rows.clear();SearchField::MatchPass pass(search->GetValue(),search->IsRegexEnabled(),search->IsCaseSensitive(),search->IsWholeWord(),search->IsMultiline());for(size_t i=0;i<choices.size();++i)if(pass.matches(wxs(choices[i]))){m_choice_rows.push_back(i);c->Append(wxs(choices[i]));if(i==previous)c->SetSelection(int(m_choice_rows.size()-1));}if(c->GetSelection()==wxNOT_FOUND&&c->GetCount())c->SetSelection(0);};filter(wxEmptyString);if(found){const auto& chosen=std::get<std::string>(value);for(size_t row=0;row<m_choice_rows.size();++row)if(d.choices[m_choice_rows[row]]==chosen){c->SetSelection(int(row));break;}}search->SetOnQuery(filter);search->SetOnRegexToggle([filter](bool){filter(wxEmptyString);});m_value=c;}else{auto* c=new TextInput(m_value_host,found?wxs(std::get<std::string>(value)):wxString(),wxEmptyString,wxEmptyString);c->GetTextCtrl()->SetMaxLength(d.max_length);m_value=c;}name_editor(m_value,label);m_value->SetMinSize(wxSize(-1,FromDIP(d.kind==SS::Kind::Choice?160:44)));s->Add(m_value,0,wxEXPAND|wxALL,FromDIP(6));auto* provenance=new Label(m_value_host,found?tr("Value from this local rule","數值來自呢條本機規則"):tr("Not in this rule. Choose a value and add it explicitly.","未加入呢條規則。請揀數值再明確加入。"));provenance->Wrap(FromDIP(500));s->Add(provenance,0,wxEXPAND|wxALL,FromDIP(6));m_value_host->Layout();Layout();}
void Panel::zones_filter(){m_zones->Clear();m_zone_indices.clear();SearchField::MatchPass pass(m_zone_search->GetValue(),m_zone_search->IsRegexEnabled(),m_zone_search->IsCaseSensitive(),m_zone_search->IsWholeWord(),m_zone_search->IsMultiline());for(size_t i=0;i<m_zone_values.size();++i){const auto& z=m_zone_values[i];auto label=z.id=="system"?tr("System local time","系統本地時間"):wxs(z.label+" ("+z.id+")");if(pass.matches(label)){m_zone_indices.push_back(i);m_zones->Append(label);if(z.id==m_draft.timezone)m_zones->SetSelection(int(m_zone_indices.size()-1));}}}
void Panel::refresh_presentation(){
    const auto previous=m_copy;
    auto translated=[&](const wxString& old){auto it=previous.find(old);return it==previous.end()?old:tr(it->second.first.c_str(),it->second.second.c_str());};
    std::function<void(wxWindow*)> visit=[&](wxWindow* w){
        w->SetName(translated(w->GetName()));
        // Kit rows keep their text in a child Label, and Wrap() rewrites a label's
        // native text, so both are translated from the text as it was set.
        if(auto* check=dynamic_cast<LabeledCheckBox*>(w)){check->SetLabel(translated(check->GetLabelCtrl()->GetUnwrappedLabel()));return;}
        if(auto* radio=dynamic_cast<LabeledRadioButton*>(w)){radio->SetLabel(translated(radio->GetLabelCtrl()->GetUnwrappedLabel()));return;}
        if(auto* text=dynamic_cast<Label*>(w)){const int wrap=text->GetWrapWidth();text->SetLabel(translated(text->GetUnwrappedLabel()));if(wrap>0)text->Wrap(wrap);return;}
        // Editable text, typed values and list rows are data, never localization keys.
        const bool data=dynamic_cast<wxTextCtrl*>(w)||dynamic_cast<ListBox*>(w)||dynamic_cast<TextInput*>(w)||dynamic_cast<SpinInput*>(w);
        if(!data&&!w->GetLabel().empty())w->SetLabel(translated(w->GetLabel()));
        if(auto* tabs=dynamic_cast<TabCtrl*>(w))for(unsigned i=0;i<tabs->GetCount();++i)tabs->SetItemText(i,translated(tabs->GetItemText(i)));
        for(auto* child:w->GetChildren())visit(child);
    };
    visit(this);
    m_search->SetPlaceholder(tr("Search rules","搜尋規則"));m_setting_search->SetPlaceholder(tr("Search available settings","搜尋可用設定"));m_zone_search->SetPlaceholder(tr("Search timezones","搜尋時區"));
    settings_filter();rebuild_rules();zones_filter();Layout();
}
Driver::Driver(SS::Service& s):m_service(s),m_timer(this){Bind(wxEVT_TIMER,[this](wxTimerEvent&){pulse();});m_timer.Start(1000);pulse();}
Driver::~Driver(){m_timer.Stop();}
bool Driver::pulse(){SS::LocalTime now;if(!SS::local_time(m_service.schedule().timezone,now))return false;m_service.tick(now,std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count());return true;}
}
