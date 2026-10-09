#pragma once
#include <wx/panel.h>
#include <wx/timer.h>
#include <vector>
#include <string>
class SearchField;
class CollapsibleFilterBar;
class Button;
class wxStaticText;
class wxScrolledWindow;

namespace Slic3r::GUI {
class StatusHubPanel final : public wxPanel {
public:
    explicit StatusHubPanel(wxWindow* parent);
    ~StatusHubPanel() override;
private:
    void refresh();
    void filter();
    wxTimer m_timer;
    CollapsibleFilterBar* m_filters = nullptr;
    SearchField* m_search = nullptr;
    Button* m_retry = nullptr;
    wxScrolledWindow* m_body = nullptr;
    wxStaticText* m_empty = nullptr;
    struct Row { wxStaticText* text; wxString full_text; };
    std::vector<Row> m_rows;
};
}
