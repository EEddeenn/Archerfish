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

TEST_CASE("SourceBase validate_positive throws on zero and negative", "[source_base]") {
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::validate_positive(0.0, "test"), std::invalid_argument);
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::validate_positive(-1.0, "test"), std::invalid_argument);
    CHECK_NOTHROW(archerfish::dsp::SourceBase::validate_positive(0.001, "test"));
}

TEST_CASE("SourceBase validate_non_negative throws on negative", "[source_base]") {
    CHECK_THROWS_AS(archerfish::dsp::SourceBase::validate_non_negative(-0.001, "test"), std::invalid_argument);
    CHECK_NOTHROW(archerfish::dsp::SourceBase::validate_non_negative(0.0, "test"));
    CHECK_NOTHROW(archerfish::dsp::SourceBase::validate_non_negative(1.0, "test"));
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
