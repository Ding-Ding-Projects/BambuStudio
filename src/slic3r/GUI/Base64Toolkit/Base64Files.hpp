#ifndef slic3r_GUI_Base64Toolkit_Base64Files_hpp_
#define slic3r_GUI_Base64Toolkit_Base64Files_hpp_

// Encode and decode whole files beside the originals, streaming 1 MiB at a
// time, never overwriting anything, and verifying every written file before it
// counts as done. No wxWidgets here: tests/base64_files_test.cpp runs it on its
// own. The Base64 toolkit page runs these on a worker thread.

#include "Base64Codec.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace Slic3r { namespace GUI { namespace Base64Toolkit {

struct FileResult
{
    std::filesystem::path source;
    std::filesystem::path output;          // empty unless ok
    std::uint64_t         bytes_in  = 0;
    std::uint64_t         bytes_out = 0;
    bool                  ok        = false;
    bool                  cancelled = false;
    bool                  renamed   = false; // the plain name was taken, so a numbered one was used
    // Stable ASCII reason when !ok: unreadable, empty, no-free-name, unwritable,
    // write-failed, verification-failed, rename-failed, cancelled, bad-data-url,
    // data-url-not-base64, or a DecodeError name from error_name().
    std::string           problem;
    std::string           sha256;          // of the original bytes (the source when encoding, the result when decoding)
    std::string           mime;            // decoding: from a data: URL header or detected from the bytes
    DecodeError           decode_error    = DecodeError::None;
    std::size_t           error_offset    = 0; // characters from the start of the file, byte order mark and data: header included
    std::uint32_t         error_character = 0;
};

// Called with bytes done and the total; returning false cancels the job.
using Progress = std::function<bool(std::uint64_t done, std::uint64_t total)>;

constexpr std::size_t kFileChunk = std::size_t(1) << 20;

// report.pdf -> report.pdf.base64.txt beside it (numbered when taken). The
// written text is decoded again and its SHA-256 compared with the source before
// the file appears under its name. An empty file is refused: there is nothing
// to encode, and decoding nothing is an error.
FileResult encode_file(const std::filesystem::path &source, const EncodeOptions &options, const Progress &progress = {});

// report.pdf.base64.txt (or .b64, or a data: URL file) -> report.pdf beside it
// (numbered when taken; an extension is added from the data: URL or the bytes
// when the name has none). A leading UTF-8 byte order mark is skipped. A file
// that fails to decode, or is cancelled, leaves nothing behind.
FileResult decode_file(const std::filesystem::path &source, const DecodeOptions &options, const Progress &progress = {});

}}} // namespace Slic3r::GUI::Base64Toolkit

#endif // slic3r_GUI_Base64Toolkit_Base64Files_hpp_
