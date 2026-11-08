#include "Base64Files.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <unistd.h>
#endif

namespace Slic3r { namespace GUI { namespace Base64Toolkit {

namespace fs = std::filesystem;

namespace {

std::uint64_t file_size_or_zero(const fs::path &path)
{
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    return ec ? 0 : std::uint64_t(size);
}

void remove_quietly(const fs::path &path)
{
    std::error_code ec;
    if (!path.empty()) fs::remove(path, ec);
}

// A temporary name beside the final one that is never an existing file.
fs::path partial_beside(const fs::path &final_path)
{
    return non_clashing(final_path.parent_path() / fs::u8path(final_path.filename().u8string() + ".partial"));
}

enum class Moved { Done, Taken, Failed };

// Moves the finished temporary file to its name without ever replacing a file
// that appeared there in the meantime.
Moved move_no_replace(const fs::path &from, const fs::path &to)
{
#ifdef _WIN32
    if (::MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_WRITE_THROUGH)) return Moved::Done;
    const DWORD error = ::GetLastError();
    return error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS ? Moved::Taken : Moved::Failed;
#else
    if (::link(from.c_str(), to.c_str()) == 0) {
        ::unlink(from.c_str());
        return Moved::Done;
    }
    if (errno == EEXIST) return Moved::Taken;
    // A file system without hard links: check, then rename.
    std::error_code ec;
    if (fs::exists(to, ec) || ec) return Moved::Taken;
    fs::rename(from, to, ec);
    return ec ? Moved::Failed : Moved::Done;
#endif
}

bool report(const Progress &progress, std::uint64_t done, std::uint64_t total)
{
    return !progress || progress(done, total);
}

std::size_t count_code_points(std::string_view text)
{
    std::size_t n = 0;
    for (const char c : text)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++n;
    return n;
}

// SHA-256 of the bytes a Base64 file decodes to (whitespace and line breaks allowed).
bool decoded_sha256(const fs::path &text_file, Alphabet alphabet, std::string &digest)
{
    std::ifstream in(text_file, std::ios::binary);
    if (!in) return false;
    DecodeOptions options;
    options.alphabet = alphabet;
    options.strict   = false;
    StreamDecoder decoder(options);
    Sha256        hash;
    std::string   chunk(kFileChunk, '\0');
    Bytes         out;
    while (in) {
        in.read(chunk.data(), std::streamsize(chunk.size()));
        const auto got = std::size_t(in.gcount());
        if (got == 0) break;
        out.clear();
        if (!decoder.feed(std::string_view(chunk.data(), got), out)) return false;
        hash.update(out.data(), out.size());
    }
    out.clear();
    if (!decoder.finish(out)) return false;
    hash.update(out.data(), out.size());
    digest = hex(hash.finish());
    return true;
}

std::string file_sha256(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    Sha256      hash;
    std::string chunk(kFileChunk, '\0');
    while (in) {
        in.read(chunk.data(), std::streamsize(chunk.size()));
        const auto got = std::size_t(in.gcount());
        if (got == 0) break;
        hash.update(reinterpret_cast<const std::uint8_t *>(chunk.data()), got);
    }
    return hex(hash.finish());
}

std::string lower_ascii(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}

} // namespace

FileResult encode_file(const fs::path &source, const EncodeOptions &options, const Progress &progress)
{
    FileResult result;
    result.source = source;
    std::ifstream in(source, std::ios::binary);
    if (!in) {
        result.problem = "unreadable";
        return result;
    }
    const std::uint64_t total = file_size_or_zero(source);
    if (total == 0) {
        result.problem = "empty";
        return result;
    }
    const fs::path wanted = encoded_path_for(source);
    if (wanted.empty()) {
        result.problem = "no-free-name";
        return result;
    }
    const fs::path partial = partial_beside(wanted);

    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        if (!out) {
            result.problem = "unwritable";
            return result;
        }
        StreamEncoder encoder(options);
        Sha256        hash;
        std::string   chunk(kFileChunk, '\0');
        while (in) {
            in.read(chunk.data(), std::streamsize(chunk.size()));
            const auto got = std::size_t(in.gcount());
            if (got == 0) break;
            const auto *bytes = reinterpret_cast<const std::uint8_t *>(chunk.data());
            hash.update(bytes, got);
            const std::string text = encoder.feed(bytes, got);
            out.write(text.data(), std::streamsize(text.size()));
            result.bytes_in += got;
            result.bytes_out += text.size();
            if (!report(progress, result.bytes_in, total)) {
                out.close();
                remove_quietly(partial);
                result.cancelled = true;
                result.problem   = "cancelled";
                return result;
            }
        }
#ifndef SLIC3R_BASE64_FILES_NEGATIVE_FIXTURE
        const std::string tail = encoder.finish();
        out.write(tail.data(), std::streamsize(tail.size()));
        result.bytes_out += tail.size();
#endif
        result.sha256 = hex(hash.finish());
        out.flush();
        if (!out) {
            out.close();
            remove_quietly(partial);
            result.problem = "write-failed";
            return result;
        }
    }

    // Verify before the file counts: decode what was written and compare.
    std::string check;
    if (!decoded_sha256(partial, options.alphabet, check) || check != result.sha256) {
        remove_quietly(partial);
        result.problem = "verification-failed";
        return result;
    }
    const std::string plain = source.filename().u8string() + ".base64.txt";
    for (int attempt = 0; attempt < 16; ++attempt) {
        // Asked again each time, in case another program took the name meanwhile.
        const fs::path final_path = encoded_path_for(source);
        if (final_path.empty()) break;
        const Moved moved = move_no_replace(partial, final_path);
        if (moved == Moved::Taken) continue;
        if (moved == Moved::Failed) break;
        result.output  = final_path;
        result.renamed = final_path.filename().u8string() != plain;
        result.ok      = true;
        return result;
    }
    remove_quietly(partial);
    result.problem = "rename-failed";
    return result;
}

FileResult decode_file(const fs::path &source, const DecodeOptions &options, const Progress &progress)
{
    FileResult result;
    result.source = source;
    std::ifstream in(source, std::ios::binary);
    if (!in) {
        result.problem = "unreadable";
        return result;
    }
    const std::uint64_t total = file_size_or_zero(source);

    // The first chunk decides where the Base64 starts: after a UTF-8 byte
    // order mark, and after the header of a data: URL.
    std::string first(kFileChunk, '\0');
    in.read(first.data(), std::streamsize(first.size()));
    first.resize(std::size_t(in.gcount()));
    std::size_t start = 0;
    if (first.compare(0, 3, "\xEF\xBB\xBF") == 0) start = 3;
    bool from_data_url = false;
    {
        std::size_t lead = start;
        while (lead < first.size() && std::isspace(static_cast<unsigned char>(first[lead]))) ++lead;
        if (first.size() - lead >= 5 && lower_ascii(first.substr(lead, 5)) == "data:") {
            const std::size_t comma = first.find(',', lead);
            if (comma == std::string::npos || comma - lead > 1024) {
                result.problem = "bad-data-url";
                return result;
            }
            const std::string meta = lower_ascii(first.substr(lead + 5, comma - lead - 5));
            if (meta.size() < 7 || meta.compare(meta.size() - 7, 7, ";base64") != 0) {
                result.problem = "data-url-not-base64";
                return result;
            }
            result.mime   = meta.substr(0, meta.find(';'));
            from_data_url = true;
            start         = comma + 1;
        }
    }
    // Offsets in the result count characters from the start of the file.
    const std::size_t header_characters = count_code_points(std::string_view(first.data(), start));

    const fs::path placeholder = source.parent_path() / fs::u8path(source.filename().u8string() + ".decoded");
    const fs::path partial     = partial_beside(placeholder);
    Bytes          head_bytes; // the first decoded bytes, to detect the type
    Sha256         hash;
    {
        std::ofstream out(partial, std::ios::binary | std::ios::trunc);
        if (!out) {
            result.problem = "unwritable";
            return result;
        }
        StreamDecoder decoder(options);
        Bytes         decoded;
        const auto    write = [&](const Bytes &bytes) {
            if (head_bytes.size() < 4096) {
                const std::size_t take = std::min<std::size_t>(bytes.size(), 4096 - head_bytes.size());
                head_bytes.insert(head_bytes.end(), bytes.begin(), bytes.begin() + std::ptrdiff_t(take));
            }
            hash.update(bytes.data(), bytes.size());
            out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
            result.bytes_out += bytes.size();
        };
        const auto fail = [&]() {
            out.close();
            remove_quietly(partial);
            result.decode_error    = decoder.error();
            result.error_offset    = header_characters + decoder.error_offset();
            result.error_character = decoder.error_character();
            result.problem         = error_name(decoder.error());
            return result;
        };
        const auto cancel = [&]() {
            out.close();
            remove_quietly(partial);
            result.cancelled = true;
            result.problem   = "cancelled";
            return result;
        };

        result.bytes_in = first.size();
        if (!decoder.feed(std::string_view(first.data() + start, first.size() - start), decoded)) return fail();
        write(decoded);
        if (!report(progress, result.bytes_in, total)) return cancel();
        std::string buffer(kFileChunk, '\0');
        while (in) {
            in.read(buffer.data(), std::streamsize(buffer.size()));
            const auto got = std::size_t(in.gcount());
            if (got == 0) break;
            result.bytes_in += got;
            decoded.clear();
            if (!decoder.feed(std::string_view(buffer.data(), got), decoded)) return fail();
            write(decoded);
            if (!report(progress, result.bytes_in, total)) return cancel();
        }
        decoded.clear();
        if (!decoder.finish(decoded)) return fail();
        write(decoded);
        out.flush();
        if (!out) {
            out.close();
            remove_quietly(partial);
            result.problem = "write-failed";
            return result;
        }
    }
    result.sha256 = hex(hash.finish());

    // Verify the written bytes before the file counts.
    if (file_sha256(partial) != result.sha256) {
        remove_quietly(partial);
        result.problem = "verification-failed";
        return result;
    }

    const DetectedType detected = detect_type(head_bytes);
    const std::string  url_mime = from_data_url ? result.mime : std::string();
    if (result.mime.empty()) result.mime = detected.mime;
    const fs::path wanted = restored_name_for(source, url_mime, detected);
    for (int attempt = 0; attempt < 16; ++attempt) {
        const fs::path final_path = non_clashing(wanted);
        if (final_path.empty()) break;
        const Moved moved = move_no_replace(partial, final_path);
        if (moved == Moved::Taken) continue;
        if (moved == Moved::Failed) break;
        result.output  = final_path;
        result.renamed = final_path != wanted;
        result.ok      = true;
        return result;
    }
    remove_quietly(partial);
    result.problem = "rename-failed";
    return result;
}

}}} // namespace Slic3r::GUI::Base64Toolkit
