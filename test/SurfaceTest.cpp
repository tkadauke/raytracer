#include "gtest.h"
#include "Surface.h"

namespace SurfaceTest {
  class ConcreteSurface : public Surface {
  public:
    Surface* intersect(const Ray&, HitPointInterval&) { return 0; }
  };
  
  TEST(Composite, ShouldReturnMaterial) {
    ConcreteSurface surface;
    ASSERT_EQ(Colord::white, surface.material().highlightColor());
  }
}
