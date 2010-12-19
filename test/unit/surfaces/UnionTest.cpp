#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "surfaces/Union.h"
#include "test/mocks/MockSurface.h"

namespace UnionTest {
  using namespace ::testing;
  
  TEST(Union, ShouldReturnSelfIfAnyOfTheChildSurfacesIntersect) {
    Union u;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    u.add(surface1);
    u.add(surface2);
    EXPECT_CALL(*surface1, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(surface1)));
    EXPECT_CALL(*surface2, intersect(_, _)).WillOnce(Return(static_cast<Surface*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = u.intersect(ray, hitPoints);
    
    ASSERT_EQ(&u, result);
  }
  
  TEST(Union, ShouldNotReturnAnySurfaceIfThereIsNoIntersection) {
    Union u;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    u.add(surface1);
    u.add(surface2);
    EXPECT_CALL(*surface1, intersect(_, _)).WillOnce(Return(static_cast<Surface*>(0)));
    EXPECT_CALL(*surface2, intersect(_, _)).WillOnce(Return(static_cast<Surface*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = u.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
  
  TEST(Union, ShouldBuildUnionOfHitPoints) {
    Union u;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    u.add(surface1);
    u.add(surface2);
    EXPECT_CALL(*surface1, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(1.0, Vector3d(), Vector3d())), Return(surface1)));
    EXPECT_CALL(*surface2, intersect(_, _)).WillOnce(DoAll(AddHitPoint(HitPoint(5.0, Vector3d(), Vector3d())), Return(surface2)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    u.intersect(ray, hitPoints);
    
    ASSERT_EQ(1, hitPoints.min().distance());
    ASSERT_EQ(5, hitPoints.max().distance());
  }
  
  TEST(Union, ShouldReturnTrueForIntersectsIfThereIsAIntersection) {
    Union u;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    u.add(surface1);
    u.add(surface2);
    EXPECT_CALL(*surface1, intersects(_)).WillOnce(Return(true));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    ASSERT_TRUE(u.intersects(ray));
  }
  
  TEST(Union, ShouldReturnFalseForIntersectsIfThereIsNoIntersection) {
    Union u;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    u.add(surface1);
    u.add(surface2);
    EXPECT_CALL(*surface1, intersects(_)).WillOnce(Return(false));
    EXPECT_CALL(*surface2, intersects(_)).WillOnce(Return(false));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    ASSERT_FALSE(u.intersects(ray));
  }
}