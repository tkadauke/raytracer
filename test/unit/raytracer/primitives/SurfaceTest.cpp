#include "gtest.h"
#include "raytracer/primitives/Primitive.h"
#include "raytracer/materials/MatteMaterial.h"

namespace PrimitiveTest {
  using namespace raytracer;

  class ConcretePrimitive : public Primitive {
  public:
    inline virtual const Primitive* intersect(const Rayd&, HitPointInterval&, State&) const {
      return nullptr;
    }

    inline virtual BoundingBoxd calculateBoundingBox() const {
      return BoundingBoxd();
    }
  };

  TEST(Primitive, ShouldReturnMaterial) {
    ConcretePrimitive primitive;
    auto material = std::make_shared<MatteMaterial>();
    primitive.setMaterial(material);
    ASSERT_EQ(material, primitive.material());
  }
}
