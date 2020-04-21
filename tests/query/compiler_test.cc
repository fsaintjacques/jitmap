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

#include "../query_test.h"

#include <atomic>

#include <jitmap/query/compiler.h>
#include <jitmap/query/parser.h>
#include <jitmap/util/compiler.h>
#include <jitmap/util/aligned.h>

namespace jitmap {
namespace query {

class JitTest : public QueryTest {
 public:
  void AssertQueryResult(const std::string& query_expr, std::vector<char> input_words,
                         char output_word) {
    auto query = Query::Make(query_name(), query_expr, &ctx);

    auto [_, inputs] = InitInputs(input_words);
    JITMAP_UNUSED(_);

    auto variables = query->variables();
    EXPECT_EQ(inputs.size(), variables.size());

    aligned_array<char, kBytesPerContainer> output;
    EXPECT_THAT(output, testing::Each(0UL));

    query->Eval(std::move(inputs), output.data());
    EXPECT_THAT(output, testing::Each(output_word));
  }

 private:
  std::string query_name() { return "query_" + std::to_string(id++); }

  std::tuple<std::vector<aligned_array<char, kBytesPerContainer>>,
             std::vector<const char*>>
  InitInputs(std::vector<char> input_words) {
    size_t n_bitmaps = input_words.size();

    std::vector<aligned_array<char, kBytesPerContainer>> bitmaps(n_bitmaps);
    std::vector<const char*> inputs(n_bitmaps);
    for (size_t i = 0; i < n_bitmaps; i++) {
      auto repeated_word = input_words[i];
      auto& bitmap = bitmaps[i];
      std::fill(bitmap.begin(), bitmap.end(), repeated_word);
      inputs[i] = bitmap.data();
      EXPECT_THAT(bitmaps[i], testing::Each(repeated_word));
    }

    return {std::move(bitmaps), std::move(inputs)};
  }

 protected:
  std::atomic<uint32_t> id = 0;
  ExecutionContext ctx{JitEngine::Make()};
};

TEST_F(JitTest, CpuDetection) {
  EXPECT_NE(ctx.jit()->GetTargetCPU(), "");
  EXPECT_NE(ctx.jit()->GetTargetTriple(), "");
}

TEST_F(JitTest, CompileAndExecuteTest) {
  char full = 0xFF;
  char empty = 0x0;

  char a = 0b00010010;
  char b = 0b11001000;
  char c = 0b00000001;
  char d = 0b11111111;
  char e = 0b11111110;

  AssertQueryResult("!a", {a}, ~a);
  AssertQueryResult("a & b", {a, b}, a & b);
  AssertQueryResult("a | b", {a, b}, a | b);
  AssertQueryResult("a ^ b", {a, b}, a ^ b);

  AssertQueryResult("full ^ b", {full, b}, full ^ b);
  AssertQueryResult("empty | !empty", {empty}, full);

  AssertQueryResult("a & b & c & d & e", {a, b, c, d, e}, a & b & c & d & e);
  AssertQueryResult("a | b | c | d | e", {a, b, c, d, e}, a | b | c | d | e);

  // Complex re-use of inputs
  AssertQueryResult("(a | b) & (((!a & c) | (d & b)) ^ (!e & b))", {a, b, c, d, e},
                    (a | b) & (((~a & c) | (d & b)) ^ (~e & b)));
}

}  // namespace query
}  // namespace jitmap
