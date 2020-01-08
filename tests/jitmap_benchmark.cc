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
#include <jitmap/query/jit.h>
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
  std::vector<std::vector<BitsetWordType>> bitmaps;
  std::vector<const BitsetWordType*> inputs;
  for (size_t i = 0; i < 2; i++) {
    bitmaps.emplace_back(kWordsPerContainers, 0UL);
    inputs.emplace_back(bitmaps[i].data());
  }
  std::vector<BitsetWordType> output(kWordsPerContainers, 0UL);

  query::JitEngine engine;
  auto query = query::Query::Make("benchmark_query2", "a & b");
  auto compiler = query::QueryIRCodeGen("benchmark_module");
  compiler.Compile(query);
  engine.Compile(std::move(compiler));
  auto eval_fn = engine.LookupUserQuery("benchmark_query2");

  for (auto _ : state) {
    eval_fn(inputs.data(), output.data());
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
  std::vector<std::vector<BitsetWordType>> bitmaps;
  std::vector<const BitsetWordType*> inputs;
  for (size_t i = 0; i < 3; i++) {
    bitmaps.emplace_back(kWordsPerContainers, 0UL);
    inputs.emplace_back(bitmaps[i].data());
  }
  std::vector<BitsetWordType> output(kWordsPerContainers, 0UL);

  query::JitEngine engine;
  auto query = query::Query::Make("benchmark_query3", "a & b & c");
  auto compiler = query::QueryIRCodeGen("benchmark_module");
  compiler.Compile(query);
  engine.Compile(std::move(compiler));
  auto eval_fn = engine.LookupUserQuery("benchmark_query3");

  for (auto _ : state) {
    eval_fn(inputs.data(), output.data());
  }

  state.SetBytesProcessed(kBytesPerContainer * 3 * state.iterations());
}
/*


static void StaticIntersection4(benchmark::State& state) {
  DenseBitmap a, b, c, d;

  for (auto _ : state) {
    benchmark::DoNotOptimize(a & b & c & d);
  }

  state.SetBytesProcessed(((kBitsPerContainer / 8) * 4) * state.iterations());
}
*/

BENCHMARK(StaticIntersection2);
BENCHMARK(JitIntersection2);
BENCHMARK(StaticIntersection3);
BENCHMARK(JitIntersection3);

/*
BENCHMARK(StaticIntersection3);
BENCHMARK(StaticIntersection4);
*/
}  // namespace jitmap
