#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "surfaces/Intersection.h"
#include "test/mocks/MockSurface.h"

namespace IntersectionTest {
  using namespace ::testing;
  
  TEST(Intersection, ShouldReturnSelfIfAllOfTheChildSurfacesIntersect) {
    Intersection i;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    i.add(surface1);
    i.add(surface2);
    EXPECT_CALL(*surface1, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(1.0, Vector3d(), Vector3d()), HitPoint(4.0, Vector3d(), Vector3d())),
        Return(surface1)
      )
    );
    EXPECT_CALL(*surface2, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(2.0, Vector3d(), Vector3d()), HitPoint(5.0, Vector3d(), Vector3d())),
        Return(surface2)
      )
    );
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(&i, result);
  }
  
  TEST(Intersection, ShouldNotReturnAnySurfaceIfThereIsNoIntersection) {
    Intersection i;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    i.add(surface1);
    i.add(surface2);
    EXPECT_CALL(*surface1, intersect(_, _)).WillOnce(Return(static_cast<Surface*>(0)));
    EXPECT_CALL(*surface2, intersect(_, _)).WillOnce(Return(static_cast<Surface*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Intersection, ShouldNotReturnAnySurfaceIfNotAllChildrenIntersect) {
    Intersection i;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    i.add(surface1);
    i.add(surface2);
    EXPECT_CALL(*surface1, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(1.0, Vector3d(), Vector3d()), HitPoint(4.0, Vector3d(), Vector3d())),
        Return(surface1)
      )
    );
    EXPECT_CALL(*surface2, intersect(_, _)).WillOnce(Return(static_cast<Surface*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Intersection, ShouldNotReturnAnySurfaceIfNotAllChildrenIntersectInOverlappingIntervals) {
    Intersection i;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    i.add(surface1);
    i.add(surface2);
    EXPECT_CALL(*surface1, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(1.0, Vector3d(), Vector3d()), HitPoint(2.0, Vector3d(), Vector3d())),
        Return(surface1)
      )
    );
    EXPECT_CALL(*surface2, intersect(_, _)).WillOnce(
      DoAll(
        AddHitPoints(HitPoint(3.0, Vector3d(), Vector3d()), HitPoint(4.0, Vector3d(), Vector3d())),
        Return(surface2)
      )
    );
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Intersection, ShouldReturnTrueForIntersectsIfAllOfTheChildSurfacesIntersect) {
    Intersection i;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    i.add(surface1);
    i.add(surface2);
    EXPECT_CALL(*surface1, intersects(_)).WillOnce(Return(true));
    EXPECT_CALL(*surface2, intersects(_)).WillOnce(Return(true));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    bool result = i.intersects(ray);
    
    ASSERT_TRUE(result);
  }
  
  TEST(Intersection, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Intersection i;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    i.add(surface1);
    i.add(surface2);
    EXPECT_CALL(*surface1, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    bool result = i.intersects(ray);
    
    ASSERT_FALSE(result);
  }
  
  TEST(Intersection, ShouldReturnFalseForIntersectsIfNotAllChildrenIntersect) {
    Intersection i;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    i.add(surface1);
    i.add(surface2);
    EXPECT_CALL(*surface1, intersects(_)).WillOnce(Return(true));
    EXPECT_CALL(*surface2, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    bool result = i.intersects(ray);
    
    ASSERT_FALSE(result);
  }
  
  TEST(Intersection, ShouldReturnBoundingBoxWithOneChild) {
    Intersection i;
    MockSurface* mockSurface = new MockSurface;
    i.add(mockSurface);
    
    BoundingBox bbox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1));
    EXPECT_CALL(*mockSurface, boundingBox()).WillOnce(Return(bbox));
    
    ASSERT_EQ(bbox, i.boundingBox());
  }
  
  TEST(Intersection, ShouldReturnBoundingBoxWithMultipleChildren) {
    Intersection i;
    MockSurface* mockSurface1 = new MockSurface;
    MockSurface* mockSurface2 = new MockSurface;
    i.add(mockSurface1);
    i.add(mockSurface2);
    
    EXPECT_CALL(*mockSurface1, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(-1, -1, -1), Vector3d(1, 1, 1))));
    EXPECT_CALL(*mockSurface2, boundingBox()).WillOnce(Return(BoundingBox(Vector3d(0, 0, 0), Vector3d(2, 2, 2))));
    
    BoundingBox expected(Vector3d(0, 0, 0), Vector3d(1, 1, 1));
    ASSERT_EQ(expected, i.boundingBox());
  }
}
