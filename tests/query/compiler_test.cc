#include "../query_test.h"

#include <jitmap/query/compiler.h>
#include <jitmap/query/query.h>

namespace jitmap {
namespace query {

class QueryCompilerTest : public QueryTest {};

TEST_F(QueryCompilerTest, Basic) {
  auto query = Query::Make("test_query", Not(Var("a")));
  Compile(*query);
}

}  // namespace query
}  // namespace jitmap
