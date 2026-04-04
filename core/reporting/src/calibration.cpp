#include "archerfish/reporting/calibration.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

namespace archerfish::reporting {

namespace {

nlohmann::json point_to_json(const CalibrationPoint& p) {
    return {
        {"freq_hz", p.freq_hz},
        {"gain_db", p.gain_db},
        {"measured_power_dbm", p.measured_power_dbm},
        {"expected_power_dbm", p.expected_power_dbm},
        {"error_db", p.error_db},
    };
}

CalibrationPoint point_from_json(const nlohmann::json& j) {
    CalibrationPoint p;
    p.freq_hz = j.at("freq_hz").get<double>();
    p.gain_db = j.at("gain_db").get<double>();
    p.measured_power_dbm = j.at("measured_power_dbm").get<double>();
    p.expected_power_dbm = j.at("expected_power_dbm").get<double>();
    p.error_db = j.at("error_db").get<double>();
    return p;
}

} // namespace

std::string CalibrationData::to_json() const {
    nlohmann::json j;
    j["device_id"] = device_id;
    j["channel"] = channel;
    j["timestamp"] = timestamp;
    auto arr = nlohmann::json::array();
    for (const auto& e : entries) {
        arr.push_back(point_to_json(e));
    }
    j["entries"] = arr;
    return j.dump(2);
}

std::expected<CalibrationData, std::string> CalibrationData::from_json(const std::string& json_str) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(json_str);
    } catch (const nlohmann::json::parse_error& e) {
        return std::unexpected(std::string("Invalid JSON: ") + e.what());
    }

    if (!j.contains("device_id") || !j["device_id"].is_string())
        return std::unexpected("Missing or invalid 'device_id'");
    if (!j.contains("channel") || !j["channel"].is_number_unsigned())
        return std::unexpected("Missing or invalid 'channel'");
    if (!j.contains("timestamp") || !j["timestamp"].is_string())
        return std::unexpected("Missing or invalid 'timestamp'");
    if (!j.contains("entries") || !j["entries"].is_array())
        return std::unexpected("Missing or invalid 'entries'");

    CalibrationData data;
    data.device_id = j["device_id"].get<std::string>();
    data.channel = j["channel"].get<uint32_t>();
    data.timestamp = j["timestamp"].get<std::string>();

    for (const auto& item : j["entries"]) {
        if (!item.contains("freq_hz") || !item.contains("gain_db") ||
            !item.contains("measured_power_dbm") || !item.contains("expected_power_dbm") ||
            !item.contains("error_db")) {
            return std::unexpected("CalibrationPoint missing required fields");
        }
        data.entries.push_back(point_from_json(item));
    }

    return data;
}

std::filesystem::path CalibrationData::calibration_dir() {
    return std::filesystem::path(getenv("HOME") ? getenv("HOME") : "/tmp") /
           ".config" / "archerfish" / "calibration";
}

std::filesystem::path CalibrationData::calibration_file(const std::string& dev_id, uint32_t ch) {
    return calibration_dir() / fmt::format("{}_ch{}.json", dev_id, ch);
}

std::optional<InterpolationResult> CalibrationData::interpolate_power(double freq_hz, double gain_db) const {
    if (entries.empty()) return std::nullopt;

    if (entries.size() == 1) {
        InterpolationResult r;
        r.estimated_power_dbm = entries[0].measured_power_dbm;
        r.method = "nearest_neighbor";
        return r;
    }

    double min_dist = std::numeric_limits<double>::max();
    size_t nearest_idx = 0;
    for (size_t i = 0; i < entries.size(); ++i) {
        double df = (entries[i].freq_hz - freq_hz) / 1e9;
        double dg = entries[i].gain_db - gain_db;
        double dist = df * df + dg * dg;
        if (dist < min_dist) {
            min_dist = dist;
            nearest_idx = i;
        }
    }

    InterpolationResult result;
    result.method = "nearest_neighbor";
    result.estimated_power_dbm = entries[nearest_idx].measured_power_dbm;

    if (entries.size() >= 4) {
        const CalibrationPoint* q00 = nullptr;
        const CalibrationPoint* q01 = nullptr;
        const CalibrationPoint* q10 = nullptr;
        const CalibrationPoint* q11 = nullptr;
        double best_q00 = std::numeric_limits<double>::max();
        double best_q01 = std::numeric_limits<double>::max();
        double best_q10 = std::numeric_limits<double>::max();
        double best_q11 = std::numeric_limits<double>::max();

        for (const auto& e : entries) {
            if (e.freq_hz <= freq_hz && e.gain_db <= gain_db) {
                double df = (freq_hz - e.freq_hz) / 1e9;
                double dg = gain_db - e.gain_db;
                double dist = df * df + dg * dg;
                if (dist < best_q00) { best_q00 = dist; q00 = &e; }
            }
            if (e.freq_hz <= freq_hz && e.gain_db >= gain_db) {
                double df = (freq_hz - e.freq_hz) / 1e9;
                double dg = e.gain_db - gain_db;
                double dist = df * df + dg * dg;
                if (dist < best_q01) { best_q01 = dist; q01 = &e; }
            }
            if (e.freq_hz >= freq_hz && e.gain_db <= gain_db) {
                double df = (e.freq_hz - freq_hz) / 1e9;
                double dg = gain_db - e.gain_db;
                double dist = df * df + dg * dg;
                if (dist < best_q10) { best_q10 = dist; q10 = &e; }
            }
            if (e.freq_hz >= freq_hz && e.gain_db >= gain_db) {
                double df = (e.freq_hz - freq_hz) / 1e9;
                double dg = e.gain_db - gain_db;
                double dist = df * df + dg * dg;
                if (dist < best_q11) { best_q11 = dist; q11 = &e; }
            }
        }

        if (q00 && q01 && q10 && q11) {
            double f0 = q00->freq_hz, f1 = q10->freq_hz;
            double g0 = q00->gain_db, g1 = q01->gain_db;

            if (f1 > f0 && g1 > g0) {
                double tx = (freq_hz - f0) / (f1 - f0);
                double ty = (gain_db - g0) / (g1 - g0);
                tx = std::clamp(tx, 0.0, 1.0);
                ty = std::clamp(ty, 0.0, 1.0);

                double p = (1 - tx) * (1 - ty) * q00->measured_power_dbm +
                           tx * (1 - ty) * q10->measured_power_dbm +
                           (1 - tx) * ty * q01->measured_power_dbm +
                           tx * ty * q11->measured_power_dbm;
                result.estimated_power_dbm = p;
                result.method = "bilinear";
            } else {
                // Fallback: inverse distance weighting from the four nearest quadrant points
                double total_weight = 0.0;
                double weighted_power = 0.0;
                auto add_idw = [&](const CalibrationPoint* p) {
                    if (!p) return;
                    double df = (p->freq_hz - freq_hz) / 1e9;
                    double dg = p->gain_db - gain_db;
                    double dist_sq = df * df + dg * dg;
                    double w = (dist_sq > 0.0) ? 1.0 / dist_sq : 1e12;
                    total_weight += w;
                    weighted_power += w * p->measured_power_dbm;
                };
                add_idw(q00); add_idw(q01); add_idw(q10); add_idw(q11);
                if (total_weight > 0.0) {
                    result.estimated_power_dbm = weighted_power / total_weight;
                    result.method = "idw_fallback";
                }
            }
        }
    }

    return result;
}

double CalibrationData::compute_required_amplitude(double target_power_dbm, double freq_hz, double gain_db) const {
    auto interp = interpolate_power(freq_hz, gain_db);
    if (!interp.has_value()) return -1.0;

    double current_power = interp->estimated_power_dbm;
    double power_diff_db = target_power_dbm - current_power;
    double amplitude_ratio = std::pow(10.0, power_diff_db / 20.0);
    return std::min(amplitude_ratio, 1.0);
}

} // namespace archerfish::reporting
