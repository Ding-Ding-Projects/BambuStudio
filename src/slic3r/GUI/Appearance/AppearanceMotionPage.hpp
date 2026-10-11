#ifndef slic3r_GUI_Appearance_AppearanceMotionPage_hpp_
#define slic3r_GUI_Appearance_AppearanceMotionPage_hpp_

// The appearance editor's Animation and transition tab.
//
// It edits the timing the real renderer consumers play for the exact element
// the editor targets (ElementMotion.hpp): duration, delay and easing of its
// hover, focus, press, selection and enable transitions (the kit
// StateHandler), and the opening fade of the window that contains it (the
// shared entrance helper and MD3::Motion::FadeIn, this editor included).
// Values are ordinary element-style properties, so they persist, reset, join
// presets, export and import with everything else, and every save is
// recorded in Version history. Undo and redo step through this editor's
// motion edits. A curve preview plays the same arithmetic the element uses,
// and settles at once under reduced motion. Every motion capability the
// renderer cannot provide stays listed, disabled, with its reason.

#include <functional>
#include <string>
#include <vector>

#include <wx/panel.h>
#include <wx/timer.h>

#include "ElementMotion.hpp"

class wxBoxSizer;
class Button;
class ComboBox;
class Label;
class SearchField;
class SpinInput;

namespace Slic3r { namespace GUI {

class MotionTimingEditor;
class MotionCurvePreview;

class AppearanceMotionPage : public wxPanel
{
public:
    // `element_id` is the editor's own id string, kept by reference so a
    // retargeted editor is read without rebuilding; `anchor` returns the
    // element the editor is anchored to (or null).
    AppearanceMotionPage(wxWindow *parent, const std::string &element_id, std::function<wxWindow *()> anchor);
    ~AppearanceMotionPage() override;

    // The editor now targets another element (or its window opened): re-read
    // everything. Undo and redo keep their edits, which name their element.
    void retarget();

    // The fixed element id of the appearance editor card itself, whose
    // opening fade is a window entrance.
    static const char *editor_card_id();

private:
    void build();
    void build_capabilities(wxWindow *parent, wxBoxSizer *sizer);
    void schedule_refresh();
    void refresh();
    void refresh_reduced();
    void refresh_entrance();
    void refresh_preview();
    void refresh_kept();
    void refresh_history_buttons();
    void filter_capabilities();
    void relayout();

    ElementMotion::Trigger state_trigger() const;
    void write(const std::string &id, ElementMotion::Trigger trigger, ElementMotion::Field field,
               const nlohmann::json &value);
    void apply_edit(const ElementMotion::Edit &edit, bool forward);
    void reset_element_motion();
    void persist();
    wxString describe(const ElementMotion::Edit &edit) const;

    const std::string &         m_id;
    std::function<wxWindow *()> m_anchor;
    ElementMotion::EditHistory  m_history;
    int                         m_registry_token { 0 };
    bool                        m_save_pending { false };
    bool                        m_refresh_pending { false };
    bool                        m_last_reduced { false };
    wxTimer                     m_reduced_timer;

    Label *             m_reduced_note { nullptr };
    Label *             m_state_target { nullptr };
    ComboBox *          m_trigger { nullptr };
    MotionTimingEditor *m_state_editor { nullptr };

    MotionCurvePreview *m_preview { nullptr };
    ComboBox *          m_preview_speed { nullptr };
    SpinInput *         m_preview_position { nullptr };
    Label *             m_preview_note { nullptr };

    Label *             m_entrance_note { nullptr };
    MotionTimingEditor *m_entrance_editor { nullptr };
    std::string         m_entrance_id;
    int                 m_entrance_default { 0 };

    Button *m_undo { nullptr };
    Button *m_redo { nullptr };
    Button *m_reset_motion { nullptr };
    Label * m_kept { nullptr };

    SearchField *m_capability_search { nullptr };
    struct CapabilityRow
    {
        const char *area;
        Label *     title;
        Label *     detail;
        wxString    text; // what the search matches
    };
    std::vector<CapabilityRow>                  m_capability_rows;
    std::vector<std::pair<const char *, Label *>> m_capability_areas;
    Label *                                     m_capability_count { nullptr };
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_Appearance_AppearanceMotionPage_hpp_
