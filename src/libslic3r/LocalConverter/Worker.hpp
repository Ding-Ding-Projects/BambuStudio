#pragma once
#include "Converter.hpp"

namespace Slic3r::LocalConverter {
// Every request is re-attested, receives an AppContainer with zero capabilities,
// and runs in a kill-on-close job limited to one process, 256 MiB and 30 seconds.
// The worker receives bytes over inherited anonymous pipes, never file paths.
Conversion isolated_transform(const PackageProof &, const std::string &, const Bytes &, const std::atomic<bool> &);
} // namespace Slic3r::LocalConverter
