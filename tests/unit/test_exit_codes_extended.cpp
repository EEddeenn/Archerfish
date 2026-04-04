#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/app.hpp>

using namespace archerfish::cli;

TEST_CASE("Exit code 6 Underrun represents buffer underrun", "[cli][exit_codes]") {
    REQUIRE(static_cast<int>(ExitCode::Underrun) == 6);
    REQUIRE(static_cast<int>(ExitCode::Underrun) > static_cast<int>(ExitCode::ExecutionFailure));
}

TEST_CASE("Exit code 7 DeviceDisconnect represents device loss", "[cli][exit_codes]") {
    REQUIRE(static_cast<int>(ExitCode::DeviceDisconnect) == 7);
}

TEST_CASE("Exit code 8 TimeoutCancellation represents timeout", "[cli][exit_codes]") {
    REQUIRE(static_cast<int>(ExitCode::TimeoutCancellation) == 8);
}

TEST_CASE("Extended exit codes are sequential", "[cli][exit_codes]") {
    REQUIRE(static_cast<int>(ExitCode::Underrun) + 1 == static_cast<int>(ExitCode::DeviceDisconnect));
    REQUIRE(static_cast<int>(ExitCode::DeviceDisconnect) + 1 == static_cast<int>(ExitCode::TimeoutCancellation));
}

TEST_CASE("All 9 exit codes form a contiguous range from 0 to 8", "[cli][exit_codes]") {
    std::vector<int> codes = {
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
    std::sort(codes.begin(), codes.end());
    for (int i = 0; i < 9; ++i) {
        REQUIRE(codes[i] == i);
    }
}
