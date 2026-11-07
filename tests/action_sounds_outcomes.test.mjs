import {readFileSync,writeFileSync,mkdtempSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
function body(text,start){const open=text.indexOf('{',start);let depth=0;const re=/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]/g;re.lastIndex=open;for(let m;(m=re.exec(text));){if(m[0]==='{')depth++;if(m[0]==='}'&&--depth===0)return text.slice(open,m.index+1);}throw new Error('Missing callback body');}
const prefs=readFileSync(path.join(root,'src/slic3r/GUI/Preferences.cpp'),'utf8');
const drop=readFileSync(path.join(root,'src/slic3r/GUI/VocabularyDrop.cpp'),'utf8');
const loadBody=body(prefs,prefs.indexOf('wording_load->Bind('));
const clearBody=body(prefs,prefs.indexOf('wording_clear->Bind('));
const dropBody=body(drop,drop.indexOf('void apply_dropped('));
const predicate=body(drop,drop.indexOf('bool is_vocabulary_file('));
const cpp=String.raw`
#include "slic3r/GUI/ActionSounds.hpp"
#include <filesystem>
#include <atomic>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <vector>
using namespace Slic3r::GUI;
struct wxString:std::string {using std::string::string;wxString()=default;wxString(const std::string&s):std::string(s){}std::wstring ToStdWstring()const{return std::wstring(begin(),end());}wxString Lower()const{auto s=*this;std::transform(s.begin(),s.end(),s.begin(),[](char c){return char(std::tolower(static_cast<unsigned char>(c)));});return s;}bool EndsWith(const char*s)const{const std::string tail(s);return size()>=tail.size()&&compare(size()-tail.size(),tail.size(),tail)==0;}};
using wxArrayString=std::vector<wxString>;struct wxWindow{};
constexpr int wxID_OK=1,wxFD_OPEN=2,wxFD_FILE_MUST_EXIST=4;
const wxString wxEmptyString;
#define _L(x) wxString(x)
#define L(x) x
#define BOOST_LOG_TRIVIAL(x) std::cerr
bool accepted=false,completed=false,cancel_picker=false;int calls=0,notices=0,celebrations=0;
std::vector<ActionSounds::Cue> cues;
namespace Slic3r::GUI::PersonalModes {std::atomic<bool> school_presentation_suppressed{false};}
namespace Slic3r::GUI::PersonalVocabulary {enum class Rejection{None,Invalid};bool load(const std::filesystem::path&,Rejection*r){++calls;completed=accepted;*r=accepted?Rejection::None:Rejection::Invalid;return accepted;}bool clear(){++calls;completed=accepted;return accepted;}}
namespace Slic3r::GUI::ActionSounds {void play(Cue c)noexcept{if(c!=Cue::Rejected)assert(completed);cues.push_back(c);}}
namespace VocabularyDrop {wxString rejection_text(PersonalVocabulary::Rejection){return "Invalid";}}
struct wxFileDialog {template<class T>wxFileDialog(T*,const wxString&,const wxString&,const wxString&,const wxString&,int){}int ShowModal(){return cancel_picker?0:wxID_OK;}wxString GetPath(){return "neutral.json";}};
wxString localize(const char*s){return s;}wxString rejection_copy(PersonalVocabulary::Rejection){return "Invalid";}
void show_refusal(wxWindow*,const wxString&){++notices;}void show_celebration(wxWindow*){++celebrations;}
struct PreferencesFixture {void refresh_wording(const wxString&){++notices;}void load()LOAD_BODY void clear()CLEAR_BODY};
void apply_dropped(wxWindow*frame,const wxString&file)DROP_BODY
bool is_vocabulary_file(const wxArrayString&files)PREDICATE
void reset(bool success){accepted=success;completed=false;calls=0;notices=0;celebrations=0;cues.clear();cancel_picker=false;PersonalModes::school_presentation_suppressed=false;}
int main(){PreferencesFixture p;wxWindow window;
 for(bool ok:{false,true}){reset(ok);apply_dropped(&window,"neutral.json");assert(calls==1&&cues.size()==1);assert(cues[0]==(ok?ActionSounds::Cue::VocabularyApplied:ActionSounds::Cue::Rejected));assert(celebrations==(ok?1:0));}
 reset(true);apply_dropped(&window,"neutral.json");apply_dropped(&window,"neutral.json");assert(calls==2&&cues.size()==2&&celebrations==2);
 for(bool ok:{false,true}){reset(ok);p.load();assert(calls==1&&cues.size()==1&&notices==1);assert(cues[0]==(ok?ActionSounds::Cue::VocabularyApplied:ActionSounds::Cue::Rejected));reset(ok);p.clear();assert(calls==1&&cues.size()==1&&notices==1);assert(cues[0]==(ok?ActionSounds::Cue::WordingCleared:ActionSounds::Cue::Rejected));}
 reset(true);cancel_picker=true;p.load();assert(calls==0&&cues.empty());
 reset(true);PersonalModes::school_presentation_suppressed=true;p.load();p.clear();apply_dropped(&window,"neutral.json");assert(calls==0&&cues.empty());
 assert(is_vocabulary_file({"neutral.JSON"}));assert(!is_vocabulary_file({"model.3mf"}));assert(!is_vocabulary_file({"model.stl"}));assert(!is_vocabulary_file({"a.json","b.json"}));
 std::cout<<"Production outcome callbacks: exactly one cue after each result, cancellation, School suppression and model-drop classification passed.\n";
}
`.replace('LOAD_BODY',loadBody).replace('CLEAR_BODY',clearBody).replace('DROP_BODY',dropBody).replace('PREDICATE',predicate);
const out=mkdtempSync(path.join(tmpdir(),'bambu-action-outcomes-')),file=path.join(out,'fixture.cpp'),exe=path.join(out,'fixture.exe');writeFileSync(file,cpp);
const compile=spawnSync(process.env.CXX||'g++',['-std=c++17','-Wall','-Wextra','-Werror','-I'+path.join(root,'src'),file,'-o',exe],{stdio:'inherit'});if(compile.error)throw compile.error;if(compile.status!==0)process.exit(compile.status??1);
const run=spawnSync(exe,[],{cwd:out,stdio:'inherit'});if(run.error)throw run.error;process.exit(run.status??1);
