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
  return expr.Visit([&](const auto* e) {
    using E = std::decay_t<std::remove_pointer_t<decltype(e)>>;

    auto mode = this->mode_;
    auto& matcher = *this->matcher_;

    if constexpr (is_not_op<E>::value) {
      return matcher(e->operand());
    }

    if constexpr (is_binary_op<E>::value) {
      bool left = matcher(e->left_operand());

      // Short-circuit
      if (left && mode == Mode::ANY) return true;
      if (!left && mode == Mode::ALL) return false;

      bool right = matcher(e->right_operand());
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

  throw Exception("Unknown match type: ", mode_);
}

}  // namespace query
}  // namespace jitmap
