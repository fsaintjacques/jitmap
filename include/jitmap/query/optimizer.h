#pragma once

#include <jitmap/query/expr.h>
#include <jitmap/query/matcher.h>

namespace jitmap {
namespace query {

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
  ConstantFolding(ExprBuilder* builder);

  Expr* Rewrite(const Expr& expr);
};

// And(e, e) -> e
// Or(e, e)  -> e
// Xor(e, e) -> 0
class SameOperandFolding final : public OptimizationPass {
 public:
  SameOperandFolding(ExprBuilder* builder);

  Expr* Rewrite(const Expr& expr);
};

// Not(Not(Not...(e)...))) -> e or Not(e) depending on occurence
class NotChainFolding final : public OptimizationPass {
 public:
  NotChainFolding(ExprBuilder* builder);

  Expr* Rewrite(const Expr& expr);
};

class Optimizer {
 public:
  struct Options {
    enum Flags : uint64_t {
      CONSTANT_FOLDING = 1U << 1,
      SAME_OPERAND_FOLDING = 1U << 2,
      NOT_CHAIN_FOLDING = 1U << 3,
    };
  };

  Expr* Optimize(const Expr& input);
};

}  // namespace query
}  // namespace jitmap
