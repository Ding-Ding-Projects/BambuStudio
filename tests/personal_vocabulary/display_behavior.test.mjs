import {readFileSync, writeFileSync, mkdtempSync} from 'node:fs';
import {execFileSync, spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

// Execute the actual production matcher and canvas label adapters with a small
// string/measurement shim. This does not prove wxWidgets painting or persistence.
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const baseline = process.argv.find(a => a.startsWith('--baseline='))?.slice(11);
const source = p => baseline ? execFileSync('git', ['show', `${baseline}:${p}`], {cwd: root, encoding:'utf8'}) : readFileSync(path.join(root,p),'utf8');
function method(text, signature) {
  const start=text.indexOf(signature);
  if(start<0) throw new Error('Missing production function: '+signature);
  const tokens=/\/\*[\s\S]*?\*\/|\/\/[^\n]*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|[{}]/g;
  tokens.lastIndex=text.indexOf('{',start);
  let depth=0;
  for(let m;(m=tokens.exec(text));) {
    if(m[0]==='{') depth++;
    if(m[0]==='}' && --depth===0) return text.slice(start,m.index+1);
  }
  throw new Error('Unterminated production function');
}
const pv=source('src/slic3r/GUI/PersonalVocabulary.cpp');
const imgui=source('src/slic3r/GUI/ImGuiWrapper.cpp');
const imguiCore=execFileSync('git',['show',`${baseline??'HEAD'}:src/imgui/imgui.cpp`],{cwd:root,encoding:'utf8',maxBuffer:4*1024*1024});
const crcTable=imguiCore.match(/static const ImU32 GCrc32LookupTable\[256\]\s*=\s*\{[\s\S]*?\};/)[0];
const functions=[...(pv.includes('wchar_t fold_ascii(')?[method(pv,'wchar_t fold_ascii(')]:[]),method(pv,'FirstUnitIndex index_entries('),method(pv,'wxString apply_indexed(')];
const fixture=String.raw`
#include <algorithm>
#include <cwctype>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>
#include <mutex>
#include <set>
#include <atomic>
struct wxString {
    std::wstring value;
    wxString() = default;
    wxString(const std::wstring &s): value(s) {}
    std::wstring ToStdWstring() const { return value; }
    std::string ToUTF8() const { return std::string(value.begin(),value.end()); }
    bool empty() const { return value.empty(); }
    std::size_t length() const { return value.length(); }
    wxString &operator+=(const wxString &other) { value+=other.value; return *this; }
    wxString &Trim(bool right=true) {
        while(!value.empty() && std::iswspace(right?value.back():value.front())) {
            if(right) value.pop_back(); else value.erase(value.begin());
        }
        return *this;
    }
    static wxString FromUTF8(const char *s) { std::string v(s); return wxString(std::wstring(v.begin(),v.end())); }
};
struct wxMenuItem {
    static wxString GetLabelText(const wxString &s) {
        std::wstring result;
        for(std::size_t i=0;i<s.value.size();++i) {
            if(s.value[i]!=L'&') result+=s.value[i];
            else if(i+1<s.value.size() && s.value[i+1]==L'&') {result+=L'&'; ++i;}
        }
        return wxString(result);
    }
};
std::string into_u8(const wxString &s) { return std::string(s.value.begin(),s.value.end()); }
using Entries=std::vector<std::pair<std::wstring,std::wstring>>;
using FirstUnitIndex=std::unordered_map<wchar_t,std::vector<std::size_t>>;
// MATCHER
Entries active;
namespace PersonalModes { std::atomic<bool> school_presentation_suppressed{false}; }
struct State { std::mutex source_mutex; std::set<std::wstring> sources; Entries entries; FirstUnitIndex index; };
State &state() { static State s; return s; }
// REGISTRATION
namespace PersonalVocabulary {
wxString display(const wxString &s) { return ::display(s); }
}
namespace I18N {
struct Profile { bool is_bilingual() const { return false; } };
Profile language_mode_profile() { return {}; }
wxString bilingual_secondary(const wxString &) { return {}; }
}
namespace ImGui {
struct Point { float x=0; };
struct Style { Point FramePadding; };
Point CalcTextSize(const char *s) { return {float(std::string(s).size())}; }
Style GetStyle() { return {}; }
}
// CANVAS
void mapping(Entries e) {
    std::stable_sort(e.begin(),e.end(),[](const auto &a,const auto &b){return a.first.size()>b.first.size();}); active=e; state().entries=e; state().index=index_entries(e);
}
int failures=0;
void check(bool condition,const char *name) { if(!condition) {std::cerr<<"FAIL: "<<name<<'\n';++failures;} }
std::wstring apply(const wchar_t *text) {return apply_indexed(wxString(text),active,index_entries(active)).value;}
using ImU32=unsigned int;
using ImGuiID=unsigned int;
// HASH
ImGuiID identity(const std::string &text) { return ImHashStr(text.c_str(),0,123); }
int main() {
 remember(wxString::FromUTF8("Open"));
 mapping({{L"open",L"Inspect"},{L"open item",L"Review"},{L"Inspect",L"Again"}});
 check(apply(L"Open OPEN open")==L"Inspect Inspect Inspect","case fallback and nonrecursive output");
 check(apply(L"OPEN ITEM")==L"Review","longest fallback");
 check(apply(L"Opened reopen open_item")==L"Opened reopen open_item","word boundaries");
 check(apply(L"o")==L"o" && apply(L"")==L"" && apply(L"open")==L"Inspect","short and end-of-input bounds");
 check(apply(L"Open/item")==L"Open/item" && apply(L"Open %s")==L"Open %s","technical text unchanged");
 mapping({{L"open",L"Lower"},{L"Open",L"Title"}});
 check(apply(L"open Open OPEN then Open")==L"Lower Title OPEN then Title","exact and conflicting fallback");
 mapping({{L"open",L"Same"},{L"Open",L"Same"}});
 check(apply(L"OPEN")==L"Same","agreeing variants");
 mapping({{L"Open",L"Exact"},{L"open item",L"Longer"}});
 check(apply(L"Open item")==L"Exact item","exact before longer folded source");
 for(const std::string original:{"Open", "Open##hidden", "Open###stable"}) {
   mapping({}); const auto before=bilingual_compact_utf8(original);
   mapping({{L"open",L"Inspect"}}); const auto loaded=bilingual_compact_utf8(original);
   check(loaded.find("Inspect")==0,"canvas mapped visible text");
   check(identity(before)==identity(loaded),"identity stable on load");
   mapping({}); check(identity(before)==identity(bilingual_compact_utf8(original)),"identity stable on clear");
 }
 check(bilingual_compact_utf8("##hidden")=="##hidden" && bilingual_compact_utf8("").empty(),"hidden and empty labels");
 mapping({{L"open",L"Inspect"}});
 check(bilingual_stacked_utf8("Open")=="Inspect","body text uses display adapter");
 check(bilingual_compact_utf8("Open###stable")=="Inspect###stable","explicit identity unchanged");
 check(identity(bilingual_compact_utf8("Open###stable"))==identity("Open###stable"),"historical explicit identity hash");
 const auto safeId=identity(bilingual_compact_utf8("Open"));
 mapping({{L"open",L"Display###pretend"}});
 check(identity(bilingual_compact_utf8("Open"))==safeId,"replacement cannot select identity");
 // COMPOSITION_TESTS
 mapping({{L"open",L"Inspect"}});
 PersonalModes::school_presentation_suppressed.store(true);
 check(bilingual_stacked_utf8("Open")=="Open","suppression preserves original");
 PersonalModes::school_presentation_suppressed.store(false);
 check(bilingual_stacked_utf8("Unregistered Open")=="Unregistered Open","unknown content is not rewritten");
 std::cout<<(failures?"FAIL":"PASS")<<": production matcher and canvas behavior\n";
 return failures?1:0;
}
`;
const registration=[method(pv,'wxString remember('),...(pv.includes('wxString remember_composed(')?[method(pv,'wxString remember_composed('),method(pv,'void remember_menu_label(')]:[]),method(pv,'wxString display(')];
const composition=pv.includes('wxString remember_composed(')?String.raw`
 mapping({{L"open",L"Inspect"},{L"close",L"Finish"}});
 const auto first=remember(wxString::FromUTF8("Open panel")), second=remember(wxString::FromUTF8("Close panel"));
 const auto joined=remember_composed({first,second},wxString::FromUTF8("\n"));
 check(display(joined).value==L"Inspect panel\nFinish panel","registered composition");
 const auto unknown=remember_composed({first,wxString::FromUTF8("Customer text")},wxString::FromUTF8("\n"));
 check(display(unknown).value==unknown.value,"unknown fragment not registered");
 const auto separator=remember_composed({first,second},wxString::FromUTF8(" unsafe "));
 check(display(separator).value==separator.value,"untrusted separator not registered");
 mapping({}); check(display(joined).value==joined.value,"clear restores composed source");
 mapping({{L"open",L"Inspect"}});
 remember(wxString::FromUTF8("&Open menu"));
 remember_menu_label(wxString::FromUTF8("&Open menu\tCtrl+O"),wxString::FromUTF8("Open menu"));
 check(display(wxString::FromUTF8("Open menu")).value==L"Inspect menu","registered menu mnemonic and shortcut");
 remember_menu_label(wxString::FromUTF8("&Open customer"),wxString::FromUTF8("Open customer"));
 check(display(wxString::FromUTF8("Open customer")).value==L"Open customer","dynamic menu excluded");
 remember_menu_label(wxString::FromUTF8("&Open menu"),wxString::FromUTF8("Open forged"));
 check(display(wxString::FromUTF8("Open forged")).value==L"Open forged","normalization must match");
`:'';
const generated=fixture.replace('// COMPOSITION_TESTS',composition).replace('// REGISTRATION',registration.join('\n')).replace('// HASH',crcTable+'\n'+method(imguiCore,'ImGuiID ImHashStr(')).replace('// MATCHER',functions.join('\n')).replace('// CANVAS',[
  method(imgui,'static void split_imgui_id('),method(imgui,'static std::string bilingual_stacked_utf8('),method(imgui,'static std::string bilingual_compact_utf8(')
].join('\n'));
const out=mkdtempSync(path.join(tmpdir(),'bambu-wording-behavior-'));
const cpp=path.join(out,'fixture.cpp'),exe=path.join(out,'fixture.exe');
writeFileSync(cpp,generated);
console.log('Production source: '+(baseline??'current working source'));
const compiled=spawnSync(process.env.CXX || 'g++',['-std=c++17','-Wall','-Wextra','-Werror',cpp,'-o',exe],{stdio:'inherit'});
if(compiled.error) throw compiled.error;
if(compiled.status!==0) process.exit(compiled.status??1);
const run=spawnSync(exe,[],{stdio:'inherit'});
if(run.error) throw run.error;
process.exit(run.status??1);
