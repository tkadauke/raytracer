#include "gtest.h"
#include "surfaces/Composite.h"
#include "test/mocks/MockSurface.h"

namespace CompositeTest {
  using namespace ::testing;
  
  TEST(Composite, ShouldInitializeWithEmptyList) {
    Composite composite;
    ASSERT_TRUE(composite.surfaces().empty());
  }
  
  TEST(Composite, ShouldAddSurface) {
    Composite composite;
    MockSurface* mockSurface = new MockSurface;
    composite.add(mockSurface);
    ASSERT_FALSE(composite.surfaces().empty());
    ASSERT_EQ(mockSurface, composite.surfaces().front());
  }
  
  TEST(Composite, ShouldDestructAllAddedSurfaces) {
    Composite* composite = new Composite;
    MockSurface* mockSurface = new MockSurface;
    composite->add(mockSurface);
    
    EXPECT_CALL(*mockSurface, destructorCall());
    
    delete composite;
  }
  
  TEST(Composite, ShouldSetParent) {
    Composite composite;
    MockSurface* mockSurface = new MockSurface;
    composite.add(mockSurface);
    
    ASSERT_EQ(&composite, mockSurface->parent());
  }
  
  TEST(Composite, ShouldReturnIntersectedSurface) {
    Composite composite;
    MockSurface* surface = new MockSurface;
    composite.add(surface);
    EXPECT_CALL(*surface, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(surface)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = composite.intersect(ray, hitPoints);
    
    ASSERT_EQ(surface, result);
  }
  
  TEST(Composite, ShouldNotReturnAnySurfaceIfThereIsNoIntersection) {
    Composite composite;
    MockSurface* surface = new MockSurface;
    composite.add(surface);
    EXPECT_CALL(*surface, intersect(_, _)).WillOnce(Return(static_cast<Surface*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = composite.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Composite, ShouldReturnClosestIntersectedSurfaceIfThereIsMoreThanOneCandidate) {
    Composite composite;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    composite.add(surface1);
    composite.add(surface2);
    EXPECT_CALL(*surface1, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(5.0, Vector3d(), Vector3d())), Return(surface1)));
    EXPECT_CALL(*surface2, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(surface2)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = composite.intersect(ray, hitPoints);
    
    ASSERT_EQ(surface2, result);
  }
  
  TEST(Composite, ShouldReturnTrueForIntersectsIfThereIsAnIntersection) {
    Composite composite;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    composite.add(surface1);
    composite.add(surface2);
    EXPECT_CALL(*surface1, intersects(_)).WillOnce(Return(false));
    EXPECT_CALL(*surface2, intersects(_)).WillOnce(Return(true));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    bool result = composite.intersects(ray);
    
    ASSERT_TRUE(result);
  }
  
  TEST(Composite, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Composite composite;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    composite.add(surface1);
    composite.add(surface2);
    EXPECT_CALL(*surface1, intersects(_)).WillOnce(Return(false));
    EXPECT_CALL(*surface2, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    bool result = composite.intersects(ray);
    
    ASSERT_FALSE(result);
  }

  TEST(Composite, ShouldReturnBoundingBoxWithOneChild) {
    Composite composite;
    MockSurface* mockSurface = new MockSurface;
    composite.add(mockSurface);
    
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    EXPECT_CALL(*mockSurface, boundingBox()).WillOnce(Return(bbox));
    
    ASSERT_EQ(bbox, composite.boundingBox());
  }
  
  TEST(Composite, ShouldReturnBoundingBoxWithMultipleChildren) {
    Composite composite;
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
