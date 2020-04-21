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

#include <memory>

#include <llvm/ADT/Triple.h>
#include <llvm/Analysis/TargetTransformInfo.h>
#include <llvm/CodeGen/TargetPassConfig.h>
#include <llvm/ExecutionEngine/JITEventListener.h>
#include <llvm/ExecutionEngine/JITSymbol.h>
#include <llvm/ExecutionEngine/Orc/ExecutionUtils.h>
#include <llvm/ExecutionEngine/Orc/JITTargetMachineBuilder.h>
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Support/CodeGen.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Transforms/Vectorize.h>
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/ExecutionEngine/Orc/ObjectTransformLayer.h"
#include "llvm/ExecutionEngine/Orc/RTDyldObjectLinkingLayer.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"

#include "codegen.h"
#include "jitmap/query/compiler.h"
#include "jitmap/util/compiler.h"

namespace orc = llvm::orc;

namespace jitmap {
namespace query {

void RaiseOnFailure(llvm::Error error) {
  if (error) {
    auto error_msg = llvm::toString(std::move(error));
    throw CompilerException("LLVM error: ", error_msg);
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
      if (listener != nullptr) {
        listener->notifyObjectLoaded(key, object, info);
      }
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

auto AsThreadSafeModule(ExpressionCodeGen::ContextAndModule ctx_module) {
  auto [context, module] = std::move(ctx_module);
  return orc::ThreadSafeModule(std::move(module), std::move(context));
}

class JitEngineImpl {
 public:
  JitEngineImpl(orc::JITTargetMachineBuilder machine_builder, const CompilerOptions& opts)
      : host_(ExpectOrRaise(machine_builder.createTargetMachine())),
        jit_(InitLLJIT(machine_builder, host_->createDataLayout(), opts)),
        user_queries_(jit_->createJITDylib("jitmap.user")),
        options_(opts) {}

  void Compile(const std::string& name, const Expr& expr) {
    auto thread_safe_module = AsThreadSafeModule(CompileInternal(name, expr));
    Optimize(thread_safe_module.getModule());
    RaiseOnFailure(jit_->addIRModule(user_queries_, std::move(thread_safe_module)));
  }

  std::string CompileIR(const std::string& n, const Expr& e) {
    auto ctx_module = CompileInternal(n, e);
    auto module = ctx_module.second.get();
    // By default, the TargetTriple is not part of the module. This ensure that
    // callers of `jitmap-ir` don't need to explicit the tripple in the command
    // line chain, e.g. via `opt` or `llc` utility.
    module->setTargetTriple(GetTargetTriple());
    Optimize(module);

    std::string ir;
    llvm::raw_string_ostream ss{ir};
    module->print(ss, nullptr);

    return ir;
  }

  DenseEvalFn LookupUserQuery(const std::string& name) {
    auto symbol = ExpectOrRaise(jit_->lookup(user_queries_, name));
    return llvm::jitTargetAddressToPointer<DenseEvalFn>(symbol.getAddress());
  }

  DenseEvalPopCountFn LookupUserPopCountQuery(const std::string& name) {
    auto symbol = ExpectOrRaise(jit_->lookup(user_queries_, query_popcount(name)));
    return llvm::jitTargetAddressToPointer<DenseEvalPopCountFn>(symbol.getAddress());
  }

  // Introspection
  std::string GetTargetCPU() const { return host_->getTargetCPU(); }
  std::string GetTargetTriple() const { return host_->getTargetTriple().normalize(); }
  llvm::DataLayout layout() const { return host_->createDataLayout(); }

 private:
  std::string query_popcount(const std::string query_name) {
    return query_name + "_popcount";
  }

  ExpressionCodeGen::ContextAndModule CompileInternal(const std::string& name,
                                                      const Expr& e) {
    // Generate 2 variants for the expression, one function that returns the
    // popcount, and the other that doesn't tally the popcount and returns void.
    return ExpressionCodeGen("module_a")
        .Compile(name, e, false /* with_popcount */)
        .Compile(query_popcount(name), e, true /* with_popcount */)
        .Finish();
  }

  llvm::Module* Optimize(llvm::Module* module) {
    unsigned opt_level = options_.optimization_level;
    // Don't optimize for size.
    unsigned size_level = 0;

    llvm::PassManagerBuilder builder;
    builder.OptLevel = opt_level;
    builder.SizeLevel = size_level;

    builder.Inliner = llvm::createFunctionInliningPass(opt_level, size_level, false);
    builder.LoopVectorize = true;
    builder.SLPVectorize = true;
    builder.DisableUnrollLoops = false;
    host_->adjustPassManager(builder);

    auto cpu = host_->getTargetCPU();
    for (auto& function : *module) {
      setFunctionAttributes("target-cpu", cpu, function);
    }

    llvm::legacy::FunctionPassManager fn_manager{module};
    auto host_analysis = host_->getTargetIRAnalysis();
    fn_manager.add(llvm::createTargetTransformInfoWrapperPass(host_analysis));
    builder.populateFunctionPassManager(fn_manager);

    fn_manager.add(
        llvm::createLoopUnrollPass(2, /*OnlyWhenForced*/ false, false, -1, 4, 1));

    fn_manager.doInitialization();
    for (auto& function : *module) {
      fn_manager.run(function);
    }
    fn_manager.doFinalization();

    llvm::legacy::PassManager mod_manager;
    auto& llvm_host = dynamic_cast<llvm::LLVMTargetMachine&>(*host_);
    mod_manager.add(llvm_host.createPassConfig(mod_manager));
    builder.populateModulePassManager(mod_manager);

    mod_manager.run(*module);

    return module;
  }

  void setFunctionAttributes(const std::string& attr, const std::string& val,
                             llvm::Function& fn) {
    llvm::AttrBuilder new_attrs;
    if (!val.empty() && !fn.hasFnAttribute(attr)) {
      new_attrs.addAttribute(attr, val);
    }

    constexpr auto fn_index = llvm::AttributeList::FunctionIndex;
    auto attrs = fn.getAttributes();
    fn.setAttributes(attrs.addAttributes(fn.getContext(), fn_index, new_attrs));
  }

 private:
  std::unique_ptr<llvm::TargetMachine> host_;
  std::unique_ptr<orc::LLJIT> jit_;
  orc::JITDylib& user_queries_;
  CompilerOptions options_;
};

JitEngine::JitEngine(CompilerOptions opts)
    : Pimpl(std::make_unique<JitEngineImpl>(InitHostTargetMachineBuilder(opts), opts)) {}

std::shared_ptr<JitEngine> JitEngine::Make(CompilerOptions opts) {
  return std::shared_ptr<JitEngine>{new JitEngine(opts)};
}

std::string JitEngine::GetTargetCPU() const { return impl().GetTargetCPU(); }
std::string JitEngine::GetTargetTriple() const { return impl().GetTargetTriple(); }
void JitEngine::Compile(const std::string& name, const Expr& expression) {
  impl().Compile(name, expression);
}

std::string JitEngine::CompileIR(const std::string& name, const Expr& expression) {
  return impl().CompileIR(name, expression);
}

DenseEvalFn JitEngine::LookupUserQuery(const std::string& query_name) {
  return impl().LookupUserQuery(query_name);
}

DenseEvalPopCountFn JitEngine::LookupUserPopCountQuery(const std::string& query_name) {
  return impl().LookupUserPopCountQuery(query_name);
}

}  // namespace query
}  // namespace jitmap
