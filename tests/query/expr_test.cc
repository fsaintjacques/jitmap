#include "../query_test.h"

#include <jitmap/query/expr.h>
#include <jitmap/query/parser.h>

namespace jitmap {
namespace query {

class ExprTest : public QueryTest {};

TEST_F(ExprTest, Equals) {
  // Ensure pointer is different than the builder's ptr.
  FullBitmapExpr f;
  EmptyBitmapExpr e;

  ExprEq(&f, &f);
  ExprEq(Full(), &f);
  ExprEq(&e, Empty());

  ExprEq(IndexRef(0), IndexRef(0));
  ExprNe(IndexRef(1), IndexRef(2));

  ExprEq(NamedRef("a"), NamedRef("a"));
  ExprNe(NamedRef("b"), NamedRef("c"));

  ExprEq(Not(&f), Not(Full()));
  ExprNe(Not(&e), Not(&f));

  ExprEq(And(IndexRef(0), Or(NamedRef("a"), &f)),
         And(IndexRef(0), Or(NamedRef("a"), &f)));
}

TEST_F(ExprTest, EqualsNotCommutative) {
  ExprNe(And(Full(), Empty()), And(Empty(), Full()));
}

using testing::UnorderedElementsAre;

template <typename... T>
void ReferencesAre(Expr* expr, T... names) {
  EXPECT_THAT(expr->CollectReferences(), UnorderedElementsAre(names...));
}

template <typename... T>
void ReferencesAre(const std::string& query, T... names) {
  ExprBuilder builder;
  auto expr = Parse(query, &builder);
  EXPECT_THAT(expr->CollectReferences(), UnorderedElementsAre(names...));
}

TEST_F(ExprTest, CollectReferences) {
  ReferencesAre("$0", "$0");
  ReferencesAre("$0 ^ $1", "$0", "$1");
  ReferencesAre("$0 ^ ($1 | $0)", "$0", "$1");
}

}  // namespace query
}  // namespace jitmap
