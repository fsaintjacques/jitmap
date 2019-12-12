#pragma once

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

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

  Expr* N(std::string name) { return expr_builder_.NamedRef(name); }
  Expr* NamedRef(std::string name) { return expr_builder_.NamedRef(name); }

  Expr* I(size_t index) { return expr_builder_.IndexRef(index); }
  Expr* IndexRef(size_t index) { return expr_builder_.IndexRef(index); }

  Expr* Not(Expr* operand) { return expr_builder_.Not(operand); }

  Expr* And(Expr* lhs, Expr* rhs) { return expr_builder_.And(lhs, rhs); }
  Expr* Or(Expr* lhs, Expr* rhs) { return expr_builder_.Or(lhs, rhs); }
  Expr* Xor(Expr* lhs, Expr* rhs) { return expr_builder_.Xor(lhs, rhs); }

 protected:
  ExprBuilder expr_builder_;
};

ExprBuilder _g_builder_;

Expr* operator"" _(unsigned long long index) { return _g_builder_.IndexRef(index); }
Expr* operator"" _(const char* name) { return _g_builder_.NamedRef(std::string(name)); }

}  // namespace query
}  // namespace jitmap
