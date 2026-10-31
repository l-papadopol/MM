// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <complex>
#include <future>
#include <memory>
#include <vector>

namespace FtSyncCompute {
struct Input {
    bool toneEnergies = false; // ordinary FT8/FT4 Goertzel demodulation, real samples
    std::vector<std::complex<double>> samples;
    std::vector<int> starts; // [timing][sync symbol]
    std::vector<int> tones;
    int frequencies = 0, symbolSamples = 0;
    double lowHz = 0, stepHz = 0, spacingHz = 0, mixHz = 0, rate = 1000;
    int timings() const;
    size_t cells() const;
    bool valid() const;
};
// Same double-precision arithmetic and score normalization as the native DSP.
// Different timings write disjoint rows, so the caller may parallelize this.
void cpuTiming(const Input &in, int timing, double *out);
struct Result {
    bool ok = false, gpuDevice = false;
    std::vector<double> scores;
    double totalMs = 0; // host packing, queue, transfers, kernel and readback
    QString device, reason;
};
struct Status { bool ready = false, unavailable = false, gpuDevice = false; QString device, reason; };
class Engine {
public:
    // CPU OpenCL devices are for the standalone validation probe ONLY.
    explicit Engine(bool allowCpuForTest = false);
    ~Engine();
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    static Engine &instance();
    Status status() const;
    // One immutable request in flight; never queues behind an existing job.
    std::shared_future<Result> submit(std::shared_ptr<const Input> input);
private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
bool equivalent(const std::vector<double> &cpu, const std::vector<double> &device,
                double *maxError = nullptr);
}
