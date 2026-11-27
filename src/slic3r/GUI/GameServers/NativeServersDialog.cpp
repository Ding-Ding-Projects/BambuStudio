#include "NativeServersDialog.hpp"
#include "NativeServerTarget.hpp"
#include "libslic3r/Utils.hpp"
#include <random>
#include "ServerSystem.hpp"
#include "ContentInstaller.hpp"
#include "BedrockSettingsDialog.hpp"
#include "BedrockDownloadsDialog.hpp"
#include "WorldRecoveryDialog.hpp"
#include "../I18N.hpp"
#include "../MsgDialog.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/LabeledCheckBox.hpp"
#include "../Widgets/MD3DataView.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/SpinInput.hpp"
#include "../Widgets/TextArea.hpp"
#include "../Widgets/TextInput.hpp"
#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/dirdlg.h>
#include <wx/filefn.h>
#include <wx/filedlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/utils.h>
#include <wx/wrapsizer.h>
#include <deque>
#include <filesystem>

namespace Slic3r { namespace GUI { namespace GameServers {
namespace {
class NativeServersDialog final : public wxDialog {
public:
    explicit NativeServersDialog(wxWindow *parent)
        : wxDialog(parent, wxID_ANY, _L("Native server console"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        auto *body = new wxBoxSizer(wxVERTICAL);
        auto *form = new wxFlexGridSizer(2, FromDIP(6), FromDIP(10));
        form->AddGrowableCol(1);
        auto field = [&](const wxString &label, wxWindow *control) {
            form->Add(new Label(this, Label::Body_13, label), 0, wxALIGN_CENTER_VERTICAL);
            form->Add(control, 1, wxEXPAND);
        };
        target = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
        target->Append(_L("Bedrock on this PC"));
        target->Append(_L("Java on SSH host"));
        target->Append(_L("Bedrock on SSH host"));
        target->SetSelection(0);
        field(_L("Target"), target);
        profile = new TextInput(this, wxEmptyString);
        field(_L("Configured SSH host alias"), profile);
        folder = new TextInput(this, wxEmptyString);
        field(_L("Server directory"), folder);
        source_package=new TextInput(this,wxEmptyString);
        source_package->GetTextCtrl()->SetHint(_L("Absolute path on the SSH host"));
        field(_L("Trusted remote server JAR or Bedrock ZIP"),source_package);
        remote_port=new SpinInput(this,{}, {},wxDefaultPosition,wxDefaultSize,0,1,65535,25565);
        field(_L("Port for new SSH server"),remote_port);
        browse = new Button(this, _L("Choose local directory..."));
        form->AddSpacer(1); form->Add(browse);
        browse->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            wxDirDialog dlg(this, _L("Select an extracted Bedrock dedicated server directory"), folder->GetTextCtrl()->GetValue());
            if (dlg.ShowModal() == wxID_OK) folder->GetTextCtrl()->SetValue(dlg.GetPath());
        });
        body->Add(form, 0, wxEXPAND | wxALL, FromDIP(10));
        agree = new LabeledCheckBox(this, _L("I accept the Minecraft EULA and authorize this server to listen on its configured addresses."));
        body->Add(agree, 0, wxALL, FromDIP(8));
        auto *actions = new wxWrapSizer(wxHORIZONTAL);
        auto action = [&](const wxString &label, std::function<void()> fn) {
            auto *button = new Button(this, label);
            button->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
            actions->Add(button, 0, wxALL, FromDIP(3));
            return button;
        };
        action(_L("Official Bedrock versions..."),[this]{show_bedrock_distributions(this,[this](const std::string &dir){folder->GetTextCtrl()->SetValue(wxString::FromUTF8(dir));});});
        settings=action(_L("Server settings..."),[this]{
            const auto dir=folder->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8);
            if(target->GetSelection()==0)show_bedrock_settings(this,dir);
            else show_target_settings(this,{false,target->GetSelection()==2,profile->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8),dir,local_owner(false)});
        });
        add_pack=action(_L("Install Bedrock pack..."),[this] { pack(false); });
        enable_pack=action(_L("Activate installed pack..."),[this] { pack(true); });
        ssh_recovery=action(_L("SSH world recovery..."),[this]{
            show_world_recovery(this,{false,target->GetSelection()==2,profile->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8),
                folder->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8),local_owner(false)});
        });
        remote_setup=action(_L("Provision new SSH server..."),[this]{ provision(); });
        setup_button = action(_L("Install Bedrock ZIP..."), [this] { install(); });
        action(_L("Read EULA"), [] { wxLaunchDefaultBrowser("https://aka.ms/MinecraftEULA"); });
        start_button = action(_L("Start server"), [this] { start(); });
        stop_button = action(_L("Stop server"), [this] { send("stop"); });
        action(_L("List players"), [this] { send("list"); });
        body->Add(actions, 0, wxEXPAND | wxLEFT, FromDIP(6));
        console = new TextArea(this, wxEmptyString, FromDIP(wxSize(600, 210)), wxTE_READONLY);
        body->Add(console, 1, wxEXPAND | wxALL, FromDIP(8));
        auto *commands = new wxBoxSizer(wxHORIZONTAL);
        command = new TextInput(this, wxEmptyString, wxEmptyString, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        command->GetTextCtrl()->SetHint(_L("Server console command"));
        commands->Add(command, 1, wxEXPAND | wxRIGHT, FromDIP(6));
        auto *run = new Button(this, _L("Send"));
        auto submit = [this](wxCommandEvent &) { if (send(command->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8))) command->GetTextCtrl()->Clear(); };
        run->Bind(wxEVT_BUTTON, submit); command->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, submit);
        commands->Add(run);
        body->Add(commands, 0, wxEXPAND | wxALL, FromDIP(8));
        target->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) { remote_port->SetValue(target->GetSelection()==2?19132:25565);controls(); });
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &e) {
            if ((running || installing) && e.CanVeto()) { append(_L("Wait for installation or stop the server before closing.")); e.Veto(); }
            else e.Skip();
        });
        SetEscapeId(wxID_NONE);
        SetSizerAndFit(body);
        MD3DialogCaption::Adopt(this); CentreOnParent(); controls();
    }
private:
    std::string local_owner(bool create)
    {
        const auto host=profile->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8),dir=folder->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8);
        if(!ssh_profile_name(host) || shell_path(dir).empty()) return {};
        const auto id_dir=std::filesystem::u8path(Slic3r::data_dir())/"minecraft-native-profiles"/sha1_hex(host+"\n"+dir);
        const auto file=id_dir/"id";
        std::string id;
        if(read_file(file.u8string(),id,64)) return ownership_id(id)?id:std::string();
        if(!create)return {};
        std::error_code ec;std::filesystem::create_directories(id_dir.parent_path(),ec);
        if(ec || !std::filesystem::create_directory(id_dir,ec))return {};
        std::random_device random;
        for(int n=0;n<32;++n)id+="0123456789abcdef"[random()%16];
        if(!write_file_atomic(file.u8string(),id)){std::filesystem::remove(id_dir,ec);return {};}
        return id;
    }
    void provision()
    {
        if(running || installing || target->GetSelection()==0)return;
        const auto host=profile->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8),dir=folder->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8);
        const auto src=source_package->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8);
        wxString path,exe;wxGetEnv("PATH",&path);
        if(!wxFindFileInPath(&exe,path,"ssh.exe") || !ssh_profile_name(host) || shell_path(dir).empty() || shell_path(src).empty()){
            append(_L("Choose an SSH identity, absolute package and destination paths, and an installed OpenSSH client."));return;
        }
        if(MessageDialog(this,wxString::Format(_L("Create a new server directory at %s on %s from the selected package? The server will stay stopped."),
                wxString::FromUTF8(dir),wxString::FromUTF8(host)),_L("Provision SSH server"),wxYES_NO|wxNO_DEFAULT|wxICON_INFORMATION).ShowModal()!=wxID_YES)return;
        const auto owner=local_owner(true);
        auto args=ssh_server_setup(exe.ToStdString(wxConvUTF8),host,dir,src,target->GetSelection()==2,remote_port->GetValue(),owner);
        if(args.empty()){append(_L("Could not establish ownership for this server path."));return;}
        std::weak_ptr<bool> weak=lifetime;
        auto lines=std::make_shared<std::string>();
        process=std::make_unique<ServerProcess>();std::string error;
        installing=process->start(args,wxGetCwd().ToStdString(wxConvUTF8),
            [lines](const std::string &line){if(lines->size()<2048)*lines+=line.substr(0,512)+"\n";},
            [this,weak,lines](int code){wxTheApp->CallAfter([this,weak,lines,code]{
                if(weak.expired())return;
                installing=false;controls();
                append(code==0?_L("New server installed on the SSH host. Review its network settings and EULA before starting."):
                    wxString::Format(_L("SSH provisioning exited with code %d: %s"),code,wxString::FromUTF8(*lines)));
            });},&error);
        controls();append(installing?_L("Provisioning the selected remote server directory..."):wxString::FromUTF8(error));
    }

    void pack(bool activate)
    {
        if (running || installing || target->GetSelection()!=0) return;
        wxFileDialog pick(this,activate ? _L("Select the installed pack manifest") : _L("Select a trusted Bedrock pack"),
                          folder->GetTextCtrl()->GetValue(),{},activate ? "manifest.json|manifest.json" : "Bedrock pack (*.mcpack;*.zip)|*.mcpack;*.zip",
                          wxFD_OPEN|wxFD_FILE_MUST_EXIST);
        if (pick.ShowModal()!=wxID_OK) return;
        const auto dir=folder->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8), file=pick.GetPath().ToStdString(wxConvUTF8);
        installing=true; controls();
        std::weak_ptr<bool> weak=lifetime;
        std::thread([this,weak,dir,file,activate] {
            auto result=activate ? activate_bedrock_content(dir,file) : install_bedrock_content(dir,file);
            wxTheApp->CallAfter([this,weak,result,activate] {
                if (weak.expired()) return;
                installing=false; controls();
                append(result.ok ? (activate ? _L("Pack enabled for the next world start.") : _L("Installed. Choose Activate installed pack to enable it in the configured world.")) : wxString::FromUTF8(result.message));
            });
        }).detach();
    }

    void install()
    {
        if (running || installing || target->GetSelection() != 0) return;
        wxFileDialog file(this, _L("Choose a trusted Windows Bedrock Dedicated Server ZIP"), {}, {}, "ZIP (*.zip)|*.zip", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (file.ShowModal() != wxID_OK) return;
        wxDirDialog parent(this, _L("Choose the parent directory for the new server"));
        if (parent.ShowModal() != wxID_OK) return;
        const std::string src = file.GetPath().ToStdString(wxConvUTF8);
        const auto leaf = std::filesystem::u8path(src).stem();
        const std::string dest = (std::filesystem::u8path(parent.GetPath().ToStdString(wxConvUTF8)) / leaf).u8string();
        installing = true;
        controls();
        append(_L("Checking the package and extracting into a new server directory."));
        std::weak_ptr<bool> weak = lifetime;
        std::thread([this, weak, src, dest] {
            const auto entries = zip_top_entries(src);
            ZipOutcome result;
            if (std::find(entries.begin(), entries.end(), "bedrock_server.exe") == entries.end() ||
                std::find(entries.begin(), entries.end(), "server.properties") == entries.end())
                result.error = "The archive does not contain bedrock_server.exe and server.properties at its root.";
            else result = extract_zip(src, dest, nullptr, std::uint64_t(4) * 1024 * 1024 * 1024);
            wxTheApp->CallAfter([this, weak, result, dest] {
                if (weak.expired()) return;
                installing = false;
                controls();
                if (result.ok) {
                    folder->GetTextCtrl()->SetValue(wxString::FromUTF8(dest));
                    append(_L("Installed. Review the EULA and network settings before starting."));
                } else append(wxString::FromUTF8(result.error));
            });
        }).detach();
    }

    void controls()
    {
        const bool idle = !running && !installing;
        target->Enable(idle); folder->Enable(idle); agree->Enable(idle);
        setup_button->Enable(idle && target->GetSelection() == 0);
        settings->Enable(idle);
        add_pack->Enable(idle && target->GetSelection() == 0); enable_pack->Enable(idle && target->GetSelection() == 0);
        browse->Enable(idle && target->GetSelection() == 0);
        profile->Enable(idle && target->GetSelection() != 0);
        remote_setup->Enable(idle && target->GetSelection()!=0);ssh_recovery->Enable(idle && target->GetSelection()!=0);
        source_package->Enable(idle && target->GetSelection()!=0);remote_port->Enable(idle && target->GetSelection()!=0);
        start_button->Enable(idle); stop_button->Enable(running); command->Enable(running);
    }
    void append(const wxString &line)
    {
        if (console->GetTextCtrl()->GetLastPosition() > 65536) console->GetTextCtrl()->Remove(0, 16384);
        console->AppendText(line.Left(4096) + "\n");
    }
    bool send(const std::string &line)
    {
        if (!running || !valid_console_command(line) || !process->send(line)) {
            append(_L("Command was not sent. Check that the server is running and enter one command line."));
            return false;
        }
        return true;
    }
    void start()
    {
        if (running || !agree->GetValue()) { append(_L("Review the EULA and network permission first.")); return; }
        std::vector<std::string> argv;
        const auto dir = folder->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8);
        std::string cwd = wxGetCwd().ToStdString(wxConvUTF8);
        if (target->GetSelection() == 0) {
            const auto path = std::filesystem::u8path(dir);
            std::error_code ec;
            if (!path.is_absolute() || !std::filesystem::is_regular_file(path / "bedrock_server.exe", ec) ||
                !std::filesystem::is_regular_file(path / "server.properties", ec)) {
                append(_L("Choose an extracted Bedrock directory containing bedrock_server.exe and server.properties."));
                return;
            }
            cwd = dir; argv = {(path / "bedrock_server.exe").u8string()};
        } else {
            wxString search, ssh;
            wxGetEnv("PATH", &search);
            if (!wxFindFileInPath(&ssh, search, "ssh.exe")) { append(_L("OpenSSH client was not found.")); return; }
            argv = ssh_server_launch(ssh.ToStdString(wxConvUTF8), profile->GetTextCtrl()->GetValue().ToStdString(wxConvUTF8), dir, target->GetSelection() == 2, local_owner(false));
            if (argv.empty()) { append(_L("Enter a configured SSH alias and an absolute POSIX server directory.")); return; }
        }
        std::weak_ptr<bool> weak = lifetime;
        process = std::make_unique<ServerProcess>();
        std::string error;
        running = process->start(argv, cwd,
            [weak, this](const std::string &line) {
                wxTheApp->CallAfter([this, weak, line] { if (!weak.expired()) append(wxString::FromUTF8(line)); });
            },
            [weak, this](int code) {
                wxTheApp->CallAfter([this, weak, code] {
                    if (weak.expired()) return;
                    running = false; controls();
                    append(wxString::Format(_L("Process exited with code %d."), code));
                });
            }, &error);
        controls();
        append(running ? _L("Process launched. Watch the server output for readiness.") : wxString::FromUTF8(error));
    }
    bool running = false, installing = false;
    ComboBox *target;
    TextInput *profile, *folder, *command, *source_package;
    SpinInput *remote_port;
    TextArea *console;
    Button *browse, *start_button, *stop_button, *setup_button, *add_pack, *enable_pack, *remote_setup, *settings, *ssh_recovery;
    LabeledCheckBox *agree;
    std::unique_ptr<ServerProcess> process;
    std::shared_ptr<bool> lifetime = std::make_shared<bool>(true);
};
}
void show_native_servers(wxWindow *parent) { NativeServersDialog dlg(parent); dlg.ShowModal(); }
}}}
