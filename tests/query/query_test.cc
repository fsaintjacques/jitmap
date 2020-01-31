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

namespace jitmap {
namespace query {

class QueryExecTest : public QueryTest {
 protected:
  ExecutionContext ctx{JitEngine::Make()};
};

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
  std::vector<char> a(kBytesPerContainer, 0x00);
  std::vector<char> result(kBytesPerContainer, 0x00);
  std::vector<const char*> inputs{};

  auto not_a = Query::Make("not_a", "!a", &ctx);
  EXPECT_THROW(not_a->Eval(nullptr, nullptr), Exception);
  EXPECT_THROW(not_a->Eval(nullptr, result.data()), Exception);
  inputs = {nullptr};
  EXPECT_THROW(not_a->Eval(inputs.data(), result.data()), Exception);
}

TEST_F(QueryExecTest, Eval) {
  std::vector<char> a(kBytesPerContainer, 0x00);
  std::vector<char> b(kBytesPerContainer, 0xFF);
  std::vector<char> result(kBytesPerContainer, 0x00);
  std::vector<const char*> inputs{};

  auto not_a = Query::Make("not_a", "!a", &ctx);

  inputs = {a.data()};
  not_a->Eval(inputs.data(), result.data());
  EXPECT_THAT(result, testing::Each(0xFF));

  auto a_and_b = Query::Make("a_and_b", "a & b", &ctx);

  inputs = {a.data(), b.data()};
  a_and_b->Eval(inputs.data(), result.data());
  EXPECT_THAT(result, testing::Each(0x00));

  // It can runs with the same input twice.
  inputs = {b.data(), b.data()};
  a_and_b->Eval(inputs.data(), result.data());
  EXPECT_THAT(result, testing::Each(0xFF));
}

}  // namespace query
}  // namespace jitmap
