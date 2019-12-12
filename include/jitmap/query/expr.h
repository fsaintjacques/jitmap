#pragma once

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace jitmap {
namespace query {

class Expr {
 public:
  enum Type {
    // Literals
    EMPTY_LITERAL = 0,
    FULL_LITERAL,
    NAMED_REF_LITERAL,
    INDEX_REF_LITERAL,

    // Operators
    NOT_OPERATOR,
    AND_OPERATOR,
    OR_OPERATOR,
    XOR_OPERATOR,
  };

  Type type() const { return type_; }

  bool IsLiteral() const;
  bool IsConstant() const;
  bool IsOperator() const;
  bool IsUnaryOperator() const;
  bool IsBinaryOperator() const;

  template <typename Visitor>
  auto Visit(Visitor&& v) const;

  // Convenience and debug operators
  bool operator==(const Expr& rhs) const;
  bool operator==(Expr* rhs) const { return *this == *rhs; }
  bool operator!=(const Expr& rhs) const { return !(*this == rhs); }
  std::string ToString() const;

  // Return all Reference expressions.
  std::unordered_set<std::string> CollectReferences() const;

  virtual ~Expr() {}

 protected:
  Expr(Type type) : type_(type) {}
  Type type_;

 private:
  Expr(const Expr&) = delete;
};

// The abstract base class exists such that the Expr class is template-free.
template <Expr::Type T>
class BaseExpr : public Expr {
 protected:
  BaseExpr() : Expr(T) {}
};

class LiteralExpr {};
class ConstantExpr : public LiteralExpr {};
class ReferenceExpr : public LiteralExpr {};
class OpExpr {};

class UnaryOpExpr : public OpExpr {
 public:
  UnaryOpExpr(Expr* expr) : operand_(expr) {}

  Expr* operand() const { return operand_; }

 protected:
  Expr* operand_;
};

class BinaryOpExpr : public OpExpr {
 public:
  BinaryOpExpr(Expr* lhs, Expr* rhs) : left_operand_(lhs), right_operand_(rhs) {}

  Expr* left_operand() const { return left_operand_; }
  Expr* right_operand() const { return right_operand_; }

 protected:
  Expr* left_operand_;
  Expr* right_operand_;
};

// Literal Expressions

// Represents an empty bitmap (all bits cleared)
class EmptyBitmapExpr final : public BaseExpr<Expr::EMPTY_LITERAL>,
                              public ConstantExpr {};

// Represents a full bitmap (all bits set)
class FullBitmapExpr final : public BaseExpr<Expr::FULL_LITERAL>, public ConstantExpr {};

// References a Bitmap by name
class NamedRefExpr final : public BaseExpr<Expr::NAMED_REF_LITERAL>,
                           public ReferenceExpr {
 public:
  NamedRefExpr(std::string name) : name_(std::move(name)) {}

  const std::string& value() const { return name_; }

 private:
  std::string name_;
};

// References a Bitmap by index
class IndexRefExpr final : public BaseExpr<Expr::INDEX_REF_LITERAL>,
                           public ReferenceExpr {
 public:
  IndexRefExpr(size_t index) : index_(index) {}

  size_t value() const { return index_; }

 private:
  size_t index_;
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

  Expr* NamedRef(std::string name) { return Build<NamedRefExpr>(name); }
  Expr* NamedRef(std::string_view name) { return Build<NamedRefExpr>(std::string(name)); }

  Expr* IndexRef(size_t index) { return Build<IndexRefExpr>(index); }
  Expr* IndexRef(std::string_view index) {
    return Build<IndexRefExpr>(std::stoull(index.data()));
  }

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
      return v(static_cast<const EmptyBitmapExpr&>(*this));
    case FULL_LITERAL:
      return v(static_cast<const FullBitmapExpr&>(*this));
    case NAMED_REF_LITERAL:
      return v(static_cast<const NamedRefExpr&>(*this));
    case INDEX_REF_LITERAL:
      return v(static_cast<const IndexRefExpr&>(*this));
    case NOT_OPERATOR:
      return v(static_cast<const NotOpExpr&>(*this));
    case AND_OPERATOR:
      return v(static_cast<const AndOpExpr&>(*this));
    case OR_OPERATOR:
      return v(static_cast<const OrOpExpr&>(*this));
    case XOR_OPERATOR:
      return v(static_cast<const XorOpExpr&>(*this));
  }
}

std::ostream& operator<<(std::ostream& os, const Expr& e);
std::ostream& operator<<(std::ostream& os, Expr* e);

}  // namespace query
}  // namespace jitmap