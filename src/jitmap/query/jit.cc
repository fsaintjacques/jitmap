#include <llvm/ADT/Triple.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>

#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Vectorize.h>
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include <jitmap/query/jit.h>

namespace orc = llvm::orc;

namespace jitmap {
namespace query {

void RaiseOnFailure(llvm::Error error) {
  if (error) {
    auto error_msg = llvm::toString(std::move(error));
    throw JitException("LLVM error: ", error_msg);
  }
}

template <typename T>
T ExpectOrRaise(llvm::Expected<T>&& expected) {
  if (!expected) {
    RaiseOnFailure(expected.takeError());
  }

  return std::move(*expected);
}

llvm::CodeGenOpt::Level CodeGetOptFromNumber(uint8_t level) {
  switch (level) {
    case 0:
      return llvm::CodeGenOpt::None;
    case 1:
      return llvm::CodeGenOpt::Less;
    case 2:
      return llvm::CodeGenOpt::Default;
    case 3:
    default:
      return llvm::CodeGenOpt::Aggressive;
  }
}

llvm::SubtargetFeatures DetectHostCPUFeatures() {
  llvm::SubtargetFeatures features;
  llvm::StringMap<bool> map;
  llvm::sys::getHostCPUFeatures(map);
  for (auto& feature : map) {
    features.AddFeature(feature.first(), feature.second);
  }
  return features;
}

auto InitHostTargetMachineBuilder(const CompilerOptions& opts) {
  // Ensure LLVM registries are populated.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  auto machine_builder = ExpectOrRaise(orc::JITTargetMachineBuilder::detectHost());
  machine_builder.setCodeGenOptLevel(CodeGetOptFromNumber(opts.optimization_level));
  // In newer LLVM version, cpu and features are detected by detectHost();
  machine_builder.setCPU(llvm::sys::getHostCPUName());
  machine_builder.addFeatures(DetectHostCPUFeatures().getFeatures());

  return machine_builder;
}

std::unique_ptr<orc::LLJIT> InitLLJIT(orc::JITTargetMachineBuilder builder,
                                      llvm::DataLayout layout,
                                      const CompilerOptions& options) {
  return ExpectOrRaise(orc::LLJIT::Create(builder, layout, options.compiler_threads));
}

auto AsThreadSafeModule(QueryIRCodeGen::ModuleAndContext module_context) {
  auto [module, context] = std::move(module_context);
  return orc::ThreadSafeModule(std::move(module), std::move(context));
}

class JitEngine::Impl {
 public:
  Impl(orc::JITTargetMachineBuilder machine_builder, const CompilerOptions& opts)
      : host_(ExpectOrRaise(machine_builder.createTargetMachine())),
        jit_(InitLLJIT(machine_builder, host_->createDataLayout(), opts)),
        user_queries_(jit_->createJITDylib("jitmap.user")) {}

  void AddModule(QueryIRCodeGen query) {
    auto module = Optimize(AsThreadSafeModule(std::move(query).Finish()));
    RaiseOnFailure(jit_->addIRModule(user_queries_, std::move(module)));
  }

  DenseEvalFn LookupUserQuery(const std::string& query_name) {
    auto symbol = ExpectOrRaise(jit_->lookup(user_queries_, query_name));
    return llvm::jitTargetAddressToPointer<DenseEvalFn>(symbol.getAddress());
  }

  // Introspection
  std::string GetTargetCPU() const { return host_->getTargetCPU(); }
  std::string GetTargetFeatureString() const { return host_->getTargetFeatureString(); }
  std::string GetTargetTriple() const { return host_->getTargetTriple().normalize(); }
  const llvm::DataLayout layout() const { return host_->createDataLayout(); }

 private:
  orc::ThreadSafeModule Optimize(orc::ThreadSafeModule thread_safe_module) {
    auto module = thread_safe_module.getModule();
    auto pass_manager = std::make_unique<llvm::legacy::FunctionPassManager>(module);
    auto target_analysis = host_->getTargetIRAnalysis();
    pass_manager->add(llvm::createTargetTransformInfoWrapperPass(target_analysis));
    pass_manager->add(llvm::createInstructionCombiningPass());
    pass_manager->add(llvm::createPromoteMemoryToRegisterPass());
    pass_manager->add(llvm::createGVNPass());
    pass_manager->add(llvm::createNewGVNPass());
    pass_manager->add(llvm::createCFGSimplificationPass());
    pass_manager->add(llvm::createLoopSimplifyPass());
    pass_manager->add(llvm::createLoopVectorizePass());
    pass_manager->add(llvm::createLoopUnrollPass());
    pass_manager->doInitialization();

    for (auto& function : *module) {
      pass_manager->run(function);
    }

    return thread_safe_module;
  }

 private:
  std::unique_ptr<llvm::TargetMachine> host_;
  std::unique_ptr<orc::LLJIT> jit_;
  orc::JITDylib& user_queries_;
};

JitEngine::JitEngine(CompilerOptions opts)
    : impl_(std::make_unique<JitEngine::Impl>(InitHostTargetMachineBuilder(opts), opts)) {
}
JitEngine::~JitEngine() {}
JitEngine::JitEngine(JitEngine&& other) { std::swap(impl_, other.impl_); }

std::string JitEngine::GetTargetCPU() const { return impl_->GetTargetCPU(); }
std::string JitEngine::GetTargetFeatureString() const {
  return impl_->GetTargetFeatureString();
}
std::string JitEngine::GetTargetTriple() const { return impl_->GetTargetTriple(); }

void JitEngine::Compile(QueryIRCodeGen query) { impl_->AddModule(std::move(query)); }

DenseEvalFn JitEngine::LookupUserQuery(const std::string& query_name) {
  return impl_->LookupUserQuery(query_name);
}

}  // namespace query
}  // namespace jitmap
