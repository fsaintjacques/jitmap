#pragma once

#include <memory>

#include <jitmap/query/query.h>
#include <jitmap/size.h>

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

  // Number of background compiler threads.
  uint8_t compiler_threads = 0;

  // Controls the number of scalar, i.e. width of the vector aggregate value in
  // the loop. A value of 1 will emit a scalar value instead of a vector value.
  //
  // Usually a small power of 2, e.g. 1, 2, 4, 8 or 16. See the documentation
  // of your hardware platform for the optimal value.
  //
  // LLVM's optimizer is able to perform the auto-vectorization of the loop. In
  // cases where it can't, change the vector width here.
  uint8_t vector_width = 1;

  // Controls the width of each scalar (in bits). See the documentation
  // of your hardware platform for the optimal value.
  uint8_t scalar_width = kBitsPerBitsetWord;
};

// The QueryIRCodeGen class generates LLVM IR for query expression on dense
// bitmaps. The QueryIRCodeGen is backed by a single llvm::Module where each
// Query object is represented by a single llvm::Function.
class QueryIRCodeGen {
 public:
  QueryIRCodeGen(const std::string& module_name, CompilerOptions options = {});
  QueryIRCodeGen(QueryIRCodeGen&&);

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

  ~QueryIRCodeGen();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  QueryIRCodeGen(const QueryIRCodeGen&) = delete;
};

}  // namespace query
}  // namespace jitmap
