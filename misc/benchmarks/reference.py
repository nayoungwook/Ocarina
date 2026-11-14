import json
import sys
import time


def integer_loop(count):
    value, total = 7, 0
    for _ in range(count):
        value = (value * 17 + 11) % 1000003
        total = (total + value) % 1000003
    return total


def double_loop(count):
    x, y = 0.25, 0.75
    for _ in range(count):
        x = x * 0.99991 + 0.00009
        y = y * 0.99981 + x * 0.00019
    return int((x + y) * 1000000.0)


def recursive_fib(n):
    if n < 2:
        return n
    return recursive_fib(n - 1) + recursive_fib(n - 2)


class Counter:
    def __init__(self):
        self.value = 7

    def step(self, delta):
        self.value = (self.value * 17 + delta) % 1000003
        return self.value


def object_methods(count):
    counter, total = Counter(), 0
    for _ in range(count):
        total = (total + counter.step(11)) % 1000003
    return total


def baseline(size):
    return size


if __name__ == "__main__":
    workload, size = sys.argv[1], int(sys.argv[2])
    function = globals()[workload]
    expected = function(size)

    def batch(repetitions):
        start = time.perf_counter_ns()
        for _ in range(repetitions):
            if function(size) != expected:
                raise RuntimeError("Unstable checksum")
        return (time.perf_counter_ns() - start) / 1e6

    for _ in range(5):
        batch(1)
    repetitions = 1
    while batch(repetitions) < 50 and repetitions < 1048576:
        repetitions *= 2
    samples = [batch(repetitions) / repetitions for _ in range(9)]
    print(json.dumps(dict(language="Python", workload=workload, size=size,
                         checksum=expected, batch_repetitions=repetitions,
                         warmup_calls=5, samples_ms=samples)))
