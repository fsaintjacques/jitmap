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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <jitmap/query/parser.h>

#include "../query_test.h"
#include "../../src/jitmap/query/parser_internal.h"

using testing::ElementsAre;

namespace jitmap {
namespace query {

class LexerTest : public QueryTest {
 protected:
  // Shortcuts
  auto Empty() { return Token::Empty(); }
  auto Full() { return Token::Full(); }
  auto Var(std::string_view s) { return Token::Var(s); }
  auto Left() { return Token::LeftParen(); }
  auto Right() { return Token::RightParen(); }
  auto Not() { return Token::NotOp(); }
  auto And() { return Token::AndOp(); }
  auto Or() { return Token::OrOp(); }
  auto Xor() { return Token::XorOp(); }

  template <typename... T>
  void ExpectTokenize(std::string_view query, T... tokens) {
    EXPECT_THAT(Lex(query), ElementsAre(tokens...));
  }

  void ExpectThrow(std::string_view query) { EXPECT_ANY_THROW(Lex(query)); }

  std::vector<Token> Lex(std::string_view query) {
    std::vector<Token> tokens;

    Lexer lexer{query};
    for (Token token = lexer.Next(); token.type() != Token::END_OF_STREAM;
         token = lexer.Next()) {
      tokens.push_back(token);
    }

    return tokens;
  }
};

class ParserTest : public QueryTest {
 protected:
  void ExpectParse(std::string_view query, Expr* expected) {
    ExprEq(Parse(query), expected);
  }

  void ExpectThrow(std::string_view query) { EXPECT_ANY_THROW(Parse(query)); }
};

TEST_F(LexerTest, Basic) {
  ExpectTokenize("(", Left());
  ExpectTokenize(")", Right());
  ExpectTokenize("()", Left(), Right());
  ExpectTokenize(")(", Right(), Left());

  ExpectTokenize("$0", Empty());
  ExpectTokenize("0", Var("0"));
  ExpectTokenize(" $0 ", Empty());
  ExpectTokenize("($0)", Left(), Empty(), Right());
  ExpectTokenize("( $0 )", Left(), Empty(), Right());

  ExpectTokenize("a", Var("a"));
  ExpectTokenize("a ", Var("a"));
  ExpectTokenize("(a)", Left(), Var("a"), Right());
  ExpectTokenize(" (a) ", Left(), Var("a"), Right());

  ExpectTokenize("($0 | a) ", Left(), Empty(), Or(), Var("a"), Right());

  ExpectTokenize("($0 | !a) ", Left(), Empty(), Or(), Not(), Var("a"), Right());

  ExpectTokenize(" (a &b & 1)\t", Left(), Var("a"), And(), Var("b"), And(), Var("1"),
                 Right());
  ExpectTokenize("(a&b&1) ", Left(), Var("a"), And(), Var("b"), And(), Var("1"), Right());

  ExpectTokenize("((a | b) ^ !b) ", Left(), Left(), Var("a"), Or(), Var("b"), Right(),
                 Xor(), Not(), Var("b"), Right());
}

TEST_F(LexerTest, Errors) {}

TEST_F(ParserTest, Basic) {
  ExpectParse("$0", Empty());
  ExpectParse("0", V("0"));
  ExpectParse("a", V("a"));
  ExpectParse("!a", Not(V("a")));
  ExpectParse("!!a", Not(Not(V("a"))));

  ExpectParse("a & b", And(V("a"), V("b")));
  ExpectParse("$0 ^ !b", Xor(Empty(), Not(V("b"))));

  ExpectParse("(a & b & c) | ($0 & $1 & a)",
              Or(And(And(V("a"), V("b")), V("c")), And(And(Empty(), Full()), Var("a"))));
}

TEST_F(ParserTest, Parenthesis) {
  ExpectParse("($1)", Full());
  ExpectParse("(((a)))", Var("a"));
  ExpectParse("(!(b))", Not(Var("b")));

  ExpectParse("a & (b | c)", And(Var("a"), Or(Var("b"), Var("c"))));
  ExpectParse("(a & (b & c))", And(Var("a"), And(Var("b"), Var("c"))));
  ExpectParse("(a & b) & (c & d)", And(And(Var("a"), Var("b")), And(Var("c"), Var("d"))));
}

TEST_F(ParserTest, OperatorPrecedence) {
  // Default precedence
  ExpectParse("a | !b | c", Or(Or(Var("a"), Not(Var("b"))), Var("c")));

  // Not precede over And precede over Xor precede over Or.
  ExpectParse("!a ^ b & c | d",
              Or(Xor(Not(Var("a")), And(Var("b"), Var("c"))), Var("d")));
  ExpectParse("a | !b ^ c", Or(Var("a"), Xor(Not(Var("b")), Var("c"))));
  ExpectParse("a ^ b & !c", Xor(Var("a"), And(Var("b"), Not(Var("c")))));

  // Enforce with parenthesis
  ExpectParse("a ^ b & (c | d)", Xor(Var("a"), And(Var("b"), Or(Var("c"), Var("d")))));
}

TEST_F(ParserTest, Errors) {
  // Invalid reference
  ExpectThrow("0$");
  ExpectThrow("$a");
  ExpectThrow("(a b)");
  ExpectThrow("(a ! b)");

  // No expressions
  ExpectThrow("()");
  ExpectThrow("(())");
  ExpectThrow("((()))");

  // Invalid parenthesis
  ExpectThrow(")a)");
  ExpectThrow("(a(");
  ExpectThrow(")a(");
  ExpectThrow("(a");
  ExpectThrow(")a");
  ExpectThrow("a(");
  ExpectThrow("a)");
  ExpectThrow("()(a)");
  ExpectThrow("(a)()");
}

}  // namespace query
}  // namespace jitmap
