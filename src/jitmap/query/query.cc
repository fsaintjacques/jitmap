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

#include "jitmap/query/query.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "jitmap/query/compiler.h"
#include "jitmap/query/optimizer.h"
#include "jitmap/query/parser.h"

namespace jitmap {
namespace query {

class QueryImpl {
 public:
  QueryImpl(std::string name, std::string query, ExecutionContext* context)
      : name_(std::move(name)),
        query_(std::move(query)),
        expr_(Parse(query_, &builder_)),
        optimized_expr_(Optimizer(&builder_).Optimize(*expr_)),
        context_(context),
        variables_(expr_->Variables()) {}

  // Accessors
  const std::string& name() const { return name_; }
  const std::string& query() const { return query_; }
  const Expr& expr() const { return *expr_; }
  const Expr& optimized_expr() const { return *optimized_expr_; }
  const std::vector<std::string>& variables() const { return variables_; }

  DenseEvalFn dense_eval_fn() const { return context_->jit()->LookupUserQuery(name()); }

 private:
  std::string name_;
  std::string query_;
  ExprBuilder builder_;
  Expr* expr_;
  Expr* optimized_expr_;
  ExecutionContext* context_;

  std::vector<std::string> variables_;
};

static inline void ValidateQueryName(const std::string& name) {
  if (name.empty()) {
    throw CompilerException("Query name must have at least one character");
  }

  auto first = name[0];
  if (!std::isalnum(first)) {
    throw CompilerException(
        "The first character of the Query name must be an alpha numeric character but "
        "got",
        first);
  }

  auto is_valid_char = [](auto c) { return std::isalnum(c) || c == '_'; };
  if (!std::all_of(name.cbegin(), name.cend(), is_valid_char)) {
    throw CompilerException(
        "The characters of a query name must either be an alpha numeric character or an "
        "underscore.");
  }
}

Query::Query(std::string name, std::string query, ExecutionContext* context)
    : Pimpl(std::make_unique<QueryImpl>(std::move(name), std::move(query), context)) {}

std::shared_ptr<Query> Query::Make(const std::string& name, const std::string& expr,
                                   ExecutionContext* context) {
  JITMAP_PRE_NE(context, nullptr);

  // Ensure that the query names follows the restriction
  ValidateQueryName(name);

  auto query = std::shared_ptr<Query>(new Query(name, expr, context));
  context->jit()->Compile(query->name(), query->expr());

  return query;
}

const std::string& Query::name() const { return impl().name(); }
const Expr& Query::expr() const { return impl().expr(); }
const std::vector<std::string>& Query::variables() const { return impl().variables(); }

template <char FillByte>
class array_bitmap : public std::array<char, kBytesPerContainer> {
 public:
  array_bitmap() { fill(FillByte); }
};

// Private read-only full and empty bitmap. Used for EvaluationContext::MissingPolicy;
static const array_bitmap<static_cast<char>(0x00)> kEmptyBitmap;
static const array_bitmap<static_cast<char>(0xFF)> kFullBitmap;

using MissingPolicy = EvaluationContext::MissingPolicy;

static inline const char* CoalesceInputPointer(const char* input,
                                               const std::string& variable,
                                               MissingPolicy policy) {
  if (input != nullptr) {
    return input;
  }

  switch (policy) {
    case MissingPolicy::ERROR:
      throw Exception("Missing pointer for bitmap '", variable, ",");
    case MissingPolicy::REPLACE_WITH_EMPTY:
      return kEmptyBitmap.data();
    case MissingPolicy::REPLACE_WITH_FULL:
      return kFullBitmap.data();
  }

  throw Exception("Unreachable in ", __FUNCTION__);
}

static inline std::vector<const char*> FixupInputs(const std::vector<std::string>& vars,
                                                   const char** inputs,
                                                   MissingPolicy policy) {
  auto n_inputs = vars.size();
  std::vector<const char*> addrs;
  for (size_t i = 0; i < n_inputs; i++) {
    addrs.push_back(CoalesceInputPointer(inputs[i], vars[i], policy));
  }

  return addrs;
}

void Query::Eval(const EvaluationContext& eval_ctx, const char** inputs, char* output) {
  JITMAP_PRE_NE(inputs, nullptr);
  JITMAP_PRE_NE(output, nullptr);

  auto addrs = FixupInputs(variables(), inputs, eval_ctx.missing_policy());
  auto eval_fn = impl().dense_eval_fn();
  eval_fn(addrs.data(), output);
}

void Query::Eval(const char** inputs, char* output) {
  EvaluationContext ctx;
  Eval(ctx, inputs, output);
}

}  // namespace query
}  // namespace jitmap
