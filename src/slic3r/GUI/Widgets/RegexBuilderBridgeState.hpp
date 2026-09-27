#ifndef slic3r_GUI_Widgets_RegexBuilderBridgeState_hpp_
#define slic3r_GUI_Widgets_RegexBuilderBridgeState_hpp_

#include <cstdint>
#include <string>
#include <utility>

namespace Slic3r::GUI {

// The wx regex builder and an ImGui search field are driven by different event
// loops.  This small, UI-independent state object makes their hand-off
// explicit: builder callbacks mark individual values as pending, the next
// ImGui frame consumes them, and ordinary ImGui edits flow back into the
// builder snapshot without overwriting an unconsumed builder edit.
struct RegexBuilderValues
{
    std::string pattern;
    bool        regex_enabled = false;
    bool        case_sensitive = false;
    bool        whole_word = false;
    bool        multiline = false;
};

class RegexBuilderBridgeState
{
public:
    const RegexBuilderValues &values() const { return m_values; }

    void synchronize_from_host(const RegexBuilderValues &host)
    {
        if ((m_pending & Pattern) == 0)
            m_values.pattern = host.pattern;
        if ((m_pending & Regex) == 0)
            m_values.regex_enabled = host.regex_enabled;
        if ((m_pending & Case) == 0)
            m_values.case_sensitive = host.case_sensitive;
        if ((m_pending & Word) == 0)
            m_values.whole_word = host.whole_word;
        if ((m_pending & Multiline) == 0)
            m_values.multiline = host.multiline;
    }

    bool apply_pending_to_host(RegexBuilderValues &host)
    {
        bool changed = false;
        if ((m_pending & Pattern) != 0) {
            changed |= host.pattern != m_values.pattern;
            host.pattern = m_values.pattern;
        }
        if ((m_pending & Regex) != 0) {
            changed |= host.regex_enabled != m_values.regex_enabled;
            host.regex_enabled = m_values.regex_enabled;
        }
        if ((m_pending & Case) != 0) {
            changed |= host.case_sensitive != m_values.case_sensitive;
            host.case_sensitive = m_values.case_sensitive;
        }
        if ((m_pending & Word) != 0) {
            changed |= host.whole_word != m_values.whole_word;
            host.whole_word = m_values.whole_word;
        }
        if ((m_pending & Multiline) != 0) {
            changed |= host.multiline != m_values.multiline;
            host.multiline = m_values.multiline;
        }
        m_pending = 0;
        return changed;
    }

    void set_pattern_from_builder(std::string pattern)
    {
        m_values.pattern = std::move(pattern);
        m_pending |= Pattern;
    }
    void set_regex_from_builder(bool value)
    {
        m_values.regex_enabled = value;
        m_pending |= Regex;
    }
    void set_case_from_builder(bool value)
    {
        m_values.case_sensitive = value;
        m_pending |= Case;
    }
    void set_word_from_builder(bool value)
    {
        m_values.whole_word = value;
        m_pending |= Word;
    }
    void set_multiline_from_builder(bool value)
    {
        m_values.multiline = value;
        m_pending |= Multiline;
    }

    bool has_pending_changes() const { return m_pending != 0; }

private:
    enum Pending : std::uint8_t {
        Pattern = 1u << 0,
        Regex   = 1u << 1,
        Case    = 1u << 2,
        Word    = 1u << 3,
        Multiline = 1u << 4,
    };

    RegexBuilderValues m_values;
    std::uint8_t        m_pending = 0;
};

} // namespace Slic3r::GUI

#endif
