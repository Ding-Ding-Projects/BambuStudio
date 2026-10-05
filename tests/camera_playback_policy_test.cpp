#include "../src/slic3r/GUI/CameraPlaybackPolicy.hpp"
#include <cassert>
#include <iostream>
using Slic3r::GUI::CameraPlaybackPolicy;
int main() {
    assert(CameraPlaybackPolicy::may_start(false, true, false, true));
    assert(!CameraPlaybackPolicy::may_start(true, true, true, true));
    assert(!CameraPlaybackPolicy::may_start(true, true, false, true));
    assert(!CameraPlaybackPolicy::may_start(false, false, true, true));
    assert(!CameraPlaybackPolicy::may_start(false, true, false, false));
    assert(CameraPlaybackPolicy::may_start(false, true, true, false));
    assert(CameraPlaybackPolicy::retry_delay_seconds(0) == 5);
    assert(CameraPlaybackPolicy::retry_delay_seconds(1) == 5);
    assert(CameraPlaybackPolicy::retry_delay_seconds(2) == 10);
    assert(CameraPlaybackPolicy::retry_delay_seconds(12) == 60);
    assert(CameraPlaybackPolicy::retry_delay_seconds(1000000) == 60);
    assert(CameraPlaybackPolicy::retryable(-1));
    assert(CameraPlaybackPolicy::retryable(0));
    assert(CameraPlaybackPolicy::retryable(2));
    assert(!CameraPlaybackPolicy::retryable(-2));
    assert(!CameraPlaybackPolicy::retryable(1));
    assert(!CameraPlaybackPolicy::retryable(100));
    assert(!CameraPlaybackPolicy::retryable(401));
    assert(!CameraPlaybackPolicy::retryable(403));
    std::cout << "18 camera playback policy assertions passed\n";
}
