#pragma once

#include <memory>
#include <string_view>

#include <jitmap/util/exception.h>

namespace jitmap {
namespace query {

class ParserException : public Exception {
 public:
  using Exception::Exception;
};

class Expr;
class ExprBuilder;

// \brief Parse the query as an expression.
//
// \param[in] query, the query string to parse.
// \param[in] builder, the expression builder used to create new expressions.
//
// \return The parsed expression, owned by the builder.
//
// \throws ParserException with a reason why the parsing failed.
Expr* Parse(std::string_view query, ExprBuilder* builder);

}  // namespace query
}  // namespace jitmap
