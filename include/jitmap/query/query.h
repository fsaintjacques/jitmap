#pragma once

#include <memory>
#include <string>

#include <jitmap/jitmap.h>
#include <jitmap/query/expr.h>
#include <jitmap/query/parser.h>

namespace jitmap {
namespace query {

// Attach a unique name to a bitmap. Used to bind name in the query with a
// physical bitmap.
struct NamedBitmap {
  Bitmap* bitmap;
  std::string name;
};

using NamedBitmaps = std::vector<NamedBitmap>;

class QueryContext;

class Query : public std::enable_shared_from_this<Query> {
 public:
  static std::shared_ptr<Query> Make(std::string name, Expr* query);

  QueryContext Bind(const NamedBitmaps& bitmaps);

  void Optimize();
  void Compile();

  const std::unordered_set<std::string>& parameters() const { return parameters_; }
  const std::string& name() const { return name_; }
  const Expr& expr() const { return *query_; }

 private:
  std::string name_;
  std::optional<Expr*> optimized_query_;
  Expr* query_;

  std::unordered_set<std::string> parameters_;

  Query(std::string name, Expr* expr);
};

class QueryContext {
 public:
  QueryContext(std::shared_ptr<Query> query, NamedBitmaps bitmaps);

  Bitmap* Execute();

  const Query& query() const { return *query_; };

 private:
  std::shared_ptr<Query> query_;
  NamedBitmaps bitmaps_;
};

}  // namespace query
}  // namespace jitmap
