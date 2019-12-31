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

#include "jitmap/query/optimizer.h"

#include "jitmap/query/expr.h"
#include "jitmap/query/type_traits.h"

namespace jitmap {
namespace query {

auto kConstantMatcher = TypeMatcher{Expr::EMPTY_LITERAL, Expr::FULL_LITERAL};

auto kConstantOperandMatcher = OperandMatcher{&kConstantMatcher};

ConstantFolding::ConstantFolding(ExprBuilder* builder)
    : OptimizationPass(&kConstantOperandMatcher, builder) {}

Expr* ConstantFolding::Rewrite(const Expr& expr) {
  return expr.Visit([&](const auto* e) -> Expr* {
    using E = std::decay_t<std::remove_pointer_t<decltype(e)>>;

    auto builder = this->builder_;

    // Not(0) -> 1
    // Not(1) -> 0
    if constexpr (is_not_op<E>::value) {
      auto constant = e->operand();
      auto type = constant->type();

      if (type == Expr::EMPTY_LITERAL) return builder->FullBitmap();
      if (type == Expr::FULL_LITERAL) return builder->EmptyBitmap();
    }

    if constexpr (is_binary_op<E>::value) {
      // Returns a pair where the first element is the constant operand.
      auto unpack_const_expr = [](const BinaryOpExpr* expr) -> std::pair<Expr*, Expr*> {
        auto left = expr->left_operand();
        auto right = expr->right_operand();
        if (left->IsLiteral()) return {left, right};
        return {right, left};
      };

      auto [constant, e_] = unpack_const_expr(e);
      auto type = constant->type();

      // And(0, e) -> 0
      // And(1, e) -> e
      if constexpr (is_and_op<E>::value) {
        if (type == Expr::EMPTY_LITERAL) return builder->EmptyBitmap();
        if (type == Expr::FULL_LITERAL) return e_;
      }

      // Or(0, e) -> e
      // Or(1, e) -> 1
      if constexpr (is_or_op<E>::value) {
        if (type == Expr::EMPTY_LITERAL) return e_;
        if (type == Expr::FULL_LITERAL) return builder->FullBitmap();
      }

      // Xor(0, e) -> e
      // Xor(1, e) -> Not(e)
      if constexpr (is_xor_op<E>::value) {
        if (type == Expr::EMPTY_LITERAL) return e_;
        if (type == Expr::FULL_LITERAL) return builder->Not(e_);
      }
    }

    return kNoOptimizationPerformed;
  });
}

class SameOperandMatcher final : public Matcher {
 public:
  bool Match(const Expr& expr) const {
    return expr.Visit([](const auto* e) {
      using E = std::decay_t<std::remove_pointer_t<decltype(e)>>;
      if constexpr (is_binary_op<E>::value) {
        return *e->left_operand() == *e->right_operand();
      }
      return false;
    });
  }
} kSameOperandMatcher;

SameOperandFolding::SameOperandFolding(ExprBuilder* builder)
    : OptimizationPass(&kSameOperandMatcher, builder) {}

Expr* SameOperandFolding::Rewrite(const Expr& expr) {
  return expr.Visit([&](const auto* e) -> Expr* {
    using E = std::decay_t<std::remove_pointer_t<decltype(e)>>;

    // And(e, e) -> e
    if constexpr (is_and_op<E>::value) {
      return e->left_operand();
    }

    // Or(e, e) -> e
    if constexpr (is_or_op<E>::value) {
      return e->left_operand();
    }

    // Xor(e, e) -> 0
    if constexpr (is_xor_op<E>::value) {
      return this->builder_->EmptyBitmap();
    }

    return kNoOptimizationPerformed;
  });
}

TypeMatcher kNotMatcher{Expr::NOT_OPERATOR};
OperandMatcher kNotOperandMatcher{&kNotMatcher};
ChainMatcher kNotNotOperandMatcher{&kNotMatcher, &kNotOperandMatcher};

NotChainFolding::NotChainFolding(ExprBuilder* builder)
    : OptimizationPass(&kNotNotOperandMatcher, builder) {}

Expr* NotChainFolding::Rewrite(const Expr& expr) {
  return expr.Visit([&](const auto* e) -> Expr* {
    using E = std::decay_t<std::remove_pointer_t<decltype(e)>>;

    if constexpr (is_not_op<E>::value) {
      size_t count = 1;
      auto operand = e->operand();

      while (operand->type() == Expr::NOT_OPERATOR) {
        count++;
        operand = (static_cast<NotOpExpr*>(operand)->operand());
      }

      return (count % 2) ? this->builder_->Not(operand) : operand;
    }

    return kNoOptimizationPerformed;
  });
}

Optimizer::Optimizer(ExprBuilder* builder, OptimizerOptions options)
    : builder_(builder), options_(options) {
  if (options.HasOptimization(OptimizerOptions::CONSTANT_FOLDING)) {
    constant_folding_ = ConstantFolding{builder_};
  }

  if (options.HasOptimization(OptimizerOptions::SAME_OPERAND_FOLDING)) {
    same_operand_folding_ = SameOperandFolding{builder_};
  }

  if (options.HasOptimization(OptimizerOptions::NOT_CHAIN_FOLDING)) {
    not_chain_folding_ = NotChainFolding{builder_};
  }
}

// Apply optimizations in a bottom-up fashion, i.e. visit children before parents.
template <typename Visitor>
struct BottonUpVisitor {
  Expr* operator()(VariableExpr* e) { return visitor(e); }

  template <typename E>
  enable_if_literal<E, Expr*> operator()(E* e) {
    return visitor(e);
  }

  template <typename E>
  enable_if_unary_op<E, Expr*> operator()(E* op) {
    op->SetOperand((op->operand()->Visit(*this)));
    return visitor(op);
  }

  template <typename E>
  enable_if_binary_op<E, Expr*> operator()(E* op) {
    op->SetLeftOperand(op->left_operand()->Visit(*this));
    op->SetRightOperand(op->right_operand()->Visit(*this));
    return visitor(op);
  }

  Visitor visitor;
};

Expr* Optimizer::Optimize(const Expr& input) {
  auto expr = input.Copy(builder_);

  // Apply folding optimizations
  auto folder = [this](Expr* e) {
    if (this->constant_folding_) e = this->constant_folding_.value()(e);
    if (this->same_operand_folding_) e = this->same_operand_folding_.value()(e);
    if (this->not_chain_folding_) e = this->not_chain_folding_.value()(e);
    return e;
  };
  BottonUpVisitor<decltype(folder)> folders{std::move(folder)};

  return expr->Visit(folders);
}

}  // namespace query
}  // namespace jitmap
