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
