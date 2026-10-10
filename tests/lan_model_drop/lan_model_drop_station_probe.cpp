// Command-line probe over the LAN model drop station's pure model
// (src/slic3r/GUI/LanModelDrop/LanModelDropModel.cpp), so tests/lan_model_drop/station_container.test.mjs
// can hand it what the real drop service answers and see exactly what the application would make of
// it: the connection state, the inbox entries, the content check, the announced SHA-256, a new drop
// code and the invite link. Each command prints one line of JSON.
//
//   probe health  <body file>
//   probe status  <http status> <body file>
//   probe failure <http status>                      (0: no HTTP answer at all)
//   probe inbox   <body file>
//   probe code    <body file>
//   probe content <3mf|stl|step|obj|amf> <file>
//   probe header  <raw header file> <name>
//   probe invite  <public url> <configured address> <comma-separated LAN IPv4s> <preferred IPv4> <drop code>
#include "../../src/slic3r/GUI/LanModelDrop/LanModelDropModel.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

using namespace Slic3r::GUI::LanModelDrop;
using json = nlohmann::json;

namespace {

std::string read_file(const std::string &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot read " + path);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

const char *state_name(LinkState state)
{
    switch (state) {
    case LinkState::Off: return "Off";
    case LinkState::Checking: return "Checking";
    case LinkState::Connected: return "Connected";
    case LinkState::NotReachable: return "NotReachable";
    case LinkState::WrongStationKey: return "WrongStationKey";
    case LinkState::ProtocolNotSupported: return "ProtocolNotSupported";
    case LinkState::NeedsStationKey: return "NeedsStationKey";
    case LinkState::InvalidAddress: return "InvalidAddress";
    case LinkState::KeyStorageUnavailable: return "KeyStorageUnavailable";
    }
    return "?";
}

const char *source_name(InviteSource source)
{
    switch (source) {
    case InviteSource::None: return "None";
    case InviteSource::PublicUrl: return "PublicUrl";
    case InviteSource::Configured: return "Configured";
    case InviteSource::LanAddress: return "LanAddress";
    }
    return "?";
}

Exchange answer(const std::string &status_text, const std::string &body)
{
    Exchange e;
    e.http_status = static_cast<unsigned>(std::stoul(status_text));
    e.completed   = e.http_status != 0;
    e.body        = body;
    return e;
}

std::vector<std::string> split(const std::string &text)
{
    std::vector<std::string> out;
    std::stringstream        in(text);
    for (std::string part; std::getline(in, part, ',');)
        if (!part.empty()) out.push_back(part);
    return out;
}

int run(const std::vector<std::string> &args)
{
    const std::string &command = args.at(0);
    json out;
    if (command == "health") {
        out["ok"] = parse_health(read_file(args.at(1)));
    } else if (command == "status") {
        StationStatus   status;
        const LinkState state = classify_status(answer(args.at(1), read_file(args.at(2))), &status);
        out["state"] = state_name(state);
        if (state == LinkState::Connected) {
            out["stationName"] = status.station_name;
            out["dropCode"]    = status.drop_code;
            out["publicUrl"]   = status.public_url;
            out["queued"]      = status.queued;
            out["queuedBytes"] = status.queued_bytes;
            out["maxBytes"]    = status.max_bytes;
            out["ttlHours"]    = status.ttl_hours;
        }
    } else if (command == "failure") {
        out["state"] = state_name(classify_failure(answer(args.at(1), std::string())));
    } else if (command == "inbox") {
        const InboxListing listing = parse_inbox(read_file(args.at(1)));
        out["ok"]       = listing.ok;
        out["items"]    = json::array();
        out["invalid"]  = listing.invalid_ids;
        out["unusable"] = listing.unusable;
        for (const InboxItem &item : listing.items)
            out["items"].push_back({{"id", item.id}, {"fileName", item.file_name}, {"sender", item.sender}, {"bytes", item.bytes},
                                    {"sha256", item.sha256}, {"receivedAt", item.received_at}, {"type", type_name(item.type)}});
    } else if (command == "code") {
        const auto code = parse_drop_code(read_file(args.at(1)));
        out["code"]     = code ? json(*code) : json(nullptr);
    } else if (command == "content") {
        const auto type = type_from_name(args.at(1));
        out["matches"]  = type.has_value() && content_matches(*type, read_file(args.at(2)));
    } else if (command == "header") {
        out["value"] = header_value(read_file(args.at(1)), args.at(2));
    } else if (command == "invite") {
        const auto configured = parse_base_address(args.at(2));
        const InviteBase base = choose_invite_base(args.at(1), configured.value_or(BaseAddress{}), split(args.at(3)), args.at(4));
        out["source"]  = source_name(base.source);
        out["base"]    = base.base;
        out["choices"] = base.choices;
        out["link"]    = invite_link(base.base, args.at(5));
    } else {
        std::cerr << "unknown command: " << command << "\n";
        return 2;
    }
    std::cout << out.dump() << "\n";
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "usage: probe <command> ...\n";
        return 2;
    }
    try {
        return run(std::vector<std::string>(argv + 1, argv + argc));
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
