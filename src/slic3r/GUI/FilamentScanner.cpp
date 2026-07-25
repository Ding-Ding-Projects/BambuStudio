#include "FilamentScanner.hpp"

#include "GUI_App.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "Monitor.hpp"
#include "Plater.hpp"
#include "StatusPanel.hpp"
#include "DeviceManager.hpp"
#include "DeviceCore/DevManager.h"
#include "DeviceCore/DevFilaSystem.h"
#include "TtsNarrator.hpp"
#include "Widgets/Button.hpp"
#include "Widgets/CheckBox.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/MD3Motion.hpp"
#include "Widgets/MD3Tokens.hpp"
#include "Widgets/StateColor.hpp"
#include "third_party/qrcodegen.hpp"
#include "slic3r/Utils/Http.hpp"

#include "libslic3r/AppConfig.hpp"
#include "libslic3r/Utils.hpp"

#include "nlohmann/json.hpp"

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/log/trivial.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <random>
#include <thread>

#include <wx/base64.h>
#include <wx/dcbuffer.h>
#include <wx/frame.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>

namespace beast = boost::beast;
namespace http  = beast::http;
using tcp       = boost::asio::ip::tcp;

namespace Slic3r { namespace GUI {

namespace {

constexpr std::size_t kMaxUploadBytes = 12 * 1024 * 1024;

std::string random_token()
{
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> pick(0, sizeof(alphabet) - 2);
    std::string token;
    for (int i = 0; i < 20; ++i)
        token.push_back(alphabet[pick(gen)]);
    return token;
}

// Best local LAN address: open a UDP socket "towards" a public address (no
// packet is sent) and read the chosen local endpoint.
std::string lan_address()
{
    try {
        boost::asio::io_context io;
        boost::asio::ip::udp::socket probe(io);
        probe.connect(boost::asio::ip::udp::endpoint(boost::asio::ip::make_address("8.8.8.8"), 80));
        return probe.local_endpoint().address().to_string();
    } catch (const std::exception &) {
        return "127.0.0.1";
    }
}

// Self-contained bilingual upload page (EN + Cantonese, per the language
// rules for user-facing surfaces; served to the phone, no external assets).
std::string upload_page_html(const std::string &token)
{
    return
        "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Bambu Studio - Filament scan</title><style>"
        "body{font-family:Roboto,system-ui,sans-serif;background:#f7f9f4;color:#191d17;"
        "display:flex;flex-direction:column;align-items:center;padding:24px;gap:16px}"
        "h1{font-size:1.25rem;margin:0}p{margin:0;color:#44483e;text-align:center}"
        "label{background:#146c2e;color:#fff;border-radius:24px;padding:14px 28px;font-weight:600}"
        "input{display:none}#s{font-weight:600}"
        "</style></head><body>"
        "<h1>Filament scan / \xE6\x83\xA8\xE6\x9D\x90\xE6\x8E\x83\xE6\x8F\x8F</h1>"
        "<p>Snap a photo of the spool or its label.<br>"
        "\xE5\xBD\xB1\xE5\xBC\xB5\xE5\x96\xB1\xE6\x9D\x90\xE5\x8D\xB7\xE5\xAE\x9A\xE5\x80\x8B\xE6\xA8\x99\xE7\xB1\xA4\xE5\x85\x88\xE3\x80\x82</p>"
        "<label>Take photo / \xE5\xBD\xB1\xE7\x9B\xB8<input id='f' type='file' accept='image/*' capture='environment'></label>"
        "<p id='s'></p>"
        "<script>document.getElementById('f').onchange=async e=>{const file=e.target.files[0];if(!file)return;"
        "const s=document.getElementById('s');s.textContent='Uploading\\u2026';"
        "try{const r=await fetch('/t/" + token + "/upload',{method:'POST',headers:{'Content-Type':file.type||'image/jpeg'},body:file});"
        "s.textContent=r.ok?'Sent! Check the desktop. / \\u9001\\u5497\\uff01\\u770b\\u8fd4\\u96fb\\u8166\\u3002':'Upload failed ('+r.status+')';}"
        "catch(err){s.textContent='Upload failed: '+err;}};</script>"
        "</body></html>";
}

wxBitmap qr_bitmap(const std::string &text, int scale)
{
    using qrcodegen::QrCode;
    const QrCode qr = QrCode::encodeText(text.c_str(), QrCode::Ecc::MEDIUM);
    const int size = qr.getSize();
    const int border = 3;
    const int dim = (size + 2 * border) * scale;
    wxBitmap bitmap(dim, dim);
    wxMemoryDC dc(bitmap);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(*wxBLACK_BRUSH);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (qr.getModule(x, y))
                dc.DrawRectangle((x + border) * scale, (y + border) * scale, scale, scale);
    dc.SelectObject(wxNullBitmap);
    return bitmap;
}

// --- announcement overlay ---------------------------------------------------

// HUGE flashing full-screen-ish announcement: which slot to load. Click,
// Esc, or 12 seconds dismisses it; the flash is skipped under reduced motion.
class AnnounceOverlay : public wxFrame
{
public:
    AnnounceOverlay(const wxString &slot_label, const wxString &fila, const wxColour &swatch)
        : wxFrame(nullptr, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                  wxFRAME_NO_TASKBAR | wxSTAY_ON_TOP | wxBORDER_NONE)
        , m_slot(slot_label)
        , m_fila(fila)
        , m_swatch(swatch)
    {
        const wxSize screen = wxGetDisplaySize();
        SetSize(screen.x * 3 / 4, screen.y / 2);
        CenterOnScreen();
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &AnnounceOverlay::OnPaint, this);
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) { Close(); });
        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent &e) {
            if (e.GetKeyCode() == WXK_ESCAPE) { Close(); return; }
            e.Skip();
        });
        m_dismiss.Bind(wxEVT_TIMER, [this](wxTimerEvent &) { Close(); });
        m_dismiss.StartOnce(12000);
        if (!MD3::Motion::reduced()) {
            m_flash.Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
                m_flash_on = !m_flash_on;
                if (++m_flash_count > 10)
                    m_flash.Stop(), m_flash_on = false;
                Refresh();
            });
            m_flash.Start(400);
        }
        Show();
        MD3::Motion::FadeIn(this, MD3::Motion::medium1);
    }

private:
    void OnPaint(wxPaintEvent &)
    {
        wxAutoBufferedPaintDC dc(this);
        const wxSize sz = GetClientSize();
        const wxColour bg = m_flash_on ? StateColor::semantic(MD3::Role::PrimaryContainer)
                                       : StateColor::semantic(MD3::Role::InverseSurface);
        const wxColour fg = m_flash_on ? StateColor::semantic(MD3::Role::OnPrimaryContainer)
                                       : StateColor::semantic(MD3::Role::InverseOn);
        dc.SetBackground(wxBrush(bg));
        dc.Clear();
        wxFont huge(sz.GetHeight() / 4, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(huge);
        dc.SetTextForeground(fg);
        wxSize te = dc.GetTextExtent(m_slot);
        dc.DrawText(m_slot, (sz.x - te.x) / 2, sz.y / 6);
        wxFont big(sz.GetHeight() / 10, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_MEDIUM);
        dc.SetFont(big);
        te = dc.GetTextExtent(m_fila);
        const int fy = sz.y * 2 / 3;
        dc.DrawText(m_fila, (sz.x - te.x) / 2 + sz.GetHeight() / 14, fy);
        // colour swatch beside the filament line
        dc.SetBrush(wxBrush(m_swatch));
        dc.SetPen(wxPen(fg, 2));
        const int d = sz.GetHeight() / 12;
        dc.DrawRoundedRectangle((sz.x - te.x) / 2 - d, fy + (te.y - d) / 2, d, d, d / 4);
    }

    wxString m_slot, m_fila;
    wxColour m_swatch;
    wxTimer  m_flash { this }, m_dismiss { this };
    bool     m_flash_on { false };
    int      m_flash_count { 0 };
};

} // namespace

// --- upload server ----------------------------------------------------------

class ScanUploadServer
{
public:
    ScanUploadServer(std::string token, std::function<void(std::string)> on_image)
        : m_token(std::move(token))
        , m_on_image(std::move(on_image))
        , m_acceptor(m_io)
    {
        boost::system::error_code ec;
        m_acceptor.open(tcp::v4(), ec);
        m_acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
        m_acceptor.bind(tcp::endpoint(tcp::v4(), 0), ec); // ephemeral port
        m_acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec) {
            BOOST_LOG_TRIVIAL(warning) << "FilamentScanner: server bind failed: " << ec.message();
            return;
        }
        m_port    = m_acceptor.local_endpoint().port();
        m_running = true;
        m_worker  = std::thread([this]() { accept_loop(); });
    }

    ~ScanUploadServer()
    {
        m_running = false;
        boost::system::error_code ec;
        m_acceptor.close(ec);
        m_io.stop();
        if (m_worker.joinable())
            m_worker.join();
    }

    unsigned short port() const { return m_port; }
    bool           ok() const { return m_running; }

private:
    void accept_loop()
    {
        while (m_running) {
            boost::system::error_code ec;
            tcp::socket socket(m_io);
            m_acceptor.accept(socket, ec);
            if (ec || !m_running)
                break;
            handle(std::move(socket));
        }
    }

    void handle(tcp::socket socket)
    {
        boost::system::error_code ec;
        beast::flat_buffer buffer;
        http::request_parser<http::string_body> parser;
        parser.body_limit(kMaxUploadBytes);
        http::read(socket, buffer, parser, ec);
        if (ec)
            return;
        const auto &req = parser.get();
        http::response<http::string_body> res;
        res.version(req.version());
        res.set(http::field::server, "BambuStudio-FilamentScan");
        const std::string target(req.target());
        const std::string page_path   = "/t/" + m_token + "/";
        const std::string upload_path = "/t/" + m_token + "/upload";
        if (req.method() == http::verb::get && (target == page_path || target == page_path.substr(0, page_path.size() - 1))) {
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/html; charset=utf-8");
            res.body() = upload_page_html(m_token);
        } else if (req.method() == http::verb::post && target == upload_path) {
            const std::string &body = req.body();
            const auto temp = std::filesystem::temp_directory_path() / ("bbs-filament-scan-" + m_token + ".jpg");
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out.write(body.data(), (std::streamsize) body.size());
            out.close();
            res.result(http::status::ok);
            res.set(http::field::content_type, "text/plain");
            res.body() = "ok";
            if (m_on_image)
                m_on_image(temp.string());
        } else {
            res.result(http::status::not_found);
            res.body() = "not found";
        }
        res.prepare_payload();
        http::write(socket, res, ec);
        socket.shutdown(tcp::socket::shutdown_send, ec);
    }

    std::string                       m_token;
    std::function<void(std::string)>  m_on_image;
    boost::asio::io_context           m_io { 1 };
    tcp::acceptor                     m_acceptor;
    std::thread                       m_worker;
    std::atomic<bool>                 m_running { false };
    unsigned short                    m_port { 0 };
};

// --- dialog -----------------------------------------------------------------

FilamentScanDialog::FilamentScanDialog(wxWindow *parent)
    : MD3Dialog(parent, _L("AI filament scanner"),
                _L("Scan a spool with your phone; the result lands in an AMS slot."),
                MaterialIcon::Palette)
{
    const std::string token = random_token();
    m_server = std::make_unique<ScanUploadServer>(token, [this](std::string path) {
        wxTheApp->CallAfter([this, path]() { on_image_received(path); });
    });

    wxBoxSizer *body = GetContentSizer();
    if (m_server->ok()) {
        const std::string url = "http://" + lan_address() + ":" + std::to_string(m_server->port()) + "/t/" + token + "/";
        m_qr_bitmap = new wxStaticBitmap(this, wxID_ANY, qr_bitmap(url, FromDIP(6)));
        body->Add(m_qr_bitmap, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(8));
        m_url_label = new Label(this, Label::Mono_11, wxString::FromUTF8(url));
        m_url_label->SetBackgroundColour(GetBackgroundColour());
        m_url_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurfaceVariant));
        body->Add(m_url_label, 0, wxALIGN_CENTER_HORIZONTAL | wxTOP, FromDIP(6));
        m_status = new Label(this, Label::Body_13,
            _L("Scan the QR code with your phone, then photograph the spool."));
    } else {
        m_status = new Label(this, Label::Body_13,
            _L("The upload server could not start. Check that no firewall rule blocks local listeners."));
    }
    m_status->SetBackgroundColour(GetBackgroundColour());
    m_status->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    m_status->Wrap(FromDIP(420));
    body->Add(m_status, 0, wxEXPAND | wxALL, FromDIP(12));

    m_tts_toggle = new CheckBox(this);
    m_tts_toggle->SetValue(wxGetApp().app_config->get("filament_scan_tts") != "false");
    auto *tts_row = new wxBoxSizer(wxHORIZONTAL);
    tts_row->Add(m_tts_toggle, 0, wxALIGN_CENTER_VERTICAL);
    auto *tts_label = new Label(this, Label::Body_13, _L("Announce the slot out loud (TTS)"));
    tts_label->SetBackgroundColour(GetBackgroundColour());
    tts_label->SetForegroundColour(StateColor::semantic(MD3::Role::OnSurface));
    tts_row->Add(tts_label, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(8));
    body->Add(tts_row, 0, wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    m_tts_toggle->Bind(wxEVT_TOGGLEBUTTON, [this](wxCommandEvent &e) {
        wxGetApp().app_config->set("filament_scan_tts", m_tts_toggle->GetValue() ? "true" : "false");
        wxGetApp().app_config->save();
        e.Skip();
    });

    auto *close = new Button(this, _L("Close"), "", 0, 0, wxID_CANCEL);
    close->SetMinSize(FromDIP(wxSize(104, 40)));
    close->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    GetFooterSizer()->AddStretchSpacer();
    GetFooterSizer()->Add(close, 0, wxALIGN_CENTER_VERTICAL);

    Layout();
    Fit();
    CenterOnParent();
}

FilamentScanDialog::~FilamentScanDialog() = default;

void FilamentScanDialog::on_image_received(std::string image_path)
{
    if (m_analyzing.exchange(true))
        return;
    m_status->SetLabel(_L("Photo received - identifying the filament with the local model..."));
    Layout();

    std::thread([this, image_path]() {
        std::string model = wxGetApp().app_config->get("printer_watch_model");
        if (model.empty()) model = "qwen2.5vl";
        std::string endpoint = wxGetApp().app_config->get("printer_watch_endpoint");
        if (endpoint.empty()) endpoint = "http://127.0.0.1:11434";

        std::string image_b64;
        {
            std::ifstream in(image_path, std::ios::binary);
            std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            image_b64 = wxBase64Encode(bytes.data(), bytes.size()).ToStdString();
        }
        const std::string prompt =
            "This photo shows a 3D-printing filament spool or its label. Reply with STRICT JSON only: "
            "{\"type\":\"PLA|PETG|ABS|TPU|ASA|PC|PA|PVA|OTHER\",\"brand\":\"...\",\"color_hex\":\"#RRGGBB\",\"confidence\":0.0}";
        nlohmann::json body = {{"model", model}, {"prompt", prompt}, {"images", {image_b64}}, {"stream", false}, {"format", "json"}};

        std::string fila_type = "PLA", brand, color_hex = "#22aa55", error;
        auto http = Http::post(endpoint + "/api/generate");
        http.header("Content-Type", "application/json")
            .set_post_body(body.dump())
            .timeout_max(180)
            .on_complete([&](std::string response, unsigned) {
                try {
                    const auto reply  = nlohmann::json::parse(response).value("response", "");
                    const auto parsed = nlohmann::json::parse(reply);
                    fila_type = parsed.value("type", "PLA");
                    brand     = parsed.value("brand", "");
                    color_hex = parsed.value("color_hex", "#22aa55");
                } catch (const std::exception &e) { error = e.what(); }
            })
            .on_error([&](std::string, std::string err, unsigned) { error = err; })
            .perform_sync();

        wxTheApp->CallAfter([this, fila_type, brand, color_hex, error]() {
            m_analyzing = false;
            if (!error.empty()) {
                m_status->SetLabel(wxString::Format(
                    _L("Identification failed: %s. Is a vision-capable Ollama model available?"),
                    wxString::FromUTF8(error)));
                Layout();
                return;
            }
            // --- pick + configure an AMS slot -----------------------------
            wxString slot_label = _L("external spool");
            DeviceManager *manager = wxGetApp().getDeviceManager();
            MachineObject *obj = manager != nullptr ? manager->get_selected_machine() : nullptr;
            if (obj != nullptr && obj->GetFilaSystem() != nullptr) {
                for (auto &[ams_id, ams] : obj->GetFilaSystem()->GetAmsList()) {
                    if (ams == nullptr) continue;
                    for (auto &[tray_id, tray] : ams->GetTrays()) {
                        if (tray == nullptr || !tray->m_fila_type.empty())
                            continue; // occupied
                        const int ams_index  = std::atoi(ams_id.c_str());
                        const int tray_index = std::atoi(tray_id.c_str());
                        std::string hex = color_hex;
                        if (!hex.empty() && hex.front() == '#') hex.erase(hex.begin());
                        static const std::map<std::string, std::pair<int, int>> temps = {
                            {"PLA", {190, 230}}, {"PETG", {220, 260}}, {"ABS", {240, 280}},
                            {"TPU", {200, 250}}, {"ASA", {240, 280}},  {"PC", {250, 280}},
                            {"PA", {250, 290}},  {"PVA", {190, 230}},
                        };
                        const auto it = temps.find(fila_type);
                        const int nmin = it != temps.end() ? it->second.first : 190;
                        const int nmax = it != temps.end() ? it->second.second : 240;
                        obj->command_ams_filament_settings(ams_index, tray_index, "", "", hex + "FF",
                                                           fila_type, nmin, nmax);
                        // TRN: %1$s is the AMS letter/number, %2$s the slot number.
                        slot_label = wxString::Format(_L("AMS %s slot %d"),
                                                      wxString::Format("%c", 'A' + ams_index), tray_index + 1);
                        goto assigned;
                    }
                }
            }
        assigned:
            // --- auto-adjust print settings: pick the best filament preset ---
            // Brand mapping mirrors the vendor families shipped in
            // resources/profiles/BBL.json (Bambu / SUNLU / PolyLite / PolyTerra
            // / Overture / eSUN / Fiberon; everything else falls back to the
            // Generic profile for the identified material).
            wxString preset_note;
            {
                auto *bundle = wxGetApp().preset_bundle;
                std::string brand_lc = brand;
                for (auto &c : brand_lc) c = (char) std::tolower((unsigned char) c);
                std::vector<std::string> prefixes;
                auto add_brand = [&](const char *needle, const char *family) {
                    if (brand_lc.find(needle) != std::string::npos)
                        prefixes.push_back(std::string(family) + " " + fila_type);
                };
                add_brand("bambu", "Bambu");
                add_brand("polymaker", "PolyLite");
                add_brand("polyterra", "PolyTerra");
                add_brand("polylite", "PolyLite");
                add_brand("esun", "eSUN");
                add_brand("sunlu", "SUNLU");
                add_brand("overture", "Overture");
                add_brand("fiberon", "Fiberon");
                prefixes.push_back("Generic " + fila_type);
                std::string chosen;
                if (bundle != nullptr) {
                    for (const std::string &prefix : prefixes) {
                        for (const auto &preset : bundle->filaments) {
                            if (!preset.is_visible || preset.name.rfind(prefix, 0) != 0)
                                continue;
                            chosen = preset.name;
                            break;
                        }
                        if (!chosen.empty())
                            break;
                    }
                    if (!chosen.empty()) {
                        bundle->set_filament_preset(0, chosen);
                        if (auto *plater = wxGetApp().plater())
                            plater->sidebar().update_presets(Preset::TYPE_FILAMENT);
                        // TRN: %s is a filament preset name applied automatically.
                        preset_note = wxString::Format(_L(" Print settings set to \"%s\"."),
                                                       wxString::FromUTF8(chosen));
                    }
                }
            }
            const wxString fila_line = brand.empty()
                ? wxString::FromUTF8(fila_type)
                : wxString::Format("%s - %s", wxString::FromUTF8(brand), wxString::FromUTF8(fila_type));
            m_status->SetLabel(wxString::Format(
                _L("Identified %s (%s). Load it into %s."),
                fila_line, wxString::FromUTF8(color_hex), slot_label) + preset_note);
            Layout();
            announce(slot_label, wxString::FromUTF8(fila_type), wxString::FromUTF8(brand),
                     wxString::FromUTF8(color_hex));
        });
    }).detach();
}

void FilamentScanDialog::announce(const wxString &slot_label, const wxString &fila_type,
                                  const wxString &brand, const wxString &color_hex)
{
    wxColour swatch(color_hex);
    if (!swatch.IsOk()) swatch = wxColour(34, 170, 85);
    new AnnounceOverlay(slot_label.Upper(),
                        brand.IsEmpty() ? fila_type : brand + " " + fila_type, swatch);
    if (m_tts_toggle != nullptr && m_tts_toggle->GetValue())
        // TRN: TTS line; %1$s filament type, %2$s slot label.
        TtsNarrator::say_now(wxString::Format(_L("Filament identified: %s. Load it into %s."),
                                              fila_type, slot_label));
}

} } // namespace Slic3r::GUI
