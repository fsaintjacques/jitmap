#include <ostream>
#include <sstream>
#include <vector>

#include "jitmap/query/matcher.h"
#include "jitmap/query/type_traits.h"

namespace jitmap {
namespace query {

bool Expr::IsLiteral() const {
  switch (type_) {
    case EMPTY_LITERAL:
    case FULL_LITERAL:
    case NAMED_REF_LITERAL:
    case INDEX_REF_LITERAL:
      return true;
    case NOT_OPERATOR:
    case AND_OPERATOR:
    case OR_OPERATOR:
    case XOR_OPERATOR:
      return false;
  }

  return false;
}

bool Expr::IsConstant() const { return type_ == EMPTY_LITERAL || type_ == FULL_LITERAL; }

bool Expr::IsOperator() const {
  switch (type_) {
    case EMPTY_LITERAL:
    case FULL_LITERAL:
    case NAMED_REF_LITERAL:
    case INDEX_REF_LITERAL:
      return false;
    case NOT_OPERATOR:
    case AND_OPERATOR:
    case OR_OPERATOR:
    case XOR_OPERATOR:
      return true;
  }

  return false;
}

bool Expr::IsUnaryOperator() const { return type_ == NOT_OPERATOR; }

bool Expr::IsBinaryOperator() const {
  return type_ == AND_OPERATOR || type_ == OR_OPERATOR || type_ == XOR_OPERATOR;
}

bool Expr::operator==(const Expr& rhs) const {
  // Pointer shorcut.
  if (this == &rhs) return true;
  return this->Visit([&](const auto& left) {
    if (type() != rhs.type()) return false;

    using E = std::decay_t<decltype(left)>;
    const E& right = static_cast<const E&>(rhs);

    if constexpr (is_constant<E>::value) return true;
    if constexpr (is_reference<E>::value) return left.value() == right.value();
    if constexpr (is_unary_op<E>::value) return *left.operand() == *right.operand();
    if constexpr (is_unary_op<E>::value) return *left.operand() == *right.operand();
    if constexpr (is_binary_op<E>::value) {
      return (*left.left_operand() == *right.left_operand()) &&
             (*left.right_operand() == *right.right_operand());
    }

    return false;
  });
}

static const char* OpToChar(Expr::Type op) {
  switch (op) {
    case Expr::EMPTY_LITERAL:
      return "∅";
    case Expr::FULL_LITERAL:
      return "⋃";
    case Expr::NAMED_REF_LITERAL:
      return "";
    case Expr::INDEX_REF_LITERAL:
      return "$";
    case Expr::NOT_OPERATOR:
      return "!";
    case Expr::AND_OPERATOR:
      return "&";
    case Expr::OR_OPERATOR:
      return "|";
    case Expr::XOR_OPERATOR:
      return "^";
  }
}

std::string Expr::ToString() const {
  return Visit([&](const auto& e) -> std::string {
    using E = std::decay_t<decltype(e)>;
    auto type = e.type();
    auto symbol = OpToChar(type);

    std::stringstream ss;

    if constexpr (is_constant<E>::value) {
      ss << symbol;
    }

    if constexpr (is_reference<E>::value) {
      ss << symbol << e.value();
    }

    if constexpr (is_unary_op<E>::value) {
      ss << symbol << e.operand()->ToString();
    }

    if constexpr (is_binary_op<E>::value) {
      auto left = e.left_operand()->ToString();
      auto right = e.right_operand()->ToString();
      ss << "(" << left << " " << symbol << " " << right << ")";
    }

    return ss.str();
  });
}

static void CollectReferencesRecursive(
    const Expr& expr, std::unordered_set<std::string>& unique_references) {
  expr.Visit([&unique_references](const auto& e) {
    using E = std::decay_t<decltype(e)>;

    if constexpr (is_literal<E>::value) {
      unique_references.insert(e.ToString());
    } else if constexpr (is_unary_op<E>::value) {
      CollectReferencesRecursive(*e.operand(), unique_references);
    } else if constexpr (is_binary_op<E>::value) {
      CollectReferencesRecursive(*e.left_operand(), unique_references);
      CollectReferencesRecursive(*e.right_operand(), unique_references);
    }
  });
}

std::unordered_set<std::string> Expr::CollectReferences() const {
  std::unordered_set<std::string> unique_references;
  CollectReferencesRecursive(*this, unique_references);
  return unique_references;
}

std::ostream& operator<<(std::ostream& os, const Expr& e) { return os << e.ToString(); }
std::ostream& operator<<(std::ostream& os, Expr* e) { return os << *e; }

}  // namespace query
}  // namespace jitmap
