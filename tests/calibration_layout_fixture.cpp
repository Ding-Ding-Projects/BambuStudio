#include "src/slic3r/GUI/CalibrationLayout.hpp"
#include <iostream>
#include <stdexcept>
using namespace Slic3r::GUI::CalibrationLayout;
void check(bool value) { if (!value) throw std::runtime_error("layout contract failed"); }
int main() {
    try {
        for (int scale : {100,125,150,200}) {
            for (int viewport : {640,800,1000,1200}) {
                const int pixels=viewport*scale/100, inset=16*scale/100;
                check(content_width(pixels,inset)==pixels-2*inset);
                check(content_width(pixels,0)==pixels);
            }
        }
        std::cout << "PASS measured viewport and four DPI scales\n";
        check(content_width(0,16)==0 && content_width(-1,16)==0);
        check(content_width(1,100)==1 && content_width(20,-1)==20);
        std::cout << "PASS hidden and narrow viewport bounds\n";
        check(needs_height_update(14,28) && needs_height_update(28,14));
        check(!needs_height_update(28,28) && !needs_height_update(14,0));
        std::cout << "PASS growing and shrinking content with stable no-op\n";
        ReflowState state; check(state.request()); check(!state.request());
        { ReflowPass pass(state); check(!state.request()); }
        check(state.request());
        { ReflowPass pass(state); check(!state.request()); }
        check(state.request());
        { ReflowPass pass(state); }
        std::cout << "PASS duplicate coalescing and non-recursive repeat passes\n";
        try { check(state.request()); ReflowPass pass(state); throw 1; } catch(int) {}
        check(state.request()); { ReflowPass pass(state); }
        std::cout << "PASS reflow state released on early exit\n5/5 cases passed\n";
        return 0;
    } catch(const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
