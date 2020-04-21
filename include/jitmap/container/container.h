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

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <jitmap/size.h>

namespace jitmap {

enum ContainerType : uint8_t {
  BITMAP = 0,
  ARRAY = 1,
  RUN_LENGTH = 2,
  EMPTY = 3,
  FULL = 4,
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

class EmptyContainer final : public Container {
 public:
  bool operator[](index_type index) const noexcept final { return false; }

 private:
  Statistics ComputeStatistics() const noexcept final { return Statistics::Empty(); }
};

class FullContainer final : public Container {
 public:
  bool operator[](index_type index) const noexcept final { return true; }

 private:
  Statistics ComputeStatistics() const noexcept final { return Statistics::Full(); }
};

template <typename EffectiveType, ContainerType Type>
class BaseContainer : public Container {
 public:
  using SelfType = EffectiveType;
  static constexpr ContainerType type = Type;

  constexpr ContainerType container_type() const { return type; }
};

using DenseBitset = std::bitset<kBitsPerContainer>;

class DenseContainer final : public BaseContainer<DenseContainer, BITMAP> {
  bool operator[](index_type index) const noexcept { return bitmap_[index]; }

 private:
  Statistics ComputeStatistics() const noexcept final {
    return {0, static_cast<uint16_t>(bitmap_.count())};
  }

  DenseBitset bitmap_;
};

};  // namespace jitmap
