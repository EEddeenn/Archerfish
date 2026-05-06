#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "archerfish/dsp/source_base.hpp"

#include <cmath>
#include <complex>
#include <limits>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

class TestSource : public archerfish::dsp::SourceBase {
public:
    void configure(const nlohmann::json& params) override { configure_common(params); }
    void prepare() override {}
    size_t render_block(std::complex<float>*, size_t) override { return 0; }
    archerfish::dsp::WaveformMetadata report_metadata() const override { return {}; }
    void reset() override { reset_common(); }
};

} // namespace

TEST_CASE("SourceBase compute_block_size no duration returns max_samples", "[source_base]") {
    TestSource src;
    CHECK(src.compute_block_size(1024) == 1024);
    CHECK(src.compute_block_size(0) == 0);
    CHECK(src.compute_block_size(std::numeric_limits<size_t>::max()) == std::numeric_limits<size_t>::max());
}

TEST_CASE("SourceBase compute_block_size with duration limits output", "[source_base]") {
    TestSource src;
    nlohmann::json params;
    params["sample_rate"] = 1000.0;
    params["duration_sec"] = 1.0;
    src.configure(params);
    src.prepare();

    CHECK(src.compute_block_size(2000) == 1000);
    CHECK(src.compute_block_size(500) == 500);
}

TEST_CASE("SourceBase compute_block_size returns 0 when all produced", "[source_base]") {
    TestSource src;
    nlohmann::json params;
    params["sample_rate"] = 1000.0;
    params["duration_sec"] = 0.001;
    src.configure(params);
    src.prepare();

    CHECK(src.compute_block_size(100) == 1);
}

TEST_CASE("SourceBase rejects duration sample count overflow", "[source_base]") {
    TestSource src;
    CHECK_THROWS_AS(src.configure({{"sample_rate", 1e308}, {"duration_sec", 1e308}}), std::overflow_error);
    CHECK_THROWS_AS(src.configure({
                        {"sample_rate", static_cast<double>(std::numeric_limits<size_t>::max())},
                        {"duration_sec", 2.0},
                    }),
                    std::overflow_error);
}

TEST_CASE("SourceBase checked_sample_count rounds and validates", "[source_base]") {
    CHECK(archerfish::dsp::SourceBase::checked_sample_count(1000.0, 0.0014) == 1);
    CHECK(archerfish::dsp::SourceBase::checked_sample_count(1000.0, 0.0015) == 2);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::checked_sample_count(0.0, 1.0), std::invalid_argument);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::checked_sample_count(1.0, -1.0), std::invalid_argument);
}

TEST_CASE("SourceBase checked_positive_rounded_count validates range", "[source_base]") {
    CHECK(archerfish::dsp::SourceBase::checked_positive_rounded_count(1.4, "count") == 1);
    CHECK(archerfish::dsp::SourceBase::checked_positive_rounded_count(1.5, "count") == 2);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::checked_positive_rounded_count(0.0, "count"),
                    std::invalid_argument);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::checked_positive_rounded_count(0.49, "count"),
                    std::invalid_argument);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::checked_positive_rounded_count(
                        std::numeric_limits<double>::infinity(), "count"),
                    std::overflow_error);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::checked_positive_rounded_count(
                        std::numeric_limits<double>::quiet_NaN(), "count"),
                    std::invalid_argument);
}

TEST_CASE("SourceBase validate_positive throws on zero and negative", "[source_base]") {
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::validate_positive(0.0, "test"), std::invalid_argument);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::validate_positive(-1.0, "test"), std::invalid_argument);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::validate_positive(std::numeric_limits<double>::quiet_NaN(), "test"), std::invalid_argument);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::validate_positive(std::numeric_limits<double>::infinity(), "test"), std::invalid_argument);
    CHECK_NOTHROW(archerfish::dsp::SourceBase::validate_positive(0.001, "test"));
}

TEST_CASE("SourceBase validate_non_negative throws on negative", "[source_base]") {
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::validate_non_negative(-0.001, "test"), std::invalid_argument);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::validate_non_negative(std::numeric_limits<double>::quiet_NaN(), "test"), std::invalid_argument);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::validate_non_negative(std::numeric_limits<double>::infinity(), "test"), std::invalid_argument);
    CHECK_NOTHROW(archerfish::dsp::SourceBase::validate_non_negative(0.0, "test"));
    CHECK_NOTHROW(archerfish::dsp::SourceBase::validate_non_negative(1.0, "test"));
}

TEST_CASE("SourceBase configure_common rejects invalid common fields", "[source_base]") {
    TestSource src;
    CHECK_THROWS_AS(src.configure(nullptr), std::invalid_argument);
    CHECK_THROWS_AS(src.configure(nlohmann::json::array()), std::invalid_argument);
    CHECK_THROWS_AS(src.configure("sample_rate=1e6"), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"amplitude", "loud"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", "fast"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"duration_sec", "long"}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", 0.0}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"sample_rate", std::numeric_limits<double>::quiet_NaN()}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"amplitude", -0.1}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"amplitude", static_cast<double>(std::numeric_limits<float>::max()) * 2.0}}),
                    std::out_of_range);
    CHECK_THROWS_AS(src.configure({{"duration_sec", -0.001}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"seed", -1}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"seed", 1.5}}), std::invalid_argument);
    CHECK_THROWS_AS(src.configure({{"seed", 4294967296}}), std::out_of_range);
}

TEST_CASE("SourceBase configure_common extracts fields", "[source_base]") {
    TestSource src;
    nlohmann::json params;
    params["amplitude"] = 0.5;
    params["sample_rate"] = 2e6;
    params["duration_sec"] = 0.01;
    params["seed"] = 123u;

    src.configure(params);

    CHECK_THAT(src.amplitude(), WithinAbs(0.5, 1e-12));
    CHECK_THAT(src.sample_rate(), WithinAbs(2e6, 1e-6));
    CHECK(src.samples_produced() == 0);
    CHECK(src.seed() == 123);
}

TEST_CASE("SourceBase configure_common does not partially commit invalid updates", "[source_base]") {
    TestSource src;
    src.configure({
        {"amplitude", 0.5},
        {"sample_rate", 2e6},
        {"duration_sec", 0.01},
        {"seed", 123u},
    });

    CHECK_THROWS_AS(src.configure({
                        {"amplitude", 0.9},
                        {"sample_rate", 3e6},
                        {"duration_sec", -1.0},
                        {"seed", 456u},
                    }),
                    std::invalid_argument);

    CHECK_THAT(src.amplitude(), WithinAbs(0.5, 1e-12));
    CHECK_THAT(src.sample_rate(), WithinAbs(2e6, 1e-6));
    CHECK(src.seed() == 123u);

    archerfish::dsp::WaveformMetadata meta;
    src.fill_common_metadata(meta);
    REQUIRE(meta.duration_sec.has_value());
    CHECK_THAT(meta.duration_sec.value(), WithinAbs(0.01, 1e-12));
}

TEST_CASE("SourceBase fill_common_metadata populates correctly", "[source_base]") {
    TestSource src;
    nlohmann::json params;
    params["sample_rate"] = 5e6;
    params["duration_sec"] = 0.5;
    src.configure(params);

    archerfish::dsp::WaveformMetadata meta;
    src.fill_common_metadata(meta);

    CHECK_THAT(meta.sample_rate, WithinAbs(5e6, 1e-6));
    REQUIRE(meta.duration_sec.has_value());
    CHECK_THAT(meta.duration_sec.value(), WithinAbs(0.5, 1e-12));
    CHECK_FALSE(meta.repeats);
}

TEST_CASE("SourceBase fill_common_metadata repeats when no duration", "[source_base]") {
    TestSource src;
    nlohmann::json params;
    params["sample_rate"] = 1e6;
    src.configure(params);

    archerfish::dsp::WaveformMetadata meta;
    src.fill_common_metadata(meta);

    CHECK(meta.repeats);
    CHECK_FALSE(meta.duration_sec.has_value());
}

TEST_CASE("SourceBase reset_common resets counter", "[source_base]") {
    TestSource src;
    nlohmann::json params;
    params["sample_rate"] = 1000.0;
    params["duration_sec"] = 0.001;
    src.configure(params);
    src.prepare();

    CHECK(src.compute_block_size(100) == 1);
}
