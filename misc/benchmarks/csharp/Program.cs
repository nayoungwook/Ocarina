using System.Diagnostics;
using System.Text.Json;

static class Program {
    static int IntegerLoop(int count) {
        int value = 7, total = 0;
        for (int i = 0; i < count; ++i) {
            value = (value * 17 + 11) % 1000003;
            total = (total + value) % 1000003;
        }
        return total;
    }

    static int DoubleLoop(int count) {
        double x = 0.25, y = 0.75;
        for (int i = 0; i < count; ++i) {
            x = x * 0.99991 + 0.00009;
            y = y * 0.99981 + x * 0.00019;
        }
        return (int)((x + y) * 1000000.0);
    }

    static int Fib(int n) {
        if (n < 2) return n;
        return Fib(n - 1) + Fib(n - 2);
    }

    sealed class Counter {
        public int Value = 7;
        public int Step(int delta) {
            Value = (Value * 17 + delta) % 1000003;
            return Value;
        }
    }

    static int ObjectMethods(int count) {
        var counter = new Counter();
        int total = 0;
        for (int i = 0; i < count; ++i)
            total = (total + counter.Step(11)) % 1000003;
        return total;
    }

    static void Main(string[] args) {
        string workload = args[0];
        int size = int.Parse(args[1]);
        Func<int, int> fn = workload switch {
            "integer_loop" => IntegerLoop, "double_loop" => DoubleLoop,
            "recursive_fib" => Fib, "object_methods" => ObjectMethods,
            "baseline" => n => n, _ => throw new ArgumentException("Unknown workload")
        };
        int expected = fn(size);
        double Batch(int repetitions) {
            long start = Stopwatch.GetTimestamp();
            for (int i = 0; i < repetitions; ++i)
                if (fn(size) != expected) throw new Exception("Unstable checksum");
            return Stopwatch.GetElapsedTime(start).TotalMilliseconds;
        }
        for (int i = 0; i < 5; ++i) Batch(1);
        int repetitions = 1;
        while (Batch(repetitions) < 50 && repetitions < 1048576) repetitions *= 2;
        double[] samples = Enumerable.Range(0, 9).Select(_ => Batch(repetitions) / repetitions).ToArray();
        Console.WriteLine(JsonSerializer.Serialize(new {
            language = "C#", workload, size, checksum = expected,
            batch_repetitions = repetitions, warmup_calls = 5, samples_ms = samples
        }));
    }
}
