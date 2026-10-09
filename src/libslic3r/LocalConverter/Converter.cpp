#include "Converter.hpp"
#include "ArchiveAdapter.hpp"
#include "Worker.hpp"
#ifdef LOCAL_CONVERTER_WITH_PDF
#include "PdfRequest.hpp"
#include "PdfPackage.hpp"
#endif
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <set>
#include <stdexcept>
#include <system_error>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

// Marks catalogue sources for xgettext. The library keeps fixed English; the
// application translates it at display time, so the worker needs no catalogue.
#ifndef L
#define L(s) s
#endif

namespace Slic3r::LocalConverter {
namespace {
namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;
constexpr char hex[] = "0123456789abcdef";
constexpr char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
bool begins(const Bytes &b, const char *s, std::size_t n) { return b.size() >= n && std::memcmp(b.data(), s, n) == 0; }
Bytes bytes(const std::string &s) { return {s.begin(), s.end()}; }
std::string text(const Bytes &b) { return {b.begin(), b.end()}; }
[[noreturn]] void reject(const char *code) { throw std::runtime_error(code); }
void check(bool value, const char *code) { if (!value) reject(code); }
Bytes read_file(const fs::path &path, std::uint64_t limit)
{
    check(fs::is_regular_file(fs::symlink_status(path)), "source_not_regular");
    const auto size = fs::file_size(path);
    check(size <= limit, "input_limit");
    std::ifstream in(path, std::ios::binary);
    check(bool(in), "source_unreadable");
    Bytes result(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char *>(result.data()), static_cast<std::streamsize>(size));
    check(in.gcount() == static_cast<std::streamsize>(size) && in.peek() == std::char_traits<char>::eof(), "source_changed");
    return result;
}
Json parse_json(const Bytes &data)
{
    std::size_t items = 0;
    std::vector<std::set<std::string>> keys;
    auto cb = [&](int depth, Json::parse_event_t event, Json &value) {
        check(depth <= static_cast<int>(Limits::depth), "structure_depth_limit");
        check(++items <= Limits::items * 4, "structure_item_limit");
        if (event == Json::parse_event_t::object_start) keys.emplace_back();
        if (event == Json::parse_event_t::key) check(keys.back().insert(value.get<std::string>()).second, "duplicate_json_key");
        if (event == Json::parse_event_t::object_end) keys.pop_back();
        return true;
    };
    return Json::parse(data.begin(), data.end(), cb);
}
std::string format_json(const Bytes &source)
{
    // The parser validates structure, duplicate keys and bounds; formatting
    // preserves every literal byte, including arbitrary-precision numbers and
    // escape spelling, instead of reserializing through binary floating point.
    std::string out;std::size_t depth=0;bool quoted=false,escaped=false;unsigned char previous=0;
    const auto whitespace=[](unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n';};
    auto line=[&]{out+='\n';out.append(depth*2,' ');};
    for(std::size_t i=0;i<source.size();++i){
        const auto c=source[i];
        if(quoted){out+=static_cast<char>(c);if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;continue;}
        if(whitespace(c))continue;
        if(c=='"'){quoted=true;out+='"';}
        else if(c=='{'||c=='['){out+=static_cast<char>(c);++depth;std::size_t next=i+1;while(next<source.size()&&whitespace(source[next]))++next;if(next<source.size()&&source[next]!='}'&&source[next]!=']')line();}
        else if(c=='}'||c==']'){--depth;if(previous!='{'&&previous!='[')line();out+=static_cast<char>(c);}
        else if(c==','){out+=',';line();}
        else if(c==':')out+=": ";
        else out+=static_cast<char>(c);
        previous=c;
    }
    out+='\n';return out;
}
unsigned unhex(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    reject("invalid_hex");
}
std::string base64(const Bytes &data)
{
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < data.size(); i += 3) {
        const unsigned n = (unsigned(data[i]) << 16) | (i + 1 < data.size() ? unsigned(data[i + 1]) << 8 : 0) |
                           (i + 2 < data.size() ? data[i + 2] : 0);
        out += b64[n >> 18]; out += b64[(n >> 12) & 63];
        out += i + 1 < data.size() ? b64[(n >> 6) & 63] : '=';
        out += i + 2 < data.size() ? b64[n & 63] : '=';
    }
    return out;
}
Bytes decode_base64(const Bytes &source)
{
    check(source.size() % 4 == 0, "invalid_base64_length");
    Bytes out;
    for (std::size_t i = 0; i < source.size(); i += 4) {
        unsigned n = 0;
        for (unsigned j = 0; j != 4; ++j) {
            const char c = static_cast<char>(source[i + j]);
            const char *found = c == 0 ? nullptr : std::strchr(b64, c);
            if (c == '=') {
                check(i + 4 == source.size() && j >= 2, "invalid_base64_padding");
                check(j != 2 || source[i + 3] == '=', "invalid_base64_padding");
                n <<= 6;
            } else {
                check(found != nullptr, "invalid_base64_character");
                n = (n << 6) | static_cast<unsigned>(found - b64);
            }
        }
        out.push_back(static_cast<unsigned char>(n >> 16));
        if (source[i + 2] != '=') out.push_back(static_cast<unsigned char>(n >> 8));
        if (source[i + 3] != '=') out.push_back(static_cast<unsigned char>(n));
    }
    check(base64(out) == text(source), "noncanonical_base64");
    return out;
}
using Table = std::vector<std::vector<std::string>>;
Table parse_table(const Bytes &data, char separator)
{
    check(valid_utf8(data), "invalid_utf8");
    Table rows;
    std::vector<std::string> row;
    std::string cell;
    bool quoted = false, closed = false;
    std::size_t count = 0;
    auto field = [&] { check(++count <= Limits::items, "table_item_limit"); row.push_back(std::move(cell)); cell.clear(); closed = false; };
    auto record = [&] { field(); rows.push_back(std::move(row)); row.clear(); };
    for (std::size_t i = 0; i < data.size(); ++i) {
        char c = static_cast<char>(data[i]);
        if (quoted) {
            if (c == '"') {
                if (i + 1 < data.size() && data[i + 1] == '"') { cell += '"'; ++i; }
                else { quoted = false; closed = true; }
            } else cell += c;
        } else if (c == separator) field();
        else if (c == '\r' || c == '\n') {
            if (c == '\r') { check(i + 1 < data.size() && data[i + 1] == '\n', "invalid_table_line_ending"); ++i; }
            record();
        } else if (c == '"') { check(cell.empty() && !closed, "invalid_table_quote"); quoted = true; }
        else { check(!closed, "data_after_table_quote"); cell += c; }
    }
    check(!quoted, "unterminated_table_quote");
    if (!cell.empty() || !row.empty() || closed || (!data.empty() && data.back() == separator)) record();
    if (!rows.empty()) for (const auto &r : rows) check(r.size() == rows.front().size(), "ragged_table");
    return rows;
}
std::string table_text(const Table &rows, char sep)
{
    std::string out;
    for (const auto &row : rows) {
        for (std::size_t i = 0; i < row.size(); ++i) {
            if (i) out += sep;
            out += '"';
            for (const char c : row[i]) { out += c; if (c == '"') out += '"'; }
            out += '"';
        }
        out += "\r\n";
    }
    return out;
}
std::uint32_t le32(const Bytes &b, std::size_t n) { check(n + 4 <= b.size(), "truncated_image"); return b[n] | (std::uint32_t(b[n + 1]) << 8) | (std::uint32_t(b[n + 2]) << 16) | (std::uint32_t(b[n + 3]) << 24); }
void put32(Bytes &b, std::size_t n, std::uint32_t v) { for (unsigned i = 0; i != 4; ++i) b[n + i] = static_cast<unsigned char>(v >> (i * 8)); }
struct Image { std::uint32_t width = 0, height = 0; Bytes rgb; };
Image parse_bmp(const Bytes &b)
{
    check(begins(b, "BM", 2) && b.size() >= 54, "invalid_bmp");
    check(le32(b, 2) == b.size() && le32(b, 10) == 54 && le32(b, 14) == 40, "unsupported_bmp_header");
    check(b[26] == 1 && b[27] == 0 && b[28] == 24 && b[29] == 0 && le32(b, 30) == 0, "unsupported_bmp_encoding");
    Image img{le32(b, 18), le32(b, 22), {}};
    check(img.width && img.height && std::uint64_t(img.width) * img.height <= Limits::pixels, "image_pixel_limit");
    const auto stride = (std::uint64_t(img.width) * 3 + 3) & ~std::uint64_t(3);
    check(54 + stride * img.height == b.size(), "invalid_bmp_length");
    img.rgb.resize(std::size_t(img.width) * img.height * 3);
    for (std::size_t y = 0; y < img.height; ++y) for (std::size_t x = 0; x < img.width; ++x)
        for (std::size_t c = 0; c < 3; ++c) img.rgb[(y * img.width + x) * 3 + c] = b[54 + (img.height - 1 - y) * stride + x * 3 + 2 - c];
    return img;
}
Image parse_ppm(const Bytes &b)
{
    check(begins(b, "P6", 2), "invalid_ppm");
    std::size_t p = 2;
    auto number = [&]() {
        while (p < b.size() && (b[p] == ' ' || b[p] == '\t' || b[p] == '\n' || b[p] == '\r')) ++p;
        std::uint64_t n = 0; const auto start = p;
        while (p < b.size() && b[p] >= '0' && b[p] <= '9') { n = n * 10 + b[p++] - '0'; check(n <= Limits::pixels, "image_pixel_limit"); }
        check(p != start, "unsupported_ppm_header"); return static_cast<std::uint32_t>(n);
    };
    Image img{number(), number(), {}};
    check(number() == 255, "unsupported_ppm_depth");
    check(p < b.size() && (b[p] == ' ' || b[p] == '\n' || b[p] == '\r' || b[p] == '\t'), "invalid_ppm_separator");
    if (b[p++] == '\r' && p < b.size() && b[p] == '\n') ++p;
    check(img.width && img.height && std::uint64_t(img.width) * img.height <= Limits::pixels, "image_pixel_limit");
    check(b.size() - p == std::uint64_t(img.width) * img.height * 3, "invalid_ppm_length");
    img.rgb.assign(b.begin() + p, b.end()); return img;
}
Bytes bmp(const Image &img)
{
    const std::size_t stride = (std::size_t(img.width) * 3 + 3) & ~std::size_t(3);
    Bytes b(54 + stride * img.height, 0); b[0] = 'B'; b[1] = 'M';
    put32(b, 2, static_cast<std::uint32_t>(b.size())); put32(b, 10, 54); put32(b, 14, 40);
    put32(b, 18, img.width); put32(b, 22, img.height); b[26] = 1; b[28] = 24;
    put32(b, 34, static_cast<std::uint32_t>(stride * img.height));
    for (std::size_t y = 0; y < img.height; ++y) for (std::size_t x = 0; x < img.width; ++x)
        for (std::size_t c = 0; c < 3; ++c) b[54 + (img.height - 1 - y) * stride + x * 3 + 2 - c] = img.rgb[(y * img.width + x) * 3 + c];
    return b;
}
Bytes ppm(const Image &img)
{
    auto b = bytes("P6\n" + std::to_string(img.width) + " " + std::to_string(img.height) + "\n255\n");
    b.insert(b.end(), img.rgb.begin(), img.rgb.end()); return b;
}
bool same_image(const Image &a, const Image &b) { return a.width == b.width && a.height == b.height && a.rgb == b.rgb; }
std::int64_t modified(const fs::path &p) { return fs::last_write_time(p).time_since_epoch().count(); }
fs::path record_path(const fs::path &root, std::uint64_t id) { return root / (std::to_string(id) + ".json"); }
void replace_record(const fs::path &path, const std::string &value)
{
    const auto tmp = fs::path(path.wstring() + L".next");
    std::error_code ec; fs::remove(tmp, ec);
    std::string code;
    check(atomic_create(tmp, bytes(value), code), "queue_write_failed");
#ifdef _WIN32
    if (!MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { fs::remove(tmp, ec); reject("queue_publish_failed"); }
#else
    fs::rename(tmp, path);
    int fd = ::open(path.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
    if (fd >= 0) { ::fsync(fd); ::close(fd); }
#endif
}
}

bool valid_utf8(const Bytes &b)
{
    for (std::size_t p = 0; p < b.size();) {
        const unsigned c = b[p++];
        if (c < 0x80) { if (c == 0 || (c < 32 && c != 9 && c != 10 && c != 13)) return false; continue; }
        unsigned n = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : 99;
        if (n == 99 || b.size() - p < n) return false;
        unsigned cp = c & ((1u << (6 - n)) - 1);
        for (unsigned j = 0; j < n; ++j) { unsigned d = b[p++]; if ((d & 0xc0) != 0x80) return false; cp = (cp << 6) | (d & 63); }
        if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) || (n == 3 && cp < 0x10000) || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) return false;
    }
    return true;
}
Kind detect(const Bytes &b)
{
    if (begins(b, "%PDF-", 5)) return Kind::Pdf;
    if (begins(b, "\x89PNG\r\n\x1a\n", 8)) return Kind::Png;
    if (begins(b, "\xff\xd8\xff", 3)) return Kind::Jpeg;
    if (begins(b, "BM", 2)) return Kind::Bmp;
    if (begins(b, "P6", 2)) return Kind::Ppm;
    if (begins(b, "PK\003\004", 4) || begins(b, "PK\005\006", 4)) return Kind::Zip;
    if (b.size() >= 12 && begins(b, "RIFF", 4) && std::memcmp(b.data() + 8, "WAVE", 4) == 0) return Kind::Wave;
    if (b.size() >= 12 && std::memcmp(b.data() + 4, "ftyp", 4) == 0) return Kind::Mp4;
    if (!valid_utf8(b)) return Kind::Binary;
    try { parse_json(b); return Kind::Json; } catch (...) { return Kind::Utf8; }
}
const char *category_name(Category c)
{
    static const char *names[]{L("Documents/PDF"), L("Images"), L("Audio"), L("Video"), L("Archives"), L("Structured Data/Spreadsheets"), L("Code/Text"), L("Binary Encodings")};
    return names[static_cast<unsigned>(c)];
}
const char *kind_name(Kind k)
{
    static const char *names[]{L("Binary"), L("UTF-8 text"), L("JSON"), L("PDF"), L("PNG"), L("JPEG"), L("BMP"), L("PPM"), L("ZIP"), L("WAVE"), L("MP4")};
    return names[static_cast<unsigned>(k)];
}
bool verify_package(const PackageProof &p, std::string &reason)
{
    reason = L("Bundled converter worker and SHA-256 package proof are required.");
    try {
        if (p.expected_sha256.size() != 64 || p.worker.filename() != "BambuStudio_converter_worker.exe" ||
            !fs::is_regular_file(fs::symlink_status(p.worker)) || fs::canonical(p.worker.parent_path()) != fs::canonical(p.installed_directory)) return false;
#ifdef _WIN32
        BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
        if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return false;
        if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) < 0) { BCryptCloseAlgorithmProvider(alg, 0); return false; }
        std::ifstream in(p.worker, std::ios::binary);
        std::array<unsigned char, 65536> buffer{}; bool ok = bool(in);
        while (in) { in.read(reinterpret_cast<char *>(buffer.data()), buffer.size()); if (in.gcount() && BCryptHashData(hash, buffer.data(), static_cast<ULONG>(in.gcount()), 0) < 0) { ok = false; break; } }
        std::array<unsigned char, 32> digest{};
        ok = ok && in.eof() && BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0;
        BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg, 0);
        std::string actual; for (auto v : digest) { actual += hex[v >> 4]; actual += hex[v & 15]; }
        if (!ok || actual != p.expected_sha256) { reason = L("Bundled worker SHA-256 does not match its installed package proof."); return false; }
        reason.clear(); return true;
#else
        reason = L("This worker containment implementation requires Windows AppContainer."); return false;
#endif
    } catch (...) { reason = L("Bundled worker package proof cannot be read."); return false; }
}
std::vector<Adapter> catalog(const PackageProof &p)
{
    // A diagnostic from the worker is kept only as a stable machine code.
    const auto code_or_invalid = [](const std::string &code) {
        return !code.empty() && code.size() < 96 && code.find_first_not_of("abcdefghijklmnopqrstuvwxyz_0123456789") == std::string::npos ? code : std::string("invalid_worker_diagnostic");
    };
    std::string unavailable, unavailable_detail; bool proven = verify_package(p, unavailable);
    bool pdf_runtime=false; std::string pdf_runtime_code;
    if(proven){
        std::atomic<bool> cancel{false};const auto probe=isolated_transform(p,"worker.capabilities",{},cancel);
        if(probe.outcome!=Outcome::Converted){proven=false;unavailable=L("The bundled worker cannot start in its sandbox.");unavailable_detail=code_or_invalid(probe.code);}
        else try {const auto capabilities=parse_json(probe.output);proven=capabilities.at("core").get<bool>();pdf_runtime=capabilities.at("pdf").get<bool>();pdf_runtime_code=code_or_invalid(capabilities.at("pdf_reason").get<std::string>());}
        catch(...){proven=false;}
        if(!proven&&unavailable.empty())unavailable=L("The bundled worker returned an invalid capability receipt.");
    }
    std::vector<Adapter> r;
    auto add = [&](std::string id, Category c, std::string name, std::string ext, std::vector<Kind> sources, std::string disclosure, std::string validator) {
        r.push_back({std::move(id), c, std::move(name), std::move(ext), std::move(sources), true, proven, false, unavailable, std::move(disclosure), std::move(validator), unavailable_detail});
    };
    const std::vector<Kind> any{Kind::Binary,Kind::Utf8,Kind::Json,Kind::Pdf,Kind::Png,Kind::Jpeg,Kind::Bmp,Kind::Ppm,Kind::Zip,Kind::Wave,Kind::Mp4};
    add("hex.encode", Category::Binary, L("Binary to hexadecimal"), "hex", any, L("Preserves every source byte; output is lowercase ASCII without whitespace."), L("Decode and compare every byte"));
    add("hex.decode", Category::Binary, L("Hexadecimal to binary"), "bin", {Kind::Utf8,Kind::Json}, L("Requires an even count of hexadecimal digits with no whitespace."), L("Re-encode and compare normalized digits"));
    add("base64.encode", Category::Binary, L("Binary to Base64"), "base64", any, L("Preserves every byte; RFC 4648 alphabet and padding, no line wrapping."), L("Decode and compare every byte"));
    add("base64.decode", Category::Binary, L("Base64 to binary"), "bin", {Kind::Utf8,Kind::Json}, L("Requires canonical RFC 4648 padding and no whitespace."), L("Re-encode and compare"));
    add("text.lf", Category::Text, L("UTF-8 with LF line endings"), "txt", {Kind::Utf8,Kind::Json}, L("Changes CRLF and CR line endings to LF. Preserves UTF-8 bytes and BOM otherwise."), L("UTF-8 validation and absence of CR"));
    add("text.crlf", Category::Text, L("UTF-8 with CRLF line endings"), "txt", {Kind::Utf8,Kind::Json}, L("Normalizes all line endings to CRLF. Preserves UTF-8 bytes and BOM otherwise."), L("UTF-8 validation and canonical line endings"));
    add("json.pretty", Category::Structured, L("JSON formatting"), "json", {Kind::Json,Kind::Utf8}, L("UTF-8, two-space indentation, LF. Duplicate keys are rejected. Number literals, string escapes and object order are preserved exactly."), L("Reparse and compare ordered values; preserve original literal bytes"));
    for (const bool csv : {true,false}) {
        const std::string sep = csv ? "csv" : "tsv";
        add(sep + ".json", Category::Structured, csv ? L("CSV to JSON rows") : L("TSV to JSON rows"), "json", {Kind::Utf8,Kind::Json}, L("Every cell remains a string in an array of arrays. Quoted separators and line breaks are preserved. Ragged rows are rejected."), L("Parse and compare every cell"));
        add("json." + sep, Category::Structured, csv ? L("JSON rows to CSV") : L("JSON rows to TSV"), sep, {Kind::Json}, L("Only rectangular arrays of string arrays are supported. Every field is quoted; UTF-8 with CRLF records."), L("Reparse and compare every cell"));
    }
    add("bmp.ppm", Category::Images, L("24-bit BMP to PPM"), "ppm", {Kind::Bmp}, L("Only uncompressed bottom-up 24-bit BMP with a 40-byte header. Resolution metadata is omitted; RGB pixels are preserved."), L("Reopen and compare dimensions and every pixel"));
    add("ppm.bmp", Category::Images, L("PPM to 24-bit BMP"), "bmp", {Kind::Ppm}, L("Only P6 8-bit RGB PPM without comments. RGB pixels are preserved; output has no color profile or resolution metadata."), L("Reopen and compare dimensions and every pixel"));
    add("zip.encode",Category::Archives,L("File to ZIP (one entry)"),"zip",any,L("Preserves every source byte in payload.bin. Original filename and filesystem timestamps are omitted. No encryption."),L("Reopen ZIP, verify CRC and compare extracted bytes"));
    add("zip.decode",Category::Archives,L("ZIP single-entry extraction"),"bin",{Kind::Zip},L("Only one unencrypted regular entry, stored or deflated, at most 64 MiB. Absolute and traversal names are rejected; entry name is not used as a path."),L("Bounded extraction with CRC verification"));
    for (auto &a : r) a.lossy = a.id == "text.lf" || a.id == "text.crlf" || a.category == Category::Images || a.id=="zip.encode";
    auto missing = [&](std::string id, Category c, std::string name, std::string reason) { r.push_back({std::move(id),c,std::move(name),"",{},false,false,false,std::move(reason),"",L("Unavailable"),""}); };
    struct PdfTool { const char *id; const char *name; const char *extension; };
    static const PdfTool pdf_tools[]{
        {"pdf.inspect",L("PDF inspect"),"json"},{"pdf.split",L("PDF split"),"zip"},{"pdf.merge",L("PDF merge"),"pdf"},
        {"pdf.extract",L("PDF extract"),"pdf"},{"pdf.reorder",L("PDF reorder"),"pdf"},{"pdf.rotate",L("PDF rotate"),"pdf"},
        {"pdf.metadata",L("PDF metadata"),"pdf"}};
#ifdef LOCAL_CONVERTER_WITH_PDF
    // The engine and its package are checked once for all seven tools.
    std::string pdf_reason, pdf_detail; bool pdf_proven=false;
    if(proven&&!pdf_runtime){pdf_reason=L("The bundled PDF engine cannot initialize in this sandbox.");pdf_detail=pdf_runtime_code;}
    else if(proven){std::string ignored;pdf_proven=verify_pdf_package(p.installed_directory,ignored);if(!pdf_proven)pdf_reason=L("PDF requires the complete pinned qpdf 12.4.2 runtime inside tools/pdf.");}
#else
    (void)pdf_runtime;
#endif
    for (const auto &tool : pdf_tools)
    {
#ifdef LOCAL_CONVERTER_WITH_PDF
        add(tool.id,Category::Documents,tool.name,tool.extension,{Kind::Pdf},
            L("qpdf 12.4.2. Unencrypted PDFs without forms, signatures or unsupported document features. Up to 1000 pages and 16 MiB aggregate input. Rewrites PDF structure. Split writes page PDFs into one ZIP. Review metadata and page choices before resuming."),
            L("Reopen every output and verify page count/order, rotations, metadata and structural page content"));
        r.back().enabled=pdf_proven; r.back().reason=pdf_proven ? "" : proven ? pdf_reason : unavailable; r.back().detail=pdf_proven ? "" : proven ? pdf_detail : unavailable_detail; r.back().lossy=true;
#else
        missing(tool.id, Category::Documents, tool.name, L("A bundled, hash-verified PDF parser/editor with post-write page and metadata validation is not installed."));
#endif
    }
    missing("image.extended",Category::Images,L("PNG, JPEG, GIF, TIFF, WebP, SVG"), L("These formats have no verified converter worker adapter; slicer image support does not establish converter containment or preservation."));
    missing("audio",Category::Audio,L("WAV, MP3, FLAC, AAC, Ogg"), L("No bundled verified offline audio converter adapter is installed."));
    missing("video",Category::Video,L("MP4, WebM, AVI, MOV"), L("No bundled verified offline video converter adapter is installed."));
    missing("archives",Category::Archives,L("Multi-entry ZIP and 7z"), L("Multi-entry extraction and 7z require a bundled verified adapter with per-entry atomic publication and decompression limits."));
    missing("office",Category::Structured,L("XLSX, ODS, YAML, TOML"), L("No bundled verified parser and loss-preserving converter for these formats is installed."));
    missing("documents",Category::Documents,L("DOCX, ODT, EPUB"), L("No bundled verified offline document converter adapter is installed."));
    return r;
}

Conversion transform(const std::string &adapter, const Bytes &source)
{
    try {
#ifdef LOCAL_CONVERTER_WITH_PDF
        if (adapter.rfind("pdf.",0)==0) return pdf_request_transform(adapter,source);
#endif
        check(source.size() <= Limits::input_bytes, "input_limit");
        if (adapter == "zip.encode" || adapter == "zip.decode") return archive_transform(adapter,source);
        Bytes out;
        if (adapter == "hex.encode") {
            out.reserve(source.size() * 2);
            for (unsigned v : source) { out.push_back(hex[v >> 4]); out.push_back(hex[v & 15]); }
            for (std::size_t i = 0; i < source.size(); ++i) check((unhex(out[2*i]) * 16 + unhex(out[2*i+1])) == source[i], "output_validation");
        } else if (adapter == "hex.decode") {
            check(source.size() % 2 == 0, "invalid_hex_length");
            for (std::size_t i = 0; i < source.size(); i += 2) out.push_back(static_cast<unsigned char>(unhex(source[i]) * 16 + unhex(source[i+1])));
        } else if (adapter == "base64.encode") { out = bytes(base64(source)); check(decode_base64(out) == source, "output_validation"); }
        else if (adapter == "base64.decode") out = decode_base64(source);
        else if (adapter == "text.lf" || adapter == "text.crlf") {
            check(valid_utf8(source), "invalid_utf8");
            for (std::size_t i = 0; i < source.size(); ++i) {
                auto c = source[i];
                if (c == '\r') { if (i + 1 < source.size() && source[i + 1] == '\n') ++i; c = '\n'; }
                if (c == '\n' && adapter == "text.crlf") out.push_back('\r');
                out.push_back(c);
            }
            check(valid_utf8(out), "output_validation");
        } else if (adapter == "json.pretty") {
            const auto j = parse_json(source); out = bytes(format_json(source)); check(parse_json(out) == j, "output_validation");
        } else if (adapter == "csv.json" || adapter == "tsv.json") {
            const auto rows = parse_table(source, adapter == "csv.json" ? ',' : '\t');
            const Json j = rows; out = bytes(j.dump(2) + "\n"); check(parse_json(out) == j, "output_validation");
        } else if (adapter == "json.csv" || adapter == "json.tsv") {
            const auto j = parse_json(source); check(j.is_array(), "table_requires_arrays");
            Table rows; std::size_t count = 0;
            for (const auto &r : j) { check(r.is_array(), "table_requires_arrays"); std::vector<std::string> row;
                for (const auto &c : r) { check(c.is_string(), "table_requires_strings"); check(++count <= Limits::items, "table_item_limit"); row.push_back(c.get<std::string>()); check(valid_utf8(bytes(row.back())), "invalid_utf8"); }
                check(!row.empty(), "empty_table_row");
                if (!rows.empty()) check(row.size() == rows.front().size(), "ragged_table"); rows.push_back(std::move(row)); }
            const char sep = adapter == "json.csv" ? ',' : '\t'; out = bytes(table_text(rows, sep)); check(parse_table(out, sep) == rows, "output_validation");
        } else if (adapter == "bmp.ppm") { const auto img = parse_bmp(source); out = ppm(img); check(same_image(img, parse_ppm(out)), "output_validation"); }
        else if (adapter == "ppm.bmp") { const auto img = parse_ppm(source); out = bmp(img); check(same_image(img, parse_bmp(out)), "output_validation"); }
        else reject("adapter_unavailable");
        check(out.size() <= Limits::output_bytes, "output_limit");
        return {Outcome::Converted,"converted",std::move(out)};
    } catch (const nlohmann::json::exception &) { return {Outcome::Failed,"invalid_or_unsupported_json",{}}; }
    catch (const std::runtime_error &e) { return {Outcome::Failed,e.what(),{}}; }
    catch (...) { return {Outcome::Failed,"conversion_failed",{}}; }
}

bool atomic_create(const fs::path &destination, const Bytes &data, std::string &code)
{
    fs::path temp;
    try {
        check(data.size() <= Limits::output_bytes, "output_limit");
        check(!fs::exists(destination), "destination_exists");
        check(fs::is_directory(destination.parent_path()), "destination_directory_missing");
        check(fs::space(destination.parent_path()).available >= data.size() + 65536, "destination_storage_low");
        static std::atomic<unsigned long> sequence{0};
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        temp = destination.parent_path() / (".conversion-" + std::to_string(nonce) + "-" + std::to_string(sequence++) + ".tmp");
#ifdef _WIN32
        HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_WRITE_THROUGH, nullptr);
        check(h != INVALID_HANDLE_VALUE, "temporary_create_failed");
        DWORD written = 0;
        const bool ok = WriteFile(h, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) && written == data.size() && FlushFileBuffers(h);
        CloseHandle(h); check(ok, "temporary_write_failed");
#else
        const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
        check(fd >= 0, "temporary_create_failed");
        std::size_t pos = 0;
        while (pos < data.size()) { const auto n = ::write(fd, data.data() + pos, data.size() - pos); if (n <= 0) break; pos += n; }
        const bool ok = pos == data.size() && ::fsync(fd) == 0; ::close(fd); check(ok, "temporary_write_failed");
#endif
        check(read_file(temp, Limits::output_bytes) == data, "output_validation");
#ifdef _WIN32
        check(MoveFileExW(temp.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH), "destination_publish_failed");
#else
        check(::link(temp.c_str(), destination.c_str()) == 0, "destination_publish_failed");
        fs::remove(temp);
        const int dir = ::open(destination.parent_path().c_str(), O_RDONLY | O_DIRECTORY); if (dir >= 0) { ::fsync(dir); ::close(dir); }
#endif
        code = "created"; return true;
    } catch (const fs::filesystem_error &) { code = "destination_filesystem_failed"; }
    catch (const std::runtime_error &e) { code = e.what(); }
    catch (...) { code = "destination_write_failed"; }
    if (!temp.empty()) { std::error_code ec; fs::remove(temp, ec); }
    return false;
}
Result convert_file(const fs::path &source, const fs::path &destination, const std::string &adapter, const Executor &execute, const std::atomic<bool> &cancel,
                    const std::string &options, const std::vector<fs::path> &additional_sources)
{
    try {
        if (cancel.load()) return {Outcome::Cancelled,"cancelled",0};
        if (fs::exists(destination)) return {Outcome::Skipped,"destination_exists",0};
        check(fs::weakly_canonical(source) != fs::weakly_canonical(destination), "source_destination_alias");
        check(fs::space(destination.parent_path()).available >= Limits::output_bytes + 65536, "destination_storage_low");
        const auto stamp = modified(source); const auto input = read_file(source, Limits::input_bytes);
        Bytes request=input;
        std::vector<std::pair<fs::path,Bytes>> extra_inputs;
        if(adapter.rfind("pdf.",0)==0) {
            check(options.size()<=16384 && additional_sources.size()<=1000,"pdf_request_limit");
            Json envelope{{"sources",Json::array({base64(input)})},{"options",options.empty()?Json::object():parse_json(bytes(options))}};
            std::uint64_t total=input.size();
            for(const auto &path:additional_sources) {
                check(fs::weakly_canonical(path)!=fs::weakly_canonical(destination),"source_destination_alias");
                auto extra=read_file(path,Limits::input_bytes-total); total+=extra.size();
                envelope["sources"].push_back(base64(extra));
                extra_inputs.emplace_back(path,std::move(extra));
            }
            request=bytes(envelope.dump()); check(request.size()<=Limits::wire_bytes,"pdf_request_limit");
        } else check(options.empty() && additional_sources.empty(),"unsupported_conversion_options");
        auto result = execute(adapter,request,cancel);
        if (cancel.load()) return {Outcome::Cancelled,"cancelled",0};
        if (result.outcome != Outcome::Converted) return {result.outcome,result.code,0};
        check(modified(source) == stamp && read_file(source,Limits::input_bytes) == input, "source_changed");
        for(const auto &extra:extra_inputs)check(read_file(extra.first,Limits::input_bytes)==extra.second,"source_changed");
        check(result.output.size() <= Limits::output_bytes, "output_limit");
        std::string code;
        if (!atomic_create(destination,result.output,code)) return {Outcome::Failed,code,0};
        return {Outcome::Converted,"converted",result.output.size()};
    } catch (...) { return {Outcome::Failed,"file_preflight_failed",0}; }
}

const char *state_name(State s)
{
    static const char *names[]{L("Pending"),L("Running"),L("Converted"),L("Skipped"),L("Cancelled"),L("Failed"),L("Recovery required")};
    return names[static_cast<unsigned>(s)];
}
const char *result_message(const std::string &code)
{
    // One plain sentence per stable code. The code itself stays visible beside
    // the translated sentence, so the exact boundary is never lost.
    struct Message { const char *code; const char *text; };
    static const Message exact[]{
        {"pending",L("Waiting to convert.")},
        {"running",L("Converting now.")},
        {"converted",L("Converted, checked and saved.")},
        {"created",L("Written and checked.")},
        {"cancelled",L("Cancelled. Nothing was written for this file.")},
        {"ok",L("The bundled PDF engine is ready.")},
        {"capabilities",L("The worker reported what it can convert.")},
        {"destination_exists",L("The destination already exists. It was not replaced.")},
        {"destination_directory_missing",L("The output folder no longer exists.")},
        {"destination_storage_low",L("The output folder does not have enough free space.")},
        {"destination_publish_failed",L("The finished output could not be published without replacing another file.")},
        {"destination_filesystem_failed",L("The file system refused the output.")},
        {"destination_write_failed",L("The output could not be written.")},
        {"temporary_create_failed",L("A temporary file could not be created in the output folder.")},
        {"temporary_write_failed",L("The temporary output could not be written completely.")},
        {"source_destination_alias",L("The source and the destination are the same file.")},
        {"source_not_regular",L("The source is not a regular file.")},
        {"source_unreadable",L("The source file could not be read.")},
        {"source_changed",L("The source changed during conversion. Nothing was written.")},
        {"source_changed_since_admission",L("The source changed after it was added. Add it again to convert the new version.")},
        {"source_unavailable",L("The source is no longer available.")},
        {"source_unreadable_or_changed",L("The source could not be read completely, or it changed while it was read.")},
        {"incompatible_source_signature",L("The file content does not match the formats this adapter accepts.")},
        {"admission_preflight_failed",L("The file could not be checked before it was added.")},
        {"directory_discovery_incomplete",L("The folder could not be read completely. Files found before the problem were kept.")},
        {"file_preflight_failed",L("The file could not be checked before conversion.")},
        {"input_limit",L("The source is larger than the 16 MiB input limit.")},
        {"input_limit_or_type",L("The source is not a regular file within the 16 MiB input limit.")},
        {"output_limit",L("The output would be larger than the 64 MiB output limit.")},
        {"output_validation",L("The output did not match when it was checked, so it was discarded.")},
        {"path_limit",L("A path is longer than the converter accepts.")},
        {"request_limit",L("The request is larger than the worker accepts.")},
        {"unsupported_conversion_options",L("This adapter takes no options or extra sources.")},
        {"adapter_unavailable",L("This adapter is not available in the bundled worker.")},
        {"invalid_adapter",L("The adapter name is invalid.")},
        {"conversion_failed",L("The conversion failed without a more specific reason.")},
        {"structure_depth_limit",L("The structure nests deeper than the 64-level limit.")},
        {"structure_item_limit",L("The structure has more items than the converter allows.")},
        {"duplicate_json_key",L("The JSON repeats a key inside one object.")},
        {"invalid_or_unsupported_json",L("The JSON is invalid or uses an unsupported form.")},
        {"invalid_utf8",L("The text is not valid UTF-8, or it contains control characters.")},
        {"invalid_hex",L("The text contains a character that is not a hexadecimal digit.")},
        {"invalid_hex_length",L("Hexadecimal text needs an even number of digits.")},
        {"invalid_base64_length",L("The Base64 text length is not a multiple of four.")},
        {"invalid_base64_padding",L("The Base64 padding is not canonical.")},
        {"invalid_base64_character",L("The text contains a character outside the Base64 alphabet.")},
        {"noncanonical_base64",L("The Base64 text is not in canonical form.")},
        {"table_item_limit",L("The table has more cells than the converter allows.")},
        {"invalid_table_line_ending",L("The table has a carriage return without a line feed.")},
        {"invalid_table_quote",L("A table cell has a quote in an unexpected place.")},
        {"data_after_table_quote",L("A table cell has text after its closing quote.")},
        {"unterminated_table_quote",L("A quoted table cell is never closed.")},
        {"ragged_table",L("The table rows do not all have the same number of cells.")},
        {"table_requires_arrays",L("The JSON must be an array of rows, and each row an array.")},
        {"table_requires_strings",L("Every JSON table cell must be a string.")},
        {"empty_table_row",L("A JSON table row is empty.")},
        {"truncated_image",L("The image ends before its header is complete.")},
        {"image_pixel_limit",L("The image has more pixels than the 4,194,304-pixel limit.")},
        {"invalid_bmp",L("The source is not a BMP image.")},
        {"invalid_bmp_length",L("The BMP size does not match its dimensions.")},
        {"unsupported_bmp_header",L("Only BMP images with the 40-byte header are supported.")},
        {"unsupported_bmp_encoding",L("Only uncompressed 24-bit BMP images are supported.")},
        {"invalid_ppm",L("The source is not a binary PPM (P6) image.")},
        {"invalid_ppm_length",L("The PPM pixel data does not match its dimensions.")},
        {"invalid_ppm_separator",L("The PPM header must end with one whitespace character.")},
        {"unsupported_ppm_header",L("The PPM header is not supported. Comments are not accepted.")},
        {"unsupported_ppm_depth",L("Only 8-bit PPM images with a maximum value of 255 are supported.")},
        {"invalid_zip",L("The source is not a readable ZIP archive.")},
        {"zip_requires_one_regular_entry",L("The ZIP archive must contain exactly one regular file.")},
        {"unsupported_or_encrypted_zip",L("The ZIP entry is encrypted or uses an unsupported compression method.")},
        {"unsafe_archive_entry_name",L("The ZIP entry name is absolute or leaves its folder.")},
        {"archive_output_limit",L("The extracted file would be larger than the 64 MiB output limit.")},
        {"archive_crc_or_decode_failed",L("The ZIP entry failed its CRC check or could not be decompressed.")},
        {"archive_memory_limit",L("The archive needs more memory than the converter allows.")},
        {"archive_encode_failed",L("The ZIP archive could not be created.")},
        {"archive_finalize_failed",L("The ZIP archive could not be completed.")},
        {"archive_roundtrip_failed",L("The new ZIP archive did not extract back to the same bytes, so it was discarded.")},
        {"queue_already_owned",L("Another converter already owns this queue.")},
        {"queue_write_failed",L("A queue record could not be saved.")},
        {"queue_publish_failed",L("A queue record could not be replaced safely.")},
        {"queue_record_limit",L("A queue record is larger than its limit.")},
        {"queue_page_limit",L("A queue page larger than 100 records was requested.")},
        {"queue_identifier_exhausted",L("The queue has run out of record numbers.")},
        {"queue_generation_exhausted",L("The queue has run out of cancellation numbers.")},
        {"queue_source_identity_missing",L("A queue record is missing a source identity.")},
        {"invalid_queue_record",L("A saved queue record is invalid.")},
        {"invalid_queue_state",L("A saved queue record has an unknown state.")},
        {"invalid_queue_cursor",L("The saved queue position is invalid.")},
        {"unsupported_queue_version",L("The saved queue was written by an unsupported version.")},
        {"invalid_rejection_code",L("The rejection code is invalid.")},
        {"interrupted_review_destination_before_retry",L("The app stopped while this file was converting. Check the destination, then retry.")},
        {"retry_unavailable",L("This record cannot be retried now.")},
        {"retry_state_invalid",L("Only failed, cancelled or interrupted records can be retried.")},
        {"retry_destination_exists",L("The destination already exists, so the record was not retried.")},
        {"bundled_worker_unavailable",L("The bundled worker file cannot be opened.")},
        {"bundled_worker_unverified",L("The bundled worker does not match its package proof.")},
        {"sandbox_unavailable",L("The isolated worker sandbox requires Windows.")},
        {"appcontainer_unavailable",L("Windows could not prepare the isolated sandbox.")},
        {"pipe_security_unavailable",L("The private channel to the worker could not be secured.")},
        {"pipe_unavailable",L("The private channel to the worker could not be created.")},
        {"pipe_isolation_failed",L("The private channel to the worker could not be isolated.")},
        {"containment_unavailable",L("The resource limits for the worker could not be created.")},
        {"containment_limits_failed",L("The resource limits for the worker could not be applied.")},
        {"containment_attributes_failed",L("The worker sandbox could not be configured.")},
        {"system_directory_unavailable",L("The Windows system folder could not be located.")},
        {"worker_job_assignment_failed",L("The worker could not be placed under its resource limits, so it was stopped.")},
        {"worker_resume_failed",L("The worker could not start after containment, so it was stopped.")},
        {"invalid_worker_response",L("The worker sent an invalid response.")},
        {"invalid_worker_diagnostic",L("The worker sent an invalid diagnostic code.")},
        {"truncated_worker_response",L("The worker response ended early.")},
        {"excess_worker_response",L("The worker sent more data than it declared.")},
        {"worker_deadline",L("The worker did not finish within its 30-second limit and was stopped.")},
        {"worker_failed",L("The worker stopped with an error.")},
        {"pdf_adapter_not_built",L("This build does not include the PDF adapter.")},
        {"pdf_sandbox_unavailable",L("The PDF engine could not confirm its sandbox.")},
        {"pdf_sandbox_required",L("The PDF engine runs only inside the isolated sandbox.")},
        {"pdf_package_unavailable",L("The PDF engine package could not be located.")},
        {"pdf_package_unreadable",L("A PDF engine file could not be opened.")},
        {"pdf_package_digest_mismatch",L("A PDF engine file does not match its pinned SHA-256.")},
        {"pdf_win32k_disabled",L("This sandbox blocks the Windows interface calls the PDF engine needs.")},
        {"pdf_engine_api_mismatch",L("The PDF engine does not provide the expected interface.")},
        {"pdf_engine_unavailable",L("The PDF engine is not loaded.")},
        {"pdf_signature",L("The source does not start with a PDF signature.")},
        {"pdf_malformed",L("The PDF is damaged or not well formed.")},
        {"pdf_encrypted",L("The PDF is encrypted. Encrypted PDFs are not supported.")},
        {"pdf_signed",L("The PDF carries a digital signature, which this tool would invalidate.")},
        {"pdf_unsupported_catalog",L("The PDF uses document features this tool does not support, such as forms, actions or outlines.")},
        {"pdf_unsupported_page_features",L("A page has annotations or actions, which this tool does not support.")},
        {"pdf_unsupported_stream",L("A PDF stream uses an encoding this tool cannot read.")},
        {"pdf_unsupported_metadata",L("The PDF metadata uses a form this tool does not support.")},
        {"pdf_unsupported_rotation",L("A page has a rotation that is not a multiple of 90 degrees.")},
        {"pdf_memory_limit",L("The PDF needs more memory than the converter allows.")},
        {"pdf_internal_error",L("The PDF engine stopped unexpectedly.")},
        {"pdf_depth_limit",L("The PDF structure nests deeper than the 64-level limit.")},
        {"pdf_item_limit",L("The PDF structure has more items than the converter allows.")},
        {"pdf_expansion_limit",L("The PDF content expands beyond the 64 MiB limit.")},
        {"pdf_page_limit",L("The PDF has more pages than the 1000-page limit.")},
        {"pdf_source_limit",L("More PDFs were given than the 1000-source limit.")},
        {"pdf_source_count",L("This PDF tool takes exactly one source. Only merge takes several.")},
        {"pdf_input_limit",L("The PDFs together are larger than the 16 MiB input limit.")},
        {"pdf_output_limit",L("The PDF output would be larger than the 64 MiB output limit.")},
        {"pdf_envelope_limit",L("The PDF request is larger than its 24 MiB limit.")},
        {"pdf_request_limit",L("The PDF request is larger than its limit.")},
        {"pdf_operation",L("The PDF operation is not recognized.")},
        {"pdf_request_operation",L("The PDF operation is not recognized.")},
        {"pdf_rotation_option",L("Rotation must be 0, 90, 180 or 270 degrees.")},
        {"pdf_page_range",L("A page number is outside the document.")},
        {"pdf_duplicate_page",L("A page number appears more than once.")},
        {"pdf_selection_required",L("Enter the page numbers to use.")},
        {"pdf_reorder_permutation",L("Reorder needs every page number exactly once.")},
        {"pdf_unexpected_selection",L("This PDF tool does not take page numbers.")},
        {"pdf_unexpected_rotation",L("This PDF tool does not take a rotation.")},
        {"pdf_unexpected_metadata",L("Only the metadata tool takes metadata values.")},
        {"pdf_merge_metadata_conflict",L("The PDFs to merge have different metadata.")},
        {"pdf_metadata_key",L("A metadata field is not one this tool can write.")},
        {"pdf_metadata_value",L("A metadata value is too long, or it is not valid UTF-8 text.")},
        {"pdf_metadata_limit",L("A metadata value is longer than 4,096 bytes.")},
        {"pdf_write_failed",L("The PDF engine could not write the output.")},
        {"pdf_validation_page_count",L("The new PDF has the wrong number of pages, so it was discarded.")},
        {"pdf_validation_page_order",L("The new PDF has its pages in the wrong order, so it was discarded.")},
        {"pdf_validation_rotation",L("The new PDF has the wrong page rotation, so it was discarded.")},
        {"pdf_validation_metadata",L("The new PDF has the wrong metadata, so it was discarded.")},
        {"pdf_result_shape",L("The PDF engine returned an unexpected number of outputs.")},
        {"pdf_split_count",L("Split produced no pages or too many pages.")},
        {"pdf_zip_memory",L("The ZIP of split pages could not be started.")},
        {"pdf_zip_write",L("A split page could not be added to the ZIP.")},
        {"pdf_zip_finalize",L("The ZIP of split pages could not be completed.")},
        {"pdf_zip_validation",L("The ZIP of split pages did not reopen to the same pages, so it was discarded.")},
        {"pdf_request_json",L("The PDF request is not well formed.")},
        {"pdf_request_object",L("The PDF options must be an object.")},
        {"pdf_request_unknown_option",L("The PDF request contains an unknown option.")},
        {"pdf_request_utf8",L("The PDF request is not valid UTF-8.")},
        {"pdf_request_depth_limit",L("The PDF request nests too deeply.")},
        {"pdf_request_item_limit",L("The PDF request has too many items.")},
        {"pdf_request_duplicate_key",L("The PDF request repeats an option.")},
        {"pdf_request_base64",L("A PDF in the request is not valid Base64.")},
        {"pdf_request_page_type",L("Page numbers must be whole numbers.")},
        {"pdf_request_pages",L("The page list is not a list, or it is too long.")},
        {"pdf_request_metadata",L("The metadata must be a set of at most nine fields.")},
        {"pdf_request_metadata_value",L("Every metadata value must be text.")},
        {"pdf_request_sources",L("The PDF request lists no sources.")},
        {"pdf_request_source_type",L("A PDF source in the request is not text.")},
        {"pdf_request_internal",L("The PDF request could not be processed.")},
    };
    // Codes that end in a Windows error number.
    static const Message numbered[]{
        {"isolated_worker_start_",L("Windows could not start the isolated worker. The number at the end of the code is the Windows error.")},
        {"pdf_system_ui_load_",L("Windows could not load its user interface library inside the sandbox. The number at the end of the code is the Windows error.")},
        {"pdf_engine_load_",L("Windows could not load the bundled PDF engine. The number at the end of the code is the Windows error.")},
    };
    for (const auto &message : exact)
        if (code == message.code) return message.text;
    for (const auto &message : numbered) {
        const std::size_t length = std::strlen(message.code);
        if (code.size() > length && code.size() <= length + 10 && code.compare(0,length,message.code) == 0 &&
            code.find_first_not_of("0123456789",length) == std::string::npos)
            return message.text;
    }
    return nullptr;
}
Queue::Queue(fs::path root) : m_root(std::move(root))
{
    fs::create_directories(m_root);
#ifdef _WIN32
    m_lock = CreateFileW((m_root / "owner.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(m_lock != INVALID_HANDLE_VALUE, "queue_already_owned");
#else
    int fd = ::open((m_root / "owner.lock").c_str(), O_CREAT | O_RDWR, 0600);
    if (fd < 0 || flock(fd, LOCK_EX | LOCK_NB) != 0) { if (fd >= 0) ::close(fd); reject("queue_already_owned"); }
    m_lock = reinterpret_cast<void *>(static_cast<intptr_t>(fd + 1));
#endif
    try {
        if (fs::exists(m_root / "queue.json")) {
            auto j = parse_json(read_file(m_root / "queue.json",65536));
            check(j.at("version") == 1, "unsupported_queue_version");
            m_count = j.at("count").get<std::uint64_t>(); m_cursor = j.at("cursor").get<std::uint64_t>();
            m_cancellation_generation=j.value("cancellation_generation",std::uint64_t(0));
            check(m_cursor >= 1 && m_cursor <= m_count + 1, "invalid_queue_cursor");
        }
        // At most one job can be active. Restart is always paused and requires
        // explicit retry, avoiding a false success after publication but before
        // the completion record became durable.
        if (m_cursor <= m_count) { auto j = read(m_cursor); if (j.state == State::Running) { j.state = State::RecoveryRequired; j.code = "interrupted_review_destination_before_retry"; write(j); } }
        save_meta();
    } catch (...) {
#ifdef _WIN32
        CloseHandle(m_lock);
#else
        ::close(static_cast<int>(reinterpret_cast<intptr_t>(m_lock) - 1));
#endif
        m_lock = nullptr; throw;
    }
}
Queue::~Queue()
{
#ifdef _WIN32
    if (m_lock) CloseHandle(m_lock);
#else
    if (m_lock) ::close(static_cast<int>(reinterpret_cast<intptr_t>(m_lock) - 1));
#endif
}
void Queue::save_meta() { replace_record(m_root / "queue.json", Json{{"version",1},{"count",m_count},{"cursor",m_cursor},{"paused",m_paused},{"cancellation_generation",m_cancellation_generation}}.dump()); }
void Queue::write(const Job &j)
{
    check(j.additional_sources.size()==j.additional_sizes.size()&&j.additional_sources.size()==j.additional_modified.size(),"queue_source_identity_missing");
    Json sources=Json::array(); for(std::size_t i=0;i<j.additional_sources.size();++i) sources.push_back(Json{{"path",j.additional_sources[i].u8string()},{"size",j.additional_sizes[i]},{"modified",j.additional_modified[i]}});
    const auto record=Json{{"version",1},{"id",j.id},{"source",j.source.u8string()},{"destination",j.destination.u8string()},
        {"adapter",j.adapter},{"state",static_cast<unsigned>(j.state)},{"code",j.code},{"input_size",j.input_size},{"input_modified",j.input_modified},
        {"options",j.options},{"additional_sources",sources},{"cancellation_generation",j.cancellation_generation}}.dump();
    check(record.size()<=65536,"queue_record_limit"); replace_record(record_path(m_root,j.id),record);
}
Job Queue::read(std::uint64_t id) const
{
    auto j = parse_json(read_file(record_path(m_root,id),65536));
    check(j.at("version") == 1 && j.at("id") == id, "invalid_queue_record");
    const auto state = j.at("state").get<unsigned>(); check(state <= static_cast<unsigned>(State::RecoveryRequired), "invalid_queue_state");
    Job result{id,fs::u8path(j.at("source").get<std::string>()),fs::u8path(j.at("destination").get<std::string>()),j.at("adapter").get<std::string>(),
        static_cast<State>(state),j.at("code").get<std::string>(),j.at("input_size").get<std::uint64_t>(),j.at("input_modified").get<std::int64_t>()};
    result.options=j.value("options",std::string());
    result.cancellation_generation=j.value("cancellation_generation",std::uint64_t(0));
    if(result.state==State::Pending&&result.cancellation_generation<m_cancellation_generation){result.state=State::Cancelled;result.code="cancelled";}
    if(j.contains("additional_sources")) for(const auto &p:j["additional_sources"]) {result.additional_sources.push_back(fs::u8path(p.at("path").get<std::string>()));result.additional_sizes.push_back(p.at("size").get<std::uint64_t>());result.additional_modified.push_back(p.at("modified").get<std::int64_t>());}
    check(result.options.size()<=16384 && result.additional_sources.size()<=1000,"queue_record_limit");
    return result;
}
std::uint64_t Queue::enqueue(const fs::path &source, const fs::path &destination, const std::string &adapter,const std::string &options,const std::vector<fs::path> &additional_sources)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    check(adapter.size() < 64 && !adapter.empty(), "invalid_adapter");
    check(source.u8string().size() <= 16384 && destination.u8string().size() <= 16384, "path_limit");
    check(fs::is_regular_file(fs::symlink_status(source)) && fs::file_size(source) <= Limits::input_bytes, "input_limit_or_type");
    check(fs::is_directory(destination.parent_path()) && fs::space(destination.parent_path()).available >= Limits::output_bytes + 65536, "destination_storage_low");
    check(fs::weakly_canonical(source) != fs::weakly_canonical(destination), "source_destination_alias");
    check(m_count < UINT64_MAX - 1, "queue_identifier_exhausted");
    Job j{m_count + 1,fs::absolute(source),fs::absolute(destination),adapter,State::Pending,"pending",fs::file_size(source),modified(source)};
    check(options.size()<=16384 && additional_sources.size()<=1000,"queue_record_limit");
    if(!options.empty()) parse_json(bytes(options));
    j.options=options;
    j.cancellation_generation=m_cancellation_generation;
    std::uint64_t total=j.input_size;
    for(const auto &p:additional_sources) { check(fs::is_regular_file(fs::symlink_status(p)),"source_not_regular");const auto size=fs::file_size(p);check(size<=Limits::input_bytes-total,"input_limit");total+=size;j.additional_sources.push_back(fs::absolute(p));j.additional_sizes.push_back(size);j.additional_modified.push_back(modified(p)); }
    write(j); ++m_count; save_meta(); return j.id;
}
std::uint64_t Queue::record_rejected(const fs::path &source,const fs::path &destination,const std::string &adapter,const std::string &code)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    check(adapter.size()<64&&code.size()<96&&!code.empty()&&code.find_first_not_of("abcdefghijklmnopqrstuvwxyz_0123456789")==std::string::npos,"invalid_rejection_code");
    check(m_count<UINT64_MAX-1,"queue_identifier_exhausted");
    Job j{m_count+1,source,destination,adapter,State::Skipped,code};j.cancellation_generation=m_cancellation_generation;
    write(j);++m_count;save_meta();return j.id;
}
std::vector<Job> Queue::page(std::uint64_t after, std::size_t count) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    check(count > 0 && count <= Limits::page_size, "queue_page_limit");
    std::vector<Job> out; if (after >= m_count) return out;
    for (auto id = after + 1; id <= m_count && out.size() < count; ++id) out.push_back(read(id));
    return out;
}
bool Queue::step(const Executor &executor, const std::atomic<bool> &cancel)
{
    Job job;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_paused || m_active || cancel.load()) return false;
        std::size_t scanned=0;
        while (m_cursor <= m_count) {
            job = read(m_cursor);
            if (job.state == State::Pending) break;
            ++m_cursor;
            if(++scanned==Limits::page_size){save_meta();return true;}
        }
        if (m_cursor > m_count) { save_meta(); return false; }
        job.state = State::Running; job.code = "running"; write(job); save_meta(); m_active = true;
    }
    Result result;
    try {
        bool sources_match=true;
        for(std::size_t i=0;i<job.additional_sources.size();++i)if(fs::file_size(job.additional_sources[i])!=job.additional_sizes[i]||modified(job.additional_sources[i])!=job.additional_modified[i])sources_match=false;
        if (!sources_match || fs::file_size(job.source) != job.input_size || modified(job.source) != job.input_modified)
            result = {Outcome::Failed,"source_changed_since_admission",0};
        else result = convert_file(job.source,job.destination,job.adapter,executor,cancel,job.options,job.additional_sources);
    } catch (...) { result = {Outcome::Failed,"source_unavailable",0}; }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active = false;
    switch (result.outcome) {
    case Outcome::Converted: job.state = State::Converted; break;
    case Outcome::Skipped: job.state = State::Skipped; break;
    case Outcome::Cancelled: job.state = State::Cancelled; break;
    case Outcome::Failed: job.state = State::Failed; break;
    }
    job.code = result.code; write(job); ++m_cursor; save_meta(); return true;
}
void Queue::pause(bool value) { std::lock_guard<std::mutex> lock(m_mutex); m_paused = value; save_meta(); }
bool Queue::paused() const { std::lock_guard<std::mutex> lock(m_mutex); return m_paused; }
std::uint64_t Queue::count() const { std::lock_guard<std::mutex> lock(m_mutex); return m_count; }
void Queue::cancel_pending()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_paused = true;
    check(m_cancellation_generation<UINT64_MAX,"queue_generation_exhausted");++m_cancellation_generation;
    save_meta();
}
void Queue::retry(std::uint64_t id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    check(!m_active && id && id <= m_count, "retry_unavailable"); auto j = read(id);
    check(j.state == State::Failed || j.state == State::Cancelled || j.state == State::RecoveryRequired, "retry_state_invalid");
    check(!fs::exists(j.destination), "retry_destination_exists");
    check(fs::file_size(j.source) == j.input_size && modified(j.source) == j.input_modified, "source_changed_since_admission");
    j.state = State::Pending; j.code = "pending"; j.cancellation_generation=m_cancellation_generation;write(j); m_cursor = std::min(m_cursor,id); save_meta();
}
} // namespace Slic3r::LocalConverter
