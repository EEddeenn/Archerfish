#pragma once

#include <complex>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace archerfish::dsp {

/// Compute best rational approximation L/M for target/source ratio
/// using continued fraction expansion with bounded denominator.
/// Returns {L, M} such that |target/source - L/M| is minimized with M <= max_denominator.
std::pair<unsigned, unsigned> compute_ratio(double target_rate, double source_rate,
                                            unsigned max_denominator = 1000);

/// Streaming resampler for complex<float> IQ data.
/// Wraps libsamplerate, processing I and Q channels independently
/// through identical filters to preserve phase coherence.
class ResamplerBlock {
public:
    /// Create resampler with given interpolation/decimation factors.
    ResamplerBlock(unsigned interp, unsigned decim);
    ~ResamplerBlock();

    // Move-only (owns SRC_STATE)
    ResamplerBlock(ResamplerBlock&&) noexcept;
    ResamplerBlock& operator=(ResamplerBlock&&) noexcept;
    ResamplerBlock(const ResamplerBlock&) = delete;
    ResamplerBlock& operator=(const ResamplerBlock&) = delete;

    /// Process input samples. Returns number of output samples produced.
    /// Output is written to `out`. `out` must have capacity >= max_out.
    size_t process(const std::complex<float>* in, size_t n_in,
                   std::complex<float>* out, size_t max_out);

    /// Process input samples, returning output vector.
    std::vector<std::complex<float>> process(const std::complex<float>* in, size_t n_in);

    /// Estimate output size for given input size.
    size_t estimated_output_size(size_t n_in) const;

    /// Get the interpolation factor.
    unsigned interp() const { return interp_; }
    /// Get the decimation factor.
    unsigned decim() const { return decim_; }
    /// Get the effective resampling ratio (interp/decim).
    double ratio() const;

    /// Reset internal state.
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    unsigned interp_;
    unsigned decim_;
};

} // namespace archerfish::dsp
