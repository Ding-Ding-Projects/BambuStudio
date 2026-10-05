#pragma once
#include "LocalSecurity.hpp"
namespace Slic3r::LocalSecurity {
// Worker-side decoder only. The host must supply bounded normalized luminance
// pixels through its isolated image path and enforce process memory/time limits.
// No image-file path, network request, logging, cache or output file is used here.
Secret decode_qr_luminance(unsigned width,unsigned height,const std::vector<unsigned char>& pixels);
}
