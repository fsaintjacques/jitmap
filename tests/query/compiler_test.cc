#include "../query_test.h"

#include <jitmap/query/compiler.h>
#include <jitmap/query/query.h>

namespace jitmap {
namespace query {

class QueryIRCodeGenTest : public QueryTest {};

TEST_F(QueryIRCodeGenTest, Basic) {
  auto query = Query::Make("not_a", Not(Var("a")));
  QueryIRCodeGen("jitmap").Compile(query);
}

}  // namespace query
}  // namespace jitmap
