#include "gtest/gtest.h"
#include "test/abstract/AbstractViewPlaneTest.h"
#include "render/viewplanes/RowShuffledViewPlane.h"

namespace RowShuffledViewPlaneTest {
  using namespace ::testing;
  using namespace render;
  
  INSTANTIATE_TYPED_TEST_SUITE_P(
    RowShuffled,
    AbstractViewPlaneTest,
    RowShuffledViewPlane
  );
}
