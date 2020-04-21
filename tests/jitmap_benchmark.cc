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
#include <functional>
#include <string_view>
#include <vector>

#include <jitmap/jitmap.h>
#include <jitmap/query/compiler.h>
#include <jitmap/query/query.h>
#include <jitmap/size.h>
#include <jitmap/util/aligned.h>

namespace jitmap {

enum PopCountOption {
  WithoutPopCount = 0,
  WithPopCount,
};

template <PopCountOption Opt>
class TreeVisitorFunctor {
 public:
  explicit TreeVisitorFunctor(size_t n_inputs) { inputs.resize(n_inputs); }

  int32_t operator()() {
    output = inputs[0];

    auto n_inputs = inputs.size();
    for (size_t i = 1; i < n_inputs; i++) {
      output &= inputs[i];
    }

    return Opt == WithPopCount ? output.count() : output.all();
  }

 private:
  std::vector<std::bitset<kBitsPerContainer>> inputs;
  std::bitset<kBitsPerContainer> output;
};

template <PopCountOption Opt>
class JitFunctor {
 public:
  explicit JitFunctor(size_t n_inputs) {
    auto query_name = util::StaticFmt("and_", n_inputs);
    query = query::Query::Make(query_name, QueryForInputs(n_inputs), &engine);
    ctx.set_popcount(Opt == WithPopCount);

    bitmaps.resize(n_inputs);
    for (const auto& input : bitmaps) inputs.push_back(input.data());
  }

  int32_t operator()() { return query->EvalUnsafe(ctx, inputs, output.data()); }

  std::string QueryForInputs(size_t n) {
    std::stringstream ss;

    ss << "i_0";
    for (size_t i = 1; i < n; i++) {
      ss << " & "
         << "i_" << i;
    }

    return ss.str();
  }

 private:
  std::shared_ptr<query::Query> query;
  query::EvaluationContext ctx;
  query::ExecutionContext engine{query::JitEngine::Make()};

  std::vector<aligned_array<char, kBytesPerContainer>> bitmaps;
  std::vector<const char*> inputs;
  aligned_array<char, kBytesPerContainer> output;
};

template <typename ComputeFunctor>
static void BasicBenchmark(benchmark::State& state) {
  auto n_bitmaps = static_cast<size_t>(state.range(0));
  std::vector<aligned_array<char, kBytesPerContainer>> bitmaps{n_bitmaps};
  aligned_array<char, kBytesPerContainer> output;

  std::vector<const char*> inputs;
  for (const auto& input : bitmaps) inputs.push_back(input.data());

  ComputeFunctor compute{n_bitmaps};
  for (auto _ : state) {
    benchmark::DoNotOptimize(compute());
  }

  state.SetBytesProcessed(kBytesPerContainer * n_bitmaps * state.iterations());
}

BENCHMARK_TEMPLATE(BasicBenchmark, TreeVisitorFunctor<WithPopCount>)
    ->RangeMultiplier(2)
    ->Range(2, 8);
BENCHMARK_TEMPLATE(BasicBenchmark, TreeVisitorFunctor<WithoutPopCount>)
    ->RangeMultiplier(2)
    ->Range(2, 8);
BENCHMARK_TEMPLATE(BasicBenchmark, JitFunctor<WithPopCount>)
    ->RangeMultiplier(2)
    ->Range(2, 8);
BENCHMARK_TEMPLATE(BasicBenchmark, JitFunctor<WithoutPopCount>)
    ->RangeMultiplier(2)
    ->Range(2, 8);

}  // namespace jitmap
