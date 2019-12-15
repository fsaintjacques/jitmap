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

#include "jitmap/query/compiler.h"
#include "jitmap/query/query.h"
#include "jitmap/query/type_traits.h"

namespace jitmap {
namespace query {

// Utility to partition function arguments into (inputs, output) pair. The
// first `n - 1` arguments are input and the last is output.
using InputsOutputArguments = std::pair<std::vector<llvm::Argument*>, llvm::Argument*>;
InputsOutputArguments PartitionFunctionArguments(llvm::Function* fn);

// Generate the hot section of the loop. Takes an expression and reduce it to a
// single (scalar or vector) value.
struct ExprCodeGenVisitor {
  llvm::Value* operator()(const VariableExpr& e) { return FindBitmapByName(e.value()); }

  llvm::Value* operator()(const EmptyBitmapExpr& e) {
    return llvm::ConstantInt::get(vector_type, 0UL);
  }

  llvm::Value* operator()(const FullBitmapExpr& e) {
    return llvm::ConstantInt::get(vector_type, UINT64_MAX);
  }

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

  llvm::Value* FindBitmapByName(const std::string& name) {
    auto result = bitmaps.find(name);
    if (result == bitmaps.end())
      throw CompilerException("Referenced bitmap '", name, "' not found.");
    return result->second;
  }

  std::unordered_map<std::string, llvm::Value*>& bitmaps;
  llvm::IRBuilder<>& builder;
  llvm::Type* vector_type;
};

class QueryCompiler::Impl {
 public:
  Impl(const std::string& module_name, CompilerOptions options)
      : ctx_(),
        builder_(ctx_),
        module_(module_name, ctx_),
        options_(std::move(options)) {}

  llvm::Function* Compile(const Query& query) { return FunctionDeclForQuery(query); }
  llvm::Module* Module() { return &module_; }

 private:
  void FunctionCodeGen(const Query& query, llvm::Function* fn) {
    auto entry_block = llvm::BasicBlock::Create(ctx_, "entry", fn);
    builder_.SetInsertPoint(entry_block);

    // Constants
    auto induction_type = llvm::Type::getInt64Ty(ctx_);
    auto zero = llvm::ConstantInt::get(induction_type, 0);
    auto step = llvm::ConstantInt::get(induction_type, vector_width());
    auto n_words = llvm::ConstantInt::get(induction_type, word_size());

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

      LoopBodyCodeGen(query, fn, i);

      // i += step
      auto next_i = builder_.CreateAdd(i, step, "next_i");
      // if (`i` == n_words) break;
      auto exit_cond = builder_.CreateICmpEQ(next_i, n_words, "exit_cond");
      builder_.CreateCondBr(exit_cond, after_block, loop_block);
      i->addIncoming(next_i, loop_block);
    }

    builder_.SetInsertPoint(after_block);
    builder_.CreateRetVoid();
  }

  void LoopBodyCodeGen(const Query& query, llvm::Function* fn, llvm::Value* loop_idx) {
    auto [inputs, output] = PartitionFunctionArguments(fn);

    // Load scalar at index for given bitmap
    auto load_vector_inst = [&](auto bitmap_addr, size_t i) {
      auto namify = [&i](std::string key) { return key + "_" + std::to_string(i); };
      // Compute the address to load
      auto gep = builder_.CreateInBoundsGEP(bitmap_addr, {loop_idx}, namify("gep"));
      // Cast previous address as a vector-type.
      auto bitcast = builder_.CreateBitCast(gep, VectorPtrType(), namify("bitcast"));
      // Load in a register
      return builder_.CreateLoad(bitcast, namify("load"));
    };

    std::vector<llvm::Value*> bitmaps;
    for (size_t i = 0; i < inputs.size(); i++) {
      bitmaps.push_back(load_vector_inst(inputs[i], i));
    }

    std::unordered_map<std::string, llvm::Value*> keyed_bitmaps;
    const auto& parameters = query.parameters();
    for (size_t i = 0; i < bitmaps.size(); i++) {
      keyed_bitmaps.emplace(parameters[i], bitmaps[i]);
    }

    ExprCodeGenVisitor visitor{keyed_bitmaps, builder_, VectorType()};
    auto result = query.expr().Visit(visitor);

    auto gep = builder_.CreateInBoundsGEP(output, {loop_idx}, "gep_output");
    auto bitcast = builder_.CreateBitCast(gep, VectorPtrType(), "bitcast_output");
    builder_.CreateStore(result, bitcast);
  }

  llvm::FunctionType* FunctionTypeForArguments(size_t n_arguments) {
    std::vector<llvm::Type*> argument_types{n_arguments + 1, ElementPtrType()};
    auto return_type = llvm::Type::getVoidTy(ctx_);
    constexpr bool is_var_args = false;
    return llvm::FunctionType::get(return_type, argument_types, is_var_args);
  }

  llvm::Function* FunctionDeclForQuery(const Query& query) {
    size_t n_arguments = query.parameters().size();
    auto query_name = query.name();

    auto fn_type = FunctionTypeForArguments(n_arguments);

    // The generated function will be exposed as an external symbol, i.e the
    // symbol will be globally visible. This would be equivalent to defining a
    // symbol with the `extern` storage classifier.
    auto linkage = llvm::Function::ExternalLinkage;
    auto fn = llvm::Function::Create(fn_type, linkage, query_name, &module_);

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

    FunctionCodeGen(query, fn);

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

  uint8_t vector_width() const { return options_.vector_width_; }
  uint8_t scalar_width() const { return options_.scalar_width_; }
  uint32_t word_size() const { return kBitsPerContainer / scalar_width(); }

  llvm::LLVMContext ctx_;
  llvm::IRBuilder<> builder_;
  llvm::Module module_;
  CompilerOptions options_;
};

QueryCompiler::QueryCompiler(const std::string& module_name, CompilerOptions options)
    : impl_(std::make_unique<QueryCompiler::Impl>(module_name, std::move(options))) {}

llvm::Function* QueryCompiler::Compile(const Query& query) {
  return impl_->Compile(query);
}

std::string QueryCompiler::DumpIR() const {
  std::string buffer;
  llvm::raw_string_ostream ss{buffer};
  impl_->Module()->print(ss, nullptr);
  return ss.str();
}

QueryCompiler::~QueryCompiler() {}

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

}  // namespace query
}  // namespace jitmap
