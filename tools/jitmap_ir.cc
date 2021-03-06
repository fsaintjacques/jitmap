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

#include <iostream>
#include <memory>
#include <string>

#include <llvm/IR/Module.h>

#include <jitmap/query/compiler.h>
#include <jitmap/query/parser.h>
#include <jitmap/query/query.h>

namespace query = jitmap::query;

int main(int argc, char** argv) {
  if (argc != 2) {
    return 1;
  }

  auto query_str = argv[1];

  try {
    query::ExecutionContext context{query::JitEngine::Make()};
    auto query = query::Query::Make("query", query_str, &context);
    std::cout << context.jit()->CompileIR(query->name(), query->expr()) << "\n";
  } catch (jitmap::Exception& e) {
    std::cerr << "Problem '" << query_str << "' :\n";
    std::cerr << "\t" << e.message() << "\n";
    return 1;
  }

  return 0;
}
