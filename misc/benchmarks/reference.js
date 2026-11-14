'use strict';
const {performance} = require('node:perf_hooks');

function integer_loop(count) {
  let value = 7, total = 0;
  for (let i = 0; i < count; ++i) {
    value = (value * 17 + 11) % 1000003;
    total = (total + value) % 1000003;
  }
  return total;
}

function double_loop(count) {
  let x = 0.25, y = 0.75;
  for (let i = 0; i < count; ++i) {
    x = x * 0.99991 + 0.00009;
    y = y * 0.99981 + x * 0.00019;
  }
  return Math.trunc((x + y) * 1000000.0);
}

function recursive_fib(n) {
  if (n < 2) return n;
  return recursive_fib(n - 1) + recursive_fib(n - 2);
}

class Counter {
  constructor() { this.value = 7; }
  step(delta) {
    this.value = (this.value * 17 + delta) % 1000003;
    return this.value;
  }
}

function object_methods(count) {
  const counter = new Counter();
  let total = 0;
  for (let i = 0; i < count; ++i)
    total = (total + counter.step(11)) % 1000003;
  return total;
}

function baseline(size) { return size; }

const functions = {integer_loop, double_loop, recursive_fib, object_methods, baseline};
const workload = process.argv[2], size = Number(process.argv[3]);
const fn = functions[workload], expected = fn(size);
let sink = 0;
function batch(repetitions) {
  const start = performance.now();
  for (let i = 0; i < repetitions; ++i) {
    sink = fn(size);
    if (sink !== expected) throw new Error('Unstable checksum');
  }
  return performance.now() - start;
}
for (let i = 0; i < 5; ++i) batch(1);
let repetitions = 1;
while (batch(repetitions) < 50 && repetitions < 1048576) repetitions *= 2;
const samples = Array.from({length: 9}, () => batch(repetitions) / repetitions);
console.log(JSON.stringify({language: 'JavaScript', workload, size, checksum: sink,
  batch_repetitions: repetitions, warmup_calls: 5, samples_ms: samples}));
