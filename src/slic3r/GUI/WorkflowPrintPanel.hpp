#ifndef slic3r_GUI_WorkflowPrintPanel_hpp_
#define slic3r_GUI_WorkflowPrintPanel_hpp_

#include "WorkflowPrintState.hpp"
#include "Widgets/MD3ScrolledWindow.hpp"
#include <functional>
#include <map>
#include <utility>
#include <vector>

class Button;
class StaticBox;
class Label;
class wxSizerItem;
class wxBoxSizer;

namespace Slic3r { namespace GUI {

struct WorkflowPrintSummary {
    WorkflowPrint::Availability availability;
    wxString plate;
    wxString printer;
    wxString estimate;
    wxString material;
    wxString output_label;
    wxString slice_label;
    wxString disabled_reason;
};

// The review page has no printer connection or slicing ownership. All explicit
// actions return to MainFrame's existing controls and their confirmation paths.
class WorkflowPrintPanel : public MD3ScrolledWindow {
public:
    using ReadSummary = std::function<WorkflowPrintSummary()>;
    using RunAction = std::function<void(WorkflowPrint::Action, wxWindow*)>;
    WorkflowPrintPanel(wxWindow* parent, ReadSummary read_summary, RunAction run_action);
    void RefreshSummary();
    void Rescale();
    void ApplyTheme();

private:
    Button* AddAction(wxWindow* parent, wxSizer* sizer, const wxString& label,
                      WorkflowPrint::Action action, bool primary = false);
    Label* AddText(wxWindow* parent, wxSizer* sizer, const wxString& text, bool heading = false);
    void Reflow();
    void SetText(Label* label, const wxString& text);
    ReadSummary m_read_summary;
    RunAction m_run_action;
    WorkflowPrintSummary m_summary;
    Label* m_status = nullptr;
    Label* m_reason = nullptr;
    Label* m_plate = nullptr;
    Label* m_printer = nullptr;
    Label* m_estimate = nullptr;
    Label* m_material = nullptr;
    Button* m_output = nullptr;
    Button* m_slice = nullptr;
    Button* m_slice_print = nullptr;
    Button* m_slice_send = nullptr;
    std::vector<StaticBox*> m_cards;
    std::vector<Label*> m_text;
    std::vector<Label*> m_headings;
    std::map<Label*, wxString> m_labels;
    std::vector<Button*> m_buttons;
    std::vector<std::pair<wxSizerItem*, bool>> m_spacing;
    std::vector<wxSizerItem*> m_spacers;
    bool m_reflowing = false;
    wxBoxSizer* m_columns = nullptr;
    wxSizerItem* m_summary_item = nullptr;
};

}} // namespace Slic3r::GUI
#endif
