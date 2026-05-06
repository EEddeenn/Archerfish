#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/cmd_calib.hpp>
#include <archerfish/cli/app.hpp>
#include <archerfish/reporting/calibration.hpp>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <unistd.h>

#include <nlohmann/json.hpp>

using namespace archerfish::cli;
using namespace archerfish::reporting;

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

TEST_CASE("build_app has calib subcommand with init/show/import children", "[cli][calib]") {
    CliOptions opts;
    auto app = build_app(opts);
    auto* calib = app->get_subcommand("calib");
    REQUIRE(calib != nullptr);
    REQUIRE(calib->get_subcommand("init") != nullptr);
    REQUIRE(calib->get_subcommand("show") != nullptr);
    REQUIRE(calib->get_subcommand("import") != nullptr);
}

TEST_CASE("calib init creates a calibration file", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_init";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);
    int rc = cmd_calib_init(opts, "testdev", 0);
    REQUIRE(rc == 0);

    auto expected_path = CalibrationData::calibration_file("testdev", 0);
    REQUIRE(std::filesystem::exists(expected_path));

    std::ifstream f(expected_path);
    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    auto result = CalibrationData::from_json(content);
    REQUIRE(result.has_value());
    REQUIRE(result->device_id == "testdev");
    REQUIRE(result->channel == 0);
    REQUIRE(result->entries.empty());

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib init rejects duplicate", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_dup";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);
    cmd_calib_init(opts, "dupdev", 0);
    int rc = cmd_calib_init(opts, "dupdev", 0);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib commands reject unsafe device IDs", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_bad_device";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);

    CHECK(cmd_calib_init(opts, "", 0) == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK(cmd_calib_init(opts, " \t\n", 0) == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK(cmd_calib_init(opts, "../escape", 0) == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK(cmd_calib_show(opts, " \t\n", 0, false) == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK(cmd_calib_show(opts, "../escape", 0, false) == static_cast<int>(ExitCode::InputValidationFailure));
    CHECK(cmd_calib_show(opts, "../escape", 0, true) == static_cast<int>(ExitCode::InputValidationFailure));

    auto import_file = test_dir / "import.json";
    {
        std::ofstream f(import_file);
        f << R"({
            "device_id": "original",
            "channel": 0,
            "timestamp": "2026-04-04T12:00:00Z",
            "entries": []
        })";
    }
    CHECK(cmd_calib_import(opts, import_file.string(), "../escape", 0) ==
          static_cast<int>(ExitCode::InputValidationFailure));
    CHECK(cmd_calib_import(opts, import_file.string(), " \t\n", 0) ==
          static_cast<int>(ExitCode::InputValidationFailure));

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib show --all accepts optional device filter", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_show_all_filter";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);
    REQUIRE(cmd_calib_init(opts, "dev_a", 0) == 0);
    REQUIRE(cmd_calib_init(opts, "dev_a", 1) == 0);
    REQUIRE(cmd_calib_init(opts, "dev_b", 0) == 0);

    CHECK(cmd_calib_show(opts, "dev_a", 0, true) == 0);
    CHECK(cmd_calib_show(opts, "missing_dev", 0, true) == 0);
    CHECK(cmd_calib_show(opts, "", 0, true) == 0);

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib show --all --json emits one parseable array", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_show_all_json";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);
    REQUIRE(cmd_calib_init(opts, "dev_a", 0) == 0);
    REQUIRE(cmd_calib_init(opts, "dev_a", 1) == 0);
    REQUIRE(cmd_calib_init(opts, "dev_b", 0) == 0);

    opts.json_output = true;
    int rc = -1;
    auto output = capture_stdout([&]() {
        return cmd_calib_show(opts, "dev_a", 0, true);
    }, rc);

    REQUIRE(rc == 0);
    auto parsed = nlohmann::json::parse(output);
    REQUIRE(parsed.is_array());
    REQUIRE(parsed.size() == 2);
    REQUIRE(parsed[0]["device_id"] == "dev_a");
    REQUIRE(parsed[1]["device_id"] == "dev_a");

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib show --all --json emits empty array when no files match", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_show_all_json_empty";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);
    REQUIRE(cmd_calib_init(opts, "dev_a", 0) == 0);

    opts.json_output = true;
    int rc = -1;
    auto output = capture_stdout([&]() {
        return cmd_calib_show(opts, "missing_dev", 0, true);
    }, rc);

    REQUIRE(rc == 0);
    auto parsed = nlohmann::json::parse(output);
    REQUIRE(parsed.is_array());
    REQUIRE(parsed.empty());

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib show displays calibration data", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_show";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);
    cmd_calib_init(opts, "showdev", 1);

    int rc = cmd_calib_show(opts, "showdev", 1, false);
    REQUIRE(rc == 0);

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib show --json displays JSON output", "[cli][calib]") {
    CliOptions opts;
    opts.json_output = true;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_show_json";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);
    cmd_calib_init(opts, "jsondev", 2);

    int rc = cmd_calib_show(opts, "jsondev", 2, false);
    REQUIRE(rc == 0);

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib show returns error for nonexistent file", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_noexist";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);
    int rc = cmd_calib_show(opts, "noddev", 0, false);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib show rejects file metadata that does not match requested device channel", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_mismatch";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);

    auto dir = CalibrationData::calibration_dir();
    std::filesystem::create_directories(dir);
    auto path = CalibrationData::calibration_file("requested", 2);
    {
        std::ofstream f(path);
        f << R"({
            "device_id": "other",
            "channel": 7,
            "timestamp": "2026-04-04T12:00:00Z",
            "entries": []
        })";
    }

    int rc = cmd_calib_show(opts, "requested", 2, false);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib import installs valid calibration file", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_import";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);

    std::string calib_json = R"({
        "device_id": "original",
        "channel": 5,
        "timestamp": "2026-04-04T12:00:00Z",
        "entries": [
            {"freq_hz": 1e9, "gain_db": 10.0, "measured_power_dbm": -5.0, "expected_power_dbm": -5.0, "error_db": 0.0}
        ]
    })";
    auto import_file = test_dir / "import.json";
    {
        std::ofstream f(import_file);
        f << calib_json;
    }

    int rc = cmd_calib_import(opts, import_file.string(), "imported_dev", 3);
    REQUIRE(rc == 0);

    auto dest = CalibrationData::calibration_file("imported_dev", 3);
    REQUIRE(std::filesystem::exists(dest));

    std::ifstream df(dest);
    std::string content((std::istreambuf_iterator<char>(df)),
                        std::istreambuf_iterator<char>());
    auto result = CalibrationData::from_json(content);
    REQUIRE(result.has_value());
    REQUIRE(result->device_id == "imported_dev");
    REQUIRE(result->channel == 3);
    REQUIRE(result->entries.size() == 1);

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib import rejects malformed file", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_badimport";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);

    auto bad_file = test_dir / "bad.json";
    {
        std::ofstream f(bad_file);
        f << "not valid json";
    }

    int rc = cmd_calib_import(opts, bad_file.string(), "dev", 0);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib import rejects nonexistent file", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_noimport";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);
    int rc = cmd_calib_import(opts, "/nonexistent/file.json", "dev", 0);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib import rejects unreadable paths", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_import_dir";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    setenv("HOME", test_dir.string().c_str(), 1);
    int rc = cmd_calib_import(opts, test_dir.string(), "dev", 0);
    REQUIRE(rc == static_cast<int>(ExitCode::InputValidationFailure));

    std::filesystem::remove_all(test_dir);
}

TEST_CASE("calib commands return errors when calibration directory cannot be created", "[cli][calib]") {
    CliOptions opts;
    auto test_dir = std::filesystem::temp_directory_path() / "archerfish_test_calib_blocked_home";
    std::filesystem::remove_all(test_dir);
    std::filesystem::create_directories(test_dir);

    auto blocked_home = test_dir / "home_file";
    {
        std::ofstream f(blocked_home);
        f << "not a directory";
    }

    setenv("HOME", blocked_home.string().c_str(), 1);
    CHECK(cmd_calib_init(opts, "blocked", 0) == static_cast<int>(ExitCode::GenericFailure));

    auto import_file = test_dir / "import.json";
    {
        std::ofstream f(import_file);
        f << R"({
            "device_id": "original",
            "channel": 0,
            "timestamp": "2026-04-04T12:00:00Z",
            "entries": []
        })";
    }
    CHECK(cmd_calib_import(opts, import_file.string(), "blocked", 0) ==
          static_cast<int>(ExitCode::GenericFailure));

    std::filesystem::remove_all(test_dir);
}
