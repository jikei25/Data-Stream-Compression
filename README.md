# Evaluation Framework for Data Stream Compression

## Description

***Streaming data*** is one of the primary sources driving today’s big data challenges, due to the immense number of data-generating devices across diverse modern industries. Numerous studies have proposed compression techniques to address the overwhelming volume of such data, with the goal of improving storage efficiency and reducing bandwidth requirements for data transmission. However, such methods are often implemented in different languages and evaluated in disparate environments.

This repository provides a ``unified framework`` for evaluating the performance of compression algorithms in the context of ___univariate___ streaming time series. The framework primarily focuses on comparing lossy functional approximation techniques, due to the fact that they can operate efficiently across arbitrary data domains without requiring explicit training phases.

The framework takes input as a __JSON configuration__ and initialize the ``compressor`` and ``decompressor`` accordingly. The corresponding dataset is then loaded and fed to the ``compressor`` sequentially to better simulate a practical streaming environment. Whenever a segment is finalized, it is immediately transmitted to the ``decompressor`` to simulate data transmission from edge to server. Both the ``compressor`` and the ``decompressor`` are monitored by a __separate thread__, and the collected metrics are combined with their respective outputs to produce the final statistics.

<p align="center">
  <img src="framework.svg" width="400"><br>
  <em><b>Figure:</b> Overview of framework architecture.</em>
</p>

To ensure a fair evaluation, all algorithms are re-implemented in C++, which is commonly supported by edge devices. Streaming behavior is faithfully simulated by feeding data points to the compressor sequentially. The compressed outputs are serialized in binary format and immediately transmitted to the decompressor for simulating data transfer from sources (e.g., sensors) to sinks (e.g., centralized servers).

## Folder Structure

```text
.
├── bin/              # C++ object and executable files
├── conf/             # Configuration files for each algorithm
├── data/             # Evaluation Datasets
├── include/          # Header files defining [de]compressor and algorithms
├── lib/              # Supporting libraries
├── out/              # Statistical outputs in CSV format
│   ├── compress/     # Compression results
│   └── decompress/   # Decompression results
├── scripts/          # Shell scripts for compilation and execution
└── src/
    ├── C++/          # Algorithm implementations and the main entry point
    └── Python/       # Configuration validation and statistics generation
```

## Environment
- **OS**: `Linux`, preferably an `Ubuntu` based distribution.
- **C++**: The source code is written in `C++11` and compiled using `g++ 11.4.0`.
- **Python**: `numpy` library is required.
- **CPU**: At least ___two cores___ are required due to the use of multithreading for monitoring operations.

## Dataset
This repository includes only a representative dataset to comply with Git Large File Storage (LFS) limitations (saving my quota :p). However, most of the datasets used in the assessment are publicly available and can be easily obtained.
- UCR Time Series Archive: https://www.cs.ucr.edu/%7Eeamonn/time_series_data/
- Sensor Weather Traces: https://traces.cs.umass.edu/docs/traces/weather/
- Huge Stock Price: https://www.kaggle.com/datasets/arashnic/stock-data-intraday-minute-ba
- HPC-ODA: https://data.europa.eu/data/datasets/oai-zenodo-org-4671477?locale=sl
- Vehicle Energy Dataset: https://www.kaggle.com/datasets/ayanmaity/vehicle-energy-data
- Smart* Dataset: https://traces.cs.umass.edu/docs/traces/smartstar/
- Icentia11k: https://physionet.org/content/icentia11k-continuous-ecg/1.0/
- LENS dataset: https://github.com/clarkzjw/LENS

## Execution
### Compilation

First, compile all C++ source files by running the following command:

```bash
$ scripts/compile-all.sh
```

It is worth noting that the project relies heavily on header only libraries. As a result, the compilation process may take a quite amount of time. However, this is only required for the first time. For subsequent modifications, you can compile just the updated algorithm by running the following command to reduce compile time. 

```bash
$ scripts/compile.sh <ALGORITHM>
```

### Configuration
Before execution, create a configuration file corresponding to the algorithm which we wish to run. Configuration files follow the JSON format below:

```json
{
    "data": Original dataset. Only CSV datasets with two columns (time, value) are accepted,
    "compress": Compressed output under binary format,
    "decompress": Decompressed output (compared with the original dataset),
    "interval": Decompressed interval between two consecutive records,
    "algorithm": {
        "name": Algorithm name,
        "error": Maximum individual error threshold,
        "...": Other algorithm-specific configurations
    }
}
```

- Configuration templates are provided in ``conf/template/``.
- Valid algorithm-specific configurations beyond those predefined in the templates can be identified by inspecting the corresponding code in ``src/C++/*``.


### Execution
To execute an algorithm, run the following command with the corresponding configuration file.

```bash
$ script/run.sh <CONFIG_FILE>
```

### Statistical Output

Statistical output of each execution is appended to the ``experiments.csv`` file, including:

| Column | Description |
|:-------------|:-----------|
| ``dataset`` | Dataset used during execution. |
| ``algorithm`` | Algorithm used for compression and decompression. |
| ``error bound/buffer size`` | Maximum allowable individual error threshold in case of **lossy** algorithms. Size of buffer in each serialization in case of **lossless** algorithms. |
| ``Compression ratio`` | Ratio of original data size to compressed data size. |
| ``mse`` | Mean squared error of the reconstructed data. |
| ``rmse`` | Root mean squared error of the reconstructed data. |
| ``mae`` | Mean absolute error of the reconstructed data. |
| ``snr`` | Signal-to-noise ratio of the reconstructed data. |
| ``psnr`` | Peak signal-to-noise ratio of the reconstructed data. |
| ``max_e`` | Maximum error of the reconstructed data. |
| ``min_e`` | Minimum error of the reconstructed data. |
| ``correlation`` | Peason correlation of the reconstructed data. |
| ``ssim`` | Structural similarity index measure of the reconstructed data (default window = 1000). |
| ``max_vsz`` | Maximum virtual memory size (VSZ) used during execution (MB). |
| ``max_rss`` | Maximum resident set size (RSS) used during execution (MB). |
| ``c_time`` | Average compression time per data point (ns). |
| ``c_avg_latency`` | Average compression latency per data point (ns). |
| ``c_max_latency`` | Maximum compression latency observed (ns). |
| ``d_time`` | Average decompression time per data point (ns). |
| ``max_d_latency`` | Maximum decompression latency observed (ns). |
| ``c_energy`` | Edge energy consumed during compression (mJ), measured from hardware telemetry. |
| ``d_energy`` | Edge energy consumed during decompression (mJ), measured from hardware telemetry. |

### Edge Energy Measurement

Energy is read from real hardware telemetry; the framework does **not** accept a
configured wattage or use ``power × elapsed time`` as a model. The monitoring
thread samples the hardware continuously (every few milliseconds) and attributes
the energy drawn over each interval to the phase that is active at that moment
(compression / decompression / idle), then reports the **dynamic** energy — the
draw above the measured idle baseline — for each phase. This mirrors the
per-rail, idle-subtracted measurement used on the Jetson edge node in the
companion ``iot-streaming`` project.

Supported sources, in order of preference:

- **Linux RAPL package-energy counter** (x86), auto-detected at
  ``/sys/class/powercap/.../energy_uj`` (cumulative microjoules).
- **An instantaneous power rail** (e.g. Jetson INA3221), configured via one of
  the environment variables below and integrated over time.

The run prints the source it used, e.g. ``Energy meter: Linux RAPL package
energy counter``. If no source is available (Windows, and typically WSL2), it
prints ``unavailable`` and both energy fields are ``0`` — never a non-numeric
placeholder — so the ``experiments.csv`` columns stay aligned. Which host can
measure real energy:

| Host | Real energy? | How |
|:--|:--|:--|
| Jetson Nano | ✅ | INA3221 rail, set an ``ENERGY_*`` variable (below) |
| x86 Linux, bare-metal (Intel/AMD) | ✅ | RAPL, auto-detected (usually needs ``sudo``) |
| WSL2 / Windows | ❌ | no hardware source; energy = 0 |

#### How it works

The same background thread that samples memory (``lib/system/monitor.hpp``) also
drives the energy measurement, so there is one sampler for all metrics:

1. **Idle baseline.** When monitoring starts, the sampler first observes the
   hardware for a short window (~200 ms) while the device is idle, before the
   workload begins.
2. **Continuous sampling.** It then wakes every ~5 ms and takes a hardware
   snapshot. The energy drawn *since the previous snapshot* is either a
   wrap-corrected RAPL counter difference (µJ → mJ) or, for a power rail, the
   trapezoidal integral ``(P_prev + P_curr) / 2 × Δt``. No fixed wattage is
   assumed — every value comes from the device.
3. **Phase attribution.** The main loop tags the current activity with an atomic
   flag — ``compression`` while the compressor runs, ``decompression`` while the
   decompressor runs, ``idle`` otherwise. Each interval's energy and duration are
   added to the bucket of whichever phase is active at that sample.
4. **Idle subtraction.** From the idle bucket the sampler derives an average idle
   power ``P_idle = E_idle / t_idle``. The reported per-phase energy is the
   **dynamic** (above-idle) part:

   ```text
   c_energy = max(0,  E_compression   − P_idle × t_compression)
   d_energy = max(0,  E_decompression − P_idle × t_decompression)
   ```

   Subtracting idle removes the device's constant baseline (and the sampler's own
   overhead, since that is present in every phase), leaving the energy actually
   attributable to the algorithm — the same idle-subtracted definition used on
   the Jetson in ``iot-streaming``.

Because attribution is statistical (thousands of 5 ms samples over a run), it
needs no per-operation instrumentation and adds negligible, constant overhead
regardless of how many data points the stream contains.

#### Run on Jetson Nano (real measurement)

1. Locate the INA3221 rail in sysfs. Prefer the **total input** rail (labelled
   like ``POM_5V_IN`` / ``VDD_IN``), not a CPU/GPU sub-rail:

   ```bash
   # List hwmon rails and their labels to find the right index.
   $ grep -H . /sys/class/hwmon/hwmon*/in*_label /sys/class/hwmon/hwmon*/curr*_label 2>/dev/null
   ```

2. Run, pointing a variable at that rail. Use the power file if present,
   otherwise the voltage + current pair:

   ```bash
   # Rail that already reports power (microwatts):
   $ ENERGY_POWER_UW_PATH=/sys/class/hwmon/hwmon0/power1_input \
     script/run.sh conf/swing-filter.json

   # Rail that reports voltage (mV) and current (mA) separately:
   $ ENERGY_VOLTAGE_MV_PATH=/sys/class/hwmon/hwmon0/in1_input \
     ENERGY_CURRENT_MA_PATH=/sys/class/hwmon/hwmon0/curr1_input \
     script/run.sh conf/swing-filter.json
   ```

#### Run on x86 Linux (real measurement)

Nothing to configure — RAPL is auto-detected; just grant access:

```bash
$ sudo script/run.sh conf/swing-filter.json
```

#### Test without hardware (simulated rail)

To verify the pipeline and the ``c_energy`` / ``d_energy`` columns on a machine
with no energy source (e.g. WSL2), point ``ENERGY_POWER_UW_PATH`` at a plain
file that holds a power value in microwatts. The meter reads it exactly like a
sensor. A background writer can vary it so the dynamic energy comes out non-zero:

```bash
$ echo 1000000 > /tmp/fake_power_uw            # 1 W idle baseline
$ ( sleep 1; echo 5000000 > /tmp/fake_power_uw # 5 W during the run
    sleep 3; echo 1000000 > /tmp/fake_power_uw ) &
$ ENERGY_POWER_UW_PATH=/tmp/fake_power_uw script/run.sh conf/swing-filter.json
```

This exercises the full sampling → integration → idle-subtraction path; it is a
plumbing check, not a real energy figure.


## Algorithms
All algorithms from prior work that we have re-implemented are listed below. For the sake of consistency, our naming conventions might look different from those used by the original authors, since some algorithms were not explicitly named in the original papers.

### Piecewise constant approximation:
This family partitions a data stream into multiple segments, with each represented by a **constant value**.
* `pmc` : Capturing Sensor-generated Time Series With Quality Guarantees. (Link: https://ieeexplore.ieee.org/document/1260811)
* `hybrid-pca` : Improved Piecewise Constant Approximation Method for Compressing Data Streams (Link: https://ieeexplore.ieee.org/document/8934460).

### Piecewise linear approximation:
This family partitions a data stream into multiple segments, with each represented by a **linear line**.
* `swab` : An Online Algorithm for Segmenting Time Series. (Link: https://ieeexplore.ieee.org/document/989531)
* `swing-filter` : Online Piece-wise Linear Approximation of Numerical Streams with Precision Guarantees. (Link: https://dl.acm.org/doi/abs/10.14778/1687627.1687645)
* `slide-filter` : Online Piece-wise Linear Approximation of Numerical Streams with Precision Guarantees. (Link: https://dl.acm.org/doi/abs/10.14778/1687627.1687645) 
* `optimal-pla` : Maximum error-bounded Piecewise Linear Representation for Online Stream Approximation. (Link: https://dl.acm.org/doi/10.1007/s00778-014-0355-0)
* `cov-pla` : Streaming Piecewise Linear Approximation for Efficient Data Management in Edge Computing. (Link: https://dl.acm.org/doi/10.1145/3297280.3297552)
* `conn-I-pla` : An Improved Algorithm for Segmenting Online Time Series with Error Bound Guarantee. (Link: https://link.springer.com/article/10.1007/s13042-014-0310-9)
* `semi-optimal-pla` : An Optimal Online Semi-Connected PLA Algorithm With Maximum Error Bound. (Link: https://ieeexplore.ieee.org/document/9039677)
* `semi-mixed-pla` : An Online PLA Algorithm with Maximum Error Bound for Generating Optimal Mixed‐segments. (Link: https://link.springer.com/article/10.1007/s13042-019-01052-y)
* `mix-piece` : Flexible Grouping of Linear Segments for Highly Accurate Lossy Compression of Time Series Data. (Link: https://link.springer.com/article/10.1007/s00778-024-00862-z)
* `ioriented-pla` : Ours.

### Piecewise polynomial approximation:
This family partitions a data stream into multiple segments, with each represented by a **polynomial of degree k**, where k is a predefined hyperparameter.
* `poly-swab` : Implementation of SWAB for polynomial of arbitrary degree without runtime optimization as in linear SWAB (since convex hull search space is only correct when applied to linear models).
* `cached-normal-equation` : Fast Piecewise Polynomial Fitting of Time-Series Data for Streaming Computing. (Link: https://ieeexplore.ieee.org/document/9016024)

### Model selection:
This family partitions a data stream into multiple segments, each represented by **the most suitable model**, which is selected from a set of candidate functions.
* `adaptive-approximation` : An Adaptive Algorithm for Online Time Series Segmentation with Error Bound Guarantee. (Link: https://dl.acm.org/doi/10.1145/2247596.2247620)
* `smart-grid-compression` : A Time-series Compression Technique and Its Application to The Smart Grid. (Link: https://link.springer.com/article/10.1007/s00778-014-0368-8)
* `adapt-ppa` : Ours.

### Floating point compression:
This family compresses each individual data point by reducing the number of bits used to represent floating-point values. The algorithms are nearly 1-to-1 mapping from the authors source code to our framework. 
* `gorilla` : Gorilla: A Fast, Scalable, In-Memory Time Series Database. (Link: https://dl.acm.org/doi/10.14778/2824032.2824078)
* `chimp` : Chimp: Efficient Lossless Floating Point Compression for Time Series Databases. (Link: https://dl.acm.org/doi/10.14778/3551793.3551852)
* `elf+` : Elf: Erasing-Based Lossless Floating-Point Compression. (Link: https://dl.acm.org/doi/10.14778/3587136.3587149) 
* `serf` : Serf: Streaming Error-Bounded Floating-Point Compression. (Link: https://dl.acm.org/doi/10.1145/3725353)
* `camel`: Camel: Eﬀicient Compression of Floating-Point Time Series. (Link: https://dl.acm.org/doi/abs/10.1145/3698802)
* `self`: Adaptive Encoding Strategies for Lossless Floating-Point Compression. (Link: https://ieeexplore.ieee.org/abstract/document/10949160)

## Contact

For questions, issues, or further information, please contact:

- **Huan** — huan@hcmut.edu.vn
