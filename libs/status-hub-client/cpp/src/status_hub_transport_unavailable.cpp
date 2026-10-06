#include "status_hub_client.hpp"

#include <memory>

namespace status_hub {
namespace {

class UnavailableTransport final : public HttpTransport {
public:
  TransportResult send(const HttpRequest&) noexcept override {
    TransportResult result;
    result.code = ResultCode::unreachable;
    result.error = "No default HTTP transport is available for this platform.";
    return result;
  }
};

} // namespace

#if !defined(_WIN32)
std::unique_ptr<HttpTransport> makeDefaultTransport() noexcept {
  try { return std::unique_ptr<HttpTransport>(new UnavailableTransport()); } catch (...) { return nullptr; }
}
#endif

#if !defined(STATUS_HUB_CLIENT_HAS_CURL)
std::unique_ptr<HttpTransport> makeCurlTransport() noexcept {
  return nullptr;
}
#endif

} // namespace status_hub
