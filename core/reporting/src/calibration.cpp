#include "archerfish/reporting/calibration.hpp"

#include <fstream>

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

} // namespace archerfish::reporting
