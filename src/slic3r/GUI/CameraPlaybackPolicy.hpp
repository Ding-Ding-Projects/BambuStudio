#pragma once
#include <algorithm>
namespace Slic3r { namespace GUI {
struct CameraPlaybackPolicy {
    static bool may_start(bool paused, bool eligible, bool visible, bool keepalive) { return !paused && eligible && (visible || keepalive); }
    static int retry_delay_seconds(int failures) { return std::min(60, 5 * std::max(1, std::min(failures, 12))); }
    static bool retryable(int code) { return code != 1 && code != -2 && code < 100; }
};
}}
