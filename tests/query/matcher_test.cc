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

#include <jitmap/query/matcher.h>

namespace jitmap {
namespace query {

class MatcherTest : public QueryTest {
 public:
  template <typename E>
  void ExpectMatch(const Matcher& m, E e) {
    EXPECT_TRUE(m(e));
  }

  template <typename E>
  void ExpectNoMatch(const Matcher& m, E e) {
    EXPECT_FALSE(m(e));
  }
};

TEST_F(MatcherTest, TypeMatcher) {
  TypeMatcher or_matcher{Expr::OR_OPERATOR};
  ExpectMatch(or_matcher, Or(Full(), Empty()));
  ExpectNoMatch(or_matcher, And(Full(), Full()));
  ExpectNoMatch(or_matcher, Full());

  TypeMatcher full_and{Expr::FULL_LITERAL, Expr::AND_OPERATOR};
  ExpectMatch(full_and, Full());
  ExpectMatch(full_and, And(Empty(), Var("a")));
  ExpectNoMatch(full_and, Empty());
  ExpectNoMatch(full_and, Or(Full(), Full()));

  TypeMatcher constant{Expr::FULL_LITERAL, Expr::EMPTY_LITERAL};
  ExpectMatch(constant, Full());
  ExpectMatch(constant, Empty());
  ExpectNoMatch(constant, Var("a"));
  ExpectNoMatch(constant, Or(Full(), Var("a")));
}

TEST_F(MatcherTest, OperandMatcher) {
  TypeMatcher constant{Expr::FULL_LITERAL, Expr::EMPTY_LITERAL};
  OperandMatcher any_constant{&constant};

  // Should only match operators
  ExpectNoMatch(any_constant, Full());
  ExpectNoMatch(any_constant, Empty());
  ExpectNoMatch(any_constant, Var("a"));

  ExpectMatch(any_constant, Not(Full()));
  ExpectMatch(any_constant, Or(Empty(), Var("a")));
  ExpectMatch(any_constant, And(Not(Var("b")), Full()));

  ExpectNoMatch(any_constant, Xor(Var("a"), Not(Full())));
}

TEST_F(MatcherTest, ChainMatcher) {
  // Checks that the expression is a NotExpr, and then checks if the operand
  // is also a NotExpr
  auto not_matcher = TypeMatcher{Expr::NOT_OPERATOR};
  auto operand_not_matcher = OperandMatcher{&not_matcher};
  auto not_not_matcher = ChainMatcher{&not_matcher, &operand_not_matcher};
  ExpectMatch(not_not_matcher, Not(Not(Full())));
  ExpectMatch(not_not_matcher, Not(Not(Not(Full()))));
  ExpectNoMatch(not_not_matcher, Not(Full()));
  ExpectNoMatch(not_not_matcher, Or(Not(Not(Full())), Empty()));

  // Equivalent to TypeMatcher of constants
  auto full = TypeMatcher{Expr::FULL_LITERAL};
  auto empty = TypeMatcher{Expr::EMPTY_LITERAL};
  auto const_matcher = ChainMatcher{{&full, &empty}, ChainMatcher::Mode::ANY};
  ExpectMatch(const_matcher, Full());
  ExpectMatch(const_matcher, Empty());
  ExpectNoMatch(const_matcher, Not(Empty()));
  ExpectNoMatch(const_matcher, Or(Full(), Empty()));
  ExpectNoMatch(const_matcher, Var("a"));
}

}  // namespace query
}  // namespace jitmap
