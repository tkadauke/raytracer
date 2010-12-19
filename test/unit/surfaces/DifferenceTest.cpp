#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "surfaces/Difference.h"
#include "test/mocks/MockSurface.h"

namespace DifferenceTest {
  using namespace ::testing;
  
  TEST(Difference, ShouldReturnSelfIfFirstOfTheChildSurfacesIntersects) {
    Difference i;
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
    
    ASSERT_EQ(&i, result);
  }
  
  TEST(Difference, ShouldNotReturnAnySurfaceIfFirstChildDoesNotIntersect) {
    Difference i;
    MockSurface* surface1 = new MockSurface;
    MockSurface* surface2 = new MockSurface;
    i.add(surface1);
    i.add(surface2);
    EXPECT_CALL(*surface1, intersect(_, _)).WillOnce(Return(static_cast<Surface*>(0)));
    
    Ray ray(Vector3d(0, 1, 0), Vector3d(1, 0, 0));
    
    HitPointInterval hitPoints;
    Surface* result = i.intersect(ray, hitPoints);
    
    ASSERT_EQ(0, result);
  }
}
