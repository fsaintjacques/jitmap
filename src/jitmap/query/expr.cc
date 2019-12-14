#include <ostream>
#include <sstream>
#include <unordered_set>
#include <vector>

#include "jitmap/query/matcher.h"
#include "jitmap/query/type_traits.h"

namespace jitmap {
namespace query {

bool Expr::IsLiteral() const {
  switch (type_) {
    case EMPTY_LITERAL:
    case FULL_LITERAL:
      return true;
    case VARIABLE:
    case NOT_OPERATOR:
    case AND_OPERATOR:
    case OR_OPERATOR:
    case XOR_OPERATOR:
      return false;
  }

  return false;
}

bool Expr::IsVariable() const { return type_ == VARIABLE; }

bool Expr::IsOperator() const {
  switch (type_) {
    case EMPTY_LITERAL:
    case FULL_LITERAL:
    case VARIABLE:
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

    if constexpr (is_literal<E>::value) return true;
    if constexpr (is_variable<E>::value) return left.value() == right.value();
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
      return "$0";
    case Expr::FULL_LITERAL:
      return "$1";
    case Expr::VARIABLE:
      return "";
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

    if constexpr (is_literal<E>::value) {
      ss << symbol;
    }

    if constexpr (is_variable<E>::value) {
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

static void CollectVariables(const Expr& expr,
                             std::unordered_set<std::string>& unique_variables,
                             std::vector<std::string>& variables) {
  expr.Visit([&unique_variables, &variables](const auto& e) {
    using E = std::decay_t<decltype(e)>;

    if constexpr (is_variable<E>::value) {
      auto var = e.value();
      if (unique_variables.insert(var).second) variables.emplace_back(var);
    } else if constexpr (is_unary_op<E>::value) {
      CollectVariables(*e.operand(), unique_variables, variables);
    } else if constexpr (is_binary_op<E>::value) {
      CollectVariables(*e.left_operand(), unique_variables, variables);
      CollectVariables(*e.right_operand(), unique_variables, variables);
    }
  });
}

std::vector<std::string> Expr::Variables() const {
  std::unordered_set<std::string> unique_variables;
  std::vector<std::string> variables;
  CollectVariables(*this, unique_variables, variables);
  return variables;
}

std::ostream& operator<<(std::ostream& os, const Expr& e) { return os << e.ToString(); }
std::ostream& operator<<(std::ostream& os, Expr* e) { return os << *e; }

}  // namespace query
}  // namespace jitmap
