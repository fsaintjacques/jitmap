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

#include <optional>

#include <jitmap/query/matcher.h>

namespace jitmap {
namespace query {

class Expr;
class ExprBuilder;

class OptimizationPass {
 public:
  // Indicate that no optimization was done, see `Rewrite`.
  static constexpr Expr* kNoOptimizationPerformed = nullptr;

  OptimizationPass(Matcher* matcher, ExprBuilder* builder)
      : matcher_(matcher), builder_(builder) {}

  bool Match(const Expr& expr) const { return matcher_->Match(expr); }

  // Rewrite an expression into an (hopefully) optimized expression.
  //
  // \param[in] expr Expression to simplify.
  //
  // \return The optimized expression or the `kNoOptimizationPerformed` token
  //         in case of failure.
  virtual Expr* Rewrite(const Expr& expr) = 0;

  // Evaluate the pass on an expression.
  //
  // This is syntactic sugar for `(opt->Match(e)) ? opt->Rewrite(e) : e`
  //
  // \param[in] expr Expression to simplify
  //
  // \return The optimized expression if possible or the original
  //         expression in the case where the expression doesn't pass the
  //         matcher or the rewrite step failed.
  Expr* operator()(Expr* expr) {
    if (!Match(*expr)) return expr;
    auto simplified = Rewrite(*expr);
    return (simplified != kNoOptimizationPerformed) ? simplified : expr;
  }

  virtual ~OptimizationPass() = default;

 protected:
  Matcher* matcher_;
  ExprBuilder* builder_;
};

// Not(0) -> 1
// Not(1) -> 0
// And(0, e) -> 0
// And(1, e) -> e
// Or(0, e)  -> e
// Or(1, e)  -> 1
// Xor(0, e) -> e
// Xor(1, e) -> Not(e)
class ConstantFolding final : public OptimizationPass {
 public:
  explicit ConstantFolding(ExprBuilder* builder);

  Expr* Rewrite(const Expr& expr) override;
};

// And(e, e) -> e
// Or(e, e)  -> e
// Xor(e, e) -> 0
class SameOperandFolding final : public OptimizationPass {
 public:
  explicit SameOperandFolding(ExprBuilder* builder);

  Expr* Rewrite(const Expr& expr) override;
};

// Not(Not(Not...(e)...))) -> e or Not(e) depending on occurence
class NotChainFolding final : public OptimizationPass {
 public:
  explicit NotChainFolding(ExprBuilder* builder);

  Expr* Rewrite(const Expr& expr) override;
};

struct OptimizerOptions {
  enum EnabledOptimizations : uint64_t {
    CONSTANT_FOLDING = 1U << 1,
    SAME_OPERAND_FOLDING = 1U << 2,
    NOT_CHAIN_FOLDING = 1U << 3,
  };

  bool HasOptimization(enum EnabledOptimizations optimization) {
    return (enabled_optimizations & optimization) != 0;
  }

  static constexpr uint64_t kDefaultOptimizations =
      CONSTANT_FOLDING | SAME_OPERAND_FOLDING | NOT_CHAIN_FOLDING;

  uint64_t enabled_optimizations = kDefaultOptimizations;
};

class Optimizer {
 public:
  explicit Optimizer(ExprBuilder* builder, OptimizerOptions options = {});

  const OptimizerOptions& options() const { return options_; }

  Expr* Optimize(const Expr& input);

 private:
  ExprBuilder* builder_;
  OptimizerOptions options_;

  std::optional<ConstantFolding> constant_folding_;
  std::optional<SameOperandFolding> same_operand_folding_;
  std::optional<NotChainFolding> not_chain_folding_;
};

}  // namespace query
}  // namespace jitmap
