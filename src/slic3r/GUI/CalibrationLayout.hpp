#ifndef slic3r_GUI_CalibrationLayout_hpp_
#define slic3r_GUI_CalibrationLayout_hpp_

#include <algorithm>

namespace Slic3r { namespace GUI { namespace CalibrationLayout {

// All inputs are measured pixels from the owning viewport, never a main-window
// DIP assumption. Zero means the owner is not ready for measurement yet.
inline int content_width(int viewport, int inset)
{
    if (viewport <= 0) return 0;
    const int bounded_inset = std::clamp(inset, 0, viewport / 2);
    return std::max(1, viewport - 2 * bounded_inset);
}

inline bool needs_height_update(int current, int measured)
{
    return measured > 0 && current != measured;
}

class ReflowState {
public:
    bool request() { if (pending || running) return false; pending = true; return true; }
    void begin() { pending = false; running = true; }
    void finish() { running = false; }
private:
    bool pending = false, running = false;
};

struct ReflowPass {
    ReflowState &state;
    explicit ReflowPass(ReflowState &value) : state(value) { state.begin(); }
    ~ReflowPass() { state.finish(); }
};

}}} // namespace Slic3r::GUI::CalibrationLayout
#endif
