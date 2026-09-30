#include "LayoutProbe.hpp"

#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "BBLTopbar.hpp"
#include "GLCanvas3D.hpp"
#include "Plater.hpp"
#include "PartPlate.hpp"
#include "NotificationManager.hpp"
#include "CommandPalette.hpp"
#include "ConfigWizard.hpp"
#include "WebGuideDialog.hpp"
#include "I18N.hpp"
#include "BilingualRegistry.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/ListBox.hpp"
#include "Widgets/MD3DataView.hpp"
#include "Widgets/MD3HtmlWindow.hpp"
#include "Widgets/MD3ScrolledWindow.hpp"
#include "Widgets/TextArea.hpp"
#include "Widgets/TextInput.hpp"
#include <wx/scrolwin.h>
#include <cwchar>
#include <cstdlib>
#include <cstring>
#include <typeinfo>
#include <algorithm>
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/StateColor.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Model.hpp"
#include "libslic3r/Utils.hpp"

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/nowide/fstream.hpp>

#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/control.h>
#include <wx/aui/auibar.h>
#include <wx/glcanvas.h>
#include <wx/radiobut.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textentry.h>
#include <wx/toplevel.h>
#include <wx/utils.h>
#include <wx/window.h>

#include <atomic>
#include <functional>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define PROBE_GETPID _getpid
#else
#include <unistd.h>
#define PROBE_GETPID getpid
#endif

namespace Slic3r { namespace GUI { namespace LayoutProbe {

namespace {

std::atomic<int> g_dump_counter{0};
bool g_installed = false;
bool g_first_dump_done = false;
// Target of a pending "canvas-png" command; the 3D canvas takes it on its next frame.
std::string g_canvas_png;

std::string env_value(const char *name)
{
    wxString v;
    if (!wxGetEnv(wxString::FromUTF8(name), &v)) return std::string();
    return std::string(v.ToUTF8().data());
}

// Minimal JSON string escaping; the payload is plain UTF-8 text.
std::string json(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out.push_back(static_cast<char>(c));
            }
        }
    }
    out.push_back('"');
    return out;
}

std::string json(const wxString &s) { return json(std::string(s.ToUTF8().data())); }

// Declared before write_model_state(), its first user.
std::uintptr_t handle_of(const wxWindow *w)
{
#ifdef _WIN32
    return reinterpret_cast<std::uintptr_t>(w->GetHWND());
#else
    return reinterpret_cast<std::uintptr_t>(w);
#endif
}

// Which scrollbars a window shows and whose they are. Windows keeps
// WS_VSCROLL/WS_HSCROLL in the style only while it shows that bar itself, so
// "native" is a Windows-drawn bar; "kit" is the Material bar (MD3ScrollBars)
// of an MD3ScrolledWindow, a kit ListBox, an MD3 data view table, a
// multi-line TextAreaEditor or an MD3HtmlWindow.
std::string scrollbars_json(const wxWindow *w)
{
    bool native_v = false;
    bool native_h = false;
#ifdef _WIN32
    if (HWND hwnd = static_cast<HWND>(w->GetHWND())) {
        const LONG_PTR style = ::GetWindowLongPtrW(hwnd, GWL_STYLE);
        native_v = (style & WS_VSCROLL) != 0;
        native_h = (style & WS_HSCROLL) != 0;
    }
#endif
    bool kit_v = false;
    bool kit_h = false;
    if (const auto *kit = dynamic_cast<const MD3ScrolledWindow *>(w)) {
        kit_v = kit->IsBarShown(wxVERTICAL);
        kit_h = kit->IsBarShown(wxHORIZONTAL);
    } else if (const auto *list = dynamic_cast<const ListBox *>(w)) {
        kit_v = list->IsBarShown(wxVERTICAL);
        kit_h = list->IsBarShown(wxHORIZONTAL);
    } else if (const auto *table = dynamic_cast<const MD3DataViewCtrl *>(w)) {
        kit_v = table->IsBarShown(wxVERTICAL);
        kit_h = table->IsBarShown(wxHORIZONTAL);
    } else if (const auto *table_list = dynamic_cast<const MD3DataViewListCtrl *>(w)) {
        kit_v = table_list->IsBarShown(wxVERTICAL);
        kit_h = table_list->IsBarShown(wxHORIZONTAL);
    } else if (const auto *editor = dynamic_cast<const TextAreaEditor *>(w)) {
        kit_v = editor->IsBarShown(wxVERTICAL);
        kit_h = editor->IsBarShown(wxHORIZONTAL);
    } else if (const auto *html = dynamic_cast<const MD3HtmlWindow *>(w)) {
        kit_v = html->IsBarShown(wxVERTICAL);
        kit_h = html->IsBarShown(wxHORIZONTAL);
    }
    std::ostringstream o;
    o << "{\"native_v\":" << (native_v ? "true" : "false") << ",\"native_h\":" << (native_h ? "true" : "false")
      << ",\"kit_v\":" << (kit_v ? "true" : "false") << ",\"kit_h\":" << (kit_h ? "true" : "false") << "}";
    return o.str();
}

// Limit user-authored fields without splitting a UTF-8 code point. The probe
// is a local diagnostic file, but it must remain bounded on a large project.
std::string bounded_utf8(const std::string &value, size_t max_bytes)
{
    if (value.size() <= max_bytes) return value;
    size_t end = max_bytes;
    while (end > 0 && (static_cast<unsigned char>(value[end]) & 0xc0) == 0x80)
        --end;
    return value.substr(0, end);
}

bool valid_utf8(const std::string &value)
{
    for (size_t i = 0; i < value.size();) {
        const unsigned char lead = static_cast<unsigned char>(value[i]);
        if (lead < 0x80) { ++i; continue; }
        const size_t length = lead >= 0xc2 && lead <= 0xdf ? 2
            : lead >= 0xe0 && lead <= 0xef ? 3
            : lead >= 0xf0 && lead <= 0xf4 ? 4 : 0;
        if (length == 0 || i + length > value.size()) return false;
        for (size_t j = 1; j < length; ++j)
            if ((static_cast<unsigned char>(value[i + j]) & 0xc0) != 0x80) return false;
        const unsigned char second = static_cast<unsigned char>(value[i + 1]);
        if ((lead == 0xe0 && second < 0xa0) || (lead == 0xed && second >= 0xa0) ||
            (lead == 0xf0 && second < 0x90) || (lead == 0xf4 && second >= 0x90))
            return false;
        i += length;
    }
    return true;
}

void write_model_state(boost::nowide::ofstream &out, Plater *plater)
{
    if (!plater) {
        MainFrame *frame = wxGetApp().mainframe;
        out << "{\"kind\":\"model_state\",\"model_available\":false"
            << ",\"plater_hwnd\":null"
            << ",\"mainframe_hwnd\":" << (frame ? std::to_string(handle_of(frame)) : "null")
            << ",\"object_count\":null,\"object_records\":0,\"objects_truncated\":false"
            << ",\"active_plate_available\":false,\"active_plate_index\":null,\"active_plate_id\":null"
            << ",\"active_plate_instance_count\":null,\"active_plate_printable_instance_count\":null"
            << ",\"project_path_available\":false,\"project_path\":null,\"project_path_truncated\":false"
            << ",\"project_name_available\":false,\"project_name\":null,\"project_name_truncated\":false}"
            << "\n";
        return;
    }
    constexpr size_t max_object_records = 64;
    constexpr size_t max_name_bytes = 160;
    constexpr size_t max_path_bytes = 1024;

    const Model &model = plater->model();
    PartPlateList &plates = plater->get_partplate_list();
    const int plate_count = plates.get_plate_count();
    const int active_index = plates.get_curr_plate_index();
    PartPlate *active_plate = active_index >= 0 && active_index < plate_count
        ? plates.get_curr_plate() : nullptr;
    size_t active_instances = 0;
    size_t printable_instances = 0;
    if (active_plate) {
        const auto &outside = active_plate->get_obj_and_inst_outside_set();
        for (const auto &entry : active_plate->get_obj_and_inst_set()) {
            const size_t object_index = static_cast<size_t>(entry.first);
            const size_t instance_index = static_cast<size_t>(entry.second);
            if (entry.first < 0 || entry.second < 0 || object_index >= model.objects.size()) continue;
            const ModelObject *object = model.objects[object_index];
            if (!object || instance_index >= object->instances.size()) continue;
            const ModelInstance *instance = object->instances[instance_index];
            if (!instance) continue;
            ++active_instances;
            if (instance->is_printable() && outside.find(entry) == outside.end())
                ++printable_instances;
        }
    }

    const auto path_utf8 = plater->get_project_filename(".3mf").ToUTF8();
    const auto name_utf8 = plater->get_project_name().ToUTF8();
    const std::string full_path = path_utf8.data() ? path_utf8.data() : "";
    const std::string name = name_utf8.data() ? name_utf8.data() : "";
    const size_t emitted = std::min(model.objects.size(), max_object_records);
    MainFrame *frame = wxGetApp().mainframe;
    out << "{\"kind\":\"model_state\",\"model_available\":true"
        << ",\"plater_hwnd\":" << handle_of(plater)
        << ",\"mainframe_hwnd\":" << (frame ? std::to_string(handle_of(frame)) : "null")
        << ",\"object_count\":" << model.objects.size()
        << ",\"object_records\":" << emitted
        << ",\"objects_truncated\":" << (model.objects.size() > emitted ? "true" : "false")
        << ",\"active_plate_available\":" << (active_plate ? "true" : "false")
        << ",\"active_plate_index\":" << (active_plate ? std::to_string(active_index) : "null")
        << ",\"active_plate_id\":" << (active_plate ? std::to_string(active_plate->get_index()) : "null")
        << ",\"active_plate_instance_count\":" << (active_plate ? std::to_string(active_instances) : "null")
        << ",\"active_plate_printable_instance_count\":" << (active_plate ? std::to_string(printable_instances) : "null")
        << ",\"project_path_available\":" << (!full_path.empty() ? "true" : "false")
        << ",\"project_path\":" << (full_path.empty() ? "null" : json(bounded_utf8(full_path, max_path_bytes)))
        << ",\"project_path_truncated\":" << (full_path.size() > max_path_bytes ? "true" : "false")
        << ",\"project_name_available\":" << (!name.empty() ? "true" : "false")
        << ",\"project_name\":" << (name.empty() ? "null" : json(bounded_utf8(name, max_name_bytes)))
        << ",\"project_name_truncated\":" << (name.size() > max_name_bytes ? "true" : "false")
        << "}\n";
    for (size_t index = 0; index < emitted; ++index) {
        const ModelObject *object = model.objects[index];
        const std::string object_name = object ? object->name : std::string();
        const bool name_valid_utf8 = object && valid_utf8(object_name);
        out << "{\"kind\":\"model_object\",\"index\":" << index
            << ",\"object_available\":" << (object ? "true" : "false")
            << ",\"name_available\":" << (name_valid_utf8 ? "true" : "false")
            << ",\"name_valid_utf8\":" << (object ? (name_valid_utf8 ? "true" : "false") : "null")
            << ",\"name\":" << (name_valid_utf8 ? json(bounded_utf8(object_name, max_name_bytes)) : "null")
            << ",\"name_truncated\":" << (name_valid_utf8 && object_name.size() > max_name_bytes ? "true" : "false")
            << ",\"instance_count\":" << (object ? std::to_string(object->instances.size()) : "null")
            << "}\n";
    }
}

std::string rect_json(const wxRect &r)
{
    std::ostringstream o;
    o << "{\"x\":" << r.x << ",\"y\":" << r.y << ",\"w\":" << r.width << ",\"h\":" << r.height << "}";
    return o.str();
}

std::string size_json(const wxSize &s)
{
    std::ostringstream o;
    o << "{\"w\":" << s.x << ",\"h\":" << s.y << "}";
    return o.str();
}

// Sum of the minimum sizes a wxBoxSizer must pay along its orientation, with
// borders, against what it actually has. This is the starvation detector.
// wxSizerItem::CalcMin() already returns the minimum with the item's borders
// (GetMinSizeWithBorder), so they are not added again: counted twice, the
// bilingual Preferences button row read 799 px required in a row its three
// buttons fill to the pixel (783 of 783).
struct RowVerdict {
    bool is_box = false;
    int  orient = 0;
    int  available = 0;
    int  required = 0;
    bool oversubscribed = false;
};

RowVerdict judge_sizer(wxSizer *sizer)
{
    RowVerdict v;
    auto *box = dynamic_cast<wxBoxSizer *>(sizer);
    if (!box) return v;
    v.is_box = true;
    v.orient = box->GetOrientation();
    const wxSize size = box->GetSize();
    v.available = v.orient == wxHORIZONTAL ? size.x : size.y;
    for (wxSizerItem *item : box->GetChildren()) {
        if (!item->IsShown()) continue;
        const wxSize min = item->CalcMin();
        v.required += v.orient == wxHORIZONTAL ? min.x : min.y;
    }
    v.oversubscribed = v.available > 0 && v.required > v.available;
    return v;
}

// Find the sizer item that owns `w` inside its parent's sizer tree, if any.
wxSizerItem *item_for(wxSizer *sizer, wxWindow *w, wxSizer **owner)
{
    if (!sizer) return nullptr;
    for (wxSizerItem *item : sizer->GetChildren()) {
        if (item->IsWindow() && item->GetWindow() == w) { *owner = sizer; return item; }
        if (item->IsSizer()) {
            if (wxSizerItem *found = item_for(item->GetSizer(), w, owner)) return found;
        }
    }
    return nullptr;
}

// The C++ type of a window. Most kit widgets carry no wx class info of their
// own, so "class" reads "wxWindow" for every one of them; this names them.
wxString type_name_of(wxWindow *w)
{
    std::string name = typeid(*w).name();
    for (const char *prefix : {"class ", "struct "})
        if (name.rfind(prefix, 0) == 0) {
            name.erase(0, std::strlen(prefix));
            break;
        }
    return wxString::FromUTF8(name.c_str());
}

void write_window(boost::nowide::ofstream &out, wxWindow *w, wxWindow *top, int depth)
{
    wxWindow *parent = w->GetParent();
    const wxRect rect = w->GetRect();
    const wxRect screen = w->GetScreenRect();
    const wxSize client = w->GetClientSize();
    const bool shown = w->IsShown();
    // Only what a user can see can be clipped: a window whose own flag says shown
    // still sits unseen inside a hidden parent (Version history's failure banner).
    const bool visible = w->IsShownOnScreen();

    // wxWindow carries the label, not only wxControl: the kit Button and
    // every other StaticBox-based control descend from wxWindow directly and
    // do set their text, so reading it only through wxControl dumped every
    // kit button with an empty label and the recapture driver found no tab.
    std::string label = std::string(w->GetLabel().ToUTF8().data());
    bool        has_label = !label.empty();

    // text_clipped: the label is cut or shortened although nothing asked for it.
    // truncated: the label is drawn shortened with an ellipsis, asked for or not
    // (an ellipsizing static text, a kit Button that shrank). A shortened action
    // in a dialog is a defect either way, so a sweep reads both.
    bool text_clipped = false;
    bool truncated = false;
    bool ellipsized = false;
    int text_width = -1;
    if (auto *st = dynamic_cast<wxStaticText *>(w)) {
        const long style = st->GetWindowStyle();
        ellipsized = (style & (wxST_ELLIPSIZE_START | wxST_ELLIPSIZE_MIDDLE | wxST_ELLIPSIZE_END)) != 0;
        // Measure what is drawn: "&&" shows as one "&" and a mnemonic "&" not
        // at all, so the raw label reads wider than the text on screen.
        const wxString text = st->GetLabelText();
        if (!text.empty() && text.Find('\n') == wxNOT_FOUND) {
            text_width = st->GetTextExtent(text).x;
            text_clipped = visible && !ellipsized && text_width > client.x;
            truncated = visible && ellipsized && text_width > client.x;
        }
    } else if (has_label && dynamic_cast<wxControl *>(w)) {
        const wxString text = wxControl::RemoveMnemonics(wxString::FromUTF8(label.c_str()));
        if (text.Find('\n') == wxNOT_FOUND) {
            text_width = w->GetTextExtent(text).x;
            // Custom controls draw icons and padding too; report the extent and
            // let the reader judge, flagging only the unambiguous case.
            text_clipped = visible && text_width > client.x;
        }
    } else if (auto *btn = dynamic_cast<::Button *>(w)) {
        // The kit Button descends from wxWindow, not wxControl, and shortens its
        // own label while painting; it records when the last paint had to.
        // Shrinking is by design only where it is allowed (notebook tabs).
        text_width = btn->GetTextRect().width;
        ellipsized = btn->AllowsShrink();
        truncated = visible && btn->LabelTruncated();
        text_clipped = truncated && !ellipsized;
    }

    // A placeholder hint is drawn by the single-line edit control inside its own
    // client area, on one line, and only while the entry is empty: a hint wider
    // than that is cut. What's new showed "YYYY-MM-DD / D" for its date hint and
    // no sweep reported it, because nothing measured hints (clipping inventory
    // CJ-027). The edit control keeps a small margin on each side.
    std::string hint;
    int hint_width = -1;
    bool hint_clipped = false;
    if (auto *entry = dynamic_cast<wxTextEntry *>(w)) {
        const auto *multi = dynamic_cast<wxTextCtrl *>(w);
        const wxString shown_hint = entry->GetHint();
        if (!shown_hint.empty() && (multi == nullptr || !multi->IsMultiLine())) {
            hint = std::string(shown_hint.ToUTF8().data());
            hint_width = w->GetTextExtent(shown_hint).x;
            hint_clipped = visible && entry->IsEmpty() && hint_width + w->FromDIP(4) > client.x;
        }
    }

    bool clipped_by_parent = false;
    // A dialog or other top-level window is its own native window and its rect
    // is in screen coordinates, so nothing of its parent can cut it.
    if (parent && !w->IsTopLevel()) {
        const wxSize parent_client = parent->GetClientSize();
        const wxSize parent_virtual = parent->GetVirtualSize();
        // A scrolling parent shows part of its content at a time: a child outside the
        // visible part along the axis it scrolls is scrolled away, not clipped
        // (the Keyboard Shortcuts rows below the fold).
        const bool scrolls_x = parent_virtual.x > parent_client.x;
        const bool scrolls_y = parent_virtual.y > parent_client.y;
        const bool out_x = rect.x < 0 || rect.x + rect.width > parent_client.x;
        const bool out_y = rect.y < 0 || rect.y + rect.height > parent_client.y;
        clipped_by_parent = visible && rect.width > 0 && rect.height > 0 && ((out_x && !scrolls_x) || (out_y && !scrolls_y));
    }

    // Sizer view of this window: allocation versus minimum, and the row verdict.
    wxSizer *owner = nullptr;
    wxSizerItem *item = parent ? item_for(parent->GetSizer(), w, &owner) : nullptr;
    std::string sizer_json = "null";
    bool starved = false;
    bool zero_sized = visible && (rect.width == 0 || rect.height == 0);
    if (item) {
        const wxSize min = item->CalcMin();
        const wxSize alloc = item->GetSize();
        RowVerdict row = judge_sizer(owner);
        if (row.is_box) {
            const int have = row.orient == wxHORIZONTAL ? alloc.x : alloc.y;
            const int need = row.orient == wxHORIZONTAL ? min.x : min.y;
            starved = visible && need > 0 && have < need;
        }
        std::ostringstream o;
        o << "{\"proportion\":" << item->GetProportion()
          << ",\"flag\":" << item->GetFlag()
          << ",\"border\":" << item->GetBorder()
          << ",\"min\":" << size_json(min)
          << ",\"alloc\":" << size_json(alloc)
          << ",\"row\":{\"box\":" << (row.is_box ? "true" : "false")
          << ",\"orient\":" << (row.orient == wxHORIZONTAL ? "\"h\"" : "\"v\"")
          << ",\"available\":" << row.available
          << ",\"required\":" << row.required
          << ",\"oversubscribed\":" << (row.oversubscribed ? "true" : "false") << "}}";
        sizer_json = o.str();
    }

    out << "{\"kind\":\"window\""
        << ",\"hwnd\":" << handle_of(w)
        << ",\"parent\":" << (parent ? handle_of(parent) : 0)
        << ",\"top\":" << handle_of(top)
        << ",\"depth\":" << depth
        << ",\"class\":" << json(wxString(w->GetClassInfo()->GetClassName()))
        << ",\"type\":" << json(type_name_of(w))
        << ",\"name\":" << json(w->GetName())
        << ",\"label\":" << json(label)
        << ",\"shown\":" << (shown ? "true" : "false")
        << ",\"on_screen\":" << (w->IsShownOnScreen() ? "true" : "false")
        << ",\"enabled\":" << (w->IsEnabled() ? "true" : "false")
        << ",\"rect\":" << rect_json(rect)
        << ",\"screen\":" << rect_json(screen)
        << ",\"client\":" << size_json(client)
        << ",\"scrollbars\":" << scrollbars_json(w)
        << ",\"min\":" << size_json(w->GetMinSize())
        << ",\"best\":" << size_json(w->GetBestSize())
        << ",\"text_width\":" << text_width
        << ",\"ellipsized\":" << (ellipsized ? "true" : "false")
        << ",\"text_clipped\":" << (text_clipped ? "true" : "false")
        << ",\"truncated\":" << (truncated ? "true" : "false")
        << ",\"hint\":" << json(hint)
        << ",\"hint_width\":" << hint_width
        << ",\"hint_clipped\":" << (hint_clipped ? "true" : "false")
        << ",\"clipped_by_parent\":" << (clipped_by_parent ? "true" : "false")
        << ",\"starved\":" << (starved ? "true" : "false")
        << ",\"zero_sized\":" << (zero_sized ? "true" : "false")
        << ",\"sizer\":" << sizer_json
        << "}\n";

    // wxAuiToolBar tools are not windows, so the caption bar's brand tile, menu
    // tools, palette and window controls would otherwise be unaddressable;
    // emit one record per tool with its label (or help text) and rectangle.
    if (auto *bar = dynamic_cast<wxAuiToolBar *>(w)) {
        const wxPoint bar_origin = bar->GetScreenPosition();
        for (size_t i = 0; i < bar->GetToolCount(); ++i) {
            wxAuiToolBarItem *item = bar->FindToolByIndex(int(i));
            if (!item || item->GetKind() == wxITEM_SEPARATOR) continue; // spacers carry no label and drop out below
            wxString name = item->GetLabel();
            if (name.empty()) name = item->GetShortHelp();
            if (name.empty()) continue;
            const wxRect r = bar->GetToolRect(item->GetId());
            out << "{\"kind\":\"tool\",\"host\":" << handle_of(w)
                << ",\"top\":" << handle_of(top)
                << ",\"id\":" << item->GetId()
                << ",\"label\":" << json(name)
                << ",\"shown\":true,\"on_screen\":" << (bar->IsShownOnScreen() ? "true" : "false")
                << ",\"rect\":" << rect_json(r)
                << ",\"screen\":" << rect_json(wxRect(bar_origin.x + r.x, bar_origin.y + r.y, r.width, r.height))
                << "}\n";
        }
    }
    for (wxWindow *child : w->GetChildren())
        write_window(out, child, top, depth + 1);
}

std::string default_path()
{
    std::string base = env_value("BAMBU_LAYOUT_PROBE");
    boost::filesystem::path dir;
    if (base == "1" || base.empty())
        dir = boost::filesystem::path(data_dir()) / "log";
    else
        dir = boost::filesystem::path(base);
    boost::system::error_code ec;
    boost::filesystem::create_directories(dir, ec);
    std::ostringstream name;
    name << "layout-probe-" << PROBE_GETPID() << "-" << g_dump_counter.load() << ".jsonl";
    return (dir / name.str()).string();
}

// ---------------------------------------------------------------------------
// language-audit: in bilingual mode, which shown native controls still show
// English only even though BilingualRegistry holds Cantonese for them.
//
// This reads the label and tooltip already on screen instead of recomputing
// I18N::enable_bilingual_decorator()'s own decision (BilingualDecorator.cpp,
// decorate_window()): that watcher ticks every 250 ms and only sweeps every
// shown window fully once every twelve ticks (about 3 seconds), so a driver
// should wait at least 4 seconds after a surface first shows before sending
// this command, or the answer describes a window that has not been decorated
// yet.

enum class AuditKind { Skip, Text, Button, Check, Radio, GroupBox };

AuditKind audit_kind_of(wxWindow *w)
{
    // Typed and chosen values are the user's own data, never catalogue text;
    // the kit ComboBox (Widgets/ComboBox.hpp) is itself a TextInput, so this
    // one check clears native and kit text entry, and every combo box, alike.
    // List controls and everything else fall through to the default Skip.
    if (dynamic_cast<wxTextEntry *>(w) != nullptr || dynamic_cast<::TextInput *>(w) != nullptr)
        return AuditKind::Skip;
    if (dynamic_cast<::Button *>(w) != nullptr || dynamic_cast<wxButton *>(w) != nullptr)
        return AuditKind::Button;
    if (dynamic_cast<wxCheckBox *>(w) != nullptr)
        return AuditKind::Check;
    if (dynamic_cast<wxRadioButton *>(w) != nullptr)
        return AuditKind::Radio;
    if (dynamic_cast<wxStaticBox *>(w) != nullptr)
        return AuditKind::GroupBox;
    if (dynamic_cast<wxStaticText *>(w) != nullptr) // also matches the kit Label
        return AuditKind::Text;
    // The section header is custom-drawn, but a label all the same.
    if (dynamic_cast<::SectionHeader *>(w) != nullptr)
        return AuditKind::Text;
    return AuditKind::Skip;
}

// The English half of a label the decorator may already have rewritten:
// before the compact inline separator ("English (middle dot) Cantonese"), or
// before the first newline of a stacked label. A label with neither is
// English-only already and is its own English part.
wxString audit_english_part(const wxString &label)
{
    static const wxString inline_sep = wxString::FromUTF8(" \xC2\xB7 ");
    const int dot = label.Find(inline_sep);
    if (dot != wxNOT_FOUND) return label.Left(dot);
    const int newline = label.Find('\n');
    if (newline != wxNOT_FOUND) return label.Left(newline);
    return label;
}

enum class AuditClass { Bilingual, Tooltip, EnglishOnly, NoTranslation };

AuditClass classify_audit_label(const wxString &label, const wxString &tooltip)
{
    const wxString english = audit_english_part(label);
    const wxString cantonese = I18N::BilingualRegistry::instance().lookup(english);
    if (cantonese.empty()) return AuditClass::NoTranslation; // not a catalogue string
    if (label.Contains(cantonese)) return AuditClass::Bilingual;
    if (!tooltip.empty() && tooltip.Contains(cantonese)) return AuditClass::Tooltip;
    return AuditClass::EnglishOnly;
}

struct AuditCounts {
    int bilingual = 0;
    int tooltip = 0;
    int english_only = 0;
    int no_translation = 0;
};

struct AuditDefect {
    std::string    top_class;
    std::string    control_class;
    std::uintptr_t handle = 0;
    std::string    label;
    bool           has_tooltip = false;
};

std::string audit_class_name(const wxWindow *w)
{
    return std::string(wxString(w->GetClassInfo()->GetClassName()).ToUTF8().data());
}

void walk_for_audit(wxWindow *w, wxWindow *top, AuditCounts &counts, std::vector<AuditDefect> &defects)
{
    constexpr size_t max_audit_label_bytes = 200;
    const AuditKind kind = audit_kind_of(w);
    if (kind != AuditKind::Skip) {
        const wxString label = w->GetLabel();
        if (!label.empty()) {
            const wxString tooltip = w->GetToolTipText();
            switch (classify_audit_label(label, tooltip)) {
            case AuditClass::Bilingual: ++counts.bilingual; break;
            case AuditClass::Tooltip: ++counts.tooltip; break;
            case AuditClass::NoTranslation: ++counts.no_translation; break;
            case AuditClass::EnglishOnly: {
                ++counts.english_only;
                AuditDefect defect;
                defect.top_class     = audit_class_name(top);
                defect.control_class = audit_class_name(w);
                defect.handle        = handle_of(w);
                defect.label         = bounded_utf8(std::string(label.ToUTF8().data()), max_audit_label_bytes);
                defect.has_tooltip   = !tooltip.empty();
                defects.push_back(std::move(defect));
                break;
            }
            }
        }
    }
    for (wxWindow *child : w->GetChildren())
        if (child != nullptr && child->IsShown())
            walk_for_audit(child, top, counts, defects);
}

// Writes language-audit.json beside the dumps. Returns false only when the
// file could not be opened for writing.
bool run_language_audit()
{
    const boost::filesystem::path out_path =
        boost::filesystem::path(default_path()).parent_path() / "language-audit.json";

    const I18N::LanguageModeProfile &profile = I18N::language_mode_profile();
    const char *mode = "english";
    switch (profile.kind) {
    case I18N::LanguageModeKind::CantoneseHongKong: mode = "cantonese"; break;
    case I18N::LanguageModeKind::BilingualEnglishCantoneseHongKong: mode = "bilingual"; break;
    default: break;
    }

    if (!profile.is_bilingual()) {
        boost::nowide::ofstream out(out_path.string());
        if (!out) {
            BOOST_LOG_TRIVIAL(error) << "LayoutProbe: language-audit cannot open " << out_path.string();
            return false;
        }
        out << "{\"mode\":" << json(std::string(mode)) << ",\"skipped\":\"not bilingual\"}\n";
        return true;
    }

    AuditCounts             totals;
    std::vector<AuditDefect> defects;
    std::ostringstream      windows_json;
    bool                    first_window = true;
    for (wxWindow *top : wxTopLevelWindows) {
        if (!top->IsShown()) continue;
        AuditCounts window_counts;
        walk_for_audit(top, top, window_counts, defects);
        totals.bilingual      += window_counts.bilingual;
        totals.tooltip        += window_counts.tooltip;
        totals.english_only   += window_counts.english_only;
        totals.no_translation += window_counts.no_translation;
        if (!first_window) windows_json << ",";
        first_window = false;
        windows_json << "{\"class\":" << json(wxString(top->GetClassInfo()->GetClassName()))
                     << ",\"title\":" << json(top->GetLabel())
                     << ",\"bilingual\":" << window_counts.bilingual
                     << ",\"tooltip\":" << window_counts.tooltip
                     << ",\"english_only\":" << window_counts.english_only
                     << ",\"no_translation\":" << window_counts.no_translation
                     << "}";
    }

    std::ostringstream defects_json;
    bool                first_defect = true;
    for (const AuditDefect &d : defects) {
        if (!first_defect) defects_json << ",";
        first_defect = false;
        defects_json << "{\"top_class\":" << json(d.top_class)
                     << ",\"control_class\":" << json(d.control_class)
                     << ",\"handle\":" << d.handle
                     << ",\"label\":" << json(d.label)
                     << ",\"has_tooltip\":" << (d.has_tooltip ? "true" : "false")
                     << "}";
    }

    boost::nowide::ofstream out(out_path.string());
    if (!out) {
        BOOST_LOG_TRIVIAL(error) << "LayoutProbe: language-audit cannot open " << out_path.string();
        return false;
    }
    out << "{\"mode\":" << json(std::string(mode))
        << ",\"registry_size\":" << I18N::BilingualRegistry::instance().size()
        << ",\"totals\":{\"bilingual\":" << totals.bilingual
        << ",\"tooltip\":" << totals.tooltip
        << ",\"english_only\":" << totals.english_only
        << ",\"no_translation\":" << totals.no_translation << "}"
        << ",\"windows\":[" << windows_json.str() << "]"
        << ",\"english_only\":[" << defects_json.str() << "]"
        << "}\n";
    BOOST_LOG_TRIVIAL(info) << "LayoutProbe: language-audit wrote " << out_path.string()
                            << ", " << totals.english_only << " english_only of "
                            << (totals.bilingual + totals.tooltip + totals.english_only + totals.no_translation)
                            << " controls";
    return true;
}

} // namespace

bool enabled()
{
    static const bool on = !env_value("BAMBU_LAYOUT_PROBE").empty();
    return on;
}

std::string artifact_path(const std::string &file_name)
{
    // default_path() creates the folder; only its directory is used here.
    return (boost::filesystem::path(default_path()).parent_path() / file_name).string();
}

bool canvas_png_requested()
{
    return !g_canvas_png.empty();
}

std::string take_canvas_png_request()
{
    std::string path;
    path.swap(g_canvas_png);
    return path;
}

std::string dump(const std::string &reason, const std::string &out_path)
{
    if (!enabled()) return std::string();
    ++g_dump_counter;
    const std::string path = out_path.empty() ? default_path() : out_path;
    boost::nowide::ofstream out(path.c_str(), std::ios::out | std::ios::trunc);
    if (!out) {
        BOOST_LOG_TRIVIAL(error) << "LayoutProbe: cannot open " << path;
        return std::string();
    }

    double dpi_scale = 1.0;
    wxWindow *first_top = nullptr;
    for (wxWindow *top : wxTopLevelWindows) { first_top = top; break; }
    if (first_top) dpi_scale = first_top->GetDPIScaleFactor();

    std::string language, density = "unknown";
    if (wxGetApp().app_config) language = wxGetApp().app_config->get("language");
    switch (MD3::Metrics::density()) {
    case MD3::Metrics::Density::Comfortable: density = "comfortable"; break;
    case MD3::Metrics::Density::Compact: density = "compact"; break;
    default: break;
    }

    out << "{\"kind\":\"header\",\"reason\":" << json(reason)
        << ",\"tag\":" << json(env_value("BAMBU_LAYOUT_PROBE_TAG"))
        << ",\"pid\":" << PROBE_GETPID()
        << ",\"dpi_scale\":" << dpi_scale
        << ",\"language\":" << json(language)
        << ",\"dark\":" << (StateColor::isDarkMode() ? "true" : "false")
        << ",\"density\":" << json(density)
        << ",\"top_levels\":" << wxTopLevelWindows.size()
        << "}\n";

    for (wxWindow *top : wxTopLevelWindows) {
        out << "{\"kind\":\"toplevel\",\"hwnd\":" << handle_of(top)
            << ",\"class\":" << json(wxString(top->GetClassInfo()->GetClassName()))
            << ",\"title\":" << json(top->GetLabel())
            << ",\"shown\":" << (top->IsShown() ? "true" : "false")
            << ",\"rect\":" << rect_json(top->GetScreenRect())
            << ",\"client\":" << size_json(top->GetClientSize())
            << "}\n";
        write_window(out, top, top, 0);
    }
    // The scene toolbar and gizmo rail are ImGui / GL, not wx windows: emit
    // their items from the canvas so a capture can be cropped to them too.
    Plater *plater = wxGetApp().plater();
    if (plater) {
        if (GLCanvas3D *canvas = plater->get_view3D_canvas3D()) {
            wxWindow     *host   = canvas->get_wxglcanvas();
            const wxPoint origin = host ? host->GetScreenPosition() : wxPoint(0, 0);
            for (const auto &it : canvas->get_toolbar_item_rects()) {
                out << "{\"kind\":\"gl_item\",\"toolbar\":" << json(it.toolbar)
                    << ",\"name\":" << json(it.name)
                    << ",\"host\":" << (host ? handle_of(host) : 0)
                    << ",\"rect\":" << rect_json(wxRect(it.x, it.y, it.w, it.h))
                    << ",\"screen\":" << rect_json(wxRect(origin.x + it.x, origin.y + it.y, it.w, it.h))
                    << "}\n";
            }
        }
    }
    // Geometry is not a wx child label. Report actual model state only in
    // this off-by-default, locally owned probe file.
    write_model_state(out, plater);
    // Readers poll the file while it streams; the end record is the only
    // reliable completion signal (a partial file of whole lines still parses).
    out << "{\"kind\":\"end\"}\n";
    out.flush();
    BOOST_LOG_TRIVIAL(info) << "LayoutProbe: wrote " << path << " (" << reason << ")";
    return path;
}

void install(wxWindow *frame)
{
    if (!enabled() || g_installed || !frame) return;
    g_installed = true;
    // One-shot: the first idle after the frame is actually on screen is the
    // first laid-out state a user sees, which is the one to measure.
    wxTheApp->Bind(wxEVT_IDLE, [frame](wxIdleEvent &evt) {
        evt.Skip();
        if (g_first_dump_done || !frame->IsShownOnScreen()) return;
        g_first_dump_done = true;
        dump("first-show");
    });
}

bool handle_command(const std::wstring &payload)
{
    // Driver hooks beyond the dump itself, still gated on the probe being armed:
    //   menu-popup <Title>   pop a top-bar menu (File, Edit, View, Objects, Calibration, Help)
    //   invoke <label>       fire the first menu item whose label contains <label>
    if (enabled()) {
        const std::wstring popup = L"menu-popup ", invoke = L"invoke ";
        MainFrame *frame = wxGetApp().mainframe;
        BBLTopbar *bar   = frame ? frame->topbar() : nullptr;
        if (bar && payload.compare(0, popup.size(), popup) == 0) {
            const bool ok = bar->PopupMenuByTitle(wxString(payload.substr(popup.size())));
            BOOST_LOG_TRIVIAL(info) << "LayoutProbe: menu-popup " << (ok ? "ok" : "no such menu");
            return ok;
        }
        //   load <path>          load a model file into the plater
        //   notify <text>        push a plain notification (toast)
        //   scroll-end <hwnd>    scroll a wxScrolledWindow to its end
        const std::wstring load = L"load ", notify = L"notify ", scroll = L"scroll-end ";
        if (frame && payload.compare(0, load.size(), load) == 0) {
            const wxString path(payload.substr(load.size()));
            frame->CallAfter([path]() {
                wxArrayString files; files.Add(path);
                if (Plater *plater = wxGetApp().plater()) plater->load_files(files);
            });
            return true;
        }
        if (frame && payload.compare(0, notify.size(), notify) == 0) {
            const std::string text = boost::nowide::narrow(payload.substr(notify.size()));
            frame->CallAfter([text]() {
                if (Plater *plater = wxGetApp().plater()) plater->get_notification_manager()->push_notification(text);
            });
            return true;
        }
        //   close <hwnd>         close a dialog (EndModal when modal) or window
        //   resize <hwnd> <w> <h> resize a window
        //   palette              open the command palette
        //   wizard-page <n>      jump the open configuration wizard to index page n
        const std::wstring close = L"close ", resize = L"resize ", palette = L"palette", wizard = L"wizard-page ";
        auto find_by_handle_any = [](unsigned long long h) -> wxWindow * {
            std::function<wxWindow *(wxWindow *)> rec = [&](wxWindow *cur) -> wxWindow * {
                if (handle_of(cur) == static_cast<std::uintptr_t>(h)) return cur;
                for (wxWindow *child : cur->GetChildren())
                    if (wxWindow *hit = rec(child)) return hit;
                return nullptr;
            };
            for (wxWindow *top : wxTopLevelWindows)
                if (wxWindow *hit = rec(top)) return hit;
            return nullptr;
        };
        if (payload.compare(0, close.size(), close) == 0) {
            wxWindow *w = find_by_handle_any(std::wcstoull(payload.substr(close.size()).c_str(), nullptr, 0));
            if (!w) return false;
            w->CallAfter([w]() {
                if (auto *dlg = dynamic_cast<wxDialog *>(w)) {
                    if (dlg->IsModal()) dlg->EndModal(wxID_CANCEL); else dlg->Close(true);
                } else if (auto *tlw = dynamic_cast<wxTopLevelWindow *>(w)) {
                    tlw->Close(true);
                } else {
                    w->Hide();
                }
            });
            return true;
        }
        if (payload.compare(0, resize.size(), resize) == 0) {
            std::wistringstream in(payload.substr(resize.size()));
            unsigned long long h = 0; int cw = 0, ch = 0;
            in >> h >> cw >> ch;
            wxWindow *w = find_by_handle_any(h);
            if (!w || cw <= 0 || ch <= 0) return false;
            w->CallAfter([w, cw, ch]() { w->SetSize(cw, ch); w->Layout(); });
            return true;
        }
        //   notification-center  toggle the top-bar notification centre popover
        //   delete-all           run Plater::delete_all_objects_from_model (raises the gate)
        if (frame && payload == L"notification-center") {
            frame->CallAfter([frame]() {
                if (BBLTopbar *tb = frame->topbar()) { wxAuiToolBarEvent ev; tb->OnNotificationBell(ev); }
            });
            return true;
        }
        if (frame && payload == L"delete-all") {
            frame->CallAfter([]() { if (Plater *plater = wxGetApp().plater()) plater->delete_all_objects_from_model(); });
            return true;
        }
        if (frame && payload == palette) {
            frame->CallAfter([frame]() { CommandPalette::ShowPalette(frame); });
            return true;
        }
        //   config-wizard        run the native configuration wizard (no menu item opens it;
        //                        the Help entry named Setup Wizard is the web guide)
        if (frame && payload == L"config-wizard") {
            frame->CallAfter([]() { wxGetApp().run_wizard(ConfigWizard::RR_USER); });
            return true;
        }
        //   guide-page <n>       load step n of the open web Setup Wizard (the guide URL target)
        const std::wstring guide = L"guide-page ";
        if (payload.compare(0, guide.size(), guide) == 0) {
            const int target = static_cast<int>(std::wcstol(payload.substr(guide.size()).c_str(), nullptr, 10));
            for (wxWindow *top : wxTopLevelWindows)
                if (auto *g = dynamic_cast<GuideFrame *>(top)) {
                    g->CallAfter([g, target]() { g->LoadTarget(target); });
                    return true;
                }
            return false;
        }
        if (payload.compare(0, wizard.size(), wizard) == 0) {
            const size_t index = static_cast<size_t>(std::wcstoull(payload.substr(wizard.size()).c_str(), nullptr, 10));
            for (wxWindow *top : wxTopLevelWindows)
                if (auto *wz = dynamic_cast<ConfigWizard *>(top)) {
                    wz->CallAfter([wz, index]() { wz->go_to_page(index); });
                    return true;
                }
            return false;
        }
        if (payload.compare(0, scroll.size(), scroll) == 0) {
            const unsigned long long h = std::wcstoull(payload.substr(scroll.size()).c_str(), nullptr, 0);
            // Portable handle lookup: walk every top-level window's tree.
            std::function<wxWindow *(wxWindow *)> find_by_handle = [&](wxWindow *cur) -> wxWindow * {
                if (handle_of(cur) == static_cast<std::uintptr_t>(h)) return cur;
                for (wxWindow *child : cur->GetChildren())
                    if (wxWindow *hit = find_by_handle(child)) return hit;
                return nullptr;
            };
            wxWindow *w = nullptr;
            for (wxWindow *top : wxTopLevelWindows)
                if ((w = find_by_handle(top)) != nullptr) break;
            if (auto *sw = dynamic_cast<wxScrolledWindow *>(w)) {
                int x = 0, y = 0; sw->GetVirtualSize(&x, &y);
                int ux = 0, uy = 0; sw->GetScrollPixelsPerUnit(&ux, &uy);
                sw->Scroll(-1, uy > 0 ? y / uy : y);
                return true;
            }
            return false;
        }
        //   sidebar-check        assert the sidebar body is laid out over its virtual
        //                        height: the last stacked child ends at the virtual
        //                        bottom and the virtual height covers the content min
        //                        height. Logs the numbers; returns false on a defect.
        if (frame && payload == L"sidebar-check") {
            auto *sw = dynamic_cast<wxScrolledWindow *>(wxGetApp().sidebar().scrolled_panel());
            if (!sw || !sw->GetSizer()) return false;
            int vx = 0, vy = 0; sw->GetVirtualSize(&vx, &vy);
            const int client_h  = sw->GetClientSize().GetHeight();
            const int content_h = sw->GetSizer()->GetMinSize().GetHeight();
            int last_bottom = 0;
            for (wxSizerItem *item : sw->GetSizer()->GetChildren())
                if (item->IsShown()) last_bottom = std::max(last_bottom, item->GetRect().GetBottom() + 1);
            const bool ok = vy >= content_h && std::abs(last_bottom - vy) <= 2;
            BOOST_LOG_TRIVIAL(info) << "LayoutProbe: sidebar-check client_h=" << client_h
                                    << " virtual_h=" << vy << " content_h=" << content_h
                                    << " last_bottom=" << last_bottom << (ok ? " ok" : " DEFECT");
            // The info log is filtered at the default level, so the verdict also
            // lands beside the dumps where the driver can read it.
            {
                const boost::filesystem::path out_path =
                    boost::filesystem::path(default_path()).parent_path() / "sidebar-check.json";
                boost::nowide::ofstream out(out_path.string());
                out << "{\"client_h\":" << client_h << ",\"virtual_h\":" << vy
                    << ",\"content_h\":" << content_h << ",\"last_bottom\":" << last_bottom
                    << ",\"ok\":" << (ok ? "true" : "false") << "}\n";
            }
            return ok;
        }
        //   canvas-png <path>    save the 3D canvas's next frame, ImGui panels
        //                        included, as a PNG at <path>. On the real graphics
        //                        driver PrintWindow gets a blank canvas, so a
        //                        capture of an unmodified package cannot show it;
        //                        the canvas reads its frame back just before the swap
        //                        (GLCanvas3D::render) and writes <path>.part first.
        const std::wstring canvas_png = L"canvas-png ";
        if (frame && payload.compare(0, canvas_png.size(), canvas_png) == 0) {
            g_canvas_png = boost::nowide::narrow(payload.substr(canvas_png.size()));
            frame->CallAfter([]() {
                if (Plater *plater = wxGetApp().plater())
                    if (GLCanvas3D *canvas = plater->get_current_canvas3D()) {
                        canvas->set_as_dirty();
                        canvas->request_extra_frame();
                    }
            });
            return !g_canvas_png.empty();
        }
        //   language-audit       in bilingual mode, write language-audit.json
        //                        beside the dumps: every shown native control
        //                        that still shows English only although a
        //                        Cantonese translation exists for it. Defers
        //                        through CallAfter like the other mutating
        //                        commands; wait at least 4 seconds after a
        //                        surface first shows before sending it, since
        //                        the bilingual decorator applies its own
        //                        labels on a delay (see run_language_audit()).
        if (frame && payload == L"language-audit") {
            frame->CallAfter([]() {
                const bool ok = run_language_audit();
                BOOST_LOG_TRIVIAL(info) << "LayoutProbe: language-audit " << (ok ? "wrote report" : "FAILED to write report");
            });
            return true;
        }
        if (bar && payload.compare(0, invoke.size(), invoke) == 0) {
            const bool ok = bar->InvokeMenuItem(wxString(payload.substr(invoke.size())));
            BOOST_LOG_TRIVIAL(info) << "LayoutProbe: invoke " << (ok ? "ok" : "no such item");
            return ok;
        }
    }
    static const std::wstring prefix = L"layout-probe";
    if (payload.compare(0, prefix.size(), prefix) != 0) return false;
    std::string out_path;
    if (payload.size() > prefix.size()) {
        std::wstring rest = payload.substr(prefix.size());
        const size_t start = rest.find_first_not_of(L" \t");
        if (start != std::wstring::npos) out_path = boost::nowide::narrow(rest.substr(start));
    }
    dump("copydata", out_path);
    return true;
}

}}} // namespace Slic3r::GUI::LayoutProbe
