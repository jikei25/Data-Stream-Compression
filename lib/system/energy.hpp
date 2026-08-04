#ifndef SYSTEM_ENERGY_HPP
#define SYSTEM_ENERGY_HPP

#include <cstdlib>
#include <string>

// -----------------------------------------------------------------------------
// Model-based edge energy estimation for the comparison framework.
//
// The evaluation reports Edge Energy Consumed (mJ) as one of its four core
// metrics. On the physical edge node (e.g. an NVIDIA Jetson Nano) this is
// obtained through power-rail monitoring. To keep the framework device-agnostic
// and reproducible on any Linux host, energy is estimated with the linear model
//
//         E (mJ) = P_active (mW) * t_active (s)
//
// where t_active is the CPU-bound wall-clock time spent inside a phase
// (compression or decompression) - a quantity the framework already measures
// precisely via the Clock utility - and P_active is the device's active power
// draw while executing the workload.
//
// P_active is a device property rather than an algorithm property, so it is
// configured once per host through the EDGE_POWER_MW environment variable
// (milliwatts). When the variable is absent or invalid, DEFAULT_POWER_MW is
// used. Calibrate it to the target edge device to obtain absolute energy
// figures; for relative cross-algorithm comparison any consistent value works.
// -----------------------------------------------------------------------------
class EnergyModel {
    private:
        double power_mw;  // active power draw of the edge device (milliwatts)

    public:
        // Active power of an NVIDIA Jetson Nano core under load (~1.5 W).
        static constexpr double DEFAULT_POWER_MW = 1500.0;

        EnergyModel() {
            const char* env = std::getenv("EDGE_POWER_MW");
            this->power_mw = (env != nullptr) ? std::atof(env) : DEFAULT_POWER_MW;

            // Guard against unset / non-numeric / non-positive overrides.
            if (this->power_mw <= 0) {
                this->power_mw = DEFAULT_POWER_MW;
            }
        }

        explicit EnergyModel(double power_mw) {
            this->power_mw = (power_mw > 0) ? power_mw : DEFAULT_POWER_MW;
        }

        double getPowerMw() const {
            return this->power_mw;
        }

        // Convert an active (CPU-bound) duration in nanoseconds into the energy
        // consumed in millijoules: E(mJ) = P(mW) * t(s) = power_mw * (ns / 1e9).
        double energy_mJ(double time_ns) const {
            return this->power_mw * (time_ns / 1e9);
        }
};

#endif
