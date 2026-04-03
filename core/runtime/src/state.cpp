#include "archerfish/runtime/state.hpp"

#include <array>
#include <utility>

namespace archerfish::runtime {

StateMachine::StateMachine(RuntimeState initial)
    : state_(initial) {}

RuntimeState StateMachine::current() const {
    return state_;
}

bool StateMachine::transition_to(RuntimeState target) {
    if (is_valid_transition(state_, target)) {
        state_ = target;
        return true;
    }
    return false;
}

std::string StateMachine::state_name(RuntimeState s) const {
    static constexpr std::array<std::pair<RuntimeState, const char*>, 9> names = {{
        {RuntimeState::Created, "Created"},
        {RuntimeState::Validated, "Validated"},
        {RuntimeState::Planned, "Planned"},
        {RuntimeState::Prepared, "Prepared"},
        {RuntimeState::Armed, "Armed"},
        {RuntimeState::Running, "Running"},
        {RuntimeState::Completed, "Completed"},
        {RuntimeState::Aborted, "Aborted"},
        {RuntimeState::Failed, "Failed"},
    }};
    for (const auto& [state, name] : names) {
        if (state == s) return name;
    }
    return "Unknown";
}

bool StateMachine::is_valid_transition(RuntimeState from, RuntimeState to) {
    if (from == to) return false;

    switch (from) {
        case RuntimeState::Created:    return to == RuntimeState::Validated || to == RuntimeState::Failed || to == RuntimeState::Aborted;
        case RuntimeState::Validated:  return to == RuntimeState::Planned   || to == RuntimeState::Failed || to == RuntimeState::Aborted;
        case RuntimeState::Planned:    return to == RuntimeState::Prepared  || to == RuntimeState::Failed || to == RuntimeState::Aborted;
        case RuntimeState::Prepared:   return to == RuntimeState::Armed     || to == RuntimeState::Failed || to == RuntimeState::Aborted;
        case RuntimeState::Armed:      return to == RuntimeState::Running   || to == RuntimeState::Failed || to == RuntimeState::Aborted;
        case RuntimeState::Running:    return to == RuntimeState::Completed || to == RuntimeState::Failed || to == RuntimeState::Aborted;
        case RuntimeState::Completed:  return false;
        case RuntimeState::Aborted:    return false;
        case RuntimeState::Failed:     return false;
    }
    return false;
}

} // namespace archerfish::runtime
