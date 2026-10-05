#include "status_hub_client.hpp"

#if defined(_WIN32)

#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace status_hub {
namespace {

std::wstring wide(const std::string& value) noexcept {
  try {
    if (value.empty()) return {};
    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (required <= 0) return {};
    std::wstring output(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), output.data(), required) != required) return {};
    return output;
  } catch (...) {
    return {};
  }
}

struct WinHttpHandle final {
  HINTERNET value = nullptr;

  WinHttpHandle() noexcept = default;
  explicit WinHttpHandle(HINTERNET handle) noexcept : value(handle) {}
  WinHttpHandle(const WinHttpHandle&) = delete;
  WinHttpHandle& operator=(const WinHttpHandle&) = delete;
  WinHttpHandle(WinHttpHandle&& other) noexcept : value(other.value) { other.value = nullptr; }
  WinHttpHandle& operator=(WinHttpHandle&& other) noexcept {
    if (this != &other) {
      if (value != nullptr) WinHttpCloseHandle(value);
      value = other.value;
      other.value = nullptr;
    }
    return *this;
  }
  ~WinHttpHandle() noexcept { if (value != nullptr) WinHttpCloseHandle(value); }
  explicit operator bool() const noexcept { return value != nullptr; }
};

TransportResult winHttpFailure(DWORD errorCode, const char* message) noexcept {
  TransportResult result;
  result.code = errorCode == ERROR_WINHTTP_TIMEOUT ? ResultCode::timeout : ResultCode::transport;
  result.error = message;
  return result;
}

std::string lastErrorText(DWORD code) noexcept {
  (void)code;
  return "WinHTTP transport failed.";
}

class WinHttpTransport final : public HttpTransport {
public:
  TransportResult send(const HttpRequest& request) noexcept override {
    TransportResult result;
    try {
      const std::wstring url = wide(request.url);
      const std::wstring method = wide(request.method.empty() ? "GET" : request.method);
      if (url.empty() || method.empty()) {
        result.code = ResultCode::invalid_url;
        result.error = "The WinHTTP request URL or method could not be encoded.";
        return result;
      }
      URL_COMPONENTS components{};
      components.dwStructSize = sizeof(components);
      components.dwSchemeLength = static_cast<DWORD>(-1);
      components.dwHostNameLength = static_cast<DWORD>(-1);
      components.dwUrlPathLength = static_cast<DWORD>(-1);
      components.dwExtraInfoLength = static_cast<DWORD>(-1);
      if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &components)) {
        result.code = ResultCode::invalid_url;
        result.error = "WinHTTP could not parse the request URL.";
        return result;
      }
      const bool secure = components.nScheme == INTERNET_SCHEME_HTTPS;
      const std::wstring host(components.lpszHostName, components.dwHostNameLength);
      std::wstring path(components.lpszUrlPath ? components.lpszUrlPath : L"", components.dwUrlPathLength == static_cast<DWORD>(-1) ? 0 : components.dwUrlPathLength);
      if (components.lpszExtraInfo && components.dwExtraInfoLength != 0 && components.dwExtraInfoLength != static_cast<DWORD>(-1)) path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
      if (path.empty()) path = L"/";

      WinHttpHandle session(WinHttpOpen(L"StatusHubClient/0.1", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
      if (!session) { result.error = lastErrorText(GetLastError()); return result; }
      const DWORD timeout = static_cast<DWORD>(std::clamp(request.timeoutMs, 1, 120000));
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
      if (!WinHttpSetTimeouts(session.value, static_cast<int>(timeout), static_cast<int>(timeout), static_cast<int>(timeout), static_cast<int>(timeout))) return winHttpFailure(GetLastError(), "WinHTTP could not set request timeouts.");
      WinHttpHandle connection(WinHttpConnect(session.value, host.c_str(), components.nPort, 0));
      if (!connection) { result.error = lastErrorText(GetLastError()); return result; }
      WinHttpHandle handle(WinHttpOpenRequest(connection.value, method.c_str(), path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0));
      if (!handle) { result.error = lastErrorText(GetLastError()); return result; }
      // Credentials are scoped to the configured origin. Never let WinHTTP
      // replay custom authentication headers to a redirect destination.
      DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
      if (!WinHttpSetOption(handle.value, WINHTTP_OPTION_REDIRECT_POLICY,
                            &redirectPolicy, sizeof(redirectPolicy)))
        return winHttpFailure(GetLastError(), "WinHTTP could not disable redirects.");

      std::wstring headers;
      for (const auto& header : request.headers) {
        const std::wstring name = wide(header.first);
        const std::wstring value = wide(header.second);
        if (name.empty()) continue;
        headers += name;
        headers += L": ";
        headers += value;
        headers += L"\r\n";
      }
      const BOOL sent = WinHttpSendRequest(handle.value, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), headers.empty() ? 0 : static_cast<DWORD>(headers.size()),
                                           request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request.body.data()), static_cast<DWORD>(request.body.size()), static_cast<DWORD>(request.body.size()), 0);
      if (!sent) return winHttpFailure(GetLastError(), "WinHTTP could not send the request.");
      if (!WinHttpReceiveResponse(handle.value, nullptr)) return winHttpFailure(GetLastError(), "WinHTTP could not receive the response.");

      DWORD status = 0;
      DWORD statusSize = sizeof(status);
      if (!WinHttpQueryHeaders(handle.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX)) return winHttpFailure(GetLastError(), "WinHTTP could not read the response status.");
      result.response.status = static_cast<int>(status);
      for (const wchar_t* name : {L"Retry-After", L"X-Status-Hub-Oldest-Sequence"}) {
        DWORD size = 0;
        WinHttpQueryHeaders(handle.value, WINHTTP_QUERY_CUSTOM, name, nullptr, &size, WINHTTP_NO_HEADER_INDEX);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0) continue;
        std::vector<wchar_t> value(size / sizeof(wchar_t) + 1u, L'\0');
        if (WinHttpQueryHeaders(handle.value, WINHTTP_QUERY_CUSTOM, name, value.data(), &size, WINHTTP_NO_HEADER_INDEX)) {
          const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(size / sizeof(wchar_t)), nullptr, 0, nullptr, nullptr);
          if (bytes > 0) {
            std::string utf8(static_cast<std::size_t>(bytes), '\0');
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(size / sizeof(wchar_t)), utf8.data(), bytes, nullptr, nullptr);
            result.response.headers.emplace_back(name == std::wstring(L"Retry-After") ? "retry-after" : "x-status-hub-oldest-sequence", utf8);
          }
        }
      }

      DWORD available = 0;
      for (;;) {
        if (std::chrono::steady_clock::now() >= deadline)
          return winHttpFailure(ERROR_WINHTTP_TIMEOUT, "The response deadline expired.");
        if (!WinHttpQueryDataAvailable(handle.value, &available)) return winHttpFailure(GetLastError(), "WinHTTP could not query response data.");
        if (available == 0) break;
        if (result.response.body.size() + available > ProtocolLimits::maxResponseBytes) {
          result.code = ResultCode::response_too_large;
          result.error = "The WinHTTP response exceeded the bounded 512 KiB size.";
          return result;
        }
        std::vector<char> buffer(available);
        DWORD read = 0;
        if (!WinHttpReadData(handle.value, buffer.data(), available, &read)) return winHttpFailure(GetLastError(), "WinHTTP could not read response data.");
        if (read == 0) return winHttpFailure(ERROR_WINHTTP_CONNECTION_ERROR, "WinHTTP returned an empty response chunk before completion.");
        result.response.body.append(buffer.data(), read);
      }
      result.ok = true;
      result.code = ResultCode::ok;
      return result;
    } catch (...) {
      result.code = ResultCode::transport;
      result.error = "WinHTTP transport failed safely.";
      return result;
    }
  }
};

} // namespace

std::unique_ptr<HttpTransport> makeWinHttpTransport() noexcept {
  try { return std::unique_ptr<HttpTransport>(new WinHttpTransport()); } catch (...) { return nullptr; }
}

std::unique_ptr<HttpTransport> makeDefaultTransport() noexcept {
  return makeWinHttpTransport();
}

} // namespace status_hub

#endif
