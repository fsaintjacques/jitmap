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

#include <memory>
#include <string>

#include <jitmap/jitmap.h>
#include <jitmap/query/expr.h>
#include <jitmap/query/parser.h>

namespace jitmap {
namespace query {

// Attach a unique name to a bitmap. Used to bind name in the query with a
// physical bitmap.
template <typename T>
struct NamedData {
  std::string name;
  T bitmap;
};

using NamedBitmap = NamedData<Bitmap*>;
using NamedDenseBitset = NamedData<DenseBitset>;

class QueryContext;

class Query : public std::enable_shared_from_this<Query> {
 public:
  // Create a new query object based on an expression.
  //
  // \param[in] name, the name of the query
  // \param[in] expr, the expression of the query
  //
  // \return a new query object
  static std::shared_ptr<Query> Make(std::string name, Expr* expr);

  // DenseBitset Evaluate(const std::vector<NamedDenseBitset> bitmaps);

  void Optimize();
  void Compile();

  // Return the names of the referenced bitmap (variables) in the expression.
  const std::vector<std::string>& variables() const { return variables_; }

  // Return the name of the query.
  const std::string& name() const { return name_; }

  // Return the expression of the query.
  const Expr& expr() const { return *query_; }

 private:
  std::string name_;
  std::optional<Expr*> optimized_query_;
  Expr* query_;

  std::vector<std::string> variables_;

  Query(std::string name, Expr* expr);
};

}  // namespace query
}  // namespace jitmap
