#include "LayoutProbe.hpp"

#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "BBLTopbar.hpp"
#include "GLCanvas3D.hpp"
#include "Plater.hpp"
#include "NotificationManager.hpp"
#include "CommandPalette.hpp"
#include "ConfigWizard.hpp"
#include <wx/scrolwin.h>
#include <cwchar>
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/StateColor.hpp"
#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Utils.hpp"

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/convert.hpp>
#include <boost/nowide/fstream.hpp>

#include <wx/app.h>
#include <wx/control.h>
#include <wx/aui/auibar.h>
#include <wx/glcanvas.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/toplevel.h>
#include <wx/utils.h>
#include <wx/window.h>

#include <atomic>
#include <functional>
#include <cstdint>
#include <sstream>
#include <string>

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

std::uintptr_t handle_of(const wxWindow *w)
{
#ifdef _WIN32
    return reinterpret_cast<std::uintptr_t>(w->GetHWND());
#else
    return reinterpret_cast<std::uintptr_t>(w);
#endif
}

// Sum of the minimum sizes a wxBoxSizer must pay along its orientation, with
// borders, against what it actually has. This is the starvation detector.
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
        int need = v.orient == wxHORIZONTAL ? min.x : min.y;
        const int flag = item->GetFlag();
        const int border = item->GetBorder();
        if (v.orient == wxHORIZONTAL) {
            if (flag & wxLEFT) need += border;
            if (flag & wxRIGHT) need += border;
        } else {
            if (flag & wxTOP) need += border;
            if (flag & wxBOTTOM) need += border;
        }
        v.required += need;
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

void write_window(boost::nowide::ofstream &out, wxWindow *w, wxWindow *top, int depth)
{
    wxWindow *parent = w->GetParent();
    const wxRect rect = w->GetRect();
    const wxRect screen = w->GetScreenRect();
    const wxSize client = w->GetClientSize();
    const bool shown = w->IsShown();

    // wxWindow carries the label, not only wxControl: the kit Button and
    // every other StaticBox-based control descend from wxWindow directly and
    // do set their text, so reading it only through wxControl dumped every
    // kit button with an empty label and the recapture driver found no tab.
    std::string label = std::string(w->GetLabel().ToUTF8().data());
    bool        has_label = !label.empty();

    bool text_clipped = false;
    bool ellipsized = false;
    int text_width = -1;
    if (auto *st = dynamic_cast<wxStaticText *>(w)) {
        const long style = st->GetWindowStyle();
        ellipsized = (style & (wxST_ELLIPSIZE_START | wxST_ELLIPSIZE_MIDDLE | wxST_ELLIPSIZE_END)) != 0;
        const wxString text = st->GetLabel();
        if (!text.empty() && text.Find('\n') == wxNOT_FOUND) {
            text_width = st->GetTextExtent(text).x;
            text_clipped = shown && !ellipsized && text_width > client.x;
        }
    } else if (has_label && dynamic_cast<wxControl *>(w)) {
        const wxString text = wxString::FromUTF8(label.c_str());
        if (text.Find('\n') == wxNOT_FOUND) {
            text_width = w->GetTextExtent(text).x;
            // Custom controls draw icons and padding too; report the extent and
            // let the reader judge, flagging only the unambiguous case.
            text_clipped = shown && text_width > client.x;
        }
    }

    bool clipped_by_parent = false;
    if (parent) {
        const wxRect parent_client(wxPoint(0, 0), parent->GetClientSize());
        clipped_by_parent = shown && rect.width > 0 && rect.height > 0 && !parent_client.Contains(rect);
    }

    // Sizer view of this window: allocation versus minimum, and the row verdict.
    wxSizer *owner = nullptr;
    wxSizerItem *item = parent ? item_for(parent->GetSizer(), w, &owner) : nullptr;
    std::string sizer_json = "null";
    bool starved = false;
    bool zero_sized = shown && (rect.width == 0 || rect.height == 0);
    if (item) {
        const wxSize min = item->CalcMin();
        const wxSize alloc = item->GetSize();
        RowVerdict row = judge_sizer(owner);
        if (row.is_box) {
            const int have = row.orient == wxHORIZONTAL ? alloc.x : alloc.y;
            const int need = row.orient == wxHORIZONTAL ? min.x : min.y;
            starved = shown && need > 0 && have < need;
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
        << ",\"name\":" << json(w->GetName())
        << ",\"label\":" << json(label)
        << ",\"shown\":" << (shown ? "true" : "false")
        << ",\"on_screen\":" << (w->IsShownOnScreen() ? "true" : "false")
        << ",\"enabled\":" << (w->IsEnabled() ? "true" : "false")
        << ",\"rect\":" << rect_json(rect)
        << ",\"screen\":" << rect_json(screen)
        << ",\"client\":" << size_json(client)
        << ",\"min\":" << size_json(w->GetMinSize())
        << ",\"best\":" << size_json(w->GetBestSize())
        << ",\"text_width\":" << text_width
        << ",\"ellipsized\":" << (ellipsized ? "true" : "false")
        << ",\"text_clipped\":" << (text_clipped ? "true" : "false")
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

} // namespace

bool enabled()
{
    static const bool on = !env_value("BAMBU_LAYOUT_PROBE").empty();
    return on;
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
    if (Plater *plater = wxGetApp().plater()) {
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
