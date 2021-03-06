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

#include <bitset>
#include <utility>
#include <vector>

#include <jitmap/query/expr.h>

namespace jitmap {
namespace query {

// The Matcher interface is used to recognize patterns in expressions.
class Matcher {
 public:
  virtual bool Match(const Expr& expr) const = 0;
  bool Match(Expr* expr) const {
    return (expr != nullptr) && Match(*expr);
  }

  bool operator()(const Expr& expr) const { return Match(expr); }
  bool operator()(Expr* expr) const { return Match(expr); }

  virtual ~Matcher() = default;
};

// TypeMatcher matches Expression of given types.
class TypeMatcher final : public Matcher {
 public:
  using Type = Expr::Type;

  // Construct a TypeMatcher matching a single type.
  explicit TypeMatcher(Type type);
  // Construct a TypeMatcher matching a set of types.
  explicit TypeMatcher(const std::vector<Expr::Type>& types);
  TypeMatcher(const std::initializer_list<Expr::Type>& types);

  bool Match(const Expr& expr) const override;

 private:
  std::bitset<8> mask_;
};

// OperandMatcher applies a matcher to an operator's operand(s).
class OperandMatcher final : public Matcher {
 public:
  // The Mode is relevant to operators with more than one operand. It
  // indicates if the match should apply to one (ANY) or all (ALL)
  // operands.
  enum Mode {
    // Any (at least one), operand must match
    ANY,
    // All operand must match
    ALL,
  };

  explicit OperandMatcher(Matcher* operand_matcher, Mode mode = ANY);

  bool Match(const Expr& expr) const override;

 private:
  Matcher* matcher_;
  Mode mode_;
};

// ChainMatcher applies many matchers in an ordered fashion. The
// short-circuiting is dictated by the mode.
class ChainMatcher final : public Matcher {
 public:
  // Indicate if all or at-least one of the matchers must match.
  enum Mode {
    // Any (at least one), operand must match
    ANY,
    // All operand must match
    ALL,
  };

  explicit ChainMatcher(const std::vector<Matcher*>& matchers, Mode mode = ALL);
  ChainMatcher(const std::initializer_list<Matcher*>& matchers, Mode mode = ALL);

  bool Match(const Expr& expr) const override;

 private:
  std::vector<Matcher*> matchers_;
  Mode mode_;
};

}  // namespace query
}  // namespace jitmap
