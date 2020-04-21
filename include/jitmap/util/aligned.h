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

#include <array>

#include <jitmap/size.h>

namespace jitmap {

template <typename T, size_t N, size_t Alignment = kCacheLineSize>
struct alignas(Alignment) aligned_array : public std::array<T, N> {
  // Change the default constructor to zero initialize the data storage.
  aligned_array() {
    T zero{};
    std::fill(this->begin(), this->end(), zero);
  }

  explicit aligned_array(T val) {
    std::fill(this->begin(), this->end(), val);
  }
};

}  // namespace jitmap
