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
