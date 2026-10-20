#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Version 1 personal vocabulary file contract, free of wxWidgets so it can be
// tested on its own. Validation covers the complete byte payload before any
// entry is used; a rejected file yields one reason code and nothing else. No
// diagnostic ever contains the input, a key, a value or a filename.
//
// The number of entries is not limited by a count. Every entry that passes the
// rules below is accepted, so the file size limit is the only thing that bounds
// how many entries a file can hold.
namespace Slic3r::GUI::PersonalVocabulary {
constexpr int         schema_version        = 1;
constexpr std::size_t max_file_bytes        = 256 * 1024; // UTF-8 bytes of the whole file
constexpr std::size_t max_depth             = 8;          // nested objects and arrays
constexpr std::size_t max_key_code_points   = 160;        // Unicode code points, not bytes
constexpr std::size_t max_value_code_points = 1000;       // Unicode code points, not bytes

enum class Rejection {
    None,
    Unreadable,         // the selected file or the saved copy could not be read
    TooLarge,           // more than max_file_bytes bytes
    Malformed,          // empty, not JSON, trailing content, comments, or invalid UTF-8
    TooDeep,            // objects and arrays nested deeper than max_depth
    DuplicateKey,       // one object names the same key twice
    UnsupportedVersion, // schemaVersion missing or not the integer 1
    UnexpectedField,    // the root is not exactly {"schemaVersion", "entries": {...}}
    NonStringValue,     // an entry value is not a string
    KeyTooLong,         // an entry key has more than max_key_code_points code points
    ValueTooLong,       // an entry value has more than max_value_code_points code points
    UnsafeText,         // blank, control or text-direction characters, or a reserved name
    CacheUnavailable,   // the file is valid but the private copy could not be written
};

// UTF-8 key and value pairs in key order.
using TextEntries = std::vector<std::pair<std::string, std::string>>;

// Validates the whole file. Returns Rejection::None and replaces result only
// when every rule passes; otherwise result is left exactly as it was.
Rejection validate(std::string_view bytes, TextEntries &result);

// Number of Unicode code points in valid UTF-8 text.
std::size_t code_points(std::string_view utf8);

// Stable ASCII name of a reason, for tests. Contains no file data.
const char *rejection_name(Rejection reason);
}
