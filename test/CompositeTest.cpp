#include "gtest.h"
#include "Composite.h"
#include "test/mocks/MockSurface.h"

namespace CompositeTest {
  class ConcreteComposite : public Composite {
  public:
    Surface* intersect(const Ray&, HitPointInterval&) { return 0; }
  };
  
  TEST(Composite, ShouldInitializeWithEmptyList) {
    ConcreteComposite composite;
    ASSERT_TRUE(composite.surfaces().empty());
  }
  
  TEST(Composite, ShouldAddSurface) {
    ConcreteComposite composite;
    MockSurface* mockSurface = new MockSurface;
    composite.add(mockSurface);
    ASSERT_FALSE(composite.surfaces().empty());
    ASSERT_EQ(mockSurface, composite.surfaces().front());
  }
  
  TEST(Composite, ShouldDestructAllAddedSurfaces) {
    ConcreteComposite* composite = new ConcreteComposite;
    MockSurface* mockSurface = new MockSurface;
    composite->add(mockSurface);
    
    EXPECT_CALL(*mockSurface, destructorCall());
    
    delete composite;
  }
}
