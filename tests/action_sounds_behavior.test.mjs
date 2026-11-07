import {readFileSync,writeFileSync,mkdtempSync,mkdirSync} from 'node:fs';
import {spawnSync} from 'node:child_process';
import {tmpdir} from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
if(process.platform!=='win32') {
  console.error('UNVERIFIED: this platform-adapter check requires the Windows C++ target; no host audio is used.');
  process.exit(2);
}
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const out=mkdtempSync(path.join(tmpdir(),'bambu-action-sounds-'));
const mutant=process.argv.find(a=>a.startsWith('--mutant='))?.slice(9);
if(mutant&&!['scheduled-cancel','submit-admission'].includes(mutant))throw new Error('Unknown mutation');
mkdirSync(path.join(out,'libslic3r'));
writeFileSync(path.join(out,'GUI_App.hpp'),String.raw`
#pragma once
#include <map>
#include <string>
#include "libslic3r/EffectivePreferences.hpp"
struct FakeConfig {
 std::map<std::string,std::string> stored;
 Slic3r::EffectivePreferences::Overlay overlay;
 std::string get(const std::string&key)const {if(auto v=overlay.read(key))return *v;auto it=stored.find(key);return it==stored.end()?"":it->second;}
 bool replace_effective_preferences(const Slic3r::EffectivePreferences::Values&values){return overlay.replace(values);}
};
struct FakeApp {FakeConfig*app_config;};
inline FakeApp application;
inline FakeApp&wxGetApp(){return application;}
`);
// Only these local fake platform headers are visible to the copied production
// translation unit. They cannot open an audio device or emit a host sound.
writeFileSync(path.join(out,'libslic3r/AppConfig.hpp'),'#pragma once\n');
writeFileSync(path.join(out,'windows.h'),String.raw`
#pragma once
#include <atomic>
#include <cstdint>
#include <chrono>
#include <thread>
using UINT=unsigned;using DWORD=std::uint32_t;using WORD=std::uint16_t;using DWORD_PTR=std::uintptr_t;using HANDLE=void*;
constexpr unsigned MAX_PATH=260,FALSE=0,CALLBACK_EVENT=1,WAVE_MAPPER=0,WAVE_FORMAT_PCM=1,WHDR_DONE=1,MMSYSERR_NOERROR=0;
struct WAVEFORMATEX {WORD wFormatTag,nChannels;DWORD nSamplesPerSec,nAvgBytesPerSec;WORD nBlockAlign,wBitsPerSample,cbSize;};
struct WAVEHDR {char*lpData{};DWORD dwBufferLength{},dwFlags{};};
struct Device {WAVEHDR*header=nullptr;};using HWAVEOUT=Device*;
inline std::atomic<int> writes{0},devices{0},max_devices{0},resets{0};
inline std::atomic<bool> hold_audio{false},device_unavailable{false},block_open{false},opening{false};
enum QUERY_USER_NOTIFICATION_STATE {QUNS_BUSY,QUNS_RUNNING_D3D_FULL_SCREEN,QUNS_PRESENTATION_MODE,QUNS_QUIET_TIME,QUNS_NOT_PRESENT,QUNS_ACCEPTS_NOTIFICATIONS,QUNS_APP};
inline QUERY_USER_NOTIFICATION_STATE notification=QUNS_ACCEPTS_NOTIFICATIONS;
#define SUCCEEDED(x) ((x)>=0)
inline int SHQueryUserNotificationState(QUERY_USER_NOTIFICATION_STATE*s){*s=notification;return 0;}
inline UINT GetWindowsDirectoryW(wchar_t*p,UINT){p[0]=L'.';p[1]=0;return 1;}
inline HANDLE CreateEventW(void*,int,int,void*){return new int(0);}
inline void CloseHandle(HANDLE h){delete static_cast<int*>(h);}
inline int WaitForSingleObject(HANDLE,unsigned){std::this_thread::sleep_for(std::chrono::milliseconds(1));return 0;}
inline int waveOutOpen(HWAVEOUT*p,unsigned,WAVEFORMATEX*,DWORD_PTR,DWORD_PTR,unsigned){opening=true;while(block_open)std::this_thread::sleep_for(std::chrono::milliseconds(1));if(device_unavailable)return 1;*p=new Device;const int n=++devices;if(n>max_devices)max_devices=n;return 0;}
inline int waveOutPrepareHeader(HWAVEOUT,WAVEHDR*,unsigned){return 0;}
inline int waveOutWrite(HWAVEOUT d,WAVEHDR*h,unsigned){d->header=h;++writes;if(!hold_audio)h->dwFlags|=WHDR_DONE;return 0;}
inline int waveOutReset(HWAVEOUT d){++resets;if(d->header)d->header->dwFlags|=WHDR_DONE;return 0;}
inline int waveOutUnprepareHeader(HWAVEOUT,WAVEHDR*,unsigned){return 0;}
inline int waveOutClose(HWAVEOUT d){delete d;--devices;return 0;}
`);
for(const name of ['mmsystem.h','shellapi.h'])writeFileSync(path.join(out,name),'#include "windows.h"\n');
let production=readFileSync(path.join(root,'src/slic3r/GUI/ActionSounds.cpp'),'utf8').replaceAll('#pragma comment(lib, "winmm.lib")','');
if(mutant==='submit-admission')production=production.replace('if (generation.load() == expected && admitted(prefs, minute_now(), os_quiet()))','if (true)');
writeFileSync(path.join(out,'ActionSounds.cpp'),production);
const scheduler=readFileSync(path.join(root,'src/slic3r/GUI/FeatureServices/ScheduledPreferences.cpp'),'utf8');
let schedulePrefix=scheduler.slice(scheduler.indexOf('void ScheduledPreferences::apply('),scheduler.indexOf('    m_applying = true;',scheduler.indexOf('void ScheduledPreferences::apply(')))+'}\n';
if(mutant==='scheduled-cancel')schedulePrefix=schedulePrefix.replace('    ActionSounds::settings_changed();','');
writeFileSync(path.join(out,'fixture.cpp'),String.raw`
#include "ActionSounds.cpp"
#include "libslic3r/PresentationDefaults.hpp"
#include <iostream>
#include <cassert>
using namespace Slic3r::GUI::ActionSounds;
namespace ActionSounds=Slic3r::GUI::ActionSounds;
namespace EffectivePreferences=Slic3r::EffectivePreferences;
namespace ScheduledSettings {struct Effective {EffectivePreferences::Values values;std::map<std::string,int>winners;};}
struct ScheduledPreferences {FakeConfig&m_config;EffectivePreferences::Values m_last;bool m_applying=false;void notify(const char*){}void apply(const ScheduledSettings::Effective&);};
// SCHEDULE_PREFIX
void wait_until(const std::function<bool()>&condition){for(int i=0;i<1000&&!condition();++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(condition());}
void wav(const std::filesystem::path&p){std::ofstream f(p,std::ios::binary);auto u32=[&](std::uint32_t n){for(int i=0;i<4;++i)f.put(char(n>>(8*i)));};auto u16=[&](unsigned n){f.put(char(n));f.put(char(n>>8));};f.write("RIFF",4);u32(196);f.write("WAVEfmt ",8);u32(16);u16(1);u16(1);u32(8000);u32(16000);u16(2);u16(16);f.write("data",4);u32(160);for(int i=0;i<160;++i)f.put(0);}
int main(){
 FakeConfig cfg;application.app_config=&cfg;
 for(const auto&e:Slic3r::PresentationDefaults::entries())cfg.stored[e.key]=e.value;
 assert(preferences().enabled);
 notification=QUNS_APP;assert(os_quiet());notification=QUNS_ACCEPTS_NOTIFICATIONS;
 Preferences p;p.hours={true,22*60,7*60};assert(!admitted(p,23*60,false));assert(!admitted(p,6*60,false));assert(admitted(p,12*60,false));p.quiet=true;assert(!admitted(p,12*60,false));p.quiet=false;assert(!admitted(p,12*60,true));p.respect_focus=false;assert(admitted(p,12*60,true));p.enabled=false;assert(!admitted(p,12*60,false));
 CueQueue queue;for(std::size_t i=0;i<CueQueue::capacity;++i)assert(queue.append(i%2?Cue::Rejected:Cue::VocabularyApplied));assert(!queue.append(Cue::Rejected));for(std::size_t i=0;i<CueQueue::capacity;++i)assert(queue.take()==(i%2?Cue::Rejected:Cue::VocabularyApplied));assert(!queue.take());queue.append(Cue::Rejected);queue.cancel();assert(!queue.take());
 std::filesystem::create_directory("Media");for(const auto*name:{"tada.wav","Windows Critical Stop.wav","Windows Notify System Generic.wav"})wav(std::filesystem::path("Media")/name);
 assert(!read_cue(Cue::VocabularyApplied).samples.empty());
 play(Cue::VocabularyApplied);play(Cue::Rejected);play(Cue::WordingCleared);wait_until([]{return writes==3&&devices==0;});assert(max_devices==1);
 cfg.stored[enabled_key]="false";settings_changed();play(Cue::Rejected);std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(writes==3);
 // Restart reads the stored setting through the production settings reader.
 {FakeConfig reloaded;reloaded.stored=cfg.stored;application.app_config=&reloaded;assert(!preferences().enabled);}
 application.app_config=&cfg;cfg.stored[enabled_key]="true";
 assert(cfg.overlay.replace({{enabled_key,false}}));play(Cue::Rejected);std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(writes==3);cfg.overlay.clear();
 hold_audio=true;play(Cue::VocabularyApplied);wait_until([]{return writes==4;});play(Cue::Rejected);cfg.stored[enabled_key]="false";settings_changed();wait_until([]{return devices==0;});hold_audio=false;std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(writes==4);
 cfg.stored[enabled_key]="true";device_unavailable=true;play(Cue::Rejected);std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(writes==4&&devices==0);device_unavailable=false;
 {std::ofstream f("Media/tada.wav",std::ios::binary);f<<"invalid";}play(Cue::VocabularyApplied);std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(writes==4);
 wav("Media/tada.wav");cfg.stored["narrator_quiet"]="true";play(Cue::VocabularyApplied);std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(writes==4);cfg.stored["narrator_quiet"]="false";
 play(Cue::VocabularyApplied);play(Cue::VocabularyApplied);wait_until([]{return writes==6&&devices==0;});
 hold_audio=true;play(Cue::VocabularyApplied);wait_until([]{return writes==7;});play(Cue::Rejected);
 ScheduledPreferences scheduler{cfg,{},false};scheduler.apply({{{enabled_key,false}},{{enabled_key,1}}});wait_until([]{return devices==0;});hold_audio=false;std::this_thread::sleep_for(std::chrono::milliseconds(30));assert(writes==7);cfg.overlay.clear();
 block_open=true;opening=false;play(Cue::VocabularyApplied);wait_until([]{return opening.load();});cfg.stored[enabled_key]="false";settings_changed();block_open=false;std::this_thread::sleep_for(std::chrono::milliseconds(50));assert(writes==7&&devices==0);
 std::cout<<"ActionSounds: admission, real worker, ordered cues, cancellation, effective settings, restart reader, malformed WAV and unavailable backend passed; no host audio.\n";
}
`.replace('// SCHEDULE_PREFIX',schedulePrefix));
const exe=path.join(out,'fixture.exe');
const result=spawnSync(process.env.CXX||'g++',['-std=c++17','-Wall','-Wextra','-Werror','-Wno-missing-field-initializers','-pthread','-include','functional','-I'+out,'-I'+path.join(root,'src/slic3r/GUI'),'-I'+path.join(root,'src'),path.join(out,'fixture.cpp'),'-o',exe],{stdio:'inherit'});
if(result.error)throw result.error;if(result.status!==0)process.exit(result.status??1);
const run=spawnSync(exe,[],{cwd:out,stdio:'inherit',timeout:15000});if(run.error)throw run.error;process.exit(run.status??1);
