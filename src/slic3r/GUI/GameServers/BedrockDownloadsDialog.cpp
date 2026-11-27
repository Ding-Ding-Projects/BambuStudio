#include "BedrockDownloadsDialog.hpp"
#include "BedrockDistributions.hpp"
#include "BedrockHistory.hpp"
#include "libslic3r/Utils.hpp"
#include <wx/datetime.h>
#include "ServerSystem.hpp"
#include "slic3r/Utils/Http.hpp"
#include "../I18N.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/ComboBox.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/LabeledCheckBox.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/TextInput.hpp"
#include <openssl/evp.h>
#include <wx/app.h>
#include <wx/dialog.h>
#include <wx/dirdlg.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#include <filesystem>
#include <fstream>
#include <random>

namespace Slic3r { namespace GUI { namespace GameServers {
namespace fs = std::filesystem;
namespace {
Download get(const std::string &url, std::size_t cap)
{
    Download d;
    Http::get(url)
        .clear_headers()
        .header("User-Agent", "BambuStudio-ServerManager/1.0")
        .follow_redirects(false)
        .timeout_connect(10)
        .timeout_max(180)
        .size_limit(cap)
        .on_complete([&](std::string bytes, unsigned code) {
            d.ok = code == 200;
            if (d.ok)
                d.body = std::move(bytes);
            else
                d.error = "HTTP " + std::to_string(code);
        })
        .on_error([&](std::string, std::string err, unsigned code) { d.error = "HTTP " + std::to_string(code) + ": " + err; })
        .perform_sync();
    return d;
}
std::string sha256_bytes(const std::string &data)
{
    unsigned char hash[EVP_MAX_MD_SIZE]{};
    unsigned int  n = 0;
    if (!EVP_Digest(data.data(), data.size(), hash, &n, EVP_sha256(), nullptr) || n != 32) return {};
    std::string out;
    for (unsigned int i = 0; i < n; ++i) {
        out += "0123456789abcdef"[hash[i] >> 4];
        out += "0123456789abcdef"[hash[i] & 15];
    }
    return out;
}
class BedrockDownloadsDialog final : public wxDialog
{
public:
    BedrockDownloadsDialog(wxWindow *parent, std::function<void(const std::string &)> done)
        : wxDialog(parent, wxID_ANY, _L("Official Bedrock distributions"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), installed(std::move(done))
    {
        auto *body = new wxBoxSizer(wxVERTICAL);
        auto *load = new Button(this, _L("Read official versions"));
        load->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { read(); });
        body->Add(load, 0, wxALL, FromDIP(8));
        versions = new ComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(480, -1)), 0, nullptr, wxCB_READONLY);
        body->Add(versions, 0, wxEXPAND | wxALL, FromDIP(8));
        digest = new TextInput(this, {});
        digest->GetTextCtrl()->SetHint(_L("Optional SHA-256 from a trusted source"));
        body->Add(digest, 0, wxEXPAND | wxALL, FromDIP(8));
        agree = new LabeledCheckBox(this, _L("I accept the Minecraft EULA for this download."));
        body->Add(agree, 0, wxALL, FromDIP(8));
        auto *actions = new wxWrapSizer(wxHORIZONTAL);
        auto  add     = [&](const wxString &s, std::function<void()> fn) {
            auto *b = new Button(this, s);
            b->Bind(wxEVT_BUTTON, [fn](wxCommandEvent &) { fn(); });
            actions->Add(b, 0, wxALL, FromDIP(3));
        };
        add(_L("Read EULA"), [] { wxLaunchDefaultBrowser("https://aka.ms/MinecraftEULA"); });
        add(_L("Download ZIP"), [this] { download(false); });
        add(_L("Install on this PC"), [this] { download(true); });
        body->Add(actions, 0, wxEXPAND | wxALL, FromDIP(6));
        status = new Label(this, Label::Body_13,
                           _L("Read the publisher's current stable and preview links. Historical downloads are available only when the publisher lists them."));
        status->Wrap(FromDIP(500));
        body->Add(status, 0, wxEXPAND | wxALL, FromDIP(8));
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &e) {
            if (busy && e.CanVeto())
                e.Veto();
            else
                e.Skip();
        });
        SetEscapeId(wxID_NONE);
        SetSizerAndFit(body);
        MD3DialogCaption::Adopt(this);
        CentreOnParent();
        std::string cached;
        if(read_file(cache_path(),cached,8*1024*1024)){
            history=read_bedrock_history(cached);
            if(!history.empty()){display_history();status->SetLabel(_L("Showing previously observed official links from this computer. Refresh to check current availability."));}
        }
    }

private:
    static std::string cache_path(){return (fs::u8path(data_dir())/"minecraft-bedrock-distributions.json").u8string();}
    void display_history()
    {
        rows.clear();versions->Clear();
        for(const auto &e:history){
            const auto &r=e.file;rows.push_back(r);
            auto label=(r.windows?_L("Windows"):_L("Linux"))+" / "+(r.preview?_L("Preview"):_L("Stable"))+" / "+wxString::FromUTF8(r.version);
            label+=" / "+(e.current?_L("Last listed"):_L("Previously observed"))+" "+wxDateTime(e.last).FormatISOCombined();
            versions->Append(label);
        }
        if(!rows.empty())versions->SetSelection(0);
    }
    void read()
    {
        if (busy) return;
        busy = true;
        status->SetLabel(_L("Reading the publisher's download index..."));
        std::weak_ptr<bool> weak = life;
        std::thread([this, weak] {
            const auto d       = get("https://net-secondary.web.minecraft-services.net/api/v1.0/download/links", 1024 * 1024);
            auto       entries = bedrock_distributions(d.body);
            wxTheApp->CallAfter([this, weak, entries = std::move(entries), error = d.error]() mutable {
                if (weak.expired()) return;
                busy=false;
                if(!error.empty() || entries.empty()){status->SetLabel(error.empty()?_L("No supported official releases were returned. Cached history was preserved."):wxString::FromUTF8(error));return;}
                history=merge_bedrock_history(history,entries,std::time(nullptr));display_history();
                std::string why;if(!write_file_atomic(cache_path(),write_bedrock_history(history),&why))status->SetLabel(wxString::FromUTF8(why));
                else status->SetLabel(_L("Live links refreshed. Previously observed links are retained; the publisher does not supply checksums in this index."));
            });
        }).detach();
    }
    void download(bool install)
    {
        int idx = versions->GetSelection();
        if (busy || idx < 0 || std::size_t(idx) >= rows.size()) return;
        const auto row = rows[std::size_t(idx)];
        if (!agree->GetValue() || (install && !row.windows)) {
            status->SetLabel(_L("Review the EULA. Local installation requires a Windows distribution."));
            return;
        }
        auto expected = digest->GetTextCtrl()->GetValue().Lower().ToStdString(wxConvUTF8);
        if (!expected.empty() && (expected.size() != 64 || expected.find_first_not_of("abcdef0123456789") != std::string::npos)) {
            status->SetLabel(_L("Enter a 64-digit SHA-256 or leave the field empty."));
            return;
        }
        wxDirDialog dir(this, _L("Choose an existing parent directory"));
        if (dir.ShowModal() != wxID_OK) return;
        const auto dest = fs::u8path(dir.GetPath().ToStdString(wxConvUTF8));
        busy            = true;
        status->SetLabel(_L("Downloading from the publisher over HTTPS..."));
        std::weak_ptr<bool> weak = life;
        std::thread([this, weak, dest, row, expected, install] {
            auto        d     = get(row.url, 512ULL * 1024 * 1024);
            auto        sum   = d.ok ? sha256_bytes(d.body) : std::string();
            std::string error = d.error, out;
            if (d.ok && (sum.empty() || (!expected.empty() && expected != sum))) error = "SHA-256 verification did not pass.";
            if (d.ok && error.empty()) {
                std::error_code ec;
                const auto      tmp = dest / (".bedrock-download-" + std::to_string(std::random_device{}()));
                if (!fs::create_directory(tmp, ec))
                    error = "Could not create a separate download directory.";
                else {
                    const auto zip = tmp / row.file;
                    if (write_file_atomic(zip.u8string(), d.body, &error)) {
                        d.body.clear();
                        d.body.shrink_to_fit();
                        const auto entries = zip_top_entries(zip.u8string());
                        const auto binary  = row.windows ? "bedrock_server.exe" : "bedrock_server";
                        if (std::find(entries.begin(), entries.end(), binary) == entries.end() || std::find(entries.begin(), entries.end(), "server.properties") == entries.end())
                            error = "The package does not contain the expected server executable and settings.";
                        else if (install) {
                            auto target = dest / ("bedrock-server-" + row.version);
                            auto result = extract_zip(zip.u8string(), target.u8string(), nullptr, 4ULL * 1024 * 1024 * 1024);
                            if (!result.ok)
                                error = result.error;
                            else
                                out = target.u8string();
                        } else {
                            // Include platform and channel so the two official archives with the same release number do not collide.
                            const auto target = dest / ((row.windows ? std::string("windows-") : std::string("linux-")) + (row.preview ? "preview-" : "stable-") + row.file);
                            fs::create_hard_link(zip, target, ec);
                            if (ec)
                                error = ec.message();
                            else
                                out = target.u8string();
                        }
                    }
                    fs::remove(zip, ec);
                    fs::remove(tmp, ec);
                }
            }
            wxTheApp->CallAfter([this, weak, error, out, sum, install] {
                if (weak.expired()) return;
                busy = false;
                if (!error.empty() || out.empty()) {
                    status->SetLabel((error.empty() ? _L("Download incomplete.") : wxString::FromUTF8(error)));
                    return;
                }
                status->SetLabel(wxString::Format(_L("Saved %s. Computed SHA-256: %s"), wxString::FromUTF8(out), wxString::FromUTF8(sum)));
                status->Wrap(FromDIP(500));
                Layout();
                if (install) installed(out);
            });
        }).detach();
    }
    std::function<void(const std::string &)> installed;
    bool                                     busy = false;
    std::vector<ObservedDistribution> history;
    std::vector<BedrockDistribution>         rows;
    ComboBox                                *versions;
    TextInput                               *digest;
    LabeledCheckBox                         *agree;
    Label                                   *status;
    std::shared_ptr<bool>                    life = std::make_shared<bool>(true);
};
} // namespace
void show_bedrock_distributions(wxWindow *parent, std::function<void(const std::string &)> installed)
{
    BedrockDownloadsDialog dlg(parent, std::move(installed));
    dlg.ShowModal();
}
}}} // namespace Slic3r::GUI::GameServers
