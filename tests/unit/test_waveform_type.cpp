#include <catch2/catch_test_macros.hpp>

#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::dsp;

TEST_CASE("WaveformType round-trip to_string/waveform_type_from_string", "[waveform_type]") {
    const WaveformType all_types[] = {
        WaveformType::CW, WaveformType::Chirp, WaveformType::Noise,
        WaveformType::BPSK, WaveformType::QPSK, WaveformType::PSK8,
        WaveformType::QAM16, WaveformType::QAM64,
        WaveformType::MultiTone, WaveformType::File,
        WaveformType::Pulse, WaveformType::ASK, WaveformType::FSK,
        WaveformType::AM, WaveformType::FM, WaveformType::PM
    };

    for (auto t : all_types) {
        auto name = to_string(t);
        auto result = waveform_type_from_string(name);
        REQUIRE(result.has_value());
        REQUIRE(result.value() == t);
    }
}

TEST_CASE("waveform_type_from_string rejects invalid inputs", "[waveform_type]") {
    REQUIRE_FALSE(waveform_type_from_string("invalid").has_value());
    REQUIRE_FALSE(waveform_type_from_string("").has_value());
    REQUIRE_FALSE(waveform_type_from_string("UNKNOWN").has_value());
    REQUIRE_FALSE(waveform_type_from_string("foo").has_value());
}

TEST_CASE("waveform_type_from_string accepts aliases", "[waveform_type]") {
    REQUIRE(waveform_type_from_string("16qam").value() == WaveformType::QAM16);
    REQUIRE(waveform_type_from_string("64qam").value() == WaveformType::QAM64);
    REQUIRE(waveform_type_from_string("psk8").value() == WaveformType::PSK8);
    REQUIRE(waveform_type_from_string("16QAM").value() == WaveformType::QAM16);
    REQUIRE(waveform_type_from_string("64QAM").value() == WaveformType::QAM64);
    REQUIRE(waveform_type_from_string("PSK8").value() == WaveformType::PSK8);
}

TEST_CASE("waveform_type_from_string is case-insensitive", "[waveform_type]") {
    REQUIRE(waveform_type_from_string("CW").value() == WaveformType::CW);
    REQUIRE(waveform_type_from_string("Cw").value() == WaveformType::CW);
    REQUIRE(waveform_type_from_string("QPSK").value() == WaveformType::QPSK);
    REQUIRE(waveform_type_from_string("Chirp").value() == WaveformType::Chirp);
}

TEST_CASE("waveform_type_cli_name returns correct names", "[waveform_type]") {
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::CW)) == "CW");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::Chirp)) == "Chirp");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::Noise)) == "Noise");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::BPSK)) == "BPSK");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::QPSK)) == "QPSK");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::PSK8)) == "8PSK");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::QAM16)) == "QAM16");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::QAM64)) == "QAM64");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::MultiTone)) == "MultiTone");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::File)) == "File");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::Pulse)) == "Pulse");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::ASK)) == "ASK");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::FSK)) == "FSK");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::AM)) == "AM");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::FM)) == "FM");
    REQUIRE(std::string(waveform_type_cli_name(WaveformType::PM)) == "PM");
}
