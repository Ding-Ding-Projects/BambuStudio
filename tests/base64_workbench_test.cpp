// Standalone assertions for src/slic3r/GUI/Base64Toolkit/Base64Workbench.cpp,
// the Base64 toolkit page's logic without wxWidgets. Compiled and run by
// tests/base64_workbench.test.mjs.
#include "src/slic3r/GUI/Base64Toolkit/Base64Codec.cpp"
#include "src/slic3r/GUI/Base64Toolkit/Base64Workbench.cpp"

#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>

using namespace Slic3r::GUI::Base64Toolkit;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); ++failures; } \
    } while (0)

// "bracket_v3 · plate 1 · 220°C", the board's sample: 28 characters, 31 bytes.
static const std::string kSample = "bracket_v3 \xC2\xB7 plate 1 \xC2\xB7 220\xC2\xB0" "C";

static std::string read_file(const fs::path &path)
{
    std::ifstream      in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

static void encoding_text()
{
    TextOptions options;
    const TextOutcome out = run_text(kSample, options);
    CHECK(out.problem == TextProblem::None);
    CHECK(out.input_characters == 28);
    CHECK(out.bytes.size() == 31);
    CHECK(out.output == "YnJhY2tldF92MyDCtyBwbGF0ZSAxIMK3IDIyMMKwQw==");
    CHECK(out.output_characters == 44);
    CHECK(out.verified);
    CHECK(out.sha256 == sha256_hex(out.bytes.data(), out.bytes.size()));
    CHECK(hex_preview(out.bytes, 8) == "62 72 61 63 6b 65 74 5f \xE2\x80\xA6");
    CHECK(hex_preview(out.bytes, 64).rfind("c2 b0 43") == hex_preview(out.bytes, 64).size() - 8);

    options.encoding = TextEncoding::Utf16LE;
    CHECK(run_text("hi", options).output == "aABpAA==");

    options.encoding = TextEncoding::Latin1;
    const TextOutcome latin = run_text("caf\xC3\xA9 \xE2\x98\x95", options); // "café ☕"
    CHECK(latin.problem == TextProblem::InputEncoding);
    CHECK(latin.offset == 5);
    CHECK(latin.character == 0x2615);

    TextOptions mime;
    mime.lines = LineMode::Mime;
    const std::string wrapped = run_text(std::string(100, 'A'), mime).output;
    CHECK(wrapped.find("\r\n") == 76);
    TextOptions custom;
    custom.lines        = LineMode::Custom;
    custom.custom_width = 2; // clamped to 4
    CHECK(encode_options(custom).line_length == 4);
    CHECK(run_text("hello", custom).output == "aGVs\nbG8=");
    TextOptions url;
    url.alphabet = Alphabet::UrlSafe;
    url.padding  = false;
    CHECK(run_text("?\?>", url).output == "Pz8-"); // "??>", escaped so no compiler reads a trigraph
    CHECK(run_text("\xFB\xFF", TextOptions{}).problem == TextProblem::InputEncoding); // not UTF-8
}

static void decoding_text()
{
    TextOptions options;
    options.encode = false;
    const TextOutcome good = run_text("YnJhY2tldF92MyDCtyBwbGF0ZSAxIMK3IDIyMMKwQw==", options);
    CHECK(good.problem == TextProblem::None);
    CHECK(good.output == kSample);
    CHECK(good.output_characters == 28);
    CHECK(good.canonical && good.verified);

    // The board's strict error: '!' at offset 16.
    const std::string broken = "YnJhY2tldF92MyDC!yBwbGF0ZQ==";
    const TextOutcome bad    = run_text(broken, options);
    CHECK(bad.problem == TextProblem::Decode);
    CHECK(bad.decode_error == DecodeError::InvalidCharacter);
    CHECK(bad.offset == 16 && bad.character == '!');
    const Excerpt e = excerpt_around(broken, bad.offset, 16);
    CHECK(e.before == "YnJhY2tldF92MyDC" && e.at == "!" && e.after == "yBwbGF0ZQ==");
    CHECK(!e.clipped_before && !e.clipped_after);
    CHECK(remove_at(broken, 16) == "YnJhY2tldF92MyDCyBwbGF0ZQ==");
    CHECK(remove_at(broken, 999) == broken);

    // Lenient accepts missing padding; canonical says whether it was the exact form.
    options.strict = false;
    const TextOutcome unpadded = run_text("aGVsbG8", options);
    CHECK(unpadded.problem == TextProblem::None && unpadded.output == "hello" && !unpadded.canonical);
    options.padding = false;
    CHECK(run_text("aGVsbG8", options).canonical);

    // Bytes that are not text keep the bytes and say where the text breaks.
    TextOptions plain;
    plain.encode = false;
    const TextOutcome binary = run_text("AP8=", plain);
    CHECK(binary.problem == TextProblem::OutputText);
    CHECK(binary.bytes == (Bytes{0x00, 0xFF}));
    CHECK(binary.offset == 1);
    plain.encoding = TextEncoding::Utf16LE;
    CHECK(run_text("aABpAA==", plain).output == "hi");

    CHECK(run_text("", plain).problem == TextProblem::Empty);
    CHECK(run_text(std::string(kTextLimit + 1, 'A'), plain).problem == TextProblem::TooLarge);
}

static void previews_and_sizes()
{
    const std::string dump = hex_dump(Bytes{'b', 'r', 'a', 'c', 'k', 'e', 't', '_', 'v', '3'}, 4096);
    CHECK(dump.rfind("00000000  62 72 61 63 6b 65 74 5f  76 33 ", 0) == 0);
    CHECK(dump.size() > 14 && dump.compare(dump.size() - 14, 14, " |bracket_v3|\n") == 0);
    Bytes many(40, 0x41);
    const std::string two = hex_dump(many, 20);
    CHECK(two.find("00000010  41 41 41 41") != std::string::npos);
    CHECK(two.find("\xE2\x80\xA6") != std::string::npos);

    const Excerpt lines = excerpt_around("ab\ncd", 2, 8);
    CHECK(lines.at == "\xE2\x86\xB5" && lines.before == "ab" && lines.after == "cd");
    const Excerpt end = excerpt_around("abcd", 4, 2);
    CHECK(end.at.empty() && end.before == "cd" && end.clipped_before);

    CHECK(human_size(1) == "1 byte");
    CHECK(human_size(31) == "31 bytes");
    CHECK(human_size(2867) == "2.8 KB");
    CHECK(human_size(3072) == "3 KB");
    CHECK(human_size(88214) == "86 KB");
    CHECK(human_size(1447034) == "1.38 MB");
    CHECK(human_size(2587708620ULL) == "2.41 GB");
    CHECK(short_hash("3f1c00000000000000000000000000000000000000000000000000000000a20e") == "3f1c\xE2\x80\xA6" "a20e");
    CHECK(short_hash("abc") == "abc");
    CHECK(percent(63, 100) == 63 && percent(5, 0) == 0 && percent(7, 5) == 100);
    CHECK(seconds_left(50, 100, 10.0) == std::optional<int>(10));
    CHECK(!seconds_left(0, 100, 10.0) && !seconds_left(50, 100, 0.5) && !seconds_left(100, 100, 9.0));
}

static void undo_and_redo()
{
    UndoStack stack;
    TextState a{"a", {}}, b{"b", {}}, c{"c", {}};
    stack.commit(a);
    stack.commit(a); // equal to the current state: ignored
    stack.commit(b);
    stack.commit(c);
    CHECK(stack.size() == 3 && stack.can_undo() && !stack.can_redo());
    CHECK(stack.undo() == std::optional<TextState>(b));
    CHECK(stack.undo() == std::optional<TextState>(a));
    CHECK(!stack.undo());
    CHECK(stack.redo() == std::optional<TextState>(b));
    TextState d{"d", {}};
    d.options.encode = false;
    stack.commit(d); // drops c, the redo branch
    CHECK(!stack.can_redo() && stack.size() == 3);
    CHECK(stack.undo() == std::optional<TextState>(b));
    UndoStack full;
    for (int i = 0; i < 60; ++i) full.commit(TextState{std::to_string(i), {}});
    CHECK(full.size() == UndoStack::kLimit);
}

static void history_keeps_payloads_only_when_asked()
{
    const fs::path dir  = fs::temp_directory_path() / ("b64-history-" + std::to_string(std::random_device{}()));
    const fs::path file = dir / "base64-toolkit-history.jsonl";
    History        history(file);
    history.load();
    CHECK(history.entries().empty());

    HistoryEntry quiet;
    quiet.time        = 1762581161;
    quiet.action      = "encode-text";
    quiet.name        = "Text";
    quiet.bytes_in    = 31;
    quiet.bytes_out   = 44;
    quiet.sha256      = "3f1c";
    quiet.has_options = true;
    quiet.has_payload = true;
    quiet.payload     = "SECRET-PAYLOAD-ONE";
    history.append(quiet, false);
    CHECK(read_file(file).find("SECRET-PAYLOAD-ONE") == std::string::npos);
    CHECK(!history.entries().back().has_payload);

    HistoryEntry kept = quiet;
    kept.payload      = "KEPT-PAYLOAD-TWO";
    kept.options.lines = LineMode::Custom;
    kept.options.custom_width = 40;
    history.append(kept, true);
    CHECK(read_file(file).find("KEPT-PAYLOAD-TWO") != std::string::npos);

    std::ofstream(file, std::ios::app) << "not json\n{\"t\":1,\"a\":\"run-something\"}\n";
    History again(file);
    again.load();
    CHECK(again.entries().size() == 2);
    CHECK(again.entries()[1].has_payload && again.entries()[1].payload == "KEPT-PAYLOAD-TWO");
    CHECK(again.entries()[1].has_options && again.entries()[1].options.lines == LineMode::Custom &&
          again.entries()[1].options.custom_width == 40);
    CHECK(again.export_json().find("\"Base64 toolkit history\"") != std::string::npos);

    again.forget_payloads();
    CHECK(read_file(file).find("KEPT-PAYLOAD-TWO") == std::string::npos);
    for (int i = 0; i < 520; ++i) again.append(quiet, false);
    CHECK(again.entries().size() == History::kLimit);
    again.clear();
    CHECK(!fs::exists(file));
    std::error_code ec;
    fs::remove_all(dir, ec);
}

static void options_and_records()
{
    TextOptions o;
    o.encode       = false;
    o.alphabet     = Alphabet::UrlSafe;
    o.lines        = LineMode::Mime;
    o.padding      = false;
    o.strict       = false;
    o.encoding     = TextEncoding::Utf16BE;
    TextOptions back;
    CHECK(parse_options_json(options_json(o), back) && back == o);
    CHECK(!parse_options_json("{\"direction\":\"sideways\"}", back));
    CHECK(!parse_options_json("[", back));

    TextOptions plain;
    const TextOutcome out = run_text("hello", plain);
    const std::string without = record_json(plain, out, 1762581161, nullptr);
    CHECK(without.find("\"payload_included\": false") != std::string::npos);
    CHECK(without.find("\"input\"") == std::string::npos);
    CHECK(without.find("2025-11-08T05:52:41Z") != std::string::npos);
    const std::string input = "hello";
    const std::string with  = record_json(plain, out, 1762581161, &input);
    CHECK(with.find("\"input\": \"hello\"") != std::string::npos && with.find("\"output\": \"aGVsbG8=\"") != std::string::npos);
}

int main()
{
    encoding_text();
    decoding_text();
    previews_and_sizes();
    undo_and_redo();
    history_keeps_payloads_only_when_asked();
    options_and_records();
    if (failures) {
        std::printf("%d failure(s)\n", failures);
        return 1;
    }
    std::printf("base64 workbench: all checks passed\n");
    return 0;
}
