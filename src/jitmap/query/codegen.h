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

#pragma once

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
#include "jitmap/query/expr.h"
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

  llvm::Value* operator()(const VariableExpr* e) { return FindBitmapByName(e->value()); }

  llvm::Value* operator()(const EmptyBitmapExpr*) {
    return llvm::ConstantInt::get(vector_type, 0UL);
  }

  llvm::Value* operator()(const FullBitmapExpr*) {
    return llvm::ConstantInt::get(vector_type, UINT64_MAX);
  }

  llvm::Value* operator()(const NotOpExpr* e) {
    auto operand = e->operand()->Visit(*this);
    return builder.CreateNot(operand);
  }

  llvm::Value* operator()(const AndOpExpr* e) {
    auto [lhs, rhs] = VisitBinary(e);
    return builder.CreateAnd(lhs, rhs);
  }

  llvm::Value* operator()(const OrOpExpr* e) {
    auto [lhs, rhs] = VisitBinary(e);
    return builder.CreateOr(lhs, rhs);
  }

  llvm::Value* operator()(const XorOpExpr* e) {
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

  std::pair<llvm::Value*, llvm::Value*> VisitBinary(const BinaryOpExpr* e) {
    return {e->left_operand()->Visit(*this), e->right_operand()->Visit(*this)};
  }
};

class ExpressionCodeGen {
 public:
  explicit ExpressionCodeGen(const std::string& module_name)
      : ctx_(std::make_unique<llvm::LLVMContext>()),
        module_(std::make_unique<llvm::Module>(module_name, *ctx_)),
        builder_(*ctx_) {}

  ExpressionCodeGen& Compile(const std::string& name, const Expr& expression,
                             bool with_popcount = true) {
    auto fn = FunctionDeclForQuery(name, expression, with_popcount);
    FunctionCodeGen(expression, with_popcount, fn);
    return *this;
  }

  using ContextAndModule =
      std::pair<std::unique_ptr<llvm::LLVMContext>, std::unique_ptr<llvm::Module>>;

  ContextAndModule Finish() { return {std::move(ctx_), std::move(module_)}; }

 private:
  void FunctionCodeGen(const Expr& expression, bool with_popcount, llvm::Function* fn) {
    auto entry_block = llvm::BasicBlock::Create(*ctx_, "entry", fn);
    builder_.SetInsertPoint(entry_block);

    auto variables = expression.Variables();
    // Load bitmaps addresses
    auto [inputs, output] = UnrollInputsOutput(variables.size(), fn);

    // Constants
    auto i64 = llvm::Type::getInt64Ty(*ctx_);
    auto zero = llvm::ConstantInt::get(i64, 0);
    auto step = llvm::ConstantInt::get(i64, 1);
    auto n_words = llvm::ConstantInt::get(i64, words());

    auto loop_block = llvm::BasicBlock::Create(*ctx_, "loop", fn);
    auto after_block = llvm::BasicBlock::Create(*ctx_, "after_loop", fn);

    llvm::PHINode* acc = nullptr;
    llvm::Value* next_acc = nullptr;

    builder_.CreateBr(loop_block);
    builder_.SetInsertPoint(loop_block);

    // The following block is equivalent to
    // for (int i = 0; i < n_words ; i += step) {
    //   LoopBodyCodeGen(fn, i)
    // }
    {
      // Define the `i` induction variable and initialize it to zero.
      auto i = builder_.CreatePHI(i64, 2, "i");
      i->addIncoming(zero, entry_block);

      if (with_popcount) {
        // Initialize an accumulator for popcount.
        auto zero_elem = llvm::ConstantInt::get(ElementType(), 0);
        auto zero_vec = llvm::ConstantVector::getSplat(vector_width(), zero_elem);
        acc = builder_.CreatePHI(VectorType(), 2, "acc");
        acc->addIncoming(zero_vec, entry_block);
      }

      auto result = LoopBodyCodeGen(expression, variables, inputs, output, i);

      if (with_popcount) {
        auto popcnt = PopCount(result);
        next_acc = builder_.CreateAdd(acc, popcnt, "next_acc");
        acc->addIncoming(next_acc, loop_block);
      }

      // i += step
      auto next_i = builder_.CreateAdd(i, step, "next_i");

      // if (`i` == n_words) break;
      auto exit_cond = builder_.CreateICmpEQ(next_i, n_words, "exit_cond");
      builder_.CreateCondBr(exit_cond, after_block, loop_block);
      i->addIncoming(next_i, loop_block);
    }

    builder_.SetInsertPoint(after_block);
    // Return the horizontal sum of the vector accumulator.
    if (with_popcount) {
      builder_.CreateRet(ReduceAdd(next_acc));
    } else {
      builder_.CreateRetVoid();
    }
  }

  llvm::Value* PopCount(llvm::Value* val) {
    // See https://reviews.llvm.org/D10084
    constexpr auto ctpop = llvm::Intrinsic::ctpop;
    return builder_.CreateUnaryIntrinsic(ctpop, val, nullptr, "popcnt");
  }

  llvm::Value* ReduceAdd(llvm::Value* val) {
    constexpr auto horizontal_add = llvm::Intrinsic::experimental_vector_reduce_add;
    return builder_.CreateUnaryIntrinsic(horizontal_add, val, nullptr, "hsum");
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
      auto addr = builder_.CreateLoad(gep, namify("bitmap"));
      return builder_.CreatePointerCast(addr, VectorPtrType(), namify("bitmap_vec"));
    };

    std::vector<llvm::Value*> inputs;
    for (size_t i = 0; i < n_bitmaps; i++) {
      inputs.push_back(load_bitmap(i));
    }

    auto output_vec_ptr =
        builder_.CreatePointerCast(output_ptr, VectorPtrType(), "output_vec");
    return {inputs, output_vec_ptr};
  }

  llvm::Value* LoopBodyCodeGen(const Expr& expression,
                               const std::vector<std::string>& variables,
                               std::vector<llvm::Value*> inputs, llvm::Value* output,
                               llvm::Value* loop_idx) {
    // Load scalar at index for given bitmap
    auto load_vector_inst = [&](auto bitmap_addr, size_t i) {
      auto namify = [&i](std::string key) { return key + "_" + std::to_string(i); };
      // Compute the address to load
      auto gep = builder_.CreateInBoundsGEP(bitmap_addr, {loop_idx}, namify("gep"));
      // Load in a register
      return builder_.CreateLoad(gep, namify("load"));
    };

    // Bind the variable bitmaps by name to inputs of the function
    const auto& parameters = variables;
    std::unordered_map<std::string, llvm::Value*> keyed_bitmaps;
    for (size_t i = 0; i < inputs.size(); i++) {
      keyed_bitmaps.emplace(parameters[i], load_vector_inst(inputs[i], i));
    }

    // Execute the expression tree on the input
    ExprCodeGenVisitor visitor{keyed_bitmaps, builder_, VectorType()};
    auto result = expression.Visit(visitor);

    // Store the result in the output bitmap.
    auto gep = builder_.CreateInBoundsGEP(output, {loop_idx}, "gep_output");
    auto bitcast = builder_.CreateBitCast(gep, VectorPtrType(), "bitcast_output");
    builder_.CreateStore(result, bitcast);

    return result;
  }

  llvm::FunctionType* FunctionTypeForArguments(bool with_popcount) {
    // void
    auto return_type = with_popcount ? ElementType() : llvm::Type::getVoidTy(*ctx_);
    auto i8 = llvm::Type::getInt8Ty(*ctx_);
    auto i8_ptr = i8->getPointerTo();
    // dense_fn(
    // const int8_t** inputs,
    auto inputs_type = i8_ptr->getPointerTo();
    // int8_t* output,
    auto output_type = i8_ptr;
    // )

    constexpr bool is_var_args = false;
    return llvm::FunctionType::get(return_type, {inputs_type, output_type}, is_var_args);
  }

  llvm::Function* FunctionDeclForQuery(const std::string& name, const Expr& expression,
                                       bool with_popcount) {
    auto fn_type = FunctionTypeForArguments(with_popcount);
    // The generated function will be exposed as an external symbol, i.e the
    // symbol will be globally visible. This would be equivalent to defining a
    // symbol with the `extern` storage classifier.
    auto linkage = llvm::Function::ExternalLinkage;
    auto fn = llvm::Function::Create(fn_type, linkage, name, *module_);

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

    return fn;
  }

  llvm::Type* ElementType() { return llvm::Type::getIntNTy(*ctx_, scalar_width()); }

  llvm::VectorType* VectorType() {
    return llvm::VectorType::get(ElementType(), vector_width());
  }
  llvm::Type* VectorPtrType() { return VectorType()->getPointerTo(); }

  uint32_t scalar_width() const { return 32; }
  uint32_t vector_width() const { return 16; }
  uint32_t unroll() const { return 4; }

  uint32_t words() const { return kBitsPerContainer / (scalar_width() * vector_width()); }

  std::unique_ptr<llvm::LLVMContext> ctx_;
  std::unique_ptr<llvm::Module> module_;
  llvm::IRBuilder<> builder_;
};

}  // namespace query
}  // namespace jitmap
