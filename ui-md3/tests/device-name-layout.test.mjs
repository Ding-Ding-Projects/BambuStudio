import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync, spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const header = path.join(root, 'src/slic3r/GUI/PrintSetupLayout.hpp');
const sourcePath = 'src/slic3r/GUI/SelectMachinePop.cpp';
const gui = process.env.BAMBU_DEVICE_NAME_SOURCE_REF
  ? execFileSync('git', ['show', `${process.env.BAMBU_DEVICE_NAME_SOURCE_REF}:${sourcePath}`], { cwd: root, encoding: 'utf8' })
  : readFileSync(path.join(root, sourcePath), 'utf8');
const labelSource = readFileSync(path.join(root, 'src/slic3r/GUI/Widgets/Label.cpp'), 'utf8');
function definition(text, signature) {
  const start = text.indexOf(signature);
  if (start < 0) return '';
  const body = text.indexOf('{', start);
  const scrubbed = text.slice(body).replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, value => ' '.repeat(value.length));
  let depth = 0;
  for (let i = 0; i < scrubbed.length; i++) {
    if (scrubbed[i] === '{') depth++;
    if (scrubbed[i] === '}' && --depth === 0) return text.slice(start, body + i + 1);
  }
  throw new Error('Unbalanced production definition');
}
const constructor = gui.slice(gui.indexOf('EditDevNameDialog::EditDevNameDialog('), gui.indexOf('EditDevNameDialog::~EditDevNameDialog()'));
const creation = constructor.match(/m_static_valid = new Label\([^;]*;/)?.[0];
const confirmation = constructor.match(/m_button_confirm->Bind\(wxEVT_BUTTON, &EditDevNameDialog::(\w+)/)?.[1];
assert.ok(creation && confirmation);
const sizeListener = constructor.includes('m_static_valid->Bind(wxEVT_SIZE');
const fit = definition(gui, 'void EditDevNameDialog::fit_validation_content()');
const validate = definition(gui, 'void EditDevNameDialog::on_edit_name(');
const confirm = definition(gui, 'void EditDevNameDialog::on_confirm(');
const setLabel = definition(labelSource, 'void Label::SetLabel(');
assert.ok(fit && validate && setLabel);
// Compile the actual validator, Label::SetLabel and confirmation/fitting paths.
// The native static-text double models its synchronous autosize event; wrapping
// metrics are synthetic and deliberately include a very long translated message.
const source = String.raw`
#include "PrintSetupLayout.hpp"
namespace PrintSetupLayout = Slic3r::GUI::PrintSetupLayout;
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>
static int scale = 100, work_height = 360;
static std::vector<std::string> trace;
static int dip(int n) { return (n * scale + 50) / 100; }
struct wxString : std::string {
    using std::string::string;
    using std::string::operator=;
    wxString() = default;
    wxString(const std::string& value) : std::string(value) {}
    std::string ToUTF8() const { return *this; }
};
static const wxString wxEmptyString;
static wxString from_u8(const std::string& value) { return value; }
static wxString _L(const char* value) {
    return std::string(value) + " translated validation text repeated for a long bilingual layout. "
        + std::string(650, 'x');
}
#define wxT(x) x
constexpr int wxST_NO_AUTORESIZE = 1, LB_AUTO_WRAP = 2, wxNOT_FOUND = -1, wxID_CLOSE = 3;
enum { Valid, NoValid };
struct wxCommandEvent {};
struct wxSize {
    int x, y;
    wxSize(int a=0, int b=0) : x(a), y(b) {}
    int GetWidth() const { return x; }
    bool operator!=(wxSize other) const { return x != other.x || y != other.y; }
};
struct wxWindow {};
struct wxDisplay {
    explicit wxDisplay(int) {}
    static int GetFromWindow(wxWindow*) { return 0; }
    struct Area { int height; };
    Area GetClientArea() const { return {dip(work_height)}; }
};
struct wxStaticText {
    int style = 0;
    wxString native;
    wxSize size{dip(280), dip(20)};
    int wrap_width = 0;
    std::function<void()> on_size;
    virtual ~wxStaticText() = default;
    virtual void SetLabel(const wxString& value) {
        trace.emplace_back("SetLabel"); native=value; wrap_width=0;
        if (!(style & wxST_NO_AUTORESIZE)) {
            size = wxSize(int(value.size()) * dip(8), dip(20));
            trace.emplace_back("autosize-event"); if (on_size) on_size();
        }
    }
    wxString GetLabel() const { return native; }
    void Wrap(int width) { trace.emplace_back("Wrap"); wrap_width=(std::max)(1,width); }
    wxSize GetBestSize() const {
        const int raw=int(native.size())*dip(8);
        const int width=wrap_width ? wrap_width : (std::max)(1,raw);
        return wxSize((std::min)(raw,width), ((raw+width-1)/width)*dip(20));
    }
    wxSize GetSize() const { return size; }
};
struct Label : wxStaticText {
    wxString m_text;
    int m_wrap_width = 0;
    Label(void*, const wxString& value, int flags=0) { style=flags; native=value; }
    int GetWindowStyle() const { return style; }
    void SetLabel(const wxString& value) override;
    void SetMinSize(wxSize) {}
    void Wrap(int width) { native=m_text; wxStaticText::Wrap(width); }
};
struct TextCtrl {
    wxString value;
    wxString GetValue() const { return value; }
};
struct TextInput {
    TextCtrl text;
    wxSize size{dip(280),dip(40)};
    TextCtrl* GetTextCtrl() { return &text; }
    wxSize GetSize() const { return size; }
};
struct MachineObject { std::string get_dev_id() const { return "fixture-device"; } };
struct DeviceManager {
    int calls = 0;
    std::string id, value;
    void modify_device_name(const std::string& key, const std::string& name) { ++calls; id=key; value=name; }
};
namespace PresetCollection { static std::string get_suffix_modified() { return "(modified)"; } }
namespace Slic3r::GUI {
    struct App { DeviceManager device; DeviceManager* getDeviceManager() { return &device; } };
    static App& wxGetApp() { static App app; return app; }
}
struct View {
    wxSize minimum{dip(280),0}, virtual_size, allocated{dip(280),0};
    wxSize GetClientSize() const { return wxSize(allocated.x - (virtual_size.y > minimum.y ? dip(16) : 0), minimum.y); }
    void SetMinSize(wxSize value) { minimum=value; }
    void SetVirtualSize(wxSize value) { virtual_size=value; }
    void FitInside() {}
};
struct EditDevNameDialog;
struct Sizer { EditDevNameDialog* owner; wxSize CalcMin(); };
struct DPIDialog : wxWindow { bool shown=true; void EndModal(int) { shown=false; } };
struct EditDevNameDialog : DPIDialog {
    bool m_fitting_content=false;
    wxSize current{dip(312),dip(180)};
    TextInput editor;
    TextInput* m_textCtr=&editor;
    View view;
    View* m_validation_view=&view;
    wxStaticText* m_static_valid=nullptr;
    MachineObject machine;
    MachineObject* m_info=&machine;
    Sizer sizer{this};
    bool scroll_enabled = SCROLL_ENABLED;
    int fits=0;
    EditDevNameDialog() { LABEL_CREATION
        if (SIZE_LISTENER) m_static_valid->on_size=[this] { fit_validation_content(); };
    }
    ~EditDevNameDialog() { delete m_static_valid; }
    bool IsShown() const { return shown; }
    wxWindow* GetParent() { return nullptr; }
    int FromDIP(int n) const { return dip(n); }
    wxSize GetSize() const { return wxSize(current.x,current.y+dip(20)); }
    wxSize GetClientSize() const { return current; }
    Sizer* GetSizer() { return &sizer; }
    void SetClientSize(wxSize value) { trace.emplace_back("dialog-resize"); current=value; ++fits; if (fits==1) fit_validation_content(); }
    void Layout() {
        editor.size.x=current.x-dip(32); view.allocated.x=editor.size.x;
        m_static_valid->size.x=scroll_enabled ? view.GetClientSize().x : editor.size.x;
        m_static_valid->size.y=m_static_valid->GetBestSize().y;
    }
    void fit_validation_content();
    void on_edit_name(wxCommandEvent&);
    void on_confirm(wxCommandEvent&);
    void invoke(wxCommandEvent& event) { CONFIRM_TARGET(event); }
};
wxSize Sizer::CalcMin() {
    if (owner->scroll_enabled) return wxSize(owner->current.x,dip(120)+owner->view.minimum.y);
    const wxSize text=owner->m_static_valid->GetBestSize();
    return wxSize((std::max)(dip(312),text.x+dip(32)),dip(120)+text.y);
}
` .replace('SCROLL_ENABLED', String(constructor.includes('m_validation_view = new MD3ScrolledWindow')))
  .replace('LABEL_CREATION', creation)
  .replace('SIZE_LISTENER', String(sizeListener))
  .replace('CONFIRM_TARGET', confirmation)
  + setLabel + '\n' + validate + '\n' + fit + '\n' + confirm + String.raw`
int main() {
    int checks=0;
    auto check=[&](bool value,const char* why) { ++checks; if(!value) { std::fprintf(stderr,"FAILED: %s\n",why); return false; } return true; };
    for (int percent : {100,125,150,200}) {
        scale=percent;
        EditDevNameDialog dialog;
        dialog.editor.text.value=" leading-space";
        trace.clear();
        wxCommandEvent event;
        dialog.invoke(event);
        if(!check(dialog.current.x==dip(312),"SetLabel must not widen the dialog before Wrap")) return 1;
        const auto wrap=std::find(trace.begin(),trace.end(),"Wrap");
        const auto fit_event=std::find(trace.begin(),trace.end(),"dialog-resize");
        if(!check(wrap!=trace.end() && wrap<fit_event,"actual validator wraps before presentation fitting")) return 1;
        if(!check(dialog.GetSize().y<=dip(work_height)-2*dip(12),"dialog stays within display reserve")) return 1;
        if(!check(dialog.view.virtual_size.y>dialog.view.minimum.y,"full long message remains scrollable")) return 1;
        if(!check(!dialog.m_fitting_content && dialog.shown && dialog.fits==2,"invalid result retains dialog with bounded reentry")) return 1;
        if(!check(Slic3r::GUI::wxGetApp().device.calls==0,"invalid input never renames a device")) return 1;
    }
    scale=100;
    EditDevNameDialog valid;
    valid.editor.text.value="Bench printer";
    wxCommandEvent event;
    valid.invoke(event);
    auto& device=Slic3r::GUI::wxGetApp().device;
    if(!check(device.calls==1 && device.id=="fixture-device" && device.value=="Bench printer","valid name dispatches once to existing device")) return 1;
    if(!check(!valid.shown && valid.fits==0,"successful modal close skips presentation fitting")) return 1;
    std::printf("%d device name ordering assertions passed\n",checks);
}
`;

function compiler() {
  if (process.platform !== 'win32') return null;
  const vswhere = path.join(process.env['ProgramFiles(x86)'] ?? 'C:/Program Files (x86)',
    'Microsoft Visual Studio/Installer/vswhere.exe');
  assert.ok(existsSync(vswhere), 'Visual Studio discovery is required for this native test');
  const installation = execFileSync(vswhere, ['-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], { encoding: 'utf8' }).trim();
  const setup = path.join(installation, 'VC/Auxiliary/Build/vcvars64.bat');
  assert.ok(installation && existsSync(setup), 'A supported MSVC toolchain is required');
  return setup;
}

function compile(directory, setup, contents) {
  writeFileSync(path.join(directory, 'PrintSetupLayout.hpp'), contents);
  writeFileSync(path.join(directory, 'test.cpp'), '#include <initializer_list>\n' + source);
  if (setup) {
    writeFileSync(path.join(directory, 'compile.cmd'),
      `@echo off\r\ncall "${setup}" >nul\r\nif errorlevel 1 exit /b %errorlevel%\r\ncl /nologo /std:c++17 /EHsc /W4 test.cpp /Fe:test.exe /Fo:test.obj\r\nexit /b %errorlevel%\r\n`);
    try {
      execFileSync(process.env.ComSpec ?? 'cmd.exe', ['/d', '/c', 'compile.cmd'], { cwd: directory, stdio: 'pipe' });
    } catch (error) {
      throw new Error(String(error.stdout) + String(error.stderr), { cause: error });
    }
  } else {
    execFileSync(process.env.CXX ?? 'c++', ['-std=c++17', '-Wall', '-Wextra', 'test.cpp', '-o', 'test'],
      { cwd: directory, stdio: 'pipe' });
  }
  return spawnSync(path.join(directory, setup ? 'test.exe' : 'test'), [], { cwd: directory, encoding: 'utf8' });
}

test('actual SetLabel event and validator Wrap order keep long messages bounded', () => {
  const directory = mkdtempSync(path.join(tmpdir(), 'device-name-ordering-'));
  try {
    const result = compile(directory, compiler(), readFileSync(header, 'utf8'));
    assert.equal(result.status, 0, result.stderr || result.stdout);
    assert.match(result.stdout, /26 device name ordering assertions passed/);
    console.log(result.stdout.trim());
  } finally {
    assert.equal(path.dirname(path.resolve(directory)), path.resolve(tmpdir()));
    assert.ok(path.basename(directory).startsWith('device-name-ordering-'));
    rmSync(directory, { recursive: true, force: true });
  }
});
