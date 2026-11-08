#ifndef slic3r_GUI_FeatureFinder_LocalToolsHubPanel_hpp_
#define slic3r_GUI_FeatureFinder_LocalToolsHubPanel_hpp_

// The Local tools page: the first page of the Local tools tab (File > Local
// tools, the command palette or Every feature). Every local tool as a card
// with what it is doing now and one click to open it, a search with the regex
// builder, chips for the pinned tools and each group, pins you can reorder,
// and Recent activity: what the tools finished, newest first, with its full
// history, Keep recent activity and Clear behind the two-key confirmation.
// Design: design/local-tools-2026-11, board Main. The rules live in the
// wx-free LocalToolsHub.

#include <string>
#include <utility>
#include <vector>

#include <wx/panel.h>

#include "LocalToolsHub.hpp"

class Button;
class SearchField;
class StaticBox;
class wxBoxSizer;
class wxScrolledWindow;

namespace Slic3r::GUI {

class LocalToolsHubPanel final : public wxPanel
{
public:
    explicit LocalToolsHubPanel(wxWindow *parent);
    ~LocalToolsHubPanel() override;

    // Opens the page in the Local tools tab.
    static void open();
    // What the tool is doing or holding now, one short line; "" when it has
    // nothing to say. Read from the tool's own files and state, never guessed.
    static wxString status_of(const std::string &feature);

private:
    void build_filters(wxBoxSizer *root);
    void style_filters();
    void set_filter(const std::string &filter);
    void rebuild();
    void rebuild_activity();
    void toggle_pin(const std::string &id);
    void move_pin(const std::string &id, int delta);
    void show_history();
    int  columns_for(int width) const;

    wxWindow *make_card(wxWindow *parent, const LocalTools::Tool &tool, const wxString &status);
    wxWindow *make_activity_card(wxWindow *parent);

    SearchField                                   *m_search { nullptr };
    std::vector<std::pair<std::string, Button *>>  m_filter_buttons;
    wxScrolledWindow                              *m_body { nullptr };
    StaticBox                                     *m_activity_card { nullptr };
    wxBoxSizer                                    *m_activity_rows { nullptr };
    std::string                                    m_filter;
    std::vector<std::string>                       m_pins;
    int                                            m_columns { 0 };
    bool                                           m_wide { false };
    int                                            m_activity_token { 0 };
};

} // namespace Slic3r::GUI

#endif // slic3r_GUI_FeatureFinder_LocalToolsHubPanel_hpp_
