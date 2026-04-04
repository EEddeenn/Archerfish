#include <catch2/catch_test_macros.hpp>
#include <archerfish/cli/cmd_calib.hpp>
#include <archerfish/cli/app.hpp>
#include <archerfish/reporting/calibration.hpp>

#include <filesystem>
#include <fstream>

using namespace archerfish::cli;
using namespace archerfish::reporting;

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
