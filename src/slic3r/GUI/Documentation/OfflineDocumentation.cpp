#include "OfflineDocumentation.hpp"
#include "DocumentationBundle.hpp"
#include "../Widgets/MD3Dialog.hpp"
#include "../Widgets/MD3HtmlWindow.hpp"
#include "../Widgets/SearchField.hpp"
#include "../Widgets/TabCtrl.hpp"
#include "../Widgets/ListBox.hpp"
#include "../Widgets/Button.hpp"
#include "../Widgets/StateColor.hpp"
#include "../I18N.hpp"
#include "../GUI_App.hpp"
#include "libslic3r/AppConfig.hpp"
#include <wx/weakref.h>
#include <wx/sizer.h>
#include <wx/fs_mem.h>
#include <wx/base64.h>
#include <wx/clipbrd.h>
#include <wx/filedlg.h>
#include <wx/file.h>
#include <wx/wrapsizer.h>
#include <wx/utils.h>
#include <algorithm>
#include <vector>

namespace Slic3r { namespace GUI { namespace Documentation {
namespace {
wxString utf(const std::string& text) { return wxString::FromUTF8(text.c_str()); }
const Article* find(wxString route) {
    route = route.BeforeFirst('#');
    for (const auto& article : articles)
        if (route == utf(article.route)) return &article;
    return nullptr;
}
class DocumentView final : public MD3HtmlWindow {
public:
    explicit DocumentView(wxWindow* parent) : MD3HtmlWindow(parent) {}
    // Generated pages are supplied using SetPage. Never allow renderer IO.
    wxHtmlOpeningStatus OnOpeningURL(wxHtmlURLType type, const wxString& url, wxString*) const override {
        if (type == wxHTML_URL_IMAGE)
            for (const auto& image : images)
                if (!image.name.empty() && url == "memory:documentation-" + utf(image.name)) return wxHTML_OPEN;
        return wxHTML_BLOCK;
    }
};
class Reader final : public MD3Dialog {
    SearchField* search;
    ListBox* index;
    TabCtrl* languages;
    TabCtrl* tabs;
    DocumentView* view;
    Button* back;
    std::vector<const Article*> filtered;
    std::vector<wxString> opened;
    std::vector<wxString> history;
    wxString current;
    int language = 0;
    bool selecting = false;
    int lastChecked = -1;

    void save() {
        if (wxGetApp().app_config) {
            wxGetApp().app_config->set("offline_documentation_language", std::to_string(language));
            wxGetApp().app_config->set("offline_documentation_article", current.ToUTF8().data());
        }
    }
    void OnHeaderClose() override { save(); Hide(); }

    wxString selectionText() const {
        wxString result;
        size_t count = 0;
        for (size_t i = 0; i < filtered.size(); ++i) {
            if (!index->IsChecked(static_cast<unsigned>(i))) continue;
            const auto& article = *filtered[i];
            wxString translated = utf(article.route); translated.Replace(".md", ".yue_HK.md");
            const auto* cantonese = find(translated);
            if (language != 1 || !cantonese) result += utf(article.text) + "\n\n";
            if (language != 0 && cantonese) result += utf(cantonese->text) + "\n\n";
            ++count;
        }
        if (!count) return {};
        return wxString::Format("# Documentation export\n\nScope: %zu selected articles in current search results. Encoding: UTF-8. Line endings: LF.\n\n", count) + result;
    }

    void exportSelection() {
        const auto text = selectionText();
        if (text.empty()) { SetHeaderSubtitle(_L("Select articles using their checkboxes before exporting.")); return; }
        wxFileDialog picker(this, _L("Export selected documentation"), wxEmptyString, "documentation.md", "Markdown (*.md)|*.md|Plain text (*.txt)|*.txt", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (picker.ShowModal() != wxID_OK) return;
        wxFile file(picker.GetPath(), wxFile::write);
        const auto bytes = text.ToUTF8();
        if (!file.IsOpened() || file.Write(bytes.data(), bytes.length()) != bytes.length() || !file.Close()) {
            SetHeaderSubtitle(_L("Documentation export could not be written.")); return;
        }
        SetHeaderSubtitle(_L("Selected documentation exported."));
    }

    void filter() {
        SearchField::MatchPass pass(search->GetValue(), search->IsRegexEnabled(), search->IsCaseSensitive(), search->IsWholeWord(), search->IsMultiline());
        filtered.clear();
        std::vector<wxString> labels;
        for (const auto& article : articles) {
            const wxString route = utf(article.route);
            if (route.Contains(".yue_HK.md")) continue;
            wxString translated = route; translated.Replace(".md", ".yue_HK.md");
            const auto* cantonese = find(translated);
            const auto searchable = utf(article.title) + "\n" + utf(article.text) + (cantonese ? "\n" + utf(cantonese->title) + "\n" + utf(cantonese->text) : wxString());
            if (!pass.matches(searchable)) continue;
            filtered.push_back(&article);
            labels.push_back(route.BeforeFirst('/') + " / " + utf(article.title));
        }
        index->Set(labels);
        lastChecked = -1;
        if (filtered.empty()) view->SetPage("<p>" + _L("No matching documentation articles.") + "</p>");
    }
    void render() {
        const auto* article = find(current);
        if (!article) return;
        wxString translated = utf(article->route);
        translated.Replace(".md", ".yue_HK.md");
        const auto* cantonese = find(translated);
        wxString body;
        if (language == 0 || language == 2) body += utf(article->html);
        if (language != 0) {
            if (cantonese) body += "<hr>" + utf(cantonese->html);
            else {
                body += "<p><b>" + _L("Cantonese translation is not available for this article. English source follows.") + "</b></p>";
                if (language == 1) body += utf(article->html);
            }
        }
        const bool dark = StateColor::isDarkMode();
        const auto bg = MD3::resolve(MD3::Role::Surface, dark).GetAsString(wxC2S_HTML_SYNTAX);
        const auto fg = MD3::resolve(MD3::Role::OnSurface, dark).GetAsString(wxC2S_HTML_SYNTAX);
        const auto link = MD3::resolve(MD3::Role::Primary, dark).GetAsString(wxC2S_HTML_SYNTAX);
        view->SetPage("<html><body bgcolor=\"" + bg + "\" text=\"" + fg + "\" link=\"" + link + "\">" + body + "</body></html>");
        if (current.Contains('#')) view->ScrollToAnchor(current.AfterFirst('#'));
        back->Enable(!history.empty());
    }
public:
    explicit Reader(wxWindow* parent) : MD3Dialog(parent, _L("Offline documentation"), _L("Bundled feature articles. External references never open automatically."), MaterialIcon::MenuBook, options()) {
        static bool imagesLoaded = false;
        if (!imagesLoaded) {
            wxFileSystem::AddHandler(new wxMemoryFSHandler);
            for (const auto& image : images) {
                if (image.name.empty()) continue;
                const auto bytes = wxBase64Decode(image.base64.c_str(), image.base64.size());
                wxMemoryFSHandler::AddFile("documentation-" + utf(image.name), bytes.GetData(), bytes.GetDataLen());
            }
            imagesLoaded = true;
        }
        auto* content = GetContentSizer();
        search = new SearchField(this, _L("Search article titles and text"));
        content->Add(search, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
        languages = new TabCtrl(this, wxID_ANY);
        languages->SetNavItemStyle(true);
        languages->AppendItem("English");
        languages->AppendItem(wxString::FromUTF8("廣東話"));
        languages->AppendItem(wxString::FromUTF8("English + 廣東話"));
        const auto& profile = I18N::language_mode_profile();
        language = profile.is_bilingual() ? 2 : profile.is_cantonese() ? 1 : 0;
        if (wxGetApp().app_config) {
            const auto saved = wxGetApp().app_config->get("offline_documentation_language");
            if (saved == "0" || saved == "1" || saved == "2") language = saved[0] - '0';
        }
        languages->SelectItem(language);
        content->Add(languages, 0, wxEXPAND | wxBOTTOM, FromDIP(8));
        tabs = new TabCtrl(this, wxID_ANY);
        tabs->SetNavItemStyle(true);
        content->Add(tabs, 0, wxEXPAND);
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        index = new ListBox(this, wxID_ANY, FromDIP(wxSize(240, 320)));
        index->EnableChecks();
        index->SetName(_L("Documentation categories and search results"));
        view = new DocumentView(this);
        view->SetName(_L("Documentation article and table of contents"));
        row->Add(index, 1, wxEXPAND | wxRIGHT, FromDIP(12));
        row->Add(view, 3, wxEXPAND);
        content->Add(row, 1, wxEXPAND);
        auto* selection = new wxWrapSizer(wxHORIZONTAL);
        auto addAction = [this, selection](const wxString& label) {
            auto* button = new Button(this, label);
            button->SetVariant(Button::Variant::Text);
            selection->Add(button, 0, wxALL, FromDIP(3));
            return button;
        };
        addAction(_L("Select all matches"))->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            for (unsigned i = 0; i < index->GetCount(); ++i) index->Check(i);
        });
        addAction(_L("Invert selection"))->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            for (unsigned i = 0; i < index->GetCount(); ++i) index->Check(i, !index->IsChecked(i));
        });
        addAction(_L("Copy selected articles"))->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            const auto text = selectionText();
            if (text.empty()) { SetHeaderSubtitle(_L("Select articles using their checkboxes before copying.")); return; }
            if (!wxTheClipboard->Open()) { SetHeaderSubtitle(_L("Clipboard is unavailable.")); return; }
            const bool copied = wxTheClipboard->SetData(new wxTextDataObject(text));
            wxTheClipboard->Close();
            SetHeaderSubtitle(copied ? _L("Selected documentation copied.") : _L("Documentation could not be copied."));
        });
        addAction(_L("Export selected articles"))->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { exportSelection(); });
        content->Add(selection, 0, wxEXPAND);
        back = AddFooterButton(new Button(this, _L("Back")));
        auto* closeTab = AddFooterButton(new Button(this, _L("Close article tab")));
        back->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            if (history.empty()) return;
            auto previous = history.back(); history.pop_back(); open(previous, false);
        });
        closeTab->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            int selection = tabs->GetSelection();
            if (selection < 0 || opened.size() <= 1) return;
            opened.erase(opened.begin() + selection); tabs->DeleteItem(selection);
            open(opened.front());
        });
        search->SetOnQuery([this](const wxString&) { filter(); });
        search->SetOnRegexToggle([this](bool) { filter(); });
        index->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
            int selected = index->GetSelection();
            if (selected >= 0 && static_cast<size_t>(selected) < filtered.size()) open(utf(filtered[selected]->route));
        });
        index->Bind(wxEVT_CHECKLISTBOX, [this](wxCommandEvent& event) {
            const int selected = event.GetInt();
            if (selected < 0 || static_cast<size_t>(selected) >= filtered.size()) return;
            if (wxGetKeyState(WXK_SHIFT) && lastChecked >= 0) {
                const bool checked = index->IsChecked(static_cast<unsigned>(selected));
                for (int i = std::min(lastChecked, selected); i <= std::max(lastChecked, selected); ++i)
                    index->Check(static_cast<unsigned>(i), checked);
            }
            lastChecked = selected;
        });
        languages->Bind(wxEVT_TAB_SEL_CHANGED, [this](wxCommandEvent&) { language = languages->GetSelection(); save(); render(); });
        tabs->Bind(wxEVT_TAB_SEL_CHANGED, [this](wxCommandEvent&) {
            if (selecting) return;
            int selected = tabs->GetSelection();
            if (selected >= 0 && static_cast<size_t>(selected) < opened.size()) open(opened[selected]);
        });
        view->Bind(wxEVT_HTML_LINK_CLICKED, [this](wxHtmlLinkEvent& event) {
            const auto href = event.GetLinkInfo().GetHref();
            if (href.StartsWith("doc:")) open(href.Mid(4));
        });
        SetMinSize(FromDIP(wxSize(680, 480)));
        SetSize(FromDIP(wxSize(1100, 760)));
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) { save(); if (event.CanVeto()) { event.Veto(); Hide(); } else event.Skip(); });
        filter();
    }
    static Options options() { Options value; value.resizable = true; return value; }
    bool open(wxString route, bool remember = true) {
        route.Replace(".yue_HK.md", ".md");
        const auto* article = find(route);
        if (!article) return false;
        if (remember && !current.empty() && current != route) {
            if (history.size() == 100) history.erase(history.begin());
            history.push_back(current);
        }
        current = route;
        selecting = true;
        const wxString base = route.BeforeFirst('#');
        auto found = std::find(opened.begin(), opened.end(), base);
        if (found == opened.end()) {
            if (opened.size() == 12) { opened.erase(opened.begin()); tabs->DeleteItem(0); }
            opened.push_back(base); tabs->AppendItem(utf(article->title));
            tabs->SelectItem(static_cast<int>(opened.size() - 1));
        } else tabs->SelectItem(static_cast<int>(found - opened.begin()));
        selecting = false;
        save();
        render();
        return true;
    }
};
}
bool ShowOfflineDocumentation(wxWindow* parent, const wxString& route) {
    wxString destination = route;
    if (destination.empty()) {
        if (wxGetApp().app_config) destination = utf(wxGetApp().app_config->get("offline_documentation_article"));
        if (!find(destination)) destination = "windows/README.md";
    }
    if (!find(destination)) return false;
    static wxWeakRef<Reader> reader;
    if (!reader) reader = new Reader(parent);
    reader->open(destination);
    reader->Show(); reader->Raise();
    return true;
}
}}}
