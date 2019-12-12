#pragma once

#include <sstream>

namespace jitmap {
namespace util {

using stream_type = std::stringstream;

template <typename H>
void StaticFmt(stream_type& stream, H&& head) {
  stream << head;
}

template <typename H, typename... T>
void StaticFmt(stream_type& stream, H&& head, T&&... tail) {
  StaticFmt(stream, std::forward<H>(head));
  StaticFmt(stream, std::forward<T>(tail)...);
}

template <typename... Args>
std::string StaticFmt(Args&&... args) {
  stream_type stream;
  StaticFmt(stream, std::forward<Args>(args)...);
  return stream.str();
}

}  // namespace util
}  // namespace jitmap
