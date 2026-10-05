#pragma once

#include <functional>
#include <memory>
#include <string>
#include <wx/string.h>

class wxWindow;

namespace Slic3r::GUI::FeatureServices {

// UI-thread-only registry. No window is owned by the registry.
class SurfaceRegistry {
public:
    enum class CaptureProtection { NotRequired, Applied, Unsupported, Failed };
    using ProtectionObserver = std::function<void(const std::string &, CaptureProtection)>;

    static SurfaceRegistry &instance();
    // Explicit IDs are public schema identifiers, never labels or user data.
    // Duplicate live IDs are rejected. A destroyed window's ID can be reused.
    bool register_surface(wxWindow *window, const std::string &id);
    void register_sensitive(wxWindow *window);
    std::string surface_id(wxWindow *window);
    // Persistent 32-hex identity for permitted history/export producers.
    // Empty on an excluded window or any persistence failure.
    std::string history_identity(wxWindow *window);

    // The complete selected subtree must be registered and non-sensitive.
    // Unknown windows, sensitive ancestors and sensitive descendants deny access.
    bool capture_permitted(wxWindow *window);
    bool history_permitted(wxWindow *window);
    bool export_permitted(wxWindow *window);

    // Only explicit public message sources are translated. Never infer source
    // messages from a rendered label, text entry, filename or document content.
    void record_label(wxWindow *window, const wxString &source);
    void record_tooltip(wxWindow *window, const wxString &source);
    // Explicit neutral accessible name, including for sensitive inputs. The
    // source describes the field's purpose and must never contain its value.
    void record_name(wxWindow *window, const wxString &source);
    // Stop managing a dynamic label without changing its current rendering.
    // Unrecorded labels are always owned by their component.
    void clear_label_source(wxWindow *window);
    void refresh_presentation();
    // Discover replacements and children immediately; idle also calls this.
    void synchronize();
    CaptureProtection capture_protection(wxWindow *window);
    void set_protection_observer(ProtectionObserver observer);

private:
    SurfaceRegistry();
    ~SurfaceRegistry();
    SurfaceRegistry(const SurfaceRegistry &) = delete;
    SurfaceRegistry &operator=(const SurfaceRegistry &) = delete;
    struct Impl;
    std::unique_ptr<Impl> m;
};

}
