#include "../query_test.h"

#include <jitmap/query/optimizer.h>

namespace jitmap {
namespace query {

class OptimizationTest : public QueryTest {
 protected:
  void ExpectOpt(OptimizationPass& p, Expr* input, Expr* expected) {
    ExprEq(p(input), expected);
  }

  Expr* e = And(Or(Var("1"), Not(Var("a"))), Var("2"));
  Expr* f = Or(And(Var("0"), Not(Var("b"))), Var("1"));
};

TEST_F(OptimizationTest, ConstantFolding) {
  ConstantFolding cf(&expr_builder_);

  ExpectOpt(cf, Not(Full()), Empty());
  ExpectOpt(cf, Not(Empty()), Full());
  ExpectOpt(cf, Not(e), Not(e));

  ExpectOpt(cf, And(Full(), Empty()), Empty());
  ExpectOpt(cf, And(e, Empty()), Empty());
  ExpectOpt(cf, And(Full(), e), e);
  ExpectOpt(cf, And(e, f), And(e, f));

  ExpectOpt(cf, Or(Full(), Empty()), Full());
  ExpectOpt(cf, Or(e, Empty()), e);
  ExpectOpt(cf, Or(Full(), e), Full());
  ExpectOpt(cf, Or(e, f), Or(e, f));

  ExpectOpt(cf, Xor(e, Empty()), e);
  ExpectOpt(cf, Xor(Full(), e), Not(e));
  ExpectOpt(cf, Xor(e, f), Xor(e, f));
}

TEST_F(OptimizationTest, SameOperandFolding) {
  SameOperandFolding so(&expr_builder_);

  ExpectOpt(so, And(e, e), e);
  ExpectOpt(so, And(e, f), And(e, f));

  ExpectOpt(so, Or(e, e), e);
  ExpectOpt(so, Or(e, f), Or(e, f));

  ExpectOpt(so, Xor(e, e), Empty());
  ExpectOpt(so, Xor(e, f), Xor(e, f));
}

TEST_F(OptimizationTest, NotChainFolding) {
  NotChainFolding nc(&expr_builder_);

  ExpectOpt(nc, e, e);
  ExpectOpt(nc, Not(e), Not(e));
  ExpectOpt(nc, Not(Not(e)), e);
  ExpectOpt(nc, Not(Not(Not(e))), Not(e));
  ExpectOpt(nc, Not(Not(Not(Not(e)))), e);
  ExpectOpt(nc, Not(Not(Not(Not(Not(e))))), Not(e));
  ExpectOpt(nc, Not(Not(Not(Not(Not(Not(e)))))), e);
}

}  // namespace query
}  // namespace jitmap
