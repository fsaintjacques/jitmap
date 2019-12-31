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

#include <jitmap/query/query.h>

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

  ExprEq(Var("a"), Var("a"));
  ExprNe(Var("b"), Var("c"));

  ExprEq(Not(&f), Not(Full()));
  ExprNe(Not(&e), Not(&f));

  ExprEq(And(Var("b"), Or(Var("a"), &f)), And(Var("b"), Or(Var("a"), &f)));
}

TEST_F(ExprTest, EqualsNotCommutative) {
  ExprNe(And(Full(), Empty()), And(Empty(), Full()));
}

}  // namespace query
}  // namespace jitmap
