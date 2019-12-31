// Copyright 2020 RStudio, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

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
