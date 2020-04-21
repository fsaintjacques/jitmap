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

#include <jitmap/query/compiler.h>
#include <jitmap/query/query.h>
#include <jitmap/util/aligned.h>

namespace jitmap {
namespace query {

class QueryExecTest : public QueryTest {};

ExecutionContext ctx{JitEngine::Make()};

using testing::ContainerEq;
using testing::ElementsAre;

TEST_F(QueryExecTest, MakeInvalidNames) {
  EXPECT_THROW(Query::Make("", "!a", &ctx), CompilerException);
  EXPECT_THROW(Query::Make("_a", "!a", &ctx), CompilerException);
  EXPECT_THROW(Query::Make("^-?", "!a", &ctx), CompilerException);
  EXPECT_THROW(Query::Make("herpidy^", "!a", &ctx), CompilerException);
}

TEST_F(QueryExecTest, MakeInvalidExpressions) {
  EXPECT_THROW(Query::Make("valid", "", &ctx), ParserException);
  EXPECT_THROW(Query::Make("valid", "a !^ b", &ctx), ParserException);
  EXPECT_THROW(Query::Make("valid", "a b", &ctx), ParserException);
}

TEST_F(QueryExecTest, Make) {
  auto q1 = Query::Make("q1", "a & b", &ctx);
  EXPECT_EQ(q1->name(), "q1");
  EXPECT_EQ(q1->expr(), And(Var("a"), Var("b")));

  auto q2 = Query::Make("q2", "a ^ a", &ctx);
  EXPECT_EQ(q2->name(), "q2");
  EXPECT_EQ(q2->expr(), Xor(Var("a"), Var("a")));
}

TEST_F(QueryExecTest, variables) {
  auto not_a = Query::Make("not_a", "!a", &ctx);
  EXPECT_THAT(not_a->variables(), ElementsAre("a"));

  auto a_and_b = Query::Make("a_and_b", "a & b", &ctx);
  EXPECT_THAT(a_and_b->variables(), ElementsAre("a", "b"));

  auto a_xor_a = Query::Make("a_xor_a", "a ^ a", &ctx);
  EXPECT_THAT(a_xor_a->variables(), ElementsAre("a"));

  auto nested = Query::Make("nested", "!a | ((a ^ b) & (c | d) ^ b)", &ctx);
  EXPECT_THAT(nested->variables(), ElementsAre("a", "b", "c", "d"));
}

TEST_F(QueryExecTest, EvalInvalidParameters) {
  aligned_array<char, kBytesPerContainer> a(0x00);
  aligned_array<char, kBytesPerContainer> result(0x00);
  std::vector<const char*> inputs{};

  auto not_a = Query::Make("invalid_param", "!a", &ctx);
  std::vector<const char*> empty;
  EXPECT_THROW(not_a->Eval(empty, nullptr), Exception);
  EXPECT_THROW(not_a->Eval(empty, result.data()), Exception);
  inputs = {nullptr};
  EXPECT_THROW(not_a->Eval(inputs, result.data()), Exception);
}

TEST_F(QueryExecTest, Eval) {
  aligned_array<char, kBytesPerContainer> a(0x00);
  aligned_array<char, kBytesPerContainer> b(0xFF);
  aligned_array<char, kBytesPerContainer> result(0x00);
  std::vector<const char*> inputs{};

  auto not_a = Query::Make("single_param", "!a", &ctx);

  inputs = {a.data()};
  not_a->Eval(inputs, result.data());
  EXPECT_THAT(result, testing::Each(0xFF));

  auto a_and_b = Query::Make("double_param", "a & b", &ctx);

  inputs = {a.data(), b.data()};
  a_and_b->Eval(inputs, result.data());
  EXPECT_THAT(result, testing::Each(0x00));

  // It can runs with the same input twice.
  a_and_b->Eval({b.data(), b.data()}, result.data());
  EXPECT_THAT(result, testing::Each(0xFF));
}

using MissingPolicy = EvaluationContext::MissingPolicy;

TEST_F(QueryExecTest, EvalWithMissingPolicy) {
  aligned_array<char, kBytesPerContainer> result(0x00);
  std::vector<const char*> inputs{nullptr};

  auto q = Query::Make("another_not_a", "!a", &ctx);

  EvaluationContext eval_ctx;
  EXPECT_THROW(q->Eval(eval_ctx, inputs, result.data()), Exception);

  eval_ctx.set_missing_policy(MissingPolicy::REPLACE_WITH_EMPTY);
  q->Eval(eval_ctx, inputs, result.data());
  EXPECT_THAT(result, testing::Each(0xFF));

  eval_ctx.set_missing_policy(MissingPolicy::REPLACE_WITH_FULL);
  q->Eval(eval_ctx, inputs, result.data());
  EXPECT_THAT(result, testing::Each(0x00));
}

TEST_F(QueryExecTest, EvalWithPopCount) {
  aligned_array<char, kBytesPerContainer> a(0x00);
  aligned_array<char, kBytesPerContainer> result(0x00);
  std::vector<const char*> inputs{a.data()};

  auto q = Query::Make("yet_not_a", "!a", &ctx);

  EvaluationContext eval_ctx;
  EXPECT_EQ(q->Eval(eval_ctx, inputs, result.data()), kUnknownPopCount);

  eval_ctx.set_popcount(true);
  EXPECT_EQ(q->Eval(eval_ctx, inputs, result.data()), kBitsPerContainer);

  a.fill(0b00001111);
  EXPECT_EQ(q->Eval(eval_ctx, inputs, result.data()), kBitsPerContainer / 2);

  a.fill(~0b00000001);
  EXPECT_EQ(q->Eval(eval_ctx, inputs, result.data()), kBitsPerContainer / 8);
}

}  // namespace query
}  // namespace jitmap
