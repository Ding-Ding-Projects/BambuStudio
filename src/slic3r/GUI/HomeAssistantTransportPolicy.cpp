#include "HomeAssistantTransportPolicy.hpp"

#include <boost/asio/ip/address.hpp>

#include <algorithm>
#include <cctype>

namespace Slic3r { namespace GUI { namespace HomeAssistant {

namespace {

std::string ascii_lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool valid_port(const std::string &port)
{
    unsigned value = 0;
    for (unsigned char ch : port) {
        if (std::isdigit(ch) == 0)
            return false;
        value = value * 10 + static_cast<unsigned>(ch - '0');
        if (value > 65535)
            return false;
    }
    return value != 0;
}

bool extract_http_host(const std::string &url, std::string &scheme, std::string &host)
{
    if (std::any_of(url.begin(), url.end(), [](unsigned char ch) {
            return ch <= 0x20 || ch == 0x7f || ch == '\\';
        }) ||
        url.find_first_of("?#") != std::string::npos)
        return false;

    const size_t scheme_end = url.find("://");
    if (scheme_end == std::string::npos || scheme_end == 0)
        return false;

    scheme = ascii_lower(url.substr(0, scheme_end));
    const size_t authority_start = scheme_end + 3;
    const size_t authority_end   = url.find('/', authority_start);
    const std::string authority  = url.substr(
        authority_start,
        authority_end == std::string::npos ? std::string::npos : authority_end - authority_start);
    if (authority.empty() || authority.find('@') != std::string::npos)
        return false;

    std::string port;
    bool bracketed = false;
    if (authority.front() == '[') {
        bracketed = true;
        const size_t bracket = authority.find(']');
        if (bracket == std::string::npos)
            return false;
        host = authority.substr(1, bracket - 1);
        const std::string remainder = authority.substr(bracket + 1);
        if (!remainder.empty()) {
            if (remainder.front() != ':')
                return false;
            port = remainder.substr(1);
        }
    } else {
        const size_t first_colon = authority.find(':');
        const size_t last_colon  = authority.rfind(':');
        if (first_colon != last_colon) // IPv6 URL literals must use brackets.
            return false;
        host = authority.substr(0, first_colon);
        if (first_colon != std::string::npos)
            port = authority.substr(first_colon + 1);
    }

    if ((!port.empty() && !valid_port(port)) ||
        (!authority.empty() && authority.back() == ':'))
        return false;

    boost::system::error_code error;
    const boost::asio::ip::address address = boost::asio::ip::make_address(host, error);
    if (!error) {
        if (address.is_v6() != bracketed)
            return false;
    } else {
        if (bracketed || host.size() > 253)
            return false;

        const std::string dns_name =
            !host.empty() && host.back() == '.' ? host.substr(0, host.size() - 1) : host;
        if (dns_name.empty())
            return false;
        size_t label_begin = 0;
        while (label_begin < dns_name.size()) {
            const size_t label_end = dns_name.find('.', label_begin);
            const size_t length =
                (label_end == std::string::npos ? dns_name.size() : label_end) - label_begin;
            if (length == 0 || length > 63 || dns_name[label_begin] == '-' ||
                dns_name[label_begin + length - 1] == '-')
                return false;
            if (!std::all_of(
                    dns_name.begin() + label_begin,
                    dns_name.begin() + label_begin + length,
                    [](unsigned char ch) { return std::isalnum(ch) != 0 || ch == '-'; }))
                return false;
            if (label_end == std::string::npos)
                break;
            label_begin = label_end + 1;
        }
    }

    host = ascii_lower(host);
    return !host.empty();
}

bool loopback_host(const std::string &host)
{
    if (host == "localhost" || host == "localhost.")
        return true;

    boost::system::error_code error;
    const boost::asio::ip::address address = boost::asio::ip::make_address(host, error);
    // Slic3r::Http currently pins libcurl to IPv4, so accepting an IPv6
    // loopback literal here would pass the security gate but could never
    // connect. `localhost` remains supported and resolves through that same
    // IPv4-only transport.
    return !error && address.is_v4() && address.is_loopback();
}

} // namespace

CredentialTransportSafety credential_transport_safety_for_url(const std::string &url)
{
    if (url.empty())
        return CredentialTransportSafety::NotConfigured;

    std::string scheme;
    std::string host;
    if (!extract_http_host(url, scheme, host))
        return CredentialTransportSafety::Insecure;
    if (scheme == "https")
        return CredentialTransportSafety::Safe;
    if (scheme == "http" && loopback_host(host))
        return CredentialTransportSafety::Safe;
    return CredentialTransportSafety::Insecure;
}

}}} // namespace Slic3r::GUI::HomeAssistant
