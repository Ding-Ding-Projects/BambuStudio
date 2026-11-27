#pragma once

#include <chrono>
#include <optional>
#include <memory>
#include <cstdint>
#include <wx/image.h>
#include <wx/panel.h>
#include <wx/timer.h>

namespace Slic3r::GUI {
class Plater;
struct SliceToolpathSnapshot;
enum class SliceMotionKind { Ordinary, DialogRebuild, Required };

struct SliceMotionKey {
    size_t model, plate, epoch;
    int index;
    uint64_t generation;
    bool operator==(const SliceMotionKey &other) const;
};

class SliceMotion {
public:
    static std::optional<SliceMotionKey> identify(Plater &plater, int index);
    static wxImage geometry(Plater &plater, const SliceMotionKey &key);
    static void request_dialog(Plater &plater);
    static void begin(Plater &plater, int plate);
    static void stop();
    static void finish(Plater &plater, int index);
    static std::shared_ptr<const SliceToolpathSnapshot> paths(Plater &plater);
    static bool active(Plater &plater);
    static SliceMotionKind kind();
    static const wxImage &image();
    static int plate_index();
    static double seconds();
};

class SliceMotionPanel : public wxPanel {
public:
    explicit SliceMotionPanel(wxWindow *parent);
    void show_required(Plater &plater, int index);
    void show_active();
    void clear();
private:
    void paint(wxPaintEvent &);
    void tick();
    wxTimer m_timer;
    std::optional<SliceMotionKey> m_required;
    wxBitmap m_image;
    std::chrono::steady_clock::time_point m_start;
};
}
