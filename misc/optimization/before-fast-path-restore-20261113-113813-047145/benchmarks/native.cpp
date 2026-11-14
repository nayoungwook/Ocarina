#include "measure.h"

#include <cstdlib>

__declspec(noinline) int integer_loop(int count) {
    int value = 7, total = 0;
    for (int i = 0; i < count; ++i) {
        value = (value * 17 + 11) % 1000003;
        total = (total + value) % 1000003;
    }
    return total;
}

__declspec(noinline) int double_loop(int count) {
    double x = 0.25, y = 0.75;
    for (int i = 0; i < count; ++i) {
        x = x * 0.99991 + 0.00009;
        y = y * 0.99981 + x * 0.00019;
    }
    return static_cast<int>((x + y) * 1000000.0);
}

int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

struct Counter {
    int value = 7;
    int step(int delta) {
        value = (value * 17 + delta) % 1000003;
        return value;
    }
};

__declspec(noinline) int object_methods(int count) {
    Counter counter;
    int total = 0;
    for (int i = 0; i < count; ++i)
        total = (total + counter.step(11)) % 1000003;
    return total;
}

int main(int argc, char **argv) {
    try {
        if (argc != 3) throw std::runtime_error("Usage: native_bench workload size");
        const std::string workload = argv[1];
        volatile int size = std::stoi(argv[2]);
        if (workload == "integer_loop")
            measure("C++", workload, size, [&] { return integer_loop(size); });
        else if (workload == "double_loop")
            measure("C++", workload, size, [&] { return double_loop(size); });
        else if (workload == "recursive_fib")
            measure("C++", workload, size, [&] { return fib(size); });
        else if (workload == "object_methods")
            measure("C++", workload, size, [&] { return object_methods(size); });
        else if (workload == "baseline")
            measure("C++", workload, size, [&] { return static_cast<int>(size); });
        else throw std::runtime_error("Unknown workload");
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
