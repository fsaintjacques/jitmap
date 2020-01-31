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

#include <jitmap/size.h>
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
  // \param[in] name, the name of the query, see further note for restriction.
  // \param[in] expr, the expression of the query.
  // \param[in] context, the context where queries are compiled.
  //
  // \return a new query object
  //
  // \throws ParserException if the expression is not valid, CompilerException
  // if any failure was encountered while compiling the expression or if the
  // query name is not valid.
  //
  // A query expression is compiled into a native function. The function is
  // loaded in the process executable memory under a unique symbol name. This
  // symbol is constructed partly from the query name. Thus, the query name must
  // start with an alpha-numeric character and the remaining characters must be
  // alpha-numeric or an underscore.
  static std::shared_ptr<Query> Make(const std::string& name, const std::string& query,
                                     ExecutionContext* context);

  // Evaluate the expression on dense bitmaps.
  //
  // \param[in] inputs, pointers to input bitmaps, see further note on ordering.
  // \param[in] output, pointer where the resulting bitmap will be written to,
  //                    must not be nullptr.
  //
  // \throws Exception if any of the inputs/output pointers are nullptr.
  //
  // All the bitmaps must have allocated of the proper size, i.e.
  // `kBytesPerContainer`. The
  //
  // The query expression references bitmaps by name, e.g. the expression
  // `a & (b ^ !c) & (!a  ^ c)` depends on the `a`, `b`, and `c` bitmaps. Since
  // the expression is a tree structure, there is multiple ways to order the
  // bitmaps. For this reason, the `Query::variables` method indicates the
  // order in which bitmaps are expected by the `Query::Eval` method. The
  // following pseudo-code shows how to do this.
  //
  // ```
  // Bitmap a, b, c, output;
  // auto order = query->variables();
  // auto ordered_bitmaps = ReorderInputs({"a": a, "b": b, "c": c}, order);
  // query->Eval(ordered_bitmaps, output);
  // ```
  void Eval(const BitsetWordType** inputs, BitsetWordType* output);

  // Return the referenced variables and the expected order.o
  //
  // The ordering is fix once a Query object is constructed, i.e. the ordering
  // is fixed for a given instance of a Query. If the query is deleted and then
  // reconstructed with the same expression, there is not guarantee.
  const std::vector<std::string>& variables() const;

  // Return the name of the query.
  const std::string& name() const;

  // Return the expression of the query.
  const Expr& expr() const;

 private:
  // Private constructor, see Query::Make.
  Query(std::string name, std::string query, ExecutionContext* context);
};

class JitEngine;

class ExecutionContext {
 public:
  explicit ExecutionContext(std::shared_ptr<JitEngine> jit) : jit_(std::move(jit)) {}

  JitEngine* jit() { return jit_.get(); }

 private:
  std::shared_ptr<JitEngine> jit_;
};

}  // namespace query
}  // namespace jitmap
