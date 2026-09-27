#include <catch_main.hpp>

#include "slic3r/GUI/Widgets/BoundedRegex.hpp"
#include "slic3r/GUI/Widgets/BoundedRegexProtocol.hpp"
#include "slic3r/GUI/Widgets/RegexBuilderBridgeState.hpp"
#include "slic3r/GUI/DeviceWeb/LatestRequestGate.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

TEST_CASE("latest request gate rejects delayed completion after owner destruction", "[deviceweb][async]")
{
    using Slic3r::GUI::DeviceWeb::LatestRequestGate;
    LatestRequestGate::Ticket delayed;
    {
        auto gate = std::make_unique<LatestRequestGate>();
        delayed = gate->begin();
        REQUIRE(delayed.is_current());
    }

    bool called = false;
    CHECK_FALSE(delayed.is_current());
    CHECK_FALSE(delayed.run_if_current([&called]() { called = true; }));
    CHECK_FALSE(called);
}

TEST_CASE("latest request gate accepts only the newest overlapping response", "[deviceweb][async]")
{
    using Slic3r::GUI::DeviceWeb::LatestRequestGate;
    LatestRequestGate gate;
    const auto older = gate.begin();
    const auto newer = gate.begin();

    bool older_called = false;
    bool newer_called = false;
    CHECK_FALSE(older.is_current());
    CHECK_FALSE(older.run_if_current([&older_called]() { older_called = true; }));
    CHECK(newer.is_current());
    CHECK(newer.run_if_current([&newer_called]() { newer_called = true; }));
    CHECK_FALSE(older_called);
    CHECK(newer_called);

    gate.invalidate();
    CHECK_FALSE(newer.is_current());
}

#ifndef BOUNDED_REGEX_TEST_WORKER_PATH
#error "BOUNDED_REGEX_TEST_WORKER_PATH must identify the just-built worker"
#endif
#ifndef BOUNDED_REGEX_HANGING_WORKER_PATH
#error "BOUNDED_REGEX_HANGING_WORKER_PATH must identify the silent test helper"
#endif
#ifndef BOUNDED_REGEX_DELAYED_WORKER_PATH
#error "BOUNDED_REGEX_DELAYED_WORKER_PATH must identify the delayed test helper"
#endif
#ifndef BOUNDED_REGEX_EVAL_HANGING_WORKER_PATH
#error "BOUNDED_REGEX_EVAL_HANGING_WORKER_PATH must identify the evaluation-hanging test helper"
#endif

using namespace Slic3r::GUI::BoundedRegex;

namespace {

Result wait_until_worker_ready(std::chrono::steady_clock::duration timeout = std::chrono::seconds(5))
{
    Options startup;
    startup.timeout_ms = 250;
    Result result;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        result = validate(L"", startup);
        if (result.status == Status::Valid)
            return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    return result;
}

void use_test_worker()
{
    Testing::set_worker_path(std::filesystem::path(BOUNDED_REGEX_TEST_WORKER_PATH).wstring());
    prewarm();
    const Result ready = wait_until_worker_ready();
    INFO("worker startup diagnostic: " << ready.diagnostic);
    REQUIRE(ready.status == Status::Valid);
}

std::wstring capture_text(const std::wstring &subject, const Capture &capture)
{
    if (!capture.matched)
        return {};
    return subject.substr(capture.begin, capture.length);
}

} // namespace

TEST_CASE("counted repetition bounds parse each component independently", "[bounded_regex]")
{
    // Regression: the structural scanner previously carried the lower-bound
    // digits across the comma, so {100,100} was misread as 100100.
    CHECK(Protocol::structurally_bounded(L"a{100}"));
    CHECK(Protocol::structurally_bounded(L"a{100,100}"));
    CHECK(Protocol::structurally_bounded(L"a{2,5000}"));
    CHECK(Protocol::structurally_bounded(L"a{10000}"));
    CHECK(Protocol::structurally_bounded(L"a{0,10000}"));

    CHECK_FALSE(Protocol::structurally_bounded(L"a{10001}"));
    CHECK_FALSE(Protocol::structurally_bounded(L"a{2,10001}"));
    CHECK_FALSE(Protocol::structurally_bounded(L"a{10001,2}"));
}

#ifndef __APPLE__
TEST_CASE("bounded regex distinguishes valid, invalid, match, and no-match states", "[bounded_regex]")
{
    use_test_worker();
    const Result valid = validate(L"[a-z]+");
    INFO("worker: " << std::filesystem::path(BOUNDED_REGEX_TEST_WORKER_PATH).string());
    INFO("diagnostic: " << valid.diagnostic);
    REQUIRE(valid.status == Status::Valid);
    REQUIRE(validate(L"(").status == Status::InvalidPattern);
    REQUIRE(search(L"needle", L"hay needle stack").status == Status::Match);
    REQUIRE(search(L"needle", L"haystack").status == Status::NoMatch);
}

TEST_CASE("bounded regex preserves Unicode text and capture groups", "[bounded_regex]")
{
    use_test_worker();
    const std::wstring subject = L"列印 成功 / PRINT OK";
    const Result result = find_all(L"(列印)\\s+(成功)", subject);
    REQUIRE(result.status == Status::Match);
    REQUIRE(result.matches.size() == 1);
    REQUIRE(result.matches[0].groups.size() == 3);
    CHECK(capture_text(subject, result.matches[0].groups[0]) == L"列印 成功");
    CHECK(capture_text(subject, result.matches[0].groups[1]) == L"列印");
    CHECK(capture_text(subject, result.matches[0].groups[2]) == L"成功");
}

TEST_CASE("bounded regex supports explicit multiline mode", "[bounded_regex]")
{
    use_test_worker();
    const std::wstring subject = L"first\nsecond\nthird";
    CHECK(search(L"^second$", subject).status == Status::NoMatch);
    CHECK(search(L"first.second", subject).status == Status::NoMatch);
    Options multiline;
    multiline.multiline = true;
    const Result result = search(L"^second$", subject, multiline);
    REQUIRE(result.status == Status::Match);
    REQUIRE(result.matches[0].groups[0].begin == 6);
    REQUIRE(result.matches[0].groups[0].length == 6);

    // Regression for MSVC, whose std::regex implementation does not expose
    // C++17's multiline flag: anchors must work inside a larger expression,
    // without offset or capture-group drift.
    const Result internal = search(L"(first\\n)^([a-z]+)$", subject, multiline);
    REQUIRE(internal.status == Status::Match);
    REQUIRE(internal.matches[0].groups.size() == 3);
    CHECK(internal.matches[0].groups[0].begin == 0);
    CHECK(internal.matches[0].groups[0].length == 12);
    CHECK(capture_text(subject, internal.matches[0].groups[1]) == L"first\n");
    CHECK(capture_text(subject, internal.matches[0].groups[2]) == L"second");
}

TEST_CASE("zero-width matches advance and respect the match cap", "[bounded_regex]")
{
    use_test_worker();
    const Result boundaries = find_all(L"^|$", L"ab", 10);
    REQUIRE(boundaries.status == Status::Match);
    REQUIRE(boundaries.matches.size() == 2);
    CHECK(boundaries.matches[0].groups[0].begin == 0);
    CHECK(boundaries.matches[0].groups[0].length == 0);
    CHECK(boundaries.matches[1].groups[0].begin == 2);
    CHECK(boundaries.matches[1].groups[0].length == 0);

    const Result capped = find_all(L"(?=a)", L"aaaa", 2);
    REQUIRE(capped.status == Status::Match);
    REQUIRE(capped.matches.size() == 2);
    REQUIRE(capped.match_limit_reached);

    const Result exactly_two = find_all(L"a", L"aa", 2);
    REQUIRE(exactly_two.matches.size() == 2);
    CHECK_FALSE(exactly_two.match_limit_reached);

    const Result search_one = search(L"a", L"aaaa");
    REQUIRE(search_one.matches.size() == 1);
    CHECK_FALSE(search_one.match_limit_reached);
}

TEST_CASE("application and worker protocol enforce the same hard bounds", "[bounded_regex]")
{
    use_test_worker();
    CHECK(validate(std::wstring(kMaxPatternCodeUnits, L'a')).status == Status::Valid);
    CHECK(validate(std::wstring(kMaxPatternCodeUnits + 1, L'a')).status == Status::PatternTooLong);
    CHECK(search(L"a", std::wstring(kMaxSubjectCodeUnits, L'a')).status == Status::Match);
    CHECK(search(L"^a+$", std::wstring(kMaxSubjectCodeUnits, L'a')).status == Status::Match);
    CHECK(search(L"a", std::wstring(kMaxSubjectCodeUnits + 1, L'a')).status == Status::SubjectTooLong);
    CHECK(validate(std::wstring(kMaxNestingDepth + 1, L'(') + L"a" +
                   std::wstring(kMaxNestingDepth + 1, L')')).status == Status::PatternTooComplex);

    Protocol::Request oversized;
    oversized.mode = Protocol::Mode::Validate;
    oversized.pattern.assign(kMaxPatternCodeUnits + 1, L'a');
    oversized.max_matches = 0;
    const auto frame = Protocol::encode_request(oversized);
    std::vector<std::uint8_t> prefix(frame.begin(), frame.begin() + Protocol::kPrefixBytes);
    std::size_t payload_size = 0;
    REQUIRE(Protocol::decode_prefix(prefix, Protocol::kRequestMagic,
                                    Protocol::kMaxRequestBytes, payload_size));
    std::vector<std::uint8_t> payload(frame.begin() + Protocol::kPrefixBytes, frame.end());
    Protocol::Request decoded;
    CHECK_FALSE(Protocol::decode_request(payload, decoded));
}

TEST_CASE("plain text remains literal while regex is an explicit opt-in", "[bounded_regex]")
{
    use_test_worker();
    CHECK_FALSE(plain_search(L"a.c", L"abc", false));
    CHECK(plain_search(L"a.c", L"value a.c value", false));
    CHECK(search(L"a.c", L"abc").status == Status::Match);
    CHECK(plain_search(L"cat", L"a cat naps", false, true));
    CHECK_FALSE(plain_search(L"cat", L"concatenate", false, true));
}

TEST_CASE("a missing worker fails open without evaluating in-process", "[bounded_regex]")
{
    Testing::set_worker_path(
        (std::filesystem::temp_directory_path() / "missing-bambu-regex-worker").wstring());
    prewarm();
    Result unavailable;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        unavailable = search(L"a+", L"aaaa");
        if (unavailable.diagnostic.find("worker executable is missing") != std::string::npos)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    CHECK(unavailable.status == Status::WorkerUnavailable);
    CHECK(unavailable.allows_candidate());
    CHECK(unavailable.diagnostic.find("worker executable is missing") != std::string::npos);
    use_test_worker();
    CHECK(search(L"a+", L"aaaa").status == Status::Match);
}

TEST_CASE("cold startup is prewarmed without blocking UI-facing search", "[bounded_regex]")
{
    Testing::set_worker_path(std::filesystem::path(BOUNDED_REGEX_DELAYED_WORKER_PATH).wstring());
    Options options;
    options.timeout_ms = 25;

    prewarm();
    const auto started = std::chrono::steady_clock::now();
    const Result pending = validate(L"safe", options);
    const auto prompt_elapsed = std::chrono::steady_clock::now() - started;
    CHECK(pending.status == Status::WorkerUnavailable);
    CHECK(pending.allows_candidate());
    // The production startup deadline is two seconds. Loaded CI runners can
    // deschedule the observer, so this wall-clock allowance is deliberately
    // wider while still catching the former synchronous startup path.
    CHECK(prompt_elapsed < std::chrono::milliseconds(500));

    const Result ready = wait_until_worker_ready();
    INFO("delayed worker startup diagnostic: " << ready.diagnostic);
    REQUIRE(ready.status == Status::Valid);

    // SearchPass starts its aggregate budget after successful validation, so
    // the first candidate receives the full request budget once prewarm lands.
    options.timeout_ms = 250;
    SearchPass pass(L"^safe$", options);
    CHECK(pass.evaluate(L"safe").status == Status::Match);
    CHECK(pass.evaluate(L"other").status == Status::NoMatch);
    use_test_worker();
}

TEST_CASE("a worker that never answers its startup ping hits the handshake cap", "[bounded_regex]")
{
    Testing::set_worker_path(std::filesystem::path(BOUNDED_REGEX_HANGING_WORKER_PATH).wstring());
    Options options;
    options.timeout_ms = 25;
    prewarm();
    const auto started = std::chrono::steady_clock::now();
    const Result pending = validate(L"safe", options);
    const auto prompt_elapsed = std::chrono::steady_clock::now() - started;
    CHECK(pending.status == Status::WorkerUnavailable);
    CHECK(pending.allows_candidate());
    CHECK(prompt_elapsed < std::chrono::milliseconds(500));

    Result result;
    const auto diagnostic_deadline = started + std::chrono::seconds(5);
    do {
        result = validate(L"safe", options);
        if (result.diagnostic.find("readiness ping exceeded") != std::string::npos)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < diagnostic_deadline);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    INFO("startup timeout diagnostic: " << result.diagnostic);
    CHECK(result.status == Status::WorkerUnavailable);
    CHECK(result.allows_candidate());
    CHECK(result.diagnostic.find("readiness ping exceeded") != std::string::npos);
    CHECK(elapsed >= std::chrono::milliseconds(1750));
    CHECK(elapsed < std::chrono::seconds(5));
    use_test_worker();
}

TEST_CASE("wall-clock timeout kills a stuck evaluator and the bounded worker recovers", "[bounded_regex]")
{
    Testing::set_worker_path(
        std::filesystem::path(BOUNDED_REGEX_EVAL_HANGING_WORKER_PATH).wstring());
    prewarm();
    REQUIRE(wait_until_worker_ready().status == Status::Valid);
    Options one_millisecond;
    one_millisecond.timeout_ms = 1;
    const auto started = std::chrono::steady_clock::now();
    const Result timed = search(L"safe", L"safe", one_millisecond);
    const auto elapsed = std::chrono::steady_clock::now() - started;
    REQUIRE(timed.status == Status::TimedOut);
    CHECK(elapsed < std::chrono::seconds(1));

    use_test_worker();

    // The timeout cache is request-specific. The real bounded engine gets a
    // chance to classify an adversarial request under a longer deadline rather
    // than replaying the synthetic one-millisecond timeout.
    const std::wstring pattern = L"(a+)+$";
    std::wstring adversarial(kMaxSubjectCodeUnits, L'a');
    adversarial.back() = L'!';
    Options longer_deadline;
    longer_deadline.timeout_ms = 250;
    const Result retried = search(pattern, adversarial, longer_deadline);
    CHECK(retried.status == Status::PatternTooComplex);

    // Complexity rejection does not poison a harmless subject or kill the
    // replacement worker.
    CHECK(search(pattern, L"aaaa").status == Status::Match);
    CHECK(search(L"needle", L"safe needle").status == Status::Match);
}

TEST_CASE("one filtering pass has a single aggregate deadline", "[bounded_regex]")
{
    use_test_worker();
    Options bounded;
    bounded.timeout_ms = 20;
    SearchPass pass(L"((a+)+)$", bounded);

    const auto started = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i) {
        std::wstring subject(kMaxSubjectCodeUnits - static_cast<std::size_t>(i), L'a');
        subject.back() = L'!';
        (void) pass.evaluate(subject);
    }
    const auto elapsed = std::chrono::steady_clock::now() - started;
    CHECK(pass.circuit_open());
    CHECK(pass.evaluate(L"remaining candidate").allows_candidate());
    CHECK(elapsed < std::chrono::milliseconds(500));

    // A timed-out worker is replaced asynchronously; once its readiness probe
    // succeeds, a new pass represents a new filter run/pattern state and is
    // independent of the prior pass's open circuit.
    const Result recovered = wait_until_worker_ready();
    INFO("aggregate-timeout recovery diagnostic: " << recovered.diagnostic);
    REQUIRE(recovered.status == Status::Valid);
    Options fresh_options;
    fresh_options.timeout_ms = 250;
    SearchPass fresh(L"safe", fresh_options);
    CHECK(fresh.evaluate(L"safe candidate").status == Status::Match);
}
#endif

TEST_CASE("regex builder bridge preserves pending edits bidirectionally", "[bounded_regex]")
{
    Slic3r::GUI::RegexBuilderBridgeState bridge;
    Slic3r::GUI::RegexBuilderValues host{"plain", false, false, false};
    bridge.synchronize_from_host(host);

    bridge.set_pattern_from_builder("(part)-(\\d+)");
    bridge.set_regex_from_builder(true);
    bridge.set_case_from_builder(true);
    bridge.set_word_from_builder(true);
    bridge.set_multiline_from_builder(true);

    // A host refresh must not erase callbacks that the ImGui frame has not yet
    // consumed.
    bridge.synchronize_from_host({"stale", false, false, false});
    REQUIRE(bridge.apply_pending_to_host(host));
    CHECK(host.pattern == "(part)-(\\d+)");
    CHECK(host.regex_enabled);
    CHECK(host.case_sensitive);
    CHECK(host.whole_word);
    CHECK(host.multiline);
    CHECK_FALSE(bridge.has_pending_changes());

    host = {"host edit", false, false, false};
    bridge.synchronize_from_host(host);
    CHECK(bridge.values().pattern == "host edit");
    CHECK_FALSE(bridge.values().regex_enabled);
    CHECK_FALSE(bridge.values().multiline);
}

#ifdef __APPLE__
TEST_CASE("Darwin worker remains functional with hard resource containment", "[bounded_regex]")
{
    use_test_worker();
    CHECK(Testing::worker_resource_limits_active());
    CHECK(validate(L"[a-z]+").status == Status::Valid);
    CHECK(validate(L"(").status == Status::InvalidPattern);
    CHECK(search(L"needle", L"safe needle").status == Status::Match);
    CHECK(search(L"needle", L"safe haystack").status == Status::NoMatch);
    Options multiline;
    multiline.multiline = true;
    CHECK(search(L"^second$", L"first\nsecond\nthird", multiline).status == Status::Match);
    Testing::reset_worker();
}
#endif

#ifndef _WIN32
TEST_CASE("POSIX worker inherits no unrelated parent descriptor", "[bounded_regex]")
{
    use_test_worker();
    CHECK(Testing::worker_resource_limits_active());
    const int fd = open("/dev/null", O_RDONLY);
    REQUIRE(fd >= 3);
    const int flags = fcntl(fd, F_GETFD);
    REQUIRE(flags >= 0);
    REQUIRE(fcntl(fd, F_SETFD, flags & ~FD_CLOEXEC) == 0);

    Testing::reset_worker();
    CHECK(Testing::worker_fd_is_closed(fd));
    close(fd);
    Testing::reset_worker();
}

TEST_CASE("POSIX worker starts when parent stdin and stdout are closed", "[bounded_regex]")
{
    Testing::set_worker_path(std::filesystem::path(BOUNDED_REGEX_TEST_WORKER_PATH).wstring());
    Testing::reset_worker();

    const int saved_stdin = dup(STDIN_FILENO);
    const int saved_stdout = dup(STDOUT_FILENO);
    REQUIRE(saved_stdin >= 3);
    REQUIRE(saved_stdout >= 3);
    const int closed_stdin = close(STDIN_FILENO);
    const int closed_stdout = close(STDOUT_FILENO);

    prewarm();
    const Result ready = wait_until_worker_ready();
    // The parent endpoint itself can occupy fd 0. Stop it before dup2 restores
    // the test process's stdin, otherwise restoration would sever a live worker.
    Testing::reset_worker();
    const int restored_stdin = dup2(saved_stdin, STDIN_FILENO);
    const int restored_stdout = dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdin);
    close(saved_stdout);

    REQUIRE(closed_stdin == 0);
    REQUIRE(closed_stdout == 0);
    REQUIRE(restored_stdin == STDIN_FILENO);
    REQUIRE(restored_stdout == STDOUT_FILENO);
    INFO("closed-stdio startup diagnostic: " << ready.diagnostic);
    CHECK(ready.status == Status::Valid);
}
#endif
