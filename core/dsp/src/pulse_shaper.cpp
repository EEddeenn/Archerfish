#include "archerfish/dsp/pulse_shaper.hpp"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include "archerfish/common/constants.hpp"

namespace {

} // namespace

namespace archerfish::dsp {

std::vector<float> RrcFilterDesign::design() const {
    if (!std::isfinite(alpha) || alpha <= 0.0 || alpha > 1.0) {
        throw std::invalid_argument("RrcFilterDesign: alpha must be finite and in (0, 1]");
    }
    if (span_symbols == 0 || samples_per_symbol == 0) {
        throw std::invalid_argument("RrcFilterDesign: span_symbols and samples_per_symbol must be nonzero");
    }
    if (span_symbols > (std::numeric_limits<size_t>::max() - 1) / samples_per_symbol) {
        throw std::overflow_error("RrcFilterDesign: tap count overflow");
    }
    size_t ntaps = span_symbols * samples_per_symbol + 1;
    std::vector<float> taps(ntaps);

    for (size_t i = 0; i < ntaps; ++i) {
        double t_norm = (static_cast<double>(i) - static_cast<double>(ntaps - 1) / 2.0) / static_cast<double>(samples_per_symbol);

        if (std::abs(t_norm) < 1e-10) {
            taps[i] = static_cast<float>(1.0 + alpha * (4.0 / archerfish::constants::kPi - 1.0));
        } else if (std::abs(std::abs(t_norm) - 1.0 / (4.0 * alpha)) < 1e-10) {
            double s = alpha / std::sqrt(2.0);
            taps[i] = static_cast<float>(s * ((1.0 + 2.0 / archerfish::constants::kPi) * std::sin(archerfish::constants::kPi / (4.0 * alpha)) +
                                               (1.0 - 2.0 / archerfish::constants::kPi) * std::cos(archerfish::constants::kPi / (4.0 * alpha))));
        } else {
            double num = std::cos((1.0 + alpha) * archerfish::constants::kPi * t_norm) +
                         std::sin((1.0 - alpha) * archerfish::constants::kPi * t_norm) / (4.0 * alpha * t_norm);
            double denom = 1.0 - (16.0 * alpha * alpha * t_norm * t_norm);
            taps[i] = static_cast<float>(num / denom);
        }
    }

    double energy = 0.0;
    for (auto t : taps)
        energy += static_cast<double>(t) * static_cast<double>(t);
    if (energy > 0.0) {
        float norm = static_cast<float>(1.0 / std::sqrt(energy));
        for (auto& t : taps)
            t *= norm;
    }

    return taps;
}

size_t RrcFilterDesign::num_taps() const {
    if (span_symbols == 0 || samples_per_symbol == 0) {
        throw std::invalid_argument("RrcFilterDesign: span_symbols and samples_per_symbol must be nonzero");
    }
    if (span_symbols > (std::numeric_limits<size_t>::max() - 1) / samples_per_symbol) {
        throw std::overflow_error("RrcFilterDesign: tap count overflow");
    }
    return span_symbols * samples_per_symbol + 1;
}

} // namespace archerfish::dsp
