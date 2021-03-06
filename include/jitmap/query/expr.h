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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <jitmap/util/exception.h>

namespace jitmap {
namespace query {

class ExprBuilder;

class Expr {
 public:
  enum Type {
    // Literals
    EMPTY_LITERAL = 0,
    FULL_LITERAL,
    VARIABLE,

    // Operators
    NOT_OPERATOR,
    AND_OPERATOR,
    OR_OPERATOR,
    XOR_OPERATOR,
  };

  Type type() const { return type_; }

  bool IsLiteral() const;
  bool IsVariable() const;
  bool IsOperator() const;
  bool IsUnaryOperator() const;
  bool IsBinaryOperator() const;

  template <typename Visitor>
  auto Visit(Visitor&& v) const;
  template <typename Visitor>
  auto Visit(Visitor&& v);

  // Convenience and debug operators
  bool operator==(const Expr& rhs) const;
  bool operator==(Expr* rhs) const { return *this == *rhs; }
  bool operator!=(const Expr& rhs) const { return !(*this == rhs); }
  std::string ToString() const;

  // Return all Reference expressions.
  std::vector<std::string> Variables() const;

  // Copy the expression
  Expr* Copy(ExprBuilder* builder) const;

  virtual ~Expr() {}

 protected:
  explicit Expr(Type type) : type_(type) {}

 private:
  Type type_;

  // Use Copy()
  Expr(const Expr&) = delete;
};

// The abstract base class exists such that the Expr class is template-free.
template <Expr::Type T>
class BaseExpr : public Expr {
 protected:
  BaseExpr() : Expr(T) {}
};

class LiteralExpr {};
class OpExpr {};

class UnaryOpExpr : public OpExpr {
 public:
  explicit UnaryOpExpr(Expr* expr) : operand_(expr) {}

  Expr* operand() const { return operand_; }
  void set_operand(Expr* expr) { operand_ = expr; }

 private:
  Expr* operand_;
};

class BinaryOpExpr : public OpExpr {
 public:
  BinaryOpExpr(Expr* lhs, Expr* rhs) : left_operand_(lhs), right_operand_(rhs) {}

  Expr* left_operand() const { return left_operand_; }
  void set_left_operand(Expr* left) { left_operand_ = left; }

  Expr* right_operand() const { return right_operand_; }
  void set_right_operand(Expr* right) { right_operand_ = right; }

 private:
  Expr* left_operand_;
  Expr* right_operand_;
};

// Literal Expressions

// Represents an empty bitmap (all bits cleared)
class EmptyBitmapExpr final : public BaseExpr<Expr::EMPTY_LITERAL>, public LiteralExpr {};

// Represents a full bitmap (all bits set)
class FullBitmapExpr final : public BaseExpr<Expr::FULL_LITERAL>, public LiteralExpr {};

// References a Bitmap by name
class VariableExpr final : public BaseExpr<Expr::VARIABLE> {
 public:
  explicit VariableExpr(std::string name) : name_(std::move(name)) {}

  const std::string& value() const { return name_; }

 private:
  std::string name_;
};

// Operators

class NotOpExpr final : public BaseExpr<Expr::NOT_OPERATOR>, public UnaryOpExpr {
 public:
  using UnaryOpExpr::UnaryOpExpr;
};

class AndOpExpr final : public BaseExpr<Expr::AND_OPERATOR>, public BinaryOpExpr {
 public:
  using BinaryOpExpr::BinaryOpExpr;
};

class OrOpExpr final : public BaseExpr<Expr::OR_OPERATOR>, public BinaryOpExpr {
 public:
  using BinaryOpExpr::BinaryOpExpr;
};

class XorOpExpr final : public BaseExpr<Expr::XOR_OPERATOR>, public BinaryOpExpr {
 public:
  using BinaryOpExpr::BinaryOpExpr;
};

class ExprBuilder {
 public:
  Expr* EmptyBitmap() {
    static EmptyBitmapExpr empty;
    return &empty;
  }

  Expr* FullBitmap() {
    static FullBitmapExpr full;
    return &full;
  }

  Expr* Var(const std::string& name) { return Build<VariableExpr>(name); }
  Expr* Var(std::string_view name) { return Build<VariableExpr>(std::string(name)); }

  Expr* Not(Expr* expr) { return Build<NotOpExpr>(expr); }

  Expr* And(Expr* lhs, Expr* rhs) { return Build<AndOpExpr>(lhs, rhs); }

  Expr* Or(Expr* lhs, Expr* rhs) { return Build<OrOpExpr>(lhs, rhs); }

  Expr* Xor(Expr* lhs, Expr* rhs) { return Build<XorOpExpr>(lhs, rhs); }

 private:
  template <typename Type, typename... Args>
  Expr* Build(Args&&... args) {
    return expressions_.emplace_back(std::make_unique<Type>(std::forward<Args>(args)...))
        .get();
  }

  std::vector<std::unique_ptr<Expr>> expressions_;
};

// TODO: Refactor to support various input types (instead of `const Expr&`).
template <typename Visitor>
auto Expr::Visit(Visitor&& v) const {
  switch (type()) {
    case EMPTY_LITERAL:
      return v(dynamic_cast<const EmptyBitmapExpr*>(this));
    case FULL_LITERAL:
      return v(dynamic_cast<const FullBitmapExpr*>(this));
    case VARIABLE:
      return v(dynamic_cast<const VariableExpr*>(this));
    case NOT_OPERATOR:
      return v(dynamic_cast<const NotOpExpr*>(this));
    case AND_OPERATOR:
      return v(dynamic_cast<const AndOpExpr*>(this));
    case OR_OPERATOR:
      return v(dynamic_cast<const OrOpExpr*>(this));
    case XOR_OPERATOR:
      return v(dynamic_cast<const XorOpExpr*>(this));
  }

  throw Exception("Unknown type: ", type());
}

template <typename Visitor>
auto Expr::Visit(Visitor&& v) {
  switch (type()) {
    case EMPTY_LITERAL:
      return v(dynamic_cast<EmptyBitmapExpr*>(this));
    case FULL_LITERAL:
      return v(dynamic_cast<FullBitmapExpr*>(this));
    case VARIABLE:
      return v(dynamic_cast<VariableExpr*>(this));
    case NOT_OPERATOR:
      return v(dynamic_cast<NotOpExpr*>(this));
    case AND_OPERATOR:
      return v(dynamic_cast<AndOpExpr*>(this));
    case OR_OPERATOR:
      return v(dynamic_cast<OrOpExpr*>(this));
    case XOR_OPERATOR:
      return v(dynamic_cast<XorOpExpr*>(this));
  }

  throw Exception("Unknown type: ", type());
}

std::ostream& operator<<(std::ostream& os, const Expr& e);
std::ostream& operator<<(std::ostream& os, Expr* e);

}  // namespace query
}  // namespace jitmap
