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
#include <vector>

#include <jitmap/util/pimpl.h>

namespace jitmap {
namespace query {

class Expr;
class ExecutionContext;

class QueryImpl;
class Query : util::Pimpl<QueryImpl> {
 public:
  // Create a new query object based on an expression.
  //
  // \param[in] name, the name of the query
  // \param[in] expr, the expression of the query
  // \param[in] context, the context where queries are compiled
  //
  // \return a new query object
  //
  // \throws ParserException with a reason why the parsing failed.
  static std::shared_ptr<Query> Make(const std::string& name, const std::string& query,
                                     ExecutionContext* context);

  // Return the name of the query.
  const std::string& name() const;

  // Return the expression of the query.
  const Expr& expr() const;

 private:
  Query(std::string name, std::string query, ExecutionContext* context);
};

class JitEngine;

class ExecutionContext {
 public:
  JitEngine* jit() { return jit_.get(); }

 private:
  std::unique_ptr<JitEngine> jit_;
};

}  // namespace query
}  // namespace jitmap
