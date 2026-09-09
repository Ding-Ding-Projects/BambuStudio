#ifndef slic3r_GUI_CommandPaletteIndex_hpp_
#define slic3r_GUI_CommandPaletteIndex_hpp_

// Pure-data half of the command palette. Nothing here needs a running
// wxApp, GUI_App, or the frame: it is the part the Catch2 completeness
// guard links against. The palette dialog (CommandPalette.cpp) and the main
// frame consume it; the tests under tests/command_palette assert on it.

#include <functional>
#include <string>
#include <vector>

#include <wx/accel.h>
#include <wx/defs.h>

namespace Slic3r::GUI::PaletteIndex {

// ---------------------------------------------------------------------------
// Accelerators
// ---------------------------------------------------------------------------

// Command id the palette accelerator posts as wxEVT_MENU.
constexpr int kPaletteCommandId = wxID_HIGHEST + 90;
// F1 opens the in-app documentation browser (DocsBrowserDialog).
constexpr int kDocsCommandId    = wxID_HIGHEST + 91;
// Ctrl+Numpad1..6 fake the Ctrl+1..6 window-menu chords on Windows; the id
// for Numpad N is kNumpadTabBaseId + N - 1 and selects workspace tab N - 1.
constexpr int kNumpadTabBaseId  = wxID_HIGHEST + 1;
constexpr int kNumpadTabCount   = 6;

// The ONE accelerator table the main frame installs. Every frame-level
// chord lives here so a later SetAcceleratorTable() can never clobber an
// earlier one (which is exactly how Ctrl+F used to wipe the numpad entries).
std::vector<wxAcceleratorEntry> main_frame_accelerators();
bool is_docs_accelerator(const wxAcceleratorEntry &entry);
const char *docs_shortcut_label(); // "F1"

// True for the palette chord: Ctrl+Shift+F, the one global shortcut.
bool is_palette_accelerator(const wxAcceleratorEntry &entry);
// True for Ctrl+Numpad<n> (1..6).
bool is_numpad_tab_accelerator(const wxAcceleratorEntry &entry, int n);
// Human-readable chord as shown in the keyboard-shortcuts dialog and docs.
const char *palette_shortcut_label(); // "Ctrl+Shift+F"

// ---------------------------------------------------------------------------
// Persisted size choice
// ---------------------------------------------------------------------------

enum class PaletteSize { Card, FullWindow };

constexpr const char *kPaletteSizeKey = "palette_size";

PaletteSize parse_palette_size(const std::string &stored); // "full" -> FullWindow, else Card
std::string palette_size_value(PaletteSize size);          // "card" / "full"

// Store-agnostic persistence so the choice can be round-tripped in a test
// without an AppConfig: `get(key)` returns "" when unset.
PaletteSize load_palette_size(const std::function<std::string(const std::string &)> &get);
void        store_palette_size(PaletteSize size,
                               const std::function<void(const std::string &, const std::string &)> &set);

// ---------------------------------------------------------------------------
// Preferences settings index
// ---------------------------------------------------------------------------

// Preferences dialog page order (m_book pages). The developer page only
// exists in non-public builds; its index is still stable.
enum PreferencePage : int {
    PageAppearance = 0,
    PageGeneral    = 1,
    PageUser       = 2,
    Page3D         = 3,
    PageOther      = 4,
    PageDeveloper  = 5,
};

const std::vector<const char *> &preference_page_names();

struct PreferenceEntry
{
    const char *key;   // AppConfig key the Preferences row is bound to
    const char *title; // untranslated row label (translate with _() at display)
    const char *desc;  // untranslated secondary text
    int         page;  // PreferencePage owning the row
};

// Every setting the Preferences dialog renders. Kept in step with
// Preferences.cpp by the source-scan guard in tests/command_palette: a
// create_item_* call whose key is missing here fails that test.
const std::vector<PreferenceEntry> &preference_entries();

// Teleport target for a setting key: null when the key is not a preference.
const PreferenceEntry *find_preference(const std::string &key);

// ---------------------------------------------------------------------------
// Workspace tabs (MainFrame::TabPosition)
// ---------------------------------------------------------------------------

struct WorkspaceTab
{
    const char *title;    // "Go to Prepare"
    const char *desc;
    int         position; // MainFrame::TabPosition value
};

const std::vector<WorkspaceTab> &workspace_tabs();

// ---------------------------------------------------------------------------
// Documentation articles (docs/features/**/*.md, README indexes excluded)
// ---------------------------------------------------------------------------

struct Article
{
    const char *path;  // repository-relative, forward slashes
    const char *title; // the article's H1
};

const std::vector<Article> &documentation_articles();
// Repository URL of the article's Markdown source. The palette itself opens
// articles in the in-app DocsBrowserDialog; this is the external fallback
// (and what the docs browser uses for links that leave the bundle).
std::string article_url(const Article &article);

} // namespace Slic3r::GUI::PaletteIndex

#endif // slic3r_GUI_CommandPaletteIndex_hpp_
