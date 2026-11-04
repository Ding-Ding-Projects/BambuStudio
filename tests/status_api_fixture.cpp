// Behaviour fixture for the local printer status API rules (StatusApi/StatusApiCore.hpp).
// Compiled and run by tests/status_api.test.mjs. Standard library only.
#include <iostream>
#include <string>
#include <vector>

#include "StatusApi/StatusApiCore.hpp"

using namespace Slic3r::GUI::StatusApi;

static int failures = 0;
static void check(bool ok, const char *name)
{
    std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
    failures += !ok;
}

static const std::string kToken = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

static Settings on()
{
    Settings s;
    s.enabled = true;
    s.token   = kToken;
    return s;
}

static Request get(const std::string &target, const std::string &auth = "Bearer " + kToken)
{
    Request r;
    r.method           = "GET";
    r.target           = target;
    r.authorization    = auth;
    r.host             = "127.0.0.1:18787";
    r.peer_is_loopback = true;
    return r;
}

static PrinterSnapshot printer(const std::string &id, bool supported, int bit)
{
    PrinterSnapshot p;
    p.dev_id         = id;
    p.name           = "Shop \"A\"";
    p.model          = "H2D";
    p.connected      = true;
    p.printing       = false;
    p.door_supported = supported;
    p.door_bit       = bit;
    p.updated_at     = "2026-11-04T09:30:12Z";
    return p;
}

int main()
{
    const std::vector<PrinterSnapshot> printers = {printer("PRN-OPEN", true, 1), printer("PRN-CLOSED", true, 0), printer("PRN-UNSEEN", true, -1),
                                                   printer("PRN-NODOOR", false, 1)};

    // Off by default.
    check(!Settings().enabled && Settings().token.empty() && Settings().port == kDefaultPort, "settings are off by default with the default port");
    check(std::string(kBindAddress) == "127.0.0.1", "the bind address is the IPv4 loopback address");
    check(handle_request(Settings(), printers, get("/api/v1/health")).status == 503, "a default (off) configuration serves nothing");
    Settings no_token = on();
    no_token.token.clear();
    check(handle_request(no_token, printers, get("/api/v1/health", "Bearer ")).status == 503, "an empty token never serves");
    Settings weak = on();
    weak.token = "short";
    check(handle_request(weak, printers, get("/api/v1/health", "Bearer short")).status == 503, "a short token never serves");

    // Peer and Host.
    Request remote = get("/api/v1/printers");
    remote.peer_is_loopback = false;
    check(handle_request(on(), printers, remote).status == 403, "a non-loopback peer gets 403 even with the right token");
    Request rebind = get("/api/v1/printers");
    rebind.host = "evil.example:18787";
    check(handle_request(on(), printers, rebind).status == 403, "a foreign Host header gets 403");
    check(host_header_is_loopback("localhost") && host_header_is_loopback("LOCALHOST:8080") && host_header_is_loopback("127.0.0.1") &&
              host_header_is_loopback("[::1]:9") && host_header_is_loopback("[::1]"),
          "loopback Host names are accepted");
    check(!host_header_is_loopback("") && !host_header_is_loopback("127.0.0.1.evil.com") && !host_header_is_loopback("localhost:") &&
              !host_header_is_loopback("localhost:80x") && !host_header_is_loopback("203.0.113.5") && !host_header_is_loopback("[::1"),
          "other Host values are refused");

    // GET only.
    for (const char *method : {"POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"}) {
        Request r = get("/api/v1/printers");
        r.method  = method;
        const Response resp = handle_request(on(), printers, r);
        check(resp.status == 405 && resp.allow == "GET", (std::string(method) + " gets 405 with Allow: GET").c_str());
    }
    {
        Request r = get("/api/v1/printers", "");
        r.method  = "POST";
        check(handle_request(on(), printers, r).status == 405, "a wrong method is refused before the token is looked at");
    }

    // Token.
    check(handle_request(on(), printers, get("/api/v1/printers", "")).status == 401, "no Authorization header gets 401");
    check(handle_request(on(), printers, get("/api/v1/printers", "Bearer wrong")).status == 401, "a wrong token gets 401");
    check(handle_request(on(), printers, get("/api/v1/printers", "Basic " + kToken)).status == 401, "another scheme gets 401");
    check(handle_request(on(), printers, get("/api/v1/printers", kToken)).status == 401, "a bare token without the scheme gets 401");
    check(handle_request(on(), printers, get("/api/v1/printers", "Bearer " + kToken.substr(0, kToken.size() - 1))).status == 401,
          "a token that is a prefix of the real one gets 401");
    check(handle_request(on(), printers, get("/api/v1/printers", "Bearer " + kToken + "0")).status == 401, "a token with extra characters gets 401");
    check(handle_request(on(), printers, get("/api/v1/printers", "Bearer a b")).status == 401, "a token with a space gets 401");
    check(handle_request(on(), printers, get("/api/v1/printers", "")).www_authenticate, "401 asks for a bearer token");
    check(handle_request(on(), printers, get("/api/v1/printers", "bearer " + kToken)).status == 200, "the scheme name is case-insensitive");
    check(parse_bearer("Bearer abc") == "abc" && parse_bearer("  Bearer   abc  ") == "abc" && parse_bearer("Bearer").empty() &&
              parse_bearer("Bearer ").empty() && parse_bearer("Bearerabc").empty() && parse_bearer("").empty(),
          "bearer parsing");
    check(constant_time_equal("abc", "abc") && !constant_time_equal("abc", "abd") && !constant_time_equal("abc", "ab") &&
              !constant_time_equal("", "a") && constant_time_equal("", ""),
          "constant-time comparison");

    // Routes.
    const Response health = handle_request(on(), printers, get("/api/v1/health"));
    check(health.status == 200 && health.body == "{\"status\":\"ok\",\"api_version\":1}", "health answers");
    const Response list = handle_request(on(), printers, get("/api/v1/printers"));
    check(list.status == 200 && list.body.find("\"dev_id\":\"PRN-OPEN\"") != std::string::npos &&
              list.body.find("\"name\":\"Shop \\\"A\\\"\"") != std::string::npos && list.body.find("\"connected\":true,\"printing\":false") != std::string::npos,
          "the list carries dev_id, name, model, connected and printing with escaped text");
    check(handle_request(on(), {}, get("/api/v1/printers")).body == "{\"printers\":[]}", "an empty list is an empty array");
    check(handle_request(on(), printers, get("/api/v1/printers/?x=1")).status == 200, "a query string and a trailing slash are ignored");

    const Response open = handle_request(on(), printers, get("/api/v1/printers/PRN-OPEN/door"));
    check(open.status == 200 &&
              open.body == "{\"dev_id\":\"PRN-OPEN\",\"supported\":true,\"state\":\"open\",\"source\":\"home_flag bit 23 or print.stat bit 23\",\"beta\":true,"
                           "\"updated_at\":\"2026-11-04T09:30:12Z\"}",
          "door open, with the exact JSON shape");
    check(handle_request(on(), printers, get("/api/v1/printers/PRN-CLOSED/door")).body.find("\"state\":\"closed\"") != std::string::npos, "door closed");
    check(handle_request(on(), printers, get("/api/v1/printers/PRN-UNSEEN/door")).body.find("\"state\":\"unknown\"") != std::string::npos,
          "an unreported bit is unknown, never closed");
    const Response nodoor = handle_request(on(), printers, get("/api/v1/printers/PRN-NODOOR/door"));
    check(nodoor.status == 200 && nodoor.body.find("\"supported\":false") != std::string::npos && nodoor.body.find("\"state\":\"unknown\"") != std::string::npos,
          "a printer without door support reports unsupported and unknown even if a bit was seen");
    check(handle_request(on(), printers, get("/api/v1/printers/NOPE/door")).status == 404, "an unknown printer gets 404");
    check(handle_request(on(), printers, get("/api/v1/printers/a%2Fb/door")).status == 404, "an encoded id gets 404");
    check(handle_request(on(), printers, get("/api/v1/printers/../door")).status == 404, "a dot-dot id gets 404");
    check(handle_request(on(), printers, get("/api/v1/printers//door")).status == 404, "an empty id gets 404");
    check(handle_request(on(), printers, get("/api/v1/other")).status == 404, "an unknown path gets 404");
    check(handle_request(on(), printers, get("/api/v1/printers/PRN-OPEN/door/extra")).status == 404, "extra path segments get 404");
    check(handle_request(on(), printers, get("/" + std::string(300, 'a'))).status == 414, "an over-long target gets 414");
    check(handle_request(on(), printers, get("/api/v1/printers/PRN-OPEN/door", "")).status == 401, "the door endpoint needs the token");

    // Settings and helpers.
    check(parse_port("18787") == 18787 && parse_port("") == kDefaultPort && parse_port("80") == kDefaultPort && parse_port("70000") == kDefaultPort &&
              parse_port("12a") == kDefaultPort && parse_port("1024") == 1024 && parse_port("65535") == 65535 && parse_port("-5") == kDefaultPort,
          "port parsing falls back to the default outside 1024 to 65535");
    unsigned char bytes[4] = {0x00, 0xab, 0x0f, 0xff};
    check(hex_encode(bytes, 4) == "00ab0fff", "hex encoding");
    check(token_usable(kToken) && !token_usable("") && !token_usable(std::string(31, 'a')) && !token_usable(std::string(129, 'a')) &&
              !token_usable(std::string(31, 'a') + " "),
          "token acceptability");
    check(json_string("a\"b\\c\n\x01") == "\"a\\\"b\\\\c\\n\\u0001\"", "JSON escaping");
    check(json_string("\xe6\x89\x93") == "\"\xe6\x89\x93\"", "UTF-8 passes through JSON unchanged");
    check(iso8601_utc(0) == "1970-01-01T00:00:00Z", "ISO 8601 time stamp");

    std::vector<PrinterSnapshot> before = {printer("A", true, 0)};
    before[0].updated_at = "T0";
    std::vector<PrinterSnapshot> same    = {printer("A", true, 0), printer("B", true, 1)};
    carry_updated_at(before, same, "T1");
    check(same[0].updated_at == "T0" && same[1].updated_at == "T1", "an unchanged printer keeps its time stamp and a new one gets the current time");
    std::vector<PrinterSnapshot> changed = {printer("A", true, 1)};
    carry_updated_at(before, changed, "T2");
    check(changed[0].updated_at == "T2", "a changed door bit gets a new time stamp");

    std::cout << (failures ? "FAILED" : "all header checks passed") << '\n';
    return failures ? 1 : 0;
}
