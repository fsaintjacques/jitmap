#pragma once

#include <string>

#include <jitmap/util/fmt.h>

namespace jitmap {

class Exception {
 public:
  explicit Exception(std::string message) : message_(std::move(message)) {}

  template <typename... Args>
  Exception(Args&&... args) : Exception(util::StaticFmt(std::forward<Args>(args)...)) {}

  const std::string& message() const { return message_; }

 protected:
  std::string message_;
};

#define JITMAP_PRE_IMPL_(expr, ...) \
  do {                              \
    if (!(expr)) {                  \
      throw Exception(__VA_ARGS__); \
    }                               \
  } while (false)

#define JITMAP_PRE(expr) JITMAP_PRE_IMPL_(expr, "Precondition ", #expr, " not satisfied")
#define JITMAP_PRE_EQ(left, right) JITMAP_PRE(left == right)
#define JITMAP_PRE_NE(left, right) JITMAP_PRE(left != right)

}  // namespace jitmap
