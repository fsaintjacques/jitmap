#include "../query_test.h"

#include <jitmap/query/optimizer.h>

namespace jitmap {
namespace query {

class OptimizationTest : public QueryTest {
 protected:
  void ExpectOpt(OptimizationPass& p, Expr* input, Expr* expected) {
    ExprEq(p(input), expected);
  }

  void ExpectOpt(Optimizer& o, Expr* input, Expr* expected) {
    ExprEq(o.Optimize(*input), expected);
  }

  Expr* e = Var("e");
  Expr* f = Or(And(Var("a"), Not(Var("b"))), Var("c"));
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

TEST_F(OptimizationTest, Optimizer) {
  Optimizer opt(&expr_builder_);

  ExpectOpt(opt, e, e);

  // ConstantFolding
  ExpectOpt(opt, Not(Full()), Empty());
  // SameOperandFolding
  ExpectOpt(opt, And(e, e), e);
  // NotChainFolding
  ExpectOpt(opt, Not(Not(e)), e);

  // A mixed bag
  ExpectOpt(opt, And(e, Or(e, Not(Not(Not(Full()))))), e);
}

}  // namespace query
}  // namespace jitmap
