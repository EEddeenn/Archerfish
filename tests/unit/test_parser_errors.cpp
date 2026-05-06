#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "archerfish/scenario/parser.hpp"

using namespace archerfish::scenario;
using namespace archerfish::common;
using Catch::Matchers::ContainsSubstring;

TEST_CASE("Parse completely empty string returns JSON parse error", "[parser][errors]") {
    auto result = parse_scenario_json("");
    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().empty());
    REQUIRE(result.error()[0].code == "E_JSON_PARSE");
}

TEST_CASE("Parse non-object JSON returns parse error", "[parser][errors]") {
    auto result = parse_scenario_json("[]");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_JSON_PARSE");
}

TEST_CASE("Parse bare number returns parse error", "[parser][errors]") {
    auto result = parse_scenario_json("42");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_JSON_PARSE");
}

TEST_CASE("Parse null returns parse error", "[parser][errors]") {
    auto result = parse_scenario_json("null");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_JSON_PARSE");
}

TEST_CASE("Parse invalid field type returns structured error", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "bad_type" },
        "devices": [{"id": 42, "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "dev0", "waveform": {"type": "cw"}}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_JSON_TYPE");
    REQUIRE_THAT(result.error()[0].message, ContainsSubstring("device.id"));
}

TEST_CASE("Parse unsigned channel fields rejects invalid integer values", "[parser][errors]") {
    SECTION("negative device channel") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_device_channel" },
            "devices": [{"id": "dev0", "channel": -1, "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{"id": "em0", "device": "dev0", "waveform": {"type": "cw"}}]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_RANGE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("device.channel"));
    }

    SECTION("out-of-range emitter channel") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_emitter_channel" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{"id": "em0", "device": "dev0", "channel": 4294967296, "waveform": {"type": "cw"}}]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_RANGE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("emitter.channel"));
    }

    SECTION("non-integer channel definition index") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_channel_index" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "channels": [{"id": "ch0", "device": "dev0", "index": 1.5,
                          "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{"id": "em0", "device": "dev0", "channel_id": "ch0", "waveform": {"type": "cw"}}]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_TYPE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("channel.index"));
    }
}

TEST_CASE("Parse repeat count rejects invalid integer values", "[parser][errors]") {
    SECTION("fractional repeat count") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_repeat_count" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{
                "id": "em0",
                "device": "dev0",
                "waveform": {"type": "cw"},
                "repeat": {"count": 1.5, "interval_sec": 1.0}
            }]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_TYPE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("emitter.repeat.count"));
    }

    SECTION("out-of-range repeat count") {
        const std::string json_str = R"({
            "metadata": { "name": "huge_repeat_count" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{
                "id": "em0",
                "device": "dev0",
                "waveform": {"type": "cw"},
                "repeat": {"count": 2147483648, "interval_sec": 1.0}
            }]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_RANGE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("emitter.repeat.count"));
    }
}

TEST_CASE("Parse numeric fields rejects invalid JSON types with field context", "[parser][errors]") {
    SECTION("rf frequency") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_rf_freq" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": "1e9", "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{"id": "em0", "device": "dev0", "waveform": {"type": "cw"}}]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_TYPE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("rf.freq_hz"));
    }

    SECTION("waveform target power") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_target_power" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "waveforms": [{"id": "cw", "type": "cw", "target_power_dbm": "high"}],
            "emitters": [{"id": "em0", "device": "dev0", "waveform_ref": "cw"}]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_TYPE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("waveform.target_power_dbm"));
    }

    SECTION("emitter timing") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_emitter_timing" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{
                "id": "em0",
                "device": "dev0",
                "start_after_sec": false,
                "waveform": {"type": "cw"}
            }]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_TYPE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("emitter.start_after_sec"));
    }

    SECTION("repeat interval") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_repeat_interval" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{
                "id": "em0",
                "device": "dev0",
                "waveform": {"type": "cw"},
                "repeat": {"count": 1, "interval_sec": "soon"}
            }]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_TYPE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("emitter.repeat.interval_sec"));
    }

    SECTION("event time") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_event_time" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{"id": "em0", "device": "dev0", "waveform": {"type": "cw"}}],
            "events": [{"target_device": "dev0", "type": "marker", "time_sec": "now"}]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_TYPE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("event.time_sec"));
    }

    SECTION("impairment number") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_impairment_number" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{
                "id": "em0",
                "device": "dev0",
                "waveform": {"type": "cw"},
                "impairments": {"cfo_hz": "fast"}
            }]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_TYPE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("impairments.cfo_hz"));
    }
}

TEST_CASE("Parse waveform string fields rejects invalid types", "[parser][errors]") {
    SECTION("waveform id must be string") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_waveform_id" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "waveforms": [{"id": 42, "type": "cw"}],
            "emitters": [{"id": "em0", "device": "dev0", "waveform_ref": "wf0"}]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_TYPE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("waveform.id"));
    }

    SECTION("waveform type must be string") {
        const std::string json_str = R"({
            "metadata": { "name": "bad_waveform_type_field" },
            "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
            "emitters": [{"id": "em0", "device": "dev0", "waveform": {"type": 42}}]
        })";

        auto result = parse_scenario_json(json_str);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error()[0].code == "E_JSON_TYPE");
        REQUIRE_THAT(result.error()[0].message, ContainsSubstring("waveform.type"));
    }
}

TEST_CASE("Parse named string fields returns field-specific type errors", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": 42, "description": false },
        "devices": [{
            "id": "dev0",
            "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0, "antenna": 7}
        }],
        "channels": [{"id": 9, "device": false, "index": 0}],
        "sync_groups": [{"id": true, "channels": ["ch0"], "mode": 12}],
        "events": [{"target_device": 4, "type": false}],
        "reporting": {"save_plan": "yes", "save_metrics": 1},
        "emitters": [{
            "id": "em0",
            "device": 123,
            "waveform_ref": false,
            "channel_id": 99,
            "mixing": true,
            "impairments": {"phase_noise_psd_shape": 1, "fading_type": true, "pa_model": []}
        }]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());

    auto has_message = [&](const std::string& field) {
        for (const auto& e : result.error()) {
            if (e.code == "E_JSON_TYPE" && e.message.find(field) != std::string::npos) {
                return true;
            }
        }
        return false;
    };

    CHECK(has_message("metadata.name"));
    CHECK(has_message("metadata.description"));
    CHECK(has_message("rf.antenna"));
    CHECK(has_message("channel.id"));
    CHECK(has_message("sync_group.mode"));
    CHECK(has_message("event.target_device"));
    CHECK(has_message("emitter.device"));
    CHECK(has_message("emitter.waveform_ref"));
    CHECK(has_message("emitter.channel_id"));
    CHECK(has_message("emitter.mixing"));
    CHECK(has_message("impairments.phase_noise_psd_shape"));
    CHECK(has_message("impairments.fading_type"));
    CHECK(has_message("impairments.pa_model"));
    CHECK(has_message("reporting.save_plan"));
    CHECK(has_message("reporting.save_metrics"));
}

TEST_CASE("Parse wrong top-level section types returns structured errors", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": "not an object",
        "devices": {"id": "dev0"},
        "emitters": {"id": "em0"},
        "events": {"type": "marker"},
        "channels": {"id": "ch0"},
        "sync_groups": {"id": "sg0"},
        "reporting": true,
        "run": "replay"
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().size() >= 8);
    for (const auto& e : result.error()) {
        CHECK(e.code == "E_JSON_TYPE");
    }
}

TEST_CASE("Parse wrong nested object types returns structured errors", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "bad_nested" },
        "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{
            "id": "em0",
            "device": "dev0",
            "waveform": "cw",
            "impairments": [],
            "repeat": true
        }],
        "events": [{"target_device": "dev0", "type": "marker", "payload": []}],
        "channels": [42],
        "sync_groups": [{"id": "sg0", "channels": "ch0"}]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    bool found_waveform = false;
    bool found_impairments = false;
    bool found_repeat = false;
    bool found_payload = false;
    bool found_channel = false;
    bool found_sync_channels = false;
    for (const auto& e : result.error()) {
        if (e.message.find("emitter.waveform") != std::string::npos) found_waveform = true;
        if (e.message.find("emitter.impairments") != std::string::npos) found_impairments = true;
        if (e.message.find("emitter.repeat") != std::string::npos) found_repeat = true;
        if (e.message.find("event.payload") != std::string::npos) found_payload = true;
        if (e.message.find("'channel'") != std::string::npos) found_channel = true;
        if (e.message.find("sync_group.channels") != std::string::npos) found_sync_channels = true;
    }
    CHECK(found_waveform);
    CHECK(found_impairments);
    CHECK(found_repeat);
    CHECK(found_payload);
    CHECK(found_channel);
    CHECK(found_sync_channels);
}

TEST_CASE("Parse channel definitions rejects missing required fields", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "missing_channel_fields" },
        "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "channels": [{}],
        "emitters": [{"id": "em0", "device": "dev0", "channel_id": "ch0", "waveform": {"type": "cw"}}]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());

    auto has_missing = [&](const std::string& field) {
        for (const auto& e : result.error()) {
            if (e.code == "E_MISSING_FIELD" && e.message.find(field) != std::string::npos) {
                return true;
            }
        }
        return false;
    };

    CHECK(has_missing("channel.id"));
    CHECK(has_missing("channel.device"));
    CHECK(has_missing("channel.index"));
    CHECK(has_missing("channel.rf"));
}

TEST_CASE("Parse sync group channel entries rejects non-string values", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "bad_sync_channel_entry" },
        "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "channels": [{"id": "ch0", "device": "dev0", "index": 0,
                      "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "sync_groups": [{"id": "sg0", "channels": ["ch0", 42], "mode": "coherent"}],
        "emitters": [{"id": "em0", "device": "dev0", "channel_id": "ch0", "waveform": {"type": "cw"}}]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_JSON_TYPE");
    REQUIRE_THAT(result.error()[0].message, ContainsSubstring("sync_group.channels"));
}

TEST_CASE("Parse sync group definitions rejects missing required fields", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "missing_sync_fields" },
        "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "channels": [{"id": "ch0", "device": "dev0", "index": 0,
                      "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "sync_groups": [{}],
        "emitters": [{"id": "em0", "device": "dev0", "channel_id": "ch0", "waveform": {"type": "cw"}}]
    })";

    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());

    auto has_missing = [&](const std::string& field) {
        for (const auto& e : result.error()) {
            if (e.code == "E_MISSING_FIELD" && e.message.find(field) != std::string::npos) {
                return true;
            }
        }
        return false;
    };

    CHECK(has_missing("sync_group.id"));
    CHECK(has_missing("sync_group.mode"));
    CHECK(has_missing("sync_group.channels"));
}

TEST_CASE("Parse truncated JSON returns parse error", "[parser][errors]") {
    auto result = parse_scenario_json(R"({"metadata": {"name": "test")");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_JSON_PARSE");
}

TEST_CASE("Device missing rf section returns error", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "no_rf" },
        "devices": [{ "id": "dev0" }],
        "emitters": [{ "id": "em0", "device": "dev0", "waveform": {"type": "cw"} }]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    bool found_rf = false;
    for (const auto& e : result.error()) {
        if (e.message.find("rf") != std::string::npos) found_rf = true;
    }
    REQUIRE(found_rf);
}

TEST_CASE("Device missing freq_hz returns error", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "no_freq" },
        "devices": [{ "id": "dev0", "rf": { "rate_sps": 1e6, "gain_db": 0 }}],
        "emitters": [{ "id": "em0", "device": "dev0", "waveform": {"type": "cw"} }]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    bool found = false;
    for (const auto& e : result.error()) {
        if (e.message.find("freq_hz") != std::string::npos) found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Emitter missing id returns error", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "no_emitter_id" },
        "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"device": "dev0", "waveform": {"type": "cw"}}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    bool found = false;
    for (const auto& e : result.error()) {
        if (e.message.find("emitter id") != std::string::npos) found = true;
    }
    REQUIRE(found);
}

TEST_CASE("Invalid waveform type returns error", "[parser][errors]") {
    const std::string json_str = R"({
        "metadata": { "name": "bad_waveform" },
        "devices": [{"id": "dev0", "rf": {"freq_hz": 1e9, "rate_sps": 1e6, "gain_db": 0}}],
        "emitters": [{"id": "em0", "device": "dev0", "waveform": {"type": "nonexistent_waveform"}}]
    })";
    auto result = parse_scenario_json(json_str);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error()[0].code == "E_INVALID_WAVEFORM_TYPE");
}

TEST_CASE("Parse error has Config category", "[parser][errors]") {
    auto result = parse_scenario_json("{invalid}");
    REQUIRE_FALSE(result.has_value());
    for (const auto& e : result.error()) {
        CHECK(e.category == ErrorCategory::Config);
    }
}
