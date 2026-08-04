#ifndef SYSTEM_MONITOR_HPP
#define SYSTEM_MONITOR_HPP

#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <atomic>
#include <utility>
#include <optional>

#include "energy.hpp"

#if defined(__linux__)
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

using namespace std::chrono;

class Monitor {
    private:
        enum Phase {
            IDLE = 0,
            COMPRESSION = 1,
            DECOMPRESSION = 2
        };

        static constexpr int SAMPLE_INTERVAL_MS = 5;   // sampler pacing
        static constexpr int IDLE_PRIME_MS = 200;      // idle-baseline window

        bool flag;
        long page_size;
        std::thread task;
        std::atomic<int> phase;

        // Real hardware energy telemetry, integrated per phase by __monitor.
        HardwareEnergyMeter meter;
        double phase_energy_mj[3] = {0.0, 0.0, 0.0};   // indexed by Phase
        double phase_time_s[3] = {0.0, 0.0, 0.0};

        // Average idle power (W) measured from the idle-phase samples; used to
        // report the dynamic (above-idle) energy attributable to the workload,
        // mirroring the idle subtraction done on the Jetson in iot-streaming.
        double idlePowerW() {
            return this->phase_time_s[IDLE] > 0.0
                ? (this->phase_energy_mj[IDLE] / 1000.0) / this->phase_time_s[IDLE]
                : 0.0;
        }

        double phaseEnergy(int phase) {
            double idle_mj = this->idlePowerW() * 1000.0 * this->phase_time_s[phase];
            double dynamic_mj = this->phase_energy_mj[phase] - idle_mj;
            return dynamic_mj > 0.0 ? dynamic_mj : 0.0;
        }

        std::string phase_name() {
            switch (this->phase.load()) {
                case COMPRESSION:
                    return "compression";
                case DECOMPRESSION:
                    return "decompression";
                default:
                    return "idle";
            }
        }

        Monitor() {
            this->flag = false;
#if defined(__linux__)
            this->page_size = sysconf(_SC_PAGE_SIZE);
#else
            this->page_size = 0;
#endif
            this->phase = IDLE;
        }

        void __monitor(std::string output) {
            std::fstream file(output, std::ios::out);
            file << "user_cpu_time,system_cpu_time,vsz,rss,phase\n";

            HardwareEnergyMeter::Snapshot prev = this->meter.snapshot();

            while (this->flag) {
#if defined(__linux__)
                // dont know why but monitoring cpu time with ruuage seem more accurate
                rusage usage;
                getrusage(RUSAGE_SELF, &usage);

                std::string data;
                std::ifstream ifs("/proc/self/stat", std::ios_base::in);

                // ignore
                ifs >> data >> data >> data >> data >> data >> data >> data >> data >> data >> data >> data
                >> data >> data >> data >> data >> data >> data >> data >> data >> data >> data >> data;

                // get necessary data
                ifs >> data; std::string vsz = data;
                ifs >> data; std::string rss = std::to_string(std::stol(data)*this->page_size);
                std::string utime = std::to_string(usage.ru_utime.tv_sec*1000000 + usage.ru_utime.tv_usec);
                std::string stime = std::to_string(usage.ru_stime.tv_sec*1000000 + usage.ru_stime.tv_usec);

                file << utime << "," << stime << "," << vsz << "," << rss << "," << this->phase_name() << "\n";
#else
                // This executable is profiled on Linux. Keep the header
                // parseable by Windows IntelliSense without pretending to
                // provide Linux /proc metrics on another platform.
                file << "0,0,0,0," << this->phase_name() << "\n";
#endif
                // Pace the sampler, then integrate the hardware energy drawn over
                // this interval and attribute it to the phase that is active now.
                // A tight loop would itself dominate the power rail and waste a
                // full core; a few milliseconds keeps memory-peak detection fine
                // while keeping the running energy integral accurate.
                std::this_thread::sleep_for(std::chrono::milliseconds(SAMPLE_INTERVAL_MS));

                HardwareEnergyMeter::Snapshot curr = this->meter.snapshot();
                std::optional<double> delta = this->meter.deltaMilliJoules(prev, curr);
                if (delta.has_value()) {
                    double dt = std::chrono::duration<double>(curr.timestamp - prev.timestamp).count();
                    int active_phase = this->phase.load();
                    this->phase_energy_mj[active_phase] += *delta;
                    this->phase_time_s[active_phase] += dt;
                }
                prev = curr;
            }

            file.close();
        }
    
    public:
        static Monitor instance;

        static std::pair<long, long> getMemory() {
#if defined(__linux__)
            long page_size = sysconf(_SC_PAGE_SIZE);
            std::string data;
            std::ifstream ifs("/proc/self/stat", std::ios_base::in);

            ifs >> data >> data >> data >> data >> data >> data >> data >> data >> data >> data >> data
            >> data >> data >> data >> data >> data >> data >> data >> data >> data >> data >> data;

            ifs >> data; long vsz = std::stol(data);
            ifs >> data; long rss = std::stol(data) * page_size;

            return std::make_pair(vsz, rss);
#else
            return std::make_pair(0L, 0L);
#endif
        }

        void setIdle() {
            this->phase = IDLE;
        }

        void setCompression() {
            this->phase = COMPRESSION;
        }

        void setDecompression() {
            this->phase = DECOMPRESSION;
        }

        // Whether a real hardware energy source was found on this host.
        bool energyAvailable() {
            return this->meter.available();
        }

        std::string energySource() {
            return this->meter.description();
        }

        // Dynamic edge energy (mJ) drawn during each phase, read from hardware.
        double getCompressionEnergy() {
            return this->phaseEnergy(COMPRESSION);
        }

        double getDecompressionEnergy() {
            return this->phaseEnergy(DECOMPRESSION);
        }

        void start(std::string output) {
            this->setIdle();
            this->flag = true;
            this->task = std::thread(&Monitor::__monitor, this, output);
            // Let the sampler capture an idle hardware-power baseline before the
            // workload starts, so per-phase energy is reported above idle draw.
            std::this_thread::sleep_for(std::chrono::milliseconds(IDLE_PRIME_MS));
        }

        void stop() {
            this->flag = false;
            this->task.join();
        }

};

class Clock {
    private:
        bool flag = true;
        int _counter = 0;
        double _avg_duration = 0; 
        long _max_duration = -1;

        high_resolution_clock::time_point _start_pivot;
        high_resolution_clock::time_point _tick_pivot;
        
    public:
        void start() {
            this->_counter = 0;
            this->_avg_duration = 0;
            this->_max_duration = -1;
            this->_start_pivot = high_resolution_clock::now();
            this->_tick_pivot = this->_start_pivot;
        }

        void tick() {
            if (this->flag) {
                this->_tick_pivot = high_resolution_clock::now();
            }
            else {
                high_resolution_clock::time_point curr = high_resolution_clock::now();
                long duration = duration_cast<nanoseconds>(curr - this->_tick_pivot).count();

                this->_avg_duration = (this->_counter * this->_avg_duration + duration) / ((double) (this->_counter + 1));
                this->_max_duration = duration > this->_max_duration ? duration : this->_max_duration;

    
                this->_counter++;
                this->_tick_pivot = curr;
            }

            this->flag = !this->flag;
        }

        double getAvgDuration() {
            return this->_avg_duration;
        }

        long getMaxDuration() {
            return this->_max_duration;
        }

        // Total accumulated active time (ns) across every measured interval.
        double getTotalDuration() {
            return this->_avg_duration * this->_counter;
        }

        long stop() {
            return duration_cast<nanoseconds>(high_resolution_clock::now() - this->_start_pivot).count();
        }
};

#endif
