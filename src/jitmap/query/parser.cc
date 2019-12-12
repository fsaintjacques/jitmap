#include "jitmap/query/parser.h"

#include <string>

#include "jitmap/query/expr.h"
#include "jitmap/query/parser_internal.h"

namespace jitmap {
namespace query {

const char* TokenTypeToString(Token::Type type) {
  switch (type) {
    case Token::Type::INDEX_LITERAL:
      return "IndexLiteral";
    case Token::Type::NAMED_LITERAL:
      return "NamedLiteral";
    case Token::Type::LEFT_PARENTHESIS:
      return "LeftParenthesis";
    case Token::Type::RIGHT_PARENTHESIS:
      return "RightParenthesis";
    case Token::Type::NOT_OPERATOR:
      return "NotOp";
    case Token::Type::AND_OPERATOR:
      return "AndOp";
    case Token::Type::OR_OPERATOR:
      return "OrOp";
    case Token::Type::XOR_OPERATOR:
      return "XorOp";
    case Token::Type::END_OF_STREAM:
      return "EOS";
  }
}

std::ostream& operator<<(std::ostream& os, Token::Type type) {
  return os << TokenTypeToString(type);
}

Token Token::IndexLit(std::string_view t) { return Token(Token::INDEX_LITERAL, t); }
Token Token::NamedLit(std::string_view t) { return Token(Token::NAMED_LITERAL, t); }
Token Token::LeftParen(std::string_view t) { return Token(Token::LEFT_PARENTHESIS, t); }
Token Token::RightParen(std::string_view t) { return Token(Token::RIGHT_PARENTHESIS, t); }
Token Token::NotOp(std::string_view t) { return Token(Token::NOT_OPERATOR, t); }
Token Token::AndOp(std::string_view t) { return Token(Token::AND_OPERATOR, t); }
Token Token::OrOp(std::string_view t) { return Token(Token::OR_OPERATOR, t); }
Token Token::XorOp(std::string_view t) { return Token(Token::XOR_OPERATOR, t); }
Token Token::EoS(std::string_view t) { return Token(Token::END_OF_STREAM, t); }

constexpr char kEoFCharacter = '\0';
constexpr char kIndefRefCharacter = '$';

static bool IsSpace(char c) { return std::isspace(c); }
static bool IsDigit(char c) { return std::isdigit(c); }
static bool IsNamedReference(char c) { return std::isalnum(c) || c == '_'; }
static bool IsLeftParenthesis(char c) { return c == '('; }
static bool IsRightParenthesis(char c) { return c == ')'; }
static bool IsParenthesis(char c) {
  return IsLeftParenthesis(c) || IsRightParenthesis(c);
}

static bool IsOperator(char c) {
  switch (c) {
    case '!':
    case '&':
    case '|':
    case '^':
      return true;
    default:
      return false;
  }
}

bool Lexer::Done() const { return position_ >= query_.size(); }

char Lexer::Peek() const {
  if (Done()) return kEoFCharacter;
  return query_[position_];
}

char Lexer::Consume() {
  if (Done()) return kEoFCharacter;
  return query_[position_++];
}

char Lexer::Consume(char expected) {
  if (Done()) return kEoFCharacter;
  char ret = query_[position_++];

  if (ret != expected)
    throw ParserException("Consumed character '", ret, "' but expected '", expected, "'");

  return ret;
}

Token Lexer::ConsumeIndexRef() {
  // Pop '$'.
  Consume(kIndefRefCharacter);

  size_t start = position_;
  while (IsDigit(Peek())) {
    Consume();
  };

  // Expected at least one digit.
  if (start == position_)
    throw ParserException("Index reference expects at least one digit");

  return Token::IndexLit(query_.substr(start, position_ - start));
}

Token Lexer::ConsumeNamedRef() {
  size_t start = position_;
  while (IsNamedReference(Peek())) {
    Consume();
  };

  // Expected at least one character
  if (start == position_)
    throw ParserException("Named reference expects at least one character");

  return Token::NamedLit(query_.substr(start, position_ - start));
}

Token Lexer::ConsumeOperator() {
  char c = Consume();
  switch (c) {
    case '(':
      return Token::LeftParen();
    case ')':
      return Token::RightParen();
    case '!':
      return Token::NotOp();
    case '&':
      return Token::AndOp();
    case '|':
      return Token::OrOp();
    case '^':
      return Token::XorOp();
    default:
      throw ParserException("Unexpected character '", c, "' while consuming operator.");
  }
}

Token Lexer::Next() {
  while (IsSpace(Peek())) {
    Consume();
  }

  char next = Peek();
  if (next == kEoFCharacter)
    return Token::EoS();
  else if (next == kIndefRefCharacter)
    return ConsumeIndexRef();
  else if (IsNamedReference(next))
    return ConsumeNamedRef();
  else if (IsOperator(next) || IsParenthesis(next))
    return ConsumeOperator();

  throw ParserException("Unexpected character '", next, "'.");
}

static constexpr int kOperatorPrecendenceTable[8] = {
    [Expr::NOT_OPERATOR] = 4,
    [Expr::AND_OPERATOR] = 3,
    [Expr::XOR_OPERATOR] = 2,
    [Expr::OR_OPERATOR] = 1,
};

static inline bool IsInfixOperator(const Token& token) {
  switch (token.type()) {
    case Token::AND_OPERATOR:
    case Token::OR_OPERATOR:
    case Token::XOR_OPERATOR:
      return true;
    default:
      return false;
  }
}

// Pratt parser adapted from
// http://journal.stuffwithstuff.com/2011/03/19/pratt-parsers-expression-parsing-made-easy/
class Parser {
 public:
  Parser(std::string_view query, ExprBuilder* builder)
      : lexer_(query), builder_(builder) {}

  Expr* Parse() { return ParseAndConsume(Token::END_OF_STREAM); }

 protected:
  Token Peek() {
    if (!next_) next_ = lexer_.Next();
    return *next_;
  }

  Token Consume() {
    Token token = Peek();
    next_.reset();
    return token;
  }

  Token Consume(Token::Type expected) {
    Token token = Consume();

    if (token.type() != expected)
      throw ParserException("Unexpected token got '", token, "' but exepected '",
                            TokenTypeToString(expected), "'");
    return token;
  }

  Expr* ParseAndConsume(Token::Type expected) {
    Expr* expr = Parse(0);
    Consume(expected);
    return expr;
  }

  Expr* Parse(int precedence) {
    Token token = Consume();
    Expr* left = ParsePrefix(token);

    while (precedence < GetPrecedence()) {
      token = Consume();
      left = ParseInfix(token, left);
    }

    return left;
  }

  Expr* ParsePrefix(Token token) {
    switch (token.type()) {
      case Token::INDEX_LITERAL:
        return builder_->IndexRef(token.string());
      case Token::NAMED_LITERAL:
        return builder_->NamedRef(token.string());
      case Token::NOT_OPERATOR:
        return builder_->Not(Parse(kOperatorPrecendenceTable[Expr::NOT_OPERATOR]));
      case Token::LEFT_PARENTHESIS:
        return ParseAndConsume(Token::RIGHT_PARENTHESIS);
      default:
        throw ParserException("Unexpected token '", token, "'");
    }
  }

  Expr* ParseInfix(Token token, Expr* left) {
    switch (token.type()) {
      case Token::AND_OPERATOR:
        return builder_->And(left, Parse(kOperatorPrecendenceTable[Expr::AND_OPERATOR]));
      case Token::OR_OPERATOR:
        return builder_->Or(left, Parse(kOperatorPrecendenceTable[Expr::OR_OPERATOR]));
      case Token::XOR_OPERATOR:
        return builder_->Xor(left, Parse(kOperatorPrecendenceTable[Expr::XOR_OPERATOR]));
      default:
        throw ParserException("Unexpected token '", token, "'");
    }
  }

  int GetPrecedence() {
    Token next_token = Peek();

    if (IsInfixOperator(next_token)) {
      return kOperatorPrecendenceTable[next_token.type()];
    }

    return 0;
  }

 private:
  Lexer lexer_;
  ExprBuilder* builder_;
  // A tiny buffer to support Peek().
  std::optional<Token> next_;
};

Expr* Parse(std::string_view query, ExprBuilder* builder) {
  return Parser(query, builder).Parse();
}

}  // namespace query
}  // namespace jitmap
