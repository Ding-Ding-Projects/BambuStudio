#pragma once
#include "libslic3r/LocalSecurity/LocalSecurity.hpp"

namespace Slic3r::GUI::LocalSecurityUI {
struct PairingQr {
    unsigned modules = 0;
    std::vector<unsigned char> dark;
    static constexpr unsigned quiet_zone = 4;
    bool module(unsigned x, unsigned y) const;
    // Registration-only matrix; do not serialize, cache, log or capture it.
    static PairingQr encode(const LocalSecurity::Enrollment&);
};
}
