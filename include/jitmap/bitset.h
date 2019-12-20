#pragma once

#include <climits>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include <jitmap/size.h>
#include <jitmap/util/exception.h>

namespace jitmap {

// Bitset is a container similar to `std::bitset<N>` but wrapping a data pointer.
// The ownership/lifetime of the data pointer is defined by `Ptr` type.
template <size_t N = kBitsPerContainer, typename Ptr = const BitsetWordType*>
class Bitset {
 public:
  using word_type = BitsetWordType;
  using storage_type = typename std::pointer_traits<Ptr>::element_type;

  template <typename Type, typename Ret = void>
  using enable_if_writable = std::enable_if_t<!std::is_const<Type>::value, Ret>;

  // Indicate if the pointer is read-only.
  static constexpr bool storage_is_const = std::is_const<storage_type>::value;

  // Construct a bitset from a pointer.
  Bitset(Ptr data) : data_(std::move(data)) { JITMAP_PRE_NE(data_, nullptr); }

  // Return the capacity (in bits) of the bitset.
  constexpr size_t size() const noexcept { return N; }
  // Return the capacity (in words) of the bitset.
  constexpr size_t size_words() const noexcept { return N / kBitsPerBitsetWord; }

  template <typename OtherPtr>
  bool operator==(const Bitset<N, OtherPtr>& rhs) const {
    const auto& lhs_data = data();
    const auto& rhs_data = rhs.data();

    if (lhs_data == rhs_data) return true;

    for (size_t i = 0; i < size_words(); i++) {
      if (lhs_data[i] != rhs_data[i]) return false;
    }

    return true;
  }

  template <typename OtherPtr>
  bool operator!=(const Bitset<N, OtherPtr>& rhs) const {
    return !(*this == rhs);
  }

  // Accessors
  bool test(size_t i) const {
    if (i >= N) throw std::out_of_range("Can't access bit");
    return operator[](i);
  }

  bool operator[](size_t i) const noexcept {
    return data()[i / sizeof(word_type)] & (1U << (i % sizeof(word_type)));
  }

  // Indicate if all bits are set.
  bool all() const noexcept {
    for (size_t i = 0; i < size_words(); i++) {
      if (data()[i] != std::numeric_limits<word_type>::max()) return false;
    }

    return true;
  }

  // Indicate if at least one bit is set.
  bool any() const noexcept {
    for (size_t i = 0; i < size_words(); i++) {
      if (data()[i] != 0) return true;
    }

    return false;
  }

  // Indicate if no bit is set.
  bool none() const noexcept {
    for (size_t i = 0; i < size_words(); i++) {
      if (data()[i] != 0) return false;
    }

    return true;
  }

  // Count the number of set bits (ones).
  size_t count() const noexcept {
    size_t sum = 0;

    for (size_t i = 0; i < size_words(); i++) {
      sum += __builtin_popcountll(data()[i]);
    }

    return sum;
  }

  // Modifiers
  //
  // The in-place modifiers are enabled only if the storage pointer is not const.

  // Perform binary AND
  template <typename OtherPtr, typename T1 = storage_type>
  enable_if_writable<T1, Bitset<N, Ptr>&> operator&=(
      const Bitset<N, OtherPtr>& other) noexcept {
    for (size_t i = 0; i < size_words(); i++) {
      data()[i] &= other.data()[i];
    }
    return *this;
  }

  // Perform binary OR
  template <typename OtherPtr, typename T1 = storage_type>
  enable_if_writable<T1, Bitset<N, Ptr>&> operator|=(
      const Bitset<N, OtherPtr>& other) noexcept {
    for (size_t i = 0; i < size_words(); i++) {
      data()[i] |= other.data()[i];
    }
    return *this;
  }

  // Perform binary XOR
  template <typename OtherPtr, typename T1 = storage_type>
  enable_if_writable<T1, Bitset<N, Ptr>&> operator^=(
      const Bitset<N, OtherPtr>& other) noexcept {
    for (size_t i = 0; i < size_words(); i++) {
      data()[i] ^= other.data()[i];
    }
    return *this;
  }

  // Perform binary NOT
  template <typename T1 = storage_type>
  enable_if_writable<T1, Bitset<N, Ptr>&> operator~() noexcept {
    for (size_t i = 0; i < size_words(); i++) {
      data()[i] = ~data()[i];
    }
    return *this;
  }

  // Set all bits.
  template <typename T1 = storage_type>
  enable_if_writable<T1> set() noexcept {
    memset(data(), 0xFF, size() / CHAR_BIT);
  }

  /* TODO
  // Set a single bit.
  template <typename T1 = storage_type>
  enable_if_writable<T1> set(size_t i, bool value = true) {}
  */

  // Clear all bits.
  template <typename T1 = storage_type>
  enable_if_writable<T1> reset() noexcept {
    memset(data(), 0, size() / CHAR_BIT);
  }

  /* TODO
  // Clear a single bit.
  template <typename T1 = storage_type>
  enable_if_writable<T1> reset(size_t i) {}
  */

  // Flip all bits (perform binary NOT).
  template <typename T1 = storage_type>
  enable_if_writable<T1> flip() noexcept {
    this->operator~();
  }

  /* TODO
  // Flip a single bit.
  template <typename T1 = storage_type>
  enable_if_writable<T1> flip(size_t i) {}
  */

  // Data pointers

  const BitsetWordType* data() const {
    return reinterpret_cast<const BitsetWordType*>(&data_[0]);
  }

  template <typename T1 = storage_type>
  enable_if_writable<T1, BitsetWordType*> data() {
    return reinterpret_cast<BitsetWordType*>(&data_[0]);
  }

 private:
  Ptr data_;

  static_assert(N % (kBitsPerBitsetWord) == 0,
                "Bitset size must be a multiple of the BitsetWordType");
  static_assert(N >= kBytesPerBitsetWord, "Bitset size must be greater than word_type");

  // Friend itself of other template parameters, used for accessing `data_`.
  template <size_t M, typename OtherPtr>
  friend class Bitset;
};

// Create a bitset from a memory address.
template <size_t N, typename Ptr = const BitsetWordType*>
Bitset<N, Ptr> make_bitset(Ptr ptr) {
  return {std::move(ptr)};
}

template <class T>
struct DeleteAligned {
  void operator()(T* data) const { free(data); }
};

template <class T>
auto allocate_aligned(size_t alignment, size_t length) {
  T* raw = reinterpret_cast<T*>(aligned_alloc(alignment, sizeof(T) * length));
  return std::unique_ptr<T[], DeleteAligned<T>>{raw};
}

template <size_t N>
auto make_owned_bitset() {
  constexpr size_t kNumberWords = N / kBitsPerBitsetWord;
  return make_bitset<N>(allocate_aligned<BitsetWordType>(kCacheLineSize, kNumberWords));
}

}  // namespace jitmap
