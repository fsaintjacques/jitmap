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

#include <climits>
#include <cstddef>
#include <cstdint>

namespace jitmap {

// The size of a cacheline.
constexpr size_t kCacheLineSize = 64ULL;

// The log of the number of bits per container.
constexpr size_t kLogBitsPerContainer = 16ULL;
// The number of bits per container.
constexpr size_t kBitsPerContainer = 1ULL << kLogBitsPerContainer;
// The number of bytes per container.
constexpr size_t kBytesPerContainer = kBitsPerContainer / CHAR_BIT;

using BitsetWordType = uint32_t;
constexpr size_t kBytesPerBitsetWord = sizeof(BitsetWordType);
constexpr size_t kBitsPerBitsetWord = kBytesPerBitsetWord * CHAR_BIT;
constexpr size_t kWordsPerContainers = kBytesPerContainer / kBytesPerBitsetWord;

}  // namespace jitmap
