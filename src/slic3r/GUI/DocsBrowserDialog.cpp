#include "DocsBrowserDialog.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MaterialIcon.hpp"
#include "Widgets/MD3DialogChrome.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/SearchField.hpp"
#include "Widgets/StateColor.hpp"
#include "Widgets/StaticBox.hpp"
#include "Widgets/WebView.hpp"

#include "libslic3r/Markdown.hpp"
#include "libslic3r/Utils.hpp"
#include "nlohmann/json.hpp"

#include <wx/accel.h>
#include <wx/sizer.h>
#include <wx/webview.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <boost/log/trivial.hpp>

namespace Slic3r::GUI {

DocsBrowserDialog *DocsBrowserDialog::s_instance = nullptr;

namespace {

constexpr int kBackId    = wxID_HIGHEST + 1201;
constexpr int kForwardId = wxID_HIGHEST + 1202;
constexpr int kSearchId  = wxID_HIGHEST + 1203;

const char *kExternalRepoBase = "https://github.com/Ding-Ding-Projects/BambuStudio/blob/main/";

std::string hex(MD3::Role role)
{
    return StateColor::semantic(role).GetAsString(wxC2S_HTML_SYNTAX).ToStdString();
}

bool is_absolute_url(const std::string &s)
{
    // scheme ":" (RFC 3986), which also covers mailto: and the internal scheme.
    size_t i = 0;
    if (s.empty() || !std::isalpha(static_cast<unsigned char>(s[0]))) return false;
    while (i < s.size() && (std::isalnum(static_cast<unsigned char>(s[i])) || s[i] == '+' || s[i] == '.' || s[i] == '-')) ++i;
    return i > 1 && i < s.size() && s[i] == ':';
}

std::string percent_encode_path(const std::string &path)
{
    static const char *hexdigits = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : path) {
        if (std::isalnum(c) || c == '/' || c == '.' || c == '-' || c == '_' || c == ':' || c == '~') out.push_back(static_cast<char>(c));
        else { out.push_back('%'); out.push_back(hexdigits[c >> 4]); out.push_back(hexdigits[c & 15]); }
    }
    return out;
}

std::string split_fragment(const std::string &link, std::string &fragment)
{
    const size_t hash = link.find('#');
    if (hash == std::string::npos) { fragment.clear(); return link; }
    fragment = link.substr(hash + 1);
    return link.substr(0, hash);
}

std::string js_string(const std::string &s)
{
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') { out.push_back('\\'); out.push_back(c); }
        else if (c == '\n') out += "\\n";
        else if (c == '<') out += "\\x3c";
        else out.push_back(c);
    }
    return out + "\"";
}

} // namespace

// ---------------------------------------------------------------------------
// Pure helpers
// ---------------------------------------------------------------------------

const char *DocsBrowserDialog::internal_scheme() { return "bambudocs"; }

std::vector<DocsBrowserDialog::Article> DocsBrowserDialog::parse_bundle(const std::string &json_text)
{
    const nlohmann::json j = nlohmann::json::parse(json_text);
    if (!j.is_object() || j.value("version", 0) != 1 || !j.contains("articles") || !j["articles"].is_array())
        throw std::runtime_error("docs bundle: unexpected shape");
    std::vector<Article> articles;
    for (const auto &a : j["articles"]) {
        Article article;
        article.path     = a.at("path").get<std::string>();
        article.category = a.at("category").get<std::string>();
        article.index    = a.value("index", false);
        article.title    = a.at("title").get<std::string>();
        article.body     = a.at("body").get<std::string>();
        article.plain    = Markdown::plain_text(article.body);
        articles.push_back(std::move(article));
    }
    std::sort(articles.begin(), articles.end(), [](const Article &l, const Article &r) { return l.path < r.path; });
    return articles;
}

void DocsBrowserDialog::resolve_relative(const std::string &from_path, const std::string &link,
                                         std::string &target, std::string &fragment)
{
    const std::string bare = split_fragment(link, fragment);
    target.clear();
    if (bare.empty() || is_absolute_url(bare)) return;
    std::filesystem::path base = std::filesystem::path(from_path).parent_path();
    std::filesystem::path joined = (base / bare).lexically_normal();
    target = joined.generic_string();
    // lexically_normal keeps a trailing slash for "dir/"; article targets are files.
    while (!target.empty() && target.back() == '/') target.pop_back();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void DocsBrowserDialog::ShowArticle(wxWindow *parent, const std::string &repo_path)
{
    if (s_instance == nullptr) {
        s_instance = new DocsBrowserDialog(parent);
        s_instance->Show();
    }
    if (!repo_path.empty()) {
        if (!s_instance->navigate_to(repo_path))
            BOOST_LOG_TRIVIAL(warning) << "DocsBrowser: article not in bundle: " << repo_path;
    }
    s_instance->Raise();
    s_instance->SetFocus();
}

DocsBrowserDialog::DocsBrowserDialog(wxWindow *parent)
    : DPIDialog(parent, wxID_ANY, _L("Documentation"), wxDefaultPosition, wxDefaultSize,
                wxRESIZE_BORDER | wxBORDER_NONE)
{
    load_bundle();
    create_ui();
    wxGetApp().UpdateDlgDarkUI(this);
    apply_theme();
    populate_tree();

    // Alt+Left / Alt+Right walk history, Ctrl+F focuses the search field.
    std::vector<wxAcceleratorEntry> entries = {
        wxAcceleratorEntry(wxACCEL_ALT, WXK_LEFT, kBackId),
        wxAcceleratorEntry(wxACCEL_ALT, WXK_RIGHT, kForwardId),
        wxAcceleratorEntry(wxACCEL_CTRL, 'F', kSearchId),
    };
    SetAcceleratorTable(wxAcceleratorTable(static_cast<int>(entries.size()), entries.data()));
    Bind(wxEVT_MENU, [this](wxCommandEvent &) { go_back(); }, kBackId);
    Bind(wxEVT_MENU, [this](wxCommandEvent &) { go_forward(); }, kForwardId);
    Bind(wxEVT_MENU, [this](wxCommandEvent &) { if (m_search) m_search->GetTextCtrl()->SetFocus(); }, kSearchId);

    // Modeless: Esc and the caption close both route through wxID_CANCEL.
    Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Close(); }, wxID_CANCEL);
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &) { Destroy(); });
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_ESCAPE) { Close(); return; }
        e.Skip();
    });

    SetMinSize(FromDIP(wxSize(760, 520)));
    SetSize(FromDIP(wxSize(1040, 720)));
    CenterOnParent();
    MD3DialogCaption::FinishChrome(this);

    if (!m_articles.empty()) {
        // Land on the first category index so the page is never blank.
        const auto first_index = std::find_if(m_articles.begin(), m_articles.end(), [](const Article &a) { return a.index; });
        open_index(first_index == m_articles.end() ? 0 : size_t(first_index - m_articles.begin()), std::string(), true);
    }
}

DocsBrowserDialog::~DocsBrowserDialog()
{
    if (s_instance == this) s_instance = nullptr;
}

bool DocsBrowserDialog::load_bundle()
{
    m_articles.clear();
    m_by_path.clear();
    m_category_titles.clear();
    const std::filesystem::path bundle = std::filesystem::path(resources_dir()) / "docs" / "bundle.json";
    std::ifstream in(bundle, std::ios::binary);
    if (!in.good()) {
        BOOST_LOG_TRIVIAL(error) << "DocsBrowser: bundle missing at " << bundle.string();
        return false;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    try {
        m_articles = parse_bundle(buffer.str());
    } catch (const std::exception &e) {
        BOOST_LOG_TRIVIAL(error) << "DocsBrowser: bundle unreadable: " << e.what();
        m_articles.clear();
        return false;
    }
    for (size_t i = 0; i < m_articles.size(); ++i) {
        m_by_path[m_articles[i].path] = i;
        if (m_articles[i].index) m_category_titles[m_articles[i].category] = m_articles[i].title;
    }
    return true;
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

void DocsBrowserDialog::create_ui()
{
    auto *root = new wxBoxSizer(wxVERTICAL);
    root->Add(new MD3DialogCaption(this, _L("Documentation")), 0, wxEXPAND);

    m_title_label = new Label(this, Label::Head_24, _L("Documentation"));
    root->Add(m_title_label, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(24));
    m_status_label = new Label(this, Label::Body_13, wxEmptyString);
    root->Add(m_status_label, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    auto *columns = new wxBoxSizer(wxHORIZONTAL);

    // Left: search + category/article tree.
    m_nav_card = new StaticBox(this);
    auto *nav_sizer = new wxBoxSizer(wxVERTICAL);
    // TRN: Placeholder of the documentation browser's search field (titles and article text).
    m_search = new SearchField(m_nav_card, _L("Search articles"));
    m_search->SetName(_L("Search documentation"));
    m_search->SetOnQuery([this](const wxString &) { populate_tree(); });
    m_search->SetOnRegexToggle([this](bool) { populate_tree(); });
    nav_sizer->Add(m_search, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(8));

    m_tree = new wxDataViewTreeCtrl(m_nav_card, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                    wxDV_SINGLE | wxDV_NO_HEADER | wxBORDER_NONE);
    m_tree->SetName(_L("Documentation articles"));
    m_tree->SetMinSize(FromDIP(wxSize(260, 200)));
    wxGetApp().UpdateDVCDarkUI(m_tree);
    m_tree->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent &e) {
        if (m_suppress_tree_events) return;
        const auto it = m_tree_articles.find(e.GetItem().GetID());
        if (it != m_tree_articles.end() && it->second != m_current) open_index(it->second, std::string(), true);
    });
    m_tree->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent &e) {
        const auto it = m_tree_articles.find(e.GetItem().GetID());
        if (it != m_tree_articles.end()) open_index(it->second, std::string(), true);
    });
    nav_sizer->Add(m_tree, 1, wxEXPAND | wxALL, FromDIP(8));
    m_nav_card->SetSizer(nav_sizer);
    columns->Add(m_nav_card, 0, wxEXPAND | wxRIGHT, FromDIP(16));

    // Right: history controls + rendered article.
    auto *article_sizer = new wxBoxSizer(wxVERTICAL);
    auto *toolbar = new wxBoxSizer(wxHORIZONTAL);
    m_back_button = new Button(this, wxEmptyString);
    m_back_button->SetIconButton(Button::IconShape::Circle, FromDIP(40));
    m_back_button->SetGlyph(MaterialIcon::ArrowBack, FromDIP(20));
    m_back_button->SetName(_L("Back"));
    m_back_button->SetToolTip(_L("Back") + " (Alt+Left)");
    m_back_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { go_back(); });
    m_forward_button = new Button(this, wxEmptyString);
    m_forward_button->SetIconButton(Button::IconShape::Circle, FromDIP(40));
    m_forward_button->SetGlyph(MaterialIcon::ArrowForward, FromDIP(20));
    m_forward_button->SetName(_L("Forward"));
    m_forward_button->SetToolTip(_L("Forward") + " (Alt+Right)");
    m_forward_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { go_forward(); });
    m_article_title = new Label(this, Label::Head_16, wxEmptyString);
    toolbar->Add(m_back_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    toolbar->Add(m_forward_button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(12));
    toolbar->Add(m_article_title, 1, wxALIGN_CENTER_VERTICAL);
    article_sizer->Add(toolbar, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    m_webview = WebView::CreateWebView(this, "about:blank", "Docs");
    if (m_webview != nullptr) {
        m_webview->SetName(_L("Article"));
        m_webview->EnableContextMenu(false);
        m_webview->Bind(wxEVT_WEBVIEW_NAVIGATING, &DocsBrowserDialog::on_navigating, this);
        m_webview->Bind(wxEVT_WEBVIEW_NEWWINDOW, [this](wxWebViewEvent &e) {
            // target=_blank and middle clicks: same routing as a normal click.
            e.Veto();
            wxWebViewEvent nav(wxEVT_WEBVIEW_NAVIGATING, e.GetId(), e.GetURL(), e.GetTarget());
            on_navigating(nav);
        });
        article_sizer->Add(m_webview, 1, wxEXPAND);
    } else {
        m_webview_fallback = new Label(this, Label::Body_14,
            _L("The embedded web view is not available on this PC, so articles cannot be rendered here. The same documentation ships in the docs folder of the source repository."));
        m_webview_fallback->Wrap(FromDIP(560));
        article_sizer->Add(m_webview_fallback, 1, wxEXPAND | wxALL, FromDIP(16));
    }
    columns->Add(article_sizer, 1, wxEXPAND);
    root->Add(columns, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(24));

    auto *actions = new wxBoxSizer(wxHORIZONTAL);
    m_close_button = new Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);
    m_close_button->SetMinSize(FromDIP(wxSize(104, 40)));
    actions->AddStretchSpacer();
    actions->Add(m_close_button, 0);
    root->Add(actions, 0, wxEXPAND | wxALL, FromDIP(24));

    SetSizer(root);
    update_nav_buttons();
}

void DocsBrowserDialog::apply_theme()
{
    const wxColour surface   = StateColor::semantic(MD3::Role::Surface);
    const wxColour card      = StateColor::semantic(MD3::Role::SurfaceContainerLow);
    const wxColour text      = StateColor::semantic(MD3::Role::OnSurface);
    const wxColour secondary = StateColor::semantic(MD3::Role::OnSurfaceVariant);
    const wxColour outline   = StateColor::semantic(MD3::Role::OutlineVariant);

    SetBackgroundColour(surface);
    for (Label *label : {m_title_label, m_status_label, m_article_title}) {
        label->SetBackgroundColour(surface);
        label->SetForegroundColour(label == m_status_label ? secondary : text);
    }
    if (m_webview_fallback) {
        m_webview_fallback->SetBackgroundColour(surface);
        m_webview_fallback->SetForegroundColour(secondary);
    }
    m_nav_card->SetBackgroundColorNormal(card);
    m_nav_card->SetBorderColorNormal(outline);
    m_nav_card->SetBorderWidth(1);
    m_tree->SetBackgroundColour(StateColor::semantic(MD3::Role::SurfaceContainerLowest));
    m_tree->SetForegroundColour(text);

    const StateColor outlined_bg(
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainerHigh), StateColor::Hovered),
        std::pair<wxColour, int>(StateColor::semantic(MD3::Role::SurfaceContainer), StateColor::Pressed),
        std::pair<wxColour, int>(surface, StateColor::Normal));
    m_close_button->SetBackgroundColor(outlined_bg);
    m_close_button->SetBorderColor(StateColor(outline));
    m_close_button->SetTextColor(StateColor(text));

    if (m_webview) {
        m_webview->SetBackgroundColour(surface);
        if (m_current < m_articles.size()) {
            std::string fragment;
            if (m_history_pos > 0 && m_history_pos <= m_history.size()) split_fragment(m_history[m_history_pos - 1], fragment);
            m_webview->SetPage(wxString::FromUTF8(html_document(m_articles[m_current], fragment)), "about:blank");
        }
    }
    Refresh();
}

void DocsBrowserDialog::populate_tree()
{
    m_suppress_tree_events = true;
    m_tree->DeleteAllItems();
    m_tree_articles.clear();

    const wxString query = m_search ? m_search->GetValue() : wxString();
    const bool filtering = !query.IsEmpty();
    SearchField::MatchPass pass(query, m_search && m_search->IsRegexEnabled(), m_search && m_search->IsCaseSensitive(),
                                m_search && m_search->IsWholeWord(), m_search && m_search->IsMultiline());

    size_t shown = 0;
    std::map<std::string, wxDataViewItem> category_items;
    for (size_t i = 0; i < m_articles.size(); ++i) {
        const Article &a = m_articles[i];
        if (filtering && !pass.matches(wxString::FromUTF8(a.title)) && !pass.matches(wxString::FromUTF8(a.plain)))
            continue;
        auto cat = category_items.find(a.category);
        if (cat == category_items.end()) {
            const auto title_it = m_category_titles.find(a.category);
            const std::string title = title_it == m_category_titles.end() ? a.category : title_it->second;
            const wxDataViewItem item = m_tree->AppendContainer(wxDataViewItem(nullptr), wxString::FromUTF8(title));
            cat = category_items.emplace(a.category, item).first;
            const auto index_it = m_by_path.find("docs/features/" + a.category + "/README.md");
            if (index_it != m_by_path.end()) m_tree_articles[item.GetID()] = index_it->second;
        }
        if (a.index) {
            // The category node itself opens the README; count it as shown.
            ++shown;
            continue;
        }
        const wxDataViewItem item = m_tree->AppendItem(cat->second, wxString::FromUTF8(a.title));
        m_tree_articles[item.GetID()] = i;
        ++shown;
    }
    for (const auto &[category, item] : category_items) m_tree->Expand(item);

    if (m_articles.empty())
        m_status_label->SetLabelText(_L("No documentation bundle was found beside the application resources."));
    else if (filtering)
        m_status_label->SetLabelText(wxString::Format(_L("%zu of %zu articles match"), shown, m_articles.size()));
    else
        m_status_label->SetLabelText(wxString::Format(_L("%zu articles, bundled offline"), m_articles.size()));

    m_suppress_tree_events = false;
    if (m_current < m_articles.size()) select_tree_item(m_current);
    Layout();
}

void DocsBrowserDialog::select_tree_item(size_t index)
{
    for (const auto &[id, article] : m_tree_articles) {
        if (article == index) {
            m_suppress_tree_events = true;
            m_tree->Select(wxDataViewItem(id));
            m_tree->EnsureVisible(wxDataViewItem(id));
            m_suppress_tree_events = false;
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

bool DocsBrowserDialog::navigate_to(const std::string &repo_path)
{
    std::string fragment;
    const std::string bare = split_fragment(repo_path, fragment);
    const auto it = m_by_path.find(bare);
    if (it == m_by_path.end()) return false;
    open_index(it->second, fragment, true);
    return true;
}

void DocsBrowserDialog::open_index(size_t index, const std::string &fragment, bool record)
{
    if (index >= m_articles.size()) return;
    const Article &a = m_articles[index];
    m_current = index;
    if (record) {
        // Drop any forward entries, then push.
        if (m_history_pos < m_history.size()) m_history.resize(m_history_pos);
        const std::string entry = fragment.empty() ? a.path : a.path + "#" + fragment;
        if (m_history.empty() || m_history.back() != entry) m_history.push_back(entry);
        m_history_pos = m_history.size();
    }
    m_article_title->SetLabelText(wxString::FromUTF8(a.title));
    SetTitle(wxString::FromUTF8(a.title) + " - " + _L("Documentation"));
    if (m_webview) m_webview->SetPage(wxString::FromUTF8(html_document(a, fragment)), "about:blank");
    select_tree_item(index);
    update_nav_buttons();
    Layout();
}

void DocsBrowserDialog::update_nav_buttons()
{
    if (m_back_button) m_back_button->Enable(m_history_pos > 1);
    if (m_forward_button) m_forward_button->Enable(m_history_pos < m_history.size());
}

void DocsBrowserDialog::go_back()
{
    if (m_history_pos <= 1) return;
    --m_history_pos;
    std::string fragment;
    const std::string path = split_fragment(m_history[m_history_pos - 1], fragment);
    const auto it = m_by_path.find(path);
    if (it != m_by_path.end()) open_index(it->second, fragment, false);
    update_nav_buttons();
}

void DocsBrowserDialog::go_forward()
{
    if (m_history_pos >= m_history.size()) return;
    ++m_history_pos;
    std::string fragment;
    const std::string path = split_fragment(m_history[m_history_pos - 1], fragment);
    const auto it = m_by_path.find(path);
    if (it != m_by_path.end()) open_index(it->second, fragment, false);
    update_nav_buttons();
}

void DocsBrowserDialog::on_navigating(wxWebViewEvent &event)
{
    const std::string url = event.GetURL().ToUTF8().data();
    const std::string scheme_prefix = std::string(internal_scheme()) + "://article/";
    if (url.rfind(scheme_prefix, 0) == 0) {
        event.Veto();
        const std::string target = url.substr(scheme_prefix.size());
        if (!navigate_to(target))
            BOOST_LOG_TRIVIAL(warning) << "DocsBrowser: unresolved internal link " << target;
        return;
    }
    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0 || url.rfind("mailto:", 0) == 0) {
        // External destinations leave the app through the usual warning dialog.
        event.Veto();
        wxGetApp().open_browser_with_warning_dialog(wxString::FromUTF8(url));
        return;
    }
    // about:blank (SetPage), file:// images and in-page fragments stay inside.
    event.Skip();
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

std::string DocsBrowserDialog::image_url(const std::string &from_path, const std::string &src) const
{
    if (is_absolute_url(src)) return src;
    std::string target, fragment;
    resolve_relative(from_path, src, target, fragment);
    // The bundle mirrors docs/ under resources/docs/assets/.
    if (target.rfind("docs/", 0) != 0) return src;
    std::string abs = (std::filesystem::path(resources_dir()) / "docs" / "assets" / target.substr(5)).generic_string();
    if (!abs.empty() && abs[0] != '/') abs = "/" + abs; // "C:/..." -> "/C:/..."
    return "file://" + percent_encode_path(abs);
}

std::string DocsBrowserDialog::html_document(const Article &article, const std::string &fragment) const
{
    Markdown::RenderOptions opts;
    const std::string from = article.path;
    opts.resolve_link = [this, from](const std::string &dest) -> std::string {
        if (dest.empty() || dest[0] == '#' || is_absolute_url(dest)) return std::string();
        std::string target, frag;
        resolve_relative(from, dest, target, frag);
        if (m_by_path.count(target) != 0)
            return std::string(internal_scheme()) + "://article/" + target + (frag.empty() ? "" : "#" + frag);
        // Files outside the bundle (repository README, Postman collections...)
        // open in the browser at the repository.
        return kExternalRepoBase + target + (frag.empty() ? "" : "#" + frag);
    };
    opts.resolve_image = [this, from](const std::string &dest) { return image_url(from, dest); };
    const std::string body = Markdown::render_to_html(article.body, opts);

    std::ostringstream html;
    html << "<!doctype html><html lang=\"" << wxGetApp().current_language_code_safe().BeforeFirst('_').ToStdString()
         << "\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
         << "<title>" << Markdown::escape_html(article.title) << "</title><style>\n"
         << ":root{color-scheme:" << (wxGetApp().dark_mode() ? "dark" : "light") << ";}\n"
         << "html,body{margin:0;background:" << hex(MD3::Role::Surface) << ";color:" << hex(MD3::Role::OnSurface) << ";}\n"
         << "body{font-family:'" << MD3::Type::font_family << "','Segoe UI',system-ui,sans-serif;font-size:14px;line-height:1.6;"
         << "padding:8px 24px 32px 24px;max-width:860px;overflow-wrap:anywhere;}\n"
         << "h1,h2,h3,h4,h5,h6{font-weight:500;line-height:1.3;margin:1.4em 0 .5em;scroll-margin-top:12px;}\n"
         << "h1{font-size:28px;margin-top:.4em;}h2{font-size:22px;border-bottom:1px solid " << hex(MD3::Role::OutlineVariant) << ";padding-bottom:.25em;}h3{font-size:18px;}\n"
         << "a{color:" << hex(MD3::Role::Primary) << ";text-decoration:none;}a:hover,a:focus{text-decoration:underline;}\n"
         << "a:focus-visible{outline:2px solid " << hex(MD3::Role::Primary) << ";outline-offset:2px;border-radius:4px;}\n"
         << "code,pre{font-family:'" << MD3::Type::font_mono << "',Consolas,monospace;font-size:13px;}\n"
         << "code{background:" << hex(MD3::Role::SurfaceContainerHigh) << ";border-radius:6px;padding:.1em .35em;}\n"
         << "pre{background:" << hex(MD3::Role::SurfaceContainerHigh) << ";border-radius:12px;padding:14px 16px;overflow:auto;}\n"
         << "pre code{background:none;padding:0;}\n"
         << "blockquote{margin:1em 0;padding:.25em 16px;border-left:4px solid " << hex(MD3::Role::Outline) << ";color:" << hex(MD3::Role::OnSurfaceVariant) << ";}\n"
         << "table{border-collapse:collapse;display:block;overflow-x:auto;max-width:100%;}\n"
         << "th,td{border:1px solid " << hex(MD3::Role::OutlineVariant) << ";padding:6px 12px;text-align:left;vertical-align:top;}\n"
         << "th{background:" << hex(MD3::Role::SurfaceContainerLow) << ";font-weight:500;}\n"
         << "img{max-width:100%;height:auto;border-radius:12px;}\n"
         << "hr{border:0;border-top:1px solid " << hex(MD3::Role::OutlineVariant) << ";margin:1.5em 0;}\n"
         << "input[type=checkbox]{accent-color:" << hex(MD3::Role::Primary) << ";}\n"
         << "@media (prefers-reduced-motion:no-preference){html{scroll-behavior:smooth;}}\n"
         << "</style></head><body>\n<main id=\"article\" aria-label=\"" << Markdown::escape_html(article.title) << "\">\n"
         << body << "</main>\n";
    if (!fragment.empty())
        html << "<script>(function(){var e=document.getElementById(" << js_string(fragment)
             << ");if(e){e.scrollIntoView();e.setAttribute('tabindex','-1');e.focus();}})();</script>\n";
    html << "</body></html>\n";
    return html.str();
}

// ---------------------------------------------------------------------------
// DPI / theme
// ---------------------------------------------------------------------------

void DocsBrowserDialog::on_dpi_changed(const wxRect &)
{
    if (m_search) m_search->Rescale();
    Layout();
    Refresh();
}

void DocsBrowserDialog::on_sys_color_changed()
{
    apply_theme();
}

} // namespace Slic3r::GUI
