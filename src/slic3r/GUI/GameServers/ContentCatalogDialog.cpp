#include "ContentCatalogDialog.hpp"
#include "ContentCatalog.hpp"
#include "ContentInstaller.hpp"
#include "ServerSystem.hpp"
#include "../I18N.hpp"
#include "../MsgDialog.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/Label.hpp"
#include "../Widgets/MD3DataView.hpp"
#include "../Widgets/MD3DialogChrome.hpp"
#include "../Widgets/SearchField.hpp"
#include "slic3r/Utils/Http.hpp"
#include <wx/app.h>
#include <wx/sizer.h>
#include <wx/wrapsizer.h>
#include <filesystem>
#include <random>

namespace Slic3r { namespace GUI { namespace GameServers {
namespace {
Download catalog_get(const std::string &url, std::size_t limit)
{
    Download result;
    Http::get(url).clear_headers().header("User-Agent", "BambuStudio-ServerManager/1.0").follow_redirects(false)
        .timeout_connect(10).timeout_max(90).size_limit(limit)
        .on_complete([&](std::string data, unsigned code) {
            result.ok = code == 200;
            if (result.ok) result.body = std::move(data);
            else result.error = "HTTP " + std::to_string(code);
        })
        .on_error([&](std::string, std::string error, unsigned code) { result.error = "HTTP " + std::to_string(code) + ": " + error; })
        .perform_sync();
    return result;
}

class CatalogDialog final : public wxDialog {
public:
    CatalogDialog(wxWindow *parent, std::string dir, std::string type, std::string game)
        : wxDialog(parent, wxID_ANY, _L("Server content catalog"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          folder(std::move(dir)), flavour(std::move(type)), version(std::move(game))
    {
        auto *body = new wxBoxSizer(wxVERTICAL);
        auto *line = new wxBoxSizer(wxHORIZONTAL);
        query = new SearchField(this, _L("Search Modrinth"));
        query->SetOnQuery([this](const wxString &) { if (query->IsRegexEnabled()) filter(); });
        query->SetOnRegexToggle([this](bool) { filter(); });
        line->Add(query, 1, wxRIGHT, FromDIP(5));
        auto *search = new Button(this, _L("Search"));
        line->Add(search);
        auto submit = [this](wxCommandEvent &) { projects(0); };
        search->Bind(wxEVT_BUTTON, submit); query->GetTextCtrl()->Bind(wxEVT_TEXT_ENTER, submit);
        body->Add(line, 0, wxEXPAND | wxALL, FromDIP(10));
        list = new MD3DataViewListCtrl(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(500, 180)), wxDV_SINGLE | wxDV_ROW_LINES);
        list->AppendTextColumn(_L("Projects and compatible packages"), wxDATAVIEW_CELL_INERT, FromDIP(470));
        body->Add(list, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(10));
        auto *row = new wxWrapSizer(wxHORIZONTAL);
        auto *prev = new Button(this, _L("Previous page"));
        auto *next = new Button(this, _L("Next page"));
        prev->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { if (offset > 0) projects(offset - 30); });
        next->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { if (offset < 9900) projects(offset + 30); });
        row->Add(prev, 0, wxRIGHT, FromDIP(6)); row->Add(next, 0, wxRIGHT, FromDIP(6));
        auto *open = new Button(this, _L("Show compatible packages"));
        auto *install = new Button(this, _L("Install selected package"));
        open->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { files(); });
        install->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { download(); });
        row->Add(open, 0, wxRIGHT, FromDIP(6)); row->Add(install);
        body->Add(row, 0, wxEXPAND | wxALL, FromDIP(10));
        status = new Label(this, Label::Body_13, wxString::Format(_L("Packages must match server version %s."), wxString::FromUTF8(version)));
        body->Add(status, 0, wxEXPAND | wxALL, FromDIP(10));
        Bind(wxEVT_CLOSE_WINDOW,[this](wxCloseEvent &e) { if (busy && e.CanVeto()) e.Veto(); else e.Skip(); });
        SetEscapeId(wxID_NONE);
        SetSizerAndFit(body); MD3DialogCaption::Adopt(this); CentreOnParent();
    }
private:
    void show(std::vector<CatalogItem> items, bool packages)
    {
        is_files = packages; rows = std::move(items); filter();
    }
    void filter()
    {
        if (!list) return;
        visible.clear(); list->DeleteAllItems();
        for (std::size_t n = 0; n < rows.size(); ++n) {
            const auto &i = rows[n];
            if (query->IsRegexEnabled() && !SearchField::textMatches(query->GetValue(), wxString::FromUTF8(i.name), true,
                  query->IsCaseSensitive(), query->IsWholeWord(), query->IsMultiline())) continue;
            visible.push_back(n);
            wxVector<wxVariant> row;
            row.push_back(wxVariant(wxString::FromUTF8(i.name))); list->AppendItem(row);
        }
        if (status) status->SetLabel(wxString::Format(_L("Showing %d of %d results on this page."), int(visible.size()), int(rows.size())));
    }
    void load(const std::string &url, bool packages)
    {
        if (busy) return;
        busy = true; status->SetLabel(_L("Reading the catalog..."));
        const auto game = version; const auto loaders = catalog_loaders(flavour);
        std::weak_ptr<bool> weak = life;
        std::thread([this, weak, url, packages, game, loaders] {
            const auto d = catalog_get(url, 8*1024*1024);
            auto items = !d.ok ? std::vector<CatalogItem>{} : packages ? catalog_files(d.body, game, loaders) : catalog_projects(d.body);
            wxTheApp->CallAfter([this, weak, packages, items = std::move(items), error=d.error]() mutable {
                if (weak.expired()) return;
                busy = false; show(std::move(items), packages);
                if (!error.empty()) status->SetLabel(wxString::FromUTF8(error));
            });
        }).detach();
    }
    void projects(int new_offset)
    {
        if (busy) return;
        const std::string value = query->IsRegexEnabled() ? std::string() : query->GetValue().ToStdString(wxConvUTF8);
        if (value.size() > 200 || version.empty()) { status->SetLabel(_L("Enter a search term and use a server with a known version.")); return; }
        const auto loaders = catalog_loaders(flavour);
        auto categories = nlohmann::json::array();
        for (const auto &loader : loaders) categories.push_back("categories:" + loader);
        auto facets = nlohmann::json::array({nlohmann::json::array({"versions:"+version}), categories});
        offset = new_offset;
        load("https://api.modrinth.com/v2/search?limit=30&offset="+std::to_string(offset)+"&query="+Http::url_encode(value)+"&facets="+Http::url_encode(facets.dump()), false);
    }
    void files()
    {
        const int idx = list->GetSelectedRow();
        if (busy || is_files || idx < 0 || std::size_t(idx) >= visible.size()) return;
        const nlohmann::json loaders = catalog_loaders(flavour);
        load("https://api.modrinth.com/v2/project/"+rows[visible[std::size_t(idx)]].id+"/version?loaders="+Http::url_encode(loaders.dump())+
             "&game_versions="+Http::url_encode(nlohmann::json::array({version}).dump()), true);
    }
    void download()
    {
        const int idx = list->GetSelectedRow();
        if (busy || !is_files || idx<0 || std::size_t(idx)>=visible.size()) return;
        const auto item=rows[visible[std::size_t(idx)]];
        if (MessageDialog(this,wxString::Format(_L("Install %s? Server plugins and mods execute code with the server's permissions."),
                wxString::FromUTF8(item.filename)),_L("Install content"),wxYES_NO | wxNO_DEFAULT | wxICON_WARNING).ShowModal()!=wxID_YES) return;
        busy = true; status->SetLabel(_L("Downloading and verifying the package..."));
        std::weak_ptr<bool> weak=life; const auto dir=folder, type=flavour;
        std::thread([this, weak, item, dir, type] {
            ContentInstallResult r;
            auto d = catalog_get(item.url, std::size_t(item.size));
            if (!d.ok) r.message = d.error;
            else if (d.body.size()!=item.size || sha1_hex(d.body)!=item.sha1) r.message="Package size or SHA-1 did not match its catalog record.";
            else {
                std::error_code ec;
                auto tmp=std::filesystem::temp_directory_path(ec) / ("bambu-catalog-"+std::to_string(std::random_device{}()));
                if (ec || !std::filesystem::create_directory(tmp,ec)) r.message="Could not create a new download directory.";
                else {
                    auto path=tmp/std::filesystem::u8path(item.filename);
                    if (write_file_atomic(path.u8string(),d.body,&r.message)) r=install_java_content(dir,type,path.u8string());
                    std::filesystem::remove(path,ec); std::filesystem::remove(tmp,ec);
                }
            }
            wxTheApp->CallAfter([this,weak,r] {
                if (weak.expired()) return;
                busy=false; status->SetLabel(r.ok ? _L("Installed. Check the server log on its next start.") : wxString::FromUTF8(r.message));
            });
        }).detach();
    }
    std::string folder,flavour,version;
    bool busy=false,is_files=false;
    std::vector<CatalogItem> rows;
    int offset = 0;
    std::vector<std::size_t> visible;
    SearchField *query;
    MD3DataViewListCtrl *list = nullptr;
    Label *status = nullptr;
    std::shared_ptr<bool> life=std::make_shared<bool>(true);
};
}
void show_content_catalog(wxWindow *parent,const std::string &folder,const std::string &flavour,const std::string &version)
{
    CatalogDialog dialog(parent,folder,flavour,version); dialog.ShowModal();
}
}}}
