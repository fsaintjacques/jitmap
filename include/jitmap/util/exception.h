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
