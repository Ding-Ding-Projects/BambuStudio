#ifndef slic3r_GUI_FeatureFinder_FeatureFinderPanel_hpp_
#define slic3r_GUI_FeatureFinder_FeatureFinderPanel_hpp_

// The Every feature page (Help > Every feature, the command palette, or the
// Local tools tab). Every required feature, where it lives in this build, its
// real shortcut, and one click to open it. A feature that is not built yet
// says so and has no Open button. Design: design/local-tools-2026-11, board
// FeatureFinder.

#include <string>
#include <utility>
#include <vector>

#include <wx/panel.h>

#include "FeatureCatalogue.hpp"

class Button;
class SearchField;
class wxBoxSizer;
class wxScrolledWindow;

namespace Slic3r::GUI {

class FeatureFinderPanel final : public wxPanel
{
public:
    explicit FeatureFinderPanel(wxWindow *parent);

    // Why a ready feature cannot be opened right now ("" when it can).
    static wxString unavailable_reason(const FeatureFinder::Feature &feature);
    // Runs the feature's action. Returns false when nothing could be opened.
    static bool open(const FeatureFinder::Feature &feature);

private:
    void build_filters(wxBoxSizer *root);
    void rebuild();
    void rebuild_recent();
    void set_filter(const std::string &filter);
    void set_sort(FeatureFinder::SortOrder sort);
    void open_and_remember(const FeatureFinder::Feature &feature);
    void style_toggle(Button *button, bool on);
    int  columns_for(int width) const;

    wxWindow *make_card(wxWindow *parent, const wxString &title, const std::vector<std::size_t> &indices);
    wxPanel *make_row(wxWindow *parent, const FeatureFinder::Feature &feature);

    SearchField                                         *m_search { nullptr };
    std::vector<std::pair<std::string, Button *>>        m_filter_buttons;
    std::vector<std::pair<FeatureFinder::SortOrder, Button *>> m_sort_buttons;
    wxPanel                                             *m_recent_panel { nullptr };
    wxBoxSizer                                          *m_recent_sizer { nullptr };
    wxScrolledWindow                                    *m_list { nullptr };
    std::string                                          m_filter;
    FeatureFinder::SortOrder                             m_sort { FeatureFinder::SortOrder::Group };
    std::vector<std::string>                             m_recent;
    int                                                  m_columns { 0 };
};

} // namespace Slic3r::GUI

#endif // slic3r_GUI_FeatureFinder_FeatureFinderPanel_hpp_
