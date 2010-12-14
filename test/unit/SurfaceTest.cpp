#include "gtest.h"
#include "Surface.h"
#include "Material.h"

namespace SurfaceTest {
  class ConcreteSurface : public Surface {
  public:
    Surface* intersect(const Ray&, HitPointInterval&) { return 0; }
  };
  
  TEST(Composite, ShouldReturnMaterial) {
    ConcreteSurface surface;
    Material material;
    surface.setMaterial(&material);
    ASSERT_EQ(&material, surface.material());
  }
}
