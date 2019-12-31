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

#pragma once

#include <string>
#include <string_view>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <jitmap/query/expr.h>
#include <jitmap/query/parser.h>
#include <jitmap/query/query.h>

namespace jitmap {
namespace query {

void ExprEq(Expr* actual, Expr* expected) {
  ASSERT_NE(actual, nullptr);
  ASSERT_NE(expected, nullptr);
  EXPECT_EQ(*actual, *expected);
}

void ExprNe(Expr* lhs, Expr* rhs) {
  ASSERT_NE(lhs, nullptr);
  ASSERT_NE(rhs, nullptr);
  EXPECT_NE(*lhs, *rhs);
}

class QueryTest : public testing::Test {
 public:
  Expr* Empty() { return expr_builder_.EmptyBitmap(); }
  Expr* Full() { return expr_builder_.FullBitmap(); }

  Expr* V(std::string name) { return expr_builder_.Var(name); }
  Expr* Var(std::string name) { return expr_builder_.Var(name); }

  Expr* Not(Expr* operand) { return expr_builder_.Not(operand); }

  Expr* And(Expr* lhs, Expr* rhs) { return expr_builder_.And(lhs, rhs); }
  Expr* Or(Expr* lhs, Expr* rhs) { return expr_builder_.Or(lhs, rhs); }
  Expr* Xor(Expr* lhs, Expr* rhs) { return expr_builder_.Xor(lhs, rhs); }

  Expr* Parse(std::string_view query) {
    return jitmap::query::Parse(query, &expr_builder_);
  }

 protected:
  ExprBuilder expr_builder_;
};

ExprBuilder _g_builder_;

Expr* operator"" _v(const char* name) { return _g_builder_.Var(std::string(name)); }
Expr* operator"" _q(const char* name) { return Parse(name, &_g_builder_); }

}  // namespace query
}  // namespace jitmap
