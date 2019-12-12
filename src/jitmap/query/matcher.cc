#include <algorithm>

#include "jitmap/query/matcher.h"
#include "jitmap/query/type_traits.h"

namespace jitmap {
namespace query {

TypeMatcher::TypeMatcher(Expr::Type type) { mask_[type] = true; }

TypeMatcher::TypeMatcher(const std::vector<Type>& types) {
  for (auto t : types) mask_[t] = true;
}

TypeMatcher::TypeMatcher(const std::initializer_list<Type>& types) {
  for (auto t : types) mask_[t] = true;
}

bool TypeMatcher::Match(const Expr& expr) const { return mask_[expr.type()]; }

OperandMatcher::OperandMatcher(Matcher* matcher, Mode mode)
    : matcher_(matcher), mode_(mode) {}

bool OperandMatcher::Match(const Expr& expr) const {
  return expr.Visit([&](const auto& e) {
    using E = std::decay_t<decltype(e)>;

    auto mode = this->mode_;
    auto& matcher = *this->matcher_;

    if constexpr (is_unary_op<E>::value) {
      return matcher(static_cast<const NotOpExpr&>(e).operand());
    }

    if constexpr (is_binary_op<E>::value) {
      bool left = matcher(e.left_operand());

      // Short-circuit
      if (left && mode == Mode::ANY) return true;
      if (!left && mode == Mode::ALL) return false;

      bool right = matcher(e.right_operand());
      return (mode == Mode::ANY) ? left || right : left && right;
    }

    return false;
  });
}

ChainMatcher::ChainMatcher(const std::vector<Matcher*>& matchers, Mode mode)
    : matchers_(matchers), mode_(mode) {}

ChainMatcher::ChainMatcher(const std::initializer_list<Matcher*>& matchers, Mode mode)
    : matchers_(matchers), mode_(mode) {}

bool ChainMatcher::Match(const Expr& expr) const {
  auto match = [&](const Matcher* m) { return m->Match(expr); };

  switch (mode_) {
    case ALL:
      return std::all_of(matchers_.cbegin(), matchers_.cend(), match);
    case ANY:
      return std::any_of(matchers_.cbegin(), matchers_.cend(), match);
  }
}

}  // namespace query
}  // namespace jitmap
