#pragma once

#include <algorithm>
#include <bitset>
#include <climits>
#include <cstdint>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace jitmap {

constexpr size_t kLogBitsPerContainer = 16U;
constexpr size_t kBitsPerContainer = 1UL << kLogBitsPerContainer;

enum ContainerType : uint8_t {
  BITMAP = 0,
  ARRAY = 1,
  RUN_LENGTH = 2,
  STATIC = 3,
};

constexpr size_t kBitsInProxy = 64U;
using ProxyBitmap = std::bitset<kBitsInProxy>;

class Statistics {
 public:
  Statistics() : Statistics(0UL, 0L) {}
  Statistics(ProxyBitmap proxy, int32_t count)
      : proxy_(std::move(proxy)), count_(count) {}

  static Statistics Empty() {
    return Statistics(0xFFFFFFFFFFFFFFFFULL, kBitsPerContainer);
  }
  static Statistics Full() { return Statistics(); };

  bool all() const noexcept { return count_ == kBitsPerContainer; }
  bool full() const noexcept { return all(); }

  bool any() const noexcept { return proxy_.any(); }

  bool none() const noexcept { return proxy_.none(); }
  bool empty() const noexcept { return none(); }

 private:
  ProxyBitmap proxy_;
  int32_t count_ = 0UL;
};

class Container {
 public:
  using index_type = uint16_t;

  Container() : statistics_(std::nullopt) {}
  Container(Statistics statistics) : statistics_(std::move(statistics)) {}

  size_t count() const noexcept { return statistics().all(); }

  bool all() const noexcept { return count() == kBitsPerContainer; }
  bool full() const noexcept { return all(); }

  bool any() const noexcept { return statistics().any(); }

  bool none() const noexcept { return statistics().none(); }
  bool empty() const noexcept { return none(); }

  bool has_statistics() const noexcept { return statistics_.has_value(); }
  const Statistics& statistics() const noexcept {
    if (!has_statistics()) statistics_ = ComputeStatistics();
    return statistics_.value();
  };

  virtual bool operator[](index_type index) const noexcept = 0;

 private:
  virtual Statistics ComputeStatistics() const noexcept = 0;

  // Field is mutable because it is lazily computed.
  mutable std::optional<Statistics> statistics_;
};

class FullContainer final : public Container {
 public:
  bool operator[](index_type index) const noexcept final { return true; }

 private:
  Statistics ComputeStatistics() const noexcept final { return Statistics::Full(); }
};

class EmptyContainer final : public Container {
 public:
  bool operator[](index_type index) const noexcept final { return false; }

 private:
  Statistics ComputeStatistics() const noexcept final { return Statistics::Empty(); }
};

template <typename EffectiveType, ContainerType Type>
class BaseContainer : public Container {
 public:
  using SelfType = EffectiveType;
  static constexpr ContainerType type = Type;

  constexpr ContainerType container_type() const { return type; }
};

class DenseContainer final : public BaseContainer<DenseContainer, BITMAP> {
  bool operator[](index_type index) const noexcept { return bitmap_[index]; }

 private:
  Statistics ComputeStatistics() const noexcept final {
    return {0, static_cast<uint16_t>(bitmap_.count())};
  }

  std::bitset<kBitsPerContainer> bitmap_;
};

class Bitmap {
 public:
  using index_type = uint64_t;
  using key_index_type = uint32_t;

  std::pair<key_index_type, Container::index_type> key(index_type index) const {
    return {index >> kLogBitsPerContainer, index & 0xFF};
  }

  bool operator[](index_type index) const {
    auto [k, offset] = key(index);

    if (auto result = containers_.find(k); result != containers_.end())
      return result->second->operator[](offset);

    return false;
  }

 private:
  std::unordered_map<key_index_type, std::unique_ptr<Container>> containers_;
};

};  // namespace jitmap
