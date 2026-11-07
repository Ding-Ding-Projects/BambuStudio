import {readFileSync, writeFileSync, mkdtempSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'../..');
const pv=readFileSync(path.join(root,'src/slic3r/GUI/PersonalVocabulary.cpp'),'utf8');
const profiles=readFileSync(path.join(root,'src/slic3r/GUI/ConfigProfilesDialog.cpp'),'utf8');
const archive=readFileSync(path.join(root,'src/slic3r/GUI/ConfigProfileArchive.cpp'),'utf8');
function method(text, signature) {
 const start=text.indexOf(signature); if(start<0) throw Error(signature);
 const tokens=/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]/g;
 tokens.lastIndex=text.indexOf('{',start); let depth=0;
 for(let m;(m=tokens.exec(text));) {if(m[0]==='{') depth++; if(m[0]==='}'&&--depth===0)return text.slice(start,m.index+1);}
 throw Error('unterminated');
}
// Actual persistence and initialization functions; parsing and repaint are doubles.
// This fixture cannot prove JSON validation or rendered UI behavior.
const cpp=String.raw`
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <atomic>
#include <chrono>
#include <cassert>
namespace fs=std::filesystem;
enum class Rejection {None,Unreadable,TooLarge,CacheUnavailable,Invalid};
using Entries=std::vector<std::string>;
constexpr size_t max_file_bytes=262144;
struct State {fs::path cache_directory; bool initialized=false,cache_loaded=false; Entries entries; std::vector<int> index; Rejection cache_rejection=Rejection::None;};
State value; State &state(){return value;}
bool wxTheApp=true;
std::vector<int> index_entries(const Entries&){return {};}
void refresh(){}
bool parse(const std::string &s,Entries &out,Rejection *why) {if(s!="neutral-A"&&s!="neutral-B"){*why=Rejection::Invalid;return false;}out={s};return true;}
struct PendingFile {fs::path path; ~PendingFile(){if(!path.empty()){std::error_code ec;fs::remove(path,ec);}}};
// FUNCTIONS
namespace PersonalVocabulary {using ::is_reserved_cache_relative; using ::is_cache_path;}
struct wxString {std::string value; wxString()=default; wxString(const char*s):value(s){} static wxString FromUTF8(const std::string&s){wxString v;v.value=s;return v;}};
#define _L(s) wxString(s)
std::vector<std::string> archived;
struct wxFFileOutputStream {wxFFileOutputStream(wxString){} bool IsOk(){return true;}bool Close(){return true;}};
struct wxFFileInputStream {wxFFileInputStream(wxString){}bool IsOk(){return true;}};
struct wxZipOutputStream {wxZipOutputStream(wxFFileOutputStream&,int){} void PutNextEntry(wxString s){archived.push_back(s.value);}void Write(wxFFileInputStream&){}bool Close(){return true;}};
// EXPORT
void put(const fs::path&p,const std::string&s){fs::create_directories(p.parent_path());std::ofstream(p)<<s;}
std::string get(const fs::path&p){std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char**argv){assert(argc==2);fs::path root=argv[1],def=root/"default",legacy=root/"legacy",a=root/"a",b=root/"b";
fs::create_directories(def);fs::create_directories(a);fs::create_directories(b);
const auto old=legacy/"private-display"/"vocabulary.json";put(old,"neutral-A");
initialize();assert(!value.initialized&&!value.cache_loaded);Rejection why;assert(!load(old,&why)&&why==Rejection::CacheUnavailable);assert(!clear());
configure_cache_directory(def/".",def,legacy);initialize();assert(value.cache_loaded&&value.entries[0]=="neutral-A");
assert(select_cache_directory(def,def,legacy)==legacy/"private-display");
value={};configure_cache_directory(a,def,legacy);initialize();assert(!value.cache_loaded);assert(!fs::exists(a/"private-display"));
const auto input=root/"input.json";put(input,"neutral-B");assert(load(input,&why));assert(get(old)=="neutral-A");assert(get(a/"private-display"/"vocabulary.json")=="neutral-B");
configure_cache_directory(b,def,legacy);assert(value.cache_directory==a/"private-display");
value={};configure_cache_directory(a,def,legacy);initialize();assert(value.entries[0]=="neutral-B");assert(clear());assert(get(old)=="neutral-A");
value={};configure_cache_directory(b,def,legacy);initialize();assert(!value.cache_loaded);assert(load(input,&why));
value={};configure_cache_directory(a,def,legacy);put(a/"private-display"/"vocabulary.json","corrupt");initialize();assert(!value.cache_loaded&&value.cache_rejection==Rejection::Invalid);assert(get(old)=="neutral-A");assert(get(b/"private-display"/"vocabulary.json")=="neutral-B");
const auto blocked=root/"blocked";put(blocked,"file");value={};configure_cache_directory(blocked,def,legacy);assert(!load(input,&why)&&why==Rejection::CacheUnavailable);assert(get(old)=="neutral-A");
for(const auto &name:{"private-display/vocabulary.json","PRIVATE-DISPLAY/pending","private-display"}) {assert(is_reserved_cache_relative(name));assert(!safe_relative(name));}
value={};configure_cache_directory(b,def,legacy);
put(a/"settings.json","setting");put(a/"private-display"/"pending.tmp","neutral");
int skipped=0;assert(zip_directory(a,root/"out.zip",&skipped).value.empty());
assert(archived.size()==1&&archived[0]=="settings.json");
assert(!is_reserved_cache_relative("user/settings.json"));assert(safe_relative("user/settings.json"));
}
`;
const names=['std::filesystem::path cache_path(','std::filesystem::path select_cache_directory(','void configure_cache_directory(','bool is_reserved_cache_relative(','Rejection read_bounded(','bool write_exclusive(','void initialize(','bool load(','bool clear(','bool is_cache_path('];
let functions=names.map(n=>method(pv,n)).join('\n')+'\n'+method(archive,'bool safe_relative(');
if(process.argv.includes('--mutant=shared-cache')) functions=functions.replace('(same ? legacy_local : active)','(legacy_local)');
const out=mkdtempSync(path.join(tmpdir(),'bambu-cache-isolation-'));writeFileSync(path.join(out,'test.cpp'),cpp.replace('// FUNCTIONS',functions).replace('// EXPORT',method(profiles,'wxString zip_directory(')));
const exe=path.join(out,'test.exe');
let result=spawnSync(process.env.CXX||'g++',['-std=c++17',path.join(out,'test.cpp'),'-o',exe],{stdio:'inherit'});if(result.status!==0)process.exit(result.status??1);
result=spawnSync(exe,[path.join(out,'data')],{stdio:'inherit'});if(result.status!==0)process.exit(result.status??1);
console.log('PASS: production cache selection, initialization, persistence, isolation and reserved archive paths (parser/UI doubles)');
