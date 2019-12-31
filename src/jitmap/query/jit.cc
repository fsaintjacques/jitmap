#include <memory>

#include <llvm/ADT/Triple.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
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
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Vectorize.h>
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/ExecutionEngine/Orc/ObjectTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
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

std::string DetectCPU(const CompilerOptions& opts) {
  if (opts.cpu.empty()) {
    return llvm::sys::getHostCPUName();
  }

  return opts.cpu;
}

auto InitHostTargetMachineBuilder(const CompilerOptions& opts) {
  // Ensure LLVM registries are populated.
  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();

  auto machine_builder = ExpectOrRaise(orc::JITTargetMachineBuilder::detectHost());
  machine_builder.setCodeGenOptLevel(CodeGetOptFromNumber(opts.optimization_level));
  machine_builder.setCPU(DetectCPU(opts));

  return machine_builder;
}

// Register a custom ObjectLinkerLayer to support (query) symbols with gdb and perf.
// LLJIT (via ORC) doesn't support explicitly the llvm::JITEventListener
// interface. This is the missing glue.
std::unique_ptr<orc::ObjectLayer> ObjectLinkingLayerFactory(
    orc::ExecutionSession& execution_session) {
  auto memory_manager_factory = []() {
    return std::make_unique<llvm::SectionMemoryManager>();
  };

  auto linking_layer = std::make_unique<orc::RTDyldObjectLinkingLayer>(
      execution_session, std::move(memory_manager_factory));

  std::vector<llvm::JITEventListener*> listeners{
      llvm::JITEventListener::createGDBRegistrationListener(),
      llvm::JITEventListener::createPerfJITEventListener()};

  // Lambda invoked whenever a new symbol is added.
  auto notify_loaded = [listeners](orc::VModuleKey key,
                                   const llvm::object::ObjectFile& object,
                                   const llvm::RuntimeDyld::LoadedObjectInfo& info) {
    for (auto listener : listeners) {
      if (listener) listener->notifyObjectLoaded(key, object, info);
    }
  };

  linking_layer->setNotifyLoaded(notify_loaded);

  return linking_layer;
}

std::unique_ptr<orc::LLJIT> InitLLJIT(orc::JITTargetMachineBuilder machine_builder,
                                      llvm::DataLayout layout,
                                      const CompilerOptions& options) {
  return ExpectOrRaise(orc::LLJITBuilder()
                           .setJITTargetMachineBuilder(machine_builder)
                           .setObjectLinkingLayerCreator(ObjectLinkingLayerFactory)
                           .create());
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
std::string JitEngine::GetTargetTriple() const { return impl_->GetTargetTriple(); }

void JitEngine::Compile(QueryIRCodeGen query) { impl_->AddModule(std::move(query)); }

DenseEvalFn JitEngine::LookupUserQuery(const std::string& query_name) {
  return impl_->LookupUserQuery(query_name);
}

}  // namespace query
}  // namespace jitmap
