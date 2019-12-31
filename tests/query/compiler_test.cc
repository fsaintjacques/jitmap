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

class QueryIRCodeGenTest : public QueryTest {};

TEST_F(QueryIRCodeGenTest, Basic) {
  auto query = Query::Make("not_a", Not(Var("a")));
  QueryIRCodeGen("jitmap").Compile(query);
}

}  // namespace query
}  // namespace jitmap
