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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <type_traits>

#include <jitmap/bitset.h>

using testing::ContainerEq;

namespace jitmap {

static constexpr size_t kBitsetSize = 64;

using BitsetUInt64 = Bitset<kBitsetSize, uint64_t*>;
using BitsetConstUInt64 = Bitset<kBitsetSize, const uint64_t*>;
using BitsetUPtrUInt64 = Bitset<kBitsetSize, std::unique_ptr<uint64_t>>;

TEST(BitsetTest, make_bitset) {
  // Create a bitset from a variable.
  uint64_t stack_ptr = 0x000000000000000F;
  auto stack = make_bitset<64>(&stack_ptr);
  EXPECT_EQ(stack.count(), 4);

  // Create a bitset from a const variable.
  const uint64_t const_stack_ptr = 0x00000000000000FF;
  auto const_stack = make_bitset<64>(&const_stack_ptr);
  EXPECT_EQ(const_stack.count(), 8);

  constexpr size_t kArraySize = 4;

  // Create a bitset from a stack array.
  uint64_t stack_array_ptr[kArraySize] = {UINT64_MAX, 0ULL, UINT64_MAX, 0ULL};
  auto stack_array = make_bitset<64 * kArraySize>(stack_array_ptr);
  EXPECT_EQ(stack_array.count(), 64 * 2);

  // Create a bitset from a heap array.
  uint64_t* heap_array_ptr = new uint64_t[kArraySize];
  memset(heap_array_ptr, 0, sizeof(uint64_t) * kArraySize);
  heap_array_ptr[1] = 0xFF00000000000000;
  auto heap_array = make_bitset<64 * kArraySize>(heap_array_ptr);
  EXPECT_EQ(heap_array.count(), 8);
  delete[] heap_array_ptr;

  // Create a bitset from a unique_ptr array.
  auto uniq = std::make_unique<uint64_t[]>(kArraySize);
  uniq[2] = 0xFF00FF00FF00FF00;
  // The bitset takes ownership
  auto uniq_bitset = make_bitset<64 * kArraySize>(std::move(uniq));
  EXPECT_EQ(uniq_bitset.count(), 32);
}

TEST(BitsetTest, StackUInt64) {
  const uint64_t no_bits = 0ULL;
  auto empty = make_bitset<64>(&no_bits);
  EXPECT_EQ(empty, empty);
  EXPECT_EQ(empty.size(), kBitsetSize);
  EXPECT_EQ(empty.count(), 0);
  EXPECT_FALSE(empty.all());
  EXPECT_FALSE(empty.any());
  EXPECT_TRUE(empty.none());

  const uint64_t all_bits = UINT64_MAX;
  auto full = make_bitset<64>(&all_bits);
  EXPECT_EQ(full, full);
  EXPECT_EQ(full.size(), kBitsetSize);
  EXPECT_EQ(full.count(), kBitsetSize);
  EXPECT_TRUE(full.all());
  EXPECT_TRUE(full.any());
  EXPECT_FALSE(full.none());

  const uint64_t some_bits = 0xF0F0F0F0F0F0F0F0;
  auto some = make_bitset<64>(&some_bits);
  EXPECT_EQ(some, some);
  EXPECT_EQ(some.size(), kBitsetSize);
  EXPECT_EQ(some.count(), 32);
  EXPECT_FALSE(some.all());
  EXPECT_TRUE(some.any());
  EXPECT_FALSE(some.none());

  const uint32_t other_bits[2] = {0x0F0F0F0F, 0x0F0F0F0F};
  auto other = make_bitset<64>(&other_bits);
  EXPECT_EQ(other, other);
  EXPECT_EQ(other.size(), kBitsetSize);
  EXPECT_EQ(other.count(), 32);
  EXPECT_FALSE(other.all());
  EXPECT_TRUE(other.any());
  EXPECT_FALSE(other.none());

  EXPECT_NE(empty, full);
  EXPECT_NE(empty, some);
  EXPECT_NE(empty, other);
  EXPECT_NE(full, some);
  EXPECT_NE(full, other);
  EXPECT_NE(some, other);
}

TEST(BitsetTest, Operations) {
  const uint64_t no_bits = 0ULL;
  auto empty = make_bitset<64>(&no_bits);

  const uint64_t all_bits = UINT64_MAX;
  auto full = make_bitset<64>(&all_bits);

  uint64_t result_bits = 0ULL;
  auto result = make_bitset<64>(&result_bits);

  // Note that the storage types are different, the inputs are const, e.g. the
  // following will not compile.
  //
  // full &= empty;
  // empty |= full;
  //
  // Thus we validate that we can compare of Bitset from other pointer types
  // even if they're considered read-only, as long as the size match.

  result &= empty;
  ASSERT_EQ(result_bits, 0);

  result &= full;
  ASSERT_EQ(result_bits, 0);

  result |= full;
  ASSERT_EQ(result_bits, UINT64_MAX);

  auto not_empty = ~empty;
  EXPECT_TRUE(not_empty.all());
  EXPECT_FALSE(not_empty.none());
  EXPECT_EQ(not_empty, full);

  auto empty_or_full = empty | full;
  EXPECT_TRUE(empty_or_full.all());
  EXPECT_FALSE(empty_or_full.none());
  EXPECT_EQ(empty_or_full, full);

  auto empty_and_full = empty & full;
  EXPECT_FALSE(empty_and_full.all());
  EXPECT_TRUE(empty_and_full.none());
  EXPECT_EQ(empty_and_full, empty);

  auto empty_xor_full = empty ^ full;
  EXPECT_TRUE(empty_xor_full.all());
  EXPECT_FALSE(empty_xor_full.none());
  EXPECT_EQ(empty_xor_full, full);

  auto full_xor_full = full ^ full;
  EXPECT_FALSE(full_xor_full.all());
  EXPECT_TRUE(full_xor_full.none());
  EXPECT_EQ(full_xor_full, empty);
}

TEST(BitsetTest, ErrorOnNullPtrConstructor) {
  EXPECT_THROW(BitsetUInt64 null_u64_bitset{nullptr}, Exception);
  EXPECT_THROW(BitsetConstUInt64 null_const_u64_bitset{nullptr}, Exception);
  EXPECT_THROW(BitsetUPtrUInt64 null_uptr_u64_bitset{nullptr}, Exception);
}

template <size_t N = kBitsPerContainer>
class alignas(kCacheLineSize) BitsetStorage
    : public std::array<BitsetWordType, N / (sizeof(BitsetWordType) * CHAR_BIT)> {
 public:
  BitsetStorage(bool value = false) {
    memset(this->data(), value ? 0xFF : 0x00, N / CHAR_BIT);
  }
};

template <typename T, size_t ExpectedAlignment, size_t ExpectedSize,
          size_t ActualAlignment = alignof(T), size_t ActualSize = sizeof(T)>
void EnsureAligmentAndSize() {
  static_assert(ExpectedAlignment == ActualAlignment, "alignment don't match!");
  static_assert(ExpectedSize == ActualSize, "size don't match!");
}

TEST(BitsetTest, AlignmentAndSize) {
  EnsureAligmentAndSize<BitsetStorage<>, kCacheLineSize, kBytesPerContainer>();
}

const BitsetStorage empty_store;
const BitsetStorage full_store{true};

TEST(BitsetTest, BitStorage) {
  auto empty = make_bitset<kBitsPerContainer>(empty_store.data());
  EXPECT_EQ(empty, empty);
  EXPECT_EQ(empty.size(), kBitsPerContainer);
  EXPECT_EQ(empty.count(), 0);
  EXPECT_FALSE(empty.all());
  EXPECT_FALSE(empty.any());
  EXPECT_TRUE(empty.none());

  auto full = make_bitset<kBitsPerContainer>(full_store.data());
  EXPECT_EQ(full, full);
  EXPECT_EQ(full.size(), kBitsPerContainer);
  EXPECT_EQ(full.count(), kBitsPerContainer);
  EXPECT_TRUE(full.all());
  EXPECT_TRUE(full.any());
  EXPECT_FALSE(full.none());
}

}  // namespace jitmap
