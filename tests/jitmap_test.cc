#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <jitmap/jitmap.h>

using testing::ContainerEq;

namespace jitmap {

static_assert(sizeof(ProxyBitmap) == sizeof(uint64_t), "ProxyBitmap must fit in 64bits");

TEST(JitmapTest, Basic) { DenseContainer dense; }
}  // namespace jitmap
