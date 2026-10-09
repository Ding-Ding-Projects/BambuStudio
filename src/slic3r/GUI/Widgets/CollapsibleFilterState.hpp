#ifndef slic3r_GUI_CollapsibleFilterState_hpp_
#define slic3r_GUI_CollapsibleFilterState_hpp_

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// Toolkit-free rules behind the shared collapsible filter and statistics
// container (Widgets/CollapsibleFilterBar). Keeping them here lets
// tests/collapsible_filters check every rule without wxWidgets:
//
//   * search bars and filter rows change what a collection shows, so they
//     start expanded; statistics and summaries only describe the collection,
//     so they start collapsed;
//   * the state the user picks is stored per surface (one key in the
//     "collapsible_filters" section of the application config) and read back
//     on the next start; anything unrecognised falls back to the default;
//   * a collapsed bar that is still narrowing the collection always says so:
//     it counts every active filter and names the first few.
namespace Slic3r { namespace GUI { namespace CollapsibleFilters {

enum class Purpose {
    Narrows,   // search bars and filter rows: they change what the collection shows
    Describes, // statistics, legends and summaries: they only describe it
};

inline constexpr bool starts_expanded(Purpose purpose) { return purpose == Purpose::Narrows; }

// Application config section holding one "expanded"/"collapsed" value per
// surface id.
inline const char *config_section() { return "collapsible_filters"; }

// Surface ids become config keys, so they are kept to a small, stable
// alphabet: a lowercase letter first, then lowercase letters, digits, '_',
// '.' or '-', at most 64 characters.
inline bool valid_surface_id(const std::string &id)
{
    if (id.empty() || id.size() > 64)
        return false;
    if (id.front() < 'a' || id.front() > 'z')
        return false;
    for (const char c : id) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        if (!ok)
            return false;
    }
    return true;
}

inline const char *encode(bool expanded) { return expanded ? "expanded" : "collapsed"; }

// Only the two values encode() writes are honoured; an empty, edited or
// foreign value restores the purpose default.
inline bool decode(const std::string &stored, Purpose purpose)
{
    if (stored == "expanded")
        return true;
    if (stored == "collapsed")
        return false;
    return starts_expanded(purpose);
}

enum class AccessibleState { Expanded, Collapsed };

inline AccessibleState accessible_state(bool expanded)
{
    return expanded ? AccessibleState::Expanded : AccessibleState::Collapsed;
}

// What the header must say about filters that are narrowing the collection.
struct Disclosure
{
    bool                     visible { false }; // show the "filters active" line
    std::size_t              active { 0 };      // every active filter, named or not
    std::vector<std::string> named;             // labels listed by name, in order
    std::size_t              more { 0 };        // active filters not listed by name
};

// `active_labels` holds one entry per filter that currently excludes items
// (a search query, a selected chip, a date range...). While the bar is
// expanded the controls themselves show the state, so no line is needed; while
// it is collapsed the line is shown whenever anything is active. Blank labels
// are still counted so a filter can never disappear silently.
inline Disclosure disclose(const std::vector<std::string> &active_labels, bool expanded,
                           std::size_t max_named = 3)
{
    Disclosure result;
    result.active  = active_labels.size();
    result.visible = !expanded && result.active > 0;
    for (const std::string &label : active_labels) {
        const bool blank = label.find_first_not_of(" \t\r\n") == std::string::npos;
        if (!blank && result.named.size() < max_named)
            result.named.push_back(label);
        else
            ++result.more;
    }
    return result;
}

// One collapsible block's state and its persistence. The reader and writer
// are injected so the widget can bind them to the application config and the
// tests to a map.
class Section
{
public:
    using Read  = std::function<std::string(const std::string &section, const std::string &key)>;
    using Write = std::function<void(const std::string &section, const std::string &key, const std::string &value)>;

    Section(std::string surface_id, Purpose purpose)
        : m_surface_id(std::move(surface_id)), m_purpose(purpose), m_expanded(starts_expanded(purpose))
    {
    }

    const std::string &surface_id() const { return m_surface_id; }
    Purpose            purpose() const { return m_purpose; }
    bool               expanded() const { return m_expanded; }
    bool               persistent() const { return valid_surface_id(m_surface_id); }

    // Load the stored state. Without a usable id or value the default stays.
    void restore(const Read &read)
    {
        if (!persistent() || !read)
            return;
        m_expanded = decode(read(config_section(), m_surface_id), m_purpose);
    }

    // Apply a user choice and store it. Returns true when the state changed.
    bool set_expanded(bool expanded, const Write &write)
    {
        const bool changed = expanded != m_expanded;
        m_expanded         = expanded;
        if (persistent() && write)
            write(config_section(), m_surface_id, encode(m_expanded));
        return changed;
    }

    // Flip the state, store it and return the new state.
    bool toggle(const Write &write)
    {
        set_expanded(!m_expanded, write);
        return m_expanded;
    }

private:
    std::string m_surface_id;
    Purpose     m_purpose;
    bool        m_expanded;
};

// Accessibility events a toggle must raise, in order.
enum class AnnounceEvent { Focus, StateChange };

// A collapsible panel drawn on the 3D canvas (ImGui), which has no native
// focus. Its keyboard path is a canvas shortcut: after it the header shows a
// focus ring until the pointer is used again, and assistive technology is
// first moved to the header and then told its new state. A pointer toggle
// only reports the state change. Both are stored like any other section.
class CanvasDisclosure
{
public:
    CanvasDisclosure(std::string surface_id, Purpose purpose) : m_section(std::move(surface_id), purpose) {}

    const Section &section() const { return m_section; }
    bool           expanded() const { return m_section.expanded(); }
    bool           focus_ring_visible() const { return m_focus_ring; }

    void restore(const Section::Read &read) { m_section.restore(read); }

    std::vector<AnnounceEvent> toggle_from_keyboard(const Section::Write &write)
    {
        m_section.toggle(write);
        m_focus_ring = true;
        return {AnnounceEvent::Focus, AnnounceEvent::StateChange};
    }

    std::vector<AnnounceEvent> toggle_from_pointer(const Section::Write &write)
    {
        m_section.toggle(write);
        m_focus_ring = false;
        return {AnnounceEvent::StateChange};
    }

    void pointer_used() { m_focus_ring = false; }

    // Back to the starting state for this session only (nothing is stored);
    // used where a preference asks not to keep the last state.
    void reset_to_default()
    {
        m_section    = Section(m_section.surface_id(), m_section.purpose());
        m_focus_ring = false;
    }

private:
    Section m_section;
    bool    m_focus_ring { false };
};

}}} // namespace Slic3r::GUI::CollapsibleFilters

#endif // slic3r_GUI_CollapsibleFilterState_hpp_
