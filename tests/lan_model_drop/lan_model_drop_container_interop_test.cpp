// The LAN model drop station against a real container: every answer captured from lan-model-drop
// running under docker compose (tests/lan_model_drop/container_fixtures, written by
// container_capture.py) goes through the application's own pure model exactly as the station's
// worker hands it over, and every field and verdict is checked against what was sent.
//
// Build and run from the repository root (container_fixtures.test.mjs does this when g++ is present):
//   g++ -std=c++17 -Wall -Wextra -Werror -Isrc tests/lan_model_drop/lan_model_drop_container_interop_test.cpp
//       src/slic3r/GUI/LanModelDrop/LanModelDropModel.cpp src/slic3r/GUI/LanModelDrop/LanModelDropFiles.cpp -o interop
//   ./interop tests/lan_model_drop/container_fixtures <empty scratch folder> [<summary.json>]
#include "../../src/slic3r/GUI/LanModelDrop/LanModelDropFiles.hpp"
#include "../../src/slic3r/GUI/LanModelDrop/LanModelDropModel.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Slic3r::GUI::LanModelDrop;
using json   = nlohmann::json;
namespace fs = std::filesystem;

namespace {

int assertions = 0;

void require(bool condition, const std::string &what)
{
    if (!condition) throw std::runtime_error(what);
    ++assertions;
}

std::string read_file(const fs::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path.u8string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// ------------------------------------------------------------------------------------------------
// SHA-256 (FIPS 180-4), so the digests are computed here and not taken from either side.
// ------------------------------------------------------------------------------------------------

std::string sha256_hex(const std::string &data)
{
    static const std::uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01,
        0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08,
        0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    const auto rotr = [](std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
    std::string message = data;
    const std::uint64_t bits = static_cast<std::uint64_t>(data.size()) * 8;
    message.push_back(static_cast<char>(0x80));
    while (message.size() % 64 != 56) message.push_back('\0');
    for (int i = 7; i >= 0; --i) message.push_back(static_cast<char>((bits >> (i * 8)) & 0xff));
    for (std::size_t block = 0; block < message.size(); block += 64) {
        std::uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            const auto *p = reinterpret_cast<const unsigned char *>(message.data() + block + i * 4);
            w[i] = (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
        }
        for (int i = 16; i < 64; ++i) {
            const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; ++i) {
            const std::uint32_t t1 = hh + (rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i];
            const std::uint32_t t2 = (rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    unsigned char digest[32];
    for (int i = 0; i < 8; ++i)
        for (int j = 0; j < 4; ++j) digest[i * 4 + j] = static_cast<unsigned char>((h[i] >> (24 - j * 8)) & 0xff);
    return to_hex(digest, sizeof(digest));
}

// ------------------------------------------------------------------------------------------------
// One captured exchange, as the station's worker builds it from libcurl: the HTTP status, the raw
// header lines (the header callback's text) and the body.
// ------------------------------------------------------------------------------------------------

struct Captured
{
    Exchange    exchange;
    std::string head;
    std::string label;
};

class Fixtures
{
public:
    explicit Fixtures(fs::path root) : m_root(std::move(root)) {}

    Captured load(const json &record) const
    {
        Captured c;
        c.label = record.at("label").get<std::string>();
        c.head  = read_file(m_root / fs::u8path(record.at("head").get<std::string>()));
        // "HTTP/1.1 200 OK": the status libcurl reports.
        const std::size_t space = c.head.find(' ');
        require(c.head.rfind("HTTP/1.1 ", 0) == 0 && space != std::string::npos, c.label + ": a status line");
        c.exchange.completed   = true;
        c.exchange.http_status = static_cast<unsigned>(std::stoul(c.head.substr(space + 1, 3)));
        c.exchange.body        = read_file(m_root / fs::u8path(record.at("body").get<std::string>()));
        require(c.exchange.http_status == record.at("status").get<unsigned>(), c.label + ": the status the manifest names");
        return c;
    }

    std::string bytes(const std::string &relative) const { return read_file(m_root / fs::u8path(relative)); }

private:
    fs::path m_root;
};

json body_json(const Captured &c)
{
    json value = json::parse(c.exchange.body, nullptr, false);
    require(!value.is_discarded(), c.label + ": a JSON body");
    return value;
}

// Every answer carries the security headers the brief names.
void security_headers(const Captured &c)
{
    require(header_value(c.head, "Content-Security-Policy").rfind("default-src 'self'", 0) == 0, c.label + ": Content-Security-Policy");
    require(header_value(c.head, "Content-Security-Policy").find("frame-ancestors 'none'") != std::string::npos,
            c.label + ": frame-ancestors 'none'");
    require(header_value(c.head, "X-Content-Type-Options") == "nosniff", c.label + ": X-Content-Type-Options");
    require(header_value(c.head, "Referrer-Policy") == "no-referrer", c.label + ": Referrer-Policy");
}

struct Expected
{
    std::string id;
    std::string file_name;
    std::string sender;
    std::string sample; // fixture path of the bytes that were sent
};

json sample_named(const json &manifest, const std::string &key, const std::string &value)
{
    for (const json &sample : manifest.at("samples"))
        if (sample.at(key).get<std::string>() == value) return sample;
    throw std::runtime_error("no sample with " + key + " " + value);
}

// An example private address of this computer, written from its octets.
std::string lan_address() { return std::to_string(192) + "." + std::to_string(168) + "." + std::to_string(1) + "." + std::to_string(20); }

void scenario(const json &manifest, const json &s, const Fixtures &fixtures, const fs::path &scratch, json &summary)
{
    const std::string name = s.at("name").get<std::string>();
    const json        env  = s.at("env");
    const auto setting = [&env](const char *key, const std::string &fallback) {
        return env.contains(key) ? env.at(key).get<std::string>() : fallback;
    };
    const std::string started  = s.at("startedAt").get<std::string>();
    const std::string finished = s.at("finishedAt").get<std::string>();
    require(is_utc_stamp(started) && is_utc_stamp(finished) && started <= finished, name + ": capture times");

    // GET /healthz, as Test connection reads it.
    const Captured health = fixtures.load(s.at("health"));
    security_headers(health);
    require(health.exchange.http_status == 200 && parse_health(health.exchange.body), name + ": the health answer");

    // GET /api/station/status before anything was sent.
    const std::string code = s.at("dropCode").get<std::string>();
    StationStatus     empty;
    const Captured    status_empty = fixtures.load(s.at("statusEmpty"));
    require(classify_status(status_empty.exchange, &empty) == LinkState::Connected, name + ": Connected before any drop");
    require(empty.queued == 0 && empty.queued_bytes == 0 && empty.drop_code == code, name + ": an empty drop box with its code");

    // Every drop: what the drop box answered, and the models the station must list, in order.
    std::vector<Expected> expected;
    if (s.contains("page") && s.at("page").is_object()) {
        const json &page = s.at("page");
        require(page.at("invite").at("code").get<bool>() && page.at("invite").at("fragmentRemoved").get<bool>(),
                name + ": the page read the invite link and removed the fragment");
        // The sender was typed as "  Mo   Li  "; the page sends it with white space runs made one space
        // and trimmed, and the station shows exactly that.
        require(page.at("sender") == "  Mo   Li  ", name + ": the typed sender");
        for (const json &row : page.at("rows")) require(row.at("status") == "sent", name + ": every page row was sent");
        for (const json &drop : page.at("drops")) {
            require(drop.at("status").get<unsigned>() == 201, name + ": the page's drop of " + drop.at("name").get<std::string>());
            const json answer = json::parse(drop.at("responseBody").get<std::string>());
            require(answer.at("ok") == true && is_item_id(answer.at("id").get<std::string>()), name + ": the page got an id");
            const json sample = sample_named(manifest, "name", drop.at("name").get<std::string>());
            expected.push_back({answer.at("id").get<std::string>(), sample.at("name").get<std::string>(), "Mo Li",
                                sample.at("file").get<std::string>()});
        }
    }
    for (const json &drop : s.at("drops")) {
        const Captured answer = fixtures.load(drop);
        security_headers(answer);
        const json &expect = drop.at("expect");
        require(answer.exchange.http_status == expect.at("status").get<unsigned>(), answer.label + ": the expected status");
        const json body = body_json(answer);
        if (answer.exchange.http_status == 201) {
            require(body.at("ok") == true && body.size() == 2 && is_item_id(body.at("id").get<std::string>()), answer.label + ": {ok, id}");
            expected.push_back({body.at("id").get<std::string>(), expect.at("stationFileName").get<std::string>(),
                                expect.at("stationSender").get<std::string>(), expect.at("sample").get<std::string>()});
            // The station's own cleaning of the name as sent never refuses a name the drop box took.
            require(!sanitize_file_name(drop.at("name").get<std::string>()).empty(), answer.label + ": a name the station takes");
        } else {
            require(body.at("ok") == false && body.at("error") == expect.at("error") && body.at("message").is_string(),
                    answer.label + ": {ok:false, error, message}");
            if (expect.at("error") == "bad_request" && drop.at("sender").get<std::string>().empty())
                // A name the drop box refused is one the station would not take either.
                require(sanitize_file_name(drop.at("name").get<std::string>()).empty(), answer.label + ": a name nobody takes");
        }
    }

    // GET /api/station/status after the drops: every field.
    StationStatus  status;
    const Captured status_answer = fixtures.load(s.at("status"));
    require(classify_status(status_answer.exchange, &status) == LinkState::Connected, name + ": Connected");
    std::uint64_t queued_bytes = 0;
    for (const Expected &e : expected) queued_bytes += fixtures.bytes(e.sample).size();
    const std::string station_name = s.contains("expect") ? s.at("expect").at("stationName").get<std::string>() : "Bambu Studio";
    require(status.station_name == station_name, name + ": the station name, in full: " + status.station_name);
    require(status.drop_code == code, name + ": the drop code");
    require(status.queued == expected.size(), name + ": queued = the models accepted");
    require(status.queued_bytes == queued_bytes, name + ": queuedBytes = their bytes");
    require(status.max_bytes == std::stoull(setting("DROP_MAX_BYTES", "268435456")), name + ": maxBytes");
    require(status.ttl_hours == std::stod(setting("DROP_TTL_HOURS", "24")), name + ": ttlHours");
    const std::string public_url = s.contains("expect") ? s.at("expect").at("publicUrl").get<std::string>() : std::string();
    require(status.public_url == public_url, name + ": publicUrl as the invite uses it: " + status.public_url);

    // The invite link built from that status, and what the sender page reads back from it.
    const auto configured = parse_base_address("http://localhost:" + std::to_string(s.at("port").get<int>()));
    require(configured && configured->loopback, name + ": the configured address");
    const InviteBase  invite = choose_invite_base(status.public_url, *configured, {lan_address()}, "");
    const std::string link   = invite_link(invite.base, status.drop_code);
    if (public_url.empty()) {
        require(invite.source == InviteSource::LanAddress &&
                    link == "http://" + lan_address() + ":" + std::to_string(s.at("port").get<int>()) + "/#code=" + code,
                name + ": the LAN invite link " + link);
    } else {
        require(invite.source == InviteSource::PublicUrl && link == s.at("expect").at("link").get<std::string>(),
                name + ": the public invite link " + link);
    }
    summary["links"].push_back({{"scenario", name}, {"link", link}, {"code", code}});

    // GET /api/station/inbox: every field of every entry, oldest first.
    const Captured     inbox_answer = fixtures.load(s.at("inbox"));
    const InboxListing listing      = parse_inbox(inbox_answer.exchange.body);
    require(listing.ok && listing.invalid_ids.empty() && listing.unusable == 0, name + ": a clean inbox listing");
    require(listing.items.size() == expected.size(), name + ": one entry per model accepted");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const InboxItem   &item   = listing.items[i];
        const Expected    &e      = expected[i];
        const std::string  bytes  = fixtures.bytes(e.sample);
        const json         sample = sample_named(manifest, "file", e.sample);
        const std::string  where  = name + " inbox " + std::to_string(i) + " (" + e.file_name + ")";
        require(item.id == e.id, where + ": the id the drop answered");
        require(item.file_name == e.file_name, where + ": the file name as the station shows it: " + item.file_name);
        require(item.sender == e.sender, where + ": the sender: " + item.sender);
        require(item.bytes == bytes.size(), where + ": bytes");
        require(item.sha256 == sha256_hex(bytes) && item.sha256 == sample.at("sha256").get<std::string>(), where + ": sha256");
        require(type_name(item.type) == sample.at("type").get<std::string>(), where + ": type");
        require(is_utc_stamp(item.received_at) && started <= item.received_at && item.received_at <= finished, where + ": receivedAt");
        require(is_clean_file_name(item.file_name), where + ": a name the station may save as it is");
    }
    InboxTracker tracker;
    require(tracker.fresh(listing.items).size() == expected.size(), name + ": every entry is new to the station");

    // GET /api/station/files/<id>: the station's verdict, then the file on disk.
    const json &files = s.at("files");
    require(files.size() == listing.items.size(), name + ": one download per entry");
    const fs::path received = scratch / name;
    fs::create_directories(received);
    for (std::size_t i = 0; i < files.size(); ++i) {
        const InboxItem &item = listing.items[i];
        const Captured   file = fixtures.load(files[i]);
        security_headers(file);
        const std::string &body = file.exchange.body;
        require(body == fixtures.bytes(expected[i].sample), file.label + ": the bytes that were sent");
        require(header_value(file.head, "Content-Type") == "application/octet-stream", file.label + ": Content-Type");
        require(header_value(file.head, "Content-Length") == std::to_string(item.bytes), file.label + ": Content-Length");
        require(header_value(file.head, "X-Content-SHA256") == item.sha256, file.label + ": X-Content-SHA256");
        require(judge_download(item, file.exchange, file.head, sha256_hex(body)) == DownloadVerdict::Accept, file.label + ": Accept");
        fs::path saved;
        require(write_received(received, item.id, item.file_name, item.type, body, saved) == WriteResult::Written, file.label + ": saved");
        require(saved.filename().u8string() == item.file_name && !is_windows_reserved_name(saved.filename().u8string()),
                file.label + ": saved under the name shown");
        require(read_file(saved) == body, file.label + ": saved bytes");
        require(received_state(received, item.id) == ReceivedState::Waiting, file.label + ": waiting for Open or Discard");
        // The same answer with one byte changed, or the announced digest changed, is refused.
        Exchange changed = file.exchange;
        changed.body.back() = static_cast<char>(changed.body.back() ^ 0x01);
        require(judge_download(item, changed, file.head, sha256_hex(changed.body)) == DownloadVerdict::RefuseChecksum,
                file.label + ": a changed byte is refused");
        tracker.take(item.id);
    }

    // Wrong and missing keys, an unknown id.
    if (s.contains("statusWrongKey")) {
        const Captured wrong = fixtures.load(s.at("statusWrongKey"));
        security_headers(wrong);
        require(classify_status(wrong.exchange) == LinkState::WrongStationKey, name + ": a wrong key reads as Wrong station key");
        require(body_json(wrong).at("error") == "unauthorized", name + ": unauthorized");
        require(header_value(wrong.head, "WWW-Authenticate").rfind("Bearer", 0) == 0, name + ": WWW-Authenticate: Bearer");
        const Captured no_key = fixtures.load(s.at("inboxNoKey"));
        require(classify_failure(no_key.exchange) == LinkState::WrongStationKey, name + ": no key reads as Wrong station key");
        const Captured file_no_key = fixtures.load(s.at("fileNoKey"));
        require(judge_download(listing.items.front(), file_no_key.exchange, file_no_key.head, "") == DownloadVerdict::Unauthorized,
                name + ": a download without the key");
        const Captured unknown = fixtures.load(s.at("fileUnknown"));
        require(judge_download(listing.items.front(), unknown.exchange, unknown.head, "") == DownloadVerdict::Gone, name + ": gone");
        require(body_json(unknown).at("error") == "not_found", name + ": not_found");
    }

    // POST /api/station/drop-code.
    const Captured       renewed = fixtures.load(s.at("newCode"));
    const NewCodeOutcome outcome = classify_new_code(renewed.exchange);
    if (setting("DROP_CODE", "").empty()) {
        require(outcome.code && *outcome.code != code && !outcome.fixed && outcome.failure == LinkState::Connected,
                name + ": a new drop code");
        StationStatus  after;
        const Captured status_after = fixtures.load(s.at("statusNewCode"));
        require(classify_status(status_after.exchange, &after) == LinkState::Connected && after.drop_code == *outcome.code,
                name + ": the status shows the new code");
        require(invite_link(invite.base, after.drop_code) != link, name + ": the new link differs from the old one");
    } else {
        require(!outcome.code && outcome.fixed && body_json(renewed).at("error") == "fixed_code", name + ": a fixed code answers 409");
    }

    // Open or Discard: DELETE settles, again too, and the file is gone.
    for (const json &record : s.at("deletes")) {
        const Captured removed = fixtures.load(record);
        require(removed.exchange.http_status == 204 && removed.exchange.body.empty() && delete_settled(removed.exchange),
                removed.label + ": 204 settles the delete");
    }
    if (s.contains("deletesAgain")) {
        for (const json &record : s.at("deletesAgain")) require(delete_settled(fixtures.load(record).exchange), name + ": again");
        for (const json &record : s.at("filesGone")) {
            const Captured gone = fixtures.load(record);
            require(judge_download(listing.items.front(), gone.exchange, gone.head, "") == DownloadVerdict::Gone, gone.label + ": gone");
        }
    }
    const InboxListing empty_listing = parse_inbox(fixtures.load(s.at("inboxEmpty")).exchange.body);
    require(empty_listing.ok && empty_listing.items.empty(), name + ": the inbox is empty afterwards");
    tracker.prune(empty_listing.items);
    for (const InboxItem &item : listing.items) require(!tracker.taken(item.id), name + ": the station forgets removed entries");
}

int run(const fs::path &root, const fs::path &scratch, const fs::path &summary_file)
{
    require(sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 of abc");
    require(sha256_hex(std::string()) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 of nothing");
    const json     manifest = json::parse(read_file(root / "manifest.json"));
    const Fixtures fixtures(root);
    require(manifest.at("protocol") == kProtocolVersion, "protocol 1");
    for (const json &sample : manifest.at("samples")) {
        const std::string bytes = fixtures.bytes(sample.at("file").get<std::string>());
        const auto        type  = type_from_name(sample.at("type").get<std::string>());
        require(bytes.size() == sample.at("bytes").get<std::size_t>() && sha256_hex(bytes) == sample.at("sha256").get<std::string>(),
                "sample " + sample.at("file").get<std::string>());
        require(type && content_matches(*type, bytes) && type_from_extension(sample.at("name").get<std::string>()) == type,
                "sample type " + sample.at("file").get<std::string>());
    }
    json summary = {{"links", json::array()}};
    std::set<std::string> names;
    for (const json &s : manifest.at("scenarios")) {
        scenario(manifest, s, fixtures, scratch, summary);
        names.insert(s.at("name").get<std::string>());
    }
    require(names == std::set<std::string>{"default", "custom"}, "both scenarios were captured");
    // The service refuses at start what the station could not use, and takes exactly its limits.
    require(manifest.at("refusedSettings").size() >= 3, "refused settings were tried");
    for (const json &refused : manifest.at("refusedSettings")) {
        require(refused.at("exitCode") == refused.at("expectedExitCode") && refused.at("message").size() == 1,
                "refused at start with one message: " + refused.at("env").dump());
        const std::string variable = refused.at("env").begin().key();
        require(refused.at("message")[0].get<std::string>().find(variable) != std::string::npos, "the message names " + variable);
    }
    const json &limits = manifest.at("acceptedLimits");
    require(limits.at("exitCode") == 0 && limits.at("config").at("maxBytes").get<std::uint64_t>() == kMaxItemBytes &&
                limits.at("config").at("queueMaxFiles").get<std::size_t>() == kMaxInboxItems,
            "the service takes exactly the station's limits");
    if (!summary_file.empty()) std::ofstream(summary_file, std::ios::binary) << summary.dump(1) << "\n";
    std::cout << "lan_model_drop_container_interop_test: " << assertions << " assertions passed\n";
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3) {
        std::cerr << "usage: interop <fixtures folder> <empty scratch folder> [<summary.json>]\n";
        return 2;
    }
    try {
        return run(fs::u8path(argv[1]), fs::u8path(argv[2]), argc > 3 ? fs::u8path(argv[3]) : fs::path());
    } catch (const std::exception &e) {
        std::cerr << "lan_model_drop_container_interop_test: FAILED after " << assertions << " assertions: " << e.what() << "\n";
        return 1;
    }
}
