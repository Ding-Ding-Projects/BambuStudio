import {readFileSync, writeFileSync, mkdtempSync} from 'node:fs';
import {execFileSync, spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

// Execute production orchestration with deterministic loader/config/UI doubles.
// This proves transitions, not archive parsing, disk persistence or rendering.
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const baseline=process.argv.find(a=>a.startsWith('--baseline='))?.slice(11);
const source=baseline ? execFileSync('git',['show',baseline+':src/slic3r/GUI/MainFrame.cpp'],{cwd:root,encoding:'utf8'})
  : readFileSync(path.join(root,'src/slic3r/GUI/MainFrame.cpp'),'utf8');
function method(signature) {
  const start=source.indexOf(signature);
  if(start<0) throw new Error('Missing production method: '+signature);
  const tokens=/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]/g;
  tokens.lastIndex=source.indexOf('{',start);
  let depth=0;
  for(let m;(m=tokens.exec(source));) {
    if(m[0]==='{') depth++;
    if(m[0]==='}' && --depth===0) return source.slice(start,m.index+1);
  }
  throw new Error('Unterminated method');
}
const fixture=String.raw`
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <functional>
#include <fstream>
#include <nlohmann/json.hpp>
struct wxString:std::string { using std::string::string; wxString()=default; wxString(const std::string&s):std::string(s){} bool IsEmpty()const{return empty();} static wxString FromUTF8(const std::string&s){return s;} };
#define _L(s) wxString(s)
#define BOOST_LOG_TRIVIAL(x) std::cerr
constexpr int wxID_CANCEL=1,wxOK=2,wxICON_WARNING=4;
int config_id=0;
struct AppConfig {
 std::map<std::string,std::string> values,persisted;
 bool writes=true;std::string filename="config-"+std::to_string(++config_id)+".json";
 std::string config_path(){return filename;}
 std::string get(const std::string&s,const std::string&k){return values[s+":"+k];}
 void set(const std::string&s,const std::string&k,const std::string&v){values[s+":"+k]=v;}
 void save(){if(!writes)return;persisted=values;std::ofstream f(filename);nlohmann::json j;j["project_tab_recovery"]["pending_path"]=get("project_tab_recovery","pending_path");f<<j<<"\n# checksum trailer\n";}
};
struct App {AppConfig*app_config;}; App app; App&wxGetApp(){return app;}
bool path_exists=true;
namespace boost {namespace nowide {using ifstream=std::ifstream;} namespace system {struct error_code{};} namespace filesystem {using path=std::string;bool exists(const path&,system::error_code&){return path_exists;}}}
std::string into_u8(const wxString&s){return s;}
struct ProjectTab {std::string file_path,snapshot_path;wxString title;bool dirty=false;};
struct ProjectTabBar {
 std::vector<ProjectTab> tabs,saved;int active=0,saved_active=-1;
 int GetActive(){return active;} int Count(){return int(tabs.size());} void SetActive(int i){active=i;}
 ProjectTab&TabAt(int i){return tabs.at(i);}
 int AddTab(const std::string&p,const wxString&t,bool activate){tabs.push_back({p,"",t,false});if(activate)active=Count()-1;return Count()-1;}
 void SetActiveTitle(const wxString&s){TabAt(active).title=s;} void SetActiveDirty(bool d){TabAt(active).dirty=d;}
 void SaveToConfig(){saved=tabs;saved_active=active;}
};
struct Plater {
 struct Model {std::vector<int> objects;} content;
 Model&model(){return content;}bool dirty=false,cancel_reset=false;bool is_project_dirty(){return dirty;}
 wxString filename,name; bool succeeds=false,throws=false; int loads=0,snapshot_loads=0,ordinary_loads=0;std::function<void()> on_load;
 wxString get_project_filename(const wxString&extension=""){return filename.empty()?wxString():wxString(filename+extension);} wxString get_project_name(){return name;}
 bool load(const std::string&p){++loads;if(on_load)on_load();filename="";name="";if(throws)throw std::runtime_error("synthetic");if(succeeds){set_project_filename(p);name="Recovered";}return succeeds;}
 bool load_snapshot_from(const std::string&p){++snapshot_loads;try{return load(p);}catch(...){return false;}}
 void load_project(const wxString&p,const char*mode,bool*success,bool){if(std::string(mode)!="-")throw std::runtime_error("wrong mode");++ordinary_loads;*success=load(p);}
 int new_project(bool,bool){if(cancel_reset)return wxID_CANCEL;filename="";name="";return 0;}
 void set_project_filename(const wxString&p){filename=p;const auto dot=filename.rfind('.');if(dot!=std::string::npos)filename.resize(dot);}
};
int dialogs=0;
struct MessageDialog {template<class T> MessageDialog(T*,const wxString&,const wxString&,int){}void ShowModal(){++dialogs;}};
struct TabOpGuard {bool&flag;explicit TabOpGuard(bool&f):flag(f){flag=true;}~TabOpGuard(){flag=false;}};
struct MainFrame {
 Plater*m_plater;ProjectTabBar*m_project_tabbar;bool m_project_tabs_ready=false,m_project_tab_switching=false;
 void reconcile_initial_project_tab();void switch_project_tab(int);
 bool save_active_tab_snapshot_if_dirty(){return true;}
 void update_title(){if(m_project_tabs_ready&&!m_project_tab_switching&&m_project_tabbar->GetActive()>=0)m_project_tabbar->SetActiveTitle(m_plater->name.empty()?_L("Untitled"):m_plater->name);}
};
// PRODUCTION
int failures=0;void check(bool value,const char*name){if(!value){std::cerr<<"FAIL "<<name<<'\n';++failures;}}
struct Scenario {
 AppConfig cfg;Plater plater;ProjectTabBar tabs;MainFrame frame{&plater,&tabs};
 Scenario(){app.app_config=&cfg;path_exists=true;dialogs=0;tabs.AddTab("model.3mf","Retained",true);}
 void pending(){cfg.set("project_tab_recovery","pending_path","model.3mf");cfg.save();}
};
int main(){
 {Scenario s;bool recorded=false;s.plater.on_load=[&]{recorded=s.cfg.persisted["project_tab_recovery:pending_path"]=="model.3mf";s.frame.update_title();};s.frame.reconcile_initial_project_tab();
  check(recorded,"pending marker saved before loader");check(s.tabs.Count()==2&&s.tabs.active==1,"failed restore gets independent empty tab");check(s.tabs.tabs[0].file_path=="model.3mf"&&s.tabs.tabs[0].title=="Retained","failed target identity retained during callbacks");check(s.tabs.saved_active==1,"empty active tab persisted");check(s.plater.filename.empty(),"failed file never owns Save");
  const int before=s.plater.loads;s.frame.reconcile_initial_project_tab();check(s.plater.loads==before,"empty recovery tab does not retry");
  s.plater.succeeds=true;s.frame.switch_project_tab(0);check(s.tabs.active==0&&s.plater.get_project_filename(".3mf")=="model.3mf","retained tab explicitly retries");check(s.cfg.persisted["project_tab_recovery:pending_path"].empty(),"successful retry clears marker durably");check(s.plater.snapshot_loads==0,"saved files use ordinary load semantics");}
 {Scenario s;s.pending();s.frame.reconcile_initial_project_tab();check(s.plater.loads==0&&s.tabs.Count()==2,"interrupted startup is not retried automatically");}
 {Scenario s;s.plater.succeeds=true;s.frame.reconcile_initial_project_tab();check(s.tabs.Count()==1&&s.tabs.active==0&&s.plater.get_project_filename(".3mf")=="model.3mf","valid startup restores directly");check(s.cfg.persisted["project_tab_recovery:pending_path"].empty(),"successful startup clears marker");}
 {Scenario s;s.plater.set_project_filename("command-line.3mf");s.frame.reconcile_initial_project_tab();check(s.plater.loads==0&&s.plater.get_project_filename(".3mf")=="command-line.3mf","command line document is untouched");}
 {Scenario s;path_exists=false;s.frame.reconcile_initial_project_tab();check(s.plater.loads==0&&s.tabs.Count()==2&&s.tabs.tabs[0].file_path=="model.3mf","missing target retained for later retry");}
 {Scenario s;s.plater.throws=true;s.frame.reconcile_initial_project_tab();check(s.tabs.Count()==2&&!s.frame.m_project_tab_switching,"loader exception retains recoverable tab");}
 {Scenario s;s.tabs.tabs[0].snapshot_path="private.3mf";s.plater.succeeds=true;s.frame.reconcile_initial_project_tab();check(s.plater.snapshot_loads==1&&s.plater.get_project_filename(".3mf")=="model.3mf","snapshot restore preserves real identity");}
 {Scenario s;s.tabs.tabs[0].file_path="";s.frame.reconcile_initial_project_tab();check(s.tabs.Count()==1&&s.plater.loads==0&&dialogs==0,"blank startup stays blank");}
 {Scenario s;s.frame.reconcile_initial_project_tab();const int before=s.plater.loads;s.frame.switch_project_tab(0);check(s.plater.loads==before+1&&s.tabs.active==1&&s.tabs.tabs[0].file_path=="model.3mf","failed explicit retry preserves empty outgoing tab and target");}
 {Scenario s;s.plater.cancel_reset=true;s.frame.reconcile_initial_project_tab();check(s.tabs.Count()==1&&s.tabs.active==0&&s.tabs.tabs[0].title=="Retained"&&!s.frame.m_project_tabs_ready,"cancelled reset does not invent an empty tab or enable identity sync");}
 {Scenario s;s.plater.content.objects.push_back(1);s.frame.reconcile_initial_project_tab();check(s.plater.loads==0&&s.tabs.Count()==2&&s.tabs.active==1&&s.tabs.tabs[0].file_path=="model.3mf","unnamed imported geometry is not overwritten");}
 {Scenario s;s.plater.dirty=true;s.frame.reconcile_initial_project_tab();check(s.plater.loads==0&&s.tabs.Count()==2&&s.tabs.tabs[1].dirty,"unnamed dirty document is not overwritten");}
 {Scenario s;s.plater.set_project_filename("other.3mf");s.frame.reconcile_initial_project_tab();check(s.tabs.Count()==2&&s.tabs.tabs[0].file_path=="model.3mf"&&s.tabs.tabs[1].file_path=="other.3mf","existing named document has matching tab identity");}
 {Scenario s;s.plater.set_project_filename("model.3mf");s.frame.reconcile_initial_project_tab();check(s.tabs.Count()==1&&s.plater.loads==0,"matching existing document reuses tab");}
 {Scenario s;s.cfg.writes=false;s.frame.reconcile_initial_project_tab();check(s.plater.loads==0&&s.tabs.Count()==2,"unverified marker persistence prevents automatic load");}
 std::cout<<"startup recovery behavioral failures: "<<failures<<'\n';return failures?1:0;
}
`;
const cpp=fixture.replace('// PRODUCTION',[
 ...(source.includes('bool persist_project_tab_restore_marker(')?[method('bool persist_project_tab_restore_marker(')]:[]),
 method('bool restore_project_tab_document('),
 method('void MainFrame::reconcile_initial_project_tab()'),
 method('void MainFrame::switch_project_tab(int target)'),
].join('\n\n'));
const out=mkdtempSync(path.join(tmpdir(),'bambu-startup-recovery-'));
const file=path.join(out,'fixture.cpp'),exe=path.join(out,'fixture.exe');writeFileSync(file,cpp);
const result=spawnSync(process.env.CXX||'g++',['-std=c++17','-Wall','-Wextra','-Werror','-I'+path.join(root,'src'),file,'-o',exe],{stdio:'inherit'});
if(result.error)throw result.error;
if(result.status!==0)process.exit(result.status??1);
const run=spawnSync(exe,[],{cwd:out,stdio:'inherit'});if(run.error)throw run.error;process.exit(run.status??1);
