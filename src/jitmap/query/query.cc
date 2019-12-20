#include "jitmap/query/query.h"
#include "jitmap/query/parser.h"

namespace jitmap {
namespace query {

Query::Query(std::string name, Expr* expr)
    : name_(std::move(name)), query_(expr), variables_(query_->Variables()) {}

std::shared_ptr<Query> Query::Make(std::string name, Expr* expr) {
  return std::shared_ptr<Query>(new Query(name, expr));
}

}  // namespace query
}  // namespace jitmap
