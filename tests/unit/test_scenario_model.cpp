#include "archerfish/scenario/parser.hpp"
#include "archerfish/dsp/waveform_type.hpp"
using archerfish::dsp::WaveformType;

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/scenario/scenario.hpp"

using namespace archerfish::scenario;
using Catch::Matchers::WithinAbs;

TEST_CASE("Default construction of all scenario structs", "[scenario][model]") {
    Metadata meta;
    REQUIRE(meta.name.empty());
    REQUIRE_FALSE(meta.description.has_value());
    REQUIRE_FALSE(meta.version.has_value());

    RfSettings rf;
    REQUIRE_THAT(rf.freq_hz, WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(rf.rate_sps, WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(rf.gain_db, WithinAbs(0.0, 1e-12));
    REQUIRE_FALSE(rf.bandwidth_hz.has_value());
    REQUIRE_FALSE(rf.antenna.has_value());

    DeviceDef dev;
    REQUIRE(dev.id.empty());
    REQUIRE_FALSE(dev.channel.has_value());

    WaveformDef wf;
    REQUIRE_FALSE(wf.id.has_value());
    REQUIRE(archerfish::dsp::to_string(wf.type) == "unknown");
    REQUIRE(wf.params.is_null());

    ImpairmentSettings imp;
    REQUIRE_FALSE(imp.cfo_hz.has_value());
    REQUIRE_FALSE(imp.phase_offset_rad.has_value());
    REQUIRE_FALSE(imp.iq_gain_imbalance_db.has_value());
    REQUIRE_FALSE(imp.iq_phase_imbalance_rad.has_value());
    REQUIRE_FALSE(imp.dc_offset_i.has_value());
    REQUIRE_FALSE(imp.dc_offset_q.has_value());
    REQUIRE_FALSE(imp.awgn_power.has_value());

    EmitterDef em;
    REQUIRE(em.id.empty());
    REQUIRE(em.device.empty());
    REQUIRE(em.channel == 0);
    REQUIRE_THAT(em.start_after_sec, WithinAbs(0.0, 1e-12));
    REQUIRE_THAT(em.duration_sec, WithinAbs(0.0, 1e-12));
    REQUIRE_FALSE(em.waveform.has_value());
    REQUIRE_FALSE(em.waveform_ref.has_value());
    REQUIRE_FALSE(em.impairments.has_value());

    ReportingConfig rpt;
    REQUIRE_FALSE(rpt.save_plan);
    REQUIRE_FALSE(rpt.save_metrics);

    Scenario scenario;
    REQUIRE(scenario.devices.empty());
    REQUIRE(scenario.waveforms.empty());
    REQUIRE(scenario.emitters.empty());
}

TEST_CASE("RfSettings field access and modification", "[scenario][model]") {
    RfSettings rf;
    rf.freq_hz = 2.45e9;
    rf.rate_sps = 10e6;
    rf.gain_db = 20.0;
    rf.bandwidth_hz = 8e6;
    rf.antenna = "TX/RX";

    REQUIRE_THAT(rf.freq_hz, WithinAbs(2.45e9, 1.0));
    REQUIRE_THAT(rf.rate_sps, WithinAbs(10e6, 1.0));
    REQUIRE_THAT(rf.gain_db, WithinAbs(20.0, 1e-12));
    REQUIRE(rf.bandwidth_hz.has_value());
    REQUIRE_THAT(*rf.bandwidth_hz, WithinAbs(8e6, 1.0));
    REQUIRE(rf.antenna.has_value());
    REQUIRE(*rf.antenna == "TX/RX");
}

TEST_CASE("EmitterDef with inline waveform", "[scenario][model]") {
    EmitterDef em;
    em.id = "cw1";
    em.device = "usrp0";
    em.channel = 0;
    em.start_after_sec = 2.0;
    em.duration_sec = 4.0;
    em.waveform = WaveformDef{std::nullopt, WaveformType::CW, nlohmann::json{{"amplitude", 0.2}}};

    REQUIRE(em.id == "cw1");
    REQUIRE(em.device == "usrp0");
    REQUIRE(em.channel == 0);
    REQUIRE_THAT(em.start_after_sec, WithinAbs(2.0, 1e-12));
    REQUIRE_THAT(em.duration_sec, WithinAbs(4.0, 1e-12));
    REQUIRE(em.waveform.has_value());
    REQUIRE(em.waveform->type == WaveformType::CW);
    REQUIRE(em.waveform->params["amplitude"].get<double>() == 0.2);
    REQUIRE_FALSE(em.waveform_ref.has_value());
}

TEST_CASE("EmitterDef with waveform_ref", "[scenario][model]") {
    EmitterDef em;
    em.id = "sig1";
    em.device = "usrp0";
    em.channel = 1;
    em.waveform_ref = "my_chirp";

    REQUIRE_FALSE(em.waveform.has_value());
    REQUIRE(em.waveform_ref.has_value());
    REQUIRE(*em.waveform_ref == "my_chirp");
}

TEST_CASE("ImpairmentSettings all fields optional", "[scenario][model]") {
    ImpairmentSettings imp;
    imp.cfo_hz = 1000.0;
    imp.phase_offset_rad = 0.5;
    imp.iq_gain_imbalance_db = -1.0;
    imp.iq_phase_imbalance_rad = 0.02;
    imp.dc_offset_i = 0.01;
    imp.dc_offset_q = -0.01;
    imp.awgn_power = -80.0;

    REQUIRE_THAT(*imp.cfo_hz, WithinAbs(1000.0, 1e-12));
    REQUIRE_THAT(*imp.phase_offset_rad, WithinAbs(0.5, 1e-12));
    REQUIRE_THAT(*imp.iq_gain_imbalance_db, WithinAbs(-1.0, 1e-12));
    REQUIRE_THAT(*imp.iq_phase_imbalance_rad, WithinAbs(0.02, 1e-12));
    REQUIRE_THAT(*imp.dc_offset_i, WithinAbs(0.01, 1e-12));
    REQUIRE_THAT(*imp.dc_offset_q, WithinAbs(-0.01, 1e-12));
    REQUIRE_THAT(*imp.awgn_power, WithinAbs(-80.0, 1e-12));
}

TEST_CASE("Scenario with named waveforms and multiple devices", "[scenario][model]") {
    Scenario s;
    s.metadata.name = "multi_device_test";
    s.metadata.version = "1.0";

    s.devices.push_back({"usrp0", 0, {2.45e9, 10e6, 20.0}});
    s.devices.push_back({"usrp1", 0, {915e6, 20e6, 18.0}});

    s.waveforms.push_back(WaveformDef{"chirp_wf", WaveformType::Chirp, nlohmann::json{{"f0_hz", -2e6}, {"f1_hz", 2e6}}});

    EmitterDef em;
    em.id = "em1";
    em.device = "usrp0";
    em.channel = 0;
    em.waveform_ref = "chirp_wf";
    s.emitters.push_back(em);

    REQUIRE(s.metadata.name == "multi_device_test");
    REQUIRE(s.metadata.version.has_value());
    REQUIRE(*s.metadata.version == "1.0");
    REQUIRE(s.devices.size() == 2);
    REQUIRE(s.waveforms.size() == 1);
    REQUIRE(s.waveforms[0].id.has_value());
    REQUIRE(*s.waveforms[0].id == "chirp_wf");
    REQUIRE(s.emitters.size() == 1);
}
