#include <chrono>
#include <thread>

int main()
{
    // A valid executable that deliberately never speaks the framed protocol.
    // The client must bound its background readiness handshake with the
    // dedicated startup deadline and terminate this process instead of waiting
    // on it indefinitely or blocking a UI-facing request.
    for (;;)
        std::this_thread::sleep_for(std::chrono::hours(24));
}
