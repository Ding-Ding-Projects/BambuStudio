// Standalone assertions for src/slic3r/GUI/Base64Toolkit/Base64Files.cpp, which
// encodes and decodes whole files beside the originals. Compiled and run by
// tests/base64_files.test.mjs.
#include "src/slic3r/GUI/Base64Toolkit/Base64Codec.cpp"
#include "src/slic3r/GUI/Base64Toolkit/Base64Files.cpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>
#include <set>
#include <sstream>

using namespace Slic3r::GUI::Base64Toolkit;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static fs::path fresh_dir(const char *name)
{
    const fs::path dir = fs::temp_directory_path() / (std::string("b64-files-") + name + "-" + std::to_string(std::random_device{}()));
    fs::create_directories(dir);
    return dir;
}

static void write_file(const fs::path &path, const std::string &content)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(content.data(), std::streamsize(content.size()));
}

static std::string read_file(const fs::path &path)
{
    std::ifstream      in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

static std::set<std::string> listing(const fs::path &dir)
{
    std::set<std::string> names;
    for (const auto &entry : fs::directory_iterator(dir)) names.insert(entry.path().filename().u8string());
    return names;
}

static bool any_partial(const fs::path &dir)
{
    for (const auto &name : listing(dir))
        if (name.find(".partial") != std::string::npos) return true;
    return false;
}

static std::string random_bytes(std::size_t size, unsigned seed)
{
    std::mt19937 rng(seed);
    std::string  bytes(size, '\0');
    for (auto &c : bytes) c = char(rng() & 0xFF);
    return bytes;
}

static Bytes as_bytes(const std::string &s) { return Bytes(s.begin(), s.end()); }

// Larger than two chunks with a remainder, so the encoder's tail matters.
static const std::size_t kBig = 2 * kFileChunk + kFileChunk / 2 + 1;

static void encode_round_trip_beside_the_original()
{
    const fs::path    dir  = fresh_dir("round");
    const fs::path    blob = dir / "blob.bin";
    const std::string data = random_bytes(kBig, 7);
    write_file(blob, data);

    EncodeOptions mime;
    mime.line_length = 76;
    std::uint64_t last_done = 0, calls = 0, seen_total = 0;
    bool          increasing = true;
    const FileResult encoded = encode_file(blob, mime, [&](std::uint64_t done, std::uint64_t total) {
        increasing = increasing && done > last_done;
        last_done  = done;
        seen_total = total;
        ++calls;
        return true;
    });
    CHECK(encoded.ok);
    CHECK(!encoded.renamed);
    CHECK(encoded.output.filename() == "blob.bin.base64.txt");
    CHECK(encoded.bytes_in == data.size());
    CHECK(encoded.sha256 == sha256_hex(reinterpret_cast<const std::uint8_t *>(data.data()), data.size()));
    CHECK(read_file(encoded.output) == encode(as_bytes(data), mime));
    CHECK(encoded.bytes_out == read_file(encoded.output).size());
    CHECK(calls == 3 && increasing && last_done == data.size() && seen_total == data.size());
    CHECK(!any_partial(dir));
    CHECK(read_file(blob) == data); // the original is untouched

    // A second run never replaces the first: the original stem is numbered.
    const FileResult again = encode_file(blob, mime);
    CHECK(again.ok && again.renamed);
    CHECK(again.output.filename() == "blob (1).bin.base64.txt");
    CHECK(read_file(again.output) == read_file(encoded.output));

    // Decoding beside an existing blob.bin numbers the result.
    const FileResult decoded = decode_file(encoded.output, [] { DecodeOptions o; o.strict = false; return o; }());
    CHECK(decoded.ok && decoded.renamed);
    CHECK(decoded.output.filename() == "blob (1).bin");
    CHECK(read_file(decoded.output) == data);
    CHECK(decoded.sha256 == encoded.sha256);
    CHECK(decoded.bytes_out == data.size());
    CHECK(!any_partial(dir));
    CHECK(listing(dir) == (std::set<std::string>{"blob.bin", "blob.bin.base64.txt", "blob (1).bin.base64.txt", "blob (1).bin"}));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

static void url_safe_without_padding()
{
    const fs::path    dir  = fresh_dir("url");
    const fs::path    file = dir / "tiny.dat";
    const std::string data("\xfb\xff\xfe\x01", 4);
    write_file(file, data);
    EncodeOptions url;
    url.alphabet = Alphabet::UrlSafe;
    url.padding  = false;
    const FileResult encoded = encode_file(file, url);
    CHECK(encoded.ok);
    CHECK(read_file(encoded.output) == "-__-AQ");
    fs::remove(file);
    DecodeOptions strict;
    strict.alphabet        = Alphabet::UrlSafe;
    strict.require_padding = false;
    const FileResult decoded = decode_file(encoded.output, strict);
    CHECK(decoded.ok && !decoded.renamed);
    CHECK(decoded.output.filename() == "tiny.dat");
    CHECK(read_file(decoded.output) == data);
    std::error_code ec;
    fs::remove_all(dir, ec);
}

static void cancelling_leaves_nothing()
{
    const fs::path dir  = fresh_dir("cancel");
    const fs::path blob = dir / "blob.bin";
    write_file(blob, random_bytes(kBig, 11));
    const FileResult encoded = encode_file(blob, EncodeOptions{}, [](std::uint64_t, std::uint64_t) { return false; });
    CHECK(!encoded.ok && encoded.cancelled && encoded.problem == "cancelled");
    CHECK(encoded.output.empty());
    CHECK(listing(dir) == (std::set<std::string>{"blob.bin"}));

    const fs::path text = dir / "blob.bin.b64";
    write_file(text, encode(as_bytes(random_bytes(kBig, 12)), EncodeOptions{}));
    int calls = 0;
    const FileResult decoded = decode_file(text, DecodeOptions{}, [&](std::uint64_t, std::uint64_t) { return ++calls < 2; });
    CHECK(!decoded.ok && decoded.cancelled);
    CHECK(calls == 2);
    CHECK(listing(dir) == (std::set<std::string>{"blob.bin", "blob.bin.b64"}));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

static void bad_character_far_into_the_file()
{
    const fs::path dir  = fresh_dir("bad");
    const fs::path text = dir / "broken.bin.base64.txt";
    std::string    coded = encode(as_bytes(random_bytes(kFileChunk + kFileChunk / 2, 13)), EncodeOptions{});
    const std::size_t where = kFileChunk + 4321; // in the second chunk
    coded[where]            = '!';
    write_file(text, coded);
    DecodeOptions lenient;
    lenient.strict = false;
    const FileResult decoded = decode_file(text, lenient);
    CHECK(!decoded.ok && !decoded.cancelled);
    CHECK(decoded.decode_error == DecodeError::InvalidCharacter);
    CHECK(decoded.error_offset == where);
    CHECK(decoded.error_character == '!');
    CHECK(decoded.problem == error_name(DecodeError::InvalidCharacter));
    CHECK(listing(dir) == (std::set<std::string>{"broken.bin.base64.txt"}));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

static void data_urls_and_byte_order_marks()
{
    const fs::path dir = fresh_dir("url");
    const std::string png("\x89PNG\r\n\x1a\n\0\0\0\rIHDR", 16);
    write_file(dir / "pic.txt", "  data:image/png;base64," + encode(as_bytes(png), EncodeOptions{}) + "\n");
    DecodeOptions lenient;
    lenient.strict = false;
    const FileResult picture = decode_file(dir / "pic.txt", lenient);
    CHECK(picture.ok);
    CHECK(picture.output.filename() == "pic.png");
    CHECK(picture.mime == "image/png");
    CHECK(read_file(picture.output) == png);

    // The header counts toward the offset of a bad character.
    write_file(dir / "bad.txt", "data:text/plain;base64,aGV!");
    const FileResult bad = decode_file(dir / "bad.txt", lenient);
    CHECK(!bad.ok && bad.decode_error == DecodeError::InvalidCharacter);
    CHECK(bad.error_offset == 26);

    write_file(dir / "plain.txt", "data:text/plain,hello");
    CHECK(decode_file(dir / "plain.txt", lenient).problem == "data-url-not-base64");
    write_file(dir / "nocomma.txt", "data:text/plain;base64");
    CHECK(decode_file(dir / "nocomma.txt", lenient).problem == "bad-data-url");

    // Notepad's byte order mark is skipped and counted as one character.
    write_file(dir / "note.txt.b64", "\xEF\xBB\xBF" "aGVsbG8=\r\n");
    const FileResult note = decode_file(dir / "note.txt.b64", lenient);
    CHECK(note.ok && note.output.filename() == "note.txt");
    CHECK(read_file(note.output) == "hello");
    write_file(dir / "strict.b64", "\xEF\xBB\xBF" "aGVs!G8=");
    const FileResult strict = decode_file(dir / "strict.b64", DecodeOptions{});
    CHECK(!strict.ok && strict.decode_error == DecodeError::InvalidCharacter && strict.error_offset == 5);
    CHECK(!any_partial(dir));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

static void refusals()
{
    const fs::path dir = fresh_dir("refuse");
    write_file(dir / "empty.bin", "");
    const FileResult empty = encode_file(dir / "empty.bin", EncodeOptions{});
    CHECK(!empty.ok && empty.problem == "empty");
    write_file(dir / "empty.b64", "");
    const FileResult nothing = decode_file(dir / "empty.b64", DecodeOptions{});
    CHECK(!nothing.ok && nothing.decode_error == DecodeError::Empty);
    CHECK(encode_file(dir / "missing.bin", EncodeOptions{}).problem == "unreadable");
    CHECK(decode_file(dir / "missing.b64", DecodeOptions{}).problem == "unreadable");
    CHECK(listing(dir) == (std::set<std::string>{"empty.bin", "empty.b64"}));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

static void names_outside_ascii()
{
#if defined(_WIN32) && defined(__GLIBCXX__)
    // MinGW's libstdc++ opens file streams through narrow paths, so a name
    // outside the code page cannot be created there at all. MSVC, which builds
    // the app, and every POSIX library open them through the real name.
    std::printf("names outside ASCII: skipped with MinGW's libstdc++\n");
    return;
#endif
    const fs::path    dir  = fresh_dir("names");
    const fs::path    file = dir / fs::u8path("\xE6\xA8\xA1\xE5\x9E\x8B \xC3\xBC.3mf"); // "model u-umlaut", in Chinese and German
    const std::string data = random_bytes(1000, 17);
    write_file(file, data);
    const FileResult encoded = encode_file(file, EncodeOptions{});
    CHECK(encoded.ok);
    CHECK(encoded.output.filename().u8string() == "\xE6\xA8\xA1\xE5\x9E\x8B \xC3\xBC.3mf.base64.txt");
    fs::remove(file);
    const FileResult decoded = decode_file(encoded.output, DecodeOptions{});
    CHECK(decoded.ok && decoded.output.filename().u8string() == "\xE6\xA8\xA1\xE5\x9E\x8B \xC3\xBC.3mf");
    CHECK(read_file(decoded.output) == data);
    std::error_code ec;
    fs::remove_all(dir, ec);
}

int main()
{
    encode_round_trip_beside_the_original();
    url_safe_without_padding();
    cancelling_leaves_nothing();
    bad_character_far_into_the_file();
    data_urls_and_byte_order_marks();
    refusals();
    names_outside_ascii();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("base64 files: all checks passed\n");
    return 0;
}
