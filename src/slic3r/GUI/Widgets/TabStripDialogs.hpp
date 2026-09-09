#ifndef slic3r_GUI_TabStripDialogs_hpp_
#define slic3r_GUI_TabStripDialogs_hpp_

// The three companion surfaces of the shared TabStrip:
//
//   * MoveToGroupDialog  -- the "Move into group..." picker: existing groups
//     listed with name, colour and member count, a create-new-group path, an
//     honest empty state, its own SearchField (regex builder attached) and
//     full keyboard operation (arrows / Enter / Escape).
//   * BulkCloseDialog    -- "Close tabs containing text" and its inverse: one
//     SearchField for the text (plain by default, the builder for regex), a
//     mode switch, the include-pinned choice, a live preview with the match
//     mode and the affected-tab count, and a Close button that refuses empty
//     or invalid patterns.
//   * TabSearchDialog    -- the four tab-discovery searches (current strip,
//     inside one group, groups by name, master over every strip). Results
//     show surface / strip / group / pinned / hidden state; Enter activates.
//
// Every dialog rides the MD3Dialog shell and is placed beside its anchor so
// it stays visually attached to the strip that opened it.

#include <functional>
#include <string>
#include <vector>

#include <wx/colour.h>
#include <wx/string.h>

#include "MD3Dialog.hpp"
#include "TabStripModel.hpp"

class SearchField;
class Label;
class LabeledCheckBox;
class TextInput;

namespace Slic3r { namespace GUI {

class ListBox;
class TabStrip;

// Anchored placement: below `anchor` when there is room, else above, always
// inside the display that holds the anchor. Falls back to centring on the
// parent when anchor is null.
void PlaceDialogBesideAnchor(wxDialog *dialog, wxWindow *anchor, const wxRect *anchor_rect_screen = nullptr);

// Matcher over a SearchField's current regex / case / whole-word / multiline
// flags; `pattern_ok` receives whether a regex pattern compiled.
MD3::Tabs::Matcher MatcherForField(const SearchField *field, bool *pattern_ok = nullptr);

class GroupNameDialog : public MD3Dialog
{
public:
    GroupNameDialog(wxWindow *parent, const wxString &title, const wxString &value);
    wxString GetValue() const;

private:
    ::TextInput *m_input = nullptr;
};

class MoveToGroupDialog : public MD3Dialog
{
public:
    // Returns wxID_OK with SelectedGroup() >= 0, or wxID_OK with
    // CreateRequested() when the user chose the create path.
    MoveToGroupDialog(wxWindow *parent, wxWindow *anchor, const MD3::Tabs::Model &model, const std::string &tab_id);

    int  SelectedGroup() const { return m_selected_group; }
    bool CreateRequested() const { return m_create; }

private:
    void Refilter();
    void Accept();

    const MD3::Tabs::Model &m_model;
    std::string             m_tab_id;
    SearchField *           m_search = nullptr;
    ListBox *               m_list   = nullptr;
    Label *                 m_empty  = nullptr;
    std::vector<int>        m_visible_groups; // group ids in list order
    int                     m_selected_group = -1;
    bool                    m_create         = false;
};

class BulkCloseDialog : public MD3Dialog
{
public:
    BulkCloseDialog(wxWindow *parent, wxWindow *anchor, const MD3::Tabs::Model &model, bool not_containing,
                    const wxString &close_verb);

    const std::vector<std::string> &Ids() const { return m_preview.ids; }

private:
    void Refresh_();

    const MD3::Tabs::Model &m_model;
    bool                    m_not_containing = false;
    wxString                m_close_verb;
    SearchField *           m_search  = nullptr;
    LabeledCheckBox *       m_invert  = nullptr;
    LabeledCheckBox *       m_pinned  = nullptr;
    Label *                 m_summary = nullptr;
    ListBox *               m_list    = nullptr;
    Button *                m_close   = nullptr;
    MD3::Tabs::ClosePreview m_preview;
};

class TabSearchDialog : public MD3Dialog
{
public:
    enum class Scope { Strip, Group, Groups, Master };

    // strips: the strips to search (one for Strip / Group / Groups, all for
    // Master). group_id applies to Scope::Group.
    TabSearchDialog(wxWindow *parent, wxWindow *anchor, Scope scope, std::vector<TabStrip *> strips, int group_id = -1);

    // Set when the dialog closed with wxID_OK.
    TabStrip *                ChosenStrip() const { return m_chosen_strip; }
    const MD3::Tabs::SearchHit &ChosenHit() const { return m_chosen_hit; }

private:
    void Refilter();
    void Accept();

    Scope                    m_scope;
    std::vector<TabStrip *>  m_strips;
    int                      m_group_id = -1;
    SearchField *            m_search   = nullptr;
    ListBox *                m_list     = nullptr;
    Label *                  m_empty    = nullptr;
    struct Row
    {
        TabStrip *           strip = nullptr;
        MD3::Tabs::SearchHit hit;
    };
    std::vector<Row>         m_rows;
    TabStrip *               m_chosen_strip = nullptr;
    MD3::Tabs::SearchHit     m_chosen_hit;
};

}} // namespace Slic3r::GUI

#endif // slic3r_GUI_TabStripDialogs_hpp_
