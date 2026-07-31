#ifndef slic3r_GUI_HomeAssistantTransportPolicy_hpp_
#define slic3r_GUI_HomeAssistantTransportPolicy_hpp_

#include <string>

namespace Slic3r { namespace GUI { namespace HomeAssistant {

enum class CredentialTransportSafety
{
    Safe,
    NotConfigured,
    Insecure
};

// Classify a Home Assistant base URL without opening a socket. Token-bearing
// requests are allowed over HTTPS, or over HTTP only to an IPv4 loopback
// address or localhost. An empty URL is NotConfigured; malformed/unsupported
// URLs and clear-text LAN destinations are Insecure.
CredentialTransportSafety credential_transport_safety_for_url(const std::string &url);

}}} // namespace Slic3r::GUI::HomeAssistant

#endif // slic3r_GUI_HomeAssistantTransportPolicy_hpp_
