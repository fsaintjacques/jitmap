#include <memory>

#include <jitmap/query/query.h>

namespace llvm {
class Function;
class Module;
}  // namespace llvm

namespace jitmap {
namespace query {

class CompilerException : public util::Exception {
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

  llvm::Function* Compile(const Query& query);

  std::string DumpIR() const;

  ~QueryCompiler();

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace query
}  // namespace jitmap
