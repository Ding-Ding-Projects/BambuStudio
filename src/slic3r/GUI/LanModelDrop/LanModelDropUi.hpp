#pragma once
// LAN model drop, the visible surfaces (docs/features/application-integration/lan-model-drop.md):
// the invite (link, Copy link, the QR code of exactly that link, New link, the LAN address picker),
// the rows of the Preferences section and the "Invite someone to send a model" dialog. Every control
// is a kit widget; each window listens to the station view and updates itself on the GUI thread.

#include <string>

#include <wx/panel.h>
#include <wx/string.h>

class Button;
class ComboBox;
class Label;
class TextInput;

namespace Slic3r { namespace GUI { namespace LanModelDrop {

// A label whose English text follows the station view. In bilingual mode the decorator writes the
// English with the Cantonese into the label itself, so the label is never asked what it shows: the
// English it was last given is kept here, and setting the same English again does nothing (no text
// set back to English only, no layout, no dialog refit). A real change asks the decorator for a pass
// at once, so the Cantonese follows within a tick instead of the next sweep.
class LiveText
{
public:
    LiveText() = default;
    explicit LiveText(Label *label);
    // Sets and wraps the text; returns true when the English changed.
    bool set(const wxString &english);
    Label *operator->() const { return m_label; }
    Label *get() const { return m_label; }

private:
    Label   *m_label = nullptr;
    wxString m_english;
};

// The QR code of one text, drawn with the bundled encoder (LocalSecurity/PairingQr): black modules
// on white with a four-module quiet zone in both themes. Hidden while there is no text.
class QrView : public wxPanel
{
public:
    QrView(wxWindow *parent, int edge_dip);
    // Returns false when the text cannot be encoded (the view then hides itself).
    bool SetText(const std::string &text);
    const std::string &GetText() const { return m_text; }

private:
    void render();

    std::string        m_text;
    unsigned           m_modules = 0;
    std::string        m_dark; // one byte per module, row-major
};

// The invite. In Preferences it sits right under the switch, so it is the first thing shown once the
// option is on; the dialog uses the same panel and offers to switch the option on.
class InvitePanel : public wxPanel
{
public:
    struct Options
    {
        bool show_heading  = true;
        bool offer_turn_on = false;
    };
    InvitePanel(wxWindow *parent, const Options &options);
    ~InvitePanel() override;

private:
    void refresh();
    void copy_link();
    void announce(const wxString &text);
    void show_larger();

    Options     m_options;
    int         m_listener = 0;
    bool        m_focus_copy_when_ready = false;
    bool        m_waiting_for_code = false;
    std::string m_code_before;

    Label     *m_off_note      = nullptr;
    Button    *m_turn_on       = nullptr;
    LiveText   m_pending_note;
    wxPanel   *m_link_area     = nullptr;
    QrView    *m_qr            = nullptr;
    TextInput *m_link          = nullptr;
    Button    *m_copy          = nullptr;
    Button    *m_new_link      = nullptr;
    Button    *m_larger        = nullptr;
    Label     *m_lan_label     = nullptr;
    ComboBox  *m_lan_choice    = nullptr;
    LiveText   m_manual;
    LiveText   m_source_note;
    LiveText   m_fixed_note;
    LiveText   m_live;
    LiveText   m_waiting_label;
    Button    *m_show_waiting  = nullptr;
};

// Rows of the Preferences section. Each one is a window, so the settings search can show, hide and
// teleport to it; Preferences.cpp registers them under the keys listed in CommandPaletteIndex.cpp.
wxWindow *create_intro_row(wxWindow *parent);
wxWindow *create_status_row(wxWindow *parent);
wxWindow *create_address_row(wxWindow *parent);
wxWindow *create_key_row(wxWindow *parent);
wxWindow *create_code_row(wxWindow *parent);

}}} // namespace Slic3r::GUI::LanModelDrop
