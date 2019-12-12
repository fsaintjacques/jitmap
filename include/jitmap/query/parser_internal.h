#pragma once

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace jitmap {
namespace query {

class Token {
 public:
  enum Type {
    INDEX_LITERAL,
    NAMED_LITERAL,
    LEFT_PARENTHESIS,
    RIGHT_PARENTHESIS,
    NOT_OPERATOR,
    AND_OPERATOR,
    OR_OPERATOR,
    XOR_OPERATOR,
    END_OF_STREAM,
  };

  Token(Type type, std::string_view token) : type_(type), string_(std::move(token)) {}
  Token(Type type) : Token(type, "") {}

  Type type() const { return type_; }
  std::string_view string() const { return string_; }

  bool operator==(const Token& rhs) const {
    return this->type_ == rhs.type() && this->string_ == rhs.string();
  }

  friend std::ostream& operator<<(std::ostream& os, const Token& token) {
    return os << token.type() << "(" << token.string() << ")";
  }

  // Friendly builder methods
  static Token IndexLit(std::string_view);
  static Token NamedLit(std::string_view);
  static Token LeftParen(std::string_view = "");
  static Token RightParen(std::string_view = "");
  static Token NotOp(std::string_view = "");
  static Token AndOp(std::string_view = "");
  static Token OrOp(std::string_view = "");
  static Token XorOp(std::string_view = "");
  static Token EoS(std::string_view = "");

 private:
  Type type_;
  std::string_view string_;
};

std::ostream& operator<<(std::ostream& os, Token::Type type);

class Lexer {
 public:
  Lexer(std::string_view query) : position_(0), query_(std::move(query)) {}

  Token Next();

 private:
  bool Done() const;
  char Peek() const;
  char Consume();
  char Consume(char expected);

  Token ConsumeIndexRef();
  Token ConsumeNamedRef();
  Token ConsumeOperator();

  size_t position_;
  std::string_view query_;
};

}  // namespace query
}  // namespace jitmap
