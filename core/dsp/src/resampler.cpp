#include "archerfish/dsp/resampler.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cmath>
#include <limits>
#include <numeric>
#include <samplerate.h>
#include <stdexcept>
#include <vector>

namespace archerfish::dsp {

namespace {

constexpr size_t kMaxBatchOutputSamples = 16'000'000;

unsigned gcd_unsigned(unsigned a, unsigned b) {
    while (b != 0) {
        unsigned t = b;
        b = a % b;
        a = t;
    }
    return a;
}

size_t checked_output_capacity(size_t n_in, double src_ratio, size_t slack) {
    if (!std::isfinite(src_ratio) || src_ratio <= 0.0) {
        throw std::invalid_argument("ResamplerBlock::process: ratio must be finite and positive");
    }
    const long double required = static_cast<long double>(n_in) * static_cast<long double>(src_ratio) +
                                 static_cast<long double>(slack);
    const long double size_limit = static_cast<long double>(std::numeric_limits<size_t>::max());
    const long double frame_limit = static_cast<long double>(std::numeric_limits<long>::max());
    if (required > size_limit || required > frame_limit ||
        required > static_cast<long double>(kMaxBatchOutputSamples)) {
        throw std::invalid_argument("ResamplerBlock::process: output buffer size is too large");
    }
    return static_cast<size_t>(required);
}

} // namespace

// ---------------------------------------------------------------------------
// compute_ratio — continued-fraction best rational approximation
// ---------------------------------------------------------------------------
std::pair<unsigned, unsigned> compute_ratio(double target_rate, double source_rate,
                                            unsigned max_denominator) {
    if (!std::isfinite(source_rate) || !std::isfinite(target_rate) ||
        source_rate <= 0.0 || target_rate <= 0.0) {
        throw std::invalid_argument("compute_ratio: rates must be finite and positive");
    }
    if (max_denominator == 0) {
        throw std::invalid_argument("compute_ratio: max_denominator must be nonzero");
    }

    double x = target_rate / source_rate;
    if (x > static_cast<double>(std::numeric_limits<unsigned>::max())) {
        throw std::invalid_argument("compute_ratio: ratio is too large");
    }

    unsigned h_prev2 = 0, k_prev2 = 1;
    unsigned h_prev1 = 1, k_prev1 = 0;

    double remainder = x;

    for (;;) {
        double q_d = std::floor(remainder);
        if (q_d + 1.0 - remainder < 1e-8)
            q_d += 1.0;
        if (q_d > static_cast<double>(std::numeric_limits<unsigned>::max())) {
            if (k_prev1 > 0) {
                const unsigned max_q = (max_denominator - k_prev2) / k_prev1;
                if (max_q > 0) {
                    const uint64_t sh = static_cast<uint64_t>(max_q) * h_prev1 + h_prev2;
                    const uint64_t sk = static_cast<uint64_t>(max_q) * k_prev1 + k_prev2;
                    if (sh <= std::numeric_limits<unsigned>::max() &&
                        sk <= max_denominator && sk > 0) {
                        h_prev1 = static_cast<unsigned>(sh);
                        k_prev1 = static_cast<unsigned>(sk);
                    }
                }
            }
            break;
        }

        unsigned q = static_cast<unsigned>(q_d);

        const uint64_t h_wide = static_cast<uint64_t>(q) * h_prev1 + h_prev2;
        const uint64_t k_wide = static_cast<uint64_t>(q) * k_prev1 + k_prev2;
        if (h_wide > std::numeric_limits<unsigned>::max() ||
            k_wide > std::numeric_limits<unsigned>::max()) {
            break;
        }
        unsigned h = static_cast<unsigned>(h_wide);
        unsigned k = static_cast<unsigned>(k_wide);

        if (k > max_denominator) {
            if (q > 0 && k_prev1 > 0) {
                unsigned max_q = (max_denominator - k_prev2) / k_prev1;
                if (max_q > 0) {
                    const uint64_t sh = static_cast<uint64_t>(max_q) * h_prev1 + h_prev2;
                    const uint64_t sk = static_cast<uint64_t>(max_q) * k_prev1 + k_prev2;
                    if (sh <= std::numeric_limits<unsigned>::max() &&
                        sk <= max_denominator && sk > 0) {
                        h_prev2 = h_prev1;
                        k_prev2 = k_prev1;
                        h_prev1 = static_cast<unsigned>(sh);
                        k_prev1 = static_cast<unsigned>(sk);
                    }
                }
            }
            break;
        }

        h_prev2 = h_prev1;
        k_prev2 = k_prev1;
        h_prev1 = h;
        k_prev1 = k;

        double frac = remainder - q_d;
        if (frac < 0.0) frac = 0.0;
        if (frac < 1e-8)
            break;
        remainder = 1.0 / frac;
    }

    if (h_prev1 == 0) {
        h_prev1 = 1;
        k_prev1 = max_denominator;
    }

    unsigned g = gcd_unsigned(h_prev1, k_prev1);
    return {h_prev1 / g, k_prev1 / g};
}

// ---------------------------------------------------------------------------
// ResamplerBlock::Impl — owns two SRC_STATE pointers (I and Q channels)
// ---------------------------------------------------------------------------
struct ResamplerBlock::Impl {
    SRC_STATE* state_i{nullptr};
    SRC_STATE* state_q{nullptr};
    std::vector<float> i_buf;
    std::vector<float> q_buf;
    std::vector<float> i_out;
    std::vector<float> q_out;

    Impl(unsigned, unsigned) {
        int error = 0;
        state_i = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &error);
        if (!state_i)
            throw std::runtime_error("Failed to create I channel SRC state");

        error = 0;
        state_q = src_new(SRC_SINC_MEDIUM_QUALITY, 1, &error);
        if (!state_q) {
            src_delete(state_i);
            throw std::runtime_error("Failed to create Q channel SRC state");
        }
    }

    ~Impl() {
        if (state_i) src_delete(state_i);
        if (state_q) src_delete(state_q);
    }

    Impl(Impl&& other) noexcept
        : state_i(other.state_i), state_q(other.state_q),
          i_buf(std::move(other.i_buf)), q_buf(std::move(other.q_buf)),
          i_out(std::move(other.i_out)), q_out(std::move(other.q_out)) {
        other.state_i = nullptr;
        other.state_q = nullptr;
    }

    Impl& operator=(Impl&& other) noexcept {
        if (this != &other) {
            if (state_i) src_delete(state_i);
            if (state_q) src_delete(state_q);
            state_i = other.state_i;
            state_q = other.state_q;
            i_buf = std::move(other.i_buf);
            q_buf = std::move(other.q_buf);
            i_out = std::move(other.i_out);
            q_out = std::move(other.q_out);
            other.state_i = nullptr;
            other.state_q = nullptr;
        }
        return *this;
    }

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
};

// ---------------------------------------------------------------------------
// ResamplerBlock — constructors / destructors
// ---------------------------------------------------------------------------
ResamplerBlock::ResamplerBlock(unsigned interp, unsigned decim)
    : impl_(nullptr), interp_(interp), decim_(decim) {
    if (interp == 0 || decim == 0) {
        throw std::invalid_argument("ResamplerBlock: interp and decim must be nonzero");
    }
    impl_ = std::make_unique<Impl>(interp, decim);
}

ResamplerBlock::~ResamplerBlock() = default;
ResamplerBlock::ResamplerBlock(ResamplerBlock&&) noexcept = default;
ResamplerBlock& ResamplerBlock::operator=(ResamplerBlock&&) noexcept = default;

// ---------------------------------------------------------------------------
// Streaming overload: single src_process call with end_of_input = 0.
// May produce fewer output frames than n_in * ratio — caller must handle this.
// ---------------------------------------------------------------------------
size_t ResamplerBlock::process(const std::complex<float>* in, size_t n_in,
                               std::complex<float>* out, size_t max_out) {
    if (n_in == 0)
        return 0;
    if (in == nullptr) {
        throw std::invalid_argument("ResamplerBlock::process: input must not be null");
    }
    if (out == nullptr) {
        throw std::invalid_argument("ResamplerBlock::process: output must not be null");
    }
    if (max_out == 0) {
        return 0;
    }
    if (n_in > static_cast<size_t>(std::numeric_limits<long>::max()) ||
        max_out > static_cast<size_t>(std::numeric_limits<long>::max())) {
        throw std::invalid_argument("ResamplerBlock::process: buffer size is too large");
    }

    impl_->i_buf.resize(n_in);
    impl_->q_buf.resize(n_in);

    for (size_t i = 0; i < n_in; ++i) {
        impl_->i_buf[i] = in[i].real();
        impl_->q_buf[i] = in[i].imag();
    }

    impl_->i_out.resize(max_out);
    impl_->q_out.resize(max_out);

    double src_ratio = static_cast<double>(interp_) / static_cast<double>(decim_);

    SRC_DATA sd_i{};
    sd_i.data_in = impl_->i_buf.data();
    sd_i.input_frames = static_cast<long>(n_in);
    sd_i.data_out = impl_->i_out.data();
    sd_i.output_frames = static_cast<long>(max_out);
    sd_i.src_ratio = src_ratio;
    sd_i.end_of_input = 0;

    int err = src_process(impl_->state_i, &sd_i);
    if (err != 0)
        throw std::runtime_error(src_strerror(err));

    SRC_DATA sd_q{};
    sd_q.data_in = impl_->q_buf.data();
    sd_q.input_frames = static_cast<long>(n_in);
    sd_q.data_out = impl_->q_out.data();
    sd_q.output_frames = static_cast<long>(max_out);
    sd_q.src_ratio = src_ratio;
    sd_q.end_of_input = 0;

    err = src_process(impl_->state_q, &sd_q);
    if (err != 0)
        throw std::runtime_error(src_strerror(err));

    long generated = std::min(sd_i.output_frames_gen, sd_q.output_frames_gen);
    for (long k = 0; k < generated; ++k)
        out[k] = std::complex<float>(impl_->i_out[k], impl_->q_out[k]);

    return static_cast<size_t>(generated);
}

// ---------------------------------------------------------------------------
// Batch (vector) overload: drains ALL input, flushes, and resets.
//
// 1. Feed input with end_of_input=0 in a loop until all input is consumed.
// 2. Flush with end_of_input=1 and input_frames=0.
// 3. Reset state so the ResamplerBlock is ready for the next independent call.
// ---------------------------------------------------------------------------
std::vector<std::complex<float>> ResamplerBlock::process(const std::complex<float>* in,
                                                         size_t n_in) {
    if (n_in == 0)
        return {};
    if (in == nullptr) {
        throw std::invalid_argument("ResamplerBlock::process: input must not be null");
    }
    if (n_in > static_cast<size_t>(std::numeric_limits<long>::max())) {
        throw std::invalid_argument("ResamplerBlock::process: input size is too large");
    }

    double src_ratio = static_cast<double>(interp_) / static_cast<double>(decim_);

    // Deinterleave I and Q
    impl_->i_buf.resize(n_in);
    impl_->q_buf.resize(n_in);
    for (size_t i = 0; i < n_in; ++i) {
        impl_->i_buf[i] = in[i].real();
        impl_->q_buf[i] = in[i].imag();
    }

    // Allocate generous output buffers (n_in * ratio + slack for flush tail)
    size_t buf_size = checked_output_capacity(n_in, src_ratio, 128);
    std::vector<float> all_i(buf_size);
    std::vector<float> all_q(buf_size);
    size_t total_gen = 0;

    // --- Phase 1: feed all input with end_of_input = 0 ---
    size_t input_offset = 0;
    while (input_offset < n_in) {
        size_t rem_in = n_in - input_offset;
        size_t rem_out = buf_size - total_gen;

        SRC_DATA sd_i{};
        sd_i.data_in = impl_->i_buf.data() + input_offset;
        sd_i.input_frames = static_cast<long>(rem_in);
        sd_i.data_out = all_i.data() + total_gen;
        sd_i.output_frames = static_cast<long>(rem_out);
        sd_i.src_ratio = src_ratio;
        sd_i.end_of_input = 0;

        int err = src_process(impl_->state_i, &sd_i);
        if (err != 0) throw std::runtime_error(src_strerror(err));

        SRC_DATA sd_q{};
        sd_q.data_in = impl_->q_buf.data() + input_offset;
        sd_q.input_frames = static_cast<long>(rem_in);
        sd_q.data_out = all_q.data() + total_gen;
        sd_q.output_frames = static_cast<long>(rem_out);
        sd_q.src_ratio = src_ratio;
        sd_q.end_of_input = 0;

        err = src_process(impl_->state_q, &sd_q);
        if (err != 0) throw std::runtime_error(src_strerror(err));

        long gen = std::min(sd_i.output_frames_gen, sd_q.output_frames_gen);
        long used = std::min(sd_i.input_frames_used, sd_q.input_frames_used);

        total_gen += static_cast<size_t>(gen);
        input_offset += static_cast<size_t>(used);

        if (used == 0 && gen == 0)
            break; // safety: avoid infinite loop
    }

    // --- Phase 2: flush with end_of_input = 1, input_frames = 0 ---
    for (;;) {
        size_t rem_out = buf_size - total_gen;
        if (rem_out == 0) break;

        SRC_DATA sd_i{};
        sd_i.data_in = impl_->i_buf.data(); // dummy pointer (not read)
        sd_i.input_frames = 0;
        sd_i.data_out = all_i.data() + total_gen;
        sd_i.output_frames = static_cast<long>(rem_out);
        sd_i.src_ratio = src_ratio;
        sd_i.end_of_input = 1;

        int err = src_process(impl_->state_i, &sd_i);
        if (err != 0) throw std::runtime_error(src_strerror(err));

        SRC_DATA sd_q{};
        sd_q.data_in = impl_->q_buf.data(); // dummy pointer (not read)
        sd_q.input_frames = 0;
        sd_q.data_out = all_q.data() + total_gen;
        sd_q.output_frames = static_cast<long>(rem_out);
        sd_q.src_ratio = src_ratio;
        sd_q.end_of_input = 1;

        err = src_process(impl_->state_q, &sd_q);
        if (err != 0) throw std::runtime_error(src_strerror(err));

        long gen = std::min(sd_i.output_frames_gen, sd_q.output_frames_gen);
        if (gen == 0) break;
        total_gen += static_cast<size_t>(gen);
    }

    // --- Phase 3: reset state for next independent call ---
    src_reset(impl_->state_i);
    src_reset(impl_->state_q);

    // Reinterleave I and Q into complex output
    std::vector<std::complex<float>> result(total_gen);
    for (size_t i = 0; i < total_gen; ++i)
        result[i] = std::complex<float>(all_i[i], all_q[i]);

    return result;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------
size_t ResamplerBlock::estimated_output_size(size_t n_in) const {
    double r = static_cast<double>(interp_) / static_cast<double>(decim_);
    const long double required = static_cast<long double>(n_in) * static_cast<long double>(r) + 64.0L;
    if (required > static_cast<long double>(std::numeric_limits<size_t>::max())) {
        throw std::overflow_error("ResamplerBlock::estimated_output_size overflow");
    }
    return static_cast<size_t>(required);
}

double ResamplerBlock::ratio() const {
    return static_cast<double>(interp_) / static_cast<double>(decim_);
}

void ResamplerBlock::reset() {
    if (impl_->state_i) src_reset(impl_->state_i);
    if (impl_->state_q) src_reset(impl_->state_q);
}

} // namespace archerfish::dsp
