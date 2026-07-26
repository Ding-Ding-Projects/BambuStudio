#include "BoundedRegexProtocol.hpp"

#include <cstdio>
#include <cwchar>
#include <vector>

#if defined(BOUNDED_REGEX_TEST_STARTUP_DELAY_MS) || defined(BOUNDED_REGEX_TEST_EVALUATION_DELAY_MS)
#include <chrono>
#include <thread>
#endif

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/resource.h>
#ifdef __APPLE__
#include <limits>
#include <mach/mach.h>
#endif
#endif

namespace {

using namespace Slic3r::GUI::BoundedRegex;

bool read_exact(void *destination, std::size_t bytes)
{
    auto *out = static_cast<unsigned char *>(destination);
    while (bytes != 0) {
        const std::size_t count = std::fread(out, 1, bytes, stdin);
        if (count == 0)
            return false;
        out += count;
        bytes -= count;
    }
    return true;
}

bool write_exact(const void *source, std::size_t bytes)
{
    const auto *in = static_cast<const unsigned char *>(source);
    while (bytes != 0) {
        const std::size_t count = std::fwrite(in, 1, bytes, stdout);
        if (count == 0)
            return false;
        in += count;
        bytes -= count;
    }
    return std::fflush(stdout) == 0;
}

Result protocol_failure()
{
    Result result;
    result.status = Status::ProtocolError;
    result.diagnostic = "malformed bounded-regex request";
    return result;
}

#ifndef _WIN32
bool apply_resource_limits()
{
#ifdef __APPLE__
    // Current XNU enforces RLIMIT_AS in vm_map_enter() through the Mach VM
    // map-size limit. A fixed 128 MiB total limit can be below dyld and the
    // worker's already-mapped image, so grant a bounded 128 MiB *additional*
    // mapping budget from the post-exec baseline. RLIMIT_DATA is applied as a
    // second kernel-enforced mapping guard, while the parent separately watches
    // physical footprint at short intervals.
    mach_task_basic_info_data_t info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO,
                  reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS)
        return false;
    constexpr rlim_t allocation_budget = 128u * 1024u * 1024u;
    if (info.virtual_size > std::numeric_limits<rlim_t>::max() - allocation_budget)
        return false;
    const rlim_t map_limit = static_cast<rlim_t>(info.virtual_size) + allocation_budget;
    const rlimit address_space{map_limit, map_limit};
    const rlimit data_space{map_limit, map_limit};
    const rlimit stack_space{8u * 1024u * 1024u, 8u * 1024u * 1024u};
    return setrlimit(RLIMIT_AS, &address_space) == 0 &&
           setrlimit(RLIMIT_DATA, &data_space) == 0 &&
           setrlimit(RLIMIT_STACK, &stack_space) == 0;
#else
    // The parent enforces the wall-clock deadline and kills a stuck worker.
    // These limits independently cap an implementation that tries to grow a
    // deep backtracking stack or allocate an excessive automaton.
    const rlimit address_space{128u * 1024u * 1024u, 128u * 1024u * 1024u};
    const rlimit stack_space{8u * 1024u * 1024u, 8u * 1024u * 1024u};
    return setrlimit(RLIMIT_AS, &address_space) == 0 &&
           setrlimit(RLIMIT_STACK, &stack_space) == 0;
#endif
}
#endif

} // namespace

int main()
{
#ifdef _WIN32
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
        return 2;
#else
    if (!apply_resource_limits())
        return 6;
#endif
    std::setvbuf(stdin, nullptr, _IONBF, 0);
    std::setvbuf(stdout, nullptr, _IONBF, 0);

#ifdef BOUNDED_REGEX_TEST_STARTUP_DELAY_MS
    // Test-only cold-loader stand-in. Production builds never define this;
    // the delayed target proves startup has a separate bounded handshake
    // budget without weakening the per-request regex deadline.
    std::this_thread::sleep_for(
        std::chrono::milliseconds(BOUNDED_REGEX_TEST_STARTUP_DELAY_MS));
#endif

    Slic3r::GUI::BoundedRegex::Protocol::Engine engine;
    for (;;) {
        std::vector<std::uint8_t> prefix(Slic3r::GUI::BoundedRegex::Protocol::kPrefixBytes);
        if (!read_exact(prefix.data(), prefix.size()))
            return 0;

        std::size_t payload_size = 0;
        if (!Slic3r::GUI::BoundedRegex::Protocol::decode_prefix(
                prefix, Slic3r::GUI::BoundedRegex::Protocol::kRequestMagic,
                Slic3r::GUI::BoundedRegex::Protocol::kMaxRequestBytes, payload_size))
            return 3;

        std::vector<std::uint8_t> payload(payload_size);
        if (payload_size != 0 && !read_exact(payload.data(), payload.size()))
            return 4;

        Slic3r::GUI::BoundedRegex::Protocol::Request request;
        Result result = protocol_failure();
        if (Slic3r::GUI::BoundedRegex::Protocol::decode_request(payload, request)) {
#ifdef BOUNDED_REGEX_TEST_EVALUATION_DELAY_MS
            if (request.mode == Slic3r::GUI::BoundedRegex::Protocol::Mode::Search ||
                request.mode == Slic3r::GUI::BoundedRegex::Protocol::Mode::FindAll)
                std::this_thread::sleep_for(
                    std::chrono::milliseconds(BOUNDED_REGEX_TEST_EVALUATION_DELAY_MS));
#endif
#ifndef _WIN32
            if (request.mode == Slic3r::GUI::BoundedRegex::Protocol::Mode::Ping &&
                request.pattern == L"__bambu_audit_inherited_fd__") {
                wchar_t *end = nullptr;
                errno = 0;
                const long fd = std::wcstol(request.subject.c_str(), &end, 10);
                result.status = fd >= 3 && errno == 0 && end != request.subject.c_str() &&
                                        *end == L'\0' &&
                                        fcntl(static_cast<int>(fd), F_GETFD) == -1 && errno == EBADF
                                    ? Status::Valid
                                    : Status::ProtocolError;
            } else if (request.mode == Slic3r::GUI::BoundedRegex::Protocol::Mode::Ping &&
                       request.pattern == L"__bambu_audit_resource_limits__") {
                rlimit address_space{};
                rlimit stack_space{};
                const bool bounded = getrlimit(RLIMIT_AS, &address_space) == 0 &&
                                     address_space.rlim_cur != RLIM_INFINITY &&
                                     getrlimit(RLIMIT_STACK, &stack_space) == 0 &&
                                     stack_space.rlim_cur <= 8u * 1024u * 1024u;
                result.status = bounded ? Status::Valid : Status::ProtocolError;
            } else
#endif
                result = engine.evaluate(request);
        }

        const std::vector<std::uint8_t> response =
            Slic3r::GUI::BoundedRegex::Protocol::encode_result(result);
        if (!write_exact(response.data(), response.size()))
            return 5;
    }
}
