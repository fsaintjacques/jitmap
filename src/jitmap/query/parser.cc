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


#include <string>

#include "jitmap/query/parser.h"
#include "jitmap/query/expr.h"

#include "parser_internal.h"

namespace jitmap {
namespace query {

const char* TokenTypeToString(Token::Type type) {
  switch (type) {
    case Token::Type::EMPTY_LITERAL:
      return "$0";
    case Token::Type::FULL_LITERAL:
      return "$1";
    case Token::Type::VARIABLE:
      return "Variable";
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

Token Token::Empty(std::string_view t) { return Token(Token::EMPTY_LITERAL, t); }
Token Token::Full(std::string_view t) { return Token(Token::FULL_LITERAL, t); }
Token Token::Var(std::string_view t) { return Token(Token::VARIABLE, t); }
Token Token::LeftParen(std::string_view t) { return Token(Token::LEFT_PARENTHESIS, t); }
Token Token::RightParen(std::string_view t) { return Token(Token::RIGHT_PARENTHESIS, t); }
Token Token::NotOp(std::string_view t) { return Token(Token::NOT_OPERATOR, t); }
Token Token::AndOp(std::string_view t) { return Token(Token::AND_OPERATOR, t); }
Token Token::OrOp(std::string_view t) { return Token(Token::OR_OPERATOR, t); }
Token Token::XorOp(std::string_view t) { return Token(Token::XOR_OPERATOR, t); }
Token Token::EoS(std::string_view t) { return Token(Token::END_OF_STREAM, t); }

constexpr char kEoFCharacter = '\0';
constexpr char kLiteralPrefixCharacter = '$';

static bool IsSpace(char c) { return std::isspace(c); }
static bool IsVariable(char c) { return std::isalnum(c) || c == '_'; }
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

Token Lexer::ConsumeLiteral() {
  // Pop '$'.
  Consume(kLiteralPrefixCharacter);

  size_t start = position_;
  if (Peek() == '0') {
    Consume();
    return Token::Empty(query_.substr(start, 0));
  } else if (Peek() == '1') {
    Consume();
    return Token::Full(query_.substr(start, 0));
  }

  throw ParserException("Invalid literal character ", Peek());
}

Token Lexer::ConsumeVariable() {
  size_t start = position_;
  while (IsVariable(Peek())) {
    Consume();
  };

  // Expected at least one character
  if (start == position_)
    throw ParserException("Named reference expects at least one character");

  return Token::Var(query_.substr(start, position_ - start));
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
  else if (next == kLiteralPrefixCharacter)
    return ConsumeLiteral();
  else if (IsVariable(next))
    return ConsumeVariable();
  else if (IsOperator(next) || IsParenthesis(next))
    return ConsumeOperator();

  throw ParserException("Unexpected character '", next, "'.");
}

int OperatorPrecedence(Token::Type type) {
  switch (type) {
    case Token::NOT_OPERATOR:
      return 4;
    case Token::AND_OPERATOR:
      return 3;
    case Token::XOR_OPERATOR:
      return 2;
    case Token::OR_OPERATOR:
      return 1;
    default:
      return 0;
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

    while (precedence < OperatorPrecedence(Peek().type())) {
      token = Consume();
      left = ParseInfix(token, left);
    }

    return left;
  }

  Expr* ParsePrefix(Token token) {
    switch (token.type()) {
      case Token::EMPTY_LITERAL:
        return builder_->EmptyBitmap();
      case Token::FULL_LITERAL:
        return builder_->FullBitmap();
      case Token::VARIABLE:
        return builder_->Var(token.string());
      case Token::NOT_OPERATOR:
        return builder_->Not(Parse(OperatorPrecedence(Token::NOT_OPERATOR)));
      case Token::LEFT_PARENTHESIS:
        return ParseAndConsume(Token::RIGHT_PARENTHESIS);
      default:
        throw ParserException("Unexpected token '", token, "'");
    }
  }

  Expr* ParseInfix(Token token, Expr* left) {
    switch (token.type()) {
      case Token::AND_OPERATOR:
        return builder_->And(left, Parse(OperatorPrecedence(Token::AND_OPERATOR)));
      case Token::OR_OPERATOR:
        return builder_->Or(left, Parse(OperatorPrecedence(Token::OR_OPERATOR)));
      case Token::XOR_OPERATOR:
        return builder_->Xor(left, Parse(OperatorPrecedence(Token::XOR_OPERATOR)));
      default:
        throw ParserException("Unexpected token '", token, "'");
    }
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
