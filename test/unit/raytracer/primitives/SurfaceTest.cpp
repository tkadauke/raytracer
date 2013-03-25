#include "gtest.h"
#include "raytracer/primitives/Primitive.h"
#include "materials/MatteMaterial.h"

namespace PrimitiveTest {
  class ConcretePrimitive : public Primitive {
  public:
    Primitive* intersect(const Ray&, HitPointInterval&) { return 0; }
    BoundingBox boundingBox() { return BoundingBox(); }
  };
  
  TEST(Primitive, ShouldReturnMaterial) {
    ConcretePrimitive primitive;
    MatteMaterial material;
    primitive.setMaterial(&material);
    ASSERT_EQ(&material, primitive.material());
  }
}
