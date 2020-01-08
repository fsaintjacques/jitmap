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
#include <tuple>

#include <jitmap/size.h>
#include <jitmap/util/exception.h>

namespace llvm {
class LLVMContext;
class Module;
}  // namespace llvm

namespace jitmap {
namespace query {

class CompilerException : public Exception {
 public:
  using Exception::Exception;
};

struct CompilerOptions {
  // Controls LLVM optimization level (-O0, -O1, -O2, -O3). Anything above 3
  // will be clamped to 3.
  uint8_t optimization_level = 3;

  // CPU architecture to optimize for. This will dictate the "best" vector
  // instruction set to compile with. If unspecified or empty, llvm will
  // auto-detect the host cpu architecture.
  //
  // Invoke clang with `-mcpu=?` options to get a list of supported strings, e.g.
  //   - core-avx-i
  //   - core-avx2
  //   - skylake-avx512
  std::string cpu = "";
};

class Query;

// The QueryIRCodeGen class generates LLVM IR for query expression on dense
// bitmaps. The QueryIRCodeGen is backed by a single llvm::Module where each
// Query object is represented by a single llvm::Function.
class QueryIRCodeGen {
 public:
  QueryIRCodeGen(const std::string& module_name);
  QueryIRCodeGen(QueryIRCodeGen&&);
  ~QueryIRCodeGen();

  // Compile a query expression into the module.
  //
  // - The query's name must be unique within the QueryIRCodeGen object.
  // - The generated function is not executable as-is, it is only the LLVM IR
  //   representation.
  //
  // \param[in] query, the query to compile into LLVM IR.
  //
  // \throws CompilerException if any errors is encountered.
  void Compile(const Query& query);

  // Compile a query expression into the module.
  template <typename Ptr>
  void Compile(const Ptr& query) {
    JITMAP_PRE_NE(query, nullptr);
    Compile(*query);
  }

  // Finalize and consume as a module and context.
  //
  // The QueryIRCodeGen must be moved to invoke this function, e.g.
  //
  //     auto [mod_, ctx_] = std::move(query_compiler).Finish();
  //
  // The move is required because the module and context ownership are
  // transferred to the caller. Instead of resetting the internal state, the
  // caller must create a new QueryIRCodeGen.
  using ModuleAndContext =
      std::pair<std::unique_ptr<llvm::Module>, std::unique_ptr<llvm::LLVMContext>>;
  ModuleAndContext Finish() &&;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  QueryIRCodeGen(const QueryIRCodeGen&) = delete;
};

}  // namespace query
}  // namespace jitmap
