#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "archerfish/scenario/parser.hpp"
#include "archerfish/dsp/waveform_type.hpp"

#include <filesystem>

using namespace archerfish::scenario;
using namespace archerfish::common;
using archerfish::dsp::WaveformType;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::ContainsSubstring;

static const char* examples_dir = EXAMPLES_DIR;

TEST_CASE("Parse future_start_cw.json", "[parser]") {
    auto path = std::filesystem::path(examples_dir) / "future_start_cw.json";
    auto result = parse_scenario(path);
    REQUIRE(result.has_value());

    const auto& s = result.value();
    REQUIRE(s.metadata.name == "future_start_cw");
    REQUIRE(s.devices.size() == 1);
    REQUIRE(s.devices[0].id == "usrp0");
    REQUIRE(s.devices[0].channel.has_value());
    REQUIRE(*s.devices[0].channel == 0);
    REQUIRE_THAT(s.devices[0].rf.freq_hz, WithinAbs(2.45e9, 1.0));
    REQUIRE_THAT(s.devices[0].rf.rate_sps, WithinAbs(10e6, 1.0));
    REQUIRE_THAT(s.devices[0].rf.gain_db, WithinAbs(20.0, 1e-12));

    REQUIRE(s.emitters.size() == 1);
    REQUIRE(s.emitters[0].id == "cw1");
    REQUIRE(s.emitters[0].device == "usrp0");
    REQUIRE(s.emitters[0].channel == 0);
    REQUIRE_THAT(s.emitters[0].start_after_sec, WithinAbs(2.0, 1e-12));
    REQUIRE_THAT(s.emitters[0].duration_sec, WithinAbs(4.0, 1e-12));
    REQUIRE(s.emitters[0].waveform.has_value());
    REQUIRE(s.emitters[0].waveform->type == WaveformType::CW);
    REQUIRE(s.emitters[0].waveform->params["amplitude"].get<double>() == 0.2);
}

TEST_CASE("Parse chirp_burst.json", "[parser]") {
    auto path = std::filesystem::path(examples_dir) / "chirp_burst.json";
    auto result = parse_scenario(path);
    REQUIRE(result.has_value());

    const auto& s = result.value();
    REQUIRE(s.metadata.name == "chirp_burst");
    REQUIRE_THAT(s.devices[0].rf.freq_hz, WithinAbs(915e6, 1.0));
    REQUIRE(s.emitters[0].id == "chirp1");
    REQUIRE(s.emitters[0].waveform->type == WaveformType::Chirp);
    REQUIRE(s.emitters[0].waveform->params["f0_hz"].get<double>() == -2e6);
    REQUIRE(s.emitters[0].waveform->params["f1_hz"].get<double>() == 2e6);
    REQUIRE(s.emitters[0].waveform->params["amplitude"].get<double>() == 0.3);
}

TEST_CASE("Parse qpsk_burst.json", "[parser]") {
    auto path = std::filesystem::path(examples_dir) / "qpsk_burst.json";
    auto result = parse_scenario(path);
    REQUIRE(result.has_value());

    const auto& s = result.value();
    REQUIRE(s.metadata.name == "qpsk_burst");
    REQUIRE(s.emitters[0].waveform->type == WaveformType::QPSK);
    REQUIRE(s.emitters[0].waveform->params["symbol_rate"].get<double>() == 1e6);
    REQUIRE(s.emitters[0].waveform->params["samples_per_symbol"].get<int>() == 8);
    REQUIRE(s.emitters[0].waveform->params["rrc_alpha"].get<double>() == 0.35);
    REQUIRE(s.emitters[0].waveform->params["amplitude"].get<double>() == 0.2);
}

TEST_CASE("Parse from JSON string", "[parser]") {
    const std::string json_str = R"({
        "metadata": { "name": "inline_test", "version": "2.0" },
        "devices": [{
            "id": "dev0",
            "channel": 1,
            "rf": { "freq_hz": 1e9, "rate_sps": 5e6, "gain_db": 10.0 }
        }],
        "emitters": [{
            "id": "em0",
            "device": "dev0",
            "channel": 1,
            "start_after_sec": 0.5,
            "duration_sec": 1.0,
            "waveform": { "type": "noise", "amplitude": 0.1 }
        }],
        "reporting": { "save_plan": true, "save_metrics": true }
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());

    const auto& s = result.value();
    REQUIRE(s.metadata.name == "inline_test");
    REQUIRE(s.metadata.version.has_value());
    REQUIRE(*s.metadata.version == "2.0");
    REQUIRE(s.devices.size() == 1);
    REQUIRE(s.emitters[0].waveform->type == WaveformType::Noise);
    REQUIRE(s.reporting.save_plan);
    REQUIRE(s.reporting.save_metrics);
}

TEST_CASE("Parse missing file returns ErrorList", "[parser]") {
    auto result = parse_scenario("/nonexistent/path/scenario.json");
    REQUIRE_FALSE(result.has_value());
    const auto& errors = result.error();
    REQUIRE_FALSE(errors.empty());
    REQUIRE(errors[0].code == "E_FILE_NOT_FOUND");
}

TEST_CASE("Parse malformed JSON returns ErrorList", "[parser]") {
    auto result = parse_scenario_json("{ this is not valid json }}}");
    REQUIRE_FALSE(result.has_value());
    const auto& errors = result.error();
    REQUIRE_FALSE(errors.empty());
    REQUIRE(errors[0].code == "E_JSON_PARSE");
}

TEST_CASE("Parse missing required fields returns ErrorList", "[parser]") {
    const std::string json_str = R"({
        "devices": [{ "id": "dev0" }]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    const auto& errors = result.error();
    REQUIRE_FALSE(errors.empty());

    bool found_rf_error = false;
    for (const auto& e : errors) {
        if (e.message.find("rf") != std::string::npos) found_rf_error = true;
    }
    REQUIRE(found_rf_error);
}

TEST_CASE("resolve_waveform_refs: valid ref resolves", "[parser]") {
    const std::string json_str = R"({
        "metadata": { "name": "ref_test" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "waveforms": [{"id": "my_cw", "type": "cw", "amplitude": 0.5}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform_ref": "my_cw"}]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto& s = *result;

    REQUIRE_FALSE(s.emitters[0].waveform.has_value());
    auto errors = resolve_waveform_refs(s);
    REQUIRE(errors.empty());
    REQUIRE(s.emitters[0].waveform.has_value());
    REQUIRE_FALSE(s.emitters[0].waveform_ref.has_value());
    REQUIRE(s.emitters[0].waveform->type == WaveformType::CW);
    REQUIRE(s.emitters[0].waveform->params["amplitude"].get<double>() == 0.5);
}

TEST_CASE("resolve_waveform_refs: invalid ref produces error", "[parser]") {
    const std::string json_str = R"({
        "metadata": { "name": "bad_ref" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform_ref": "nonexistent"}]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto errors = resolve_waveform_refs(*result);
    REQUIRE_FALSE(errors.empty());
    REQUIRE(errors[0].code == "E_UNRESOLVED_REF");
}

TEST_CASE("resolve_waveform_refs: inline waveform untouched", "[parser]") {
    const std::string json_str = R"({
        "metadata": { "name": "inline_wf" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform": {"type": "cw", "amplitude": 0.8}}]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    auto& s = *result;

    REQUIRE(s.emitters[0].waveform.has_value());
    auto errors = resolve_waveform_refs(s);
    REQUIRE(errors.empty());
    REQUIRE(s.emitters[0].waveform->type == WaveformType::CW);
    REQUIRE(s.emitters[0].waveform->params["amplitude"].get<double>() == 0.8);
}

TEST_CASE("Optional fields default correctly", "[parser]") {
    const std::string json_str = R"({
        "metadata": { "name": "minimal" },
        "devices": [{"id": "usrp0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "usrp0", "waveform": {"type": "cw"}}]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    const auto& s = *result;

    REQUIRE_FALSE(s.metadata.description.has_value());
    REQUIRE_FALSE(s.metadata.version.has_value());
    REQUIRE_FALSE(s.reporting.save_plan);
    REQUIRE_FALSE(s.reporting.save_metrics);
    REQUIRE_FALSE(s.devices[0].rf.bandwidth_hz.has_value());
    REQUIRE_FALSE(s.devices[0].rf.antenna.has_value());
    REQUIRE(s.waveforms.empty());
}
