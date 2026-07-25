#ifndef slic3r_GUI_FilamentScanner_hpp_
#define slic3r_GUI_FilamentScanner_hpp_

#include "Widgets/MD3Dialog.hpp"

#include <atomic>
#include <memory>
#include <string>

class Label;
class Button;
class CheckBox;
class wxStaticBitmap;

namespace Slic3r { namespace GUI {

class ScanUploadServer;

// AI filament scanner: snap a photo of a spool on your phone, the desktop
// identifies it with a LOCAL vision model and loads the result straight into
// an AMS slot.
//
// Flow:
//   1. The dialog starts a tiny LAN-only upload server and shows a QR code
//      (plus the plain URL) for the phone. The URL carries a random
//      single-session token; requests without it are rejected, uploads are
//      size-capped, and the server lives only while the dialog is open.
//   2. The phone opens the upload page (self-contained HTML, camera capture)
//      and posts the photo.
//   3. The desktop asks the local Ollama vision model (same backend and
//      model preference as the printer watch: qwen2.5vl default, Gemma
//      works, gpt-oss is text-only) for a strict-JSON identification:
//      filament type, brand, colour.
//   4. If a printer with an AMS is connected, the first empty slot is
//      configured with the identified type/colour
//      (command_ams_filament_settings) and the user is told which slot to
//      load — via a HUGE flashing full-screen announcement, an optional TTS
//      voice line (toggle in the dialog, persisted), and the dialog status.
//      Hardware slot LEDs expose no blink API, so the flash is on-screen.
class FilamentScanDialog final : public MD3Dialog
{
public:
    explicit FilamentScanDialog(wxWindow *parent);
    ~FilamentScanDialog() override;

private:
    void on_image_received(std::string image_path);
    void announce(const wxString &slot_label, const wxString &fila_type,
                  const wxString &brand, const wxString &color_hex);

    std::unique_ptr<ScanUploadServer> m_server;
    wxStaticBitmap *m_qr_bitmap { nullptr };
    Label          *m_url_label { nullptr };
    Label          *m_status { nullptr };
    CheckBox       *m_tts_toggle { nullptr };
    std::atomic<bool> m_analyzing { false };
};

} } // namespace Slic3r::GUI

#endif // slic3r_GUI_FilamentScanner_hpp_
