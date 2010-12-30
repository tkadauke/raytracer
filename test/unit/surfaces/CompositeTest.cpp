#include "gtest.h"
#include "surfaces/Composite.h"
#include "test/mocks/MockSurface.h"

namespace CompositeTest {
  using namespace ::testing;
  
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
  
  TEST(Composite, ShouldSetParent) {
    ConcreteComposite composite;
    MockSurface* mockSurface = new MockSurface;
    composite.add(mockSurface);
    
    ASSERT_EQ(&composite, mockSurface->parent());
  }
  
  TEST(Composite, ShouldReturnBoundingBoxWithOneChild) {
    ConcreteComposite composite;
    MockSurface* mockSurface = new MockSurface;
    composite.add(mockSurface);
    
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    EXPECT_CALL(*mockSurface, boundingBox()).WillOnce(Return(bbox));
    
    ASSERT_EQ(bbox, composite.boundingBox());
  }
  
  TEST(Composite, ShouldReturnBoundingBoxWithMultipleChildren) {
    ConcreteComposite composite;
    MockSurface* mockSurface1 = new MockSurface;
    MockSurface* mockSurface2 = new MockSurface;
    composite.add(mockSurface1);
    composite.add(mockSurface2);
    
    EXPECT_CALL(*mockSurface1, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    EXPECT_CALL(*mockSurface2, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(0, 0, 0), Vector3d(2, 2, 2))));
    
    BoundingBox expected(Vector3d(-1, -1, -1), Vector3d(2, 2, 2));
    ASSERT_EQ(expected, composite.boundingBox());
  }
}
