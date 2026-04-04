#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/app.hpp>

using namespace archerfish::cli;

TEST_CASE("Extended exit codes are defined", "[exit_codes]") {
    REQUIRE(static_cast<int>(ExitCode::Success) == 0);
    REQUIRE(static_cast<int>(ExitCode::GenericFailure) == 1);
    REQUIRE(static_cast<int>(ExitCode::InputValidationFailure) == 2);
    REQUIRE(static_cast<int>(ExitCode::PlanningFailure) == 3);
    REQUIRE(static_cast<int>(ExitCode::PreparationFailure) == 4);
    REQUIRE(static_cast<int>(ExitCode::ExecutionFailure) == 5);
    REQUIRE(static_cast<int>(ExitCode::Underrun) == 6);
    REQUIRE(static_cast<int>(ExitCode::DeviceDisconnect) == 7);
    REQUIRE(static_cast<int>(ExitCode::TimeoutCancellation) == 8);
}

TEST_CASE("Exit codes are distinct", "[exit_codes]") {
    int codes[] = {
        static_cast<int>(ExitCode::Success),
        static_cast<int>(ExitCode::GenericFailure),
        static_cast<int>(ExitCode::InputValidationFailure),
        static_cast<int>(ExitCode::PlanningFailure),
        static_cast<int>(ExitCode::PreparationFailure),
        static_cast<int>(ExitCode::ExecutionFailure),
        static_cast<int>(ExitCode::Underrun),
        static_cast<int>(ExitCode::DeviceDisconnect),
        static_cast<int>(ExitCode::TimeoutCancellation),
    };
    for (size_t i = 0; i < 9; ++i) {
        for (size_t j = i + 1; j < 9; ++j) {
            REQUIRE(codes[i] != codes[j]);
        }
    }
}
