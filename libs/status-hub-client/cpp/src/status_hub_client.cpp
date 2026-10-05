#include "status_hub_client.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
#include <thread>
#include <unordered_set>

#if defined(_WIN32)
#  if !defined(NOMINMAX)
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <bcrypt.h>
#else
#  include <fcntl.h>
#  include <pthread.h>
#  include <signal.h>
#  include <sys/select.h>
#  include <sys/types.h>
#  include <sys/wait.h>
#  include <unistd.h>
#  if defined(__linux__)
#    include <sys/random.h>
#  endif
#endif

namespace status_hub {
namespace {

using Clock = std::chrono::steady_clock;
using Deadline = std::optional<Clock::time_point>;

int remainingMs(const Deadline& deadline, int fallbackMs) noexcept {
  try {
    const int fallback = std::max(1, fallbackMs);
    if (!deadline) return fallback;
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - Clock::now()).count();
    if (remaining <= 0) return 0;
    return static_cast<int>(std::min<std::int64_t>(remaining, fallback));
  } catch (...) {
    return 0;
  }
}

std::int64_t systemNowMs() noexcept {
  try {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
  } catch (...) {
    return 0;
  }
}

std::string isoNow(std::int64_t value) noexcept {
  try {
    const auto time = static_cast<std::time_t>(value / 1000);
    std::tm tm{};
#if defined(_WIN32)
    if (gmtime_s(&tm, &time) != 0) return {};
#else
    if (gmtime_r(&time, &tm) == nullptr) return {};
#endif
    std::ostringstream stream;
    stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.'
           << std::setw(3) << std::setfill('0') << (value % 1000) << 'Z';
    return stream.str();
  } catch (...) {
    return {};
  }
}

std::string environment(const char* name) noexcept {
  try {
    const char* value = std::getenv(name);
    return value == nullptr ? std::string{} : std::string(value);
  } catch (...) {
    return {};
  }
}

void appendWarning(std::vector<Warning>& warnings, const std::string& field,
                   const std::string& reason) noexcept {
  try {
    warnings.push_back(Warning{field, reason});
  } catch (...) {
    // A warning is diagnostic only.  A failed allocation must not turn a
    // bounded client operation into an exception path.
  }
}

bool isControl(unsigned char value) noexcept {
  return (value <= 8u) || value == 11u || value == 12u || (value >= 14u && value <= 31u) || value == 127u;
}

std::string cleanString(const std::string& input, std::size_t maximum) noexcept {
  try {
    std::string output;
    output.reserve(std::min(input.size(), maximum));
    for (unsigned char value : input) {
      if (!isControl(value)) output.push_back(static_cast<char>(value));
    }
    std::size_t first = 0;
    while (first < output.size() && std::isspace(static_cast<unsigned char>(output[first])) != 0) ++first;
    std::size_t last = output.size();
    while (last > first && std::isspace(static_cast<unsigned char>(output[last - 1])) != 0) --last;
    output = output.substr(first, last - first);
    if (output.size() > maximum) output.resize(maximum);
    return output;
  } catch (...) {
    return {};
  }
}

bool validId(const std::string& input, std::size_t maximum) noexcept {
  if (input.empty() || input.size() > maximum) return false;
  for (unsigned char value : input) {
    if (!(std::isalnum(value) != 0 || value == '.' || value == '_' || value == ':' || value == '-')) return false;
  }
  return true;
}

std::string cleanId(const std::string& input, std::size_t maximum) noexcept {
  const std::string value = cleanString(input, maximum);
  return validId(value, maximum) ? value : std::string{};
}

bool validUrl(const std::string& input) noexcept {
  const std::string value = cleanString(input, ProtocolLimits::evidenceUrlMax);
  const bool scheme = value.rfind("https://", 0) == 0 || value.rfind("http://", 0) == 0;
  if (!scheme || value.find_first_of("\r\n\t") != std::string::npos) return false;
  const std::size_t authorityStart = value.find("://") + 3;
  const std::size_t authorityEnd = value.find_first_of("/?#", authorityStart);
  const std::size_t authorityLength = authorityEnd == std::string::npos
      ? value.size() - authorityStart : authorityEnd - authorityStart;
  return authorityLength > 0 && value.substr(authorityStart, authorityLength).find('@') == std::string::npos;
}

bool validBaseUrl(const std::string& input) noexcept {
  if (input.rfind("https://", 0) != 0 && input.rfind("http://", 0) != 0) return false;
  if (input.find_first_of("\r\n\t") != std::string::npos) return false;
  const std::size_t authorityStart = input.find("://") + 3u;
  const std::size_t authorityEnd = input.find_first_of("/?#", authorityStart);
  if (input.find_first_of("?#", authorityStart) != std::string::npos) return false;
  const std::size_t authorityLength = authorityEnd == std::string::npos
      ? input.size() - authorityStart : authorityEnd - authorityStart;
  if (authorityLength == 0) return false;
  const std::string authority = input.substr(authorityStart, authorityLength);
  if (authority.find('@') != std::string::npos) return false;
  for (unsigned char value : authority) if (std::isspace(value) != 0 || isControl(value)) return false;
  return true;
}

bool validSessionKey(const std::string& input) noexcept {
  if (input.size() != ProtocolLimits::sessionKeyLength) return false;
  for (unsigned char value : input) {
    if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'))) return false;
  }
  return true;
}

bool validCommit(const std::string& input) noexcept {
  if (input.size() < 7 || input.size() > ProtocolLimits::worktreeCommitMax) return false;
  for (unsigned char value : input) {
    if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f') || (value >= 'A' && value <= 'F'))) return false;
  }
  return true;
}

bool validEndpointLabel(const std::string& input) noexcept {
  if (input.find("://") != std::string::npos || input.find('@') != std::string::npos) return false;
  for (unsigned char value : input) if (value <= 31u || value == 127u) return false;
  return true;
}

template <typename T>
std::optional<T> boundedCount(std::optional<T> value, T maximum) noexcept {
  if (!value || *value < 0 || *value > maximum) return std::nullopt;
  return value;
}

const char* statusNameImpl(Status value) noexcept {
  switch (value) {
    case Status::running: return "running";
    case Status::waiting: return "waiting";
    case Status::blocked: return "blocked";
    case Status::landed: return "landed";
    case Status::failed: return "failed";
  }
  return "running";
}

const char* evidenceStateNameImpl(EvidenceState value) noexcept {
  switch (value) {
    case EvidenceState::pending: return "pending";
    case EvidenceState::running: return "running";
    case EvidenceState::verified: return "verified";
    case EvidenceState::failed: return "failed";
  }
  return "pending";
}

const char* progressStateNameImpl(ProgressState value) noexcept {
  switch (value) {
    case ProgressState::running: return "running";
    case ProgressState::waiting: return "waiting";
    case ProgressState::blocked: return "blocked";
    case ProgressState::completed: return "completed";
  }
  return "running";
}

const char* lowlevelStateNameImpl(LowlevelState value) noexcept {
  switch (value) {
    case LowlevelState::configured: return "configured";
    case LowlevelState::reachable: return "reachable";
    case LowlevelState::unavailable: return "unavailable";
    case LowlevelState::stale: return "stale";
  }
  return "configured";
}

bool validStatusValue(Status value) noexcept {
  return value == Status::running || value == Status::waiting || value == Status::blocked
      || value == Status::landed || value == Status::failed;
}

bool validEvidenceStateValue(EvidenceState value) noexcept {
  return value == EvidenceState::pending || value == EvidenceState::running
      || value == EvidenceState::verified || value == EvidenceState::failed;
}

bool validProgressStateValue(ProgressState value) noexcept {
  return value == ProgressState::running || value == ProgressState::waiting
      || value == ProgressState::blocked || value == ProgressState::completed;
}

bool validLowlevelStateValue(LowlevelState value) noexcept {
  return value == LowlevelState::configured || value == LowlevelState::reachable
      || value == LowlevelState::unavailable || value == LowlevelState::stale;
}

bool fillOsRandom(unsigned char* bytes, std::size_t length) noexcept {
  if (bytes == nullptr || length == 0) return false;
#if defined(_WIN32)
  return BCryptGenRandom(nullptr, bytes, static_cast<ULONG>(length), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
#  if defined(__linux__)
  {
    std::size_t offset = 0;
    while (offset < length) {
      const ssize_t count = getrandom(bytes + offset, length - offset, 0);
      if (count > 0) { offset += static_cast<std::size_t>(count); continue; }
      if (count < 0 && errno == EINTR) continue;
      if (count < 0 && errno != ENOSYS && errno != EINVAL) return false;
      break;
    }
    if (offset == length) return true;
  }
#  endif
  const int descriptor = open("/dev/urandom", O_RDONLY
#  if defined(O_CLOEXEC)
      | O_CLOEXEC
#  endif
  );
  if (descriptor < 0) return false;
  std::size_t offset = 0;
  while (offset < length) {
    const ssize_t count = read(descriptor, bytes + offset, length - offset);
    if (count > 0) { offset += static_cast<std::size_t>(count); continue; }
    if (count < 0 && errno == EINTR) continue;
    close(descriptor);
    return false;
  }
  close(descriptor);
  return true;
#endif
}

std::string jsonEscape(const std::string& value) noexcept {
  try {
    std::string output;
    output.reserve(value.size() + 2);
    output.push_back('"');
    static constexpr char hex[] = "0123456789abcdef";
    for (unsigned char current : value) {
      switch (current) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
          if (current < 32u) {
            output += "\\u00";
            output.push_back(hex[(current >> 4u) & 0x0fu]);
            output.push_back(hex[current & 0x0fu]);
          } else {
            output.push_back(static_cast<char>(current));
          }
          break;
      }
    }
    output.push_back('"');
    return output;
  } catch (...) {
    return "\"\"";
  }
}

struct JsonObject final {
  std::string value = "{";
  bool first = true;

  void separator() noexcept {
    try {
      if (!first) value.push_back(',');
      first = false;
    } catch (...) {
    }
  }

  void stringField(const std::string& key, const std::string& fieldValue) noexcept {
    try {
      separator();
      value += jsonEscape(key);
      value.push_back(':');
      value += jsonEscape(fieldValue);
    } catch (...) {
    }
  }

  void boolField(const std::string& key, bool fieldValue) noexcept {
    try {
      separator();
      value += jsonEscape(key);
      value += fieldValue ? ":true" : ":false";
    } catch (...) {
    }
  }

  void intField(const std::string& key, std::int64_t fieldValue) noexcept {
    try {
      separator();
      value += jsonEscape(key);
      value.push_back(':');
      value += std::to_string(fieldValue);
    } catch (...) {
    }
  }

  void nullableBoolField(const std::string& key, const std::optional<bool>& fieldValue) noexcept {
    try {
      separator();
      value += jsonEscape(key);
      value.push_back(':');
      value += fieldValue ? (*fieldValue ? "true" : "false") : "null";
    } catch (...) {
    }
  }

  void nullableIntField(const std::string& key, const std::optional<std::int64_t>& fieldValue) noexcept {
    try {
      separator();
      value += jsonEscape(key);
      value.push_back(':');
      value += fieldValue ? std::to_string(*fieldValue) : "null";
    } catch (...) {
    }
  }

  void rawField(const std::string& key, const std::string& raw) noexcept {
    try {
      separator();
      value += jsonEscape(key);
      value.push_back(':');
      value += raw;
    } catch (...) {
    }
  }

  std::string finish() noexcept {
    try { value.push_back('}'); return value; } catch (...) { return "{}"; }
  }
};

std::string jsonArray(const std::vector<std::string>& values) noexcept {
  try {
    std::string output = "[";
    for (std::size_t index = 0; index < values.size(); ++index) {
      if (index > 0) output.push_back(',');
      output += jsonEscape(values[index]);
    }
    output.push_back(']');
    return output;
  } catch (...) {
    return "[]";
  }
}

std::string percentEncode(const std::string& value) noexcept {
  // Session ids are already restricted to the protocol's identifier alphabet,
  // but retaining a small encoder keeps custom callers safe at the path edge.
  try {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string output;
    for (unsigned char current : value) {
      if (std::isalnum(current) != 0 || current == '.' || current == '_' || current == ':' || current == '-') {
        output.push_back(static_cast<char>(current));
      } else {
        output.push_back('%');
        output.push_back(hex[(current >> 4u) & 0x0fu]);
        output.push_back(hex[current & 0x0fu]);
      }
    }
    return output;
  } catch (...) {
    return {};
  }
}

std::string trimTrailingSlashes(std::string value) noexcept {
  try {
    while (!value.empty() && value.back() == '/') value.pop_back();
    return value;
  } catch (...) {
    return {};
  }
}

std::string headerValue(const HttpResponse& response, const std::string& wanted) noexcept {
  try {
    for (const auto& header : response.headers) {
      std::string name = header.first;
      std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
      if (name == wanted) return header.second;
    }
  } catch (...) {
  }
  return {};
}

std::optional<std::string> optionalHeaderValue(const HttpResponse& response, const std::string& wanted) noexcept {
  try {
    for (const auto& header : response.headers) {
      std::string name = header.first;
      std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
      });
      if (name == wanted) return header.second;
    }
  } catch (...) {
  }
  return std::nullopt;
}

std::int64_t daysFromCivil(int year, unsigned month, unsigned day) noexcept {
  year -= month <= 2u;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
  const unsigned adjustedMonth = month > 2u ? month - 3u : month + 9u;
  const unsigned dayOfYear = (153u * adjustedMonth + 2u) / 5u + day - 1u;
  const unsigned dayOfEra = yearOfEra * 365u + yearOfEra / 4u - yearOfEra / 100u + dayOfYear;
  return static_cast<std::int64_t>(era) * 146097 + static_cast<std::int64_t>(dayOfEra) - 719468;
}

std::optional<std::int64_t> parseHttpDateMs(const std::string& input) noexcept {
  try {
    std::string value = cleanString(input, 128);
    const std::size_t comma = value.find(',');
    int day = 0;
    int year = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    std::string month;
    std::string time;
    if (comma != std::string::npos) {
      std::istringstream stream(value.substr(comma + 1));
      std::string dateToken;
      std::string yearToken;
      std::string zone;
      stream >> dateToken;
      if (dateToken.find('-') != std::string::npos) {
        // RFC 850: Wednesday, 21-Nov-94 08:48:37 GMT
        stream >> time >> zone;
        const std::size_t firstDash = dateToken.find('-');
        const std::size_t secondDash = dateToken.find('-', firstDash == std::string::npos ? 0 : firstDash + 1);
        if (firstDash == std::string::npos || secondDash == std::string::npos) return std::nullopt;
        day = std::stoi(dateToken.substr(0, firstDash));
        month = dateToken.substr(firstDash + 1, secondDash - firstDash - 1);
        year = std::stoi(dateToken.substr(secondDash + 1));
        if (year < 70) year += 2000; else if (year < 100) year += 1900;
      } else {
        stream >> month >> yearToken >> time >> zone;
        if (dateToken.empty() || yearToken.empty()) return std::nullopt;
        day = std::stoi(dateToken);
        year = std::stoi(yearToken);
      }
      if (time.empty() || zone != "GMT") return std::nullopt;
    } else {
      // ANSI C asctime form: Wed Nov 21 08:48:37 1994
      std::istringstream stream(value);
      std::string weekday;
      std::string yearToken;
      stream >> weekday >> month >> day >> time >> yearToken;
      year = std::stoi(yearToken);
    }
    const std::array<std::string, 12> months = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    auto monthIt = std::find(months.begin(), months.end(), month);
    if (monthIt == months.end()) return std::nullopt;
    const unsigned monthNumber = static_cast<unsigned>(std::distance(months.begin(), monthIt)) + 1u;
    const std::size_t firstColon = time.find(':');
    const std::size_t secondColon = time.find(':', firstColon == std::string::npos ? 0 : firstColon + 1);
    if (firstColon == std::string::npos || secondColon == std::string::npos) return std::nullopt;
    hour = std::stoi(time.substr(0, firstColon));
    minute = std::stoi(time.substr(firstColon + 1, secondColon - firstColon - 1));
    second = std::stoi(time.substr(secondColon + 1));
    if (year < 1970 || day < 1 || day > 31 || hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 60) return std::nullopt;
    const std::int64_t epochSeconds = daysFromCivil(year, monthNumber, static_cast<unsigned>(day)) * 86400
        + static_cast<std::int64_t>(hour) * 3600 + static_cast<std::int64_t>(minute) * 60 + second;
    return epochSeconds * 1000;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<int> parseRetryAfter(const HttpResponse& response) noexcept {
  const auto value = optionalHeaderValue(response, "retry-after");
  if (!value || value->empty()) return std::nullopt;
  try {
    const std::string text = cleanString(*value, 128);
    if (text.empty()) return std::nullopt;
    bool digits = true;
    for (unsigned char character : text) if (character < '0' || character > '9') digits = false;
    if (digits) {
      const auto seconds = std::stoll(text);
      if (seconds < 0) return std::nullopt;
      return static_cast<int>(std::min<std::int64_t>(seconds * 1000, std::numeric_limits<int>::max()));
    }
    const auto targetMs = parseHttpDateMs(text);
    if (!targetMs) return std::nullopt;
    const auto remaining = *targetMs - systemNowMs();
    return static_cast<int>(std::clamp<std::int64_t>(remaining, 0, std::numeric_limits<int>::max()));
  } catch (...) {
    return std::nullopt;
  }
}

class JsonResponseValidator final {
public:
  explicit JsonResponseValidator(const std::string& value) noexcept : text(value) {}

  bool completeNonEmptyObject() noexcept {
    try {
      skipWhitespace();
      bool nonEmpty = false;
      if (!parseObject(nonEmpty, 0)) return false;
      skipWhitespace();
      return nonEmpty && position == text.size();
    } catch (...) {
      return false;
    }
  }

private:
  const std::string& text;
  std::size_t position = 0;

  void skipWhitespace() noexcept {
    while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) != 0) ++position;
  }

  bool parseString() noexcept {
    if (position >= text.size() || text[position] != '"') return false;
    ++position;
    while (position < text.size()) {
      const unsigned char current = static_cast<unsigned char>(text[position++]);
      if (current == '"') return true;
      if (current < 32u) return false;
      if (current != '\\') continue;
      if (position >= text.size()) return false;
      const char escaped = text[position++];
      if (escaped == 'u') {
        if (position + 4u > text.size()) return false;
        for (std::size_t index = 0; index < 4u; ++index) {
          const unsigned char hex = static_cast<unsigned char>(text[position++]);
          if (!std::isxdigit(hex)) return false;
        }
      } else if (escaped != '"' && escaped != '\\' && escaped != '/' && escaped != 'b' && escaped != 'f' && escaped != 'n' && escaped != 'r' && escaped != 't') {
        return false;
      }
    }
    return false;
  }

  bool parseNumber() noexcept {
    const std::size_t start = position;
    if (position < text.size() && text[position] == '-') ++position;
    if (position >= text.size()) return false;
    if (text[position] == '0') {
      ++position;
      if (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])) != 0) return false;
    } else {
      if (std::isdigit(static_cast<unsigned char>(text[position])) == 0) return false;
      while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])) != 0) ++position;
    }
    if (position < text.size() && text[position] == '.') {
      ++position;
      const std::size_t fractionStart = position;
      while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])) != 0) ++position;
      if (position == fractionStart) return false;
    }
    if (position < text.size() && (text[position] == 'e' || text[position] == 'E')) {
      ++position;
      if (position < text.size() && (text[position] == '+' || text[position] == '-')) ++position;
      const std::size_t exponentStart = position;
      while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])) != 0) ++position;
      if (position == exponentStart) return false;
    }
    return position > start;
  }

  bool parseArray(int depth) noexcept {
    if (depth > 32 || position >= text.size() || text[position] != '[') return false;
    ++position;
    skipWhitespace();
    if (position < text.size() && text[position] == ']') { ++position; return true; }
    for (;;) {
      if (!parseValue(depth + 1)) return false;
      skipWhitespace();
      if (position >= text.size()) return false;
      if (text[position] == ']') { ++position; return true; }
      if (text[position++] != ',') return false;
      skipWhitespace();
    }
  }

  bool parseObject(bool& nonEmpty, int depth) noexcept {
    if (depth > 32 || position >= text.size() || text[position] != '{') return false;
    ++position;
    skipWhitespace();
    if (position < text.size() && text[position] == '}') { ++position; return true; }
    for (;;) {
      if (!parseString()) return false;
      skipWhitespace();
      if (position >= text.size() || text[position++] != ':') return false;
      skipWhitespace();
      if (!parseValue(depth + 1)) return false;
      nonEmpty = true;
      skipWhitespace();
      if (position >= text.size()) return false;
      if (text[position] == '}') { ++position; return true; }
      if (text[position++] != ',') return false;
      skipWhitespace();
    }
  }

  bool parseValue(int depth) noexcept {
    if (depth > 32) return false;
    skipWhitespace();
    if (position >= text.size()) return false;
    switch (text[position]) {
      case '"': return parseString();
      case '{': { bool ignored = false; return parseObject(ignored, depth); }
      case '[': return parseArray(depth);
      case 't': if (text.compare(position, 4, "true") == 0) { position += 4; return true; } return false;
      case 'f': if (text.compare(position, 5, "false") == 0) { position += 5; return true; } return false;
      case 'n': if (text.compare(position, 4, "null") == 0) { position += 4; return true; } return false;
      default: return parseNumber();
    }
  }
};

[[maybe_unused]] bool jsonShapeLooksValid(const std::string& body) noexcept {
  return JsonResponseValidator(body).completeNonEmptyObject();
}

// Parsed route responses retain bounded structure after validation.  Callers
// never search the raw body for field names, so a decoy string cannot become
// an error, cursor, or reply field.
struct JsonNode final {
  enum class Kind { nullValue, boolean, number, string, array, object };
  Kind kind = Kind::nullValue;
  bool boolValue = false;
  double numberValue = 0.0;
  std::string stringValue;
  std::vector<std::unique_ptr<JsonNode>> arrayValue;
  std::vector<std::pair<std::string, std::unique_ptr<JsonNode>>> objectValue;

  const JsonNode* get(const std::string& key) const noexcept {
    for (const auto& member : objectValue) if (member.first == key) return member.second.get();
    return nullptr;
  }

  std::optional<std::int64_t> integer() const noexcept {
    if (kind != Kind::number || !std::isfinite(numberValue) || std::floor(numberValue) != numberValue
        || numberValue < static_cast<double>(std::numeric_limits<std::int64_t>::min())
        || numberValue > static_cast<double>(std::numeric_limits<std::int64_t>::max())) return std::nullopt;
    return static_cast<std::int64_t>(numberValue);
  }
};

class JsonDocumentParser final {
public:
  explicit JsonDocumentParser(const std::string& value) noexcept : text(value) {}

  bool duplicateKey() const noexcept { return duplicateKeyDetected; }
  bool nodeBudgetExceeded() const noexcept { return nodeBudgetHit; }

  bool parseRoot(std::unique_ptr<JsonNode>& result) noexcept {
    try {
      skipWhitespace();
      if (!parseValue(result, 0) || !result || result->kind != JsonNode::Kind::object || result->objectValue.empty()) return false;
      skipWhitespace();
      return position == text.size();
    } catch (...) {
      return false;
    }
  }

private:
  const std::string& text;
  std::size_t position = 0;
  std::size_t nodeCount = 0;
  bool duplicateKeyDetected = false;
  bool nodeBudgetHit = false;

  void skipWhitespace() noexcept { while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position])) != 0) ++position; }

  bool parseString(std::string& output) noexcept {
    if (position >= text.size() || text[position] != '"') return false;
    ++position;
    output.clear();
    while (position < text.size()) {
      const unsigned char current = static_cast<unsigned char>(text[position++]);
      if (current == '"') return true;
      if (current < 32u) return false;
      if (current != '\\') { output.push_back(static_cast<char>(current)); continue; }
      if (position >= text.size()) return false;
      const char escaped = text[position++];
      switch (escaped) {
        case '"': output.push_back('"'); break;
        case '\\': output.push_back('\\'); break;
        case '/': output.push_back('/'); break;
        case 'b': output.push_back('\b'); break;
        case 'f': output.push_back('\f'); break;
        case 'n': output.push_back('\n'); break;
        case 'r': output.push_back('\r'); break;
        case 't': output.push_back('\t'); break;
        case 'u':
          if (position + 4u > text.size()) return false;
          for (std::size_t index = 0; index < 4u; ++index) if (!std::isxdigit(static_cast<unsigned char>(text[position++]))) return false;
          break;
        default: return false;
      }
    }
    return false;
  }

  bool parseNumber(double& output) noexcept {
    const std::size_t start = position;
    if (position < text.size() && text[position] == '-') ++position;
    if (position >= text.size()) return false;
    if (text[position] == '0') {
      ++position;
      if (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])) != 0) return false;
    } else {
      if (std::isdigit(static_cast<unsigned char>(text[position])) == 0) return false;
      while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])) != 0) ++position;
    }
    if (position < text.size() && text[position] == '.') {
      ++position;
      const std::size_t fractionStart = position;
      while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])) != 0) ++position;
      if (position == fractionStart) return false;
    }
    if (position < text.size() && (text[position] == 'e' || text[position] == 'E')) {
      ++position;
      if (position < text.size() && (text[position] == '+' || text[position] == '-')) ++position;
      const std::size_t exponentStart = position;
      while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position])) != 0) ++position;
      if (position == exponentStart) return false;
    }
    try { output = std::stod(text.substr(start, position - start)); return std::isfinite(output); } catch (...) { return false; }
  }

  bool parseValue(std::unique_ptr<JsonNode>& output, int depth) noexcept {
    if (depth > 32) return false;
    skipWhitespace();
    if (position >= text.size()) return false;
    if (++nodeCount > ProtocolLimits::maxJsonNodes) { nodeBudgetHit = true; return false; }
    auto node = std::make_unique<JsonNode>();
    if (text[position] == '"') {
      node->kind = JsonNode::Kind::string;
      if (!parseString(node->stringValue)) return false;
    } else if (text[position] == '{') {
      node->kind = JsonNode::Kind::object;
      if (!parseObject(*node, depth)) return false;
    } else if (text[position] == '[') {
      node->kind = JsonNode::Kind::array;
      if (!parseArray(*node, depth)) return false;
    } else if (text.compare(position, 4, "true") == 0) {
      node->kind = JsonNode::Kind::boolean; node->boolValue = true; position += 4;
    } else if (text.compare(position, 5, "false") == 0) {
      node->kind = JsonNode::Kind::boolean; node->boolValue = false; position += 5;
    } else if (text.compare(position, 4, "null") == 0) {
      position += 4;
    } else {
      node->kind = JsonNode::Kind::number;
      if (!parseNumber(node->numberValue)) return false;
    }
    output = std::move(node);
    return true;
  }

  bool parseArray(JsonNode& node, int depth) noexcept {
    ++position; skipWhitespace();
    if (position < text.size() && text[position] == ']') { ++position; return true; }
    for (;;) {
      std::unique_ptr<JsonNode> child;
      if (!parseValue(child, depth + 1)) return false;
      node.arrayValue.push_back(std::move(child));
      skipWhitespace();
      if (position >= text.size()) return false;
      if (text[position] == ']') { ++position; return true; }
      if (text[position++] != ',') return false;
      skipWhitespace();
    }
  }

  bool parseObject(JsonNode& node, int depth) noexcept {
    ++position; skipWhitespace();
    if (position < text.size() && text[position] == '}') { ++position; return true; }
    for (;;) {
      std::string key;
      if (!parseString(key)) return false;
      for (const auto& member : node.objectValue) if (member.first == key) { duplicateKeyDetected = true; return false; }
      skipWhitespace();
      if (position >= text.size() || text[position++] != ':') return false;
      std::unique_ptr<JsonNode> child;
      if (!parseValue(child, depth + 1)) return false;
      node.objectValue.emplace_back(std::move(key), std::move(child));
      skipWhitespace();
      if (position >= text.size()) return false;
      if (text[position] == '}') { ++position; return true; }
      if (text[position++] != ',') return false;
      skipWhitespace();
    }
  }
};

std::string serializeJsonNode(const JsonNode& node) noexcept {
  try {
    switch (node.kind) {
      case JsonNode::Kind::nullValue: return "null";
      case JsonNode::Kind::boolean: return node.boolValue ? "true" : "false";
      case JsonNode::Kind::number: return std::to_string(node.numberValue);
      case JsonNode::Kind::string: return jsonEscape(node.stringValue);
      case JsonNode::Kind::array: {
        std::string value = "[";
        for (std::size_t index = 0; index < node.arrayValue.size(); ++index) { if (index > 0) value.push_back(','); value += serializeJsonNode(*node.arrayValue[index]); }
        value.push_back(']'); return value;
      }
      case JsonNode::Kind::object: {
        std::string value = "{";
        for (std::size_t index = 0; index < node.objectValue.size(); ++index) { if (index > 0) value.push_back(','); value += jsonEscape(node.objectValue[index].first) + ":" + serializeJsonNode(*node.objectValue[index].second); }
        value.push_back('}'); return value;
      }
    }
  } catch (...) {
  }
  return "null";
}

struct ParsedHttpResponse final {
  HttpResponse wire;
  std::unique_ptr<JsonNode> json;
};

struct StringField final {
  std::string path;
  std::string value;
};

struct PayloadJson final {
  std::string body;
  std::vector<StringField> strings;
};

void rememberString(std::vector<StringField>& strings, const std::string& path,
                    const std::string& value) noexcept {
  try { strings.push_back(StringField{path, value}); } catch (...) {}
}

std::string evidenceJson(const std::vector<EvidenceItem>& items,
                         std::vector<StringField>& strings) noexcept {
  try {
    std::string output = "[";
    for (std::size_t index = 0; index < items.size(); ++index) {
      if (index > 0) output.push_back(',');
      const EvidenceItem& item = items[index];
      JsonObject object;
      object.stringField("id", item.id);
      rememberString(strings, "/evidence/" + std::to_string(index) + "/id", item.id);
      object.stringField("label", item.label);
      rememberString(strings, "/evidence/" + std::to_string(index) + "/label", item.label);
      object.stringField("url", item.url);
      rememberString(strings, "/evidence/" + std::to_string(index) + "/url", item.url);
      object.stringField("state", evidenceStateNameImpl(item.state));
      output += object.finish();
    }
    output.push_back(']');
    return output;
  } catch (...) {
    return "[]";
  }
}

std::string worktreesJson(const std::vector<Worktree>& items,
                          std::vector<StringField>& strings) noexcept {
  try {
    std::string output = "[";
    for (std::size_t index = 0; index < items.size(); ++index) {
      if (index > 0) output.push_back(',');
      const Worktree& item = items[index];
      JsonObject object;
      object.stringField("path", item.path);
      rememberString(strings, "/worktrees/" + std::to_string(index) + "/path", item.path);
      object.stringField("branch", item.branch);
      rememberString(strings, "/worktrees/" + std::to_string(index) + "/branch", item.branch);
      object.stringField("commit", item.commit);
      rememberString(strings, "/worktrees/" + std::to_string(index) + "/commit", item.commit);
      object.intField("bytes", item.bytes);
      object.boolField("dirty", item.dirty);
      output += object.finish();
    }
    output.push_back(']');
    return output;
  } catch (...) {
    return "[]";
  }
}

std::string progressJson(const Progress& progress,
                         std::vector<StringField>& strings) noexcept {
  try {
    JsonObject object;
    if (progress.completedSteps || progress.totalSteps) {
      object.intField("completedSteps", progress.completedSteps.value_or(0));
      object.intField("totalSteps", progress.totalSteps.value_or(0));
    } else if (progress.percent) {
      object.intField("percent", *progress.percent);
    }
    object.stringField("currentStep", progress.currentStep);
    rememberString(strings, "/progress/currentStep", progress.currentStep);
    object.stringField("nextStep", progress.nextStep);
    rememberString(strings, "/progress/nextStep", progress.nextStep);
    object.stringField("state", progressStateNameImpl(progress.state));
    return object.finish();
  } catch (...) {
    return "{}";
  }
}

std::string lowlevelJson(const Lowlevel& lowlevel,
                         std::vector<StringField>& strings) noexcept {
  try {
    JsonObject object;
    object.stringField("state", lowlevelStateNameImpl(lowlevel.state));
    object.nullableBoolField("cheapHeadless", lowlevel.cheapHeadless);
    object.stringField("transport", lowlevel.transport);
    rememberString(strings, "/lowlevel/transport", lowlevel.transport);
    object.stringField("endpointLabel", lowlevel.endpointLabel);
    rememberString(strings, "/lowlevel/endpointLabel", lowlevel.endpointLabel);
    object.stringField("version", lowlevel.version);
    rememberString(strings, "/lowlevel/version", lowlevel.version);
    object.stringField("lastHeartbeat", lowlevel.lastHeartbeat);
    rememberString(strings, "/lowlevel/lastHeartbeat", lowlevel.lastHeartbeat);
    object.nullableIntField("headlessDesktopCount", lowlevel.headlessDesktopCount);
    object.nullableIntField("headlessWindowCount", lowlevel.headlessWindowCount);
    object.stringField("diagnostic", lowlevel.diagnostic);
    rememberString(strings, "/lowlevel/diagnostic", lowlevel.diagnostic);
    return object.finish();
  } catch (...) {
    return "{}";
  }
}

bool containsIdentity(const std::string& value, const std::string& identity) noexcept {
  return !identity.empty() && value.find(identity) != std::string::npos;
}

struct NodeCredentials final {
  std::string password;
  std::string identity;
};

struct ProcessResult final {
  int exitCode = -1;
  bool timedOut = false;
  ResultCode code = ResultCode::ok;
  std::string error;
  std::string output;
};

constexpr std::size_t kChildOutputMaxBytes = 128u * 1024u;
constexpr std::size_t kWorktreeWalkEntriesMax = 4096u;

bool appendProcessOutput(ProcessResult& result, const char* data, std::size_t count) noexcept {
  try {
    if (count > kChildOutputMaxBytes || result.output.size() > kChildOutputMaxBytes - count) {
      result.code = ResultCode::child_output_overflow;
      result.error = "The child process output exceeded the bounded limit.";
      return false;
    }
    result.output.append(data, count);
    return true;
  } catch (...) {
    result.code = ResultCode::child_output_overflow;
    result.error = "The child process output could not remain bounded.";
    return false;
  }
}

struct JoinThread final {
  std::thread value;
  ~JoinThread() noexcept {
    try {
      if (value.joinable()) value.join();
    } catch (...) {
    }
  }
};

std::string quoteWindowsArg(const std::string& input) noexcept {
#if defined(_WIN32)
  try {
    std::string output = "\"";
    std::size_t slashes = 0;
    for (char current : input) {
      if (current == '\\') { ++slashes; continue; }
      if (current == '"') {
        output.append(slashes * 2u + 1u, '\\');
        output.push_back('"');
        slashes = 0;
        continue;
      }
      output.append(slashes, '\\');
      slashes = 0;
      output.push_back(current);
    }
    output.append(slashes * 2u, '\\');
    output.push_back('"');
    return output;
  } catch (...) {
    return "\"\"";
  }
#else
  return input;
#endif
}

ProcessResult runProcess(const std::string& executable, const std::vector<std::string>& arguments,
                         const std::string& input, int timeoutMs) noexcept {
  ProcessResult result;
  try {
    if (executable.empty() || timeoutMs <= 0) {
      result.timedOut = timeoutMs <= 0;
      return result;
    }
#if defined(_WIN32)
  HANDLE childStdoutRead = nullptr;
  HANDLE childStdoutWrite = nullptr;
  HANDLE childStdinRead = nullptr;
  HANDLE childStdinWrite = nullptr;
  SECURITY_ATTRIBUTES attributes{};
  attributes.nLength = sizeof(attributes);
  attributes.bInheritHandle = TRUE;
  if (!CreatePipe(&childStdoutRead, &childStdoutWrite, &attributes, 0)
      || !CreatePipe(&childStdinRead, &childStdinWrite, &attributes, 0)) {
    if (childStdoutRead) CloseHandle(childStdoutRead);
    if (childStdoutWrite) CloseHandle(childStdoutWrite);
    if (childStdinRead) CloseHandle(childStdinRead);
    if (childStdinWrite) CloseHandle(childStdinWrite);
    return result;
  }
  SetHandleInformation(childStdoutRead, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(childStdinWrite, HANDLE_FLAG_INHERIT, 0);
  std::string command = quoteWindowsArg(executable);
  for (const auto& argument : arguments) { command.push_back(' '); command += quoteWindowsArg(argument); }
  std::vector<char> commandLine(command.begin(), command.end());
  commandLine.push_back('\0');
  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = childStdinRead;
  startup.hStdOutput = childStdoutWrite;
  startup.hStdError = childStdoutWrite;
  PROCESS_INFORMATION process{};
  const BOOL created = CreateProcessA(nullptr, commandLine.data(), nullptr, nullptr, TRUE,
                                      CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
  CloseHandle(childStdinRead);
  CloseHandle(childStdoutWrite);
  if (!created) {
    CloseHandle(childStdoutRead);
    CloseHandle(childStdinWrite);
    return result;
  }
  std::atomic<bool> inputFailure{false};
  JoinThread writer;
  try {
    writer.value = std::thread([&]() {
      const char* data = input.data();
      std::size_t remaining = input.size();
      while (remaining > 0) {
        DWORD written = 0;
        if (!WriteFile(childStdinWrite, data, static_cast<DWORD>(std::min<std::size_t>(remaining, 32768u)), &written, nullptr) || written == 0) {
          if (GetLastError() == ERROR_BROKEN_PIPE) inputFailure.store(true);
          break;
        }
        data += written;
        remaining -= written;
      }
      CloseHandle(childStdinWrite);
    });
  } catch (...) {
    CloseHandle(childStdinWrite);
    TerminateProcess(process.hProcess, 125);
    WaitForSingleObject(process.hProcess, 1000);
    CloseHandle(childStdoutRead);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    result.error = "The process writer could not be started.";
    return result;
  }
  const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
  for (;;) {
    DWORD available = 0;
    if (inputFailure.load()) {
      result.code = ResultCode::child_input_failed;
      result.error = "The child process closed its input pipe.";
      TerminateProcess(process.hProcess, 126);
      WaitForSingleObject(process.hProcess, 1000);
      break;
    }
    if (PeekNamedPipe(childStdoutRead, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
      std::array<char, 8192> buffer{};
      DWORD read = 0;
      if (ReadFile(childStdoutRead, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read > 0
          && !appendProcessOutput(result, buffer.data(), read)) {
        TerminateProcess(process.hProcess, 127);
        WaitForSingleObject(process.hProcess, 1000);
        break;
      }
    }
    const DWORD wait = WaitForSingleObject(process.hProcess, 20);
    if (wait == WAIT_OBJECT_0) break;
    if (Clock::now() >= deadline) {
      result.timedOut = true;
      TerminateProcess(process.hProcess, 124);
      WaitForSingleObject(process.hProcess, 1000);
      break;
    }
  }
  std::array<char, 8192> buffer{};
  DWORD read = 0;
  while (result.code != ResultCode::child_output_overflow
      && ReadFile(childStdoutRead, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) && read > 0) {
    if (!appendProcessOutput(result, buffer.data(), read)) break;
  }
  DWORD exitCode = 1;
  GetExitCodeProcess(process.hProcess, &exitCode);
  result.exitCode = static_cast<int>(exitCode);
  writer.value.join();
  if (inputFailure.load() && result.code == ResultCode::ok) {
    result.code = ResultCode::child_input_failed;
    result.error = "The child process closed its input pipe.";
  }
  CloseHandle(childStdoutRead);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
#else
  int stdinPipe[2] = {-1, -1};
  int stdoutPipe[2] = {-1, -1};
  if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0) {
    if (stdinPipe[0] >= 0) close(stdinPipe[0]);
    if (stdinPipe[1] >= 0) close(stdinPipe[1]);
    if (stdoutPipe[0] >= 0) close(stdoutPipe[0]);
    if (stdoutPipe[1] >= 0) close(stdoutPipe[1]);
    return result;
  }
  const pid_t child = fork();
  if (child < 0) {
    close(stdinPipe[0]); close(stdinPipe[1]); close(stdoutPipe[0]); close(stdoutPipe[1]);
    return result;
  }
  if (child == 0) {
    dup2(stdinPipe[0], STDIN_FILENO);
    dup2(stdoutPipe[1], STDOUT_FILENO);
    dup2(stdoutPipe[1], STDERR_FILENO);
    close(stdinPipe[0]); close(stdinPipe[1]); close(stdoutPipe[0]); close(stdoutPipe[1]);
    std::vector<char*> argv;
    argv.reserve(arguments.size() + 2u);
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    execvp(executable.c_str(), argv.data());
    _exit(127);
  }
  close(stdinPipe[0]);
  close(stdoutPipe[1]);
  const int flags = fcntl(stdoutPipe[0], F_GETFL, 0);
  if (flags >= 0) fcntl(stdoutPipe[0], F_SETFL, flags | O_NONBLOCK);
  std::atomic<bool> inputFailure{false};
  JoinThread writer;
  try {
    writer.value = std::thread([&]() {
      sigset_t blocked{};
      sigset_t previous{};
      if (sigemptyset(&blocked) != 0 || sigaddset(&blocked, SIGPIPE) != 0 || pthread_sigmask(SIG_BLOCK, &blocked, &previous) != 0) {
        inputFailure.store(true);
        close(stdinPipe[1]);
        return;
      }
      const char* data = input.data();
      std::size_t remaining = input.size();
      while (remaining > 0) {
        const ssize_t written = write(stdinPipe[1], data, remaining);
        if (written > 0) {
          data += written;
          remaining -= static_cast<std::size_t>(written);
          continue;
        }
        if (written < 0 && errno == EINTR) continue;
        if (written < 0 && errno == EPIPE) inputFailure.store(true);
        break;
      }
      close(stdinPipe[1]);
      pthread_sigmask(SIG_SETMASK, &previous, nullptr);
    });
  } catch (...) {
    close(stdinPipe[1]);
    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
    close(stdoutPipe[0]);
    result.error = "The process writer could not be started.";
    return result;
  }
  const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
  bool exited = false;
  int waitStatus = 0;
  while (!exited) {
    if (inputFailure.load()) {
      result.code = ResultCode::child_input_failed;
      result.error = "The child process closed its input pipe.";
      kill(child, SIGKILL);
      waitpid(child, &waitStatus, 0);
      exited = true;
      break;
    }
    std::array<char, 8192> buffer{};
    const ssize_t count = read(stdoutPipe[0], buffer.data(), buffer.size());
    if (count > 0 && !appendProcessOutput(result, buffer.data(), static_cast<std::size_t>(count))) {
      kill(child, SIGKILL);
      waitpid(child, &waitStatus, 0);
      exited = true;
      break;
    }
    pid_t waited = waitpid(child, &waitStatus, WNOHANG);
    if (waited == child) { exited = true; break; }
    if (waited < 0 && errno != EINTR) { exited = true; break; }
    if (Clock::now() >= deadline) {
      result.timedOut = true;
      kill(child, SIGKILL);
      waitpid(child, &waitStatus, 0);
      exited = true;
      break;
    }
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(stdoutPipe[0], &readSet);
    timeval waitTime{0, 20000};
    select(stdoutPipe[0] + 1, &readSet, nullptr, nullptr, &waitTime);
  }
  std::array<char, 8192> buffer{};
  ssize_t count = 0;
  while (result.code != ResultCode::child_output_overflow
      && (count = read(stdoutPipe[0], buffer.data(), buffer.size())) > 0) {
    if (!appendProcessOutput(result, buffer.data(), static_cast<std::size_t>(count))) break;
  }
  close(stdoutPipe[0]);
  writer.value.join();
  if (inputFailure.load() && result.code == ResultCode::ok) {
    result.code = ResultCode::child_input_failed;
    result.error = "The child process closed its input pipe.";
  }
  if (WIFEXITED(waitStatus)) result.exitCode = WEXITSTATUS(waitStatus);
  else if (WIFSIGNALED(waitStatus)) result.exitCode = 128 + WTERMSIG(waitStatus);
#endif
    return result;
  } catch (...) {
    result.error = "The bounded child process failed safely.";
    return result;
  }
}

std::filesystem::path findManagedFile(const std::string& explicitPath,
                                      const std::string& relativePath) noexcept {
  try {
    if (!explicitPath.empty() && std::filesystem::is_regular_file(explicitPath)) return std::filesystem::path(explicitPath);
    std::filesystem::path current = std::filesystem::current_path();
    for (int depth = 0; depth < 12; ++depth) {
      const auto candidate = current / relativePath;
      if (std::filesystem::is_regular_file(candidate)) return candidate;
      const auto parent = current.parent_path();
      if (parent == current) break;
      current = parent;
    }
  } catch (...) {
  }
  return {};
}

std::string jsonStringField(const std::string& json, const std::string& key) noexcept {
  try {
    const std::string needle = jsonEscape(key);
    const std::size_t keyPosition = json.find(needle);
    if (keyPosition == std::string::npos) return {};
    std::size_t position = json.find(':', keyPosition + needle.size());
    if (position == std::string::npos) return {};
    ++position;
    while (position < json.size() && std::isspace(static_cast<unsigned char>(json[position])) != 0) ++position;
    if (position >= json.size() || json[position] != '"') return {};
    ++position;
    std::string output;
    while (position < json.size()) {
      const char current = json[position++];
      if (current == '"') return output;
      if (current != '\\' || position >= json.size()) { output.push_back(current); continue; }
      const char escaped = json[position++];
      switch (escaped) {
        case '"': output.push_back('"'); break;
        case '\\': output.push_back('\\'); break;
        case '/': output.push_back('/'); break;
        case 'b': output.push_back('\b'); break;
        case 'f': output.push_back('\f'); break;
        case 'n': output.push_back('\n'); break;
        case 'r': output.push_back('\r'); break;
        case 't': output.push_back('\t'); break;
        default: return {};
      }
    }
  } catch (...) {
  }
  return {};
}

bool validPassword(const std::string& password) noexcept {
  if (password.size() != 64u) return false;
  for (unsigned char value : password) {
    if (!((value >= '0' && value <= '9') || (value >= 'a' && value <= 'f'))) return false;
  }
  return true;
}

std::string privateCode(const std::string& output) noexcept {
  try {
    for (const char* code : {"STALE_VOCABULARY_PASSWORD", "PRIVATE_EMISSION_REJECTED", "INVALID_JSON", "INPUT_TOO_LARGE", "GUARD_UNAVAILABLE"}) {
      if (output.find(code) != std::string::npos) return code;
    }
  } catch (...) {
  }
  return {};
}

struct CredentialCache final {
  std::mutex mutex;
  bool loaded = false;
  std::string guardPath;
  NodeCredentials credentials;
};

CredentialCache& credentialCache() noexcept {
  static CredentialCache cache;
  return cache;
}

Result<NodeCredentials> discoverNodeCredentials(const std::string& node,
                                                const std::filesystem::path& guardPath,
                                                int timeoutMs) noexcept {
  const std::string script =
      "import('node:url').then(({pathToFileURL})=>import(pathToFileURL(process.argv[1]).href)"
      ".then(m=>process.stdout.write(JSON.stringify({password:m.vocabularyPassword(),identity:m.PRIVATE_ASSISTANT_IDENTITY}))))";
  const ProcessResult process = runProcess(node, {"-e", script, guardPath.string()}, {}, timeoutMs);
  if (process.code != ResultCode::ok) return Result<NodeCredentials>::failure(process.code, process.error.empty() ? "The canonical private-emission guard probe failed." : process.error);
  if (process.timedOut) return Result<NodeCredentials>::failure(ResultCode::timeout, "The canonical private-emission guard probe timed out.");
  if (process.exitCode == 127) return Result<NodeCredentials>::failure(ResultCode::node_unavailable, "The configured Node executable could not be started.");
  if (process.exitCode != 0) return Result<NodeCredentials>::failure(ResultCode::private_guard_failed, "The canonical private-emission guard probe failed.");
  NodeCredentials credentials;
  credentials.password = jsonStringField(process.output, "password");
  credentials.identity = jsonStringField(process.output, "identity");
  if (!validPassword(credentials.password) || credentials.identity.empty()) return Result<NodeCredentials>::failure(ResultCode::private_guard_failed, "The canonical private-emission guard probe returned an incomplete interface.");
  return Result<NodeCredentials>::success(std::move(credentials));
}

Result<NodeCredentials> getNodeCredentials(const ClientOptions& options, bool refresh, int timeoutMs) noexcept {
  try {
    const std::string guardOverride = options.guardPath.empty() ? environment("STATUS_HUB_PRIVATE_EMISSION_GUARD") : options.guardPath;
    const auto guardPath = findManagedFile(guardOverride, "skills/agent-global-memory/scripts/private-emission-guard.mjs");
    if (guardPath.empty()) return Result<NodeCredentials>::failure(ResultCode::private_guard_unavailable, "The canonical private-emission guard could not be resolved.");
    std::string node = options.nodePath.empty() ? environment("STATUS_HUB_NODE") : options.nodePath;
    if (node.empty()) node = "node";
    CredentialCache& cache = credentialCache();
    std::lock_guard<std::mutex> lock(cache.mutex);
    if (!refresh && cache.loaded && cache.guardPath == guardPath.string()) return Result<NodeCredentials>::success(cache.credentials);
    auto result = discoverNodeCredentials(node, guardPath, timeoutMs);
    if (!result.ok) return result;
    cache.loaded = true;
    cache.guardPath = guardPath.string();
    cache.credentials = result.value;
    return result;
  } catch (...) {
    return Result<NodeCredentials>::failure(ResultCode::private_guard_failed, "The canonical private-emission guard could not be loaded.");
  }
}

Result<bool> runPrivatePreflight(const ClientOptions& options, const std::string& body, int timeoutMs) noexcept {
  try {
    const std::string preflightOverride = options.preflightPath.empty() ? environment("STATUS_HUB_PRIVATE_EMISSION_PREFLIGHT") : options.preflightPath;
    const auto preflightPath = findManagedFile(preflightOverride, "skills/agent-global-memory/scripts/private-emission-preflight.mjs");
    if (preflightPath.empty()) return Result<bool>::failure(ResultCode::private_guard_unavailable, "The canonical private-emission preflight could not be resolved.");
    std::string node = options.nodePath.empty() ? environment("STATUS_HUB_NODE") : options.nodePath;
    if (node.empty()) node = "node";
    const ProcessResult process = runProcess(node, {preflightPath.string()}, body, timeoutMs);
    if (process.code != ResultCode::ok) return Result<bool>::failure(process.code, process.error.empty() ? "The private-emission preflight failed." : process.error);
    if (process.timedOut) return Result<bool>::failure(ResultCode::timeout, "The private-emission preflight exceeded its deadline.");
    if (process.exitCode == 127) return Result<bool>::failure(ResultCode::node_unavailable, "The configured Node executable could not be started.");
    if (process.exitCode == 0) return Result<bool>::success(true);
    const std::string code = privateCode(process.output);
    if (code == "STALE_VOCABULARY_PASSWORD") return Result<bool>::failure(ResultCode::stale_vocabulary_password, "The private-emission vocabulary password is stale.");
    if (code == "GUARD_UNAVAILABLE") return Result<bool>::failure(ResultCode::private_guard_unavailable, "The canonical private-emission guard is unavailable.");
    return Result<bool>::failure(ResultCode::private_emission_rejected, "The canonical private-emission preflight rejected the payload.");
  } catch (...) {
    return Result<bool>::failure(ResultCode::private_preflight_failed, "The canonical private-emission preflight could not run.");
  }
}

} // namespace

const char* resultCodeName(ResultCode code) noexcept {
  switch (code) {
    case ResultCode::ok: return "ok";
    case ResultCode::coalesced: return "coalesced";
    case ResultCode::already_terminal: return "already_terminal";
    case ResultCode::closed: return "closed";
    case ResultCode::invalid_input: return "invalid_input";
    case ResultCode::invalid_session_id: return "invalid_session_id";
    case ResultCode::invalid_session_key: return "invalid_session_key";
    case ResultCode::missing_ingest_token: return "missing_ingest_token";
    case ResultCode::invalid_url: return "invalid_url";
    case ResultCode::health_failed: return "health_failed";
    case ResultCode::unreachable: return "unreachable";
    case ResultCode::transport: return "transport";
    case ResultCode::timeout: return "timeout";
    case ResultCode::response_too_large: return "response_too_large";
    case ResultCode::body_too_large: return "body_too_large";
    case ResultCode::bad_json: return "bad_json";
    case ResultCode::json_duplicate_key: return "json_duplicate_key";
    case ResultCode::json_node_budget: return "json_node_budget";
    case ResultCode::child_input_failed: return "child_input_failed";
    case ResultCode::child_output_overflow: return "child_output_overflow";
    case ResultCode::rate_limited: return "rate_limited";
    case ResultCode::http_error: return "http_error";
    case ResultCode::degraded: return "degraded";
    case ResultCode::private_guard_unavailable: return "private_guard_unavailable";
    case ResultCode::private_guard_failed: return "private_guard_failed";
    case ResultCode::private_preflight_failed: return "private_preflight_failed";
    case ResultCode::stale_vocabulary_password: return "stale_vocabulary_password";
    case ResultCode::private_emission_rejected: return "private_emission_rejected";
    case ResultCode::cursor_resync_failed: return "cursor_resync_failed";
    case ResultCode::node_unavailable: return "node_unavailable";
    case ResultCode::csprng_unavailable: return "csprng_unavailable";
    case ResultCode::worktree_incomplete: return "worktree_incomplete";
    case ResultCode::internal_error: return "internal_error";
  }
  return "internal_error";
}

const char* statusName(Status value) noexcept { return statusNameImpl(value); }
const char* evidenceStateName(EvidenceState value) noexcept { return evidenceStateNameImpl(value); }
const char* progressStateName(ProgressState value) noexcept { return progressStateNameImpl(value); }
const char* lowlevelStateName(LowlevelState value) noexcept { return lowlevelStateNameImpl(value); }

SessionState clampSession(const SessionPatch& patch, std::vector<Warning>& warnings) noexcept {
  SessionState output;
  try {
    if (patch.title) output.title = cleanString(*patch.title, ProtocolLimits::titleMax);
    if (patch.repository) output.repository = cleanString(*patch.repository, ProtocolLimits::repositoryMax);
    if (patch.branch) output.branch = cleanString(*patch.branch, ProtocolLimits::branchMax);
    if (patch.status) {
      if (validStatusValue(*patch.status)) output.status = *patch.status;
      else appendWarning(warnings, "status", "must be one of running|waiting|blocked|landed|failed");
    }
    if (patch.summary) output.summary = cleanString(*patch.summary, ProtocolLimits::summaryMax);
    if (patch.assumption) output.assumption = cleanString(*patch.assumption, ProtocolLimits::assumptionMax);
    if (patch.verifiedBaseline) output.verifiedBaseline = cleanString(*patch.verifiedBaseline, ProtocolLimits::verifiedBaselineMax);
    if (patch.machine) output.machine = cleanString(*patch.machine, ProtocolLimits::machineMax);
    if (patch.evidence) {
      if (patch.evidence->size() > ProtocolLimits::maxEvidenceItems) appendWarning(warnings, "evidence", "only the first 8 items are kept");
      for (std::size_t index = 0; index < patch.evidence->size() && index < ProtocolLimits::maxEvidenceItems; ++index) {
        const auto& item = (*patch.evidence)[index];
        EvidenceItem cleaned;
        cleaned.id = cleanId(item.id, 60);
        if (cleaned.id.empty()) cleaned.id = "evidence-" + std::to_string(index + 1);
        cleaned.label = cleanString(item.label, ProtocolLimits::evidenceLabelMax);
        cleaned.url = cleanString(item.url, ProtocolLimits::evidenceUrlMax);
        cleaned.state = validEvidenceStateValue(item.state) ? item.state : EvidenceState::pending;
        if (!validEvidenceStateValue(item.state)) appendWarning(warnings, "evidence[" + std::to_string(index) + "].state", "must be one of pending|running|verified|failed");
        if (cleaned.label.empty() || !validUrl(cleaned.url)) {
          appendWarning(warnings, "evidence[" + std::to_string(index) + "]", "needs a label and an http(s) URL without embedded credentials");
          continue;
        }
        output.evidence.push_back(std::move(cleaned));
      }
    }
    if (patch.nextGates) {
      if (patch.nextGates->size() > ProtocolLimits::maxNextGates) appendWarning(warnings, "nextGates", "only the first 8 entries are kept");
      std::unordered_set<std::string> seen;
      for (std::size_t index = 0; index < patch.nextGates->size() && index < ProtocolLimits::maxNextGates; ++index) {
        std::string gate = cleanString((*patch.nextGates)[index], ProtocolLimits::nextGateMax);
        if (!gate.empty() && seen.insert(gate).second) output.nextGates.push_back(std::move(gate));
      }
    }
    if (patch.worktrees) {
      if (patch.worktrees->size() > ProtocolLimits::maxWorktrees) appendWarning(warnings, "worktrees", "only the first 128 entries are kept");
      for (std::size_t index = 0; index < patch.worktrees->size() && index < ProtocolLimits::maxWorktrees; ++index) {
        const auto& item = (*patch.worktrees)[index];
        Worktree cleaned;
        if (item.incomplete) {
          appendWarning(warnings, "worktrees[" + std::to_string(index) + "]", "the inventory row is incomplete and is not published");
          continue;
        }
        cleaned.path = cleanString(item.path, ProtocolLimits::worktreePathMax);
        cleaned.branch = cleanString(item.branch, ProtocolLimits::worktreeBranchMax);
        cleaned.commit = cleanString(item.commit, ProtocolLimits::worktreeCommitMax);
        cleaned.bytes = item.bytes;
        cleaned.dirty = item.dirty;
        if (cleaned.path.empty() || !validCommit(cleaned.commit) || cleaned.bytes < 0) {
          appendWarning(warnings, "worktrees[" + std::to_string(index) + "]", "needs a path, a 7-64 hex commit, and a non-negative integer byte size");
          continue;
        }
        output.worktrees.push_back(std::move(cleaned));
      }
    }
    if (patch.progress) {
      Progress progress = *patch.progress;
      progress.currentStep = cleanString(progress.currentStep, ProtocolLimits::stepMax);
      progress.nextStep = cleanString(progress.nextStep, ProtocolLimits::stepMax);
      if (!validProgressStateValue(progress.state)) {
        appendWarning(warnings, "progress.state", "must be one of running|waiting|blocked|completed");
        progress.state = ProgressState::running;
      }
      const bool counts = progress.completedSteps || progress.totalSteps;
      const bool validCounts = counts && progress.completedSteps && progress.totalSteps
          && *progress.completedSteps >= 0 && *progress.totalSteps >= 0
          && *progress.totalSteps <= ProtocolLimits::maxProgressSteps
          && *progress.completedSteps <= *progress.totalSteps;
      const bool validPercent = !counts && progress.percent && *progress.percent >= 0 && *progress.percent <= 100;
      if (validCounts || validPercent) output.progress = std::move(progress);
      else appendWarning(warnings, "progress", "needs completedSteps<=totalSteps<=10000 or percent 0-100");
    }
    if (patch.lowlevel) {
      Lowlevel lowlevel = *patch.lowlevel;
      lowlevel.transport = cleanString(lowlevel.transport, ProtocolLimits::lowlevelTransportMax);
      lowlevel.endpointLabel = cleanString(lowlevel.endpointLabel, ProtocolLimits::lowlevelEndpointLabelMax);
      lowlevel.version = cleanString(lowlevel.version, ProtocolLimits::lowlevelVersionMax);
      lowlevel.lastHeartbeat = cleanString(lowlevel.lastHeartbeat, ProtocolLimits::lowlevelHeartbeatMax);
      lowlevel.diagnostic = cleanString(lowlevel.diagnostic, ProtocolLimits::lowlevelDiagnosticMax);
      lowlevel.headlessDesktopCount = boundedCount(lowlevel.headlessDesktopCount, ProtocolLimits::lowlevelCountMax);
      lowlevel.headlessWindowCount = boundedCount(lowlevel.headlessWindowCount, ProtocolLimits::lowlevelCountMax);
      if (!validLowlevelStateValue(lowlevel.state)) {
        appendWarning(warnings, "lowlevel.state", "must be one of configured|reachable|unavailable|stale");
      } else if (validEndpointLabel(lowlevel.endpointLabel)) output.lowlevel = std::move(lowlevel);
      else appendWarning(warnings, "lowlevel", "needs a valid state and an endpointLabel without ://, @, or control characters");
    }
  } catch (...) {
    appendWarning(warnings, "client", "an allocation or validation failure dropped part of the update");
  }
  return output;
}

std::optional<Question> clampQuestion(const Question& question, std::vector<Warning>& warnings) noexcept {
  try {
    Question output;
    output.id = cleanId(question.id, ProtocolLimits::questionIdMax);
    output.prompt = cleanString(question.prompt, ProtocolLimits::questionPromptMax);
    output.detail = cleanString(question.detail, ProtocolLimits::questionDetailMax);
    output.allowText = question.allowText;
    if (output.id.empty() || output.prompt.empty()) return std::nullopt;
    if (question.options.size() > ProtocolLimits::questionOptionsMax) appendWarning(warnings, "options", "only the first 8 options are kept");
    for (std::size_t index = 0; index < question.options.size() && index < ProtocolLimits::questionOptionsMax; ++index) {
      QuestionOption option;
      option.id = cleanId(question.options[index].id.empty() ? "option-" + std::to_string(index + 1) : question.options[index].id, ProtocolLimits::optionIdMax);
      option.label = cleanString(question.options[index].label, ProtocolLimits::optionLabelMax);
      if (!option.id.empty() && !option.label.empty()) output.options.push_back(std::move(option));
    }
    if (output.options.empty() && !output.allowText) return std::nullopt;
    return output;
  } catch (...) {
    return std::nullopt;
  }
}

Result<std::string> generateSessionKey() noexcept {
  try {
    std::array<unsigned char, 32> bytes{};
    if (!fillOsRandom(bytes.data(), bytes.size())) return Result<std::string>::failure(ResultCode::csprng_unavailable, "The platform CSPRNG could not provide a session key.");
    static constexpr char hex[] = "0123456789abcdef";
    std::string key;
    key.reserve(bytes.size() * 2u);
    for (unsigned char byte : bytes) {
      key.push_back(hex[(byte >> 4u) & 0x0fu]);
      key.push_back(hex[byte & 0x0fu]);
    }
    return Result<std::string>::success(std::move(key));
  } catch (...) {
    return Result<std::string>::failure(ResultCode::csprng_unavailable, "The platform CSPRNG could not provide a session key.");
  }
}

int effectiveRequestsPerMinute(int requested) noexcept {
  return std::clamp(requested, 1, ProtocolLimits::clientRequestsPerMinute);
}

std::string machineLabel() noexcept {
  std::string value = environment("COMPUTERNAME");
  if (value.empty()) value = environment("HOSTNAME");
  if (value.empty()) value = "unknown-machine";
  return cleanString(value, ProtocolLimits::machineMax);
}

Result<std::vector<Worktree>> collectWorktrees(const WorktreeReportOptions& options) noexcept {
  try {
    const std::string repository = options.repositoryPath.empty() ? "." : options.repositoryPath;
    const ProcessResult listed = runProcess("git", {"-C", repository, "worktree", "list", "--porcelain"}, {}, 10000);
    std::vector<std::string> roots;
    bool incomplete = listed.code != ResultCode::ok || listed.exitCode != 0 || listed.timedOut;
    if (!incomplete) {
      std::istringstream stream(listed.output);
      std::string line;
      while (std::getline(stream, line)) {
        if (line.rfind("worktree ", 0) == 0) {
          if (roots.size() < ProtocolLimits::maxWorktrees) roots.push_back(line.substr(9));
          else incomplete = true;
        }
      }
    }
    if (roots.empty()) { roots.push_back(repository); incomplete = true; }
    std::vector<Worktree> output;
    for (const std::string& root : roots) {
      Worktree item;
      item.incomplete = incomplete;
      try { item.path = std::filesystem::absolute(root).string(); }
      catch (...) { item.path = root; item.incomplete = true; }
      const ProcessResult commit = runProcess("git", {"-C", root, "rev-parse", "HEAD"}, {}, 10000);
      const std::string commitValue = cleanString(commit.output, ProtocolLimits::worktreeCommitMax);
      if (commit.code != ResultCode::ok || commit.exitCode != 0 || commit.timedOut || !validCommit(commitValue)) {
        item.dirty = true;
        item.incomplete = true;
        output.push_back(std::move(item));
        continue;
      }
      const ProcessResult branch = runProcess("git", {"-C", root, "rev-parse", "--abbrev-ref", "HEAD"}, {}, 10000);
      const ProcessResult dirty = runProcess("git", {"-C", root, "status", "--porcelain"}, {}, 10000);
      item.branch = cleanString(branch.exitCode == 0 && !branch.timedOut ? branch.output : std::string{}, ProtocolLimits::worktreeBranchMax);
      item.commit = commitValue;
      item.dirty = dirty.exitCode != 0 || dirty.timedOut || !cleanString(dirty.output, 1024).empty();
      if (branch.code != ResultCode::ok || branch.exitCode != 0 || branch.timedOut || dirty.code != ResultCode::ok || dirty.exitCode != 0 || dirty.timedOut) item.incomplete = true;
      const auto deadline = Clock::now() + std::chrono::milliseconds(std::max(1, options.budgetMs));
      std::vector<std::filesystem::path> queue{std::filesystem::path(root)};
      std::unordered_set<std::string> visited;
      while (!queue.empty() && Clock::now() < deadline) {
        const auto directory = queue.back();
        queue.pop_back();
        if (!visited.insert(directory.lexically_normal().string()).second) continue;
        if (visited.size() > kWorktreeWalkEntriesMax) { item.incomplete = true; break; }
        std::error_code error;
        bool stoppedByDeadline = false;
        for (std::filesystem::directory_iterator iterator(directory, std::filesystem::directory_options::skip_permission_denied, error), end; iterator != end && !error && Clock::now() < deadline; iterator.increment(error)) {
          if (Clock::now() >= deadline) { stoppedByDeadline = true; break; }
          const auto& entry = *iterator;
          const std::string name = entry.path().filename().string();
          std::error_code statusError;
          if (entry.is_symlink(statusError)) continue;
          if (entry.is_directory(statusError)) {
            if (name != "node_modules" && name != ".git" && name != "dist" && name != "dist-release" && name != "out") {
              if (queue.size() >= kWorktreeWalkEntriesMax || visited.size() + queue.size() >= kWorktreeWalkEntriesMax) item.incomplete = true;
              else queue.push_back(entry.path());
            }
          } else if (entry.is_regular_file(statusError)) {
            const auto size = entry.file_size(statusError);
            if (!statusError) item.bytes += static_cast<std::int64_t>(size);
          }
        }
        if (error) item.incomplete = true;
        if (stoppedByDeadline) item.incomplete = true;
      }
      if (!queue.empty()) item.incomplete = true;
      output.push_back(std::move(item));
    }
    if (incomplete || std::any_of(output.begin(), output.end(), [](const Worktree& item) { return item.incomplete; })) {
      Result<std::vector<Worktree>> result;
      result.code = ResultCode::worktree_incomplete;
      result.error = "The worktree inventory is incomplete; inspect the marked rows before publishing it.";
      result.value = std::move(output);
      return result;
    }
    return Result<std::vector<Worktree>>::success(std::move(output));
  } catch (...) {
    return Result<std::vector<Worktree>>::failure(ResultCode::internal_error, "The worktree inventory could not be collected.");
  }
}

struct StatusHubClient::Impl final {
  HttpTransport& transport;
  ClientOptions options;
  std::string baseUrl;
  std::string sessionId;
  std::string sessionKeyValue;
  SessionState state;
  std::unordered_set<std::string> present;
  std::vector<Warning> warningsValue;
  std::int64_t cursor = 0;
  std::int64_t lastSentAt = 0;
  std::string lastSentJson;
  std::string lastErrorValue;
  std::string lastSuccessValue;
  std::string degradedSinceValue;
  std::int64_t degradedUntil = 0;
  bool permanentDegraded = false;
  bool created = false;
  bool terminal = false;
  bool closed = false;
  std::deque<std::int64_t> requestStamps;
  std::mt19937 jitterEngine;
  // Locking policy: sendMutex serializes lifecycle/network mutations; mutex
  // protects all mutable lifecycle state. Never acquire mutex before sendMutex.
  mutable std::mutex mutex;
  mutable std::mutex sendMutex;
  std::condition_variable heartbeatCondition;
  bool heartbeatStop = false;
  std::thread heartbeatThread;
  StatusHubClient* owner = nullptr;

  explicit Impl(StatusHubClient* client, HttpTransport& value, const ClientOptions& source) noexcept
      : transport(value), options(source), baseUrl(trimTrailingSlashes(source.baseUrl)), sessionId(cleanId(source.sessionId, 120)), owner(client) {
    try {
      if (options.nowMs == nullptr) options.nowMs = systemNowMs;
      if (options.sleep == nullptr) options.sleep = [](int milliseconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(std::max(0, milliseconds)));
      };
      jitterEngine.seed(static_cast<std::mt19937::result_type>(systemNowMs()) ^ static_cast<std::mt19937::result_type>(reinterpret_cast<std::uintptr_t>(this)));
      if (baseUrl.empty()) baseUrl = trimTrailingSlashes(environment("STATUS_HUB_URL"));
      if (baseUrl.empty()) baseUrl = kDefaultBaseUrl;
      if (!validBaseUrl(baseUrl)) recordPermanent(ResultCode::invalid_url, "The Status Hub base URL must be an absolute http or https URL without embedded credentials.");
      if (options.ingestToken.empty()) options.ingestToken = environment("AGENT_INGEST_TOKEN");
      sessionKeyValue = options.sessionKey;
      bool keyGenerationFailed = false;
      if (sessionKeyValue.empty()) {
        const auto generated = options.sessionKeyGenerator ? options.sessionKeyGenerator() : generateSessionKey();
        if (generated.ok) sessionKeyValue = generated.value;
        else { keyGenerationFailed = true; recordPermanent(generated.code, generated.error); }
      }
      state.status = Status::running;
      present.insert("status");
      if (sessionId.empty()) recordPermanent(ResultCode::invalid_session_id, "The session id must use only letters, digits, dot, underscore, colon, or hyphen.");
      if (!keyGenerationFailed && !validSessionKey(sessionKeyValue)) recordPermanent(ResultCode::invalid_session_key, "The session key must be exactly 64 lowercase hexadecimal characters.");
      if (options.ingestToken.empty()) recordPermanent(ResultCode::missing_ingest_token, "AGENT_INGEST_TOKEN is not configured, so hub writes are recorded no-ops.");
      if (options.startHeartbeat && options.heartbeatMs > 0) {
        heartbeatThread = std::thread([this]() noexcept { heartbeatLoop(); });
      }
    } catch (...) {
      recordPermanent(ResultCode::internal_error, "The client could not initialize its bounded state.");
    }
  }

  ~Impl() noexcept {
    stopHeartbeat();
  }

  std::int64_t now() const noexcept {
    try { return options.nowMs ? options.nowMs() : systemNowMs(); } catch (...) { return systemNowMs(); }
  }

  void sleepFor(int milliseconds) noexcept {
    try { if (options.sleep) options.sleep(std::max(0, milliseconds)); } catch (...) {}
  }

  void recordPermanent(ResultCode code, const std::string& message) noexcept {
    try {
      if (lastErrorValue.empty()) lastErrorValue = message;
      permanentDegraded = true;
      degradedUntil = std::numeric_limits<std::int64_t>::max();
      if (degradedSinceValue.empty()) degradedSinceValue = isoNow(now());
      (void)code;
    } catch (...) {}
  }

  void recordTemporary(ResultCode code, const std::string& message) noexcept {
    try {
      std::lock_guard<std::mutex> lock(mutex);
      if (permanentDegraded) return;
      lastErrorValue = message;
      if (degradedSinceValue.empty()) degradedSinceValue = isoNow(now());
      degradedUntil = now() + std::max(1, options.probeRetryMs);
      (void)code;
    } catch (...) {}
  }

  bool degradedUnlocked() const noexcept {
    const auto value = now();
    return permanentDegraded || (degradedUntil > 0 && value < degradedUntil);
  }

  bool degraded() const noexcept {
    try {
      std::lock_guard<std::mutex> lock(mutex);
      return degradedUnlocked();
    } catch (...) {
      return true;
    }
  }

  void recover() noexcept {
    try {
      std::lock_guard<std::mutex> lock(mutex);
      if (!permanentDegraded) { degradedUntil = 0; degradedSinceValue.clear(); }
      lastErrorValue.clear();
      lastSuccessValue = isoNow(now());
    } catch (...) {}
  }

  void heartbeatLoop() noexcept {
    try {
      std::unique_lock<std::mutex> lock(mutex);
      while (!heartbeatStop) {
        heartbeatCondition.wait_for(lock, std::chrono::milliseconds(std::max(50, options.heartbeatMs / 4)));
        if (heartbeatStop) break;
        const auto sent = lastSentAt;
        const auto current = now();
        const bool terminalState = terminal;
        const bool closedState = closed;
        lock.unlock();
        if (!terminalState && !closedState && (sent == 0 || current - sent >= options.heartbeatMs) && owner != nullptr) {
          (void)owner->update(SessionPatch{});
        }
        lock.lock();
      }
    } catch (...) {
      // A heartbeat failure must never take down the host process.
    }
  }

  void stopHeartbeat() noexcept {
    try {
      {
        std::lock_guard<std::mutex> lock(mutex);
        heartbeatStop = true;
      }
      heartbeatCondition.notify_all();
      if (heartbeatThread.joinable() && heartbeatThread.get_id() != std::this_thread::get_id()) heartbeatThread.join();
    } catch (...) {}
  }

  void applyPatch(const SessionPatch& patch) noexcept {
    try {
      const SessionState clamped = clampSession(patch, warningsValue);
      if (patch.title) { state.title = clamped.title; present.insert("title"); }
      if (patch.repository) { state.repository = clamped.repository; present.insert("repository"); }
      if (patch.branch) { state.branch = clamped.branch; present.insert("branch"); }
      if (patch.status) { state.status = clamped.status; present.insert("status"); }
      if (patch.summary) { state.summary = clamped.summary; present.insert("summary"); }
      if (patch.assumption) { state.assumption = clamped.assumption; present.insert("assumption"); }
      if (patch.verifiedBaseline) { state.verifiedBaseline = clamped.verifiedBaseline; present.insert("verifiedBaseline"); }
      if (patch.evidence) { state.evidence = clamped.evidence; present.insert("evidence"); }
      if (patch.nextGates) { state.nextGates = clamped.nextGates; present.insert("nextGates"); }
      if (patch.machine) { state.machine = clamped.machine; present.insert("machine"); }
      if (patch.worktrees) { state.worktrees = clamped.worktrees; present.insert("worktrees"); }
      if (patch.progress && clamped.progress) { state.progress = clamped.progress; present.insert("progress"); }
      if (patch.lowlevel && clamped.lowlevel) { state.lowlevel = clamped.lowlevel; present.insert("lowlevel"); }
    } catch (...) {
      appendWarning(warningsValue, "client", "an allocation failure dropped part of the update");
    }
  }

  std::string stateJson() const noexcept {
    try {
      JsonObject object;
      std::vector<StringField> ignoredStrings;
      object.stringField("id", sessionId);
      if (present.count("title")) object.stringField("title", state.title);
      if (present.count("repository")) object.stringField("repository", state.repository);
      if (present.count("branch")) object.stringField("branch", state.branch);
      if (present.count("status")) object.stringField("status", statusNameImpl(state.status));
      if (present.count("summary")) object.stringField("summary", state.summary);
      if (present.count("assumption")) object.stringField("assumption", state.assumption);
      if (present.count("verifiedBaseline")) object.stringField("verifiedBaseline", state.verifiedBaseline);
      if (present.count("evidence")) object.rawField("evidence", evidenceJson(state.evidence, ignoredStrings));
      if (present.count("nextGates")) object.rawField("nextGates", jsonArray(state.nextGates));
      if (present.count("machine")) object.stringField("machine", state.machine);
      if (present.count("worktrees")) object.rawField("worktrees", worktreesJson(state.worktrees, ignoredStrings));
      if (present.count("progress") && state.progress) object.rawField("progress", progressJson(*state.progress, ignoredStrings));
      if (present.count("lowlevel") && state.lowlevel) object.rawField("lowlevel", lowlevelJson(*state.lowlevel, ignoredStrings));
      return object.finish();
    } catch (...) {
      return "{}";
    }
  }

  PayloadJson payloadJson(const std::string& extraPath, const std::string& extraJson,
                          bool includeAgent, const NodeCredentials& credentials) const noexcept {
    PayloadJson payload;
    try {
      JsonObject object;
      object.stringField("id", sessionId);
      rememberString(payload.strings, "/id", sessionId);
      if (present.count("title")) { object.stringField("title", state.title); rememberString(payload.strings, "/title", state.title); }
      if (present.count("repository")) { object.stringField("repository", state.repository); rememberString(payload.strings, "/repository", state.repository); }
      if (present.count("branch")) { object.stringField("branch", state.branch); rememberString(payload.strings, "/branch", state.branch); }
      if (present.count("status")) { object.stringField("status", statusNameImpl(state.status)); rememberString(payload.strings, "/status", statusNameImpl(state.status)); }
      if (present.count("summary")) { object.stringField("summary", state.summary); rememberString(payload.strings, "/summary", state.summary); }
      if (present.count("assumption")) { object.stringField("assumption", state.assumption); rememberString(payload.strings, "/assumption", state.assumption); }
      if (present.count("verifiedBaseline")) { object.stringField("verifiedBaseline", state.verifiedBaseline); rememberString(payload.strings, "/verifiedBaseline", state.verifiedBaseline); }
      if (present.count("evidence")) object.rawField("evidence", evidenceJson(state.evidence, payload.strings));
      if (present.count("nextGates")) {
        object.rawField("nextGates", jsonArray(state.nextGates));
        for (std::size_t index = 0; index < state.nextGates.size(); ++index) rememberString(payload.strings, "/nextGates/" + std::to_string(index), state.nextGates[index]);
      }
      if (present.count("machine")) { object.stringField("machine", state.machine); rememberString(payload.strings, "/machine", state.machine); }
      if (present.count("worktrees")) object.rawField("worktrees", worktreesJson(state.worktrees, payload.strings));
      if (present.count("progress") && state.progress) object.rawField("progress", progressJson(*state.progress, payload.strings));
      if (present.count("lowlevel") && state.lowlevel) object.rawField("lowlevel", lowlevelJson(*state.lowlevel, payload.strings));
      if (includeAgent) {
        object.stringField("agent", credentials.identity);
        rememberString(payload.strings, "/agent", credentials.identity);
      }
      if (!extraPath.empty()) object.rawField(extraPath, extraJson);
      std::string root = object.finish();
      std::vector<StringField> identityFields;
      for (const auto& field : payload.strings) {
        if (!containsIdentity(field.value, credentials.identity)) continue;
        JsonObject metadataField;
        metadataField.stringField("path", field.path);
        JsonObject part;
        part.stringField("text", field.value);
        part.stringField("owner", "agent");
        part.stringField("subject", field.path == "/agent" ? "assistant-identity" : "self-reference");
        part.stringField("boundary", "agent-prose");
        metadataField.rawField("parts", "[" + part.finish() + "]");
        identityFields.push_back(StringField{field.path, metadataField.finish()});
      }
      JsonObject privateEmission;
      privateEmission.intField("version", kPrivateEmissionVersion);
      privateEmission.stringField("audience", "private");
      privateEmission.stringField("producer", "agent");
      privateEmission.stringField("vocabularyPassword", credentials.password);
      std::string fields = "[";
      for (std::size_t index = 0; index < identityFields.size(); ++index) {
        if (index > 0) fields.push_back(',');
        fields += identityFields[index].value;
      }
      fields.push_back(']');
      privateEmission.rawField("fields", fields);
      // Reuse the already-built root without parsing it: the metadata is the
      // final property, exactly like the JavaScript object spread path.
      if (!root.empty() && root.back() == '}') root.pop_back();
      payload.body = root + ",\"privateEmission\":" + privateEmission.finish() + "}";
      return payload;
    } catch (...) {
      payload.body = "{}";
      return payload;
    }
  }

  Result<PayloadJson> compose(const std::string& extraPath, const std::string& extraJson,
                              bool includeAgent, const Deadline& deadline = std::nullopt) noexcept {
    const int credentialTimeout = remainingMs(deadline, options.timeoutMs);
    if (credentialTimeout <= 0) return Result<PayloadJson>::failure(ResultCode::timeout, "The private-emission deadline expired before credential discovery.");
    auto credentials = getNodeCredentials(options, false, credentialTimeout);
    if (!credentials.ok) return Result<PayloadJson>::failure(credentials.code, credentials.error);
    PayloadJson payload = payloadJson(extraPath, extraJson, includeAgent, credentials.value);
    if (payload.body.size() > ProtocolLimits::maxBodyBytes) return Result<PayloadJson>::failure(ResultCode::body_too_large, "The composed request exceeds the 64 KiB body limit after clamping.");
    const int preflightTimeout = remainingMs(deadline, options.timeoutMs);
    if (preflightTimeout <= 0) return Result<PayloadJson>::failure(ResultCode::timeout, "The private-emission deadline expired before preflight.");
    auto preflight = runPrivatePreflight(options, payload.body, preflightTimeout);
    if (preflight.ok) return Result<PayloadJson>::success(std::move(payload));
    if (preflight.code == ResultCode::stale_vocabulary_password) {
      const int refreshTimeout = remainingMs(deadline, options.timeoutMs);
      if (refreshTimeout <= 0) return Result<PayloadJson>::failure(ResultCode::timeout, "The private-emission deadline expired before password refresh.");
      auto refreshed = getNodeCredentials(options, true, refreshTimeout);
      if (!refreshed.ok) return Result<PayloadJson>::failure(refreshed.code, refreshed.error);
      payload = payloadJson(extraPath, extraJson, includeAgent, refreshed.value);
      if (payload.body.size() > ProtocolLimits::maxBodyBytes) return Result<PayloadJson>::failure(ResultCode::body_too_large, "The composed request exceeds the 64 KiB body limit after refresh.");
      const int refreshedPreflightTimeout = remainingMs(deadline, options.timeoutMs);
      if (refreshedPreflightTimeout <= 0) return Result<PayloadJson>::failure(ResultCode::timeout, "The private-emission deadline expired before refreshed preflight.");
      preflight = runPrivatePreflight(options, payload.body, refreshedPreflightTimeout);
      if (preflight.ok) return Result<PayloadJson>::success(std::move(payload));
    }
    return Result<PayloadJson>::failure(preflight.code, preflight.error);
  }

  int rateWait(const Deadline& deadline, bool& expired) noexcept {
    expired = false;
    try {
      const auto current = now();
      const int effectiveRate = effectiveRequestsPerMinute(options.requestsPerMinute);
      while (!requestStamps.empty() && requestStamps.front() <= current - ProtocolLimits::agentWindowMs) requestStamps.pop_front();
      if (requestStamps.size() >= static_cast<std::size_t>(effectiveRate)) {
        const int wait = static_cast<int>(std::max<std::int64_t>(1, ProtocolLimits::agentWindowMs - (current - requestStamps.front())));
        if (deadline && remainingMs(deadline, wait) < wait) { expired = true; return 0; }
        return wait;
      }
      requestStamps.push_back(current);
    } catch (...) {
    }
    return 0;
  }

  Result<ParsedHttpResponse> request(const std::string& path, const std::string& method,
                               const std::string& body, bool includeSessionKey,
                               int timeoutOverride = 0,
                               const Deadline& deadline = std::nullopt) noexcept {
    try {
      bool rateExpired = false;
      const int wait = rateWait(deadline, rateExpired);
      if (rateExpired) return Result<ParsedHttpResponse>::failure(ResultCode::timeout, "The request rate budget exceeded the absolute deadline.");
      if (wait > 0) sleepFor(wait);
      if (deadline && remainingMs(deadline, 1) <= 0) return Result<ParsedHttpResponse>::failure(ResultCode::timeout, "The request rate wait exceeded the absolute deadline.");
      HttpRequest requestValue;
      requestValue.method = method;
      requestValue.url = baseUrl + path;
      requestValue.timeoutMs = timeoutOverride > 0 ? timeoutOverride : std::max(1, options.timeoutMs);
      requestValue.headers.emplace_back("accept", "application/json");
      requestValue.headers.emplace_back(kIngestTokenHeader, options.ingestToken);
      if (includeSessionKey) requestValue.headers.emplace_back(kSessionKeyHeader, sessionKeyValue);
      if (!body.empty()) {
        if (body.size() > ProtocolLimits::maxBodyBytes) return Result<ParsedHttpResponse>::failure(ResultCode::body_too_large, "The request body exceeds the 64 KiB protocol limit.");
        requestValue.headers.emplace_back("content-type", "application/json");
        requestValue.body = body;
      }
      TransportResult transportResult = transport.send(requestValue);
      if (!transportResult.ok) {
        Result<ParsedHttpResponse> failure = Result<ParsedHttpResponse>::failure(transportResult.code, transportResult.code == ResultCode::timeout ? "The hub transport timed out." : "The hub transport failed.");
        failure.httpStatus = transportResult.response.status;
        return failure;
      }
      if (transportResult.response.body.size() > ProtocolLimits::maxResponseBytes) return Result<ParsedHttpResponse>::failure(ResultCode::response_too_large, "The hub response exceeded the bounded 512 KiB size.");
      auto document = std::make_unique<JsonNode>();
      JsonDocumentParser parser(transportResult.response.body);
      if (!parser.parseRoot(document)) {
        if (parser.nodeBudgetExceeded()) return Result<ParsedHttpResponse>::failure(ResultCode::json_node_budget, "The hub response exceeded the bounded JSON-node budget.");
        if (parser.duplicateKey()) return Result<ParsedHttpResponse>::failure(ResultCode::json_duplicate_key, "The hub response contains a duplicate JSON object key.");
        return Result<ParsedHttpResponse>::failure(ResultCode::bad_json, "The hub returned malformed or empty JSON object data.");
      }
      ParsedHttpResponse parsed;
      parsed.wire = std::move(transportResult.response);
      parsed.json = std::move(document);
      Result<ParsedHttpResponse> response = Result<ParsedHttpResponse>::success(std::move(parsed));
      response.httpStatus = response.value.wire.status;
      if (response.value.wire.status < 200 || response.value.wire.status >= 300) {
        response.ok = false;
        response.code = response.value.wire.status == 429 ? ResultCode::rate_limited : ResultCode::http_error;
        const JsonNode* errorNode = response.value.json ? response.value.json->get("error") : nullptr;
        const std::string serverError = errorNode && errorNode->kind == JsonNode::Kind::string ? cleanString(errorNode->stringValue, 400) : std::string{};
        response.error = serverError.empty() ? "The hub answered " + path + " with HTTP " + std::to_string(response.value.wire.status) + "." : serverError;
        const auto retryAfter = parseRetryAfter(response.value.wire);
        if (retryAfter) { response.retryAfterPresent = true; response.retryAfterMs = *retryAfter; }
        try { response.oldestSequence = std::stoll(headerValue(response.value.wire, kOldestSequenceHeader)); } catch (...) { response.oldestSequence = 0; }
      }
      return response;
    } catch (...) {
      return Result<ParsedHttpResponse>::failure(ResultCode::transport, "The hub request failed without exposing transport details.");
    }
  }

  Result<SessionSnapshot> sendPayload(const std::string& path, const std::string& method,
                                      const std::string& extraPath, const std::string& extraJson,
                                      bool includeAgent, int timeoutOverride = 0,
                                      int attemptsOverride = 0,
                                      const Deadline& deadline = std::nullopt) noexcept {
    try {
      const int initialTimeout = remainingMs(deadline, timeoutOverride > 0 ? timeoutOverride : options.timeoutMs);
      if (initialTimeout <= 0) return Result<SessionSnapshot>::failure(ResultCode::timeout, "The hub send deadline expired before composition.");
      bool closedState = false;
      bool permanentState = false;
      std::string degradedError;
      {
        std::lock_guard<std::mutex> lock(mutex);
        closedState = closed;
        permanentState = permanentDegraded;
        degradedError = lastErrorValue;
      }
      if (closedState) return Result<SessionSnapshot>::failure(ResultCode::closed, "The client is closed.");
      if (permanentState) {
        auto failure = Result<SessionSnapshot>::failure(ResultCode::degraded, degradedError.empty() ? "The client is degraded." : degradedError);
        failure.degraded = true;
        return failure;
      }
      auto composed = compose(extraPath, extraJson, includeAgent, deadline);
      if (!composed.ok) {
        {
          std::lock_guard<std::mutex> lock(mutex);
          lastErrorValue = composed.error;
        }
        auto failure = Result<SessionSnapshot>::failure(composed.code, composed.error);
        failure.degraded = composed.code == ResultCode::private_guard_unavailable || composed.code == ResultCode::private_guard_failed;
        return failure;
      }
      const int attempts = attemptsOverride > 0 ? attemptsOverride : std::max(1, options.maxSendAttempts);
      int delay = 500;
      for (int attempt = 0; attempt < attempts; ++attempt) {
        const int requestTimeout = remainingMs(deadline, timeoutOverride > 0 ? timeoutOverride : options.timeoutMs);
        if (requestTimeout <= 0) break;
        auto response = request(path, method, composed.value.body, true, requestTimeout, deadline);
        if (response.ok) {
          recover();
          SessionSnapshot snapshot;
          snapshot.rawJson = response.value.wire.body;
          snapshot.httpStatus = response.httpStatus;
          if (response.value.json) {
            const JsonNode* sessionNode = response.value.json->get("session");
            const JsonNode* idNode = sessionNode && sessionNode->kind == JsonNode::Kind::object ? sessionNode->get("id") : nullptr;
            if (idNode && idNode->kind == JsonNode::Kind::string) snapshot.sessionId = idNode->stringValue;
            const JsonNode* errorNode = response.value.json->get("error");
            if (errorNode && errorNode->kind == JsonNode::Kind::string) snapshot.error = cleanString(errorNode->stringValue, 400);
          }
          return Result<SessionSnapshot>::success(std::move(snapshot));
        }
        {
          std::lock_guard<std::mutex> lock(mutex);
          lastErrorValue = response.error;
        }
        const bool retryable = response.code == ResultCode::rate_limited
            || response.code == ResultCode::transport
            || response.code == ResultCode::timeout
            || response.httpStatus >= 500;
        if (!retryable) {
          Result<SessionSnapshot> failure = Result<SessionSnapshot>::failure(response.code, response.error);
          failure.httpStatus = response.httpStatus;
          failure.retryAfterMs = response.retryAfterMs;
          failure.retryAfterPresent = response.retryAfterPresent;
          return failure;
        }
        if (attempt + 1 >= attempts) break;
        int wait = response.retryAfterPresent ? response.retryAfterMs : delay;
        if (!response.retryAfterPresent && wait > 1) {
          try { wait += std::uniform_int_distribution<int>(0, wait - 1)(jitterEngine); } catch (...) {}
        }
        const int remaining = remainingMs(deadline, std::max(1, options.backoffCapMs));
        if (remaining <= 0) break;
        sleepFor(std::min(wait, remaining));
        delay = std::min(delay * 2, std::max(1, options.backoffCapMs));
      }
      Result<SessionSnapshot> failure = Result<SessionSnapshot>::failure(
          deadline ? ResultCode::timeout : (attemptsOverride > 0 ? ResultCode::timeout : ResultCode::degraded),
          [&]() {
            std::lock_guard<std::mutex> lock(mutex);
            return lastErrorValue.empty() ? std::string("The hub request did not complete.") : lastErrorValue;
          }());
      failure.degraded = attemptsOverride <= 0;
      if (attemptsOverride <= 0) recordTemporary(ResultCode::degraded, failure.error);
      return failure;
    } catch (...) {
      return Result<SessionSnapshot>::failure(ResultCode::internal_error, "The bounded hub send failed safely.");
    }
  }

  std::string questionJson(const Question& question, std::vector<StringField>& strings) const noexcept {
    try {
      JsonObject object;
      object.stringField("id", question.id); rememberString(strings, "/id", question.id);
      object.stringField("prompt", question.prompt); rememberString(strings, "/prompt", question.prompt);
      object.stringField("detail", question.detail); rememberString(strings, "/detail", question.detail);
      std::string options = "[";
      for (std::size_t index = 0; index < question.options.size(); ++index) {
        if (index > 0) options.push_back(',');
        JsonObject option;
        option.stringField("id", question.options[index].id); rememberString(strings, "/options/" + std::to_string(index) + "/id", question.options[index].id);
        option.stringField("label", question.options[index].label); rememberString(strings, "/options/" + std::to_string(index) + "/label", question.options[index].label);
        options += option.finish();
      }
      options.push_back(']');
      object.rawField("options", options);
      object.boolField("allowText", question.allowText);
      return object.finish();
    } catch (...) {
      return "{}";
    }
  }

  Result<PayloadJson> composeQuestion(const Question& question, const Deadline& deadline = std::nullopt) noexcept {
    const int credentialTimeout = remainingMs(deadline, options.timeoutMs);
    if (credentialTimeout <= 0) return Result<PayloadJson>::failure(ResultCode::timeout, "The private-emission deadline expired before credential discovery.");
    auto credentials = getNodeCredentials(options, false, credentialTimeout);
    if (!credentials.ok) return Result<PayloadJson>::failure(credentials.code, credentials.error);
    try {
      const auto build = [&](const NodeCredentials& current) {
        PayloadJson payload;
        std::vector<StringField> strings;
        const std::string raw = questionJson(question, strings);
        if (!raw.empty() && raw.front() == '{' && raw.back() == '}') {
          const std::string without = raw.substr(1, raw.size() - 2);
          payload.body = "{" + without + ",\"privateEmission\":{";
          payload.body += "\"version\":2,\"audience\":\"private\",\"producer\":\"agent\",\"vocabularyPassword\":" + jsonEscape(current.password) + ",\"fields\":[";
          bool first = true;
          for (const auto& field : strings) {
            if (!containsIdentity(field.value, current.identity)) continue;
            if (!first) payload.body.push_back(',');
            first = false;
            JsonObject metadata;
            metadata.stringField("path", field.path);
            JsonObject part;
            part.stringField("text", field.value);
            part.stringField("owner", "agent");
            part.stringField("subject", "self-reference");
            part.stringField("boundary", "agent-prose");
            metadata.rawField("parts", "[" + part.finish() + "]");
            payload.body += metadata.finish();
          }
          payload.body += "]}}";
          payload.strings = std::move(strings);
        }
        return payload;
      };
      PayloadJson payload = build(credentials.value);
      if (payload.body.size() > ProtocolLimits::maxBodyBytes) return Result<PayloadJson>::failure(ResultCode::body_too_large, "The question body exceeds the 64 KiB protocol limit.");
      const int preflightTimeout = remainingMs(deadline, options.timeoutMs);
      if (preflightTimeout <= 0) return Result<PayloadJson>::failure(ResultCode::timeout, "The private-emission deadline expired before preflight.");
      auto preflight = runPrivatePreflight(options, payload.body, preflightTimeout);
      if (preflight.ok) return Result<PayloadJson>::success(std::move(payload));
      if (preflight.code == ResultCode::stale_vocabulary_password) {
        const int refreshTimeout = remainingMs(deadline, options.timeoutMs);
        if (refreshTimeout <= 0) return Result<PayloadJson>::failure(ResultCode::timeout, "The private-emission deadline expired before password refresh.");
        auto refreshed = getNodeCredentials(options, true, refreshTimeout);
        if (!refreshed.ok) return Result<PayloadJson>::failure(refreshed.code, refreshed.error);
        payload = build(refreshed.value);
        if (payload.body.size() > ProtocolLimits::maxBodyBytes) return Result<PayloadJson>::failure(ResultCode::body_too_large, "The question body exceeds the 64 KiB protocol limit after refresh.");
        const int refreshedPreflightTimeout = remainingMs(deadline, options.timeoutMs);
        if (refreshedPreflightTimeout <= 0) return Result<PayloadJson>::failure(ResultCode::timeout, "The private-emission deadline expired before refreshed preflight.");
        preflight = runPrivatePreflight(options, payload.body, refreshedPreflightTimeout);
        if (preflight.ok) return Result<PayloadJson>::success(std::move(payload));
      }
      return Result<PayloadJson>::failure(preflight.code, preflight.error);
    } catch (...) {
      return Result<PayloadJson>::failure(ResultCode::internal_error, "The question payload could not be composed safely.");
    }
  }

  Result<SessionSnapshot> sendQuestion(const Question& question, const Deadline& deadline = std::nullopt) noexcept {
    try {
      if (closed) return Result<SessionSnapshot>::failure(ResultCode::closed, "The client is closed.");
      auto payload = composeQuestion(question, deadline);
      if (!payload.ok) return Result<SessionSnapshot>::failure(payload.code, payload.error);
      const std::string path = std::string(kSessionsPath) + "/" + percentEncode(sessionId) + "/questions";
      const int requestTimeout = remainingMs(deadline, options.timeoutMs);
      if (requestTimeout <= 0) return Result<SessionSnapshot>::failure(ResultCode::timeout, "The question send deadline expired.");
      auto response = request(path, "POST", payload.value.body, true, requestTimeout, deadline);
      if (!response.ok) {
        Result<SessionSnapshot> failure = Result<SessionSnapshot>::failure(response.code, response.error);
        failure.httpStatus = response.httpStatus;
        return failure;
      }
      SessionSnapshot snapshot;
      snapshot.rawJson = response.value.wire.body;
      snapshot.httpStatus = response.httpStatus;
      if (response.value.json) {
        const JsonNode* questionNode = response.value.json->get("question");
        const JsonNode* idNode = questionNode && questionNode->kind == JsonNode::Kind::object ? questionNode->get("id") : nullptr;
        if (idNode && idNode->kind == JsonNode::Kind::string) snapshot.questionId = idNode->stringValue;
        const JsonNode* errorNode = response.value.json->get("error");
        if (errorNode && errorNode->kind == JsonNode::Kind::string) snapshot.error = cleanString(errorNode->stringValue, 400);
      }
      return Result<SessionSnapshot>::success(std::move(snapshot));
    } catch (...) {
      return Result<SessionSnapshot>::failure(ResultCode::internal_error, "The question send failed safely.");
    }
  }
};

StatusHubClient::StatusHubClient(HttpTransport& transport, const ClientOptions& options) noexcept
    : impl_(nullptr) {
  try {
    impl_ = std::make_unique<Impl>(this, transport, options);
  } catch (...) {
    impl_.reset();
  }
}

StatusHubClient::~StatusHubClient() noexcept {
  try {
    bool shouldFinish = false;
    if (impl_) {
      std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
      std::lock_guard<std::mutex> lock(impl_->mutex);
      shouldFinish = !impl_->closed && !impl_->terminal;
    }
    if (shouldFinish) (void)finish(Status::waiting, SessionPatch{});
  } catch (...) {
  }
}

Result<std::unique_ptr<StatusHubClient>> StatusHubClient::create(HttpTransport& transport, ClientOptions options) noexcept {
  try {
    if (options.baseUrl.empty()) options.baseUrl = trimTrailingSlashes(environment("STATUS_HUB_URL"));
    if (options.baseUrl.empty()) options.baseUrl = kDefaultBaseUrl;
    auto client = std::unique_ptr<StatusHubClient>(new StatusHubClient(transport, options));
    auto health = resolveBaseUrl(transport, client->impl_->baseUrl, options.timeoutMs);
    if (!health.ok) {
      std::lock_guard<std::mutex> sendLock(client->impl_->sendMutex);
      client->impl_->recordTemporary(health.code, health.error);
    } else {
      (void)client->update(SessionPatch{});
    }
    Result<std::unique_ptr<StatusHubClient>> result = Result<std::unique_ptr<StatusHubClient>>::success(std::move(client));
    return result;
  } catch (...) {
    return Result<std::unique_ptr<StatusHubClient>>::failure(ResultCode::internal_error, "The client could not be created safely.");
  }
}

Result<std::string> StatusHubClient::resolveBaseUrl(HttpTransport& transport, std::string baseUrl, int timeoutMs) noexcept {
  try {
    baseUrl = trimTrailingSlashes(baseUrl.empty() ? std::string(kDefaultBaseUrl) : std::move(baseUrl));
    if (!validBaseUrl(baseUrl)) return Result<std::string>::failure(ResultCode::invalid_url, "The Status Hub base URL must be an absolute http or https URL without embedded credentials.");
    HttpRequest request;
    request.method = "GET";
    request.url = baseUrl + kHealthPath;
    request.timeoutMs = std::max(1, timeoutMs);
    request.headers.emplace_back("accept", "application/json");
    TransportResult response = transport.send(request);
    if (!response.ok) return Result<std::string>::failure(response.code, response.code == ResultCode::timeout ? "The Status Hub health probe timed out." : "The Status Hub health probe could not reach the hub.");
    if (response.response.status < 200 || response.response.status >= 300) {
      Result<std::string> failure = Result<std::string>::failure(ResultCode::health_failed, "The Status Hub health route did not answer successfully.");
      failure.httpStatus = response.response.status;
      return failure;
    }
    return Result<std::string>::success(baseUrl);
  } catch (...) {
    return Result<std::string>::failure(ResultCode::internal_error, "The Status Hub health probe failed safely.");
  }
}

Result<SessionSnapshot> StatusHubClient::update(const SessionPatch& patch) noexcept {
  try {
    if (!impl_) return Result<SessionSnapshot>::failure(ResultCode::internal_error, "The client has no state.");
    std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      if (impl_->terminal) return Result<SessionSnapshot>::failure(ResultCode::already_terminal, "The session already sent its terminal status.");
      impl_->applyPatch(patch);
      const std::string desired = impl_->stateJson();
      const auto current = impl_->now();
      if (impl_->created && desired == impl_->lastSentJson && current - impl_->lastSentAt < impl_->options.coalesceMs) {
        Result<SessionSnapshot> coalesced = Result<SessionSnapshot>::success();
        coalesced.coalesced = true;
        coalesced.code = ResultCode::coalesced;
        return coalesced;
      }
    }
    auto result = impl_->sendPayload(kSessionsPath, "POST", {}, {}, !impl_->created);
    if (result.ok) {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      impl_->created = true;
      impl_->lastSentJson = impl_->stateJson();
      impl_->lastSentAt = impl_->now();
    }
    return result;
  } catch (...) {
    return Result<SessionSnapshot>::failure(ResultCode::internal_error, "The session update failed safely.");
  }
}

Result<SessionSnapshot> StatusHubClient::publishQuestion(const Question& question) noexcept {
  try {
    if (!impl_) return Result<SessionSnapshot>::failure(ResultCode::internal_error, "The client has no state.");
    bool alreadyCreated = false;
    {
      std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
      std::lock_guard<std::mutex> lock(impl_->mutex);
      alreadyCreated = impl_->created;
    }
    if (!alreadyCreated) {
      auto created = update(SessionPatch{});
      if (!created.ok) return created;
    }
    std::vector<Warning> localWarnings;
    auto clamped = clampQuestion(question, localWarnings);
    if (!clamped) return Result<SessionSnapshot>::failure(ResultCode::invalid_input, "A question needs an id, a prompt, and at least one option or a text answer.");
    std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      for (auto& warning : localWarnings) impl_->warningsValue.push_back(std::move(warning));
      if (!impl_->created) return Result<SessionSnapshot>::failure(ResultCode::degraded, "The session could not be created before publishing the question.");
    }
    return impl_->sendQuestion(*clamped);
  } catch (...) {
    return Result<SessionSnapshot>::failure(ResultCode::internal_error, "The question publication failed safely.");
  }
}

Result<Replies> StatusHubClient::pollReplies() noexcept {
  try {
    if (!impl_) return Result<Replies>::failure(ResultCode::internal_error, "The client has no state.");
    std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
    std::int64_t cursor = 0;
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      cursor = impl_->cursor;
      if (!impl_->created) {
        Replies empty;
        empty.after = cursor;
        empty.latest = cursor;
        return Result<Replies>::success(std::move(empty));
      }
    }
    for (int attempt = 0; attempt < 2; ++attempt) {
      const std::string path = std::string(kSessionsPath) + "/" + percentEncode(impl_->sessionId) + "/replies?after=" + std::to_string(cursor);
      auto response = impl_->request(path, "GET", {}, true);
      if (response.ok) {
        Replies replies;
        replies.after = cursor;
        replies.latest = cursor;
        const JsonNode& document = *response.value.json;
        if (const JsonNode* after = document.get("after"); after != nullptr) if (const auto value = after->integer()) replies.after = *value;
        if (const JsonNode* latest = document.get("latest"); latest != nullptr) if (const auto value = latest->integer()) replies.latest = *value;
        if (const JsonNode* error = document.get("error"); error != nullptr && error->kind == JsonNode::Kind::string) replies.error = cleanString(error->stringValue, 400);
        cursor = std::max(cursor, replies.latest);
        const JsonNode* replyArray = document.get("replies");
        if (replyArray != nullptr && replyArray->kind == JsonNode::Kind::array) {
          for (const auto& eventNode : replyArray->arrayValue) {
            if (!eventNode || eventNode->kind != JsonNode::Kind::object) continue;
            Reply reply;
            if (const JsonNode* sequence = eventNode->get("seq"); sequence != nullptr) {
              const auto value = sequence->integer();
              if (!value) continue;
              reply.sequence = *value;
            }
            if (const JsonNode* type = eventNode->get("type"); type != nullptr && type->kind == JsonNode::Kind::string) reply.type = type->stringValue;
            const JsonNode* payload = eventNode->get("payload");
            if (payload != nullptr && payload->kind == JsonNode::Kind::object) {
              if (const JsonNode* field = payload->get("kind"); field != nullptr && field->kind == JsonNode::Kind::string) reply.kind = field->stringValue;
              if (const JsonNode* field = payload->get("questionId"); field != nullptr && field->kind == JsonNode::Kind::string) reply.questionId = field->stringValue;
              if (const JsonNode* field = payload->get("optionId"); field != nullptr && field->kind == JsonNode::Kind::string) reply.optionId = field->stringValue;
              if (const JsonNode* field = payload->get("text"); field != nullptr && field->kind == JsonNode::Kind::string) reply.text = field->stringValue;
            }
            reply.rawJson = serializeJsonNode(*eventNode);
            replies.replies.push_back(std::move(reply));
          }
        }
        impl_->recover();
        {
          std::lock_guard<std::mutex> lock(impl_->mutex);
          impl_->cursor = cursor;
        }
        return Result<Replies>::success(std::move(replies));
      }
      if (response.httpStatus == 409 && response.oldestSequence > 0) {
        cursor = response.oldestSequence - 1;
        {
          std::lock_guard<std::mutex> lock(impl_->mutex);
          impl_->cursor = cursor;
        }
        continue;
      }
      Result<Replies> failure = Result<Replies>::failure(response.code, response.error);
      failure.httpStatus = response.httpStatus;
      return failure;
    }
    return Result<Replies>::failure(ResultCode::cursor_resync_failed, "The reply cursor could not be resynchronized.");
  } catch (...) {
    return Result<Replies>::failure(ResultCode::internal_error, "The reply poll failed safely.");
  }
}

Result<CheckpointResult> StatusHubClient::pollAtCheckpoint(const SessionPatch& patch) noexcept {
  try {
    CheckpointResult checkpoint;
    checkpoint.update = update(patch);
    checkpoint.replies = pollReplies();
    return Result<CheckpointResult>::success(std::move(checkpoint));
  } catch (...) {
    return Result<CheckpointResult>::failure(ResultCode::internal_error, "The checkpoint poll failed safely.");
  }
}

Result<SessionSnapshot> StatusHubClient::finish(Status status, const SessionPatch& patch) noexcept {
  try {
    if (!impl_) return Result<SessionSnapshot>::failure(ResultCode::internal_error, "The client has no state.");
    const Deadline deadline = Clock::now() + std::chrono::milliseconds(std::max(1, impl_->options.finishDeadlineMs));
    bool needsAgent = false;
    Result<SessionSnapshot> result;
    {
      std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
      {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        if (impl_->terminal) {
          Result<SessionSnapshot> already = Result<SessionSnapshot>::success();
          already.code = ResultCode::already_terminal;
          return already;
        }
        impl_->applyPatch(patch);
        impl_->state.status = (status == Status::landed || status == Status::failed || status == Status::waiting) ? status : Status::waiting;
        impl_->present.insert("status");
        needsAgent = !impl_->created;
      }
      result = impl_->sendPayload(kSessionsPath, "POST", {}, {}, needsAgent, 0, 1, deadline);
    }
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      impl_->terminal = true;
    }
    close();
    return result;
  } catch (...) {
    if (impl_) {
      {
        std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
        std::lock_guard<std::mutex> lock(impl_->mutex);
        impl_->terminal = true;
      }
      close();
    }
    return Result<SessionSnapshot>::failure(ResultCode::internal_error, "The terminal update failed safely.");
  }
}

void StatusHubClient::close() noexcept {
  try {
    if (!impl_) return;
    std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
    {
      std::lock_guard<std::mutex> lock(impl_->mutex);
      impl_->closed = true;
    }
    impl_->stopHeartbeat();
  } catch (...) {
  }
}

bool StatusHubClient::isClosed() const noexcept {
  try {
    if (!impl_) return true;
    std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->closed;
  } catch (...) { return true; }
}

bool StatusHubClient::isDegraded() const noexcept {
  try {
    if (!impl_) return true;
    std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->degradedUnlocked();
  } catch (...) { return true; }
}

ClientStatus StatusHubClient::status() const noexcept {
    ClientStatus output;
  try {
    if (!impl_) return output;
    std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    output.sessionId = impl_->sessionId;
    output.degraded = impl_->degradedUnlocked();
    output.degradedSince = impl_->degradedSinceValue;
    output.lastError = impl_->lastErrorValue;
    output.lastSuccessAt = impl_->lastSuccessValue;
    output.terminal = impl_->terminal;
    output.replyCursor = impl_->cursor;
  } catch (...) {
  }
  return output;
}

std::vector<Warning> StatusHubClient::warnings() const noexcept {
  try {
    if (!impl_) return {};
    std::lock_guard<std::mutex> sendLock(impl_->sendMutex);
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->warningsValue;
  } catch (...) {
    return {};
  }
}

#if !defined(_WIN32)
namespace {
class UnavailableTransport final : public HttpTransport {
public:
  TransportResult send(const HttpRequest&) noexcept override {
    TransportResult result;
    result.code = ResultCode::transport;
    result.error = "No default HTTP transport is bundled for this platform. Inject HttpTransport or enable libcurl.";
    return result;
  }
};
} // namespace

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
