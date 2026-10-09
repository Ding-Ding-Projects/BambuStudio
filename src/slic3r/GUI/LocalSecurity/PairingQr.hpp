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
    // Public, non-secret text such as the LAN model drop site address, with the same encoder,
    // error correction and 2,048-byte bound. Throws LocalSecurity::Failure on an empty or longer text.
    static PairingQr encode_text(const std::string& text);
};
}
