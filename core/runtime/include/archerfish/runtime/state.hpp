#pragma once

#include <string>

namespace archerfish::runtime {

enum class RuntimeState {
    Created,
    Validated,
    Planned,
    Prepared,
    Armed,
    Running,
    Completed,
    Aborted,
    Failed
};

class StateMachine {
public:
    explicit StateMachine(RuntimeState initial = RuntimeState::Created);

    [[nodiscard]] RuntimeState current() const;
    [[nodiscard]] bool transition_to(RuntimeState target);
    [[nodiscard]] std::string state_name(RuntimeState s) const;

private:
    RuntimeState state_;
    static bool is_valid_transition(RuntimeState from, RuntimeState to);
};

} // namespace archerfish::runtime
