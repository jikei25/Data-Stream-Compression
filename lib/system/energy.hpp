#ifndef SYSTEM_ENERGY_HPP
#define SYSTEM_ENERGY_HPP

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

// Reads energy from hardware exposed by Linux rather than estimating it from a
// configured wattage. The preferred backend is the RAPL energy counter exposed
// by the kernel powercap interface. It reports cumulative microjoules, so an
// interval's energy is simply the difference between two counter snapshots.
//
// Jetson boards generally expose instantaneous rail telemetry instead of a
// cumulative energy counter. For those boards, configure one of:
//
//   ENERGY_POWER_UW_PATH   path to a sysfs power sensor in microwatts
//   ENERGY_VOLTAGE_MV_PATH path to a sysfs voltage sensor in millivolts, and
//   ENERGY_CURRENT_MA_PATH path to its matching current sensor in milliamps
//
// The latter two are read from the hardware at both interval boundaries and
// integrated with the trapezoidal rule. No fixed power value is accepted.
class HardwareEnergyMeter {
    public:
        enum class Backend {
            Unavailable,
            Counter,
            PowerSensor,
            VoltageCurrentSensor
        };

        struct Snapshot {
            Backend backend = Backend::Unavailable;
            std::vector<std::uint64_t> energy_uj;
            double power_w = 0.0;
            std::chrono::steady_clock::time_point timestamp;
            bool valid = false;
        };

    private:
        struct Counter {
            std::filesystem::path energy_path;
            std::uint64_t maximum_uj;
        };

        Backend backend = Backend::Unavailable;
        std::vector<Counter> counters;
        std::filesystem::path power_uw_path;
        std::filesystem::path voltage_mv_path;
        std::filesystem::path current_ma_path;
        std::string meter_description = "unavailable (no supported hardware energy telemetry found)";

        static std::optional<std::uint64_t> readUnsigned(const std::filesystem::path& path) {
            std::ifstream input(path);
            std::uint64_t value = 0;
            if (!(input >> value)) {
                return std::nullopt;
            }
            return value;
        }

        static std::optional<double> readDouble(const std::filesystem::path& path) {
            std::ifstream input(path);
            double value = 0.0;
            if (!(input >> value) || value < 0.0) {
                return std::nullopt;
            }
            return value;
        }

        static bool isRaplPackageZone(const std::filesystem::path& path) {
            const std::string zone = path.filename().string();
            const bool is_rapl = zone.rfind("intel-rapl:", 0) == 0 ||
                                 zone.rfind("amd-rapl:", 0) == 0;
            if (!is_rapl) {
                return false;
            }

            const std::size_t first_colon = zone.find(':');
            return zone.find(':', first_colon + 1) == std::string::npos;
        }

        bool addCounter(const std::filesystem::path& energy_path) {
            const auto energy = readUnsigned(energy_path);
            if (!energy.has_value()) {
                return false;
            }

            const auto maximum = readUnsigned(energy_path.parent_path() / "max_energy_range_uj");
            counters.push_back({energy_path, maximum.value_or(0)});
            return true;
        }

        void discoverRaplCounters() {
            const std::filesystem::path powercap_root("/sys/class/powercap");
            std::error_code error;
            if (!std::filesystem::exists(powercap_root, error)) {
                return;
            }

            std::filesystem::recursive_directory_iterator iterator(
                powercap_root,
                std::filesystem::directory_options::skip_permission_denied,
                error
            );
            const std::filesystem::recursive_directory_iterator end;
            while (!error && iterator != end) {
                const std::filesystem::path path = iterator->path();
                if (path.filename() == "energy_uj" && isRaplPackageZone(path.parent_path())) {
                    addCounter(path);
                }
                iterator.increment(error);
            }

            if (!counters.empty()) {
                backend = Backend::Counter;
                meter_description = "Linux RAPL package energy counter";
                if (counters.size() > 1) {
                    meter_description += "s (" + std::to_string(counters.size()) + " packages)";
                }
            }
        }

        void discoverConfiguredSensor() {
            const char* power_path = std::getenv("ENERGY_POWER_UW_PATH");
            if (power_path != nullptr && readDouble(power_path).has_value()) {
                backend = Backend::PowerSensor;
                power_uw_path = power_path;
                meter_description = "Linux hardware power sensor (microwatts)";
                return;
            }

            const char* voltage_path = std::getenv("ENERGY_VOLTAGE_MV_PATH");
            const char* current_path = std::getenv("ENERGY_CURRENT_MA_PATH");
            if (voltage_path != nullptr && current_path != nullptr &&
                readDouble(voltage_path).has_value() && readDouble(current_path).has_value()) {
                backend = Backend::VoltageCurrentSensor;
                voltage_mv_path = voltage_path;
                current_ma_path = current_path;
                meter_description = "Linux hardware voltage/current power rail";
            }
        }

        std::optional<double> readPowerWatts() const {
            if (backend == Backend::PowerSensor) {
                const auto power_uw = readDouble(power_uw_path);
                return power_uw.has_value()
                    ? std::optional<double>(*power_uw / 1'000'000.0)
                    : std::nullopt;
            }

            if (backend == Backend::VoltageCurrentSensor) {
                const auto voltage_mv = readDouble(voltage_mv_path);
                const auto current_ma = readDouble(current_ma_path);
                if (!voltage_mv.has_value() || !current_ma.has_value()) {
                    return std::nullopt;
                }
                return (*voltage_mv * *current_ma) / 1'000'000.0;
            }

            return std::nullopt;
        }

    public:
        HardwareEnergyMeter() {
            const char* counter_path = std::getenv("ENERGY_UJ_PATH");
            if (counter_path != nullptr && addCounter(counter_path)) {
                backend = Backend::Counter;
                meter_description = "configured Linux hardware energy counter";
                return;
            }

            discoverRaplCounters();
            if (backend == Backend::Unavailable) {
                discoverConfiguredSensor();
            }
        }

        bool available() const {
            return backend != Backend::Unavailable;
        }

        const std::string& description() const {
            return meter_description;
        }

        Snapshot snapshot() const {
            Snapshot result;
            result.backend = backend;
            result.timestamp = std::chrono::steady_clock::now();

            if (backend == Backend::Counter) {
                result.energy_uj.reserve(counters.size());
                for (const Counter& counter : counters) {
                    const auto energy = readUnsigned(counter.energy_path);
                    if (!energy.has_value()) {
                        return result;
                    }
                    result.energy_uj.push_back(*energy);
                }
                result.valid = true;
                return result;
            }

            const auto power_w = readPowerWatts();
            if (power_w.has_value()) {
                result.power_w = *power_w;
                result.valid = true;
            }
            return result;
        }

        // Energy (mJ) drawn between two snapshots taken from this meter. For a
        // cumulative counter it is the wrap-corrected difference; for an
        // instantaneous power source it is the trapezoidal integral over the
        // interval. Feeding a continuous stream of adjacent snapshots (e.g. from
        // the monitor thread) turns this into an accurate running integral.
        std::optional<double> deltaMilliJoules(const Snapshot& start, const Snapshot& end) const {
            if (!start.valid || !end.valid || start.backend != backend || end.backend != backend) {
                return std::nullopt;
            }

            if (backend == Backend::Counter) {
                if (start.energy_uj.size() != counters.size() || end.energy_uj.size() != counters.size()) {
                    return std::nullopt;
                }

                std::uint64_t delta_uj = 0;
                for (std::size_t index = 0; index < counters.size(); ++index) {
                    if (end.energy_uj[index] >= start.energy_uj[index]) {
                        delta_uj += end.energy_uj[index] - start.energy_uj[index];
                    }
                    else if (counters[index].maximum_uj > start.energy_uj[index]) {
                        delta_uj += counters[index].maximum_uj - start.energy_uj[index] + end.energy_uj[index];
                    }
                    else {
                        return std::nullopt;
                    }
                }
                return static_cast<double>(delta_uj) / 1000.0;
            }

            const double duration_seconds = std::chrono::duration<double>(end.timestamp - start.timestamp).count();
            if (duration_seconds < 0.0) {
                return std::nullopt;
            }
            return ((start.power_w + end.power_w) / 2.0) * duration_seconds * 1000.0;
        }

        std::optional<double> energyMilliJoulesSince(const Snapshot& start) const {
            return deltaMilliJoules(start, snapshot());
        }

        static std::string formatMilliJoules(const std::optional<double>& value) {
            return value.has_value() ? std::to_string(*value) : "N/A";
        }
};

#endif
