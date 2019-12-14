#include <memory>

#include <jitmap/query/query.h>

namespace jitmap {
namespace query {

class CompilerException : public util::Exception {
 public:
  using Exception::Exception;
};

std::string Compile(Query& query);
}  // namespace query
}  // namespace jitmap
