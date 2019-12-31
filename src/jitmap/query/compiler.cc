#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
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

// Generate the hot section of the loop. Takes an expression and reduce it to a
// single (scalar or vector) value.
struct ExprCodeGenVisitor {
 public:
  std::unordered_map<std::string, llvm::Value*>& bitmaps;
  llvm::IRBuilder<>& builder;
  llvm::Type* vector_type;

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

 private:
  llvm::Value* FindBitmapByName(const std::string& name) {
    auto result = bitmaps.find(name);
    if (result == bitmaps.end())
      throw CompilerException("Referenced bitmap '", name, "' not found.");
    return result->second;
  }

  std::pair<llvm::Value*, llvm::Value*> VisitBinary(const BinaryOpExpr& e) {
    return {e.left_operand()->Visit(*this), e.right_operand()->Visit(*this)};
  }
};

class QueryIRCodeGen::Impl {
 public:
  Impl(const std::string& module_name, CompilerOptions options)
      : ctx_(std::make_unique<llvm::LLVMContext>()),
        module_(std::make_unique<llvm::Module>(module_name, *ctx_)),
        builder_(*ctx_),
        options_(std::move(options)) {}

  void Compile(const Query& query) { FunctionDeclForQuery(query); }

  ModuleAndContext Finish() { return {std::move(module_), std::move(ctx_)}; }

 private:
  void FunctionCodeGen(const Query& query, llvm::Function* fn) {
    auto entry_block = llvm::BasicBlock::Create(*ctx_, "entry", fn);
    builder_.SetInsertPoint(entry_block);

    // Load bitmaps addresses
    auto [inputs, output] = UnrollInputsOutput(query.variables().size(), fn);

    // Constants
    auto induction_type = llvm::Type::getInt64Ty(*ctx_);
    auto zero = llvm::ConstantInt::get(induction_type, 0);
    auto step = llvm::ConstantInt::get(induction_type, 1);
    auto n_words = llvm::ConstantInt::get(induction_type, word_size());

    auto loop_block = llvm::BasicBlock::Create(*ctx_, "loop", fn);
    auto after_block = llvm::BasicBlock::Create(*ctx_, "after_loop", fn);

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

      LoopBodyCodeGen(query, inputs, output, i);

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

  std::pair<std::vector<llvm::Value*>, llvm::Value*> UnrollInputsOutput(
      size_t n_bitmaps, llvm::Function* fn) {
    auto args_it = fn->args().begin();
    auto inputs_ptr = args_it++;
    auto output_ptr = args_it++;

    // Load scalar at index for given bitmap
    auto load_bitmap = [&](size_t i) {
      auto namify = [&i](std::string key) { return key + "_" + std::to_string(i); };
      auto bitmap_i = llvm::ConstantInt::get(llvm::Type::getInt64Ty(*ctx_), i);
      auto gep = builder_.CreateInBoundsGEP(inputs_ptr, bitmap_i, namify("bitmap_gep"));
      return builder_.CreateLoad(gep, namify("bitmap"));
    };

    std::vector<llvm::Value*> inputs;
    for (size_t i = 0; i < n_bitmaps; i++) {
      inputs.push_back(load_bitmap(i));
    }

    return {inputs, output_ptr};
  }

  void LoopBodyCodeGen(const Query& query, std::vector<llvm::Value*> inputs,
                       llvm::Value* output, llvm::Value* loop_idx) {
    // Load scalar at index for given bitmap
    auto load_vector_inst = [&](auto bitmap_addr, size_t i) {
      auto namify = [&i](std::string key) { return key + "_" + std::to_string(i); };
      // Compute the address to load
      auto gep = builder_.CreateInBoundsGEP(bitmap_addr, {loop_idx}, namify("gep"));
      // Load in a register
      return builder_.CreateLoad(gep, namify("load"));
    };

    // Bind the variable bitmaps by name to inputs of the function
    const auto& parameters = query.variables();
    std::unordered_map<std::string, llvm::Value*> keyed_bitmaps;
    for (size_t i = 0; i < inputs.size(); i++) {
      keyed_bitmaps.emplace(parameters[i], load_vector_inst(inputs[i], i));
    }

    // Execute the expression tree on the input
    ExprCodeGenVisitor visitor{keyed_bitmaps, builder_, ElementType()};
    auto result = query.expr().Visit(visitor);

    // Store the result in the output bitmap.
    auto gep = builder_.CreateInBoundsGEP(output, {loop_idx}, "gep_output");
    auto bitcast = builder_.CreateBitCast(gep, ElementPtrType(), "bitcast_output");
    builder_.CreateStore(result, bitcast);
  }

  llvm::FunctionType* FunctionTypeForArguments() {
    // void
    auto return_type = llvm::Type::getVoidTy(*ctx_);
    // dense_fn(
    // const int32_t** inputs,
    auto inputs_type = ElementPtrType()->getPointerTo();
    // int32_t* output,
    auto output_type = ElementPtrType();
    // )

    constexpr bool is_var_args = false;
    return llvm::FunctionType::get(return_type, {inputs_type, output_type}, is_var_args);
  }

  llvm::Function* FunctionDeclForQuery(const Query& query) {
    auto fn_type = FunctionTypeForArguments();
    // The generated function will be exposed as an external symbol, i.e the
    // symbol will be globally visible. This would be equivalent to defining a
    // symbol with the `extern` storage classifier.
    auto linkage = llvm::Function::ExternalLinkage;
    auto fn = llvm::Function::Create(fn_type, linkage, query.name(), *module_);

    // The generated objects are accessed by taking the symbol address and
    // casting to a function type. Thus, we must use the C calling convention.
    fn->setCallingConv(llvm::CallingConv::C);
    // The function will only access memory pointed by the parameter pointers.
    fn->addFnAttr(llvm::Attribute::ArgMemOnly);

    auto args_it = fn->args().begin();
    auto inputs_ptr = args_it++;
    auto output = args_it++;

    inputs_ptr->setName("inputs");
    // The NoCapture attribute indicates that the bitmap pointer
    // will not be captured (leak outside the function).
    inputs_ptr->addAttr(llvm::Attribute::NoCapture);
    inputs_ptr->addAttr(llvm::Attribute::ReadOnly);

    output->setName("output");
    output->addAttr(llvm::Attribute::NoCapture);

    FunctionCodeGen(query, fn);

    return fn;
  }

  llvm::Type* ElementType() { return llvm::Type::getIntNTy(*ctx_, scalar_width()); }
  llvm::Type* ElementPtrType() { return ElementType()->getPointerTo(); }

  uint8_t scalar_width() const { return kBitsPerBitsetWord; }
  uint32_t word_size() const { return kBitsPerContainer / scalar_width(); }

  std::unique_ptr<llvm::LLVMContext> ctx_;
  std::unique_ptr<llvm::Module> module_;
  llvm::IRBuilder<> builder_;
  CompilerOptions options_;
};

QueryIRCodeGen::QueryIRCodeGen(const std::string& module_name, CompilerOptions options)
    : impl_(std::make_unique<QueryIRCodeGen::Impl>(module_name, std::move(options))) {}
QueryIRCodeGen::QueryIRCodeGen(QueryIRCodeGen&& other) { std::swap(impl_, other.impl_); }
QueryIRCodeGen::~QueryIRCodeGen() {}

void QueryIRCodeGen::Compile(const Query& query) { impl_->Compile(query); }

QueryIRCodeGen::ModuleAndContext QueryIRCodeGen::Finish() && { return impl_->Finish(); }

}  // namespace query
}  // namespace jitmap
