#include "src/slic3r/GUI/PreferencesGeometry.hpp"
#include <iostream>
#include <stdexcept>
using namespace Slic3r::GUI::PreferencesGeometry;
static void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int main()
{
    try {
        require(restored_dimension("1120",780)==1120, "chosen width restored");
        for (const auto *invalid : {"", "0", "-1", "1x", "2147483648", "9999999999999999999", "16385"})
            require(restored_dimension(invalid,780)==780, "corrupt dimension falls back before scaling");
        const auto chosen = bounded({200,100,1120,820},{0,0,1920,1040},{640,480});
        require(chosen.width==1120 && chosen.height==820, "user size survives layout");
        const auto dpi = bounded({2100,100,1680,1230},{1920,0,2560,1400},{960,720});
        require(dpi.width==1680 && dpi.height==1230, "DPI suggestion preserves scaled user size");
        const auto removed = bounded({3200,1800,1600,1200},{0,0,1280,680},{640,480});
        require(removed.x==0 && removed.y==0 && removed.width==1280 && removed.height==680, "removed monitor and taskbar clamp");
        const auto negative = bounded({-2100,-200,800,600},{-1920,-100,1920,1000},{640,480});
        require(negative.x==-1920 && negative.y==-100, "negative monitor coordinates remain valid");
        const auto small = bounded({0,0,100,100},{0,0,480,320},{640,480});
        require(small.width==480 && small.height==320, "minimum yields to smaller work area");
        const auto minimum_size = bounded({100,100,200,200},{0,0,1920,1040},{640,480});
        require(minimum_size.width==640 && minimum_size.height==480, "normal minimum retained");
        std::cout << "PASS 7 production geometry scenarios\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
