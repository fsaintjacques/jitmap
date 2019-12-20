#pragma once

namespace jitmap {

#ifndef CACHELINE
#define CACHELINE 64
#endif

constexpr size_t kCacheLineSize = CACHELINE;

enum PlatformIntrinsic {
  X86_SSE,
  X86_AVX,
  X86_AVX2,
  X86_AVX512,
};

constexpr size_t RequiredAlignment(PlatformIntrinsic platform) {
  switch (platform) {
    case X86_SSE:
      return 16;
    case X86_AVX:
      return 16;
    case X86_AVX2:
      return 32;
    case X86_AVX512:
      return 64;
  }
}

}  // namespace jitmap
