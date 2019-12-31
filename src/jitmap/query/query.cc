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

#include <memory>
#include <string>
#include <vector>

#include "jitmap/query/optimizer.h"
#include "jitmap/query/parser.h"

namespace jitmap {
namespace query {

class Query::Impl {
 public:
  Impl(std::string name, std::string query)
      : name_(std::move(name)),
        query_(std::move(query)),
        expr_(Parse(query_, &builder_)),
        optimized_expr_(Optimizer(&builder_).Optimize(*expr_)),
        variables_(expr_->Variables()) {}

  // Accessors
  const std::string& name() const { return name_; }
  const std::string& query() const { return query_; }
  const Expr& expr() const { return *expr_; }
  const Expr& optimized_expr() const { return *optimized_expr_; }
  const std::vector<std::string>& variables() const { return variables_; }

 private:
  std::string name_;
  std::string query_;
  ExprBuilder builder_;
  Expr* expr_;
  Expr* optimized_expr_;
  std::vector<std::string> variables_;
};

Query::Query(std::string name, std::string query)
    : impl_(std::make_unique<Impl>(std::move(name), std::move(query))) {}
Query::~Query() {}

std::shared_ptr<Query> Query::Make(std::string name, std::string query) {
  return std::shared_ptr<Query>(new Query(std::move(name), std::move(query)));
}

const std::string& Query::name() const { return impl_->name(); }
const Expr& Query::expr() const { return impl_->expr(); }
const std::vector<std::string>& Query::variables() const { return impl_->variables(); }

}  // namespace query
}  // namespace jitmap
