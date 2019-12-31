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
#include "jitmap/query/parser.h"

namespace jitmap {
namespace query {

Query::Query(std::string name, Expr* expr)
    : name_(std::move(name)), query_(expr), variables_(query_->Variables()) {}

std::shared_ptr<Query> Query::Make(std::string name, Expr* expr) {
  return std::shared_ptr<Query>(new Query(name, expr));
}

}  // namespace query
}  // namespace jitmap
