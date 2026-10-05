#pragma once
#include "Converter.hpp"
#include <map>

namespace Slic3r::LocalConverter {
enum class PdfOperation { Inspect, Split, Merge, Extract, Reorder, Rotate, Metadata };
struct PdfOptions {
    // One-based indices. Extract/reorder require a nonempty explicit selection.
    std::vector<std::size_t> pages;
    int rotation = 0; // Absolute clockwise orientation: 0, 90, 180, or 270.
    std::map<std::string, std::string> metadata;
};
struct PdfResult {
    Outcome outcome = Outcome::Failed;
    std::string code;
    std::vector<Bytes> outputs;
    std::size_t page_count = 0;
    std::vector<int> rotations;
    std::map<std::string, std::string> metadata;
};
// Worker-only: module must already have passed the compiled package-hash checks
// and been loaded from the exact installed bundle with restricted DLL search.
// Its lifetime must cover every call. No DLL loading or path discovery occurs here.
bool pdf_install_engine(void *verified_module);
PdfResult pdf_transform(PdfOperation, const std::vector<Bytes> &, const PdfOptions & = {});
}
