// Pure-model test for the LAN model drop station (docs/features/application-integration/lan-model-drop.md).
// Build and run from the repository root (one line):
//   g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/lan_model_drop/lan_model_drop_model_test.cpp
//       src/slic3r/GUI/LanModelDrop/LanModelDropModel.cpp -o lan_model_drop_model_test
//   ./lan_model_drop_model_test
// tests/lan_model_drop/model_native.test.mjs does exactly that when g++ is present.
#undef NDEBUG
#include "../../src/slic3r/GUI/LanModelDrop/LanModelDropModel.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

using namespace Slic3r::GUI::LanModelDrop;

static int assertions = 0;
#define CHECK(expr) do { assert(expr); ++assertions; } while (0)

static const std::string kId1 = "0123456789abcdef0123456789abcdef";
static const std::string kId2 = "fedcba9876543210fedcba9876543210";
static const std::string kSha = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

// Dotted IPv4 text from four octets.
static std::string dotted(int a, int b, int c, int d)
{
    return std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c) + "." + std::to_string(d);
}

static std::string item_json(const std::string &id, const std::string &name, const std::string &type, const std::string &extra = "")
{
    return "{\"id\":\"" + id + "\",\"fileName\":\"" + name + "\",\"sender\":\"Ada\",\"bytes\":1234,\"sha256\":\"" + kSha +
           "\",\"receivedAt\":\"2026-10-09T08:15:00Z\",\"type\":\"" + type + "\"" + extra + "}";
}

static void types()
{
    CHECK(type_from_name("3mf") == FileType::ThreeMF);
    CHECK(type_from_name("step") == FileType::Step);
    CHECK(!type_from_name("stp"));
    CHECK(!type_from_name("STL"));
    CHECK(!type_from_name("exe"));
    CHECK(std::string(type_name(FileType::Obj)) == "obj");
    CHECK(type_from_extension("Bracket.STL") == FileType::Stl);
    CHECK(type_from_extension("part.stp") == FileType::Step);
    CHECK(type_from_extension("part.StEp") == FileType::Step);
    CHECK(type_from_extension("plate.3mf") == FileType::ThreeMF);
    CHECK(type_from_extension("mesh.obj") == FileType::Obj);
    CHECK(type_from_extension("old.amf") == FileType::Amf);
    for (const char *bad : {"model.stl.exe", "model", "model.gcode", "model.3mf.lnk", "stl", "model.st"})
        CHECK(!type_from_extension(bad));

    const std::string zip("PK\x03\x04rest", 8);
    CHECK(content_matches(FileType::ThreeMF, zip));
    CHECK(!content_matches(FileType::ThreeMF, "solid x facet"));
    CHECK(content_matches(FileType::Step, "ISO-10303-21;\nHEADER;"));
    CHECK(!content_matches(FileType::Step, " ISO-10303-21;"));
    CHECK(content_matches(FileType::Amf, "<?xml version=\"1.0\"?>\n<amf unit=\"mm\">"));
    CHECK(content_matches(FileType::Amf, zip));
    CHECK(!content_matches(FileType::Amf, "<xml/>"));
    CHECK(content_matches(FileType::Stl, "solid cube\n facet normal 0 0 1\n"));
    CHECK(!content_matches(FileType::Stl, "solid cube\n"));
    // Binary STL: 80-byte header, little-endian triangle count, 50 bytes per triangle.
    std::string binary(84 + 2 * 50, '\0');
    binary[80] = 2;
    CHECK(content_matches(FileType::Stl, binary));
    binary.push_back('\0');
    CHECK(!content_matches(FileType::Stl, binary));
    CHECK(content_matches(FileType::Obj, "# comment\nv 0 0 0\nv 1 0 0\nf 1 2 3\n"));
    CHECK(content_matches(FileType::Obj, "v 1 2 3\n"));
    CHECK(!content_matches(FileType::Obj, "# only a comment\n"));
    CHECK(!content_matches(FileType::Obj, std::string("v 1 2 3\n\0", 9)));
}

static void names()
{
    CHECK(sanitize_file_name("Bracket.stl") == "Bracket.stl");
    CHECK(sanitize_file_name("C:\\Users\\ada\\Desktop\\Bracket.stl") == "Bracket.stl");
    CHECK(sanitize_file_name("../../etc/passwd.stl") == "passwd.stl");
    CHECK(sanitize_file_name("a/b\\c/../d.3mf") == "d.3mf");
    CHECK(sanitize_file_name("  spaced  name .obj  ") == "spaced  name .obj");
    CHECK(sanitize_file_name("we<i>rd:\"na|me?*.step") == "weirdname.step");
    CHECK(sanitize_file_name(std::string("tab\there\x01\x7f.stl")) == "tabhere.stl");
    CHECK(sanitize_file_name("bad.stl.") == "bad.stl");
    CHECK(sanitize_file_name("CON.stl") == "_CON.stl");
    CHECK(sanitize_file_name("lpt1.3mf") == "_lpt1.3mf");
    CHECK(sanitize_file_name("com0.3mf") == "com0.3mf");
    CHECK(sanitize_file_name("console.stl") == "console.stl");
    CHECK(sanitize_file_name("nul.backup.obj") == "_nul.backup.obj");
    CHECK(sanitize_file_name(u8"齒輪 支架.stl") == u8"齒輪 支架.stl");
    // A right-to-left override could make "model\u202Elts.exe" look like something else.
    CHECK(sanitize_file_name(u8"evil\u202Egnp.stl") == "evilgnp.stl");
    for (const char *bad : {"", ".stl", "   .stl", "model.exe", "model", "dir/", "x.stl.exe", "<>.stl"})
        CHECK(sanitize_file_name(bad).empty());
    CHECK(sanitize_file_name(std::string("bad\xff.stl")).empty());     // invalid UTF-8
    CHECK(sanitize_file_name(std::string("over\xc0\xafx.stl")).empty()); // overlong '/'
    CHECK(sanitize_file_name(std::string("half\xe6\x97.stl")).empty());  // truncated sequence
    // At most 200 characters, extension kept.
    const std::string long_stem(300, 'a');
    const std::string trimmed = sanitize_file_name(long_stem + ".step");
    CHECK(trimmed.size() == 200);
    CHECK(trimmed.substr(trimmed.size() - 5) == ".step");
    std::string wide;
    for (int i = 0; i < 250; ++i) wide += u8"齒";
    const std::string wide_trimmed = sanitize_file_name(wide + ".stl");
    CHECK(wide_trimmed.size() == 196 * 3 + 4); // 196 characters of stem plus ".stl"

    CHECK(sanitize_sender("  Ada  ") == "Ada");
    CHECK(sanitize_sender(std::string("A\x07" "da\n")) == "Ada");
    CHECK(sanitize_sender(u8"陳大文") == u8"陳大文");
    CHECK(sanitize_sender(u8"x\u202Ey") == "xy");
    CHECK(sanitize_sender(std::string(60, 'z')) == std::string(40, 'z'));
    CHECK(sanitize_sender(std::string("ok\xff")) == "ok");
    CHECK(sanitize_sender("").empty());

    CHECK(is_item_id(kId1));
    CHECK(!is_item_id("0123456789ABCDEF0123456789ABCDEF"));
    CHECK(!is_item_id(kId1.substr(1)));
    CHECK(!is_item_id("../../../../../../../../etc/pass"));
    CHECK(is_sha256_hex(kSha));
    CHECK(!is_sha256_hex(kSha + "0"));
    CHECK(is_utc_stamp("2026-10-09T08:15:00Z"));
    CHECK(!is_utc_stamp("2026-10-09T08:15:00"));
    CHECK(!is_utc_stamp("2026-02-30T08:15:00Z"));
    CHECK(is_station_key("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQ"));
    CHECK(!is_station_key("short"));
    CHECK(!is_station_key("has a space inside it"));
    CHECK(!is_station_key("line\r\nInjected: header"));
    CHECK(!is_station_key(std::string(513, 'k')));
    CHECK(is_drop_code("123456"));
    CHECK(!is_drop_code(""));
    CHECK(!is_drop_code("12 34"));
    const unsigned char raw[] = {0x00, 0x0f, 0xa5, 0xff};
    CHECK(to_hex(raw, sizeof(raw)) == "000fa5ff");
}

static void inbox()
{
    const std::string body = "{\"items\":[" + item_json(kId1, "Bracket.stl", "stl") + "," + item_json(kId2, "plate.3mf", "3mf") + "]}";
    const InboxListing listing = parse_inbox(body);
    CHECK(listing.ok);
    CHECK(listing.items.size() == 2);
    CHECK(listing.items[0].id == kId1);
    CHECK(listing.items[0].file_name == "Bracket.stl");
    CHECK(listing.items[0].sender == "Ada");
    CHECK(listing.items[0].bytes == 1234);
    CHECK(listing.items[0].type == FileType::Stl);
    CHECK(listing.items[1].type == FileType::ThreeMF);
    CHECK(listing.invalid_ids.empty() && listing.unusable == 0);

    // .stp is the step type.
    CHECK(parse_inbox("{\"items\":[" + item_json(kId1, "part.stp", "step") + "]}").items.size() == 1);
    // Sender may be absent or null.
    CHECK(parse_inbox("{\"items\":[{\"id\":\"" + kId1 + "\",\"fileName\":\"a.obj\",\"bytes\":5,\"sha256\":\"" + kSha +
                      "\",\"receivedAt\":\"2026-10-09T08:15:00Z\",\"type\":\"obj\"}]}").items.size() == 1);
    CHECK(parse_inbox("{\"items\":[" + item_json(kId1, "a.obj", "obj").replace(item_json(kId1, "a.obj", "obj").find("\"Ada\""), 5, "null") + "]}")
              .items.size() == 1);

    // Each bad field makes the entry invalid, but its id is still reported so it can be removed.
    const std::vector<std::string> bad = {
        item_json(kId1, "Bracket.stl", "3mf"),                 // declared type disagrees with the extension
        item_json(kId1, "Bracket.exe", "stl"),                 // not an accepted extension
        item_json(kId1, "Bracket.stl", "zip"),                 // unknown type
        "{\"id\":\"" + kId1 + "\",\"fileName\":\"a.stl\",\"sender\":\"Ada\",\"bytes\":0,\"sha256\":\"" + kSha +
            "\",\"receivedAt\":\"2026-10-09T08:15:00Z\",\"type\":\"stl\"}", // empty
        "{\"id\":\"" + kId1 + "\",\"fileName\":\"a.stl\",\"sender\":\"Ada\",\"bytes\":-4,\"sha256\":\"" + kSha +
            "\",\"receivedAt\":\"2026-10-09T08:15:00Z\",\"type\":\"stl\"}", // negative
        "{\"id\":\"" + kId1 + "\",\"fileName\":\"a.stl\",\"sender\":\"Ada\",\"bytes\":3000000000,\"sha256\":\"" + kSha +
            "\",\"receivedAt\":\"2026-10-09T08:15:00Z\",\"type\":\"stl\"}", // above the station cap
        "{\"id\":\"" + kId1 + "\",\"fileName\":\"a.stl\",\"sender\":\"Ada\",\"bytes\":\"12\",\"sha256\":\"" + kSha +
            "\",\"receivedAt\":\"2026-10-09T08:15:00Z\",\"type\":\"stl\"}", // size as text
        "{\"id\":\"" + kId1 + "\",\"fileName\":\"a.stl\",\"sender\":\"Ada\",\"bytes\":12,\"sha256\":\"XYZ\",\"receivedAt\":\"2026-10-09T08:15:00Z\",\"type\":\"stl\"}",
        "{\"id\":\"" + kId1 + "\",\"fileName\":\"a.stl\",\"sender\":\"Ada\",\"bytes\":12,\"sha256\":\"" + kSha +
            "\",\"receivedAt\":\"yesterday\",\"type\":\"stl\"}",
        "{\"id\":\"" + kId1 + "\",\"fileName\":\"a.stl\",\"sender\":7,\"bytes\":12,\"sha256\":\"" + kSha +
            "\",\"receivedAt\":\"2026-10-09T08:15:00Z\",\"type\":\"stl\"}", // sender not text
    };
    for (const std::string &entry : bad) {
        const InboxListing one = parse_inbox("{\"items\":[" + entry + "]}");
        CHECK(one.ok);
        CHECK(one.items.empty());
        CHECK(one.invalid_ids.size() == 1 && one.invalid_ids[0] == kId1);
    }
    // Entries without a usable id cannot be addressed at all; duplicates are not processed twice.
    const InboxListing unusable = parse_inbox("{\"items\":[" + item_json("NOT-AN-ID", "a.stl", "stl") + ",42," +
                                              item_json(kId1, "a.stl", "stl") + "," + item_json(kId1, "b.stl", "stl") + "]}");
    CHECK(unusable.ok && unusable.items.size() == 1 && unusable.unusable == 3);
    // Not an inbox answer at all.
    for (const char *body_text : {"", "[]", "{}", "{\"items\":{}}", "<html>", "{\"items\":[", "null"})
        CHECK(!parse_inbox(body_text).ok);
    CHECK(parse_inbox("{\"items\":[]}").ok);
}

static void status_messages()
{
    const std::string good = "{\"protocol\":1,\"stationName\":\"Bambu Studio\",\"dropCode\":\"123456\",\"queued\":2,"
                             "\"queuedBytes\":4096,\"maxBytes\":268435456,\"ttlHours\":24}";
    const auto status = parse_status(good);
    CHECK(status && status->drop_code == "123456" && status->station_name == "Bambu Studio" && status->queued == 2 &&
          status->queued_bytes == 4096 && status->max_bytes == 268435456ULL && status->ttl_hours == 24.0 &&
          status->public_url.empty());
    // DROP_TTL_HOURS accepts fractions; publicUrl is optional, may be null, and is normalized.
    const std::string head = "{\"protocol\":1,\"stationName\":\"Bambu Studio\",\"dropCode\":\"123456\",\"queued\":0,"
                             "\"queuedBytes\":0,\"maxBytes\":1024,";
    CHECK(parse_status(head + "\"ttlHours\":0.5}") && parse_status(head + "\"ttlHours\":0.5}")->ttl_hours == 0.5);
    CHECK(!parse_status(head + "\"ttlHours\":0}") && !parse_status(head + "\"ttlHours\":-1}") && !parse_status(head + "\"ttlHours\":\"24\"}"));
    CHECK(parse_status(head + "\"ttlHours\":24,\"publicUrl\":null}")->public_url.empty());
    CHECK(parse_status(head + "\"ttlHours\":24,\"publicUrl\":\"https://Drop.Example.org/models/\"}")->public_url ==
          "https://drop.example.org/models");
    CHECK(parse_status(head + "\"ttlHours\":24,\"publicUrl\":\"https://user:pw@drop.example.org\"}")->public_url.empty());
    CHECK(!parse_status("{\"protocol\":2,\"stationName\":\"x\",\"dropCode\":\"1\",\"queued\":0,\"queuedBytes\":0,\"maxBytes\":1,\"ttlHours\":1}"));
    CHECK(!parse_status("{\"stationName\":\"x\",\"dropCode\":\"1\",\"queued\":0,\"queuedBytes\":0,\"maxBytes\":1,\"ttlHours\":1}"));
    CHECK(!parse_status("{\"protocol\":1,\"stationName\":\"x\",\"dropCode\":\"\",\"queued\":0,\"queuedBytes\":0,\"maxBytes\":1,\"ttlHours\":1}"));
    CHECK(!parse_status("{\"protocol\":\"1\",\"stationName\":\"x\",\"dropCode\":\"1\",\"queued\":0,\"queuedBytes\":0,\"maxBytes\":1,\"ttlHours\":1}"));
    CHECK(!parse_status("not json"));

    CHECK(parse_drop_code("{\"dropCode\":\"654321\"}") == std::optional<std::string>("654321"));
    CHECK(!parse_drop_code("{\"ok\":false,\"error\":\"fixed_code\"}"));
    CHECK(parse_health("{\"ok\":true,\"service\":\"lan-model-drop\",\"protocol\":1}"));
    CHECK(!parse_health("{\"ok\":true,\"service\":\"something-else\",\"protocol\":1}"));
    CHECK(!parse_health("{\"ok\":true,\"service\":\"lan-model-drop\",\"protocol\":2}"));

    StationStatus filled;
    CHECK(classify_status({true, 200, good}, &filled) == LinkState::Connected && filled.drop_code == "123456");
    CHECK(classify_status({false, 0, ""}) == LinkState::NotReachable);
    CHECK(classify_status({true, 401, "{\"ok\":false}"}) == LinkState::WrongStationKey);
    CHECK(classify_status({true, 403, ""}) == LinkState::WrongStationKey);
    CHECK(classify_status({true, 404, "Not found"}) == LinkState::ProtocolNotSupported);
    CHECK(classify_status({true, 302, ""}) == LinkState::ProtocolNotSupported);
    CHECK(classify_status({true, 200, "<html>router login</html>"}) == LinkState::ProtocolNotSupported);
    CHECK(classify_status({true, 200, "{\"protocol\":2}"}) == LinkState::ProtocolNotSupported);
    CHECK(classify_status({true, 502, ""}) == LinkState::NotReachable);
    CHECK(classify_failure({true, 401, ""}) == LinkState::WrongStationKey);
    CHECK(classify_failure({false, 0, ""}) == LinkState::NotReachable);
    CHECK(classify_failure({true, 400, ""}) == LinkState::ProtocolNotSupported);
    CHECK(is_error_state(LinkState::NotReachable) && is_error_state(LinkState::NeedsStationKey));
    CHECK(!is_error_state(LinkState::Connected) && !is_error_state(LinkState::Off) && !is_error_state(LinkState::Checking));

    const std::string headers = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nx-content-sha256:  " + kSha +
                                " \r\nContent-Length: 5\r\n\r\n";
    CHECK(header_value(headers, "X-Content-SHA256") == kSha);
    CHECK(header_value(headers, "content-length") == "5");
    CHECK(header_value(headers, "X-Missing").empty());
    CHECK(header_value("X-Content-SHA256-Extra: 1\r\n", "X-Content-SHA256").empty());
    CHECK(header_value("A: 1\r\nA: 2\r\n", "a") == "2");

    CHECK(next_poll_delay_seconds(0) == 5);
    CHECK(next_poll_delay_seconds(-3) == 5);
    CHECK(next_poll_delay_seconds(1) == 10);
    CHECK(next_poll_delay_seconds(2) == 20);
    CHECK(next_poll_delay_seconds(3) == 40);
    CHECK(next_poll_delay_seconds(4) == 60);
    CHECK(next_poll_delay_seconds(1000) == 60);
}

static void addresses()
{
    const auto local = parse_base_address(kDefaultAddress);
    CHECK(local && local->base == "http://localhost:8833" && local->port == 8833 && local->loopback && local->explicit_port);
    CHECK(inbox_url(*local) == "http://localhost:8833/api/station/inbox");
    CHECK(status_url(*local) == "http://localhost:8833/api/station/status");
    CHECK(health_url(*local) == "http://localhost:8833/healthz");
    CHECK(drop_code_url(*local) == "http://localhost:8833/api/station/drop-code");
    CHECK(file_url(*local, kId1) == "http://localhost:8833/api/station/files/" + kId1);
    CHECK(file_url(*local, "../secret").empty());

    // Documentation-range addresses (RFC 5737) where any address will do; private ranges are built
    // with dotted() so no fixed private address appears in this public file.
    const auto lan = parse_base_address("  192.0.2.20:8833/ ");
    CHECK(lan && lan->scheme == "http" && lan->base == "http://192.0.2.20:8833" && !lan->loopback);
    const auto https = parse_base_address("HTTPS://Drop.Example.LAN");
    CHECK(https && https->base == "https://drop.example.lan" && https->port == 443 && !https->explicit_port);
    const auto v6 = parse_base_address("http://[::1]:8833");
    CHECK(v6 && v6->loopback && v6->base == "http://[::1]:8833");
    CHECK(parse_base_address("http://127.0.0.1:9000")->loopback);
    for (const char *bad : {"", "ftp://host", "http://", "http://user:pass@host", "http://host/path", "http://host?x=1",
                            "http://host:0", "http://host:65536", "http://host:", "http://ho st", "http://-host",
                            "http://host:80:81", "http://[zz]:1", "javascript:alert(1)", "http://host#frag", "http://a..b"})
        CHECK(!parse_base_address(bad));

    CHECK(is_ipv4("192.0.2.20") && !is_ipv4("192.0.2") && !is_ipv4("192.0.2.256") && !is_ipv4("01.2.3.4"));
    CHECK(is_private_ipv4(dotted(10, 0, 0, 5)) && is_private_ipv4(dotted(172, 20, 1, 1)) && is_private_ipv4(dotted(192, 168, 0, 9)));
    CHECK(!is_private_ipv4(dotted(172, 32, 0, 1)) && !is_private_ipv4("198.51.100.7") && !is_private_ipv4("192.0.2.20"));
    CHECK(looks_virtual_adapter("vEthernet (WSL)") && looks_virtual_adapter("VirtualBox Host-Only") && !looks_virtual_adapter("Wi-Fi"));

    const std::string lan_ip = dotted(192, 168, 1, 20);
    std::vector<AdapterAddress> adapters = {
        {"127.0.0.1", "Loopback Pseudo-Interface 1", true, true, false},
        {"169.254.10.10", "Ethernet 2", true, false, false},
        {dotted(172, 28, 48, 1), "vEthernet (WSL)", true, false, false},
        {lan_ip, "Wi-Fi", true, false, true},
        {dotted(10, 8, 0, 2), "Ethernet", false, false, true},
    };
    CHECK(choose_lan_ipv4(adapters) == lan_ip);
    // Loopback, link-local and down adapters are never offered; the WSL switch is private, so it stays
    // as a lower choice.
    CHECK((lan_ipv4_candidates(adapters) == std::vector<std::string>{lan_ip, dotted(172, 28, 48, 1)}));
    adapters[3].up = false;
    CHECK(choose_lan_ipv4(adapters) == dotted(172, 28, 48, 1));
    CHECK(choose_lan_ipv4({}).empty());
    CHECK(choose_lan_ipv4({{"127.0.0.1", "lo", true, true, false}}).empty());
    // A public address is never offered, even on the only adapter with a gateway.
    CHECK(lan_ipv4_candidates({{"198.51.100.7", "Ethernet", true, false, true}}).empty());
    CHECK(lan_ipv4_candidates({{"224.0.0.1", "Ethernet", true, false, true}, {dotted(100, 64, 0, 1), "Ethernet", true, false, true}}).empty());
    // The same address on two adapters is listed once.
    CHECK(lan_ipv4_candidates({{lan_ip, "Wi-Fi", true, false, true}, {lan_ip, "Wi-Fi 2", true, false, false}}).size() == 1);
    CHECK(parse_base_address("http://0.0.0.0:8833")->loopback && parse_base_address("http://[::]:8833")->loopback);
}

static void invites()
{
    const auto local = *parse_base_address(kDefaultAddress);
    const auto lan   = *parse_base_address("http://192.0.2.20:8833");
    const std::string first  = dotted(192, 168, 1, 20);
    const std::string second = dotted(10, 0, 0, 7);

    CHECK(normalize_public_url("https://drop.example.org") == "https://drop.example.org");
    CHECK(normalize_public_url("HTTPS://Drop.Example.org:8443/a/b/") == "https://drop.example.org:8443/a/b");
    CHECK(normalize_public_url("http://drop.example.org/%7Eme") == "http://drop.example.org/%7Eme");
    for (const char *bad : {"", "drop.example.org", "ftp://drop.example.org", "https://user@drop.example.org",
                            "https://drop.example.org/?code=1", "https://drop.example.org/#code=1", "javascript:alert(1)",
                            "https://drop.example.org/a b", "https://drop.example.org/%zz", "https://drop.example.org//x",
                            "https://", "https://drop.example.org:99999"})
        CHECK(normalize_public_url(bad).empty());

    // 1. The container's public URL wins over everything.
    InviteBase invite = choose_invite_base("https://drop.example.org/models", local, {first}, "");
    CHECK(invite.source == InviteSource::PublicUrl && invite.base == "https://drop.example.org/models" && invite.choices.empty());
    // An unusable public URL is skipped, never shown.
    CHECK(choose_invite_base("https://user:pw@drop.example.org", lan, {first}, "").source == InviteSource::Configured);
    // 2. A configured address that is not this computer's loopback.
    invite = choose_invite_base("", lan, {first, second}, second);
    CHECK(invite.source == InviteSource::Configured && invite.base == "http://192.0.2.20:8833" && invite.choices.empty());
    // 3. This computer's private LAN IPv4 with the configured port, the first candidate by default...
    invite = choose_invite_base("", local, {first, second}, "");
    CHECK(invite.source == InviteSource::LanAddress && invite.base == "http://" + first + ":8833" && invite.chosen == first &&
          (invite.choices == std::vector<std::string>{first, second}));
    // ...or the user's pick while it is still a candidate.
    CHECK(choose_invite_base("", local, {first, second}, second).base == "http://" + second + ":8833");
    CHECK(choose_invite_base("", local, {first, second}, "192.0.2.99").chosen == first);
    // Loopback, link-local and public addresses never become a link base, even when passed in.
    CHECK(choose_invite_base("", local, {"127.0.0.1", "169.254.3.4", "198.51.100.7"}, "127.0.0.1").source == InviteSource::None);
    invite = choose_invite_base("", local, {}, "");
    CHECK(invite.source == InviteSource::None && invite.base.empty() && invite.choices.empty());
    CHECK(choose_invite_base("", *parse_base_address("localhost"), {first}, "").base == "http://" + first);
    CHECK(choose_invite_base("", *parse_base_address("http://0.0.0.0:8833"), {first}, "").source == InviteSource::LanAddress);

    // The code travels in the fragment, encoded like encodeURIComponent.
    CHECK(invite_link("http://" + first + ":8833", "123456") == "http://" + first + ":8833/#code=123456");
    CHECK(invite_link("https://drop.example.org/models", "12+4&5") == "https://drop.example.org/models/#code=12%2B4%265");
    CHECK(invite_link("", "123456").empty());
    CHECK(invite_link("https://drop.example.org", "").empty());
    CHECK(invite_link("https://drop.example.org", "12\n34").empty());
    CHECK(invite_link("https://drop.example.org", "123456").find('?') == std::string::npos);
}

static void presentation_and_tracking()
{
    CHECK(format_size(0) == "0 B");
    CHECK(format_size(1023) == "1023 B");
    CHECK(format_size(1536) == "1.5 KB");
    CHECK(format_size(5ULL * 1024 * 1024) == "5.0 MB");
    CHECK(format_size(3ULL * 1024 * 1024 * 1024 / 2) == "1.50 GB");
    CHECK(distinct_text("Ada sent a.stl (1.0 KB)", {}) == "Ada sent a.stl (1.0 KB)");
    CHECK(distinct_text("x", {"x"}) == "x (2)");
    CHECK(distinct_text("x", {"x", "x (2)"}) == "x (3)");

    InboxItem a, b;
    a.id = kId1;
    b.id = kId2;
    InboxTracker tracker;
    CHECK(tracker.fresh({a, b}).size() == 2);
    tracker.take(kId1);
    CHECK(tracker.fresh({a, b}).size() == 1 && tracker.fresh({a, b})[0].id == kId2);
    // A network failure frees the item for a later poll, up to the attempt limit.
    CHECK(tracker.retry_later(kId1));
    CHECK(tracker.fresh({a}).size() == 1);
    tracker.take(kId1);
    CHECK(tracker.retry_later(kId1));
    tracker.take(kId1);
    CHECK(!tracker.retry_later(kId1));
    CHECK(tracker.taken(kId1));
    // Deletes stay pending until confirmed, and an item waiting for removal is never fetched again.
    tracker.queue_delete(kId2);
    CHECK(tracker.fresh({a, b}).empty());
    CHECK(tracker.pending_deletes() == std::vector<std::string>{kId2});
    tracker.prune({});
    CHECK(tracker.pending_deletes().size() == 1 && tracker.taken(kId2) && !tracker.taken(kId1));
    tracker.delete_confirmed(kId2);
    CHECK(tracker.pending_deletes().empty());
    tracker.prune({});
    CHECK(!tracker.taken(kId2));
    tracker.queue_delete("not-an-id");
    CHECK(tracker.pending_deletes().empty());
}

int main()
{
    types();
    names();
    inbox();
    status_messages();
    addresses();
    invites();
    presentation_and_tracking();
    std::cout << "lan_model_drop_model_test: " << assertions << " assertions passed\n";
    return 0;
}
