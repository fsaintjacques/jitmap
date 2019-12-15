#include <memory>

#include <jitmap/query/query.h>

namespace llvm {
class Function;
class Module;
}  // namespace llvm

namespace jitmap {
namespace query {

class CompilerException : public Exception {
 public:
  using Exception::Exception;
};

struct CompilerOptions {
  // Controls the number of scalar, i.e. width of the vector aggregate value in
  // the loop. A value of 1 will emit a scalar value instead of a vector value.
  //
  // Usually a small power of 2, e.g. 1, 2, 4, 8 or 16. See the documentation
  // of your hardware platform for the optimal value.
  //
  // LLVM's optimizer is able to perform the auto-vectorization of the loop. In
  // cases where it can't, change the vector width here.
  uint8_t vector_width_ = 1;

  // Controls the width of each scalar (in bits). See the documentation
  // of your hardware platform for the optimal value.
  //
  // LLVM should be able to figure this by default.
  uint8_t scalar_width_ = 64;
};

class QueryCompiler {
 public:
  QueryCompiler(const std::string& module_name, CompilerOptions options = {});

  // Compile a query expression in an llvm::Function.
  //
  // - The query's name must be unique within the QueryCompiler object. It
  //   always returns the first inserted function irregardless of the
  //   expression.
  // - The generated function is not executable as-is, it is only the LLVM IR
  //   representation.
  //
  // \param[in] query, the query to compile into LLVM IR.
  //
  // \return an llvm function object
  //
  // \throws CompilerExpception if any errors is encountered.
  llvm::Function* Compile(const Query& query);

  // Return the LLVM IR representation of the module and all the compiled queries.
  std::string DumpIR() const;

  ~QueryCompiler();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace query
}  // namespace jitmap
