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
