#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <jitmap/query/parser.h>
#include "jitmap/query/parser_internal.h"

#include "../query_test.h"

using testing::ElementsAre;

namespace jitmap {
namespace query {

class LexerTest : public QueryTest {
 protected:
  // Shortcuts
  auto Index(std::string_view s) { return Token::IndexLit(s); }
  auto Name(std::string_view s) { return Token::NamedLit(s); }
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
    ExprEq(Parse(query, &expr_builder_), expected);
  }

  void ExpectThrow(std::string_view query) {
    EXPECT_ANY_THROW(Parse(query, &expr_builder_));
  }
};

TEST_F(LexerTest, Basic) {
  ExpectTokenize("(", Left());
  ExpectTokenize(")", Right());
  ExpectTokenize("()", Left(), Right());
  ExpectTokenize(")(", Right(), Left());

  ExpectTokenize("$0", Index("0"));
  ExpectTokenize(" $0 ", Index("0"));
  ExpectTokenize("($0)", Left(), Index("0"), Right());
  ExpectTokenize("( $0 )", Left(), Index("0"), Right());

  ExpectTokenize("a", Name("a"));
  ExpectTokenize("a ", Name("a"));
  ExpectTokenize("(a)", Left(), Name("a"), Right());
  ExpectTokenize(" (a) ", Left(), Name("a"), Right());

  ExpectTokenize("($0 | a) ", Left(), Index("0"), Or(), Name("a"), Right());

  ExpectTokenize("($0 | !a) ", Left(), Index("0"), Or(), Not(), Name("a"), Right());

  ExpectTokenize(" (a &b & 1)\t", Left(), Name("a"), And(), Name("b"), And(), Name("1"),
                 Right());
  ExpectTokenize("(a&b&1) ", Left(), Name("a"), And(), Name("b"), And(), Name("1"),
                 Right());

  ExpectTokenize("((a | b) ^ !b) ", Left(), Left(), Name("a"), Or(), Name("b"), Right(),
                 Xor(), Not(), Name("b"), Right());
}

TEST_F(LexerTest, Errors) {}

TEST_F(ParserTest, Basic) {
  ExpectParse("$0", I(0));
  ExpectParse("a", N("a"));
  ExpectParse("!a", Not(N("a")));
  ExpectParse("!!a", Not(Not(N("a"))));

  ExpectParse("a & b", And(N("a"), N("b")));
  ExpectParse("$0 ^ !b", Xor(I(0), Not(N("b"))));

  ExpectParse("(a & b & c) | ($0 & $1 & $2)",
              Or(And(And(N("a"), N("b")), N("c")), And(And(I(0), I(1)), I(2))));
}

TEST_F(ParserTest, Parenthesis) {
  ExpectParse("($1)", I(1));
  ExpectParse("(((a)))", N("a"));
  ExpectParse("(!(b))", Not(N("b")));

  ExpectParse("a & (b | c)", And(N("a"), Or(N("b"), N("c"))));
  ExpectParse("(a & (b & c))", And(N("a"), And(N("b"), N("c"))));
  ExpectParse("(a & b) & (c & d)", And(And(N("a"), N("b")), And(N("c"), N("d"))));
}

TEST_F(ParserTest, OperatorPrecedence) {
  // Default precedence
  ExpectParse("a | !b | c", Or(Or(N("a"), Not(N("b"))), N("c")));

  // Not precede over And precede over Xor precede over Or.
  ExpectParse("!a ^ b & c | d", Or(Xor(Not(N("a")), And(N("b"), N("c"))), N("d")));
  ExpectParse("a | !b ^ c", Or(N("a"), Xor(Not(N("b")), N("c"))));
  ExpectParse("a ^ b & !c", Xor(N("a"), And(N("b"), Not(N("c")))));

  // Enforce with parenthesis
  ExpectParse("a ^ b & (c | d)", Xor(N("a"), And(N("b"), Or(N("c"), N("d")))));
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
