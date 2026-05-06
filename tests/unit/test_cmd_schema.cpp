#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/cmd_schema.hpp>
#include <archerfish/cli/app.hpp>

#include <cstdio>
#include <functional>
#include <string>
#include <unistd.h>

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

TEST_CASE("schema print returns exit code 0 with human-readable output", "[cli][schema]") {
    CliOptions opts;
    int rc = cmd_schema_print(opts, false, false);
    REQUIRE(rc == 0);
}

TEST_CASE("schema print --json returns exit code 0", "[cli][schema]") {
    CliOptions opts;
    int rc = cmd_schema_print(opts, true, false);
    REQUIRE(rc == 0);
}

TEST_CASE("schema print --markdown returns exit code 0", "[cli][schema]") {
    CliOptions opts;
    int rc = cmd_schema_print(opts, false, true);
    REQUIRE(rc == 0);
}

TEST_CASE("schema print rejects multiple output formats", "[cli][schema]") {
    CliOptions opts;
    int rc = cmd_schema_print(opts, true, true);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));
}

TEST_CASE("schema print --markdown documents canonical OFDM parameter names", "[cli][schema]") {
    CliOptions opts;

    int rc = -1;
    auto output = capture_stdout([&]() {
        return cmd_schema_print(opts, false, true);
    }, rc);

    REQUIRE(rc == 0);
    REQUIRE(output.find("`cyclic_prefix_size`") != std::string::npos);
    REQUIRE(output.find("`cp_size`") == std::string::npos);
}

TEST_CASE("build_app has schema subcommand with print child", "[cli][schema]") {
    CliOptions opts;
    auto app = build_app(opts);
    auto* schema = app->get_subcommand("schema");
    REQUIRE(schema != nullptr);
    REQUIRE(schema->get_subcommand("print") != nullptr);
}
