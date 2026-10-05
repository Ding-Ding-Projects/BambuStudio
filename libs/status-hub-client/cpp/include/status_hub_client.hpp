#pragma once

// C++17 client contract for the Status Hub agent protocol.
//
// The core has no third-party dependencies.  Applications may inject their own
// HttpTransport, use the WinHTTP transport on Windows, or opt into libcurl at
// build time.  Public operations are noexcept and return typed results.

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace status_hub {

inline constexpr char kDefaultBaseUrl[] = ""; // Consumer configuration is required.
inline constexpr char kHealthPath[] = "/health";
inline constexpr char kSessionsPath[] = "/api/agent/sessions";
inline constexpr char kIngestTokenHeader[] = "x-agent-ingest-token";
inline constexpr char kSessionKeyHeader[] = "x-session-key";
inline constexpr char kOldestSequenceHeader[] = "x-status-hub-oldest-sequence";
inline constexpr int kPrivateEmissionVersion = 2;

struct ProtocolLimits final {
  static constexpr std::size_t maxBodyBytes = 64u * 1024u;
  static constexpr std::size_t maxResponseBytes = 512u * 1024u;
  static constexpr std::size_t maxJsonNodes = 8192u;
  static constexpr std::size_t maxEvents = 5000u;
  static constexpr std::size_t maxSessions = 256u;
  static constexpr std::size_t maxQuestionsPerSession = 100u;
  static constexpr std::size_t maxEvidenceItems = 8u;
  static constexpr std::size_t maxNextGates = 8u;
  static constexpr std::size_t maxWorktrees = 128u;
  static constexpr std::int64_t maxProgressSteps = 10000;
  static constexpr std::size_t agentMaxAttempts = 60u;
  static constexpr std::int64_t agentWindowMs = 60000;
  static constexpr int clientRequestsPerMinute = 50;
  static constexpr std::size_t titleMax = 160u;
  static constexpr std::size_t repositoryMax = 240u;
  static constexpr std::size_t branchMax = 160u;
  static constexpr std::size_t summaryMax = 2000u;
  static constexpr std::size_t assumptionMax = 1600u;
  static constexpr std::size_t verifiedBaselineMax = 400u;
  static constexpr std::size_t machineMax = 160u;
  static constexpr std::size_t evidenceLabelMax = 160u;
  static constexpr std::size_t evidenceUrlMax = 800u;
  static constexpr std::size_t nextGateMax = 240u;
  static constexpr std::size_t worktreePathMax = 600u;
  static constexpr std::size_t worktreeBranchMax = 240u;
  static constexpr std::size_t worktreeCommitMax = 64u;
  static constexpr std::size_t stepMax = 240u;
  static constexpr std::size_t questionIdMax = 80u;
  static constexpr std::size_t questionPromptMax = 1000u;
  static constexpr std::size_t questionDetailMax = 1200u;
  static constexpr std::size_t questionOptionsMax = 8u;
  static constexpr std::size_t optionIdMax = 60u;
  static constexpr std::size_t optionLabelMax = 160u;
  static constexpr std::size_t lowlevelTransportMax = 80u;
  static constexpr std::size_t lowlevelEndpointLabelMax = 160u;
  static constexpr std::size_t lowlevelVersionMax = 80u;
  static constexpr std::size_t lowlevelHeartbeatMax = 40u;
  static constexpr std::size_t lowlevelDiagnosticMax = 400u;
  static constexpr std::int64_t lowlevelCountMax = 10000;
  static constexpr std::size_t sessionKeyMin = 24u;
  static constexpr std::size_t sessionKeyLength = 64u;
};

enum class ResultCode {
  ok,
  coalesced,
  already_terminal,
  closed,
  invalid_input,
  invalid_session_id,
  invalid_session_key,
  missing_ingest_token,
  invalid_url,
  health_failed,
  unreachable,
  transport,
  timeout,
  response_too_large,
  body_too_large,
  bad_json,
  json_duplicate_key,
  json_node_budget,
  child_input_failed,
  child_output_overflow,
  rate_limited,
  http_error,
  degraded,
  private_guard_unavailable,
  private_guard_failed,
  private_preflight_failed,
  stale_vocabulary_password,
  private_emission_rejected,
  cursor_resync_failed,
  node_unavailable,
  csprng_unavailable,
  worktree_incomplete,
  internal_error
};

const char* resultCodeName(ResultCode code) noexcept;

struct Empty final {};

template <typename T>
struct Result final {
  bool ok = false;
  ResultCode code = ResultCode::internal_error;
  std::string error;
  int httpStatus = 0;
  int retryAfterMs = 0;
  bool retryAfterPresent = false;
  std::int64_t oldestSequence = 0;
  bool degraded = false;
  bool coalesced = false;
  T value{};

  static Result success(T result = T{}) noexcept {
    Result output;
    try {
      output.ok = true;
      output.code = ResultCode::ok;
      output.value = std::move(result);
    } catch (...) {
      output.ok = false;
      output.code = ResultCode::internal_error;
      output.error.clear();
    }
    return output;
  }

  static Result failure(ResultCode resultCode, const std::string& message) noexcept {
    Result output;
    output.code = resultCode;
    try { output.error = message; } catch (...) { output.error.clear(); }
    return output;
  }
};

struct Warning final {
  std::string field;
  std::string reason;
};

enum class Status { running, waiting, blocked, landed, failed };
enum class EvidenceState { pending, running, verified, failed };
enum class ProgressState { running, waiting, blocked, completed };
enum class LowlevelState { configured, reachable, unavailable, stale };

const char* statusName(Status value) noexcept;
const char* evidenceStateName(EvidenceState value) noexcept;
const char* progressStateName(ProgressState value) noexcept;
const char* lowlevelStateName(LowlevelState value) noexcept;

struct EvidenceItem final {
  std::string id;
  std::string label;
  std::string url;
  EvidenceState state = EvidenceState::pending;
};

struct Worktree final {
  std::string path;
  std::string branch;
  std::string commit;
  std::int64_t bytes = 0;
  bool dirty = false;
  bool incomplete = false;
};

struct Progress final {
  std::optional<std::int64_t> completedSteps;
  std::optional<std::int64_t> totalSteps;
  std::optional<int> percent;
  std::string currentStep;
  std::string nextStep;
  ProgressState state = ProgressState::running;
};

struct Lowlevel final {
  LowlevelState state = LowlevelState::configured;
  std::optional<bool> cheapHeadless;
  std::string transport;
  std::string endpointLabel;
  std::string version;
  std::string lastHeartbeat;
  std::optional<std::int64_t> headlessDesktopCount;
  std::optional<std::int64_t> headlessWindowCount;
  std::string diagnostic;
};

struct SessionPatch final {
  std::optional<std::string> title;
  std::optional<std::string> repository;
  std::optional<std::string> branch;
  std::optional<Status> status;
  std::optional<std::string> summary;
  std::optional<std::string> assumption;
  std::optional<std::string> verifiedBaseline;
  std::optional<std::vector<EvidenceItem>> evidence;
  std::optional<std::vector<std::string>> nextGates;
  std::optional<std::string> machine;
  std::optional<std::vector<Worktree>> worktrees;
  std::optional<Progress> progress;
  std::optional<Lowlevel> lowlevel;
};

struct SessionState final {
  std::string title;
  std::string repository;
  std::string branch;
  Status status = Status::running;
  std::string summary;
  std::string assumption;
  std::string verifiedBaseline;
  std::vector<EvidenceItem> evidence;
  std::vector<std::string> nextGates;
  std::string machine;
  std::vector<Worktree> worktrees;
  std::optional<Progress> progress;
  std::optional<Lowlevel> lowlevel;
};

SessionState clampSession(const SessionPatch& patch, std::vector<Warning>& warnings) noexcept;

struct QuestionOption final {
  std::string id;
  std::string label;
};

struct Question final {
  std::string id;
  std::string prompt;
  std::string detail;
  std::vector<QuestionOption> options;
  bool allowText = true;
};

std::optional<Question> clampQuestion(const Question& question, std::vector<Warning>& warnings) noexcept;

// Generates a 32-byte hexadecimal session key using the platform CSPRNG.
// Failure is typed and never replaced with a predictable fallback.
Result<std::string> generateSessionKey() noexcept;
int effectiveRequestsPerMinute(int requested) noexcept;

struct HttpRequest final {
  std::string method;
  std::string url;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
  int timeoutMs = 10000;
};

struct HttpResponse final {
  int status = 0;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
};

struct TransportResult final {
  bool ok = false;
  ResultCode code = ResultCode::transport;
  std::string error;
  HttpResponse response;
};

class HttpTransport {
public:
  virtual ~HttpTransport() = default;
  virtual TransportResult send(const HttpRequest& request) noexcept = 0;
};

struct ClientStatus final {
  std::string sessionId;
  bool degraded = false;
  std::string degradedSince;
  std::string lastError;
  std::string lastSuccessAt;
  bool terminal = false;
  std::int64_t replyCursor = 0;
};

struct SessionSnapshot final {
  std::string rawJson;
  int httpStatus = 0;
  std::string sessionId;
  std::string questionId;
  std::string error;
};

struct Reply final {
  std::int64_t sequence = 0;
  std::string type;
  std::string kind;
  std::string questionId;
  std::string optionId;
  std::string text;
  std::string rawJson;
};

struct Replies final {
  std::int64_t after = 0;
  std::int64_t latest = 0;
  std::string error;
  std::vector<Reply> replies;
};

struct CheckpointResult final {
  Result<SessionSnapshot> update;
  Result<Replies> replies;
};

struct ClientOptions final {
  std::string sessionId;
  std::string baseUrl;
  std::string ingestToken;
  std::string sessionKey;
  std::string nodePath;
  std::string guardPath;
  std::string preflightPath;
  std::string repositoryPath;
  int timeoutMs = 10000;
  int coalesceMs = 15000;
  int heartbeatMs = 120000;
  int probeRetryMs = 60000;
  int finishDeadlineMs = 3000;
  int maxSendAttempts = 3;
  int backoffCapMs = 60000;
  int requestsPerMinute = 50;
  bool startHeartbeat = true;
  std::function<std::int64_t()> nowMs;
  std::function<void(int)> sleep;
  std::function<Result<std::string>()> sessionKeyGenerator;
};

struct WorktreeReportOptions final {
  std::string repositoryPath;
  int budgetMs = 1500;
};

std::string machineLabel() noexcept;

// Collects the complete local Git worktree inventory without requiring a Git
// library. A bounded or failed enumeration returns worktree_incomplete with
// incomplete rows so the caller cannot present a partial inventory as complete.
Result<std::vector<Worktree>> collectWorktrees(const WorktreeReportOptions& options) noexcept;

class StatusHubClient final {
public:
  StatusHubClient(HttpTransport& transport, const ClientOptions& options) noexcept;
  ~StatusHubClient() noexcept;

  StatusHubClient(const StatusHubClient&) = delete;
  StatusHubClient& operator=(const StatusHubClient&) = delete;

  static Result<std::unique_ptr<StatusHubClient>> create(HttpTransport& transport, ClientOptions options) noexcept;
  static Result<std::string> resolveBaseUrl(HttpTransport& transport, std::string baseUrl, int timeoutMs = 10000) noexcept;

  Result<SessionSnapshot> update(const SessionPatch& patch = SessionPatch{}) noexcept;
  Result<SessionSnapshot> publishQuestion(const Question& question) noexcept;
  Result<Replies> pollReplies() noexcept;
  Result<CheckpointResult> pollAtCheckpoint(const SessionPatch& patch = SessionPatch{}) noexcept;
  Result<SessionSnapshot> finish(Status status = Status::waiting, const SessionPatch& patch = SessionPatch{}) noexcept;

  void close() noexcept;
  bool isClosed() const noexcept;
  bool isDegraded() const noexcept;
  ClientStatus status() const noexcept;
  std::vector<Warning> warnings() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// On Windows this is the WinHTTP implementation.  On other platforms it is a
// typed unavailable transport, allowing the example and core to remain portable
// without silently selecting a different network stack.
std::unique_ptr<HttpTransport> makeDefaultTransport() noexcept;

#if defined(_WIN32)
std::unique_ptr<HttpTransport> makeWinHttpTransport() noexcept;
#endif

std::unique_ptr<HttpTransport> makeCurlTransport() noexcept;

} // namespace status_hub
