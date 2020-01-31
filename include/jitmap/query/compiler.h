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
#include <jitmap/util/pimpl.h>

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

// Signature of generated functions
typedef void (*DenseEvalFn)(const char**, char*);

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

class Expr;

class JitEngineImpl;
// The JitEngine class transforms IR queries into executable functions.
class JitEngine : util::Pimpl<JitEngineImpl> {
 public:
  // Create a JitEngine.
  //
  // \param[in] options, see `CompilerOptions` documentation.
  //
  // \return a JitEngine
  static std::shared_ptr<JitEngine> Make(CompilerOptions options = {});

  // Compile a query expression into the module.
  //
  // Takes an expression, lowers it to LLVM's IR. Pass the IR module to the
  // internal jit engine which compiles it into assembly. Inject an executable
  // function symbol in the current process. See `Lookup` in order to retrieve
  // a function pointer to this symbol.
  //
  // \param[in] name, the query name, will be used in the generated symbol name.
  //                  The name must be unique with regards to previously
  //                  compiled queries.
  // \param[in] expr, the query expression.
  //
  // \throws CompilerException if any errors is encountered.
  void Compile(const std::string& name, const Expr& expression);

  // Lower a query expression to LLVM's IR representation.
  //
  // This method is used for debugging. An executable symbol is _not_ generated
  // with this function, see `Compile`.
  //
  // \param[in] name, the query name, will be used in the generated symbol name.
  // \param[in] expr, the query expression.
  //
  // \return the IR of the compiled function.
  //
  // \throws CompilerException if any errors is encountered.
  std::string CompileIR(const std::string& name, const Expr& expression);

  // Lookup a query
  DenseEvalFn LookupUserQuery(const std::string& query_name);

  // Return the LLVM name for the host CPU.
  //
  // This is the string given to `-march/-mtune/-mcpu`. See
  // http://llvm.org/doxygen/Host_8h_source.html for more information.
  std::string GetTargetCPU() const;

  // Return the LLVM target triple for the host.
  //
  // The format is ARCHITECTURE-VENDOR-OPERATING_SYSTEM-ENVIRONMENT. See
  // http://llvm.org/doxygen/Triple_8h_source.html for more information.
  std::string GetTargetTriple() const;

 private:
  explicit JitEngine(CompilerOptions options);
};

}  // namespace query
}  // namespace jitmap
