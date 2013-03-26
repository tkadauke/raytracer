#include "gtest.h"
#include "raytracer/primitives/Primitive.h"
#include "raytracer/materials/MatteMaterial.h"

namespace PrimitiveTest {
  using namespace raytracer;

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
