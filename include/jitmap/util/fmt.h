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
