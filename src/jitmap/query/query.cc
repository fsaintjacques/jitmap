#include "jitmap/query/query.h"
#include "jitmap/query/parser.h"

namespace jitmap {
namespace query {

Query::Query(std::string name, Expr* expr)
    : name_(std::move(name)), query_(expr), parameters_(query_->CollectReferences()) {}

std::shared_ptr<Query> Query::Make(std::string name, Expr* expr) {
  return std::shared_ptr<Query>(new Query(name, expr));
}

QueryContext::QueryContext(std::shared_ptr<Query> query, NamedBitmaps bitmaps)
    : query_(std::move(query)), bitmaps_(std::move(bitmaps)) {}

}  // namespace query
}  // namespace jitmap
