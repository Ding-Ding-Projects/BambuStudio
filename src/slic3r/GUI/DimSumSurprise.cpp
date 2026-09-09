#include "DimSumSurprise.hpp"
#include "DimSumSurpriseModel.hpp"

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "GUI_Init.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/StateColor.hpp"
#include "slic3r/Utils/Http.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Utils.hpp"

#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>
#include <boost/nowide/fstream.hpp>

#include <atomic>
#include <chrono>
#include <ctime>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <wx/dcbuffer.h>
#include <wx/dialog.h>
#include <wx/graphics.h>
#include <wx/image.h>
#include <wx/log.h>
#include <wx/toplevel.h>
#include <wx/arrstr.h>
#include <wx/popupwin.h>
#include <wx/timer.h>
#include <wx/tooltip.h>

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = boost::filesystem;

namespace Slic3r { namespace GUI { namespace DimSumSurprise {

using namespace Slic3r::GUI::DimSum;

namespace {

// ------------------------------------------------------------------ state

std::atomic<bool> s_startup_error { false };
std::atomic<bool> s_background_busy { false };
class DimSumCard;
DimSumCard *s_card { nullptr };

// ------------------------------------------------------------ cache paths

fs::path cache_root() { return fs::path(data_dir()) / "dim-sum"; }
fs::path catalog_path() { return cache_root() / "catalog.json"; }
fs::path photos_dir() { return cache_root() / "photos"; }
fs::path photo_path(const Dish &dish) { return photos_dir() / dish.image_file; }

bool photo_cached(const Dish &dish)
{
    boost::system::error_code ec;
    const fs::path path = photo_path(dish);
    return fs::is_regular_file(path, ec) && fs::file_size(path, ec) > 0 && !ec;
}

std::size_t cached_photo_count(const std::vector<Dish> &dishes)
{
    std::size_t count = 0;
    for (const Dish &dish : dishes)
        if (photo_cached(dish))
            ++count;
    return count;
}

std::optional<CatalogCache> load_cache()
{
    boost::system::error_code ec;
    const fs::path path = catalog_path();
    if (!fs::is_regular_file(path, ec) || ec)
        return std::nullopt;
    // A compact record of ~3000 dishes is about a megabyte; anything past the
    // catalog bound is not ours.
    if (fs::file_size(path, ec) > CATALOG_SIZE_LIMIT_BYTES || ec)
        return std::nullopt;
    boost::nowide::ifstream in(path.string(), std::ios::binary);
    if (!in)
        return std::nullopt;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return parse_cache(text);
}

bool write_file_atomically(const fs::path &target, const std::string &bytes)
{
    boost::system::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    const fs::path temp = target.string() + ".part";
    {
        boost::nowide::ofstream out(temp.string(), std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out)
            return false;
    }
    fs::rename(temp, target, ec);
    if (ec) {
        fs::remove(temp, ec);
        return false;
    }
    return true;
}

std::string utc_now_iso8601()
{
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm {};
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buffer;
}

// Pulls the ETag value out of a raw header block, case-insensitively.
std::string etag_from_headers(const std::string &headers)
{
    std::string lower = headers;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char) std::tolower(c); });
    std::size_t pos = 0;
    std::string found;
    while ((pos = lower.find("etag:", pos)) != std::string::npos) {
        std::size_t start = pos + 5;
        std::size_t end   = lower.find_first_of("\r\n", start);
        if (end == std::string::npos)
            end = lower.size();
        std::string value = headers.substr(start, end - start);
        while (!value.empty() && (value.front() == ' ' || value.front() == '"' || value.front() == 'W' || value.front() == '/'))
            value.erase(value.begin());
        while (!value.empty() && (value.back() == ' ' || value.back() == '"'))
            value.pop_back();
        if (!value.empty())
            found = value; // the last hop's ETag wins after redirects
        pos = end;
    }
    return found;
}

bool looks_like_png(const std::string &bytes)
{
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (bytes.size() < sizeof(signature))
        return false;
    for (std::size_t i = 0; i < sizeof(signature); ++i)
        if (static_cast<unsigned char>(bytes[i]) != signature[i])
            return false;
    return true;
}

// -------------------------------------------------------- background work

// Worker thread. One bounded HTTPS GET of the public catalog when the cache
// is missing, then photo downloads until PHOTO_CACHE_TARGET are on disk. No
// UI work happens here and nothing waits on it.
void warm_cache_worker(std::optional<CatalogCache> cache)
{
    if (!cache) {
        std::string body;
        std::string headers;
        std::string error;
        unsigned    status = 0;
        auto http = Http::get(CATALOG_URL);
        http.size_limit(CATALOG_SIZE_LIMIT_BYTES)
            .timeout_connect(10)
            .timeout_max(CATALOG_TIMEOUT_SECONDS)
            .on_header_callback([&headers](std::string h) { headers = std::move(h); })
            .on_complete([&body, &status](std::string b, unsigned s) { body = std::move(b); status = s; })
            .on_error([&error, &status](std::string, std::string e, unsigned s) { error = e.empty() ? "HTTP error" : e; status = s; })
            .perform_sync();
        if (!error.empty() || body.empty()) {
            BOOST_LOG_TRIVIAL(info) << "DimSumSurprise: public catalog unavailable (" << error << ", status " << status
                                    << "); nothing shown until it can be fetched";
            s_background_busy = false;
            return;
        }
        CatalogCache fresh;
        fresh.source_url = CATALOG_URL;
        fresh.revision   = etag_from_headers(headers);
        if (fresh.revision.empty())
            fresh.revision = "unknown";
        fresh.fetched_at = utc_now_iso8601();
        fresh.dishes     = parse_catalog(body);
        if (fresh.dishes.empty()) {
            BOOST_LOG_TRIVIAL(warning) << "DimSumSurprise: public catalog parsed to zero valid dishes; not caching";
            s_background_busy = false;
            return;
        }
        if (!write_file_atomically(catalog_path(), serialize_cache(fresh))) {
            BOOST_LOG_TRIVIAL(warning) << "DimSumSurprise: could not write " << catalog_path().string();
            s_background_busy = false;
            return;
        }
        BOOST_LOG_TRIVIAL(info) << "DimSumSurprise: cached " << fresh.dishes.size() << " dishes from " << CATALOG_URL
                                << " revision " << fresh.revision;
        cache = std::move(fresh);
    }

    std::mt19937 rng { std::random_device {}() };
    std::size_t  have    = cached_photo_count(cache->dishes);
    int          attempts = 0;
    while (have < PHOTO_CACHE_TARGET && attempts < 3 * static_cast<int>(PHOTO_CACHE_TARGET)) {
        ++attempts;
        std::optional<Dish> next = pick_prefetch(cache->dishes, photo_cached, rng);
        if (!next)
            break;
        bool stored = false;
        for (const std::string &url : asset_url_candidates(*next)) {
            std::string body;
            std::string error;
            unsigned    status = 0;
            auto http = Http::get(url);
            http.size_limit(PHOTO_SIZE_LIMIT_BYTES)
                .timeout_connect(10)
                .timeout_max(PHOTO_TIMEOUT_SECONDS)
                .on_complete([&body, &status](std::string b, unsigned s) { body = std::move(b); status = s; })
                .on_error([&error, &status](std::string, std::string e, unsigned s) { error = e.empty() ? "HTTP error" : e; status = s; })
                .perform_sync();
            if (!error.empty() || body.empty()) {
                if (status == 404)
                    continue; // try the next volume
                BOOST_LOG_TRIVIAL(info) << "DimSumSurprise: photo fetch failed (" << error << ", status " << status << "): " << url;
                break;
            }
            if (!looks_like_png(body)) {
                BOOST_LOG_TRIVIAL(warning) << "DimSumSurprise: asset is not a PNG, discarded: " << url;
                break;
            }
            if (write_file_atomically(photo_path(*next), body)) {
                stored = true;
                ++have;
            }
            break;
        }
        if (!stored && attempts >= 3) {
            // Network is not cooperating; stop hammering it this launch.
            break;
        }
    }
    s_background_busy = false;
}

void warm_cache_in_background(std::optional<CatalogCache> cache)
{
    if (cache && cached_photo_count(cache->dishes) >= PHOTO_CACHE_TARGET)
        return;
    if (s_background_busy.exchange(true))
        return;
    std::thread(warm_cache_worker, std::move(cache)).detach();
}

// ------------------------------------------------------------ OS settings

bool os_quiet_mode()
{
#ifdef _WIN32
    QUERY_USER_NOTIFICATION_STATE state {};
    if (SUCCEEDED(::SHQueryUserNotificationState(&state))) {
        switch (state) {
        case QUNS_BUSY:
        case QUNS_RUNNING_D3D_FULL_SCREEN:
        case QUNS_PRESENTATION_MODE:
        case QUNS_QUIET_TIME:
        case QUNS_APP:
            return true;
        default:
            break;
        }
    }
#endif
    return false;
}

bool os_reduced_motion()
{
#ifdef _WIN32
    BOOL animations = TRUE;
    if (::SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
        return animations == FALSE;
#endif
    return false;
}

bool any_modal_dialog_open()
{
    for (wxWindow *window : wxTopLevelWindows) {
        auto *dialog = dynamic_cast<wxDialog *>(window);
        if (dialog != nullptr && dialog->IsShown() && dialog->IsModal())
            return true;
    }
    return false;
}

// ------------------------------------------------------------------ copy

struct CardCopy {
    wxString badge;
    wxString name;
    wxString line;      // primary language
    wxString line2;     // secondary language in bilingual mode, else empty
    wxString alt;
    wxString dismiss;
};

CardCopy build_copy(const Dish &dish)
{
    const I18N::LanguageModeProfile &profile = I18N::language_mode_profile();
    const AppConfig *config = wxGetApp().app_config;
    const int level_en  = parse_funny_level(config ? config->get("funny_level_en") : std::string());
    const int level_yue = parse_funny_level(config ? config->get("funny_level_yue") : std::string());

    const bool cantonese_first = profile.kind == I18N::LanguageModeKind::CantoneseHongKong;
    CardCopy copy;
    copy.name = from_u8(display_name(dish, cantonese_first));
    copy.alt  = from_u8(alt_text(dish, cantonese_first));

    const std::string name_utf8 = into_u8(copy.name);
    // _L() applies the product vocabulary to the translated sentence; the dish
    // name is substituted afterwards so it is never rewritten.
    const wxString line_en = from_u8(apply_dish_name(into_u8(_L(surprise_line_english_source(level_en))), name_utf8));
    const wxString line_yue = from_u8(apply_dish_name(surprise_line_cantonese(level_yue), name_utf8));

    switch (profile.kind) {
    case I18N::LanguageModeKind::CantoneseHongKong:
        copy.badge   = from_u8(badge_cantonese());
        copy.line    = line_yue;
        copy.dismiss = from_u8(dismiss_cantonese());
        break;
    case I18N::LanguageModeKind::BilingualEnglishCantoneseHongKong:
        copy.badge   = _L(badge_english_source()) + wxString::FromUTF8(" \xC2\xB7 ") + from_u8(badge_cantonese());
        copy.line    = line_en;
        copy.line2   = line_yue;
        copy.dismiss = _L(dismiss_english_source()) + wxString::FromUTF8(" \xC2\xB7 ") + from_u8(dismiss_cantonese());
        break;
    default:
        copy.badge   = _L(badge_english_source());
        copy.line    = line_en;
        copy.dismiss = _L(dismiss_english_source());
        break;
    }
    return copy;
}

// ------------------------------------------------------------------ card

// Word-wraps text to max_width using dc's current font. Tokens wider than a
// line (CJK runs, long words) are broken per character.
wxArrayString wrap_text(wxDC &dc, const wxString &text, int max_width)
{
    wxArrayString lines;
    wxArrayString paragraphs = wxSplit(text, '\n', '\0');
    for (const wxString &paragraph : paragraphs) {
        wxString current;
        auto flush = [&]() {
            lines.Add(current);
            current.clear();
        };
        auto fits = [&](const wxString &candidate) { return dc.GetTextExtent(candidate).x <= max_width; };
        wxArrayString words = wxSplit(paragraph, ' ', '\0');
        for (wxString word : words) {
            if (word.empty())
                continue;
            wxString candidate = current.empty() ? word : current + " " + word;
            if (fits(candidate)) {
                current = candidate;
                continue;
            }
            if (!current.empty())
                flush();
            // Break an oversized token per character.
            for (wxUniChar ch : word) {
                wxString next = current + ch;
                if (!fits(next) && !current.empty())
                    flush();
                current += ch;
            }
        }
        flush();
    }
    return lines;
}

class DimSumCard : public wxPopupWindow
{
public:
    DimSumCard(wxWindow *parent, const Dish &dish, const wxImage &photo)
        : wxPopupWindow(parent, wxBORDER_NONE), m_parent(parent), m_dish(dish), m_copy(build_copy(dish))
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetName(m_copy.alt);
        SetToolTip(m_copy.alt);

        const int photo_px = FromDIP(96);
        wxImage scaled = photo;
        if (scaled.GetWidth() != photo_px || scaled.GetHeight() != photo_px)
            scaled.Rescale(photo_px, photo_px, wxIMAGE_QUALITY_HIGH);
        m_photo = wxBitmap(scaled);

        layout();

        Bind(wxEVT_PAINT, &DimSumCard::on_paint, this);
        Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) { dismiss(); });
        Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent &) { m_hovered = true; m_dismiss_timer.Stop(); });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent &) { m_hovered = false; arm_dismiss(); });
        m_dismiss_timer.Bind(wxEVT_TIMER, [this](wxTimerEvent &) { if (!m_hovered) dismiss(); });
        m_fade_timer.Bind(wxEVT_TIMER, &DimSumCard::on_fade_tick, this);

        m_parent->Bind(wxEVT_MOVE, &DimSumCard::on_parent_geometry, this);
        m_parent->Bind(wxEVT_SIZE, &DimSumCard::on_parent_geometry, this);
        m_parent->Bind(wxEVT_ICONIZE, &DimSumCard::on_parent_iconize, this);
        m_parent->Bind(wxEVT_CHAR_HOOK, &DimSumCard::on_parent_char_hook, this);
        m_parent->Bind(wxEVT_CLOSE_WINDOW, &DimSumCard::on_parent_close, this);
    }

    void present()
    {
        reposition();
        if (!os_reduced_motion() && CanSetTransparent()) {
            m_alpha = 0;
            SetTransparent(0);
            Show(true);
            m_fade_timer.Start(16);
        } else {
            SetTransparent(255);
            Show(true);
        }
        arm_dismiss();
        BOOST_LOG_TRIVIAL(info) << "DimSumSurprise: showing " << m_dish.id << " (" << m_dish.name_en << ")";
    }

    void dismiss()
    {
        if (m_closing)
            return;
        m_closing = true;
        m_dismiss_timer.Stop();
        m_fade_timer.Stop();
        unbind_parent();
        Hide();
        s_card = nullptr;
        CallAfter([this] { Destroy(); });
    }

private:
    void arm_dismiss() { m_dismiss_timer.StartOnce(AUTO_DISMISS_MILLISECONDS); }

    void unbind_parent()
    {
        if (m_parent == nullptr)
            return;
        m_parent->Unbind(wxEVT_MOVE, &DimSumCard::on_parent_geometry, this);
        m_parent->Unbind(wxEVT_SIZE, &DimSumCard::on_parent_geometry, this);
        m_parent->Unbind(wxEVT_ICONIZE, &DimSumCard::on_parent_iconize, this);
        m_parent->Unbind(wxEVT_CHAR_HOOK, &DimSumCard::on_parent_char_hook, this);
        m_parent->Unbind(wxEVT_CLOSE_WINDOW, &DimSumCard::on_parent_close, this);
        m_parent = nullptr;
    }

    void layout()
    {
        wxClientDC dc(this);
        m_pad       = FromDIP(16);
        m_gap       = FromDIP(12);
        m_radius    = FromDIP(16);
        const int width = FromDIP(380);
        const int text_width = width - 2 * m_pad - m_photo.GetWidth() - m_gap - FromDIP(28);

        dc.SetFont(Label::Body_12);
        m_badge_lines = wrap_text(dc, m_copy.badge, text_width);
        m_badge_line_h = dc.GetCharHeight();
        dc.SetFont(Label::Head_14);
        m_name_lines = wrap_text(dc, m_copy.name, text_width);
        m_name_line_h = dc.GetCharHeight();
        dc.SetFont(Label::Body_12);
        m_line_lines = wrap_text(dc, m_copy.line, text_width);
        if (!m_copy.line2.empty()) {
            wxArrayString second = wrap_text(dc, m_copy.line2, text_width);
            for (const wxString &s : second)
                m_line_lines.Add(s);
        }
        m_line_line_h = dc.GetCharHeight();

        int text_height = (int) m_badge_lines.size() * m_badge_line_h + FromDIP(4) +
                          (int) m_name_lines.size() * m_name_line_h + FromDIP(4) +
                          (int) m_line_lines.size() * m_line_line_h;
        const int height = 2 * m_pad + std::max(text_height, m_photo.GetHeight());
        SetSize(width, height);
        SetMinSize(wxSize(width, height));
    }

    void reposition()
    {
        if (m_parent == nullptr)
            return;
        const wxRect client = m_parent->GetClientRect();
        const wxPoint origin = m_parent->ClientToScreen(client.GetTopLeft());
        const wxSize size = GetSize();
        const int margin = FromDIP(16);
        wxPoint pos(origin.x + client.width - size.x - margin, origin.y + client.height - size.y - margin);
        Position(pos, wxSize(0, 0));
    }

    void on_parent_geometry(wxEvent &event)
    {
        event.Skip();
        reposition();
    }

    void on_parent_iconize(wxIconizeEvent &event)
    {
        event.Skip();
        if (event.IsIconized())
            dismiss();
    }

    void on_parent_close(wxCloseEvent &event)
    {
        event.Skip();
        dismiss();
    }

    void on_parent_char_hook(wxKeyEvent &event)
    {
        // Escape dismisses the card but is never swallowed: the focused
        // control still sees it, because the card never had focus.
        event.Skip();
        if (event.GetKeyCode() == WXK_ESCAPE)
            dismiss();
    }

    void on_fade_tick(wxTimerEvent &)
    {
        m_alpha = std::min(255, m_alpha + 32);
        SetTransparent((wxByte) m_alpha);
        if (m_alpha >= 255)
            m_fade_timer.Stop();
    }

    void on_paint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        const wxColour container = StateColor::semantic(MD3::Role::SurfaceContainerHigh);
        const wxColour outline   = StateColor::semantic(MD3::Role::OutlineVariant);
        const wxColour on_surface = StateColor::semantic(MD3::Role::OnSurface);
        const wxColour on_variant = StateColor::semantic(MD3::Role::OnSurfaceVariant);
        const wxColour primary    = StateColor::semantic(MD3::Role::Primary);

        // Popup windows are rectangular; paint the corners in the host's
        // surface tone so the rounded card reads as floating over it.
        dc.SetBrush(wxBrush(StateColor::semantic(MD3::Role::Surface)));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(GetClientRect());

        const wxRect rect = GetClientRect();
        std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::Create(dc));
        if (gc) {
            gc->SetAntialiasMode(wxANTIALIAS_DEFAULT);
            gc->SetBrush(wxBrush(container));
            gc->SetPen(wxPen(outline, 1));
            gc->DrawRoundedRectangle(rect.x + 0.5, rect.y + 0.5, rect.width - 1.0, rect.height - 1.0, m_radius);

            // Photo with rounded corners.
            const int photo_x = m_pad;
            const int photo_y = m_pad;
            wxGraphicsPath clip = gc->CreatePath();
            clip.AddRoundedRectangle(photo_x, photo_y, m_photo.GetWidth(), m_photo.GetHeight(), FromDIP(12));
            gc->PushState();
            gc->Clip(wxRegion(wxRect(photo_x, photo_y, m_photo.GetWidth(), m_photo.GetHeight())));
            gc->SetBrush(wxBrush(container));
            gc->SetPen(*wxTRANSPARENT_PEN);
            gc->FillPath(clip);
            gc->DrawBitmap(m_photo, photo_x, photo_y, m_photo.GetWidth(), m_photo.GetHeight());
            gc->PopState();
        } else {
            dc.SetBrush(wxBrush(container));
            dc.SetPen(wxPen(outline, 1));
            dc.DrawRoundedRectangle(rect, m_radius);
            dc.DrawBitmap(m_photo, m_pad, m_pad, true);
        }

        // Text column.
        int x = m_pad + m_photo.GetWidth() + m_gap;
        int y = m_pad;
        dc.SetFont(Label::Body_12);
        dc.SetTextForeground(primary);
        for (const wxString &line : m_badge_lines) { dc.DrawText(line, x, y); y += m_badge_line_h; }
        y += FromDIP(4);
        dc.SetFont(Label::Head_14);
        dc.SetTextForeground(on_surface);
        for (const wxString &line : m_name_lines) { dc.DrawText(line, x, y); y += m_name_line_h; }
        y += FromDIP(4);
        dc.SetFont(Label::Body_12);
        dc.SetTextForeground(on_variant);
        for (const wxString &line : m_line_lines) { dc.DrawText(line, x, y); y += m_line_line_h; }

        // Dismiss glyph, top-right. The whole card is the click target.
        dc.SetFont(Label::Head_14);
        dc.SetTextForeground(on_variant);
        const wxString glyph = wxString::FromUTF8("\xC3\x97"); // multiplication sign
        const wxSize glyph_size = dc.GetTextExtent(glyph);
        dc.DrawText(glyph, rect.width - m_pad - glyph_size.x, m_pad - FromDIP(2));
    }

    wxWindow     *m_parent;
    Dish          m_dish;
    CardCopy      m_copy;
    wxBitmap      m_photo;
    wxTimer       m_dismiss_timer;
    wxTimer       m_fade_timer;
    wxArrayString m_badge_lines, m_name_lines, m_line_lines;
    int           m_badge_line_h { 0 }, m_name_line_h { 0 }, m_line_line_h { 0 };
    int           m_pad { 16 }, m_gap { 12 }, m_radius { 16 };
    int           m_alpha { 255 };
    bool          m_hovered { false };
    bool          m_closing { false };
};

// ------------------------------------------------------------ show logic

void show_now()
{
    GUI_App &app = wxGetApp();
    if (app.is_closing() || app.mainframe == nullptr)
        return;
    Eligibility late;
    late.prior_launch_recorded = true;
    late.onboarding_finished   = true;
    late.modal_dialog_open     = any_modal_dialog_open();
    late.quiet_mode            = os_quiet_mode();
    late.main_frame_visible    = app.mainframe->IsShownOnScreen() && !app.mainframe->IsIconized();
    if (const char *reason = ineligibility_reason(late)) {
        BOOST_LOG_TRIVIAL(info) << "DimSumSurprise: drawn but not shown: " << reason;
        return;
    }

    std::optional<CatalogCache> cache = load_cache();
    if (!cache || cache->dishes.empty()) {
        BOOST_LOG_TRIVIAL(info) << "DimSumSurprise: drawn but the catalog cache is empty; nothing shown this launch";
        return;
    }
    std::mt19937 rng { std::random_device {}() };
    std::optional<Dish> dish = pick_dish(cache->dishes, photo_cached, rng);
    if (!dish) {
        BOOST_LOG_TRIVIAL(info) << "DimSumSurprise: drawn but no photo is cached yet; nothing shown this launch";
        return;
    }
    wxImage photo;
    {
        wxLogNull silence;
        photo.LoadFile(from_u8(photo_path(*dish).string()), wxBITMAP_TYPE_PNG);
    }
    if (!photo.IsOk() || photo.GetWidth() < 64 || photo.GetHeight() < 64) {
        boost::system::error_code ec;
        fs::remove(photo_path(*dish), ec);
        BOOST_LOG_TRIVIAL(warning) << "DimSumSurprise: cached photo did not decode and was removed: " << dish->image_file;
        return;
    }
    if (s_card != nullptr)
        return;
    s_card = new DimSumCard(app.mainframe, *dish, photo);
    s_card->present();
}

} // namespace

void mark_startup_error() { s_startup_error = true; }

bool card_visible() { return s_card != nullptr; }

void maybe_show_after_startup(bool config_wizard_shown)
{
    static LaunchGuard guard;
    if (!guard.claim())
        return;

    GUI_App &app = wxGetApp();
    if (app.app_config == nullptr || app.mainframe == nullptr)
        return;
    AppConfig &config = *app.app_config;

    Eligibility early;
    early.onboarding_finished   = config.get("firstguide", "finish") == "true";
    early.prior_launch_recorded = config.get("dim_sum_prior_launch") == "true";
    early.cli_input_files       = app.init_params != nullptr && !app.init_params->input_files.empty();
    early.config_wizard_shown   = config_wizard_shown;
    early.startup_error_shown   = s_startup_error.load();
    early.modal_dialog_open     = false; // checked again right before showing
    early.quiet_mode            = false;
    early.main_frame_visible    = true;

    // Every launch that got this far counts as a prior launch for the next one.
    if (early.onboarding_finished)
        config.set("dim_sum_prior_launch", "true");

    if (const char *reason = ineligibility_reason(early)) {
        BOOST_LOG_TRIVIAL(info) << "DimSumSurprise: not eligible this launch: " << reason;
        return;
    }

    // Keep the cache warm whether or not this launch draws, so the first
    // eligible surprise has a photo to show.
    warm_cache_in_background(load_cache());

    std::mt19937 rng { std::random_device {}() };
    if (!draw(rng)) {
        BOOST_LOG_TRIVIAL(debug) << "DimSumSurprise: no surprise this launch";
        return;
    }

    // Let the window settle before the card appears; the startup path never
    // waits on this.
    auto *handler = new wxEvtHandler();
    auto *timer   = new wxTimer(handler);
    handler->Bind(wxEVT_TIMER, [handler, timer](wxTimerEvent &) {
        show_now();
        delete timer;
        wxTheApp->CallAfter([handler] { delete handler; });
    });
    timer->StartOnce(STARTUP_DELAY_MILLISECONDS);
}

} } } // namespace Slic3r::GUI::DimSumSurprise
