#pragma once
#include "Converter.hpp"
namespace Slic3r::LocalConverter {
// Worker-only, after installation of the verified PDF engine. The envelope is
// bounded UTF-8 JSON, never a filename or an argument for an external process.
Conversion pdf_request_transform(const std::string &adapter, const Bytes &envelope);
}
