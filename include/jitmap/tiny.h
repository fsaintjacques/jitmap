#pragma once

#include <bitset>
#include <cstdint>

namespace jitmap {

constexpr size_t kBitsInTiny = 64U;
using TinyBitmap = std::bitset<kBitsInTiny>;

}  // namespace jitmap
