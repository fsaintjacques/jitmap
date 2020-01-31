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
#include <vector>

#include <jitmap/jitmap.h>
#include <jitmap/query/compiler.h>
#include <jitmap/query/query.h>
#include <jitmap/size.h>

namespace jitmap {

using DenseBitmap = std::bitset<kBitsPerContainer>;

static void StaticIntersection2(benchmark::State& state) {
  DenseBitmap a, b;

  for (auto _ : state) {
    benchmark::DoNotOptimize(a & b);
  }

  state.SetBytesProcessed(kBytesPerContainer * 2 * state.iterations());
}

static void JitIntersection2(benchmark::State& state) {
  std::array<BitsetWordType, kWordsPerContainers> a, b, output;
  std::vector<const BitsetWordType*> inputs{a.data(), b.data()};

  query::ExecutionContext engine{query::JitEngine::Make()};
  auto query = query::Query::Make("benchmark_query_2", "a & b", &engine);

  for (auto _ : state) {
    query->Eval(inputs.data(), output.data());
  }

  state.SetBytesProcessed(kBytesPerContainer * 2 * state.iterations());
}

static void StaticIntersection3(benchmark::State& state) {
  DenseBitmap a, b, c;

  for (auto _ : state) {
    benchmark::DoNotOptimize(a & b & c);
  }

  state.SetBytesProcessed(kBytesPerContainer * 3 * state.iterations());
}

static void JitIntersection3(benchmark::State& state) {
  std::array<BitsetWordType, kWordsPerContainers> a, b, c, output;
  std::vector<const BitsetWordType*> inputs{a.data(), b.data(), c.data()};

  query::ExecutionContext engine{query::JitEngine::Make()};
  auto query = query::Query::Make("benchmark_query_3", "a & b & c", &engine);

  for (auto _ : state) {
    query->Eval(inputs.data(), output.data());
  }

  state.SetBytesProcessed(kBytesPerContainer * 3 * state.iterations());
}

BENCHMARK(StaticIntersection2);
BENCHMARK(JitIntersection2);
BENCHMARK(StaticIntersection3);
BENCHMARK(JitIntersection3);

}  // namespace jitmap
