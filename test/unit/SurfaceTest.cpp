#include "gtest.h"
#include "Surface.h"
#include "MatteMaterial.h"

namespace SurfaceTest {
  class ConcreteSurface : public Surface {
  public:
    Surface* intersect(const Ray&, HitPointInterval&) { return 0; }
  };
  
  TEST(Composite, ShouldReturnMaterial) {
    ConcreteSurface surface;
    MatteMaterial material;
    surface.setMaterial(&material);
    ASSERT_EQ(&material, surface.material());
  }
}
