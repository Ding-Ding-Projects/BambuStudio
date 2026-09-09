#include "ElementStyle.hpp"

#include <algorithm>
#include <cmath>

#include <wx/event.h>
#include <wx/file.h>
#include <wx/filename.h>
#include <wx/fontenum.h>
#include <wx/log.h>
#include <wx/settings.h>
#include <wx/window.h>

namespace Slic3r { namespace GUI {

// ---------------------------------------------------------------------------
// Property catalogue
// ---------------------------------------------------------------------------
namespace StyleProp {

const std::vector<std::string> &all()
{
    static const std::vector<std::string> keys = {
        font_family, font_size, font_weight, font_style, underline, strikethrough,
        letter_spacing, line_height, foreground, background, highlight,
        border_color, border_width, radius, padding, margin,
    };
    return keys;
}

bool is_known(const std::string &key)
{
    const auto &keys = all();
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

} // namespace StyleProp

// ---------------------------------------------------------------------------
// Colour / font helpers
// ---------------------------------------------------------------------------
wxColour style_colour_from_json(const nlohmann::json &value, const wxColour &fallback)
{
    if (!value.is_string())
        return fallback;
    wxColour c;
    if (!c.Set(wxString::FromUTF8(value.get<std::string>())) || !c.IsOk())
        return fallback;
    return c;
}

std::string style_colour_to_string(const wxColour &colour)
{
    if (!colour.IsOk())
        return {};
    // Lowercase "#rrggbb", plus "aa" only when translucent: the same shape the
    // site's elementStyles and the colour picker's hex field use.
    wxString s = wxString::Format("#%02x%02x%02x", static_cast<int>(colour.Red()),
                                  static_cast<int>(colour.Green()), static_cast<int>(colour.Blue()));
    if (colour.Alpha() != wxALPHA_OPAQUE)
        s << wxString::Format("%02x", static_cast<int>(colour.Alpha()));
    return std::string(s.ToUTF8().data());
}

wxFont style_font_from_bag(const StyleBag &bag, const wxFont &base)
{
    if (!bag.is_object() || bag.empty())
        return base;
    wxFont f = base.IsOk() ? base : wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);

    auto it = bag.find(StyleProp::font_family);
    if (it != bag.end() && it->is_string()) {
        const wxString face = wxString::FromUTF8(it->get<std::string>());
        // wxFontBase::SetFaceName UnRef()s the font when the face is unknown,
        // which would turn a hand-edited or stale family name into an invalid
        // font. Validate against the session font table first (the bundled
        // faces are registered session-visible, so they pass too).
        if (!face.IsEmpty() && wxFontEnumerator::IsValidFacename(face))
            f.SetFaceName(face);
    }
    it = bag.find(StyleProp::font_size);
    if (it != bag.end() && it->is_number()) {
        const double pt = it->get<double>();
        if (pt >= 4.0 && pt <= 96.0)
            f.SetFractionalPointSize(pt);
    }
    it = bag.find(StyleProp::font_weight);
    if (it != bag.end() && it->is_number()) {
        int w = it->get<int>();
        w     = std::max(100, std::min(900, w));
        f.SetNumericWeight(w);
    }
    it = bag.find(StyleProp::font_style);
    if (it != bag.end() && it->is_string())
        f.SetStyle(it->get<std::string>() == "italic" ? wxFONTSTYLE_ITALIC : wxFONTSTYLE_NORMAL);
    it = bag.find(StyleProp::underline);
    if (it != bag.end() && it->is_boolean())
        f.SetUnderlined(it->get<bool>());
    it = bag.find(StyleProp::strikethrough);
    if (it != bag.end() && it->is_boolean())
        f.SetStrikethrough(it->get<bool>());
    return f;
}

// ---------------------------------------------------------------------------
// StyleRegistry
// ---------------------------------------------------------------------------
StyleRegistry::StyleRegistry()
    : m_active_preset(kDefaultPreset)
    , m_unknown_top_level(nlohmann::json::object())
{
    install_shipped_presets(m_shipped);
}

void StyleRegistry::install_shipped_presets(std::map<std::string, StylePreset> &into)
{
    // Shipped presets are derived from the MD3 type scale and shape tokens
    // (MD3Tokens.hpp): nothing here invents a value the kit does not have.
    into[kDefaultPreset] = StylePreset{}; // no overrides: pure tokens

    StylePreset large;
    large[kEveryElement] = StyleBag{{StyleProp::font_size, 15.5}, {StyleProp::line_height, 1.4}};
    into["Large text"] = large;

    StylePreset compact;
    compact[kEveryElement] = StyleBag{{StyleProp::font_size, 12.5}, {StyleProp::padding, 6}};
    into["Compact text"] = compact;

    StylePreset rounded;
    rounded[kEveryElement] = StyleBag{{StyleProp::radius, 20}}; // Metrics::radius_home
    into["Rounded"] = rounded;

    StylePreset bold;
    bold[kEveryElement] = StyleBag{{StyleProp::font_weight, 600}};
    into["Bold labels"] = bold;
}

const nlohmann::json *StyleRegistry::lookup(const StyleBag *bag, const std::string &key) const
{
    if (!bag || !bag->is_object())
        return nullptr;
    auto it = bag->find(key);
    if (it == bag->end() || it->is_null())
        return nullptr;
    return &*it;
}

nlohmann::json StyleRegistry::resolve_exact(const std::string &id, const std::string &key) const
{
    // 1. user override
    auto e = m_elements.find(id);
    if (e != m_elements.end())
        if (const nlohmann::json *v = lookup(&e->second, key))
            return *v;
    // 2. active preset, element entry then "*"
    if (const StylePreset *p = preset(m_active_preset)) {
        auto pe = p->find(id);
        if (pe != p->end())
            if (const nlohmann::json *v = lookup(&pe->second, key))
                return *v;
    }
    return nlohmann::json();
}

nlohmann::json StyleRegistry::resolve(const std::string &id, const std::string &key) const
{
    // Walk "a/b/c" -> "a/b" -> "a" at every layer, then the preset's "*".
    std::string cur = id;
    while (true) {
        nlohmann::json v = resolve_exact(cur, key);
        if (!v.is_null())
            return v;
        const auto slash = cur.find_last_of('/');
        if (slash == std::string::npos)
            break;
        cur = cur.substr(0, slash);
    }
    if (const StylePreset *p = preset(m_active_preset)) {
        auto star = p->find(kEveryElement);
        if (star != p->end())
            if (const nlohmann::json *v = lookup(&star->second, key))
                return *v;
    }
    return nlohmann::json();
}

bool StyleRegistry::has_override(const std::string &id, const std::string &key) const
{
    auto e = m_elements.find(id);
    return e != m_elements.end() && lookup(&e->second, key) != nullptr;
}

StyleBag StyleRegistry::resolved_bag(const std::string &id) const
{
    StyleBag out = StyleBag::object();
    for (const std::string &key : StyleProp::all()) {
        nlohmann::json v = resolve(id, key);
        if (!v.is_null())
            out[key] = v;
    }
    // Unknown user properties ride along so a consumer can see them.
    auto e = m_elements.find(id);
    if (e != m_elements.end() && e->second.is_object())
        for (auto it = e->second.begin(); it != e->second.end(); ++it)
            if (!StyleProp::is_known(it.key()) && !out.contains(it.key()))
                out[it.key()] = it.value();
    return out;
}

void StyleRegistry::set(const std::string &id, const std::string &key, const nlohmann::json &value)
{
    if (id.empty() || key.empty())
        return;
    StyleBag &bag = m_elements[id];
    if (!bag.is_object())
        bag = StyleBag::object();
    if (value.is_null())
        bag.erase(key);
    else
        bag[key] = value;
    if (bag.empty())
        m_elements.erase(id);
    notify(id);
}

void StyleRegistry::reset_property(const std::string &id, const std::string &key)
{
    auto e = m_elements.find(id);
    if (e == m_elements.end())
        return;
    e->second.erase(key);
    if (e->second.empty())
        m_elements.erase(e);
    notify(id);
}

void StyleRegistry::reset_element(const std::string &id)
{
    if (m_elements.erase(id) > 0)
        notify(id);
}

void StyleRegistry::reset_all()
{
    m_elements.clear();
    m_active_preset = kDefaultPreset;
    notify(kEveryElement);
}

std::vector<std::string> StyleRegistry::overridden_ids() const
{
    std::vector<std::string> ids;
    for (const auto &kv : m_elements)
        ids.push_back(kv.first);
    return ids;
}

const StyleBag *StyleRegistry::user_bag(const std::string &id) const
{
    auto e = m_elements.find(id);
    return e == m_elements.end() ? nullptr : &e->second;
}

bool StyleRegistry::set_active_preset(const std::string &name)
{
    if (!preset(name))
        return false;
    if (m_active_preset == name)
        return true;
    m_active_preset = name;
    notify(kEveryElement);
    return true;
}

std::vector<std::string> StyleRegistry::preset_names() const
{
    std::vector<std::string> names;
    names.push_back(kDefaultPreset);
    for (const auto &kv : m_shipped)
        if (kv.first != kDefaultPreset)
            names.push_back(kv.first);
    for (const auto &kv : m_user_presets)
        names.push_back(kv.first);
    return names;
}

bool StyleRegistry::is_shipped_preset(const std::string &name) const
{
    return m_shipped.find(name) != m_shipped.end();
}

const StylePreset *StyleRegistry::preset(const std::string &name) const
{
    auto u = m_user_presets.find(name);
    if (u != m_user_presets.end())
        return &u->second;
    auto s = m_shipped.find(name);
    if (s != m_shipped.end())
        return &s->second;
    return nullptr;
}

bool StyleRegistry::save_preset(const std::string &name)
{
    StylePreset p;
    // Snapshot: the currently resolved layers flattened per element so the
    // preset reproduces what the user sees, independent of which preset was
    // active when it was saved.
    std::set<std::string> ids;
    for (const auto &kv : m_elements)
        ids.insert(kv.first);
    if (const StylePreset *active = preset(m_active_preset))
        for (const auto &kv : *active)
            ids.insert(kv.first);
    for (const std::string &id : ids) {
        StyleBag bag = StyleBag::object();
        if (id == kEveryElement) {
            if (const StylePreset *active = preset(m_active_preset)) {
                auto star = active->find(kEveryElement);
                if (star != active->end())
                    bag = star->second;
            }
        } else {
            if (const StylePreset *active = preset(m_active_preset)) {
                auto pe = active->find(id);
                if (pe != active->end() && pe->second.is_object())
                    bag.update(pe->second);
            }
            auto e = m_elements.find(id);
            if (e != m_elements.end() && e->second.is_object())
                bag.update(e->second);
        }
        if (!bag.empty())
            p[id] = bag;
    }
    return save_preset(name, p);
}

bool StyleRegistry::save_preset(const std::string &name, const StylePreset &preset)
{
    if (name.empty() || is_shipped_preset(name))
        return false;
    m_user_presets[name] = preset;
    notify(kEveryElement);
    return true;
}

bool StyleRegistry::delete_preset(const std::string &name)
{
    auto u = m_user_presets.find(name);
    if (u == m_user_presets.end())
        return false;
    m_user_presets.erase(u);
    if (m_active_preset == name)
        m_active_preset = kDefaultPreset;
    notify(kEveryElement);
    return true;
}

// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------
nlohmann::json StyleRegistry::to_json() const
{
    nlohmann::json doc = nlohmann::json::object();
    // Unknown top-level keys first so ours win on a key clash.
    if (m_unknown_top_level.is_object())
        for (auto it = m_unknown_top_level.begin(); it != m_unknown_top_level.end(); ++it)
            doc[it.key()] = it.value();
    doc["schema"]       = kSchema;
    doc["activePreset"] = m_active_preset;
    nlohmann::json presets = nlohmann::json::object();
    for (const auto &kv : m_user_presets) {
        nlohmann::json p = nlohmann::json::object();
        for (const auto &e : kv.second)
            p[e.first] = e.second;
        presets[kv.first] = p;
    }
    doc["presets"] = presets;
    nlohmann::json elements = nlohmann::json::object();
    for (const auto &kv : m_elements)
        elements[kv.first] = kv.second;
    doc["elements"] = elements;
    return doc;
}

StyleLoadReport StyleRegistry::from_json(const nlohmann::json &doc)
{
    StyleLoadReport report;
    if (!doc.is_object()) {
        report.ok    = false;
        report.error = "appearance file is not a JSON object";
        m_last_report = report;
        return report;
    }
    auto schema_it = doc.find("schema");
    if (schema_it == doc.end() || !schema_it->is_number_integer()) {
        report.ok    = false;
        report.error = "appearance file has no integer \"schema\" field";
        m_last_report = report;
        return report;
    }
    report.schema = schema_it->get<int>();
    if (report.schema > kSchema) {
        report.ok    = false;
        report.error = "appearance file schema " + std::to_string(report.schema) + " is newer than supported " +
                       std::to_string(kSchema);
        m_last_report = report;
        return report;
    }

    std::map<std::string, StylePreset> user_presets;
    std::map<std::string, StyleBag>    elements;
    nlohmann::json                     unknown_top = nlohmann::json::object();
    std::string                        active      = kDefaultPreset;

    auto read_bag = [&report](const std::string &owner, const nlohmann::json &src) -> StyleBag {
        StyleBag bag = StyleBag::object();
        if (!src.is_object())
            return bag;
        for (auto it = src.begin(); it != src.end(); ++it) {
            bag[it.key()] = it.value(); // kept verbatim, known or not
            if (!StyleProp::is_known(it.key()))
                report.unknown_properties.push_back(owner + "." + it.key());
        }
        return bag;
    };

    for (auto it = doc.begin(); it != doc.end(); ++it) {
        const std::string &key = it.key();
        if (key == "schema")
            continue;
        if (key == "activePreset") {
            if (it->is_string())
                active = it->get<std::string>();
            continue;
        }
        if (key == "presets") {
            if (it->is_object())
                for (auto p = it->begin(); p != it->end(); ++p) {
                    if (is_shipped_preset(p.key()))
                        continue; // a shipped name in a file never shadows the shipped preset
                    StylePreset preset;
                    if (p->is_object())
                        for (auto e = p->begin(); e != p->end(); ++e)
                            preset[e.key()] = read_bag(p.key() + "/" + e.key(), e.value());
                    user_presets[p.key()] = preset;
                }
            continue;
        }
        if (key == "elements") {
            if (it->is_object())
                for (auto e = it->begin(); e != it->end(); ++e) {
                    StyleBag bag = read_bag(e.key(), e.value());
                    if (!bag.empty())
                        elements[e.key()] = bag;
                }
            continue;
        }
        unknown_top[key] = it.value();
        report.unknown_top_level.push_back(key);
    }

    m_user_presets      = std::move(user_presets);
    m_elements          = std::move(elements);
    m_unknown_top_level = std::move(unknown_top);
    m_active_preset     = preset(active) ? active : std::string(kDefaultPreset);
    if (!preset(active))
        report.unknown_properties.push_back("activePreset=" + active + " (not found, using default)");
    m_last_report = report;
    notify(kEveryElement);
    return report;
}

std::string StyleRegistry::dump() const { return to_json().dump(2); }

StyleLoadReport StyleRegistry::parse(const std::string &text)
{
    nlohmann::json doc = nlohmann::json::parse(text, nullptr, false);
    if (doc.is_discarded()) {
        StyleLoadReport r;
        r.ok    = false;
        r.error = "appearance file is not valid JSON";
        m_last_report = r;
        return r;
    }
    return from_json(doc);
}

namespace {

bool read_text_file(const std::string &path, std::string &out, std::string *error)
{
    wxFile file;
    {
        wxLogNull quiet;
        if (!file.Open(wxString::FromUTF8(path), wxFile::read)) {
            if (error)
                *error = "cannot open " + path;
            return false;
        }
    }
    wxString content;
    if (!file.ReadAll(&content, wxConvUTF8)) {
        if (error)
            *error = "cannot read " + path;
        return false;
    }
    out = std::string(content.ToUTF8().data());
    return true;
}

bool write_text_file(const std::string &path, const std::string &text, std::string *error)
{
    const wxFileName fn(wxString::FromUTF8(path));
    if (!fn.GetPath().IsEmpty() && !wxFileName::DirExists(fn.GetPath()) &&
        !wxFileName::Mkdir(fn.GetPath(), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
        if (error)
            *error = "cannot create " + std::string(fn.GetPath().ToUTF8().data());
        return false;
    }
    // Write beside, then rename over: a crash mid-write never leaves a
    // half-written file where the good one was.
    const wxString tmp = fn.GetFullPath() + ".tmp";
    {
        wxFile file;
        wxLogNull quiet;
        if (!file.Open(tmp, wxFile::write) || !file.Write(wxString::FromUTF8(text), wxConvUTF8)) {
            if (error)
                *error = "cannot write " + path;
            return false;
        }
    }
    wxLogNull quiet;
    if (!wxRenameFile(tmp, fn.GetFullPath(), true)) {
        wxRemoveFile(tmp);
        if (error)
            *error = "cannot replace " + path;
        return false;
    }
    return true;
}

} // namespace

StyleLoadReport StyleRegistry::load(const std::string &path)
{
    std::string text, err;
    if (!read_text_file(path, text, &err)) {
        StyleLoadReport r;
        r.ok    = false;
        r.error = err;
        m_last_report = r;
        return r;
    }
    return parse(text);
}

bool StyleRegistry::save(const std::string &path, std::string *error) const
{
    return write_text_file(path, dump() + "\n", error);
}

bool StyleRegistry::export_theme(const std::string &path, std::string *error) const
{
    return save(path, error);
}

StyleLoadReport StyleRegistry::import_theme(const std::string &path)
{
    // Import merges: the file's user presets are added (same name replaces),
    // its element overrides replace ours, its active preset becomes active.
    std::string text, err;
    if (!read_text_file(path, text, &err)) {
        StyleLoadReport r;
        r.ok    = false;
        r.error = err;
        m_last_report = r;
        return r;
    }
    StyleRegistry incoming;
    StyleLoadReport r = incoming.parse(text);
    if (!r.ok) {
        m_last_report = r;
        return r;
    }
    for (const auto &kv : incoming.m_user_presets)
        m_user_presets[kv.first] = kv.second;
    m_elements      = incoming.m_elements;
    m_active_preset = preset(incoming.m_active_preset) ? incoming.m_active_preset : std::string(kDefaultPreset);
    for (auto it = incoming.m_unknown_top_level.begin(); it != incoming.m_unknown_top_level.end(); ++it)
        m_unknown_top_level[it.key()] = it.value();
    m_last_report = r;
    notify(kEveryElement);
    return r;
}

// ---------------------------------------------------------------------------
// Notification / registration
// ---------------------------------------------------------------------------
int StyleRegistry::subscribe(Listener listener)
{
    const int token   = m_next_token++;
    m_listeners[token] = std::move(listener);
    return token;
}

void StyleRegistry::unsubscribe(int token) { m_listeners.erase(token); }

void StyleRegistry::notify(const std::string &id)
{
    // Copy: a listener may (un)subscribe while being called.
    std::vector<Listener> copy;
    for (const auto &kv : m_listeners)
        copy.push_back(kv.second);
    for (const Listener &l : copy)
        if (l)
            l(id);
}

void StyleRegistry::register_id(const std::string &id, const wxString &display_name)
{
    if (id.empty())
        return;
    wxString &slot = m_known_ids[id];
    if (!display_name.IsEmpty() || slot.IsEmpty())
        slot = display_name.IsEmpty() ? wxString::FromUTF8(id) : display_name;
}

// ---------------------------------------------------------------------------
// ElementStyle facade
// ---------------------------------------------------------------------------
namespace {

std::string &storage_dir_state()
{
    static std::string dir;
    return dir;
}

ElementStyle::AttachHook &attach_hook_state()
{
    static ElementStyle::AttachHook hook;
    return hook;
}

} // namespace

void ElementStyle::set_attach_hook(AttachHook hook) { attach_hook_state() = std::move(hook); }

StyleRegistry &ElementStyle::registry()
{
    static StyleRegistry *reg = new StyleRegistry(); // never destroyed: windows outlive static teardown order
    return *reg;
}

void ElementStyle::set_storage_dir(const std::string &dir)
{
    storage_dir_state() = dir;
    if (dir.empty())
        return;
    const std::string file = storage_file();
    if (wxFileName::FileExists(wxString::FromUTF8(file))) {
        const StyleLoadReport r = registry().load(file);
        if (!r.ok)
            wxLogWarning("Appearance: %s", wxString::FromUTF8(r.error));
    }
}

const std::string &ElementStyle::storage_dir() { return storage_dir_state(); }

std::string ElementStyle::storage_file()
{
    if (storage_dir_state().empty())
        return {};
    return storage_dir_state() + "/element-styles.json";
}

bool ElementStyle::save()
{
    const std::string file = storage_file();
    if (file.empty())
        return false;
    std::string err;
    const bool  ok = registry().save(file, &err);
    if (!ok)
        wxLogWarning("Appearance: %s", wxString::FromUTF8(err));
    return ok;
}

wxFont ElementStyle::font_for(const std::string &id, const wxFont &base)
{
    if (id.empty())
        return base;
    const StyleRegistry &reg = registry();
    StyleBag bag = StyleBag::object();
    static const char *keys[] = {StyleProp::font_family, StyleProp::font_size,   StyleProp::font_weight,
                                 StyleProp::font_style,  StyleProp::underline,   StyleProp::strikethrough};
    for (const char *key : keys) {
        nlohmann::json v = reg.resolve(id, key);
        if (!v.is_null())
            bag[key] = v;
    }
    return style_font_from_bag(bag, base);
}

wxColour ElementStyle::colour_for(const std::string &id, const char *role, const wxColour &base)
{
    if (id.empty() || !role)
        return base;
    return style_colour_from_json(registry().resolve(id, role), base);
}

double ElementStyle::number_for(const std::string &id, const char *key, double base)
{
    if (id.empty() || !key)
        return base;
    nlohmann::json v = registry().resolve(id, key);
    return v.is_number() ? v.get<double>() : base;
}

// ---------------------------------------------------------------------------
// Adopter
// ---------------------------------------------------------------------------
struct ElementStyle::Adopted
{
    std::string id;
    wxFont      base_font;
    wxColour    base_fg;
    wxColour    base_bg;
    int         token { 0 };
};

std::map<wxWindow *, std::shared_ptr<ElementStyle::Adopted>> &ElementStyle::adopted()
{
    static auto *m = new std::map<wxWindow *, std::shared_ptr<Adopted>>();
    return *m;
}

void ElementStyle::re_apply(wxWindow *window, Adopted &a)
{
    if (!window)
        return;
    const wxFont f = font_for(a.id, a.base_font);
    if (f.IsOk() && f != window->GetFont())
        window->SetFont(f);
    const wxColour fg = colour_for(a.id, StyleProp::foreground, a.base_fg);
    const wxColour bg = colour_for(a.id, StyleProp::background, a.base_bg);
    if (fg.IsOk() && fg != window->GetForegroundColour())
        window->SetForegroundColour(fg);
    if (bg.IsOk() && bg != window->GetBackgroundColour())
        window->SetBackgroundColour(bg);
    window->InvalidateBestSize();
    if (wxWindow *parent = window->GetParent())
        parent->Layout();
    window->Refresh();
}

void ElementStyle::apply(wxWindow *window, const std::string &id, const wxString &display_name, bool wire_context_menu)
{
    if (!window || id.empty())
        return;
    auto &map = adopted();
    auto  it  = map.find(window);
    std::shared_ptr<Adopted> a;
    if (it != map.end()) {
        a = it->second;
        // Re-adoption: restore the remembered base before re-applying so a
        // previous style does not become the new base.
        window->SetFont(a->base_font);
        window->SetForegroundColour(a->base_fg);
        window->SetBackgroundColour(a->base_bg);
        a->id = id;
    } else {
        a            = std::make_shared<Adopted>();
        a->id        = id;
        a->base_font = window->GetFont();
        a->base_fg   = window->GetForegroundColour();
        a->base_bg   = window->GetBackgroundColour();
        map[window]  = a;

        std::weak_ptr<Adopted> weak = a;
        a->token = registry().subscribe([window, weak](const std::string &changed) {
            auto self = weak.lock();
            if (!self)
                return;
            if (changed != StyleRegistry::kEveryElement) {
                // Only re-apply when the change touches this id or an ancestor.
                const std::string &mine = self->id;
                if (!(mine == changed ||
                      (mine.size() > changed.size() && mine.compare(0, changed.size(), changed) == 0 &&
                       mine[changed.size()] == '/')))
                    return;
            }
            re_apply(window, *self);
        });
        window->Bind(wxEVT_DESTROY, [window](wxWindowDestroyEvent &e) {
            if (e.GetWindow() == window)
                release(window);
            e.Skip();
        });
        if (wire_context_menu)
            if (const AttachHook &hook = attach_hook_state())
                hook(window, id);
    }
    registry().register_id(id, display_name);
    re_apply(window, *a);
}

void ElementStyle::release(wxWindow *window)
{
    auto &map = adopted();
    auto  it  = map.find(window);
    if (it == map.end())
        return;
    registry().unsubscribe(it->second->token);
    map.erase(it);
}

std::string ElementStyle::element_id_of(const wxWindow *window)
{
    const auto &map = adopted();
    for (const wxWindow *w = window; w; w = w->GetParent()) {
        auto it = map.find(const_cast<wxWindow *>(w));
        if (it != map.end())
            return it->second->id;
    }
    return {};
}

wxString ElementStyle::display_name_of(const std::string &id)
{
    const auto &known = registry().known_ids();
    auto        it    = known.find(id);
    if (it != known.end() && !it->second.IsEmpty())
        return it->second;
    return wxString::FromUTF8(id);
}

void ElementStyle::restyle_all()
{
    // Copy: re_apply may trigger layout that destroys/creates windows.
    std::vector<std::pair<wxWindow *, std::shared_ptr<Adopted>>> copy(adopted().begin(), adopted().end());
    for (auto &kv : copy)
        re_apply(kv.first, *kv.second);
}

void ElementStyle::rebase(wxWindow *window)
{
    auto &map = adopted();
    auto  it  = map.find(window);
    if (it == map.end())
        return;
    it->second->base_font = window->GetFont();
    it->second->base_fg   = window->GetForegroundColour();
    it->second->base_bg   = window->GetBackgroundColour();
    re_apply(window, *it->second);
}

}} // namespace Slic3r::GUI
