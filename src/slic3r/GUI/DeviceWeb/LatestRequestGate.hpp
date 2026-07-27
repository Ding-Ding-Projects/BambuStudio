#ifndef __LATEST_REQUEST_GATE_HPP__
#define __LATEST_REQUEST_GATE_HPP__

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

namespace Slic3r { namespace GUI { namespace DeviceWeb {

// Hands out tickets for overlapping asynchronous requests; only the most
// recently issued ticket is "current". Completion handlers route through
// Ticket::run_if_current so a stale response - or one arriving after the
// owning gate was destroyed - is dropped instead of touching dead state.
// Tickets share ownership of the generation counter, so they stay safe to
// query after the gate itself is gone.
class LatestRequestGate
{
    struct State
    {
        std::atomic<std::uint64_t> current{0};
    };

public:
    class Ticket
    {
    public:
        Ticket() = default;

        bool is_current() const
        {
            return m_state && m_id != 0 && m_state->current.load(std::memory_order_acquire) == m_id;
        }

        template<typename Fn>
        bool run_if_current(Fn &&fn) const
        {
            if (!is_current())
                return false;
            std::forward<Fn>(fn)();
            return true;
        }

    private:
        friend class LatestRequestGate;
        Ticket(std::shared_ptr<const State> state, std::uint64_t id) : m_state(std::move(state)), m_id(id) {}

        std::shared_ptr<const State> m_state;
        std::uint64_t                m_id{0};
    };

    LatestRequestGate() : m_state(std::make_shared<State>()) {}
    ~LatestRequestGate() { invalidate(); }
    LatestRequestGate(const LatestRequestGate &) = delete;
    LatestRequestGate &operator=(const LatestRequestGate &) = delete;

    // Issues a new ticket and makes it the only current one.
    Ticket begin()
    {
        const std::uint64_t id = ++m_next_id;
        m_state->current.store(id, std::memory_order_release);
        return Ticket(m_state, id);
    }

    // Drops every outstanding ticket without issuing a new one.
    void invalidate() { m_state->current.store(0, std::memory_order_release); }

private:
    std::shared_ptr<State> m_state;
    std::uint64_t          m_next_id{0};
};

}}} // namespace Slic3r::GUI::DeviceWeb

#endif // __LATEST_REQUEST_GATE_HPP__
