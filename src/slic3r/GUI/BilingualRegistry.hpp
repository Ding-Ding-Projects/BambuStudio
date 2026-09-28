#ifndef slic3r_GUI_BilingualRegistry_hpp_
#define slic3r_GUI_BilingualRegistry_hpp_

#include <wx/string.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Slic3r { namespace GUI { namespace I18N {

// In bilingual mode every legacy lookup (_L, _u8L, _CTX, plurals ... all ending
// in I18N::finish()) records the Hong Kong Cantonese that belongs to the English
// it handed to the UI. The display-boundary decorator, the self-drawn widgets,
// MD3 menus and ImGui then ask for the Cantonese of the label they are about to
// show, so no call site has to change. Outside bilingual mode it stays empty.
class BilingualRegistry
{
public:
    static BilingualRegistry &instance();

    // Clears everything; called whenever the language mode is configured.
    void reset(bool enabled);
    bool enabled() const { return m_enabled.load(std::memory_order_relaxed); }

    // `english` is the text the UI received (vocabulary applied) and
    // `cantonese` the catalogue text for the same message. Strings with format
    // placeholders are kept as templates for matching formatted labels.
    void record(const wxString &english, const wxString &cantonese);

    // Cantonese for a label exactly as displayed, or an empty string. Menu
    // mnemonics, accelerators, wrapping newlines, trailing ":"/"..." and
    // formatted templates are handled; the result carries no mnemonic.
    wxString lookup(const wxString &label) const;

    size_t size() const;

    // Windows that already present both languages themselves
    // (apply_localized_text) and must not be decorated a second time. Keyed by
    // address only; the display side drops an entry when its window is destroyed.
    void set_managed(const void *window, bool managed);
    bool is_managed(const void *window) const;

    // Normalisation shared with the tests: drops '&' mnemonics (keeping "&&"
    // as "&"), cuts a "\t" accelerator, joins wrapped lines and collapses
    // whitespace.
    static std::wstring normalize(const wxString &text);
    // Removes a mnemonic from catalogue Cantonese: "檔案(&F)" -> "檔案".
    static wxString strip_mnemonic(const wxString &text);

private:
    BilingualRegistry() = default;

    struct Template {
        std::wstring              english;      // normalised English template
        std::wstring              cantonese;    // Cantonese template, mnemonic removed
        std::vector<std::wstring> literals;     // English literal pieces between placeholders
        std::vector<int>          english_slots; // per English placeholder: position number, or 0 for sequential
        std::wstring              anchor;       // longest literal, used as a quick filter
    };

    bool                 add_template(const std::wstring &english, const std::wstring &cantonese);
    std::wstring         lookup_exact(const std::wstring &key) const;
    std::wstring         lookup_template(const std::wstring &key) const;
    static std::wstring  fill_template(const Template &tpl, const std::vector<std::wstring> &captures);

    std::wstring         resolve(const std::wstring &key) const;

    mutable std::shared_mutex                     m_mutex;
    std::atomic<bool>                             m_enabled { false };
    std::unordered_map<std::wstring, std::wstring> m_exact;
    std::unordered_map<std::wstring, size_t>       m_template_index;
    std::vector<Template>                          m_templates;
    // Paint-time callers ask for the same labels on every repaint, and a
    // template scan is linear, so answers (misses included) are cached until
    // the next record() adds something.
    mutable std::mutex                                     m_cache_mutex;
    mutable std::unordered_map<std::wstring, std::wstring> m_cache;

    mutable std::mutex              m_managed_mutex;
    std::unordered_set<const void *> m_managed;
};

}}} // namespace Slic3r::GUI::I18N

#endif // slic3r_GUI_BilingualRegistry_hpp_
