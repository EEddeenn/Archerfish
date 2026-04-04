#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"
#include "archerfish/scenario/plan_io.hpp"

#include <filesystem>
#include "archerfish/dsp/waveform_type.hpp"
#include <fstream>
#include <vector>
#include <string>

using namespace archerfish::scenario;
using namespace archerfish::common;
using WaveformType = archerfish::dsp::WaveformType;

static const char* examples_dir = EXAMPLES_DIR;

static const std::vector<std::string> example_files = {
    "future_start_cw.json",
    "chirp_burst.json",
    "qpsk_burst.json",
    "mixed_scene.json",
};

static bool run_full_pipeline(const std::filesystem::path& json_path) {
    auto parse_result = parse_scenario(json_path);
    if (!parse_result.has_value()) return false;

    auto& scenario = *parse_result;
    auto ref_errors = resolve_waveform_refs(scenario);
    if (!ref_errors.empty()) return false;

    auto validation = validate(scenario);
    if (!validation.ok()) return false;

    auto plan_result = plan(scenario);
    if (!plan_result.has_value()) return false;

    auto json = plan_to_json(plan_result.value());
    if (!json.is_object()) return false;
    if (!json.contains("render_instructions")) return false;

    return true;
}

TEST_CASE("E2E Pipeline: all example files pass validation and produce valid plans", "[e2e][pipeline]") {
    for (const auto& filename : example_files) {
        auto path = std::filesystem::path(examples_dir) / filename;
        REQUIRE(std::filesystem::exists(path));
        REQUIRE(run_full_pipeline(path));
    }
}

TEST_CASE("E2E Pipeline: plan JSON serialization and file output", "[e2e][pipeline]") {
    auto temp_dir = std::filesystem::temp_directory_path() / "archerfish_e2e_pipeline";
    std::filesystem::create_directories(temp_dir);

    for (const auto& filename : example_files) {
        auto path = std::filesystem::path(examples_dir) / filename;
        auto parse_result = parse_scenario(path);
        REQUIRE(parse_result.has_value());

        auto& scenario = *parse_result;
        auto ref_err = resolve_waveform_refs(scenario);
        REQUIRE(ref_err.empty());
        REQUIRE(validate(scenario).ok());

        auto plan_result = plan(scenario);
        REQUIRE(plan_result.has_value());

        auto plan_json = plan_to_json(plan_result.value());
        REQUIRE(plan_json.is_object());

        auto stem = std::filesystem::path(filename).stem().string();
        auto out_path = temp_dir / (stem + "_plan.json");

        {
            std::ofstream out(out_path);
            out << plan_json.dump(2);
        }

        REQUIRE(std::filesystem::exists(out_path));
        REQUIRE(std::filesystem::file_size(out_path) > 0);
    }

    size_t files_written = 0;
    for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
        if (entry.path().extension() == ".json") {
            ++files_written;
        }
    }
    REQUIRE(files_written == example_files.size());

    std::filesystem::remove_all(temp_dir);
}

TEST_CASE("E2E Pipeline: mixed_scene resolves waveform_ref and validates", "[e2e][pipeline]") {
    auto path = std::filesystem::path(examples_dir) / "mixed_scene.json";
    REQUIRE(std::filesystem::exists(path));

    auto parse_result = parse_scenario(path);
    REQUIRE(parse_result.has_value());

    auto& scenario = *parse_result;
    REQUIRE(scenario.metadata.name == "mixed_scene_demo");
    REQUIRE(scenario.devices.size() == 1);
    REQUIRE(scenario.waveforms.size() == 1);
    REQUIRE(scenario.emitters.size() == 2);

    REQUIRE(scenario.emitters[0].waveform.has_value());
    REQUIRE(scenario.emitters[0].waveform->type == WaveformType::Chirp);

    REQUIRE_FALSE(scenario.emitters[1].waveform.has_value());
    REQUIRE(scenario.emitters[1].waveform_ref.has_value());
    REQUIRE(*scenario.emitters[1].waveform_ref == "qpsk_base");

    auto ref_errors = resolve_waveform_refs(scenario);
    REQUIRE(ref_errors.empty());
    REQUIRE(scenario.emitters[1].waveform.has_value());
    REQUIRE(scenario.emitters[1].waveform->type == WaveformType::QPSK);

    auto validation = validate(scenario);
    REQUIRE(validation.ok());

    auto plan_result = plan(scenario);
    REQUIRE(plan_result.has_value());

    const auto& p = plan_result.value();
    REQUIRE(p.render_instructions.size() == 2);
    REQUIRE(p.timeline.size() == 4);
    REQUIRE(p.channels.size() == 1);
}

TEST_CASE("E2E Pipeline: plan roundtrip via plan_to_json and plan_from_json", "[e2e][pipeline]") {
    for (const auto& filename : example_files) {
        auto path = std::filesystem::path(examples_dir) / filename;
        auto parse_result = parse_scenario(path);
        REQUIRE(parse_result.has_value());

        auto& scenario = *parse_result;
        auto ref_err = resolve_waveform_refs(scenario);
        REQUIRE(ref_err.empty());
        REQUIRE(validate(scenario).ok());

        auto plan_result = plan(scenario);
        REQUIRE(plan_result.has_value());

        const auto& original = plan_result.value();
        auto json = plan_to_json(original);

        auto restored_result = plan_from_json(json);
        REQUIRE(restored_result.has_value());

        const auto& restored = restored_result.value();
        REQUIRE(restored.render_instructions.size() == original.render_instructions.size());
        REQUIRE(restored.timeline.size() == original.timeline.size());
        REQUIRE(restored.channels.size() == original.channels.size());
    }
}
