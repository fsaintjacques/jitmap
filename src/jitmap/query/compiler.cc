#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>

#include "jitmap/query/query.h"
#include "jitmap/query/type_traits.h"

namespace jitmap {
namespace query {

// Utility to partition function arguments into (inputs, output) pair. The
// first `n - 1` arguments are input and the last is output.
using InputsOutputArguments = std::pair<std::vector<llvm::Argument*>, llvm::Argument*>;
InputsOutputArguments PartitionFunctionArguments(llvm::Function* fn);

struct CompilerOptions {
  uint64_t word_size() const { return kBitsPerContainer / scalar_width_; }

  uint8_t vector_width_ = 4;
  uint8_t scalar_width_ = 64;
};

struct ExprCodeGenVisitor {
  llvm::Value* operator()(const IndexRefExpr& e) { return bitmaps[e.value()]; }
  llvm::Value* operator()(const NamedRefExpr& e) { return nullptr; }
  llvm::Value* operator()(const EmptyBitmapExpr& e) { return nullptr; }
  llvm::Value* operator()(const FullBitmapExpr& e) { return nullptr; }

  llvm::Value* operator()(const NotOpExpr& e) {
    auto operand = e.operand()->Visit(*this);
    return builder.CreateNot(operand);
  }

  std::pair<llvm::Value*, llvm::Value*> VisitBinary(const BinaryOpExpr& e) {
    return {e.left_operand()->Visit(*this), e.right_operand()->Visit(*this)};
  }

  llvm::Value* operator()(const AndOpExpr& e) {
    auto [lhs, rhs] = VisitBinary(e);
    return builder.CreateAnd(lhs, rhs);
  }

  llvm::Value* operator()(const OrOpExpr& e) {
    auto [lhs, rhs] = VisitBinary(e);
    return builder.CreateOr(lhs, rhs);
  }

  llvm::Value* operator()(const XorOpExpr& e) {
    auto [lhs, rhs] = VisitBinary(e);
    return builder.CreateXor(lhs, rhs);
  }

  std::vector<llvm::Value*>& bitmaps;
  llvm::IRBuilder<>& builder;
};

class QueryCompiler {
 public:
  QueryCompiler(Query& query)
      : query_(query), ctx_(), builder_(ctx_), module_(query_.name(), ctx_) {}

  void Compile() {
    module_.setSourceFileName(query_.expr().ToString());
    auto fn = FunctionDeclForQuery(query_.parameters().size(), query_.name(), &module_);
    auto entry_block = llvm::BasicBlock::Create(ctx_, "entry", fn);
    builder_.SetInsertPoint(entry_block);

    LoopCodeGen(fn);

    builder_.CreateRetVoid();
  }

  std::string DebugIR() const {
    std::string buffer;
    llvm::raw_string_ostream ss{buffer};
    module_.print(ss, nullptr);
    return ss.str();
  }

 private:
  void LoopCodeGen(llvm::Function* fn) {
    // Constants
    auto induction_type = llvm::Type::getInt64Ty(ctx_);
    auto zero = llvm::ConstantInt::get(induction_type, 0);
    auto step = llvm::ConstantInt::get(induction_type, vector_width());
    auto n_words = llvm::ConstantInt::get(induction_type, word_size());

    auto entry_block = builder_.GetInsertBlock();
    auto loop_block = llvm::BasicBlock::Create(ctx_, "loop", fn);
    auto after_block = llvm::BasicBlock::Create(ctx_, "after_loop", fn);

    builder_.CreateBr(loop_block);
    builder_.SetInsertPoint(loop_block);

    // The following block is equivalent to
    // for (int i = 0; i < n_words ; i += step) {
    //   LoopBodyCodeGen(fn, i)
    // }
    {
      // Define the `i` induction variable and initialize it to zero.
      auto i = builder_.CreatePHI(induction_type, 2, "i");
      i->addIncoming(zero, entry_block);

      LoopBodyCodeGen(fn, i);

      // i += step
      auto next_i = builder_.CreateAdd(i, step, "next_i");
      // if (`i` == n_words) break;
      auto exit_cond = builder_.CreateICmpEQ(next_i, n_words, "exit_cond");
      builder_.CreateCondBr(exit_cond, after_block, loop_block);
      i->addIncoming(next_i, loop_block);
    }

    builder_.SetInsertPoint(after_block);
  }

  void LoopBodyCodeGen(llvm::Function* fn, llvm::Value* loop_var) {
    auto [inputs, output] = PartitionFunctionArguments(fn);

    auto namify = [](std::string key, size_t i) { return key + "_" + std::to_string(i); };

    auto load_vector_inst = [&](auto input, size_t i) {
      auto gep = builder_.CreateInBoundsGEP(input, {loop_var}, namify("gep", i));
      // Cast previous reference as a vector-type.
      auto bitcast = builder_.CreateBitCast(gep, VectorPtrType(), namify("bitcast", i));
      // Load in a vector register
      return builder_.CreateLoad(bitcast, namify("load", i));
    };

    std::vector<llvm::Value*> bitmaps;
    for (size_t i = 0; i < inputs.size(); i++) {
      bitmaps.push_back(load_vector_inst(inputs[i], i));
    }

    auto result = ExprCodeGen(bitmaps);

    auto gep = builder_.CreateInBoundsGEP(output, {loop_var}, "gep_output");
    auto bitcast = builder_.CreateBitCast(gep, VectorPtrType(), "bitcast_output");
    builder_.CreateStore(result, bitcast);
  }

  llvm::Value* ExprCodeGen(std::vector<llvm::Value*>& bitmaps) {
    return query_.expr().Visit(ExprCodeGenVisitor{bitmaps, builder_});
  }

  llvm::FunctionType* FunctionTypeForArguments(size_t n_arguments) {
    std::vector<llvm::Type*> argument_types{n_arguments + 1, ElementPtrType()};
    auto return_type = llvm::Type::getVoidTy(ctx_);
    constexpr bool is_var_args = false;
    return llvm::FunctionType::get(return_type, argument_types, is_var_args);
  }

  llvm::Function* FunctionDeclForQuery(size_t n_arguments, const std::string& query_name,
                                       llvm::Module* module) {
    auto fn_type = FunctionTypeForArguments(n_arguments);

    // The generated function will be exposed as an external symbol, i.e the
    // symbol will be globally visible. This would be equivalent to defining a
    // symbol with the `extern` storage classifier.
    auto linkage = llvm::Function::ExternalLinkage;
    auto fn = llvm::Function::Create(fn_type, linkage, query_name, module);

    // The generated objects are accessed by taking the symbol address and
    // casting to a function type. Thus, we must use the C calling convention.
    fn->setCallingConv(llvm::CallingConv::C);
    // The function will only access memory pointed by the parameter pointers.
    fn->addFnAttr(llvm::Attribute::ArgMemOnly);

    auto [inputs, output] = PartitionFunctionArguments(fn);
    for (auto input : inputs) {
      input->setName("in");
      // The NoCapture attribute indicates that the bitmap pointer
      // will not be captured (leak outside the function).
      input->addAttr(llvm::Attribute::NoCapture);
      input->addAttr(llvm::Attribute::ReadOnly);
    }

    output->setName("out");
    output->addAttr(llvm::Attribute::NoCapture);

    return fn;
  }

  void SetModuleAttribute(llvm::Module* module, std::string key, std::string value) {
    using llvm::MDNode;
    using llvm::MDString;
    auto named_metadata = module->getOrInsertNamedMetadata("jitmap." + key);
    named_metadata->addOperand(MDNode::get(ctx_, {MDString::get(ctx_, value)}));
  }

  llvm::Type* ElementType() { return llvm::Type::getIntNTy(ctx_, scalar_width()); }
  llvm::Type* ElementPtrType() { return ElementType()->getPointerTo(); }
  llvm::Type* VectorType() {
    // Reverts to scalar for debugging purposes.
    if (vector_width() == 1) return ElementType();
    return llvm::VectorType::get(ElementType(), vector_width());
  }
  llvm::Type* VectorPtrType() { return VectorType()->getPointerTo(); }

  uint32_t word_size() const { return options_.word_size(); }
  uint8_t vector_width() const { return options_.vector_width_; }
  uint8_t scalar_width() const { return options_.scalar_width_; }

  Query& query_;
  llvm::LLVMContext ctx_;
  llvm::IRBuilder<> builder_;
  llvm::Module module_;
  CompilerOptions options_;
};

InputsOutputArguments PartitionFunctionArguments(llvm::Function* fn) {
  InputsOutputArguments io;

  size_t arg_size = fn->arg_size();
  size_t i = 0;
  for (auto& argument : fn->args()) {
    if (i++ < arg_size - 1) {
      io.first.push_back(&argument);
    } else {
      io.second = &argument;
    }
  }

  return io;
}

std::string Compile(Query& query) {
  QueryCompiler compiler(query);
  compiler.Compile();
  return compiler.DebugIR();
};

}  // namespace query
}  // namespace jitmap
