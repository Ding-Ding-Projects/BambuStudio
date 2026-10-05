#include "status_hub_client.hpp"

#if defined(STATUS_HUB_CLIENT_HAS_CURL)

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <cstdlib>

namespace status_hub {
namespace {

struct CurlContext final {
  HttpResponse response;
  ResultCode code = ResultCode::transport;
  std::string error;
  bool tooLarge = false;
};

std::size_t writeBody(char* data, std::size_t size, std::size_t count, void* opaque) noexcept {
  try {
    auto* context = static_cast<CurlContext*>(opaque);
    const std::size_t bytes = size * count;
    if (context->response.body.size() + bytes > ProtocolLimits::maxResponseBytes) {
      context->tooLarge = true;
      return 0;
    }
    context->response.body.append(data, bytes);
    return bytes;
  } catch (...) {
    return 0;
  }
}

std::size_t readHeader(char* data, std::size_t size, std::size_t count, void* opaque) noexcept {
  try {
    auto* context = static_cast<CurlContext*>(opaque);
    const std::size_t bytes = size * count;
    std::string line(data, bytes);
    const std::size_t colon = line.find(':');
    if (colon != std::string::npos) {
      std::string name = line.substr(0, colon);
      std::string value = line.substr(colon + 1);
      while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) value.erase(value.begin());
      while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || std::isspace(static_cast<unsigned char>(value.back())) != 0)) value.pop_back();
      std::transform(name.begin(), name.end(), name.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
      if (name == "retry-after" || name == "x-status-hub-oldest-sequence") context->response.headers.emplace_back(std::move(name), std::move(value));
    }
    return bytes;
  } catch (...) {
    return 0;
  }
}

class CurlGlobal final {
public:
  static bool available() noexcept {
    static const CurlGlobal global;
    return global.available_;
  }

private:
  bool available_ = false;

  CurlGlobal() noexcept {
    try {
      available_ = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
      if (available_) std::atexit(&CurlGlobal::cleanup);
    } catch (...) {
      available_ = false;
    }
  }

  ~CurlGlobal() noexcept = default;

  static void cleanup() noexcept {
    curl_global_cleanup();
  }
};

class CurlTransport final : public HttpTransport {
public:
  CurlTransport() noexcept = default;
  ~CurlTransport() noexcept override = default;

  TransportResult send(const HttpRequest& request) noexcept override {
    TransportResult result;
    CURL* curl = nullptr;
    curl_slist* headers = nullptr;
    try {
      curl = curl_easy_init();
      if (!curl) { result.error = "libcurl could not create a request handle."; return result; }
      CurlContext context;
      curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.empty() ? "GET" : request.method.c_str());
      curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(std::clamp(request.timeoutMs, 1, 120000)));
      curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(std::clamp(request.timeoutMs, 1, 120000)));
      curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
      curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBody);
      curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
      curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, readHeader);
      curl_easy_setopt(curl, CURLOPT_HEADERDATA, &context);
      if (!request.body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
      }
      for (const auto& header : request.headers) {
        headers = curl_slist_append(headers, (header.first + ": " + header.second).c_str());
      }
      curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
      const CURLcode code = curl_easy_perform(curl);
      if (context.tooLarge) {
        result.code = ResultCode::response_too_large;
        result.error = "The libcurl response exceeded the bounded 512 KiB size.";
      } else if (code != CURLE_OK) {
        result.code = code == CURLE_OPERATION_TIMEDOUT ? ResultCode::timeout : ResultCode::transport;
        result.error = curl_easy_strerror(code);
      } else {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        context.response.status = static_cast<int>(status);
        result.ok = true;
        result.code = ResultCode::ok;
        result.response = std::move(context.response);
      }
    } catch (...) {
      result.code = ResultCode::transport;
      result.error = "libcurl transport failed safely.";
    }
    if (headers) curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);
    return result;
  }
};

} // namespace

std::unique_ptr<HttpTransport> makeCurlTransport() noexcept {
  try {
    if (!CurlGlobal::available()) return nullptr;
    return std::unique_ptr<HttpTransport>(new CurlTransport());
  } catch (...) {
    return nullptr;
  }
}

} // namespace status_hub

#endif
