#include <catch2/catch_test_macros.hpp>

#include "archerfish/scenario/scenario.hpp"
#include "archerfish/scenario/parser.hpp"
#include "archerfish/scenario/validator.hpp"
#include "archerfish/scenario/planner.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;

namespace {

Scenario make_marker_scenario() {
    Scenario s;
    s.metadata.name = "marker_test";

    DeviceDef dev;
    dev.id = "usrp0";
    dev.channel = 0;
    dev.rf = {2450000000.0, 10000000.0, 20.0};
    s.devices.push_back(dev);

    EmitterDef em;
    em.id = "cw1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 0.0;
    em.duration_sec = 2.0;
    em.waveform = WaveformDef{std::nullopt, archerfish::dsp::WaveformType::CW, {{"amplitude", 0.3}}};
    s.emitters.push_back(em);

    ScenarioEvent marker1;
    marker1.target_device = "usrp0";
    marker1.time_sec = 0.5;
    marker1.type = "marker";
    marker1.payload = nlohmann::json{{"name", "burst_start"}};
    s.events.push_back(marker1);

    ScenarioEvent marker2;
    marker2.target_device = "usrp0";
    marker2.time_sec = 1.5;
    marker2.type = "marker";
    marker2.payload = nlohmann::json{{"name", "mid_point"}};
    s.events.push_back(marker2);

    return s;
}

} // namespace

TEST_CASE("Marker events: parser reads marker events", "[parser][marker]") {
    std::string json_str = R"({
        "metadata": {"name": "marker_parse"},
        "devices": [{"id": "d0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 10}}],
        "emitters": [
            {"id": "e1", "device": "d0", "duration_sec": 1.0,
             "waveform": {"type": "cw", "amplitude": 0.3}}
        ],
        "events": [
            {"target_device": "d0", "time_sec": 0.5, "type": "marker", "payload": {"name": "m1"}}
        ]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE(result.has_value());
    REQUIRE(result->events.size() == 1);
    REQUIRE(result->events[0].type == "marker");
    REQUIRE(result->events[0].payload["name"].get<std::string>() == "m1");
}

TEST_CASE("Marker events: planner creates Marker timeline events", "[planner][marker]") {
    auto s = make_marker_scenario();
    auto plan_result = plan(s);
    REQUIRE(plan_result.has_value());

    int marker_count = 0;
    for (const auto& te : plan_result->timeline) {
        if (te.type == TimelineEventType::Marker) {
            marker_count++;
        }
    }
    REQUIRE(marker_count == 2);
}

TEST_CASE("Marker events: validator accepts marker event type", "[validator][marker]") {
    auto s = make_marker_scenario();
    auto result = validate(s);
    REQUIRE(result.ok());
}
