#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using BenchmarkClock = std::chrono::steady_clock;
inline volatile int benchmark_sink = 0;

template<class Function>
void measure(const std::string &language, const std::string &workload, int size,
    Function function, int samples = 9, double minimum_batch_ms = 50.0) {
    const int expected = function();
    const auto invoke = [&] {
        const int result = function();
        benchmark_sink = result;
        if (result != expected) throw std::runtime_error("Unstable checksum");
    };
    for (int i = 0; i < 5; ++i) invoke();
    const auto batch = [&](int count) {
        const auto start = BenchmarkClock::now();
        for (int i = 0; i < count; ++i) invoke();
        return std::chrono::duration<double, std::milli>(BenchmarkClock::now() - start).count();
    };
    int repetitions = 1;
    while (batch(repetitions) < minimum_batch_ms && repetitions < 1048576)
        repetitions *= 2;
    std::vector<double> times;
    for (int i = 0; i < samples; ++i) times.push_back(batch(repetitions) / repetitions);
    std::cout << std::setprecision(12)
        << "{\"language\":\"" << language << "\",\"workload\":\"" << workload
        << "\",\"size\":" << size << ",\"checksum\":" << expected
        << ",\"batch_repetitions\":" << repetitions << ",\"warmup_calls\":5,\"samples_ms\":[";
    for (std::size_t i = 0; i < times.size(); ++i) {
        if (i) std::cout << ',';
        std::cout << times[i];
    }
    std::cout << "]}" << std::endl;
}
