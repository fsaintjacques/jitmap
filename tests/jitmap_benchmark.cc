#include <benchmark/benchmark.h>
#include <bitset>

#include <jitmap/jitmap.h>

namespace jitmap {

using DenseBitmap = std::bitset<kBitsPerContainer>;

static void StaticIntersection2(benchmark::State& state) {
  DenseBitmap a, b;

  for (auto _ : state) {
    benchmark::DoNotOptimize(a & b);
  }

  state.SetBytesProcessed(((kBitsPerContainer / 8) * 2) * state.iterations());
}

static void StaticIntersection3(benchmark::State& state) {
  DenseBitmap a, b, c;

  for (auto _ : state) {
    benchmark::DoNotOptimize(a & b & c);
  }

  state.SetBytesProcessed(((kBitsPerContainer / 8) * 3) * state.iterations());
}

static void StaticIntersection4(benchmark::State& state) {
  DenseBitmap a, b, c, d;

  for (auto _ : state) {
    benchmark::DoNotOptimize(a & b & c & d);
  }

  state.SetBytesProcessed(((kBitsPerContainer / 8) * 4) * state.iterations());
}

BENCHMARK(StaticIntersection2);
BENCHMARK(StaticIntersection3);
BENCHMARK(StaticIntersection4);
}  // namespace jitmap
