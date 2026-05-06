#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>

#include "archerfish/dsp/source_factory.hpp"
#include "archerfish/dsp/waveform_type.hpp"

using namespace archerfish::dsp;

TEST_CASE("create_source with WaveformType::CW returns non-null", "[dsp][source_factory]") {
    auto src = create_source(WaveformType::CW);
    REQUIRE(src != nullptr);
}

TEST_CASE("create_source with WaveformType::Chirp returns non-null", "[dsp][source_factory]") {
    auto src = create_source(WaveformType::Chirp);
    REQUIRE(src != nullptr);
}

TEST_CASE("create_source with WaveformType::Noise returns non-null", "[dsp][source_factory]") {
    auto src = create_source(WaveformType::Noise);
    REQUIRE(src != nullptr);
}

TEST_CASE("create_source with string returns non-null for valid types", "[dsp][source_factory]") {
    for (const char* type : {"cw", "chirp", "noise", "multi_tone", "file", "pulse", "am", "fm", "pm", "ask", "fsk", "ofdm"}) {
        auto src = create_source(std::string(type));
        REQUIRE(src != nullptr);
    }
}

TEST_CASE("create_source with modulator string types returns non-null", "[dsp][source_factory]") {
    for (const char* type : {"bpsk", "qpsk", "8psk", "16qam", "64qam", "apsk16", "apsk32"}) {
        auto src = create_source(std::string(type));
        REQUIRE(src != nullptr);
    }
}

TEST_CASE("create_source with multi_tone returns non-null", "[dsp][source_factory]") {
    auto src = create_source(WaveformType::MultiTone);
    REQUIRE(src != nullptr);
}

TEST_CASE("create_source with WaveformType::File returns non-null", "[dsp][source_factory]") {
    auto src = create_source(WaveformType::File);
    REQUIRE(src != nullptr);
}
