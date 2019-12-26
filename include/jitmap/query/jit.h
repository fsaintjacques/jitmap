#pragma once

#include <functional>
#include <memory>

#include <jitmap/query/compiler.h>
#include <jitmap/util/exception.h>

namespace jitmap {
namespace query {

class JitException : public Exception {
 public:
  using Exception::Exception;
};

// Signature of generated functions
typedef void (*DenseEvalFn)(const BitsetWordType**, BitsetWordType*);

// The JitEngine class transforms IR queries into executable functions.
class JitEngine {
 public:
  JitEngine(CompilerOptions options = {});
  JitEngine(JitEngine&&);
  ~JitEngine();

  // Return the LLVM name for the host CPU.
  //
  // This is the string given to `-march/-mtune/-mcpu`. See
  // http://llvm.org/doxygen/Host_8h_source.html for more information.
  std::string GetTargetCPU() const;

  // Return the LLVM features string for the host CPU.
  //
  // An array delimited by comma of symbols referencing a specific cpu feature.
  // The feature supported are prefixed by `+`, and unsupported by `-`. See
  // http://llvm.org/doxygen/Host_8h_source.html for more information.
  std::string GetTargetFeatureString() const;

  // Return the LLVM target triple for the host.
  //
  // The format is ARCHITECTURE-VENDOR-OPERATING_SYSTEM-ENVIRONMENT. See
  // http://llvm.org/doxygen/Triple_8h_source.html for more information.
  std::string GetTargetTriple() const;

  // Compile a query.
  void Compile(QueryIRCodeGen query);

  // Lookup a query
  DenseEvalFn LookupUserQuery(const std::string& query_name);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;

  JitEngine(const JitEngine&) = delete;
};

}  // namespace query
}  // namespace jitmap
