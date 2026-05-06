#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/cmd_devices.hpp>

#include <cstdio>
#include <functional>
#include <string>
#include <unistd.h>

#include <nlohmann/json.hpp>

using namespace archerfish::cli;

namespace {

std::string capture_stdout(const std::function<int()>& fn, int& rc) {
    std::fflush(nullptr);

    int pipefd[2];
    REQUIRE(::pipe(pipefd) == 0);

    int saved_stdout = ::dup(STDOUT_FILENO);
    REQUIRE(saved_stdout != -1);
    REQUIRE(::dup2(pipefd[1], STDOUT_FILENO) != -1);
    ::close(pipefd[1]);

    rc = fn();
    std::fflush(nullptr);

    REQUIRE(::dup2(saved_stdout, STDOUT_FILENO) != -1);
    ::close(saved_stdout);

    std::string output;
    char buffer[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = ::read(pipefd[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, static_cast<size_t>(bytes_read));
    }
    ::close(pipefd[0]);

    return output;
}

} // namespace

TEST_CASE("devices list without json returns success", "[cli][devices]") {
    CliOptions opts;
    opts.json_output = false;
    int rc = cmd_devices_list(opts);
    REQUIRE(rc == 0);
}

TEST_CASE("devices list with --json outputs JSON array", "[cli][devices]") {
    CliOptions opts;
    opts.json_output = true;
    int rc = cmd_devices_list(opts);
    REQUIRE(rc == 0);
}

TEST_CASE("devices info --device stub0 returns success", "[cli][devices]") {
    CliOptions opts;
    int rc = cmd_devices_info(opts, "stub0");
    REQUIRE(rc == 0);
}

TEST_CASE("devices info --json for stub0 emits parseable JSON only", "[cli][devices]") {
    CliOptions opts;
    opts.json_output = true;

    int rc = -1;
    auto output = capture_stdout([&]() {
        return cmd_devices_info(opts, "stub0");
    }, rc);

    REQUIRE(rc == 0);
    auto parsed = nlohmann::json::parse(output);
    REQUIRE(parsed["id"] == "stub0");
    REQUIRE(parsed["type"] == "stub");
    REQUIRE(parsed["num_channels"].is_number_unsigned());
}

TEST_CASE("devices info --device unknown returns failure", "[cli][devices]") {
    CliOptions opts;
    int rc = cmd_devices_info(opts, "nonexistent");
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
}

TEST_CASE("devices info rejects empty device id", "[cli][devices]") {
    CliOptions opts;
    int rc = cmd_devices_info(opts, "");
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
}

TEST_CASE("devices info rejects blank device id", "[cli][devices]") {
    CliOptions opts;
    int rc = cmd_devices_info(opts, " \t\n");
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
}
