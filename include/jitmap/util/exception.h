#pragma once

#include <string>

#include <jitmap/util/fmt.h>

namespace jitmap {
namespace util {

class Exception {
 public:
  explicit Exception(std::string message) : message_(std::move(message)) {}

  template <typename... Args>
  Exception(Args&&... args) : Exception(StaticFmt(std::forward<Args>(args)...)) {}

  const std::string& message() const { return message_; }

 protected:
  std::string message_;
};

}  // namespace util
}  // namespace jitmap
