#include <catch2/catch_test_macros.hpp>

#include "archerfish/runtime/state.hpp"

using namespace archerfish::runtime;

TEST_CASE("StateMachine initial state is Created", "[runtime][state]") {
    StateMachine sm;
    REQUIRE(sm.current() == RuntimeState::Created);
}

TEST_CASE("StateMachine Created -> Validated succeeds", "[runtime][state]") {
    StateMachine sm;
    REQUIRE(sm.transition_to(RuntimeState::Validated));
    REQUIRE(sm.current() == RuntimeState::Validated);
}

TEST_CASE("StateMachine Validated -> Planned succeeds", "[runtime][state]") {
    StateMachine sm{RuntimeState::Validated};
    REQUIRE(sm.transition_to(RuntimeState::Planned));
    REQUIRE(sm.current() == RuntimeState::Planned);
}

TEST_CASE("StateMachine Planned -> Prepared succeeds", "[runtime][state]") {
    StateMachine sm{RuntimeState::Planned};
    REQUIRE(sm.transition_to(RuntimeState::Prepared));
    REQUIRE(sm.current() == RuntimeState::Prepared);
}

TEST_CASE("StateMachine Prepared -> Armed succeeds", "[runtime][state]") {
    StateMachine sm{RuntimeState::Prepared};
    REQUIRE(sm.transition_to(RuntimeState::Armed));
    REQUIRE(sm.current() == RuntimeState::Armed);
}

TEST_CASE("StateMachine Armed -> Running succeeds", "[runtime][state]") {
    StateMachine sm{RuntimeState::Armed};
    REQUIRE(sm.transition_to(RuntimeState::Running));
    REQUIRE(sm.current() == RuntimeState::Running);
}

TEST_CASE("StateMachine Running -> Completed succeeds", "[runtime][state]") {
    StateMachine sm{RuntimeState::Running};
    REQUIRE(sm.transition_to(RuntimeState::Completed));
    REQUIRE(sm.current() == RuntimeState::Completed);
}

TEST_CASE("StateMachine full chain Created -> Completed", "[runtime][state]") {
    StateMachine sm;
    REQUIRE(sm.transition_to(RuntimeState::Validated));
    REQUIRE(sm.transition_to(RuntimeState::Planned));
    REQUIRE(sm.transition_to(RuntimeState::Prepared));
    REQUIRE(sm.transition_to(RuntimeState::Armed));
    REQUIRE(sm.transition_to(RuntimeState::Running));
    REQUIRE(sm.transition_to(RuntimeState::Completed));
    REQUIRE(sm.current() == RuntimeState::Completed);
}

TEST_CASE("StateMachine invalid transition Created -> Running fails", "[runtime][state]") {
    StateMachine sm;
    REQUIRE_FALSE(sm.transition_to(RuntimeState::Running));
    REQUIRE(sm.current() == RuntimeState::Created);
}

TEST_CASE("StateMachine invalid transition Completed -> Running fails", "[runtime][state]") {
    StateMachine sm{RuntimeState::Completed};
    REQUIRE_FALSE(sm.transition_to(RuntimeState::Running));
    REQUIRE(sm.current() == RuntimeState::Completed);
}

TEST_CASE("StateMachine any state -> Failed", "[runtime][state]") {
    for (auto s : {RuntimeState::Created, RuntimeState::Validated, RuntimeState::Planned,
                   RuntimeState::Prepared, RuntimeState::Armed, RuntimeState::Running}) {
        StateMachine sm{s};
        REQUIRE(sm.transition_to(RuntimeState::Failed));
        REQUIRE(sm.current() == RuntimeState::Failed);
    }
}

TEST_CASE("StateMachine any state -> Aborted", "[runtime][state]") {
    for (auto s : {RuntimeState::Created, RuntimeState::Validated, RuntimeState::Planned,
                   RuntimeState::Prepared, RuntimeState::Armed, RuntimeState::Running}) {
        StateMachine sm{s};
        REQUIRE(sm.transition_to(RuntimeState::Aborted));
        REQUIRE(sm.current() == RuntimeState::Aborted);
    }
}

TEST_CASE("StateMachine state_name returns human-readable strings", "[runtime][state]") {
    StateMachine sm;
    REQUIRE(sm.state_name(RuntimeState::Created) == "Created");
    REQUIRE(sm.state_name(RuntimeState::Validated) == "Validated");
    REQUIRE(sm.state_name(RuntimeState::Planned) == "Planned");
    REQUIRE(sm.state_name(RuntimeState::Prepared) == "Prepared");
    REQUIRE(sm.state_name(RuntimeState::Armed) == "Armed");
    REQUIRE(sm.state_name(RuntimeState::Running) == "Running");
    REQUIRE(sm.state_name(RuntimeState::Completed) == "Completed");
    REQUIRE(sm.state_name(RuntimeState::Aborted) == "Aborted");
    REQUIRE(sm.state_name(RuntimeState::Failed) == "Failed");
}

TEST_CASE("StateMachine terminal states reject all transitions", "[runtime][state]") {
    StateMachine sm_completed{RuntimeState::Completed};
    REQUIRE_FALSE(sm_completed.transition_to(RuntimeState::Created));
    REQUIRE_FALSE(sm_completed.transition_to(RuntimeState::Running));

    StateMachine sm_aborted{RuntimeState::Aborted};
    REQUIRE_FALSE(sm_aborted.transition_to(RuntimeState::Created));

    StateMachine sm_failed{RuntimeState::Failed};
    REQUIRE_FALSE(sm_failed.transition_to(RuntimeState::Created));
}
