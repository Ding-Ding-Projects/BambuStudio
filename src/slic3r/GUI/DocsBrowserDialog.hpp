#ifndef slic3r_GUI_DocsBrowserDialog_hpp_
#define slic3r_GUI_DocsBrowserDialog_hpp_

#include "GUI_Utils.hpp"

#include <map>
#include <string>
#include <vector>

#include <wx/dataview.h>

class Button;
class Label;
class SearchField;
class StaticBox;
class wxWebView;
class wxWebViewEvent;

namespace Slic3r::GUI {

// In-app, offline documentation browser (Help > Documentation, F1, and the
// command palette's "Documentation / ..." rows).
//
//   * Every docs/features/**/*.md article is bundled at build time into
//     resources/docs/bundle.json by cmake/modules/BundleDocs.cmake; nothing is
//     fetched from the network and the browser works with no connection.
//   * Articles render through the ONE shared Markdown renderer
//     (libslic3r/Markdown) into a wxWebView via SetPage(); the page CSS is
//     built from the live MD3 colour roles so it follows theme and density.
//   * Left: SearchField (plain text default, ".*" regex toggle + full regex
//     builder popover) over titles AND bodies, above a category -> article
//     tree. Right: the rendered article with Back / Forward.
//   * Article-to-article links resolve inside the browser (relative path
//     against the bundle); `#fragment` links scroll within the page; every
//     http(s) link opens the system browser through the usual warning
//     dialog; images come from resources/docs/assets (mirrored docs/ tree).
//   * Keyboard: Tab order search -> tree -> back -> forward -> article;
//     Alt+Left / Alt+Right navigate history; Ctrl+F focuses the search
//     field; Esc closes. Every control carries a screen-reader name.
//
// The dialog is modeless and single-instance: ShowArticle() raises the open
// window and navigates it rather than opening a second one.
class DocsBrowserDialog final : public DPIDialog
{
public:
    // Open (or raise) the browser. `repo_path` is repository-relative with
    // forward slashes ("docs/features/windows/command-palette.md"), optionally
    // with a "#fragment"; empty opens the first category index.
    static void ShowArticle(wxWindow *parent, const std::string &repo_path = std::string());

    explicit DocsBrowserDialog(wxWindow *parent);
    ~DocsBrowserDialog() override;

    // Navigate to a bundled article (repo-relative path, optional fragment).
    // Returns false when the path is not in the bundle.
    bool navigate_to(const std::string &repo_path);

    // --- Pure helpers, exposed for tests -----------------------------------

    struct Article
    {
        std::string path;     // "docs/features/<category>/<file>.md"
        std::string category; // "<category>"
        bool        index { false }; // the category README
        std::string title;    // H1
        std::string body;     // Markdown source
        std::string plain;    // body with markup stripped, for search
    };

    // Parse bundle.json text into articles (sorted by path). Throws on a
    // malformed bundle.
    static std::vector<Article> parse_bundle(const std::string &json_text);

    // Resolve a link written inside `from_path` (repo-relative article) to a
    // repo-relative target, lexically normalised ("../pages/x.md" from
    // "docs/features/windows/a.md" -> "docs/features/pages/x.md"). Fragments
    // are returned separately. Absolute URLs return "" in `target`.
    static void resolve_relative(const std::string &from_path, const std::string &link,
                                 std::string &target, std::string &fragment);

    // Scheme used for in-browser article links inside the rendered HTML.
    static const char *internal_scheme(); // "bambudocs"

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;
    void on_sys_color_changed() override;

private:
    void create_ui();
    void apply_theme();
    bool load_bundle();
    void populate_tree();
    void open_index(size_t index, const std::string &fragment, bool record);
    void update_nav_buttons();
    void go_back();
    void go_forward();
    void select_tree_item(size_t index);
    void on_navigating(wxWebViewEvent &event);
    std::string html_document(const Article &article, const std::string &fragment) const;
    std::string image_url(const std::string &from_path, const std::string &src) const;

    std::vector<Article>                m_articles;
    std::map<std::string, size_t>       m_by_path;
    std::map<std::string, std::string>  m_category_titles; // category -> README H1
    std::vector<std::string>            m_history;          // "path#fragment"
    size_t                              m_history_pos { 0 };
    size_t                              m_current { size_t(-1) };
    bool                                m_suppress_tree_events { false };

    Label              *m_title_label { nullptr };
    Label              *m_status_label { nullptr };
    StaticBox          *m_nav_card { nullptr };
    SearchField        *m_search { nullptr };
    wxDataViewTreeCtrl *m_tree { nullptr };
    std::map<void *, size_t> m_tree_articles; // wxDataViewItem id -> article
    Button             *m_back_button { nullptr };
    Button             *m_forward_button { nullptr };
    Button             *m_close_button { nullptr };
    Label              *m_article_title { nullptr };
    wxWebView          *m_webview { nullptr };
    Label              *m_webview_fallback { nullptr };

    static DocsBrowserDialog *s_instance;
};

} // namespace Slic3r::GUI

#endif // slic3r_GUI_DocsBrowserDialog_hpp_
