// Standalone assertions for src/slic3r/GUI/Base64Toolkit/Base64Codec.cpp, the
// local Base64 toolkit's codec. Compiled and run by tests/base64_codec.test.mjs.
#include "src/slic3r/GUI/Base64Toolkit/Base64Codec.cpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <random>

using namespace Slic3r::GUI::Base64Toolkit;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

static Bytes B(const std::string &s) { return Bytes(s.begin(), s.end()); }

static void rfc4648_vectors()
{
    const std::pair<const char *, const char *> vectors[] = {
        {"", ""}, {"f", "Zg=="}, {"fo", "Zm8="}, {"foo", "Zm9v"}, {"foob", "Zm9vYg=="}, {"fooba", "Zm9vYmE="}, {"foobar", "Zm9vYmFy"}};
    for (const auto &[plain, coded] : vectors) {
        CHECK(encode(B(plain), EncodeOptions{}) == coded);
        if (*plain) {
            const auto back = decode(coded, DecodeOptions{});
            CHECK(back.ok());
            CHECK(back.bytes == B(plain));
        }
    }
    CHECK(decode("", DecodeOptions{}).error == DecodeError::Empty);
}

static void alphabets_padding_and_wrapping()
{
    const Bytes data{0xfb, 0xff};
    CHECK(encode(data, EncodeOptions{}) == "+/8=");
    EncodeOptions url;
    url.alphabet = Alphabet::UrlSafe;
    CHECK(encode(data, url) == "-_8=");
    url.padding = false;
    CHECK(encode(data, url) == "-_8");
    DecodeOptions strict_url;
    strict_url.alphabet        = Alphabet::UrlSafe;
    strict_url.require_padding = false;
    CHECK(decode("-_8", strict_url).bytes == data);
    CHECK(decode("+/8=", strict_url).error == DecodeError::InvalidCharacter);

    Bytes big(100, 0x41);
    EncodeOptions mime;
    mime.line_length = 76;
    const std::string wrapped = encode(big, mime);
    CHECK(wrapped.find("\r\n") == 76);
    CHECK(wrapped.size() == 136 + 2);
    CHECK(wrapped.back() != '\n');
    EncodeOptions custom;
    custom.line_length = 10;
    custom.crlf        = false;
    const std::string ten = encode(big, custom);
    CHECK(ten.find('\n') == 10);
    CHECK(ten.find("\r") == std::string::npos);
    DecodeOptions lenient;
    lenient.strict = false;
    CHECK(decode(wrapped, lenient).bytes == big);
    CHECK(decode(ten, lenient).bytes == big);
}

static void strict_and_lenient_errors()
{
    DecodeOptions strict;
    // The documented example: '!' at offset 17.
    const auto bad = decode("YnJhY2tldF92MyDC!yBwbGF0ZQ==", strict);
    CHECK(bad.error == DecodeError::InvalidCharacter);
    CHECK(bad.offset == 16);
    CHECK(bad.character == '!');
    const auto space = decode("Zm9v Zg==", strict);
    CHECK(space.error == DecodeError::InvalidCharacter && space.offset == 4 && space.character == ' ');
    CHECK(decode("Zg=", strict).error == DecodeError::WrongPadding);
    CHECK(decode("Zg", strict).error == DecodeError::WrongPadding);
    CHECK(decode("Z===", strict).error == DecodeError::WrongPadding);
    CHECK(decode("Zg==Zg==", strict).error == DecodeError::TrailingData);
    CHECK(decode("Zm9vZ", strict).error == DecodeError::TruncatedInput);
    CHECK(decode("Zh==", strict).error == DecodeError::WrongPadding); // non-zero unused bits
    // Offsets count code points, not bytes.
    const auto unicode = decode("Zm9vé", strict);
    CHECK(unicode.error == DecodeError::InvalidCharacter && unicode.offset == 4 && unicode.character == 0xE9);

    DecodeOptions lenient;
    lenient.strict = false;
    CHECK(decode(" Zm9v\r\nYg ", lenient).bytes == B("foob"));
    CHECK(decode("Zm9vYg", lenient).bytes == B("foob"));
    CHECK(decode("Zm9vYg=", lenient).bytes == B("foob"));
    CHECK(decode("Zh==", lenient).bytes == B("f"));
    CHECK(decode("Zm9v!", lenient).error == DecodeError::InvalidCharacter);
    CHECK(decode("  \n ", lenient).error == DecodeError::Empty);
}

static void streaming_matches_one_shot()
{
    std::mt19937 rng(20261108);
    for (int round = 0; round < 40; ++round) {
        Bytes data(rng() % 5000);
        for (auto &b : data) b = std::uint8_t(rng());
        EncodeOptions options;
        options.alphabet    = round % 2 ? Alphabet::UrlSafe : Alphabet::Standard;
        options.padding     = round % 3 != 0;
        options.line_length = round % 4 == 0 ? 76 : (round % 4 == 1 ? 0 : 7 + round);
        options.crlf        = round % 5 != 0;
        const std::string whole = encode(data, options);
        StreamEncoder     encoder(options);
        std::string       streamed;
        for (std::size_t i = 0; i < data.size();) {
            const std::size_t n = std::min<std::size_t>(data.size() - i, 1 + rng() % 97);
            streamed += encoder.feed(data.data() + i, n);
            i += n;
        }
        streamed += encoder.finish();
        CHECK(streamed == whole);
        DecodeOptions decode_options;
        decode_options.alphabet = options.alphabet;
        decode_options.strict   = false;
        StreamDecoder decoder(decode_options);
        Bytes         back;
        bool          ok = true;
        for (std::size_t i = 0; i < whole.size();) {
            const std::size_t n = std::min<std::size_t>(whole.size() - i, 1 + rng() % 61);
            ok = ok && decoder.feed(std::string_view(whole).substr(i, n), back);
            i += n;
        }
        ok = ok && (data.empty() ? true : decoder.finish(back));
        CHECK(ok);
        CHECK(back == data);
    }
    // A multi-byte character split across chunks still reports one offset.
    DecodeOptions strict;
    StreamDecoder decoder(strict);
    Bytes         out;
    const std::string text = "Zm9v\xC3\xA9";
    CHECK(decoder.feed(std::string_view(text).substr(0, 5), out));
    CHECK(!decoder.feed(std::string_view(text).substr(5), out));
    CHECK(decoder.error() == DecodeError::InvalidCharacter && decoder.error_offset() == 4 && decoder.error_character() == 0xE9);
}

static void text_encodings()
{
    const std::string text = "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80"; // A é € 😀
    Bytes             out;
    std::size_t       bad = 0;
    CHECK(text_to_bytes(text, TextEncoding::Utf8, out, bad) && out == B(text));
    CHECK(text_to_bytes(text, TextEncoding::Utf16LE, out, bad));
    CHECK((out == Bytes{0x41, 0, 0xE9, 0, 0xAC, 0x20, 0x3D, 0xD8, 0x00, 0xDE}));
    CHECK(text_to_bytes(text, TextEncoding::Utf16BE, out, bad));
    CHECK((out == Bytes{0, 0x41, 0, 0xE9, 0x20, 0xAC, 0xD8, 0x3D, 0xDE, 0x00}));
    CHECK(!text_to_bytes(text, TextEncoding::Latin1, out, bad) && bad == 2);
    CHECK(text_to_bytes("A\xC3\xA9", TextEncoding::Latin1, out, bad) && (out == Bytes{0x41, 0xE9}));
    std::string back;
    CHECK(bytes_to_text(Bytes{0x41, 0, 0xE9, 0, 0xAC, 0x20, 0x3D, 0xD8, 0x00, 0xDE}, TextEncoding::Utf16LE, back, bad) && back == text);
    CHECK(!bytes_to_text(Bytes{0x41, 0}, TextEncoding::Utf16LE, back, bad) == false);
    CHECK(!bytes_to_text(Bytes{0x41}, TextEncoding::Utf16LE, back, bad) && bad == 0);
    CHECK(!bytes_to_text(Bytes{0x41, 0xC3}, TextEncoding::Utf8, back, bad) && bad == 1);
    CHECK(!bytes_to_text(Bytes{0x00, 0xDC, 0x41, 0x00}, TextEncoding::Utf16LE, back, bad) && bad == 0);
    CHECK(bytes_to_text(Bytes{0xE9}, TextEncoding::Latin1, back, bad) && back == "\xC3\xA9");
}

static void data_urls()
{
    const std::string url = build_data_url("image/png", Bytes{0x89, 'P', 'N', 'G'});
    CHECK(url == "data:image/png;base64,iVBORw==");
    DataUrl     parsed;
    std::string error;
    CHECK(parse_data_url(url, parsed, error));
    CHECK(parsed.mime == "image/png" && parsed.base64 && (parsed.bytes == Bytes{0x89, 'P', 'N', 'G'}));
    CHECK(parse_data_url("data:text/plain;charset=utf-8,hello%20world", parsed, error));
    CHECK(!parsed.base64 && parsed.bytes == B("hello world") && parsed.parameters.size() == 1 && parsed.parameters[0].second == "utf-8");
    CHECK(parse_data_url("DATA:,abc", parsed, error) && parsed.mime == "text/plain" && parsed.bytes == B("abc"));
    CHECK(!parse_data_url("data:image/png;base64", parsed, error) && error == "missing-comma");
    CHECK(!parse_data_url("http://x", parsed, error) && error == "not-a-data-url");
    CHECK(!parse_data_url("data:text/plain,%G1", parsed, error) && error == "invalid-percent-escape");
    CHECK(parse_data_url("data:application/octet-stream;base64,-_8", parsed, error) && (parsed.bytes == Bytes{0xfb, 0xff}));
}

static void detection_and_naming()
{
    CHECK(detect_type(B("\x89PNG\r\n\x1a\n....")).extension == ".png");
    CHECK(detect_type(B("%PDF-1.7\n")).mime == "application/pdf");
    CHECK(detect_type(B("PK\x03\x04....")).extension == ".zip");
    CHECK(detect_type(B("....ftypisom")).extension == ".mp4");
    CHECK(detect_type(B("  {\"a\": 1}")).extension == ".json");
    CHECK(detect_type(B("<svg xmlns='http://www.w3.org/2000/svg'></svg>")).extension == ".svg");
    CHECK(detect_type(B("hello, plate 1")).extension == ".txt");
    CHECK(detect_type(Bytes{0, 1, 2, 3, 255}).extension == ".bin");
    CHECK(extension_for_mime("image/jpeg; q=1") == ".jpg");

    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / ("b64-naming-" + std::to_string(std::random_device{}()));
    fs::create_directories(dir);
    const fs::path report = dir / "report.pdf";
    std::ofstream(report) << "x";
    CHECK(encoded_path_for(report).filename() == "report.pdf.base64.txt");
    std::ofstream(dir / "report.pdf.base64.txt") << "eA==";
    CHECK(encoded_path_for(report).filename() == "report (1).pdf.base64.txt");
    CHECK(restored_path_for(dir / "report.pdf.base64.txt", "", DetectedType{}).filename() == "report (1).pdf");
    CHECK(restored_path_for(dir / "logo.png.b64", "", DetectedType{}).filename() == "logo.png");
    CHECK(restored_path_for(dir / "logo.b64", "image/png", DetectedType{}).filename() == "logo.png");
    CHECK(restored_path_for(dir / "blob.txt", "", detect_type(B("%PDF-"))).filename() == "blob.pdf");
    CHECK(restored_path_for(dir / "blob.txt", "", DetectedType{}).filename() == "blob.bin");
    CHECK(looks_like_base64_file("a.b64") && looks_like_base64_file("a.pdf.base64.txt") && !looks_like_base64_file("a.pdf"));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

static void sha256_vectors()
{
    CHECK(sha256_hex(nullptr, 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::string abc = "abc";
    CHECK(sha256_hex(reinterpret_cast<const std::uint8_t *>(abc.data()), abc.size()) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const std::string two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(sha256_hex(reinterpret_cast<const std::uint8_t *>(two.data()), two.size()) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    Sha256 split;
    split.update(reinterpret_cast<const std::uint8_t *>(two.data()), 10);
    split.update(reinterpret_cast<const std::uint8_t *>(two.data()) + 10, two.size() - 10);
    CHECK(hex(split.finish()) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

int main()
{
    rfc4648_vectors();
    alphabets_padding_and_wrapping();
    strict_and_lenient_errors();
    streaming_matches_one_shot();
    text_encodings();
    data_urls();
    detection_and_naming();
    sha256_vectors();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("base64 codec: all checks passed\n");
    return 0;
}
