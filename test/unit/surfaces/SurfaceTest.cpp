#include "gtest.h"
#include "surfaces/Surface.h"
#include "materials/MatteMaterial.h"

namespace SurfaceTest {
  class ConcreteSurface : public Surface {
  public:
    Surface* intersect(const Ray&, HitPointInterval&) { return 0; }
    BoundingBox boundingBox() { return BoundingBox(); }
  };
  
  TEST(Surface, ShouldReturnMaterial) {
    ConcreteSurface surface;
    MatteMaterial material;
    surface.setMaterial(&material);
    ASSERT_EQ(&material, surface.material());
  }
}
