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

using namespace jitmap::query;

std::string DumpIR(llvm::Module& module) {
  std::string buffer;
  llvm::raw_string_ostream ss{buffer};
  module.print(ss, nullptr);
  return ss.str();
}

int main(int argc, char** argv) {
  if (argc != 2) {
    return 1;
  }

  try {
    auto query = Query::Make("query", argv[1]);
    auto compiler = QueryIRCodeGen("jitmap-ir-module");
    compiler.Compile(*query);
    auto [module, ctx] = std::move(compiler).Finish();
    std::cout << DumpIR(*module) << "\n";
    module.reset();
  } catch (jitmap::query::ParserException& e) {
    std::cerr << "Problem parsing '" << argv[1] << "' :\n";
    std::cerr << "\t" << e.message() << "\n";
    return 1;
  }

  return 0;
}
