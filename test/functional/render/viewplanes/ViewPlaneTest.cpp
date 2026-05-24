#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"

namespace ViewPlaneTest {
  using namespace ::testing;
  using namespace render;

  INSTANTIATE_TYPED_TEST_SUITE_P(ViewPlane, AbstractViewPlaneTest, ViewPlane);
}
